#ifndef _VX_LIMIT_PROC_H
#define _VX_LIMIT_PROC_H

#include <linux/vserver/limit_int.h>


#define VX_LIMIT_FMT	":\t%8ld\t%8ld/%8ld\t%8lld/%8lld\t%6d\n"
#define VX_LIMIT_TOP	\
	"Limit\t current\t     min/max\t\t    soft/hard\t\thits\n"

#define VX_LIMIT_ARG(r)				\
	(unsigned long)__rlim_get(limit, r),	\
	(unsigned long)__rlim_rmin(limit, r),	\
	(unsigned long)__rlim_rmax(limit, r),	\
	VX_VLIM(__rlim_soft(limit, r)),		\
	VX_VLIM(__rlim_hard(limit, r)),		\
	atomic_read(&__rlim_lhit(limit, r))

static inline int vx_info_proc_limit(struct _vx_limit *limit, char *buffer)
{
	vx_limit_fixup(limit, -1);
	return sprintf(buffer, VX_LIMIT_TOP
		"PROC"	VX_LIMIT_FMT
		"VM"	VX_LIMIT_FMT
		"VML"	VX_LIMIT_FMT
		"RSS"	VX_LIMIT_FMT
		"ANON"	VX_LIMIT_FMT
		"RMAP"	VX_LIMIT_FMT
		"FILES" VX_LIMIT_FMT
		"OFD"	VX_LIMIT_FMT
		"LOCKS" VX_LIMIT_FMT
		"SOCK"	VX_LIMIT_FMT
		"MSGQ"	VX_LIMIT_FMT
		"SHM"	VX_LIMIT_FMT
		"SEMA"	VX_LIMIT_FMT
		"SEMS"	VX_LIMIT_FMT
		"DENT"	VX_LIMIT_FMT,
		VX_LIMIT_ARG(RLIMIT_NPROC),
		VX_LIMIT_ARG(RLIMIT_AS),
		VX_LIMIT_ARG(RLIMIT_MEMLOCK),
		VX_LIMIT_ARG(RLIMIT_RSS),
		VX_LIMIT_ARG(VLIMIT_ANON),
		VX_LIMIT_ARG(VLIMIT_MAPPED),
		VX_LIMIT_ARG(RLIMIT_NOFILE),
		VX_LIMIT_ARG(VLIMIT_OPENFD),
		VX_LIMIT_ARG(RLIMIT_LOCKS),
		VX_LIMIT_ARG(VLIMIT_NSOCK),
		VX_LIMIT_ARG(RLIMIT_MSGQUEUE),
		VX_LIMIT_ARG(VLIMIT_SHMEM),
		VX_LIMIT_ARG(VLIMIT_SEMARY),
		VX_LIMIT_ARG(VLIMIT_NSEMS),
		VX_LIMIT_ARG(VLIMIT_DENTRY));
}

#endif	/* _VX_LIMIT_PROC_H */


