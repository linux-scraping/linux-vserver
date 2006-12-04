#ifndef _VX_BASE_H
#define _VX_BASE_H


/* context state changes */

enum {
	VSC_STARTUP = 1,
	VSC_SHUTDOWN,

	VSC_NETUP,
	VSC_NETDOWN,
};


#define MAX_S_CONTEXT	65535	/* Arbitrary limit */

#ifdef	CONFIG_VSERVER_DYNAMIC_IDS
#define MIN_D_CONTEXT	49152	/* dynamic contexts start here */
#else
#define MIN_D_CONTEXT	65536
#endif

/* check conditions */

#define VS_ADMIN	0x0001
#define VS_WATCH	0x0002
#define VS_HIDE		0x0004
#define VS_HOSTID	0x0008

#define VS_IDENT	0x0010
#define VS_EQUIV	0x0020
#define VS_PARENT	0x0040
#define VS_CHILD	0x0080

#define VS_ARG_MASK	0x00F0

#define VS_DYNAMIC	0x0100
#define VS_STATIC	0x0200

#define VS_ATR_MASK	0x0F00

#ifdef	CONFIG_VSERVER_PRIVACY
#define VS_ADMIN_P	(0)
#define VS_WATCH_P	(0)
#else
#define VS_ADMIN_P	VS_ADMIN
#define VS_WATCH_P	VS_WATCH
#endif

#define VS_HARDIRQ	0x1000
#define VS_SOFTIRQ	0x2000
#define VS_IRQ		0x4000

#define VS_IRQ_MASK	0xF000

#include <linux/hardirq.h>

/*
 * check current context for ADMIN/WATCH and
 * optionally against supplied argument
 */
static inline int __vs_check(int cid, int id, unsigned int mode)
{
	if (mode & VS_ARG_MASK) {
		if ((mode & VS_IDENT) &&
			(id == cid))
			return 1;
	}
	if (mode & VS_ATR_MASK) {
		if ((mode & VS_DYNAMIC) &&
			(id >= MIN_D_CONTEXT) &&
			(id <= MAX_S_CONTEXT))
			return 1;
		if ((mode & VS_STATIC) &&
			(id > 1) && (id < MIN_D_CONTEXT))
			return 1;
	}
	if (mode & VS_IRQ_MASK) {
		if ((mode & VS_IRQ) && unlikely(in_interrupt()))
			return 1;
		if ((mode & VS_HARDIRQ) && unlikely(in_irq()))
			return 1;
		if ((mode & VS_SOFTIRQ) && unlikely(in_softirq()))
			return 1;
	}
	return (((mode & VS_ADMIN) && (cid == 0)) ||
		((mode & VS_WATCH) && (cid == 1)) ||
		((mode & VS_HOSTID) && (id == 0)));
}

#define vx_task_xid(t)	((t)->xid)

#define vx_current_xid() vx_task_xid(current)

#define current_vx_info() (current->vx_info)


#define vx_check(c,m)	__vs_check(vx_current_xid(),c,(m)|VS_IRQ)

#define vx_weak_check(c,m)	((m) ? vx_check(c,m) : 1)


#define nx_task_nid(t)	((t)->nid)

#define nx_current_nid() nx_task_nid(current)

#define current_nx_info() (current->nx_info)


#define nx_check(c,m)	__vs_check(nx_current_nid(),c,m)

#define nx_weak_check(c,m)	((m) ? nx_check(c,m) : 1)



/* generic flag merging */

#define vs_check_flags(v,m,f)	(((v) & (m)) ^ (f))

#define vs_mask_flags(v,f,m)	(((v) & ~(m)) | ((f) & (m)))

#define vs_mask_mask(v,f,m)	(((v) & ~(m)) | ((v) & (f) & (m)))

#define vs_check_bit(v,n)	((v) & (1LL << (n)))


/* context flags */

#define __vx_flags(v)	((v) ? (v)->vx_flags : 0)

#define vx_current_flags()	__vx_flags(current->vx_info)

#define vx_info_flags(v,m,f) \
	vs_check_flags(__vx_flags(v),(m),(f))

#define task_vx_flags(t,m,f) \
	((t) && vx_info_flags((t)->vx_info, (m), (f)))

#define vx_flags(m,f)	vx_info_flags(current->vx_info,(m),(f))


/* context caps */

#define __vx_ccaps(v)	((v) ? (v)->vx_ccaps : 0)

#define vx_current_ccaps()	__vx_ccaps(current->vx_info)

#define vx_info_ccaps(v,c)	(__vx_ccaps(v) & (c))

#define vx_ccaps(c)	vx_info_ccaps(current->vx_info,(c))



/* network flags */

#define __nx_flags(v)	((v) ? (v)->nx_flags : 0)

#define nx_current_flags()	__nx_flags(current->nx_info)

#define nx_info_flags(v,m,f) \
	vs_check_flags(__nx_flags(v),(m),(f))

#define task_nx_flags(t,m,f) \
	((t) && nx_info_flags((t)->nx_info, (m), (f)))

#define nx_flags(m,f)	nx_info_flags(current->nx_info,(m),(f))


/* network caps */

#define __nx_ncaps(v)	((v) ? (v)->nx_ncaps : 0)

#define nx_current_ncaps()	__nx_ncaps(current->nx_info)

#define nx_info_ncaps(v,c)	(__nx_ncaps(v) & (c))

#define nx_ncaps(c)	nx_info_ncaps(current->nx_info,(c))


/* context mask capabilities */

#define __vx_mcaps(v)	((v) ? (v)->vx_ccaps >> 32UL : ~0 )

#define vx_info_mcaps(v,c)	(__vx_mcaps(v) & (c))

#define vx_mcaps(c)	vx_info_mcaps(current->vx_info,(c))


/* context bcap mask */

#define __vx_bcaps(v)	((v) ? (v)->vx_bcaps : ~0 )

#define vx_current_bcaps()	__vx_bcaps(current->vx_info)

#define vx_info_bcaps(v,c)	(__vx_bcaps(v) & (c))

#define vx_bcaps(c)	vx_info_bcaps(current->vx_info,(c))


#define vx_info_cap_bset(v)	((v) ? (v)->vx_cap_bset : cap_bset)

#define vx_current_cap_bset()	vx_info_cap_bset(current->vx_info)


#define __vx_info_mbcap(v,b) \
	(!vx_info_flags(v, VXF_STATE_SETUP, 0) ? \
	vx_info_bcaps(v, b) : (b))

#define vx_info_mbcap(v,b)	__vx_info_mbcap(v,cap_t(b))

#define task_vx_mbcap(t,b) \
	vx_info_mbcap((t)->vx_info, (t)->b)

#define vx_mbcap(b)	task_vx_mbcap(current,b)

#define vx_cap_raised(v,c,f)	(vx_info_mbcap(v,c) & CAP_TO_MASK(f))

#define vx_capable(b,c) (capable(b) || \
	(cap_raised(current->cap_effective,b) && vx_ccaps(c)))


#define vx_current_initpid(n) \
	(current->vx_info && \
	(current->vx_info->vx_initpid == (n)))


#define __vx_state(v)	((v) ? ((v)->vx_state) : 0)

#define vx_info_state(v,m)	(__vx_state(v) & (m))


#define __nx_state(v)	((v) ? ((v)->nx_state) : 0)

#define nx_info_state(v,m)	(__nx_state(v) & (m))

#endif
