#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>

#include <libudev.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <X11/Xlib.h>
#include <curl/curl.h>

#define BLK_BUFFER_SIZE (64)
#define ARRAY_SIZE(a) (sizeof(a))
#define ARRAY_ELMENT_SIZE(a) (sizeof(a[0]))
#define ARRAY_LEN(a) (ARRAY_SIZE(a)/ARRAY_ELMENT_SIZE(a))

enum cpu_thermal_driver {
	CPU_THERMAL_DRIVER_K10TEMP,
	CPU_THERMAL_DRIVER_INTEL_SOC_DTS,
};

enum gpu_thermal_driver {
	GPU_THERMAL_DRIVER_AMDGPU
};

struct blk;
typedef void(*blk_update_fnp)(struct blk *);

struct blk_buf {
	char data[BLK_BUFFER_SIZE];
	int len;
};

struct blk {
	blk_update_fnp update_fn;
	time_t delay;
	time_t timer;
	struct blk_buf buf;
};

struct mount_info {
	const char *path;
	const char *label;
};


static void cpublk_update(struct blk *blk);
static void gpublk_update(struct blk *blk);
static void ramblk_update(struct blk *blk);
static void mountblk_update(struct blk *blk);
static void timedateblk_update(struct blk *blk);
static void weatherblk_update(struct blk *blk);

#include "config.h"

static Display *dpy;
static Window root;
static struct udev *udev;

static char oldstat[BLK_BUFFER_SIZE * ARRAY_LEN(blks)];
static char newstat[BLK_BUFFER_SIZE * ARRAY_LEN(blks)];
static bool terminate = false;

static void fscanf_aux(const char *filepath, const char *fmt, ...)
{
	FILE *f;
	va_list vl;

	f = fopen(filepath, "r");
	assert(f != NULL);
	va_start(vl, fmt);
	vfscanf(f, fmt, vl);
	va_end(vl);
	fclose(f);
}

static void blk_buf_clean(struct blk_buf *buf)
{
	memset(buf->data, 0, BLK_BUFFER_SIZE);
	buf->len = 0;
}

static void blk_buf_vwrite(struct blk_buf *buf, const char *fmt, va_list vl)
{
	int cap;
	char *cursor;

	cap = BLK_BUFFER_SIZE - buf->len;
	cursor = buf->data + buf->len;
	buf->len += vsnprintf(cursor, cap, fmt, vl);
}

static void blk_buf_write(struct blk_buf *buf, const char *fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);
	blk_buf_vwrite(buf, fmt, vl);
	va_end(vl);
}

static void blk_buf_alert_write(struct blk_buf* buf, bool alert, const char* fmt, ...)
{
	va_list vl;

	va_start(vl, fmt);

	if (alert) 
		blk_buf_write(buf, "%s", alert_txt);
	
	blk_buf_vwrite(buf, fmt, vl);

	va_end(vl);
}

static struct udev_device *find_device_by_driver_name(const char *driver_name)
{
	struct udev_enumerate *enu;
	struct udev_list_entry *entry;
	const char *syspath;
	struct udev_device *dev = NULL;

	enu = udev_enumerate_new(udev);
	udev_enumerate_add_match_sysattr(enu, "name", driver_name);
	udev_enumerate_scan_devices(enu);

	entry = udev_enumerate_get_list_entry(enu);
	if (entry != NULL) {
		syspath = udev_list_entry_get_name(entry);
		dev = udev_device_new_from_syspath(udev, syspath); 
	}

	udev_enumerate_unref(enu);
	return dev;
}

static int device_sysattr_scanf(const char *driver_name, const char *sysattr, const char *fmt, ...)
{
	struct udev_device *dev;
	const char *sysattr_value;
	va_list vl;
	int ret = -1;

	va_start(vl, fmt);

	dev = find_device_by_driver_name(driver_name);
	if (dev == NULL)
		goto Lret;

	sysattr_value = udev_device_get_sysattr_value(dev, sysattr);
	ret = vsscanf(sysattr_value, fmt, vl);

	udev_device_unref(dev);
Lret:
	va_end(vl);
	return ret;
}

static void cpublk_update(struct blk *blk)
{
	static unsigned long long a[4] = { 0 };

	unsigned long long b[4];
	double usage_percent;
	double therm, therm2;
	bool alert;

	/* usage info */
	fscanf_aux("/proc/stat", "%*s %llu %llu %llu %llu", &b[0], &b[1], &b[2],
		&b[3]);


	usage_percent = (double)((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / 
	            (double)((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])); 

	memcpy(a, b, sizeof(a));

	usage_percent *= 100;

	/* thermal info */
	switch (cpu_thermal_driver) {
	case CPU_THERMAL_DRIVER_K10TEMP:
		device_sysattr_scanf("k10temp", "temp1_input", "%lf", &therm);
		therm /= 1000;
		break;
	case CPU_THERMAL_DRIVER_INTEL_SOC_DTS:
		device_sysattr_scanf("soc_dts0", "temp1_input", "%lf", &therm);
		device_sysattr_scanf("soc_dts1", "temp1_input", "%lf", &therm2);
		therm = ((therm + therm2) / 2) / 1000;
		break;
	}

	alert = therm >= cpu_therm_alert_val ||
		usage_percent >= cpu_usage_alert_val;

	blk_buf_clean(&blk->buf);
	blk_buf_alert_write(&blk->buf, alert, "[CPU %04.1lf%% %.1lfºC]",
		usage_percent, therm);
}

static void gpublk_update(struct blk *blk)
{
	double therm;
	bool alert;

	switch (gpu_thermal_driver) {
	case GPU_THERMAL_DRIVER_AMDGPU:
		device_sysattr_scanf("amdgpu", "temp1_input", "%lf", &therm);
		therm /= 1000;
		break;
	}

	alert = therm >= gpu_therm_alert_val;

	blk_buf_clean(&blk->buf);
	blk_buf_alert_write(&blk->buf, alert, "[GPU %.1lfºC]", therm);
}

static void ramblk_update(struct blk *blk)
{
	unsigned long total, free, cached, buffers;
	double in_use, usage_percent;
	bool alert;

	fscanf_aux(
		"/proc/meminfo",
		"MemTotal: %lu kB\n"
		"MemFree: %lu kB\n"
		"MemAvailable: %*d kB\n"
		"Buffers: %lu kB\n"
		"Cached: %lu kB",
		&total,
		&free,
		&buffers,
		&cached
	);

	in_use = (total - free) - (cached + buffers);
	usage_percent = (in_use / total) * 100.0;
	alert = usage_percent >= ram_usage_alert_val;

	blk_buf_clean(&blk->buf);
	blk_buf_alert_write(&blk->buf, alert, "[RAM %1.1lf%%]", usage_percent);
}

static void mountblk_update(struct blk *blk)
{
	struct statvfs info;
	double total, avail, used;
	double used_percent;

	blk_buf_clean(&blk->buf);
	blk_buf_write(&blk->buf, "[");

	for (size_t i = 0; i < ARRAY_LEN(mounts); ++i) {
		statvfs(mounts[i].path, &info);
		total = info.f_blocks;
		avail = info.f_bfree;
		used = total - avail;
		used_percent = (used / total) * 100;
		blk_buf_write(&blk->buf, "%s %.1lf%%%s", mounts[i].label, used_percent,
			(i == (ARRAY_LEN(mounts) - 1)) ? "" : " "
		);
	}

	blk_buf_write(&blk->buf, "]");
}

static void timedateblk_update(struct blk *blk)
{
	char tmpbuf[BLK_BUFFER_SIZE];
	struct timeval tv;
	struct tm *tm;

	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);
	strftime(tmpbuf, BLK_BUFFER_SIZE, "%A %B %d %H:%M", tm);

	blk_buf_clean(&blk->buf);
	blk_buf_write(&blk->buf, "[%s]", tmpbuf);
}

static ssize_t weatherblk_curl_clbk(void *data, size_t size, size_t nmemb, void *udata)
{
	char *tmpbuf = udata;
	const size_t data_size = size * nmemb;

	if (data_size >= BLK_BUFFER_SIZE)
		return -1;

	memcpy(tmpbuf, data, data_size);
	tmpbuf[data_size] = '\0';
	return data_size;
}

static void weatherblk_update(struct blk *blk)
{
	char tmpbuf[BLK_BUFFER_SIZE];

	CURL *ctx;
	CURLcode code;

	ctx = curl_easy_init();
	assert(ctx != NULL);
	curl_easy_setopt(ctx, CURLOPT_URL, wttr_url);
	curl_easy_setopt(ctx, CURLOPT_WRITEFUNCTION, weatherblk_curl_clbk);
	curl_easy_setopt(ctx, CURLOPT_WRITEDATA, tmpbuf);
	curl_easy_setopt(ctx, CURLOPT_TIMEOUT, 3);
	code = curl_easy_perform(ctx);

	if (code == CURLE_OK) {
		blk_buf_clean(&blk->buf);
		blk_buf_write(&blk->buf, "[%s]", tmpbuf);
	}

	curl_easy_cleanup(ctx);
}

static time_t blks_update()
{
	time_t min_sleep, now, secs_remain;

	min_sleep = 0;
	now = time(NULL);

	for (size_t i = 0; i < ARRAY_LEN(blks); ++i) {
		if ((now - blks[i].timer) >= blks[i].delay) {
			blks[i].timer = now;
			blks[i].update_fn(&blks[i]);
		} 

		secs_remain = blks[i].delay - (now - blks[i].timer);
		if (min_sleep == 0) {
			min_sleep = secs_remain;
		} else if (min_sleep > secs_remain) {
			min_sleep = secs_remain;
		}
	}

	return min_sleep;
}

static void dwmstat_flush()
{
	memset(newstat, 0, ARRAY_SIZE(newstat));
	for (size_t i = 0; i < ARRAY_LEN(blks); ++i)
		strcat(newstat, blks[i].buf.data);

	if (strcmp(oldstat, newstat) == 0)
		return;

	strcpy(oldstat, newstat);
	XStoreName(dpy, root, newstat);
	XFlush(dpy);
}

static void dwmstat_sighandler(int signum)
{
	switch (signum) {
		case SIGTERM:
		case SIGINT:
			terminate = true;
			break;
	}
}

static void dwmstat_init()
{
	setlocale(LC_ALL, locale);
	dpy = XOpenDisplay(NULL);
	root = RootWindow(dpy, DefaultScreen(dpy));
	signal(SIGTERM, dwmstat_sighandler);
	signal(SIGINT, dwmstat_sighandler);
	udev = udev_new();
}

static void dwmstat_term()
{
	udev_unref(udev);
	XCloseDisplay(dpy);
}

int main()
{
	time_t sleep_secs;

	dwmstat_init();

	while (!terminate) {
		sleep_secs = blks_update();
		dwmstat_flush();
		sleep(sleep_secs);
	}

	dwmstat_term();

	return 0;
}

