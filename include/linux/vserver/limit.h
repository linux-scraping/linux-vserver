#ifndef _VX_LIMIT_H
#define _VX_LIMIT_H


#define VLIMIT_NSOCK	16
#define VLIMIT_OPENFD	17
#define VLIMIT_ANON	18
#define VLIMIT_SHMEM	19
#define VLIMIT_SEMARY	20
#define VLIMIT_NSEMS	21

#ifdef	__KERNEL__

/*	keep in sync with CRLIM_INFINITY */

#define	VLIM_INFINITY	(~0ULL)

#if	(RLIM_INFINITY == VLIM_INFINITY)
#define	VX_VLIM(v) (unsigned long long)(v)
#define	VX_RLIM(v) (unsigned long)(v)
#else
#define	VX_VLIM(r) (((r) == RLIM_INFINITY) ? \
		VLIM_INFINITY : (unsigned long long)(r))
#define	VX_RLIM(v) (((v) == VLIM_INFINITY) ? \
		RLIM_INFINITY : (unsigned long)(v))
#endif

struct sysinfo;

void vx_vsi_meminfo(struct sysinfo *);
void vx_vsi_swapinfo(struct sysinfo *);

#define NUM_LIMITS	24

#endif	/* __KERNEL__ */
#endif	/* _VX_LIMIT_H */
