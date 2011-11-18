#ifndef _VX_CONTEXT_H
#define _VX_CONTEXT_H

#include <linux/types.h>
#include <linux/capability.h>


/* context flags */

#define VXF_INFO_SCHED		0x00000002
#define VXF_INFO_NPROC		0x00000004
#define VXF_INFO_PRIVATE	0x00000008

#define VXF_INFO_INIT		0x00000010
#define VXF_INFO_HIDE		0x00000020
#define VXF_INFO_ULIMIT		0x00000040
#define VXF_INFO_NSPACE		0x00000080

#define VXF_SCHED_HARD		0x00000100
#define VXF_SCHED_PRIO		0x00000200
#define VXF_SCHED_PAUSE		0x00000400

#define VXF_VIRT_MEM		0x00010000
#define VXF_VIRT_UPTIME		0x00020000
#define VXF_VIRT_CPU		0x00040000
#define VXF_VIRT_LOAD		0x00080000
#define VXF_VIRT_TIME		0x00100000

#define VXF_HIDE_MOUNT		0x01000000
/* was	VXF_HIDE_NETIF		0x02000000 */
#define VXF_HIDE_VINFO		0x04000000

#define VXF_STATE_SETUP		(1ULL << 32)
#define VXF_STATE_INIT		(1ULL << 33)
#define VXF_STATE_ADMIN		(1ULL << 34)

#define VXF_SC_HELPER		(1ULL << 36)
#define VXF_REBOOT_KILL		(1ULL << 37)
#define VXF_PERSISTENT		(1ULL << 38)

#define VXF_FORK_RSS		(1ULL << 48)
#define VXF_PROLIFIC		(1ULL << 49)

#define VXF_IGNEG_NICE		(1ULL << 52)

#define VXF_ONE_TIME		(0x0007ULL << 32)

#define VXF_INIT_SET		(VXF_STATE_SETUP | VXF_STATE_INIT | VXF_STATE_ADMIN)


/* context migration */

#define VXM_SET_INIT		0x00000001
#define VXM_SET_REAPER		0x00000002

/* context caps */

#define VXC_SET_UTSNAME		0x00000001
#define VXC_SET_RLIMIT		0x00000002
#define VXC_FS_SECURITY		0x00000004
#define VXC_FS_TRUSTED		0x00000008
#define VXC_TIOCSTI		0x00000010

/* was	VXC_RAW_ICMP		0x00000100 */
#define VXC_SYSLOG		0x00001000
#define VXC_OOM_ADJUST		0x00002000
#define VXC_AUDIT_CONTROL	0x00004000

#define VXC_SECURE_MOUNT	0x00010000
#define VXC_SECURE_REMOUNT	0x00020000
#define VXC_BINARY_MOUNT	0x00040000

#define VXC_QUOTA_CTL		0x00100000
#define VXC_ADMIN_MAPPER	0x00200000
#define VXC_ADMIN_CLOOP		0x00400000

#define VXC_KTHREAD		0x01000000
#define VXC_NAMESPACE		0x02000000


#ifdef	__KERNEL__

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>

#include "limit_def.h"
#include "sched_def.h"
#include "cvirt_def.h"
#include "cacct_def.h"
#include "device_def.h"

#define VX_SPACES	2

struct _vx_info_pc {
	struct _vx_sched_pc sched_pc;
	struct _vx_cvirt_pc cvirt_pc;
};

struct _vx_space {
	unsigned long vx_nsmask;		/* assignment mask */
	struct nsproxy *vx_nsproxy;             /* private namespaces */
	struct fs_struct *vx_fs;                /* private namespace fs */
	const struct cred *vx_cred;             /* task credentials */
};

struct vx_info {
	struct hlist_node vx_hlist;		/* linked list of contexts */
	xid_t vx_id;				/* context id */
	atomic_t vx_usecnt;			/* usage count */
	atomic_t vx_tasks;			/* tasks count */
	struct vx_info *vx_parent;		/* parent context */
	int vx_state;				/* context state */

	struct _vx_space space[VX_SPACES];	/* namespace store */

	uint64_t vx_flags;			/* context flags */
	uint64_t vx_ccaps;			/* context caps (vserver) */
	uint64_t vx_umask;			/* unshare mask (guest) */
	uint64_t vx_wmask;			/* warn mask (guest) */
	kernel_cap_t vx_bcaps;			/* bounding caps (system) */

	struct task_struct *vx_reaper;		/* guest reaper process */
	pid_t vx_initpid;			/* PID of guest init */
	int64_t vx_badness_bias;		/* OOM points bias */

	struct _vx_limit limit;			/* vserver limits */
	struct _vx_sched sched;			/* vserver scheduler */
	struct _vx_cvirt cvirt;			/* virtual/bias stuff */
	struct _vx_cacct cacct;			/* context accounting */

	struct _vx_device dmap;			/* default device map targets */

#ifndef CONFIG_SMP
	struct _vx_info_pc info_pc;		/* per cpu data */
#else
	struct _vx_info_pc *ptr_pc;		/* per cpu array */
#endif

	wait_queue_head_t vx_wait;		/* context exit waitqueue */
	int reboot_cmd;				/* last sys_reboot() cmd */
	int exit_code;				/* last process exit code */

	char vx_name[65];			/* vserver name */
};

#ifndef CONFIG_SMP
#define	vx_ptr_pc(vxi)		(&(vxi)->info_pc)
#define	vx_per_cpu(vxi, v, id)	vx_ptr_pc(vxi)->v
#else
#define	vx_ptr_pc(vxi)		((vxi)->ptr_pc)
#define	vx_per_cpu(vxi, v, id)	per_cpu_ptr(vx_ptr_pc(vxi), id)->v
#endif

#define	vx_cpu(vxi, v)		vx_per_cpu(vxi, v, smp_processor_id())


struct vx_info_save {
	struct vx_info *vxi;
	xid_t xid;
};


/* status flags */

#define VXS_HASHED	0x0001
#define VXS_PAUSED	0x0010
#define VXS_SHUTDOWN	0x0100
#define VXS_HELPER	0x1000
#define VXS_RELEASED	0x8000


extern void claim_vx_info(struct vx_info *, struct task_struct *);
extern void release_vx_info(struct vx_info *, struct task_struct *);

extern struct vx_info *lookup_vx_info(int);
extern struct vx_info *lookup_or_create_vx_info(int);

extern int get_xid_list(int, unsigned int *, int);
extern int xid_is_hashed(xid_t);

extern int vx_migrate_task(struct task_struct *, struct vx_info *, int);

extern long vs_state_change(struct vx_info *, unsigned int);


#endif	/* __KERNEL__ */
#endif	/* _VX_CONTEXT_H */
