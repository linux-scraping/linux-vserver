
static inline void vx_info_init_limit(struct _vx_limit *limit)
{
	int lim;

	for (lim=0; lim<NUM_LIMITS; lim++) {
		limit->rlim[lim] = RLIM_INFINITY;
		limit->rmax[lim] = 0;
		atomic_set(&limit->rcur[lim], 0);
		atomic_set(&limit->lhit[lim], 0);
	}
}

static inline void vx_info_exit_limit(struct _vx_limit *limit)
{
#ifdef	CONFIG_VSERVER_DEBUG
	unsigned long value;
	unsigned int lim;

	for (lim=0; lim<NUM_LIMITS; lim++) {
		value = atomic_read(&limit->rcur[lim]);
		vxwprintk(value,
			"!!! limit: %p[%s,%d] = %ld on exit.",
			limit, vlimit_name[lim], lim, value);
	}
#endif
}

