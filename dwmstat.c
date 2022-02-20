#include <unistd.h>
#include <signal.h>
#include <stdbool.h>
#include <sys/time.h>
#include <locale.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>

#define ARRLEN(a) (sizeof(a)/sizeof(a[0]))
typedef char*(*dwmstat_blk_update_fnp)(char*, int len);
static char* cpublk_update(char* statp, int len);
static char* gpublk_update(char* statp, int len);
static char* ramblk_update(char* statp, int len);
static char* timedateblk_update(char* statp, int len);

#include "config.h"


static Display *dpy;
static Window root;
static char oldstat[STATBUF_LEN + 1];
static char newstat[STATBUF_LEN + 1];
static bool terminate = false;


static char* cpublk_update(char* statp, int len)
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

	usage_percent = (long double)((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / 
	            (long double)((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])); 

	memcpy(a, b, sizeof(a));

	usage_percent *= 100;
	therm /= cpu_thermal_divisor;

	statp += snprintf(statp, len, "[CPU %04.1lf%% %.1lfºC]", usage_percent, therm);
	return statp;
}

static char* gpublk_update(char* statp, int len)
{
	double therm;
	FILE* f;

	f = fopen(gpu_thermal_file, "r");
	fscanf(f, gpu_thermal_scan_fmt, &therm);
	fclose(f);

	therm /= gpu_thermal_divisor;

	statp += snprintf(statp, len, "[GPU %.1lfºC]", therm);
	return statp;
}

static char* ramblk_update(char* statp, int len)
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

	statp += snprintf(statp, len, "[RAM %1.1lf%%]", usage);
	return statp;
}

static char* timedateblk_update(char* statp, int len)
{
	struct timeval tv;
	struct tm* tm;
	gettimeofday(&tv, NULL);
	tm = localtime(&tv.tv_sec);

	statp += strftime(statp, len, "[%A %B %d %H:%M]", tm);
	return statp;
}

static void dwmstat_flush()
{
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
		char* statp = newstat;
		for (int i = 0; i < ARRLEN(dwmstat_blks); ++i) {
			int len = (newstat + STATBUF_LEN) - statp;
			statp = dwmstat_blks[i](statp, len);
		}

		dwmstat_flush();
		sleep(1);
	}

	dwmstat_term();

	return 0;
}


