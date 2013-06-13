/*
 *  linux/kernel/vserver/tag.c
 *
 *  Virtual Server: Shallow Tag Space
 *
 *  Copyright (C) 2007  Herbert Pötzl
 *
 *  V0.01  basic implementation
 *
 */

#include <linux/sched.h>
#include <linux/vserver/debug.h>
#include <linux/vs_pid.h>
#include <linux/vs_tag.h>

#include <linux/vserver/tag_cmd.h>


int dx_migrate_task(struct task_struct *p, vtag_t tag)
{
	if (!p)
		BUG();

	vxdprintk(VXD_CBIT(tag, 5),
		"dx_migrate_task(%p[#%d],#%d)", p, p->tag, tag);

	task_lock(p);
	p->tag = tag;
	task_unlock(p);

	vxdprintk(VXD_CBIT(tag, 5),
		"moved task %p into [#%d]", p, tag);
	return 0;
}

/* vserver syscall commands below here */

/* taks xid and vx_info functions */


int vc_task_tag(uint32_t id)
{
	vtag_t tag;

	if (id) {
		struct task_struct *tsk;
		rcu_read_lock();
		tsk = find_task_by_real_pid(id);
		tag = (tsk) ? tsk->tag : -ESRCH;
		rcu_read_unlock();
	} else
		tag = dx_current_tag();
	return tag;
}


int vc_tag_migrate(uint32_t tag)
{
	return dx_migrate_task(current, tag & 0xFFFF);
}


