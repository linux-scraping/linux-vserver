

static inline void vx_info_init_limit(struct _vx_limit *limit)
{
	int lim;

	for (lim = 0; lim < NUM_LIMITS; lim++) {
		__rlim_soft(limit, lim) = RLIM_INFINITY;
		__rlim_hard(limit, lim) = RLIM_INFINITY;
		__rlim_set(limit, lim, 0);
		atomic_set(&__rlim_lhit(limit, lim), 0);
		__rlim_rmin(limit, lim) = 0;
		__rlim_rmax(limit, lim) = 0;
	}
}

static inline void vx_info_exit_limit(struct _vx_limit *limit)
{
	rlim_t value;
	int lim;

	for (lim = 0; lim < NUM_LIMITS; lim++) {
		if ((1 << lim) & VLIM_NOCHECK)
			continue;
		value = __rlim_get(limit, lim);
		vxwprintk_xid(value,
			"!!! limit: %p[%s,%d] = %ld on exit.",
			limit, vlimit_name[lim], lim, (long)value);
	}
}

