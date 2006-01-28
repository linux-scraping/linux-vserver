#ifndef _VX_LIMIT_PROC_H
#define _VX_LIMIT_PROC_H


static inline void vx_limit_fixup(struct _vx_limit *limit)
{
	rlim_t value;
	int lim;

	for (lim=0; lim<NUM_LIMITS; lim++) {
		value = __rlim_get(limit, lim);
		if (value > limit->rmax[lim])
			limit->rmax[lim] = value;
		if (value < limit->rmin[lim])
			limit->rmin[lim] = value;
		if (limit->rmax[lim] > limit->hard[lim])
			limit->rmax[lim] = limit->hard[lim];
	}
}


#define VX_LIMIT_FMT	":\t%8ld\t%8ld/%8ld\t%8lld/%8lld\t%6d\n"
#define VX_LIMIT_TOP	\
	"Limit\t current\t     min/max\t\t    soft/hard\t\thits\n"

#define VX_LIMIT_ARG(r)				\
	,(unsigned long)__rlim_get(limit, r)	\
	,(unsigned long)limit->rmin[r]		\
	,(unsigned long)limit->rmax[r]		\
	,VX_VLIM(limit->soft[r])		\
	,VX_VLIM(limit->hard[r])		\
	,atomic_read(&limit->lhit[r])

static inline int vx_info_proc_limit(struct _vx_limit *limit, char *buffer)
{
	vx_limit_fixup(limit);
	return sprintf(buffer, VX_LIMIT_TOP
		"PROC"	VX_LIMIT_FMT
		"VM"	VX_LIMIT_FMT
		"VML"	VX_LIMIT_FMT
		"RSS"	VX_LIMIT_FMT
		"ANON"	VX_LIMIT_FMT
		"FILES" VX_LIMIT_FMT
		"OFD"	VX_LIMIT_FMT
		"LOCKS" VX_LIMIT_FMT
		"SOCK"	VX_LIMIT_FMT
		"MSGQ"	VX_LIMIT_FMT
		"SHM"	VX_LIMIT_FMT
		"SEMA"	VX_LIMIT_FMT
		"SEMS"	VX_LIMIT_FMT
		VX_LIMIT_ARG(RLIMIT_NPROC)
		VX_LIMIT_ARG(RLIMIT_AS)
		VX_LIMIT_ARG(RLIMIT_MEMLOCK)
		VX_LIMIT_ARG(RLIMIT_RSS)
		VX_LIMIT_ARG(VLIMIT_ANON)
		VX_LIMIT_ARG(RLIMIT_NOFILE)
		VX_LIMIT_ARG(VLIMIT_OPENFD)
		VX_LIMIT_ARG(RLIMIT_LOCKS)
		VX_LIMIT_ARG(VLIMIT_NSOCK)
		VX_LIMIT_ARG(RLIMIT_MSGQUEUE)
		VX_LIMIT_ARG(VLIMIT_SHMEM)
		VX_LIMIT_ARG(VLIMIT_SEMARY)
		VX_LIMIT_ARG(VLIMIT_NSEMS)
		);
}

#endif	/* _VX_LIMIT_PROC_H */


