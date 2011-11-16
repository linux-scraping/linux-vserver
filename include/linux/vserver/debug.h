#ifndef _VX_DEBUG_H
#define _VX_DEBUG_H


#define VXD_CBIT(n, m)	(vs_debug_ ## n & (1 << (m)))
#define VXD_CMIN(n, m)	(vs_debug_ ## n > (m))
#define VXD_MASK(n, m)	(vs_debug_ ## n & (m))

#define VXD_DEV(d)	(d), (d)->bd_inode->i_ino,		\
			imajor((d)->bd_inode), iminor((d)->bd_inode)
#define VXF_DEV		"%p[%lu,%d:%d]"

#if	defined(CONFIG_QUOTES_UTF8)
#define	VS_Q_LQM	"\xc2\xbb"
#define	VS_Q_RQM	"\xc2\xab"
#elif	defined(CONFIG_QUOTES_ASCII)
#define	VS_Q_LQM	"\x27"
#define	VS_Q_RQM	"\x27"
#else
#define	VS_Q_LQM	"\xbb"
#define	VS_Q_RQM	"\xab"
#endif

#define	VS_Q(f)		VS_Q_LQM f VS_Q_RQM


#define vxd_path(p)						\
	({ static char _buffer[PATH_MAX];			\
	   d_path(p, _buffer, sizeof(_buffer)); })

#define vxd_cond_path(n)					\
	((n) ? vxd_path(&(n)->path) : "<null>" )


#ifdef	CONFIG_VSERVER_DEBUG

extern unsigned int vs_debug_switch;
extern unsigned int vs_debug_xid;
extern unsigned int vs_debug_nid;
extern unsigned int vs_debug_tag;
extern unsigned int vs_debug_net;
extern unsigned int vs_debug_limit;
extern unsigned int vs_debug_cres;
extern unsigned int vs_debug_dlim;
extern unsigned int vs_debug_quota;
extern unsigned int vs_debug_cvirt;
extern unsigned int vs_debug_space;
extern unsigned int vs_debug_perm;
extern unsigned int vs_debug_misc;


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

/*
#define vs_debug_switch 0
#define vs_debug_xid	0
#define vs_debug_nid	0
#define vs_debug_tag	0
#define vs_debug_net	0
#define vs_debug_limit	0
#define vs_debug_cres	0
#define vs_debug_dlim	0
#define vs_debug_cvirt	0
*/

#define vxdprintk(x...) do { } while (0)
#define vxlprintk(x...) do { } while (0)
#define vxfprintk(x...) do { } while (0)

#endif	/* CONFIG_VSERVER_DEBUG */


#ifdef	CONFIG_VSERVER_WARN

#define VX_WARNLEVEL	KERN_WARNING "vxW: "
#define VX_WARN_TASK	"[" VS_Q("%s") ",%u:#%u|%u|%u] "
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
