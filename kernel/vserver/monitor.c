/*
 *  kernel/vserver/monitor.c
 *
 *  Virtual Context Scheduler Monitor
 *
 *  Copyright (C) 2006 Herbert Pötzl
 *
 *  V0.01  basic design
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ctype.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/unistd.h>

#include <linux/vserver/monitor.h>


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

