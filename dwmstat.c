#include <unistd.h>
#include <sys/time.h>
#include <locale.h>
#include <time.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <X11/Xlib.h>


#define STATBUF_LEN (1024)
static Display *dpy;
static Window root;
static char oldstat[STATBUF_LEN + 1];
static char newstat[STATBUF_LEN + 1];

static char* cpublk_update(char* statp, int len)
{
    static unsigned long long a[4] = { 0 };

    unsigned long long b[4];

    unsigned long therm;

    FILE* stat = fopen("/proc/stat", "r");
    fscanf(stat, "%*s %llu %llu %llu %llu", &b[0], &b[1], &b[2], &b[3]);
    fclose(stat);

    FILE* temp = fopen("/sys/module/k10temp/drivers/pci:k10temp/0000:00:18.3/hwmon/hwmon0/temp1_input", "r");
    fscanf(temp, "%ld", &therm);
    fclose(temp);

    long double avg = (long double)((b[0]+b[1]+b[2]) - (a[0]+a[1]+a[2])) / (long double)((b[0]+b[1]+b[2]+b[3]) - (a[0]+a[1]+a[2]+a[3])); 

    memcpy(a, b, sizeof(a));

    statp += snprintf(statp, len, "[CPU: %04.1LF%% %.1lfºC]", avg * 100, ((double)therm) / 1000);
    return statp;
}

static char* ramusgblk_update(char* statp, int len)
{
    unsigned long total, free, cached, buffers;
    FILE* meminfo = fopen("/proc/meminfo", "r");
    fscanf(meminfo, "%*s %lu %*s %*s %lu %*s %*s %*d %*s %*s %lu %*s %*s %lu", &total, &free, &buffers, &cached);
    fclose(meminfo);

    const unsigned long inuse_kb = (total - free) - (cached + buffers);
    const double inuse_gb = ((double)inuse_kb) / (1024 * 1024);
    const double avail_gb = ((double)total) / (1024 * 1024);

    statp += snprintf(statp, len, "[RAM: %1.1lf/%1.1lf]", inuse_gb, avail_gb);
    return statp;
}

static char* timeblk_update(char* statp, int len)
{
    struct timeval tv;
    struct tm* tm;
    gettimeofday(&tv, NULL);
    tm = localtime(&tv.tv_sec);

    statp += strftime(statp, len, "[%B %d %A %H:%M]", tm);
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

static int dwmstat_init()
{
    setlocale(LC_ALL, "");
	dpy = XOpenDisplay(NULL);
	root = RootWindow(dpy, DefaultScreen(dpy));
    return 0;
}


int main()
{
    dwmstat_init();

    for (;;) {
        int len = STATBUF_LEN;
        char* statp = newstat;

        statp = cpublk_update(statp, len);
        len -= (newstat - statp);

        statp = ramusgblk_update(statp, len);
        len -= (newstat - statp);

        statp = timeblk_update(statp, len);
        len -= (newstat - statp);

        dwmstat_flush();
        sleep(1);
    }

    return 0;
}


