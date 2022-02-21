
/* maximum characteres on status bar excluding string terminator */
#define STATBUF_LEN (128)

static const char* const locale = "en_US.UTF-8";

/* cpu thermal driver info */
static const char* const cpu_thermal_file = "/sys/module/k10temp/drivers/pci:k10temp/0000:00:18.3/hwmon/hwmon0/temp1_input";
static const char* const cpu_thermal_scan_fmt = "%lf";
static const double cpu_thermal_divisor = 1000;

/* gpu usage and thermal drivers info */
static const char* const gpu_thermal_file = "/sys/module/amdgpu/drivers/pci:amdgpu/0000:06:00.0/hwmon/hwmon1/temp1_input";
static const char* const gpu_thermal_scan_fmt = "%lf";
static const double gpu_thermal_divisor = 1000;

static dwmstat_blk_update_fnp dwmstat_blks[] = {
	cpublk_update,
	gpublk_update,
	ramblk_update,
	timedateblk_update,
};


