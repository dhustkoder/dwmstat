
/* maximum characteres on status bar excluding string terminator */
#define STATBUF_LEN (64)

static const char* locale = "en_US.UTF-8";

static dwmstat_blk_update_fnp dwmstat_blks[] = {
	cpublk_update,
	ramblk_update,
	timedateblk_update,
};


