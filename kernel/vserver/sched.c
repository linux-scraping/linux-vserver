/*
 *  linux/kernel/vserver/sched.c
 *
 *  Virtual Server: Scheduler Support
 *
 *  Copyright (C) 2004-2006  Herbert Pötzl
 *
 *  V0.01  adapted Sam Vilains version to 2.6.3
 *  V0.02  removed legacy interface
 *  V0.03  changed vcmds to vxi arg
 *
 */

#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vs_sched.h>
#include <linux/vserver/sched_cmd.h>

#include <asm/errno.h>
#include <asm/uaccess.h>

#define vxd_check_range(val, min, max) do {		\
	vxlprintk((val<min) || (val>max),		\
		"check_range(%ld,%ld,%ld)",		\
		(long)val, (long)min, (long)max, 	\
		__FILE__, __LINE__);			\
	} while (0)


void vx_update_sched_param(struct _vx_sched *sched,
	struct _vx_sched_pc *sched_pc)
{
	unsigned int set_mask = sched->update_mask;

	if (set_mask & VXSM_FILL_RATE)
		sched_pc->fill_rate[0] = sched->fill_rate[0];
	if (set_mask & VXSM_INTERVAL)
		sched_pc->interval[0] = sched->interval[0];
	if (set_mask & VXSM_FILL_RATE2)
		sched_pc->fill_rate[1] = sched->fill_rate[1];
	if (set_mask & VXSM_INTERVAL2)
		sched_pc->interval[1] = sched->interval[1];
	if (set_mask & VXSM_TOKENS)
		sched_pc->tokens = sched->tokens;
	if (set_mask & VXSM_TOKENS_MIN)
		sched_pc->tokens_min = sched->tokens_min;
	if (set_mask & VXSM_TOKENS_MAX)
		sched_pc->tokens_max = sched->tokens_max;

	if (set_mask & VXSM_IDLE_TIME)
		sched_pc->flags |= VXSF_IDLE_TIME;
	else
		sched_pc->flags &= ~VXSF_IDLE_TIME;

	/* reset time */
	sched_pc->norm_time = jiffies;
}


/*
 * recalculate the context's scheduling tokens
 *
 * ret > 0 : number of tokens available
 * ret < 0 : on hold, check delta_min[]
 *	     -1 only jiffies
 *	     -2 also idle time
 *
 */
int vx_tokens_recalc(struct _vx_sched_pc *sched_pc,
	unsigned long *norm_time, unsigned long *idle_time, int delta_min[2])
{
	long delta;
	long tokens = 0;
	int flags = sched_pc->flags;

	/* how much time did pass? */
	delta = *norm_time - sched_pc->norm_time;
	vxd_check_range(delta, 0, INT_MAX);

	if (delta >= sched_pc->interval[0]) {
		long tokens, integral;

		/* calc integral token part */
		tokens = delta / sched_pc->interval[0];
		integral = tokens * sched_pc->interval[0];
		tokens *= sched_pc->fill_rate[0];
#ifdef	CONFIG_VSERVER_HARDCPU
		delta_min[0] = delta - integral;
		vxd_check_range(delta_min[0], 0, sched_pc->interval[0]);
#endif
		/* advance time */
		sched_pc->norm_time += delta;

		/* add tokens */
		sched_pc->tokens += tokens;
		sched_pc->token_time += tokens;
	}
	else
		delta_min[0] = delta;

#ifdef	CONFIG_VSERVER_IDLETIME
	if (!(flags & VXSF_IDLE_TIME))
		goto skip_idle;

	/* how much was the idle skip? */
	delta = *idle_time - sched_pc->idle_time;
	vxd_check_range(delta, 0, INT_MAX);

	if (delta >= sched_pc->interval[1]) {
		long tokens, integral;

		/* calc fair share token part */
		tokens = delta / sched_pc->interval[1];
		integral = tokens * sched_pc->interval[1];
		tokens *= sched_pc->fill_rate[1];
		delta_min[1] = delta - integral;
		vxd_check_range(delta_min[1], 0, sched_pc->interval[1]);

		/* advance idle time */
		sched_pc->idle_time += integral;

		/* add tokens */
		sched_pc->tokens += tokens;
		sched_pc->token_time += tokens;
	}
	else
		delta_min[1] = delta;
skip_idle:
#endif

	/* clip at maximum */
	if (sched_pc->tokens > sched_pc->tokens_max)
		sched_pc->tokens = sched_pc->tokens_max;
	tokens = sched_pc->tokens;

	if ((flags & VXSF_ONHOLD)) {
		/* can we unhold? */
		if (tokens >= sched_pc->tokens_min) {
			flags &= ~VXSF_ONHOLD;
			sched_pc->hold_ticks +=
				*norm_time - sched_pc->onhold;
		}
		else
			goto on_hold;
	} else {
		/* put on hold? */
		if (tokens <= 0) {
			flags |= VXSF_ONHOLD;
			sched_pc->onhold = *norm_time;
			goto on_hold;
		}
	}
	sched_pc->flags = flags;
	return tokens;

on_hold:
	tokens = sched_pc->tokens_min - tokens;
	sched_pc->flags = flags;
	BUG_ON(tokens < 0);

#ifdef	CONFIG_VSERVER_HARDCPU
	/* next interval? */
	if (!sched_pc->fill_rate[0])
		delta_min[0] = HZ;
	else if (tokens > sched_pc->fill_rate[0])
		delta_min[0] += sched_pc->interval[0] *
			tokens / sched_pc->fill_rate[0];
	vxd_check_range(delta_min[0], 0, INT_MAX);

#ifdef	CONFIG_VSERVER_IDLETIME
	if (!(flags & VXSF_IDLE_TIME))
		return -1;

	/* next interval? */
	if (!sched_pc->fill_rate[1])
		delta_min[1] = HZ;
	else if (tokens > sched_pc->fill_rate[1])
		delta_min[1] += sched_pc->interval[1] *
			tokens / sched_pc->fill_rate[1];
	vxd_check_range(delta_min[1], 0, INT_MAX);

	return -2;
#else
	return -1;
#endif /* CONFIG_VSERVER_IDLETIME */
#else
	return 0;
#endif /* CONFIG_VSERVER_HARDCPU */
}


static int do_set_sched(struct vx_info *vxi, struct vcmd_set_sched_v4 *data)
{
	unsigned int set_mask = data->set_mask;
	unsigned int update_mask;

	/* Sanity check data values */
	if (data->fill_rate < 0)
		data->fill_rate = 1;
	if (data->interval <= 0)
		data->interval = HZ;
	if (data->tokens_max <= 0)
		data->tokens_max = HZ;
	if (data->tokens_min < 0)
		data->tokens_min = data->fill_rate*3;
	if (data->tokens_min >= data->tokens_max)
		data->tokens_min = data->tokens_max;

	if (data->prio_bias > MAX_PRIO_BIAS)
		data->prio_bias = MAX_PRIO_BIAS;
	if (data->prio_bias < MIN_PRIO_BIAS)
		data->prio_bias = MIN_PRIO_BIAS;

	spin_lock(&vxi->sched.tokens_lock);

	if (set_mask & VXSM_FILL_RATE)
		vxi->sched.fill_rate[0] = data->fill_rate;
	if (set_mask & VXSM_INTERVAL)
		vxi->sched.interval[0] = data->interval;
	if (set_mask & VXSM_FILL_RATE2)
		vxi->sched.fill_rate[1] = data->fill_rate;
	if (set_mask & VXSM_INTERVAL2)
		vxi->sched.interval[1] = data->interval;
	if (set_mask & VXSM_TOKENS)
		vxi->sched.tokens = data->tokens;
	if (set_mask & VXSM_TOKENS_MIN)
		vxi->sched.tokens_min = data->tokens_min;
	if (set_mask & VXSM_TOKENS_MAX)
		vxi->sched.tokens_max = data->tokens_max;
	if (set_mask & VXSM_PRIO_BIAS)
		vxi->sched.prio_bias = data->prio_bias;

	update_mask = vxi->sched.update_mask & VXSM_SET_MASK;
	update_mask |= (set_mask & (VXSM_SET_MASK|VXSM_IDLE_TIME));
	vxi->sched.update_mask = update_mask;
#ifdef	CONFIG_SMP
	rmb();
	if (set_mask & VXSM_CPU_ID)
		vxi->sched.update = cpumask_of_cpu(data->cpu_id);
	else
		vxi->sched.update = CPU_MASK_ALL;
	/* forced reload? */
	if (set_mask & VXSM_FORCE) {
		int cpu;

		for_each_cpu(cpu)
			vx_update_sched_param(&vxi->sched,
				&vx_per_cpu(vxi, sched_pc, cpu));
	}
#else
	/* on UP we update immediately */
	vx_update_sched_param(&vxi->sched,
		&vx_per_cpu(vxi, sched_pc, 0));
#endif

	spin_unlock(&vxi->sched.tokens_lock);
	return 0;
}


#ifdef	CONFIG_VSERVER_LEGACY

#define COPY_MASK_V2(name, mask)			\
	if (vc_data.name != SCHED_KEEP) {		\
		vc_data_v4.name = vc_data.name;		\
		vc_data_v4.set_mask |= mask;		\
	}

int vc_set_sched_v2(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_sched_v2 vc_data;
	struct vcmd_set_sched_v4 vc_data_v4 = { .set_mask = 0 };

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	COPY_MASK_V2(fill_rate,  VXSM_FILL_RATE);
	COPY_MASK_V2(interval,	 VXSM_INTERVAL);
	COPY_MASK_V2(tokens,	 VXSM_TOKENS);
	COPY_MASK_V2(tokens_min, VXSM_TOKENS_MIN);
	COPY_MASK_V2(tokens_max, VXSM_TOKENS_MAX);
	vc_data_v4.bucket_id = 0;

	do_set_sched(vxi, &vc_data_v4);
	return 0;
}
#endif

int vc_set_sched_v3(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_sched_v3 vc_data;
	struct vcmd_set_sched_v4 vc_data_v4;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	/* structures are binary compatible */
	memcpy(&vc_data_v4, &vc_data, sizeof(vc_data));
	vc_data_v4.set_mask &= VXSM_V3_MASK;
	vc_data_v4.bucket_id = 0;

	return do_set_sched(vxi, &vc_data_v4);
}

int vc_set_sched(struct vx_info *vxi, void __user *data)
{
	struct vcmd_set_sched_v4 vc_data;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_sched(vxi, &vc_data);
}

