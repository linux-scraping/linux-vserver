/*
 *  linux/kernel/vserver/sched.c
 *
 *  Virtual Server: Scheduler Support
 *
 *  Copyright (C) 2004-2005  Herbert Pötzl
 *
 *  V0.01  adapted Sam Vilains version to 2.6.3
 *  V0.02  removed legacy interface
 *
 */

#include <linux/config.h>
#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vs_sched.h>
#include <linux/vserver/sched_cmd.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


/*
 * recalculate the context's scheduling tokens
 *
 * ret > 0 : number of tokens available
 * ret = 0 : context is paused
 * ret < 0 : number of jiffies until new tokens arrive
 *
 */
int vx_tokens_recalc(struct vx_info *vxi)
{
	long delta, tokens = 0;

	if (vx_info_flags(vxi, VXF_SCHED_PAUSE, 0))
		/* we are paused */
		return 0;

	delta = jiffies - vxi->sched.jiffies;

	if (delta >= vxi->sched.interval) {
		/* lockdown scheduler info */
		spin_lock(&vxi->sched.tokens_lock);

		/* calc integral token part */
		delta = jiffies - vxi->sched.jiffies;
		tokens = delta / vxi->sched.interval;
		delta = tokens * vxi->sched.interval;
		tokens *= vxi->sched.fill_rate;

		atomic_add(tokens, &vxi->sched.tokens);
		vxi->sched.jiffies += delta;
		tokens = atomic_read(&vxi->sched.tokens);

		if (tokens > vxi->sched.tokens_max) {
			tokens = vxi->sched.tokens_max;
			atomic_set(&vxi->sched.tokens, tokens);
		}
		spin_unlock(&vxi->sched.tokens_lock);
	} else {
		/* no new tokens */
		tokens = vx_tokens_avail(vxi);
		if (tokens <= 0)
			vxi->vx_state |= VXS_ONHOLD;
		if (tokens < vxi->sched.tokens_min) {
			/* enough tokens will be available in */
			if (vxi->sched.tokens_min == 0)
				return delta - vxi->sched.interval;
			return delta - vxi->sched.interval *
				vxi->sched.tokens_min / vxi->sched.fill_rate;
		}
	}

	/* we have some tokens left */
	if (vx_info_state(vxi, VXS_ONHOLD) &&
		(tokens >= vxi->sched.tokens_min))
		vxi->vx_state &= ~VXS_ONHOLD;
	if (vx_info_state(vxi, VXS_ONHOLD))
		tokens -= vxi->sched.tokens_min;

	return tokens;
}

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
int vx_effective_vavavoom(struct vx_info *vxi, int max_prio)
{
	int vavavoom, max;

	/* lots of tokens = lots of vavavoom
	 *      no tokens = no vavavoom      */
	if ((vavavoom = atomic_read(&vxi->sched.tokens)) >= 0) {
		max = vxi->sched.tokens_max;
		vavavoom = max - vavavoom;
		max = max * max;
		vavavoom = max_prio * VAVAVOOM_RATIO / 100
			* (vavavoom*vavavoom - (max >> 2)) / max;
	} else
		vavavoom = 0;

	vxi->sched.vavavoom = vavavoom;
	return vavavoom;
}


int vc_set_sched_v2(uint32_t xid, void __user *data)
{
	struct vcmd_set_sched_v2 vc_data;
	struct vx_info *vxi;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = lookup_vx_info(xid);
	if (!vxi)
		return -ESRCH;

	spin_lock(&vxi->sched.tokens_lock);

	if (vc_data.interval != SCHED_KEEP)
		vxi->sched.interval = vc_data.interval;
	if (vc_data.fill_rate != SCHED_KEEP)
		vxi->sched.fill_rate = vc_data.fill_rate;
	if (vc_data.tokens_min != SCHED_KEEP)
		vxi->sched.tokens_min = vc_data.tokens_min;
	if (vc_data.tokens_max != SCHED_KEEP)
		vxi->sched.tokens_max = vc_data.tokens_max;
	if (vc_data.tokens != SCHED_KEEP)
		atomic_set(&vxi->sched.tokens, vc_data.tokens);

	/* Sanity check the resultant values */
	if (vxi->sched.fill_rate <= 0)
		vxi->sched.fill_rate = 1;
	if (vxi->sched.interval <= 0)
		vxi->sched.interval = HZ;
	if (vxi->sched.tokens_max == 0)
		vxi->sched.tokens_max = 1;
	if (atomic_read(&vxi->sched.tokens) > vxi->sched.tokens_max)
		atomic_set(&vxi->sched.tokens, vxi->sched.tokens_max);
	if (vxi->sched.tokens_min > vxi->sched.tokens_max)
		vxi->sched.tokens_min = vxi->sched.tokens_max;

	spin_unlock(&vxi->sched.tokens_lock);
	put_vx_info(vxi);
	return 0;
}


int vc_set_sched(uint32_t xid, void __user *data)
{
	struct vcmd_set_sched_v3 vc_data;
	struct vx_info *vxi;
	unsigned int set_mask;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = lookup_vx_info(xid);
	if (!vxi)
		return -ESRCH;

	set_mask = vc_data.set_mask;

	spin_lock(&vxi->sched.tokens_lock);

	if (set_mask & VXSM_FILL_RATE)
		vxi->sched.fill_rate = vc_data.fill_rate;
	if (set_mask & VXSM_INTERVAL)
		vxi->sched.interval = vc_data.interval;
	if (set_mask & VXSM_TOKENS)
		atomic_set(&vxi->sched.tokens, vc_data.tokens);
	if (set_mask & VXSM_TOKENS_MIN)
		vxi->sched.tokens_min = vc_data.tokens_min;
	if (set_mask & VXSM_TOKENS_MAX)
		vxi->sched.tokens_max = vc_data.tokens_max;
	if (set_mask & VXSM_PRIO_BIAS)
		vxi->sched.priority_bias = vc_data.priority_bias;

	/* Sanity check the resultant values */
	if (vxi->sched.fill_rate <= 0)
		vxi->sched.fill_rate = 1;
	if (vxi->sched.interval <= 0)
		vxi->sched.interval = HZ;
	if (vxi->sched.tokens_max == 0)
		vxi->sched.tokens_max = 1;
	if (atomic_read(&vxi->sched.tokens) > vxi->sched.tokens_max)
		atomic_set(&vxi->sched.tokens, vxi->sched.tokens_max);
	if (vxi->sched.tokens_min > vxi->sched.tokens_max)
		vxi->sched.tokens_min = vxi->sched.tokens_max;
	if (vxi->sched.priority_bias > MAX_PRIO_BIAS)
		vxi->sched.priority_bias = MAX_PRIO_BIAS;
	if (vxi->sched.priority_bias < MIN_PRIO_BIAS)
		vxi->sched.priority_bias = MIN_PRIO_BIAS;

	spin_unlock(&vxi->sched.tokens_lock);
	put_vx_info(vxi);
	return 0;
}

