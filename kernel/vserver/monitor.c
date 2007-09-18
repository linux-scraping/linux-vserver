/*
 *  kernel/vserver/monitor.c
 *
 *  Virtual Context Scheduler Monitor
 *
 *  Copyright (C) 2006-2007 Herbert Pötzl
 *
 *  V0.01  basic design
 *
 */

#include <linux/module.h>
#include <linux/jiffies.h>
#include <asm/uaccess.h>
#include <asm/atomic.h>

#include <linux/vserver/monitor.h>
#include <linux/vserver/debug_cmd.h>


#ifdef	CONFIG_VSERVER_MONITOR
#define VXM_SIZE	CONFIG_VSERVER_MONITOR_SIZE
#else
#define VXM_SIZE	64
#endif

struct _vx_monitor {
	unsigned int counter;

	struct _vx_mon_entry entry[VXM_SIZE+1];
};


DEFINE_PER_CPU(struct _vx_monitor, vx_monitor_buffer);

unsigned volatile int vxm_active = 1;

static atomic_t sequence = ATOMIC_INIT(0);


/*	vxm_advance()

	* requires disabled preemption				*/

struct _vx_mon_entry *vxm_advance(int cpu)
{
	struct _vx_monitor *mon = &per_cpu(vx_monitor_buffer, cpu);
	struct _vx_mon_entry *entry;
	unsigned int index;

	index = vxm_active ? (mon->counter++ % VXM_SIZE) : VXM_SIZE;
	entry = &mon->entry[index];

	entry->ev.seq = atomic_inc_return(&sequence);
	entry->ev.jif = jiffies;
	return entry;
}

EXPORT_SYMBOL_GPL(vxm_advance);


int do_read_monitor(struct __user _vx_mon_entry *data,
	int cpu, uint32_t *index, uint32_t *count)
{
	int pos, ret = 0;
	struct _vx_monitor *mon = &per_cpu(vx_monitor_buffer, cpu);
	int end = mon->counter;
	int start = end - VXM_SIZE + 2;
	int idx = *index;

	/* special case: get current pos */
	if (!*count) {
		*index = end;
		return 0;
	}

	/* have we lost some data? */
	if (idx < start)
		idx = start;

	for (pos = 0; (pos < *count) && (idx < end); pos++, idx++) {
		struct _vx_mon_entry *entry =
			&mon->entry[idx % VXM_SIZE];

		/* send entry to userspace */
		ret = copy_to_user(&data[pos], entry, sizeof(*entry));
		if (ret)
			break;
	}
	/* save new index and count */
	*index = idx;
	*count = pos;
	return ret ? ret : (*index < end);
}

int vc_read_monitor(uint32_t id, void __user *data)
{
	struct vcmd_read_monitor_v0 vc_data;
	int ret;

	if (id >= NR_CPUS)
		return -EINVAL;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_read_monitor((struct __user _vx_mon_entry *)vc_data.data,
		id, &vc_data.index, &vc_data.count);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return ret;
}

#ifdef	CONFIG_COMPAT

int vc_read_monitor_x32(uint32_t id, void __user *data)
{
	struct vcmd_read_monitor_v0_x32 vc_data;
	int ret;

	if (id >= NR_CPUS)
		return -EINVAL;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_read_monitor((struct __user _vx_mon_entry *)
		compat_ptr(vc_data.data_ptr),
		id, &vc_data.index, &vc_data.count);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return ret;
}

#endif	/* CONFIG_COMPAT */

