#ifndef _VS_SCHED_H
#define _VS_SCHED_H

#include "vserver/base.h"
#include "vserver/context.h"
#include "vserver/sched.h"


#define VAVAVOOM_RATIO		 50

#define MAX_PRIO_BIAS		 20
#define MIN_PRIO_BIAS		-20


#ifdef CONFIG_VSERVER_HARDCPU

/*
 * effective_prio - return the priority that is based on the static
 * priority but is modified by bonuses/penalties.
 *
 * We scale the actual sleep average [0 .... MAX_SLEEP_AVG]
 * into a -4 ... 0 ... +4 bonus/penalty range.
 *
 * Additionally, we scale another amount based on the number of
 * CPU tokens currently held by the context, if the process is
 * part of a context (and the appropriate SCHED flag is set).
 * This ranges from -5 ... 0 ... +15, quadratically.
 *
 * So, the total bonus is -9 .. 0 .. +19
 * We use ~50% of the full 0...39 priority range so that:
 *
 * 1) nice +19 interactive tasks do not preempt nice 0 CPU hogs.
 * 2) nice -20 CPU hogs do not get preempted by nice 0 tasks.
 *    unless that context is far exceeding its CPU allocation.
 *
 * Both properties are important to certain workloads.
 */
static inline
int vx_effective_vavavoom(struct _vx_sched_pc *sched_pc, int max_prio)
{
	int vavavoom, max;

	/* lots of tokens = lots of vavavoom
	 *      no tokens = no vavavoom      */
	if ((vavavoom = sched_pc->tokens) >= 0) {
		max = sched_pc->tokens_max;
		vavavoom = max - vavavoom;
		max = max * max;
		vavavoom = max_prio * VAVAVOOM_RATIO / 100
			* (vavavoom*vavavoom - (max >> 2)) / max;
		return vavavoom;
	}
	return 0;
}


static inline
int vx_adjust_prio(struct task_struct *p, int prio, int max_user)
{
	struct vx_info *vxi = p->vx_info;

	if (!vxi)
		return prio;

	if (vx_info_flags(vxi, VXF_SCHED_PRIO, 0)) {
		struct _vx_sched_pc *sched_pc = &vx_cpu(vxi, sched_pc);
		int vavavoom = vx_effective_vavavoom(sched_pc, max_user);

		vxi->sched.vavavoom = vavavoom;
		prio += vavavoom;
	}
	prio += vxi->sched.prio_bias;
	return prio;
}

#else /* !CONFIG_VSERVER_HARDCPU */

static inline
int vx_adjust_prio(struct task_struct *p, int prio, int max_user)
{
	struct vx_info *vxi = p->vx_info;

	if (vxi)
		prio += vxi->sched.prio_bias;
	return prio;
}

#endif /* CONFIG_VSERVER_HARDCPU */


static inline void vx_account_user(struct vx_info *vxi,
	cputime_t cputime, int nice)
{
	if (!vxi)
		return;
	vx_cpu(vxi, sched_pc).user_ticks += cputime;
}

static inline void vx_account_system(struct vx_info *vxi,
	cputime_t cputime, int idle)
{
	if (!vxi)
		return;
	vx_cpu(vxi, sched_pc).sys_ticks += cputime;
}

#else
#warning duplicate inclusion
#endif
