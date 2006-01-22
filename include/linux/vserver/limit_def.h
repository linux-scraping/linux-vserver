#ifndef _VX_LIMIT_DEF_H
#define _VX_LIMIT_DEF_H

#include <asm/atomic.h>
#include <asm/resource.h>

#include "limit.h"

/* context sub struct */

struct _vx_limit {
	rlim_t soft[NUM_LIMITS];	/* Context soft limit */
	rlim_t hard[NUM_LIMITS];	/* Context hard limit */

	rlim_atomic_t rcur[NUM_LIMITS];	/* Current value */
	rlim_t rmin[NUM_LIMITS];	/* Context minimum */
	rlim_t rmax[NUM_LIMITS];	/* Context maximum */

	atomic_t lhit[NUM_LIMITS];	/* Limit hits */
};

#ifdef CONFIG_VSERVER_DEBUG

static inline void __dump_vx_limit(struct _vx_limit *limit)
{
	int i;

	printk("\t_vx_limit:");
	for (i=0; i<NUM_LIMITS; i++) {
		printk("\t [%2d] = %8lu %8lu/%8lu, %8ld/%8ld, %8d\n",
			i, (unsigned long)__rlim_get(limit, i),
			(unsigned long)limit->rmin[i],
			(unsigned long)limit->rmax[i],
			(long)limit->soft[i], (long)limit->hard[i],
			atomic_read(&limit->lhit[i]));
	}
}

#endif

#endif	/* _VX_LIMIT_DEF_H */
