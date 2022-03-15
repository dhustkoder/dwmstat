#include <locale.h>
#include <signal.h>
#include <stdbool.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdarg.h>

#include <unistd.h>
#include <sys/time.h>
#include <sys/statvfs.h>
#include <X11/Xlib.h>
#include <curl/curl.h>

/* maximum characteres on status bar excluding string terminator */
#define BLK_BUFFER_SIZE (64)
#define ARRAY_SIZE(a) (sizeof(a))
#define ARRAY_ELMENT_SIZE(a) (sizeof(a[0]))
#define ARRAY_LEN(a) (ARRAY_SIZE(a)/ARRAY_ELMENT_SIZE(a))


struct blk_buf {
	char data[BLK_BUFFER_SIZE];
	int len;
};

typedef void(*blk_update_fnp)(struct blk_buf*);

struct blk {
	blk_update_fnp update_fn;
	int delay;
	int timer;
	struct blk_buf buf;
};

struct mount_info {
	const char* path;
	const char* label;
};


static void cpublk_update(struct blk_buf* buf);
static void gpublk_update(struct blk_buf* buf);
static void ramblk_update(struct blk_buf* buf);
static void mountblk_update(struct blk_buf* buf);
static void timedateblk_update(struct blk_buf* buf);
static void weatherblk_update(struct blk_buf* buf);

#include "config.h"


static Display *dpy;
static Window root;

static char oldstat[BLK_BUFFER_SIZE * ARRAY_LEN(blks)];
static char newstat[BLK_BUFFER_SIZE * ARRAY_LEN(blks)];
static bool terminate = false;

static void blk_buf_clean(struct blk_buf* buf)
{
	memset(buf->data, 0, BLK_BUFFER_SIZE);
	buf->len = 0;
}

static void blk_buf_vwrite(struct blk_buf* buf, const char* fmt, va_list vl)
{
	int cap = BLK_BUFFER_SIZE - buf->len;
	char* cursor = buf->data + buf->len;
	buf->len += vsnprintf(cursor, cap, fmt, vl);
}

static void blk_buf_write(struct blk_buf* buf, const char* fmt, ...)
{
	va_list vl;
	va_start(vl, fmt);
	blk_buf_vwrite(buf, fmt, vl);
	va_end(vl);
}

static void blk_buf_alert_write(struct blk_buf* buf, bool alert, const char* fmt, ...)
{
	if (alert) 
		blk_buf_write(buf, "%s", alert_txt);
	
	va_list vl;
	va_start(vl, fmt);
	blk_buf_vwrite(buf, fmt, vl);
	va_end(vl);
}

static void cpublk_update(struct blk_buf* buf)
{
	static unsigned long long a[4] = { 0 };
	unsigned long long b[4];

	double usage_percent, therm;
	FILE* f;

	f = fopen("/proc/stat", "r");
	fscanf(f, "%*s %llu %llu %llu %llu", &b[0], &b[1], &b[2], &b[3]);
	fclose(f);

	f = fopen(cpu_thermal_file, "r");
	fscanf(f, cpu_thermal_scan_fmt, &therm);
	fclose(f);

	usage_percent = (double)((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / 
	            (double)((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])); 

	memcpy(a, b, sizeof(a));

	usage_percent *= 100;
	therm /= cpu_thermal_divisor;

	const bool alert = therm >= cpu_therm_alert_val ||
	                   usage_percent >= cpu_usage_alert_val;

	blk_buf_alert_write(buf, alert, "[CPU %04.1lf%% %.1lfºC]", usage_percent, therm);
}

static void gpublk_update(struct blk_buf* buf)
{
	double therm;
	FILE* f;

	f = fopen(gpu_thermal_file, "r");
	fscanf(f, gpu_thermal_scan_fmt, &therm);
	fclose(f);

	therm /= gpu_thermal_divisor;

	const bool alert = therm >= gpu_therm_alert_val;
	blk_buf_alert_write(buf, alert, "[GPU %.1lfºC]", therm);
}

static void ramblk_update(struct blk_buf* buf)
{
	unsigned long total, free, cached, buffers;
	FILE* meminfo = fopen("/proc/meminfo", "r");
	fscanf(
		meminfo, 
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
	fclose(meminfo);

	const double inuse = (total - free) - (cached + buffers);
	const double usage = (inuse / total) * 100.0;
	const bool alert = usage >= ram_usage_alert_val;
	blk_buf_alert_write(buf, alert, "[RAM %1.1lf%%]", usage);
}

static void mountblk_update(struct blk_buf* buf)
{
	struct statvfs info;

	blk_buf_write(buf, "[");

	for (size_t i = 0; i < ARRAY_LEN(mounts); ++i) {
		statvfs(mounts[i].path, &info);
		double total = info.f_blocks;
		double avail = info.f_bfree;
		double used = total - avail;
		blk_buf_write(
			buf,
			"%s %.1lf%%%s",
			mounts[i].label,
			(used / total) * 100,
			(i == (ARRAY_LEN(mounts) - 1)) ? "" : " "
		);
	}

	blk_buf_write(buf, "]");
}


static void timedateblk_update(struct blk_buf* buf)
{
	struct timeval tv;
	struct tm* tm;
	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);
	buf->len += strftime(buf->data, BLK_BUFFER_SIZE, "[%A %B %d %H:%M]", tm);
}

ssize_t weather_curl_clbk(void *data, size_t size, size_t nmemb, void* udata)
{
	for (size_t i = 0; i < (size * nmemb); ++i)
		printf("0x%X ", ((char*)data)[i]);
	printf("\n");
	size_t udata_size = 0;
	size_t cp_size = 0;

	/* copies the weather icon and one leading space */
	const char* second_space = strchr(data, 0x20) + 1;
	cp_size = second_space - (char*)data;
	if ((udata_size + cp_size) >= weather_buf_size)
		return -1;

	memcpy(udata + udata_size, data, cp_size);
	udata_size += cp_size;

	/* copies the temperature, excluding line feed */
	const char* temp = second_space + 1;
	const char* linefeed = strchr(second_space, 0x0A);
	cp_size = linefeed - temp;
	if ((udata_size + cp_size) >= weather_buf_size)
		return -1;

	memcpy(udata + udata_size, temp, cp_size);
	udata_size += cp_size;
	/* assign the terminating nul charactere */
	((char*)udata)[udata_size] = 0x00;
	return size * nmemb;
}

static void weatherblk_update(struct blk_buf* buf)
{
	char weather_buf[weather_buf_size];
	CURL* curl_handle = curl_easy_init();
	curl_easy_setopt(curl_handle, CURLOPT_URL, wttr_url);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, weather_curl_clbk);
	curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, weather_buf);
	CURLcode res = curl_easy_perform(curl_handle);

	if (res == CURLE_OK)
		blk_buf_write(buf, "[%s]", weather_buf);

	curl_easy_cleanup(curl_handle);
}

static void dwmstat_flush()
{
	strcpy(newstat, blks[0].buf.data);
	for (size_t i = 1; i < ARRAY_LEN(blks); ++i)
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
}

static void dwmstat_term()
{
	XCloseDisplay(dpy);
}


int main()
{
	dwmstat_init();

	while (!terminate) {
		int min_sleep = 0;
		int now = time(NULL);
		for (size_t i = 0; i < ARRAY_LEN(blks); ++i) {
			if ((now - blks[i].timer) >= blks[i].delay) {
				blks[i].timer = now;
				blk_buf_clean(&blks[i].buf);
				blks[i].update_fn(&blks[i].buf);
			} else {
				const int secs_remain = blks[i].delay - (now - blks[i].timer);
				if (min_sleep == 0) {
					min_sleep = secs_remain;
				} else if (min_sleep > secs_remain) {
					min_sleep = secs_remain;
				}
			}
		}

		dwmstat_flush();
		sleep(min_sleep);
	}

	dwmstat_term();

	return 0;
}


