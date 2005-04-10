#ifndef _VX_VS_CONTEXT_H
#define _VX_VS_CONTEXT_H


#include <linux/kernel.h>
#include "vserver/debug.h"


#define get_vx_info(i)	__get_vx_info(i,__FILE__,__LINE__)

static inline struct vx_info *__get_vx_info(struct vx_info *vxi,
	const char *_file, int _line)
{
	if (!vxi)
		return NULL;

	vxlprintk(VXD_CBIT(xid, 2), "get_vx_info(%p[#%d.%d])",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_usecnt):0,
		_file, _line);
	vxh_get_vx_info(vxi);

	atomic_inc(&vxi->vx_usecnt);
	return vxi;
}


extern void free_vx_info(struct vx_info *);

#define put_vx_info(i)	__put_vx_info(i,__FILE__,__LINE__)

static inline void __put_vx_info(struct vx_info *vxi, const char *_file, int _line)
{
	if (!vxi)
		return;
	vxlprintk(VXD_CBIT(xid, 2), "put_vx_info(%p[#%d.%d])",
		vxi, vxi?vxi->vx_id:0, vxi?atomic_read(&vxi->vx_usecnt):0,
		_file, _line);
	vxh_put_vx_info(vxi);

	if (atomic_dec_and_test(&vxi->vx_usecnt))
		free_vx_info(vxi);
}


#define init_vx_info(p,i) __init_vx_info(p,i,__FILE__,__LINE__)

static inline void __init_vx_info(struct vx_info **vxp, struct vx_info *vxi,
		const char *_file, int _line)
{
	if (vxi) {
		vxlprintk(VXD_CBIT(xid, 3),
			"init_vx_info(%p[#%d.%d])",
			vxi, vxi?vxi->vx_id:0,
			vxi?atomic_read(&vxi->vx_usecnt):0,
			_file, _line);
		vxh_init_vx_info(vxi, vxp);

		atomic_inc(&vxi->vx_usecnt);
	}
	*vxp = vxi;
}


#define set_vx_info(p,i) __set_vx_info(p,i,__FILE__,__LINE__)

static inline void __set_vx_info(struct vx_info **vxp, struct vx_info *vxi,
	const char *_file, int _line)
{
	struct vx_info *vxo;

	if (!vxi)
		return;

	vxlprintk(VXD_CBIT(xid, 3), "set_vx_info(%p[#%d.%d])",
		vxi, vxi?vxi->vx_id:0,
		vxi?atomic_read(&vxi->vx_usecnt):0,
		_file, _line);
	vxh_set_vx_info(vxi, vxp);

	// vxo = xchg(vxp, __get_vx_info(vxi, _file, _line));
	atomic_inc(&vxi->vx_usecnt);
	vxo = xchg(vxp, vxi);
	BUG_ON(vxo);
}


#define clr_vx_info(p) __clr_vx_info(p,__FILE__,__LINE__)

static inline void __clr_vx_info(struct vx_info **vxp,
	const char *_file, int _line)
{
	struct vx_info *vxo;

	vxo = xchg(vxp, NULL);
	if (!vxo)
		return;

	vxlprintk(VXD_CBIT(xid, 3), "clr_vx_info(%p[#%d.%d])",
		vxo, vxo?vxo->vx_id:0,
		vxo?atomic_read(&vxo->vx_usecnt):0,
		_file, _line);
	vxh_clr_vx_info(vxo, vxp);

	// __put_vx_info(vxo, _file, _line);
	if (atomic_dec_and_test(&vxo->vx_usecnt))
		free_vx_info(vxo);
}


#define claim_vx_info(v,p) __claim_vx_info(v,p,__FILE__,__LINE__)

static inline void __claim_vx_info(struct vx_info *vxi,
	struct task_struct *task, const char *_file, int _line)
{
	vxlprintk(VXD_CBIT(xid, 3), "claim_vx_info(%p[#%d.%d.%d]) %p",
		vxi, vxi?vxi->vx_id:0,
		vxi?atomic_read(&vxi->vx_usecnt):0,
		vxi?atomic_read(&vxi->vx_tasks):0,
		task, _file, _line);
	vxh_claim_vx_info(vxi, task);

	atomic_inc(&vxi->vx_tasks);
}


extern void unhash_vx_info(struct vx_info *);

#define release_vx_info(v,p) __release_vx_info(v,p,__FILE__,__LINE__)

static inline void __release_vx_info(struct vx_info *vxi,
	struct task_struct *task, const char *_file, int _line)
{
	vxlprintk(VXD_CBIT(xid, 3), "release_vx_info(%p[#%d.%d.%d]) %p",
		vxi, vxi?vxi->vx_id:0,
		vxi?atomic_read(&vxi->vx_usecnt):0,
		vxi?atomic_read(&vxi->vx_tasks):0,
		task, _file, _line);
	vxh_release_vx_info(vxi, task);

	might_sleep();

	if (atomic_dec_and_test(&vxi->vx_tasks))
		unhash_vx_info(vxi);
}


#define task_get_vx_info(p)	__task_get_vx_info(p,__FILE__,__LINE__)

static __inline__ struct vx_info *__task_get_vx_info(struct task_struct *p,
	const char *_file, int _line)
{
	struct vx_info *vxi;

	task_lock(p);
	vxlprintk(VXD_CBIT(xid, 5), "task_get_vx_info(%p)",
		p, _file, _line);
	vxi = __get_vx_info(p->vx_info, _file, _line);
	task_unlock(p);
	return vxi;
}


#else
#warning duplicate inclusion
#endif
