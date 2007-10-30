#ifndef _VX_DEBUG_H
#define _VX_DEBUG_H


#define VXD_CBIT(n, m)	(vx_debug_ ## n & (1 << (m)))
#define VXD_CMIN(n, m)	(vx_debug_ ## n > (m))
#define VXD_MASK(n, m)	(vx_debug_ ## n & (m))

#define VXD_DEV(d)	(d), (d)->bd_inode->i_ino,		\
			imajor((d)->bd_inode), iminor((d)->bd_inode)
#define VXF_DEV		"%p[%lu,%d:%d]"


#define __FUNC__	__func__


#define vxd_path(d, m)						\
	({ static char _buffer[PATH_MAX];			\
	   d_path(d, m, _buffer, sizeof(_buffer)); })

#define vxd_cond_path(n)					\
	((n) ? vxd_path((n)->dentry, (n)->mnt) : "<null>" )


#ifdef	CONFIG_VSERVER_DEBUG

extern unsigned int vx_debug_switch;
extern unsigned int vx_debug_xid;
extern unsigned int vx_debug_nid;
extern unsigned int vx_debug_tag;
extern unsigned int vx_debug_net;
extern unsigned int vx_debug_limit;
extern unsigned int vx_debug_cres;
extern unsigned int vx_debug_dlim;
extern unsigned int vx_debug_quota;
extern unsigned int vx_debug_cvirt;
extern unsigned int vx_debug_space;
extern unsigned int vx_debug_misc;


#define VX_LOGLEVEL	"vxD: "
#define VX_PROC_FMT	"%p: "
#define VX_PROCESS	current

#define vxdprintk(c, f, x...)					\
	do {							\
		if (c)						\
			printk(VX_LOGLEVEL VX_PROC_FMT f "\n",	\
				VX_PROCESS , ##x);		\
	} while (0)

#define vxlprintk(c, f, x...)					\
	do {							\
		if (c)						\
			printk(VX_LOGLEVEL f " @%s:%d\n", x);	\
	} while (0)

#define vxfprintk(c, f, x...)					\
	do {							\
		if (c)						\
			printk(VX_LOGLEVEL f " %s@%s:%d\n", x); \
	} while (0)


struct vx_info;

void dump_vx_info(struct vx_info *, int);
void dump_vx_info_inactive(int);

#else	/* CONFIG_VSERVER_DEBUG */

#define vx_debug_switch 0
#define vx_debug_xid	0
#define vx_debug_nid	0
#define vx_debug_tag	0
#define vx_debug_net	0
#define vx_debug_limit	0
#define vx_debug_cres	0
#define vx_debug_dlim	0
#define vx_debug_cvirt	0

#define vxdprintk(x...) do { } while (0)
#define vxlprintk(x...) do { } while (0)
#define vxfprintk(x...) do { } while (0)

#endif	/* CONFIG_VSERVER_DEBUG */


#ifdef	CONFIG_VSERVER_WARN

#define VX_WARNLEVEL	KERN_WARNING "vxW: "
#define VX_WARN_TASK	"[»%s«,%u:#%u|%u|%u] "
#define VX_WARN_XID	"[xid #%u] "
#define VX_WARN_NID	"[nid #%u] "
#define VX_WARN_TAG	"[tag #%u] "

#define vxwprintk(c, f, x...)					\
	do {							\
		if (c)						\
			printk(VX_WARNLEVEL f "\n", ##x);	\
	} while (0)

#else	/* CONFIG_VSERVER_WARN */

#define vxwprintk(x...) do { } while (0)

#endif	/* CONFIG_VSERVER_WARN */

#define vxwprintk_task(c, f, x...)				\
	vxwprintk(c, VX_WARN_TASK f,				\
		current->comm, current->pid,			\
		current->xid, current->nid, current->tag, ##x)
#define vxwprintk_xid(c, f, x...)				\
	vxwprintk(c, VX_WARN_XID f, current->xid, x)
#define vxwprintk_nid(c, f, x...)				\
	vxwprintk(c, VX_WARN_NID f, current->nid, x)
#define vxwprintk_tag(c, f, x...)				\
	vxwprintk(c, VX_WARN_TAG f, current->tag, x)

#ifdef	CONFIG_VSERVER_DEBUG
#define vxd_assert_lock(l)	assert_spin_locked(l)
#define vxd_assert(c, f, x...)	vxlprintk(!(c), \
	"assertion [" f "] failed.", ##x, __FILE__, __LINE__)
#else
#define vxd_assert_lock(l)	do { } while (0)
#define vxd_assert(c, f, x...)	do { } while (0)
#endif


#endif /* _VX_DEBUG_H */
