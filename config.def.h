static const char* const locale = "en_US.UTF-8";
static const char* const alert_txt = "!";

/* cpu thermal driver info */
static const char* const cpu_thermal_file = "/sys/module/k10temp/drivers/pci:k10temp/0000:00:18.3/hwmon/hwmon0/temp1_input";
static const char* const cpu_thermal_scan_fmt = "%lf";
static const double cpu_thermal_divisor = 1000;
static const double cpu_usage_alert_val = 95.0;
static const double cpu_therm_alert_val = 60.0;

/* gpu usage and thermal drivers info */
static const char* const gpu_thermal_file = "/sys/module/amdgpu/drivers/pci:amdgpu/0000:06:00.0/hwmon/hwmon1/temp1_input";
static const char* const gpu_thermal_scan_fmt = "%lf";
static const double gpu_thermal_divisor = 1000;
static const double gpu_therm_alert_val = 70.0;

/* ram */
static const double ram_usage_alert_val = 80.0;

/* mounted partitions info */
static struct mount_info mounts[] = {
	{ .path = "/", .label = "ROOT" },
	{ .path = "/home", .label = "HOME" },
};

/* weather wttr.in url */
static const char* const wttr_url = "https://wttr.in/Sao+Paulo,Brazil?format=%C+%t";

static struct blk blks[] = {
	{ .update_fn = cpublk_update, .delay = 5 },
	{ .update_fn = gpublk_update, .delay = 5 },
	{ .update_fn = ramblk_update, .delay = 5 },
	{ .update_fn = mountblk_update, .delay = 10 },
	{ .update_fn = timedateblk_update, .delay = 5 },
	{ .update_fn = weatherblk_update, .delay = 60 * 60 },
};


