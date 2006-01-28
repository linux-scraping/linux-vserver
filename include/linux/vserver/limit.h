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

#ifndef RLIM_INFINITY
#warning RLIM_INFINITY is undefined
#endif

#define	__rlim_val(l,r)		((l)->rcur[(r)])
#define	__rlim_hit(l,r)		atomic_inc(&(l)->lhit[(r)])

#ifdef ATOMIC64_INIT
typedef atomic64_t rlim_atomic_t;
typedef unsigned long rlim_t;

#define __rlim_get(l,r)		atomic64_read(&__rlim_val(l,r))
#define __rlim_set(l,r,v)	atomic64_set(&__rlim_val(l,r), v)
#define	__rlim_inc(l,r)		atomic64_inc(&__rlim_val(l,r))
#define	__rlim_dec(l,r)		atomic64_dec(&__rlim_val(l,r))
#define	__rlim_add(l,r,v)	atomic64_add(v, &__rlim_val(l,r))

#else /* !ATOMIC64_INIT */
typedef atomic_t rlim_atomic_t;
typedef unsigned rlim_t;

#define __rlim_get(l,r)		atomic_read(&__rlim_val(l,r))
#define __rlim_set(l,r,v)	atomic_set(&__rlim_val(l,r), v)
#define	__rlim_inc(l,r)		atomic_inc(&__rlim_val(l,r))
#define	__rlim_dec(l,r)		atomic_dec(&__rlim_val(l,r))
#define	__rlim_add(l,r,v)	atomic_add(v, &__rlim_val(l,r))
#endif /* ATOMIC64_INIT */

#if	(RLIM_INFINITY == VLIM_INFINITY)
#define	VX_VLIM(r) ((long long)(long)(r))
#define	VX_RLIM(v) ((rlim_t)(v))
#else
#define	VX_VLIM(r) (((r) == RLIM_INFINITY) \
		? VLIM_INFINITY : (long long)(r))
#define	VX_RLIM(v) (((v) == VLIM_INFINITY) \
		? RLIM_INFINITY : (rlim_t)(v))
#endif

struct sysinfo;

void vx_vsi_meminfo(struct sysinfo *);
void vx_vsi_swapinfo(struct sysinfo *);

#define NUM_LIMITS	24

#endif	/* __KERNEL__ */
#endif	/* _VX_LIMIT_H */
