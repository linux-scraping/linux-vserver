#ifndef _VSERVER_CONTEXT_H
#define _VSERVER_CONTEXT_H


#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <uapi/vserver/context.h>

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
	vxid_t vx_id;				/* context id */
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
	vxid_t xid;
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
extern int xid_is_hashed(vxid_t);

extern int vx_migrate_task(struct task_struct *, struct vx_info *, int);

extern long vs_state_change(struct vx_info *, unsigned int);


#endif	/* _VSERVER_CONTEXT_H */
