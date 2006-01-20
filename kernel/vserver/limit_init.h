
static inline void vx_info_init_limit(struct _vx_limit *limit)
{
	int lim;

	for (lim=0; lim<NUM_LIMITS; lim++) {
		limit->soft[lim] = RLIM_INFINITY;
		limit->hard[lim] = RLIM_INFINITY;
		__rlim_set(limit, lim, 0);
		atomic_set(&limit->lhit[lim], 0);
		limit->rmin[lim] = 0;
		limit->rmax[lim] = 0;
	}
}

static inline void vx_info_exit_limit(struct _vx_limit *limit)
{
#ifdef	CONFIG_VSERVER_DEBUG
	rlim_t value;
	int lim;

	for (lim=0; lim<NUM_LIMITS; lim++) {
		value = __rlim_get(limit, lim);
		vxwprintk(value,
			"!!! limit: %p[%s,%d] = %ld on exit.",
			limit, vlimit_name[lim], lim, (long)value);
	}
#endif
}

