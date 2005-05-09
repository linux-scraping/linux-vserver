#ifndef _NX_VS_NETWORK_H
#define _NX_VS_NETWORK_H


#include "vserver/network.h"
#include "vserver/debug.h"


#define get_nx_info(i)	__get_nx_info(i,__FILE__,__LINE__)

static inline struct nx_info *__get_nx_info(struct nx_info *nxi,
	const char *_file, int _line)
{
	if (!nxi)
		return NULL;
	vxlprintk(VXD_CBIT(nid, 2), "get_nx_info(%p[#%d.%d])",
		nxi, nxi?nxi->nx_id:0, nxi?atomic_read(&nxi->nx_usecnt):0,
		_file, _line);
	atomic_inc(&nxi->nx_usecnt);
	return nxi;
}


extern void free_nx_info(struct nx_info *);

#define put_nx_info(i)	__put_nx_info(i,__FILE__,__LINE__)

static inline void __put_nx_info(struct nx_info *nxi, const char *_file, int _line)
{
	if (!nxi)
		return;
	vxlprintk(VXD_CBIT(nid, 2), "put_nx_info(%p[#%d.%d])",
		nxi, nxi?nxi->nx_id:0, nxi?atomic_read(&nxi->nx_usecnt):0,
		_file, _line);
	if (atomic_dec_and_test(&nxi->nx_usecnt))
		free_nx_info(nxi);
}

#define set_nx_info(p,i) __set_nx_info(p,i,__FILE__,__LINE__)

static inline void __set_nx_info(struct nx_info **nxp, struct nx_info *nxi,
	const char *_file, int _line)
{
	struct nx_info *nxo;

	if (!nxi)
		return;

	vxlprintk(VXD_CBIT(nid, 3), "set_nx_info(%p[#%d.%d.%d])",
		nxi, nxi?nxi->nx_id:0,
		nxi?atomic_read(&nxi->nx_usecnt):0,
		nxi?atomic_read(&nxi->nx_refcnt):0,
		_file, _line);

	atomic_inc(&nxi->nx_refcnt);
	// nxo = xchg(nxp, __get_nx_info(nxi, _file, _line));
	nxo = xchg(nxp, nxi);
	BUG_ON(nxo);
}

#define clr_nx_info(p)	__clr_nx_info(p,__FILE__,__LINE__)

static inline void __clr_nx_info(struct nx_info **nxp,
	const char *_file, int _line)
{
	struct nx_info *nxo;

	nxo = xchg(nxp, NULL);
	if (!nxo)
		return;

	vxlprintk(VXD_CBIT(nid, 3), "clr_nx_info(%p[#%d.%d.%d])",
		nxo, nxo?nxo->nx_id:0,
		nxo?atomic_read(&nxo->nx_usecnt):0,
		nxo?atomic_read(&nxo->nx_refcnt):0,
		_file, _line);

	if (atomic_dec_and_test(&nxo->nx_refcnt))
		unhash_nx_info(nxo);
	// __put_nx_info(nxo, _file, _line);
}


#define task_get_nx_info(i)	__task_get_nx_info(i,__FILE__,__LINE__)

static __inline__ struct nx_info *__task_get_nx_info(struct task_struct *p,
	const char *_file, int _line)
{
	struct nx_info *nxi;

	task_lock(p);
	nxi = __get_nx_info(p->nx_info, _file, _line);
	vxlprintk(VXD_CBIT(nid, 5), "task_get_nx_info(%p)",
		p, _file, _line);
	task_unlock(p);
	return nxi;
}


#define nx_task_nid(t)	((t)->nid)

#define nx_current_nid() nx_task_nid(current)

#define nx_check(c,m)	__nx_check(nx_current_nid(),c,m)

#define nx_weak_check(c,m)	((m) ? nx_check(c,m) : 1)


#define __nx_flags(v,m,f)	(((v) & (m)) ^ (f))

#define __nx_task_flags(t,m,f) \
	(((t) && ((t)->nx_info)) ? \
		__nx_flags((t)->nx_info->nx_flags,(m),(f)) : 0)

#define nx_current_flags() \
	((current->nx_info) ? current->nx_info->nx_flags : 0)

#define nx_flags(m,f)	__nx_flags(nx_current_flags(),(m),(f))


#define nx_current_ncaps() \
	((current->nx_info) ? current->nx_info->nx_ncaps : 0)

#define nx_ncaps(c)	(nx_current_ncaps() & (c))


static inline int addr_in_nx_info(struct nx_info *nxi, uint32_t addr)
{
	int n,i;

	if (!nxi)
		return 1;

	n = nxi->nbipv4;
	for (i=0; i<n; i++) {
		if (nxi->ipv4[i] == addr)
			return 1;
	}
	return 0;
}


#else
#warning duplicate inclusion
#endif
