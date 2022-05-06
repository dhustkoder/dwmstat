static const char locale[] = "en_US.UTF-8";
static const char alert_txt[] = "!";

/* specify the driver which reports CPU thermal information */
static enum cpu_thermal_driver cpu_thermal_driver = CPU_THERMAL_DRIVER_K10TEMP;
static const double cpu_usage_alert_val = 95.0;
static const double cpu_therm_alert_val = 60.0;

/* specify the driver which reports GPU thermal information */
static enum gpu_thermal_driver gpu_thermal_driver = GPU_THERMAL_DRIVER_AMDGPU;
static const double gpu_therm_alert_val = 70.0;

/* ram */
static const double ram_usage_alert_val = 80.0;

/* mounted partitions info */
static struct mount_info mounts[] = {
	{ .path = "/", .label = "ROOT" },
	{ .path = "/home", .label = "HOME" },
};

/* weather url and error str */
static const char weatherblk_url[] = "https://wttr.in/Sao+Paulo,Brazil?format=%C+%t";
static const char weatherblk_error_str[] = "Unknown location";

/* active blocks */
static struct blk blks[] = {
	{ .update_fn = cpublk_update, .delay = 5 },
	{ .update_fn = gpublk_update, .delay = 5 },
	{ .update_fn = ramblk_update, .delay = 5 },
	{ .update_fn = mountblk_update, .delay = 10 },
	{ .update_fn = timedateblk_update, .delay = 5 },
	{ .update_fn = weatherblk_update, .delay = 60 * 60 },
};


