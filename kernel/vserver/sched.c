/*
 *  linux/kernel/vserver/sched.c
 *
 *  Virtual Server: Scheduler Support
 *
 *  Copyright (C) 2004-2010  Herbert Pötzl
 *
 *  V0.01  adapted Sam Vilains version to 2.6.3
 *  V0.02  removed legacy interface
 *  V0.03  changed vcmds to vxi arg
 *  V0.04  removed older and legacy interfaces
 *  V0.05  removed scheduler code/commands
 *
 */

#include <linux/vs_context.h>
#include <linux/vs_sched.h>
#include <linux/cpumask.h>
#include <linux/vserver/sched_cmd.h>

#include <asm/uaccess.h>


void vx_update_sched_param(struct _vx_sched *sched,
	struct _vx_sched_pc *sched_pc)
{
	sched_pc->prio_bias = sched->prio_bias;
}

static int do_set_prio_bias(struct vx_info *vxi, struct vcmd_prio_bias *data)
{
	int cpu;

	if (data->prio_bias > MAX_PRIO_BIAS)
		data->prio_bias = MAX_PRIO_BIAS;
	if (data->prio_bias < MIN_PRIO_BIAS)
		data->prio_bias = MIN_PRIO_BIAS;

	if (data->cpu_id != ~0) {
		vxi->sched.update = cpumask_of_cpu(data->cpu_id);
		cpumask_and(&vxi->sched.update, &vxi->sched.update,
			cpu_online_mask);
	} else
		cpumask_copy(&vxi->sched.update, cpu_online_mask);

	for_each_cpu_mask(cpu, vxi->sched.update)
		vx_update_sched_param(&vxi->sched,
			&vx_per_cpu(vxi, sched_pc, cpu));
	return 0;
}

int vc_set_prio_bias(struct vx_info *vxi, void __user *data)
{
	struct vcmd_prio_bias vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_prio_bias(vxi, &vc_data);
}

int vc_get_prio_bias(struct vx_info *vxi, void __user *data)
{
	struct vcmd_prio_bias vc_data;
	struct _vx_sched_pc *pcd;
	int cpu;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	cpu = vc_data.cpu_id;

	if (!cpu_possible(cpu))
		return -EINVAL;

	pcd = &vx_per_cpu(vxi, sched_pc, cpu);
	vc_data.prio_bias = pcd->prio_bias;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

