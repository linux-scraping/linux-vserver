#ifndef _VX_VS_SCHED_H
#define _VX_VS_SCHED_H

#ifndef CONFIG_VSERVER
#warning config options missing
#endif

#include "vserver/sched.h"


#define VAVAVOOM_RATIO		 50

#define MAX_PRIO_BIAS		 20
#define MIN_PRIO_BIAS		-20


static inline int vx_tokens_avail(struct vx_info *vxi)
{
	return atomic_read(&vxi->sched.tokens);
}

static inline void vx_consume_token(struct vx_info *vxi)
{
	atomic_dec(&vxi->sched.tokens);
}

static inline int vx_need_resched(struct task_struct *p)
{
#ifdef	CONFIG_VSERVER_HARDCPU
	struct vx_info *vxi = p->vx_info;
#endif
	int slice = --p->time_slice;

#ifdef	CONFIG_VSERVER_HARDCPU
	if (vxi) {
		int tokens;

		if ((tokens = vx_tokens_avail(vxi)) > 0)
			vx_consume_token(vxi);
		/* for tokens > 0, one token was consumed */
		if (tokens < 2)
			return 1;
	}
#endif
	return (slice == 0);
}


static inline void vx_onhold_inc(struct vx_info *vxi)
{
	int onhold = atomic_read(&vxi->cvirt.nr_onhold);

	atomic_inc(&vxi->cvirt.nr_onhold);
	if (!onhold)
		vxi->cvirt.onhold_last = jiffies;
}

static inline void __vx_onhold_update(struct vx_info *vxi)
{
	int cpu = smp_processor_id();
	uint32_t now = jiffies;
	uint32_t delta = now - vxi->cvirt.onhold_last;

	vxi->cvirt.onhold_last = now;
	vxi->sched.cpu[cpu].hold_ticks += delta;
}

static inline void vx_onhold_dec(struct vx_info *vxi)
{
	if (atomic_dec_and_test(&vxi->cvirt.nr_onhold))
		__vx_onhold_update(vxi);
}

static inline void vx_account_user(struct vx_info *vxi,
	cputime_t cputime, int nice)
{
	int cpu = smp_processor_id();

	if (!vxi)
		return;
	vxi->sched.cpu[cpu].user_ticks += cputime;
}

static inline void vx_account_system(struct vx_info *vxi,
	cputime_t cputime, int idle)
{
	int cpu = smp_processor_id();

	if (!vxi)
		return;
	vxi->sched.cpu[cpu].sys_ticks += cputime;
}

#else
#warning duplicate inclusion
#endif
