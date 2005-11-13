#ifndef _VX_LIMIT_PROC_H
#define _VX_LIMIT_PROC_H


static inline void vx_limit_fixup(struct _vx_limit *limit)
{
	unsigned long value;
	unsigned int lim;

	for (lim=0; lim<NUM_LIMITS; lim++) {
		value = atomic_read(&limit->rcur[lim]);
		if (value > limit->rmax[lim])
			limit->rmax[lim] = value;
		if (limit->rmax[lim] > limit->rlim[lim])
			limit->rmax[lim] = limit->rlim[lim];
	}
}


#define VX_LIMIT_FMT	":\t%10d\t%10ld\t%10lld\t%6d\n"

#define VX_LIMIT_ARG(r)				\
		,atomic_read(&limit->rcur[r])	\
		,limit->rmax[r]			\
		,VX_VLIM(limit->rlim[r])	\
		,atomic_read(&limit->lhit[r])

static inline int vx_info_proc_limit(struct _vx_limit *limit, char *buffer)
{
	vx_limit_fixup(limit);
	return sprintf(buffer,
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


