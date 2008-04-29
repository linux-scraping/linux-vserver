#ifndef _NX_VS_NETWORK_H
#define _NX_VS_NETWORK_H

#include "vserver/context.h"
#include "vserver/network.h"
#include "vserver/base.h"
#include "vserver/check.h"
#include "vserver/debug.h"

#include <linux/sched.h>


#define get_nx_info(i) __get_nx_info(i, __FILE__, __LINE__)

static inline struct nx_info *__get_nx_info(struct nx_info *nxi,
	const char *_file, int _line)
{
	if (!nxi)
		return NULL;

	vxlprintk(VXD_CBIT(nid, 2), "get_nx_info(%p[#%d.%d])",
		nxi, nxi ? nxi->nx_id : 0,
		nxi ? atomic_read(&nxi->nx_usecnt) : 0,
		_file, _line);

	atomic_inc(&nxi->nx_usecnt);
	return nxi;
}


extern void free_nx_info(struct nx_info *);

#define put_nx_info(i) __put_nx_info(i, __FILE__, __LINE__)

static inline void __put_nx_info(struct nx_info *nxi, const char *_file, int _line)
{
	if (!nxi)
		return;

	vxlprintk(VXD_CBIT(nid, 2), "put_nx_info(%p[#%d.%d])",
		nxi, nxi ? nxi->nx_id : 0,
		nxi ? atomic_read(&nxi->nx_usecnt) : 0,
		_file, _line);

	if (atomic_dec_and_test(&nxi->nx_usecnt))
		free_nx_info(nxi);
}


#define init_nx_info(p, i) __init_nx_info(p, i, __FILE__, __LINE__)

static inline void __init_nx_info(struct nx_info **nxp, struct nx_info *nxi,
		const char *_file, int _line)
{
	if (nxi) {
		vxlprintk(VXD_CBIT(nid, 3),
			"init_nx_info(%p[#%d.%d])",
			nxi, nxi ? nxi->nx_id : 0,
			nxi ? atomic_read(&nxi->nx_usecnt) : 0,
			_file, _line);

		atomic_inc(&nxi->nx_usecnt);
	}
	*nxp = nxi;
}


#define set_nx_info(p, i) __set_nx_info(p, i, __FILE__, __LINE__)

static inline void __set_nx_info(struct nx_info **nxp, struct nx_info *nxi,
	const char *_file, int _line)
{
	struct nx_info *nxo;

	if (!nxi)
		return;

	vxlprintk(VXD_CBIT(nid, 3), "set_nx_info(%p[#%d.%d])",
		nxi, nxi ? nxi->nx_id : 0,
		nxi ? atomic_read(&nxi->nx_usecnt) : 0,
		_file, _line);

	atomic_inc(&nxi->nx_usecnt);
	nxo = xchg(nxp, nxi);
	BUG_ON(nxo);
}

#define clr_nx_info(p) __clr_nx_info(p, __FILE__, __LINE__)

static inline void __clr_nx_info(struct nx_info **nxp,
	const char *_file, int _line)
{
	struct nx_info *nxo;

	nxo = xchg(nxp, NULL);
	if (!nxo)
		return;

	vxlprintk(VXD_CBIT(nid, 3), "clr_nx_info(%p[#%d.%d])",
		nxo, nxo ? nxo->nx_id : 0,
		nxo ? atomic_read(&nxo->nx_usecnt) : 0,
		_file, _line);

	if (atomic_dec_and_test(&nxo->nx_usecnt))
		free_nx_info(nxo);
}


#define claim_nx_info(v, p) __claim_nx_info(v, p, __FILE__, __LINE__)

static inline void __claim_nx_info(struct nx_info *nxi,
	struct task_struct *task, const char *_file, int _line)
{
	vxlprintk(VXD_CBIT(nid, 3), "claim_nx_info(%p[#%d.%d.%d]) %p",
		nxi, nxi ? nxi->nx_id : 0,
		nxi?atomic_read(&nxi->nx_usecnt):0,
		nxi?atomic_read(&nxi->nx_tasks):0,
		task, _file, _line);

	atomic_inc(&nxi->nx_tasks);
}


extern void unhash_nx_info(struct nx_info *);

#define release_nx_info(v, p) __release_nx_info(v, p, __FILE__, __LINE__)

static inline void __release_nx_info(struct nx_info *nxi,
	struct task_struct *task, const char *_file, int _line)
{
	vxlprintk(VXD_CBIT(nid, 3), "release_nx_info(%p[#%d.%d.%d]) %p",
		nxi, nxi ? nxi->nx_id : 0,
		nxi ? atomic_read(&nxi->nx_usecnt) : 0,
		nxi ? atomic_read(&nxi->nx_tasks) : 0,
		task, _file, _line);

	might_sleep();

	if (atomic_dec_and_test(&nxi->nx_tasks))
		unhash_nx_info(nxi);
}


#define task_get_nx_info(i)	__task_get_nx_info(i, __FILE__, __LINE__)

static __inline__ struct nx_info *__task_get_nx_info(struct task_struct *p,
	const char *_file, int _line)
{
	struct nx_info *nxi;

	task_lock(p);
	vxlprintk(VXD_CBIT(nid, 5), "task_get_nx_info(%p)",
		p, _file, _line);
	nxi = __get_nx_info(p->nx_info, _file, _line);
	task_unlock(p);
	return nxi;
}


static inline void exit_nx_info(struct task_struct *p)
{
	if (p->nx_info)
		release_nx_info(p->nx_info, p);
}


#else
#warning duplicate inclusion
#endif
