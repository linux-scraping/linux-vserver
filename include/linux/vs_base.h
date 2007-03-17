#ifndef _VX_VS_BASE_H
#define _VX_VS_BASE_H

#include "vserver/context.h"


#define vx_task_xid(t)	((t)->xid)

#define vx_current_xid() vx_task_xid(current)

#define current_vx_info() (current->vx_info)

#define vx_check(c,m)	__vx_check(vx_current_xid(),c,m)

#define vx_weak_check(c,m)	((m) ? vx_check(c,m) : 1)


/*
 * check current context for ADMIN/WATCH and
 * optionally against supplied argument
 */
static inline int __vx_check(xid_t cid, xid_t id, unsigned int mode)
{
	if (mode & VX_ARG_MASK) {
		if ((mode & VX_IDENT) &&
			(id == cid))
			return 1;
	}
	if (mode & VX_ATR_MASK) {
		if ((mode & VX_DYNAMIC) &&
			(id >= MIN_D_CONTEXT) &&
			(id <= MAX_S_CONTEXT))
			return 1;
		if ((mode & VX_STATIC) &&
			(id > 1) && (id < MIN_D_CONTEXT))
			return 1;
	}
	return (((mode & VX_ADMIN) && (cid == 0)) ||
		((mode & VX_WATCH) && (cid == 1)) ||
		((mode & VX_HOSTID) && (id == 0)));
}


#define __vx_state(v)	((v) ? ((v)->vx_state) : 0)

#define vx_info_state(v,m)	(__vx_state(v) & (m))


/* generic flag merging */

#define vx_check_flags(v,m,f)	(((v) & (m)) ^ (f))

#define vx_mask_flags(v,f,m)	(((v) & ~(m)) | ((f) & (m)))

#define vx_mask_mask(v,f,m)	(((v) & ~(m)) | ((v) & (f) & (m)))

#define vx_check_bit(v,n)	((v) & (1LL << (n)))


/* context flags */

#define __vx_flags(v)	((v) ? (v)->vx_flags : 0)

#define vx_current_flags()	__vx_flags(current->vx_info)

#define vx_info_flags(v,m,f) \
	vx_check_flags(__vx_flags(v),(m),(f))

#define task_vx_flags(t,m,f) \
	((t) && vx_info_flags((t)->vx_info, (m), (f)))

#define vx_flags(m,f)	vx_info_flags(current->vx_info,(m),(f))


/* context caps */

#define __vx_ccaps(v)	((v) ? (v)->vx_ccaps : 0)

#define vx_current_ccaps()	__vx_ccaps(current->vx_info)

#define vx_info_ccaps(v,c)	(__vx_ccaps(v) & (c))

#define vx_ccaps(c)	vx_info_ccaps(current->vx_info,(c))


#define __vx_mcaps(v)	((v) ? (v)->vx_ccaps >> 32UL : ~0 )

#define vx_info_mcaps(v,c)	(__vx_mcaps(v) & (c))

#define vx_mcaps(c)	vx_info_mcaps(current->vx_info,(c))


#define vx_current_bcaps() \
	(((current->vx_info) && !vx_flags(VXF_STATE_SETUP, 0)) ? \
	current->vx_info->vx_bcaps : cap_bset)


#define vx_current_initpid(n) \
	(current->vx_info && \
	(current->vx_info->vx_initpid == (n)))

#define vx_capable(b,c) (capable(b) || \
	((current->euid == 0) && vx_ccaps(c)))


#else
#warning duplicate inclusion
#endif
