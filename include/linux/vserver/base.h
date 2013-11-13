#ifndef _VSERVER_BASE_H
#define _VSERVER_BASE_H


/* context state changes */

enum {
	VSC_STARTUP = 1,
	VSC_SHUTDOWN,

	VSC_NETUP,
	VSC_NETDOWN,
};



#define vx_task_xid(t)	((t)->xid)

#define vx_current_xid() vx_task_xid(current)

#define current_vx_info() (current->vx_info)


#define nx_task_nid(t)	((t)->nid)

#define nx_current_nid() nx_task_nid(current)

#define current_nx_info() (current->nx_info)


/* generic flag merging */

#define vs_check_flags(v, m, f)	(((v) & (m)) ^ (f))

#define vs_mask_flags(v, f, m)	(((v) & ~(m)) | ((f) & (m)))

#define vs_mask_mask(v, f, m)	(((v) & ~(m)) | ((v) & (f) & (m)))

#define vs_check_bit(v, n)	((v) & (1LL << (n)))


/* context flags */

#define __vx_flags(v)	((v) ? (v)->vx_flags : 0)

#define vx_current_flags()	__vx_flags(current_vx_info())

#define vx_info_flags(v, m, f) \
	vs_check_flags(__vx_flags(v), m, f)

#define task_vx_flags(t, m, f) \
	((t) && vx_info_flags((t)->vx_info, m, f))

#define vx_flags(m, f)	vx_info_flags(current_vx_info(), m, f)


/* context caps */

#define __vx_ccaps(v)	((v) ? (v)->vx_ccaps : 0)

#define vx_current_ccaps()	__vx_ccaps(current_vx_info())

#define vx_info_ccaps(v, c)	(__vx_ccaps(v) & (c))

#define vx_ccaps(c)	vx_info_ccaps(current_vx_info(), (c))



/* network flags */

#define __nx_flags(n)	((n) ? (n)->nx_flags : 0)

#define nx_current_flags()	__nx_flags(current_nx_info())

#define nx_info_flags(n, m, f) \
	vs_check_flags(__nx_flags(n), m, f)

#define task_nx_flags(t, m, f) \
	((t) && nx_info_flags((t)->nx_info, m, f))

#define nx_flags(m, f)	nx_info_flags(current_nx_info(), m, f)


/* network caps */

#define __nx_ncaps(n)	((n) ? (n)->nx_ncaps : 0)

#define nx_current_ncaps()	__nx_ncaps(current_nx_info())

#define nx_info_ncaps(n, c)	(__nx_ncaps(n) & (c))

#define nx_ncaps(c)	nx_info_ncaps(current_nx_info(), c)


/* context mask capabilities */

#define __vx_mcaps(v)	((v) ? (v)->vx_ccaps >> 32UL : ~0 )

#define vx_info_mcaps(v, c)	(__vx_mcaps(v) & (c))

#define vx_mcaps(c)	vx_info_mcaps(current_vx_info(), c)


/* context bcap mask */

#define __vx_bcaps(v)		((v)->vx_bcaps)

#define vx_current_bcaps()	__vx_bcaps(current_vx_info())


/* mask given bcaps */

#define vx_info_mbcaps(v, c)	((v) ? cap_intersect(__vx_bcaps(v), c) : c)

#define vx_mbcaps(c)		vx_info_mbcaps(current_vx_info(), c)


/* masked cap_bset */

#define vx_info_cap_bset(v)	vx_info_mbcaps(v, current->cap_bset)

#define vx_current_cap_bset()	vx_info_cap_bset(current_vx_info())

#if 0
#define vx_info_mbcap(v, b) \
	(!vx_info_flags(v, VXF_STATE_SETUP, 0) ? \
	vx_info_bcaps(v, b) : (b))

#define task_vx_mbcap(t, b) \
	vx_info_mbcap((t)->vx_info, (t)->b)

#define vx_mbcap(b)	task_vx_mbcap(current, b)
#endif

#define vx_cap_raised(v, c, f)	cap_raised(vx_info_mbcaps(v, c), f)

#define vx_capable(b, c) (capable(b) || \
	(cap_raised(current_cap(), b) && vx_ccaps(c)))

#define vx_ns_capable(n, b, c) (ns_capable(n, b) || \
	(cap_raised(current_cap(), b) && vx_ccaps(c)))

#define nx_capable(b, c) (capable(b) || \
	(cap_raised(current_cap(), b) && nx_ncaps(c)))

#define nx_ns_capable(n, b, c) (ns_capable(n, b) || \
	(cap_raised(current_cap(), b) && nx_ncaps(c)))

#define vx_task_initpid(t, n) \
	((t)->vx_info && \
	((t)->vx_info->vx_initpid == (n)))

#define vx_current_initpid(n)	vx_task_initpid(current, n)


/* context unshare mask */

#define __vx_umask(v)		((v)->vx_umask)

#define vx_current_umask()	__vx_umask(current_vx_info())

#define vx_can_unshare(b, f) (capable(b) || \
	(cap_raised(current_cap(), b) && \
	!((f) & ~vx_current_umask())))

#define vx_ns_can_unshare(n, b, f) (ns_capable(n, b) || \
	(cap_raised(current_cap(), b) && \
	!((f) & ~vx_current_umask())))

#define __vx_wmask(v)		((v)->vx_wmask)

#define vx_current_wmask()	__vx_wmask(current_vx_info())


#define __vx_state(v)	((v) ? ((v)->vx_state) : 0)

#define vx_info_state(v, m)	(__vx_state(v) & (m))


#define __nx_state(n)	((n) ? ((n)->nx_state) : 0)

#define nx_info_state(n, m)	(__nx_state(n) & (m))

#endif
