#ifndef _VSERVER_CHECK_H
#define _VSERVER_CHECK_H


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
		if ((mode & VS_IDENT) && (id == cid))
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

#define vx_check(c, m)	__vs_check(vx_current_xid(), c, (m) | VS_IRQ)

#define vx_weak_check(c, m)	((m) ? vx_check(c, m) : 1)


#define nx_check(c, m)	__vs_check(nx_current_nid(), c, m)

#define nx_weak_check(c, m)	((m) ? nx_check(c, m) : 1)

#endif
