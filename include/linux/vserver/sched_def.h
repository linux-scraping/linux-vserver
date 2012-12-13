#ifndef _VSERVER_SCHED_DEF_H
#define _VSERVER_SCHED_DEF_H

#include <linux/spinlock.h>
#include <linux/jiffies.h>
#include <linux/cpumask.h>
#include <asm/atomic.h>
#include <asm/param.h>


/* context sub struct */

struct _vx_sched {
	int prio_bias;			/* bias offset for priority */

	cpumask_t update;		/* CPUs which should update */
};

struct _vx_sched_pc {
	int prio_bias;			/* bias offset for priority */

	uint64_t user_ticks;		/* token tick events */
	uint64_t sys_ticks;		/* token tick events */
	uint64_t hold_ticks;		/* token ticks paused */
};


#ifdef CONFIG_VSERVER_DEBUG

static inline void __dump_vx_sched(struct _vx_sched *sched)
{
	printk("\t_vx_sched:\n");
	printk("\t priority = %4d\n", sched->prio_bias);
}

#endif

#endif	/* _VSERVER_SCHED_DEF_H */
