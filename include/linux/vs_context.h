#ifndef _VS_CONTEXT_H
#define _VS_CONTEXT_H

#include "vserver/base.h"
#include "vserver/check.h"
#include "vserver/context.h"
#include "vserver/history.h"
#include "vserver/debug.h"

#include <linux/sched.h>


#define get_vx_info(i) __get_vx_info(i, __FILE__, __LINE__, __HERE__)

static inline struct vx_info *__get_vx_info(struct vx_info *vxi,
	const char *_file, int _line, void *_here)
{
	if (!vxi)
		return NULL;

	vxlprintk(VXD_CBIT(xid, 2), "get_vx_info(%p[#%d.%d])",
		vxi, vxi ? vxi->vx_id : 0,
		vxi ? atomic_read(&vxi->vx_usecnt) : 0,
		_file, _line);
	__vxh_get_vx_info(vxi, _here);

	atomic_inc(&vxi->vx_usecnt);
	return vxi;
}


extern void free_vx_info(struct vx_info *);

#define put_vx_info(i) __put_vx_info(i, __FILE__, __LINE__, __HERE__)

static inline void __put_vx_info(struct vx_info *vxi,
	const char *_file, int _line, void *_here)
{
	if (!vxi)
		return;

	vxlprintk(VXD_CBIT(xid, 2), "put_vx_info(%p[#%d.%d])",
		vxi, vxi ? vxi->vx_id : 0,
		vxi ? atomic_read(&vxi->vx_usecnt) : 0,
		_file, _line);
	__vxh_put_vx_info(vxi, _here);

	if (atomic_dec_and_test(&vxi->vx_usecnt))
		free_vx_info(vxi);
}


#define init_vx_info(p, i) \
	__init_vx_info(p, i, __FILE__, __LINE__, __HERE__)

static inline void __init_vx_info(struct vx_info **vxp, struct vx_info *vxi,
	const char *_file, int _line, void *_here)
{
	if (vxi) {
		vxlprintk(VXD_CBIT(xid, 3),
			"init_vx_info(%p[#%d.%d])",
			vxi, vxi ? vxi->vx_id : 0,
			vxi ? atomic_read(&vxi->vx_usecnt) : 0,
			_file, _line);
		__vxh_init_vx_info(vxi, vxp, _here);

		atomic_inc(&vxi->vx_usecnt);
	}
	*vxp = vxi;
}


#define set_vx_info(p, i) \
	__set_vx_info(p, i, __FILE__, __LINE__, __HERE__)

static inline void __set_vx_info(struct vx_info **vxp, struct vx_info *vxi,
	const char *_file, int _line, void *_here)
{
	struct vx_info *vxo;

	if (!vxi)
		return;

	vxlprintk(VXD_CBIT(xid, 3), "set_vx_info(%p[#%d.%d])",
		vxi, vxi ? vxi->vx_id : 0,
		vxi ? atomic_read(&vxi->vx_usecnt) : 0,
		_file, _line);
	__vxh_set_vx_info(vxi, vxp, _here);

	atomic_inc(&vxi->vx_usecnt);
	vxo = xchg(vxp, vxi);
	BUG_ON(vxo);
}


#define clr_vx_info(p) __clr_vx_info(p, __FILE__, __LINE__, __HERE__)

static inline void __clr_vx_info(struct vx_info **vxp,
	const char *_file, int _line, void *_here)
{
	struct vx_info *vxo;

	vxo = xchg(vxp, NULL);
	if (!vxo)
		return;

	vxlprintk(VXD_CBIT(xid, 3), "clr_vx_info(%p[#%d.%d])",
		vxo, vxo ? vxo->vx_id : 0,
		vxo ? atomic_read(&vxo->vx_usecnt) : 0,
		_file, _line);
	__vxh_clr_vx_info(vxo, vxp, _here);

	if (atomic_dec_and_test(&vxo->vx_usecnt))
		free_vx_info(vxo);
}


#define claim_vx_info(v, p) \
	__claim_vx_info(v, p, __FILE__, __LINE__, __HERE__)

static inline void __claim_vx_info(struct vx_info *vxi,
	struct task_struct *task,
	const char *_file, int _line, void *_here)
{
	vxlprintk(VXD_CBIT(xid, 3), "claim_vx_info(%p[#%d.%d.%d]) %p",
		vxi, vxi ? vxi->vx_id : 0,
		vxi ? atomic_read(&vxi->vx_usecnt) : 0,
		vxi ? atomic_read(&vxi->vx_tasks) : 0,
		task, _file, _line);
	__vxh_claim_vx_info(vxi, task, _here);

	atomic_inc(&vxi->vx_tasks);
}


extern void unhash_vx_info(struct vx_info *);

#define release_vx_info(v, p) \
	__release_vx_info(v, p, __FILE__, __LINE__, __HERE__)

static inline void __release_vx_info(struct vx_info *vxi,
	struct task_struct *task,
	const char *_file, int _line, void *_here)
{
	vxlprintk(VXD_CBIT(xid, 3), "release_vx_info(%p[#%d.%d.%d]) %p",
		vxi, vxi ? vxi->vx_id : 0,
		vxi ? atomic_read(&vxi->vx_usecnt) : 0,
		vxi ? atomic_read(&vxi->vx_tasks) : 0,
		task, _file, _line);
	__vxh_release_vx_info(vxi, task, _here);

	might_sleep();

	if (atomic_dec_and_test(&vxi->vx_tasks))
		unhash_vx_info(vxi);
}


#define task_get_vx_info(p) \
	__task_get_vx_info(p, __FILE__, __LINE__, __HERE__)

static inline struct vx_info *__task_get_vx_info(struct task_struct *p,
	const char *_file, int _line, void *_here)
{
	struct vx_info *vxi;

	task_lock(p);
	vxlprintk(VXD_CBIT(xid, 5), "task_get_vx_info(%p)",
		p, _file, _line);
	vxi = __get_vx_info(p->vx_info, _file, _line, _here);
	task_unlock(p);
	return vxi;
}


static inline void __wakeup_vx_info(struct vx_info *vxi)
{
	if (waitqueue_active(&vxi->vx_wait))
		wake_up_interruptible(&vxi->vx_wait);
}


#define enter_vx_info(v, s) __enter_vx_info(v, s, __FILE__, __LINE__)

static inline void __enter_vx_info(struct vx_info *vxi,
	struct vx_info_save *vxis, const char *_file, int _line)
{
	vxlprintk(VXD_CBIT(xid, 5), "enter_vx_info(%p[#%d],%p) %p[#%d,%p]",
		vxi, vxi ? vxi->vx_id : 0, vxis, current,
		current->xid, current->vx_info, _file, _line);
	vxis->vxi = xchg(&current->vx_info, vxi);
	vxis->xid = current->xid;
	current->xid = vxi ? vxi->vx_id : 0;
}

#define leave_vx_info(s) __leave_vx_info(s, __FILE__, __LINE__)

static inline void __leave_vx_info(struct vx_info_save *vxis,
	const char *_file, int _line)
{
	vxlprintk(VXD_CBIT(xid, 5), "leave_vx_info(%p[#%d,%p]) %p[#%d,%p]",
		vxis, vxis->xid, vxis->vxi, current,
		current->xid, current->vx_info, _file, _line);
	(void)xchg(&current->vx_info, vxis->vxi);
	current->xid = vxis->xid;
}


static inline void __enter_vx_admin(struct vx_info_save *vxis)
{
	vxis->vxi = xchg(&current->vx_info, NULL);
	vxis->xid = xchg(&current->xid, (vxid_t)0);
}

static inline void __leave_vx_admin(struct vx_info_save *vxis)
{
	(void)xchg(&current->xid, vxis->xid);
	(void)xchg(&current->vx_info, vxis->vxi);
}

#define task_is_init(p) \
	__task_is_init(p, __FILE__, __LINE__, __HERE__)

static inline int __task_is_init(struct task_struct *p,
	const char *_file, int _line, void *_here)
{
	int is_init = is_global_init(p);

	task_lock(p);
	if (p->vx_info)
		is_init = p->vx_info->vx_initpid == p->pid;
	task_unlock(p);
	return is_init;
}

extern void exit_vx_info(struct task_struct *, int);
extern void exit_vx_info_early(struct task_struct *, int);


#else
#warning duplicate inclusion
#endif
