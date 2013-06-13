/*
 *  linux/kernel/vserver/context.c
 *
 *  Virtual Server: Context Support
 *
 *  Copyright (C) 2003-2011  Herbert Pötzl
 *
 *  V0.01  context helper
 *  V0.02  vx_ctx_kill syscall command
 *  V0.03  replaced context_info calls
 *  V0.04  redesign of struct (de)alloc
 *  V0.05  rlimit basic implementation
 *  V0.06  task_xid and info commands
 *  V0.07  context flags and caps
 *  V0.08  switch to RCU based hash
 *  V0.09  revert to non RCU for now
 *  V0.10  and back to working RCU hash
 *  V0.11  and back to locking again
 *  V0.12  referenced context store
 *  V0.13  separate per cpu data
 *  V0.14  changed vcmds to vxi arg
 *  V0.15  added context stat
 *  V0.16  have __create claim() the vxi
 *  V0.17  removed older and legacy stuff
 *  V0.18  added user credentials
 *  V0.19  added warn mask
 *
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/security.h>
#include <linux/pid_namespace.h>
#include <linux/capability.h>

#include <linux/vserver/context.h>
#include <linux/vserver/network.h>
#include <linux/vserver/debug.h>
#include <linux/vserver/limit.h>
#include <linux/vserver/limit_int.h>
#include <linux/vserver/space.h>
#include <linux/init_task.h>
#include <linux/fs_struct.h>
#include <linux/cred.h>

#include <linux/vs_context.h>
#include <linux/vs_limit.h>
#include <linux/vs_pid.h>
#include <linux/vserver/context_cmd.h>

#include "cvirt_init.h"
#include "cacct_init.h"
#include "limit_init.h"
#include "sched_init.h"


atomic_t vx_global_ctotal	= ATOMIC_INIT(0);
atomic_t vx_global_cactive	= ATOMIC_INIT(0);


/*	now inactive context structures */

static struct hlist_head vx_info_inactive = HLIST_HEAD_INIT;

static DEFINE_SPINLOCK(vx_info_inactive_lock);


/*	__alloc_vx_info()

	* allocate an initialized vx_info struct
	* doesn't make it visible (hash)			*/

static struct vx_info *__alloc_vx_info(vxid_t xid)
{
	struct vx_info *new = NULL;
	int cpu, index;

	vxdprintk(VXD_CBIT(xid, 0), "alloc_vx_info(%d)*", xid);

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct vx_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset(new, 0, sizeof(struct vx_info));
#ifdef CONFIG_SMP
	new->ptr_pc = alloc_percpu(struct _vx_info_pc);
	if (!new->ptr_pc)
		goto error;
#endif
	new->vx_id = xid;
	INIT_HLIST_NODE(&new->vx_hlist);
	atomic_set(&new->vx_usecnt, 0);
	atomic_set(&new->vx_tasks, 0);
	new->vx_parent = NULL;
	new->vx_state = 0;
	init_waitqueue_head(&new->vx_wait);

	/* prepare reaper */
	get_task_struct(init_pid_ns.child_reaper);
	new->vx_reaper = init_pid_ns.child_reaper;
	new->vx_badness_bias = 0;

	/* rest of init goes here */
	vx_info_init_limit(&new->limit);
	vx_info_init_sched(&new->sched);
	vx_info_init_cvirt(&new->cvirt);
	vx_info_init_cacct(&new->cacct);

	/* per cpu data structures */
	for_each_possible_cpu(cpu) {
		vx_info_init_sched_pc(
			&vx_per_cpu(new, sched_pc, cpu), cpu);
		vx_info_init_cvirt_pc(
			&vx_per_cpu(new, cvirt_pc, cpu), cpu);
	}

	new->vx_flags = VXF_INIT_SET;
	new->vx_bcaps = CAP_FULL_SET;	// maybe ~CAP_SETPCAP
	new->vx_ccaps = 0;
	new->vx_umask = 0;
	new->vx_wmask = 0;

	new->reboot_cmd = 0;
	new->exit_code = 0;

	// preconfig spaces
	for (index = 0; index < VX_SPACES; index++) {
		struct _vx_space *space = &new->space[index];

		// filesystem
		spin_lock(&init_fs.lock);
		init_fs.users++;
		spin_unlock(&init_fs.lock);
		space->vx_fs = &init_fs;

		/* FIXME: do we want defaults? */
		// space->vx_real_cred = 0;
		// space->vx_cred = 0;
	}


	vxdprintk(VXD_CBIT(xid, 0),
		"alloc_vx_info(%d) = %p", xid, new);
	vxh_alloc_vx_info(new);
	atomic_inc(&vx_global_ctotal);
	return new;
#ifdef CONFIG_SMP
error:
	kfree(new);
	return 0;
#endif
}

/*	__dealloc_vx_info()

	* final disposal of vx_info				*/

static void __dealloc_vx_info(struct vx_info *vxi)
{
#ifdef	CONFIG_VSERVER_WARN
	struct vx_info_save vxis;
	int cpu;
#endif
	vxdprintk(VXD_CBIT(xid, 0),
		"dealloc_vx_info(%p)", vxi);
	vxh_dealloc_vx_info(vxi);

#ifdef	CONFIG_VSERVER_WARN
	enter_vx_info(vxi, &vxis);
	vx_info_exit_limit(&vxi->limit);
	vx_info_exit_sched(&vxi->sched);
	vx_info_exit_cvirt(&vxi->cvirt);
	vx_info_exit_cacct(&vxi->cacct);

	for_each_possible_cpu(cpu) {
		vx_info_exit_sched_pc(
			&vx_per_cpu(vxi, sched_pc, cpu), cpu);
		vx_info_exit_cvirt_pc(
			&vx_per_cpu(vxi, cvirt_pc, cpu), cpu);
	}
	leave_vx_info(&vxis);
#endif

	vxi->vx_id = -1;
	vxi->vx_state |= VXS_RELEASED;

#ifdef CONFIG_SMP
	free_percpu(vxi->ptr_pc);
#endif
	kfree(vxi);
	atomic_dec(&vx_global_ctotal);
}

static void __shutdown_vx_info(struct vx_info *vxi)
{
	struct nsproxy *nsproxy;
	struct fs_struct *fs;
	struct cred *cred;
	int index, kill;

	might_sleep();

	vxi->vx_state |= VXS_SHUTDOWN;
	vs_state_change(vxi, VSC_SHUTDOWN);

	for (index = 0; index < VX_SPACES; index++) {
		struct _vx_space *space = &vxi->space[index];

		nsproxy = xchg(&space->vx_nsproxy, NULL);
		if (nsproxy)
			put_nsproxy(nsproxy);

		fs = xchg(&space->vx_fs, NULL);
		spin_lock(&fs->lock);
		kill = !--fs->users;
		spin_unlock(&fs->lock);
		if (kill)
			free_fs_struct(fs);

		cred = (struct cred *)xchg(&space->vx_cred, NULL);
		if (cred)
			abort_creds(cred);
	}
}

/* exported stuff */

void free_vx_info(struct vx_info *vxi)
{
	unsigned long flags;
	unsigned index;

	/* check for reference counts first */
	BUG_ON(atomic_read(&vxi->vx_usecnt));
	BUG_ON(atomic_read(&vxi->vx_tasks));

	/* context must not be hashed */
	BUG_ON(vx_info_state(vxi, VXS_HASHED));

	/* context shutdown is mandatory */
	BUG_ON(!vx_info_state(vxi, VXS_SHUTDOWN));

	/* spaces check */
	for (index = 0; index < VX_SPACES; index++) {
		struct _vx_space *space = &vxi->space[index];

		BUG_ON(space->vx_nsproxy);
		BUG_ON(space->vx_fs);
		// BUG_ON(space->vx_real_cred);
		// BUG_ON(space->vx_cred);
	}

	spin_lock_irqsave(&vx_info_inactive_lock, flags);
	hlist_del(&vxi->vx_hlist);
	spin_unlock_irqrestore(&vx_info_inactive_lock, flags);

	__dealloc_vx_info(vxi);
}


/*	hash table for vx_info hash */

#define VX_HASH_SIZE	13

static struct hlist_head vx_info_hash[VX_HASH_SIZE] =
	{ [0 ... VX_HASH_SIZE-1] = HLIST_HEAD_INIT };

static DEFINE_SPINLOCK(vx_info_hash_lock);


static inline unsigned int __hashval(vxid_t xid)
{
	return (xid % VX_HASH_SIZE);
}



/*	__hash_vx_info()

	* add the vxi to the global hash table
	* requires the hash_lock to be held			*/

static inline void __hash_vx_info(struct vx_info *vxi)
{
	struct hlist_head *head;

	vxd_assert_lock(&vx_info_hash_lock);
	vxdprintk(VXD_CBIT(xid, 4),
		"__hash_vx_info: %p[#%d]", vxi, vxi->vx_id);
	vxh_hash_vx_info(vxi);

	/* context must not be hashed */
	BUG_ON(vx_info_state(vxi, VXS_HASHED));

	vxi->vx_state |= VXS_HASHED;
	head = &vx_info_hash[__hashval(vxi->vx_id)];
	hlist_add_head(&vxi->vx_hlist, head);
	atomic_inc(&vx_global_cactive);
}

/*	__unhash_vx_info()

	* remove the vxi from the global hash table
	* requires the hash_lock to be held			*/

static inline void __unhash_vx_info(struct vx_info *vxi)
{
	unsigned long flags;

	vxd_assert_lock(&vx_info_hash_lock);
	vxdprintk(VXD_CBIT(xid, 4),
		"__unhash_vx_info: %p[#%d.%d.%d]", vxi, vxi->vx_id,
		atomic_read(&vxi->vx_usecnt), atomic_read(&vxi->vx_tasks));
	vxh_unhash_vx_info(vxi);

	/* context must be hashed */
	BUG_ON(!vx_info_state(vxi, VXS_HASHED));
	/* but without tasks */
	BUG_ON(atomic_read(&vxi->vx_tasks));

	vxi->vx_state &= ~VXS_HASHED;
	hlist_del_init(&vxi->vx_hlist);
	spin_lock_irqsave(&vx_info_inactive_lock, flags);
	hlist_add_head(&vxi->vx_hlist, &vx_info_inactive);
	spin_unlock_irqrestore(&vx_info_inactive_lock, flags);
	atomic_dec(&vx_global_cactive);
}


/*	__lookup_vx_info()

	* requires the hash_lock to be held
	* doesn't increment the vx_refcnt			*/

static inline struct vx_info *__lookup_vx_info(vxid_t xid)
{
	struct hlist_head *head = &vx_info_hash[__hashval(xid)];
	struct hlist_node *pos;
	struct vx_info *vxi;

	vxd_assert_lock(&vx_info_hash_lock);
	hlist_for_each(pos, head) {
		vxi = hlist_entry(pos, struct vx_info, vx_hlist);

		if (vxi->vx_id == xid)
			goto found;
	}
	vxi = NULL;
found:
	vxdprintk(VXD_CBIT(xid, 0),
		"__lookup_vx_info(#%u): %p[#%u]",
		xid, vxi, vxi ? vxi->vx_id : 0);
	vxh_lookup_vx_info(vxi, xid);
	return vxi;
}


/*	__create_vx_info()

	* create the requested context
	* get(), claim() and hash it				*/

static struct vx_info *__create_vx_info(int id)
{
	struct vx_info *new, *vxi = NULL;

	vxdprintk(VXD_CBIT(xid, 1), "create_vx_info(%d)*", id);

	if (!(new = __alloc_vx_info(id)))
		return ERR_PTR(-ENOMEM);

	/* required to make dynamic xids unique */
	spin_lock(&vx_info_hash_lock);

	/* static context requested */
	if ((vxi = __lookup_vx_info(id))) {
		vxdprintk(VXD_CBIT(xid, 0),
			"create_vx_info(%d) = %p (already there)", id, vxi);
		if (vx_info_flags(vxi, VXF_STATE_SETUP, 0))
			vxi = ERR_PTR(-EBUSY);
		else
			vxi = ERR_PTR(-EEXIST);
		goto out_unlock;
	}
	/* new context */
	vxdprintk(VXD_CBIT(xid, 0),
		"create_vx_info(%d) = %p (new)", id, new);
	claim_vx_info(new, NULL);
	__hash_vx_info(get_vx_info(new));
	vxi = new, new = NULL;

out_unlock:
	spin_unlock(&vx_info_hash_lock);
	vxh_create_vx_info(IS_ERR(vxi) ? NULL : vxi, id);
	if (new)
		__dealloc_vx_info(new);
	return vxi;
}


/*	exported stuff						*/


void unhash_vx_info(struct vx_info *vxi)
{
	spin_lock(&vx_info_hash_lock);
	__unhash_vx_info(vxi);
	spin_unlock(&vx_info_hash_lock);
	__shutdown_vx_info(vxi);
	__wakeup_vx_info(vxi);
}


/*	lookup_vx_info()

	* search for a vx_info and get() it
	* negative id means current				*/

struct vx_info *lookup_vx_info(int id)
{
	struct vx_info *vxi = NULL;

	if (id < 0) {
		vxi = get_vx_info(current_vx_info());
	} else if (id > 1) {
		spin_lock(&vx_info_hash_lock);
		vxi = get_vx_info(__lookup_vx_info(id));
		spin_unlock(&vx_info_hash_lock);
	}
	return vxi;
}

/*	xid_is_hashed()

	* verify that xid is still hashed			*/

int xid_is_hashed(vxid_t xid)
{
	int hashed;

	spin_lock(&vx_info_hash_lock);
	hashed = (__lookup_vx_info(xid) != NULL);
	spin_unlock(&vx_info_hash_lock);
	return hashed;
}

#ifdef	CONFIG_PROC_FS

/*	get_xid_list()

	* get a subset of hashed xids for proc
	* assumes size is at least one				*/

int get_xid_list(int index, unsigned int *xids, int size)
{
	int hindex, nr_xids = 0;

	/* only show current and children */
	if (!vx_check(0, VS_ADMIN | VS_WATCH)) {
		if (index > 0)
			return 0;
		xids[nr_xids] = vx_current_xid();
		return 1;
	}

	for (hindex = 0; hindex < VX_HASH_SIZE; hindex++) {
		struct hlist_head *head = &vx_info_hash[hindex];
		struct hlist_node *pos;

		spin_lock(&vx_info_hash_lock);
		hlist_for_each(pos, head) {
			struct vx_info *vxi;

			if (--index > 0)
				continue;

			vxi = hlist_entry(pos, struct vx_info, vx_hlist);
			xids[nr_xids] = vxi->vx_id;
			if (++nr_xids >= size) {
				spin_unlock(&vx_info_hash_lock);
				goto out;
			}
		}
		/* keep the lock time short */
		spin_unlock(&vx_info_hash_lock);
	}
out:
	return nr_xids;
}
#endif

#ifdef	CONFIG_VSERVER_DEBUG

void	dump_vx_info_inactive(int level)
{
	struct hlist_node *entry, *next;

	hlist_for_each_safe(entry, next, &vx_info_inactive) {
		struct vx_info *vxi =
			list_entry(entry, struct vx_info, vx_hlist);

		dump_vx_info(vxi, level);
	}
}

#endif

#if 0
int vx_migrate_user(struct task_struct *p, struct vx_info *vxi)
{
	struct user_struct *new_user, *old_user;

	if (!p || !vxi)
		BUG();

	if (vx_info_flags(vxi, VXF_INFO_PRIVATE, 0))
		return -EACCES;

	new_user = alloc_uid(vxi->vx_id, p->uid);
	if (!new_user)
		return -ENOMEM;

	old_user = p->user;
	if (new_user != old_user) {
		atomic_inc(&new_user->processes);
		atomic_dec(&old_user->processes);
		p->user = new_user;
	}
	free_uid(old_user);
	return 0;
}
#endif

#if 0
void vx_mask_cap_bset(struct vx_info *vxi, struct task_struct *p)
{
	// p->cap_effective &= vxi->vx_cap_bset;
	p->cap_effective =
		cap_intersect(p->cap_effective, vxi->cap_bset);
	// p->cap_inheritable &= vxi->vx_cap_bset;
	p->cap_inheritable =
		cap_intersect(p->cap_inheritable, vxi->cap_bset);
	// p->cap_permitted &= vxi->vx_cap_bset;
	p->cap_permitted =
		cap_intersect(p->cap_permitted, vxi->cap_bset);
}
#endif


#include <linux/file.h>
#include <linux/fdtable.h>

static int vx_openfd_task(struct task_struct *tsk)
{
	struct files_struct *files = tsk->files;
	struct fdtable *fdt;
	const unsigned long *bptr;
	int count, total;

	/* no rcu_read_lock() because of spin_lock() */
	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	bptr = fdt->open_fds;
	count = fdt->max_fds / (sizeof(unsigned long) * 8);
	for (total = 0; count > 0; count--) {
		if (*bptr)
			total += hweight_long(*bptr);
		bptr++;
	}
	spin_unlock(&files->file_lock);
	return total;
}


/*	for *space compatibility */

asmlinkage long sys_unshare(unsigned long);

/*
 *	migrate task to new context
 *	gets vxi, puts old_vxi on change
 *	optionally unshares namespaces (hack)
 */

int vx_migrate_task(struct task_struct *p, struct vx_info *vxi, int unshare)
{
	struct vx_info *old_vxi;
	int ret = 0;

	if (!p || !vxi)
		BUG();

	vxdprintk(VXD_CBIT(xid, 5),
		"vx_migrate_task(%p,%p[#%d.%d])", p, vxi,
		vxi->vx_id, atomic_read(&vxi->vx_usecnt));

	if (vx_info_flags(vxi, VXF_INFO_PRIVATE, 0) &&
		!vx_info_flags(vxi, VXF_STATE_SETUP, 0))
		return -EACCES;

	if (vx_info_state(vxi, VXS_SHUTDOWN))
		return -EFAULT;

	old_vxi = task_get_vx_info(p);
	if (old_vxi == vxi)
		goto out;

//	if (!(ret = vx_migrate_user(p, vxi))) {
	{
		int openfd;

		task_lock(p);
		openfd = vx_openfd_task(p);

		if (old_vxi) {
			atomic_dec(&old_vxi->cvirt.nr_threads);
			atomic_dec(&old_vxi->cvirt.nr_running);
			__rlim_dec(&old_vxi->limit, RLIMIT_NPROC);
			/* FIXME: what about the struct files here? */
			__rlim_sub(&old_vxi->limit, VLIMIT_OPENFD, openfd);
			/* account for the executable */
			__rlim_dec(&old_vxi->limit, VLIMIT_DENTRY);
		}
		atomic_inc(&vxi->cvirt.nr_threads);
		atomic_inc(&vxi->cvirt.nr_running);
		__rlim_inc(&vxi->limit, RLIMIT_NPROC);
		/* FIXME: what about the struct files here? */
		__rlim_add(&vxi->limit, VLIMIT_OPENFD, openfd);
		/* account for the executable */
		__rlim_inc(&vxi->limit, VLIMIT_DENTRY);

		if (old_vxi) {
			release_vx_info(old_vxi, p);
			clr_vx_info(&p->vx_info);
		}
		claim_vx_info(vxi, p);
		set_vx_info(&p->vx_info, vxi);
		p->xid = vxi->vx_id;

		vxdprintk(VXD_CBIT(xid, 5),
			"moved task %p into vxi:%p[#%d]",
			p, vxi, vxi->vx_id);

		// vx_mask_cap_bset(vxi, p);
		task_unlock(p);

		/* hack for *spaces to provide compatibility */
		if (unshare) {
			struct nsproxy *old_nsp, *new_nsp;

			ret = unshare_nsproxy_namespaces(
				CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWUSER,
				&new_nsp, NULL, NULL);
			if (ret)
				goto out;

			old_nsp = xchg(&p->nsproxy, new_nsp);
			vx_set_space(vxi,
				CLONE_NEWUTS | CLONE_NEWIPC | CLONE_NEWUSER, 0);
			put_nsproxy(old_nsp);
		}
	}
out:
	put_vx_info(old_vxi);
	return ret;
}

int vx_set_reaper(struct vx_info *vxi, struct task_struct *p)
{
	struct task_struct *old_reaper;
	struct vx_info *reaper_vxi;

	if (!vxi)
		return -EINVAL;

	vxdprintk(VXD_CBIT(xid, 6),
		"vx_set_reaper(%p[#%d],%p[#%d,%d])",
		vxi, vxi->vx_id, p, p->xid, p->pid);

	old_reaper = vxi->vx_reaper;
	if (old_reaper == p)
		return 0;

	reaper_vxi = task_get_vx_info(p);
	if (reaper_vxi && reaper_vxi != vxi) {
		vxwprintk(1,
			"Unsuitable reaper [" VS_Q("%s") ",%u:#%u] "
			"for [xid #%u]",
			p->comm, p->pid, p->xid, vx_current_xid());
		goto out;
	}

	/* set new child reaper */
	get_task_struct(p);
	vxi->vx_reaper = p;
	put_task_struct(old_reaper);
out:
	put_vx_info(reaper_vxi);
	return 0;
}

int vx_set_init(struct vx_info *vxi, struct task_struct *p)
{
	if (!vxi)
		return -EINVAL;

	vxdprintk(VXD_CBIT(xid, 6),
		"vx_set_init(%p[#%d],%p[#%d,%d,%d])",
		vxi, vxi->vx_id, p, p->xid, p->pid, p->tgid);

	vxi->vx_flags &= ~VXF_STATE_INIT;
	// vxi->vx_initpid = p->tgid;
	vxi->vx_initpid = p->pid;
	return 0;
}

void vx_exit_init(struct vx_info *vxi, struct task_struct *p, int code)
{
	vxdprintk(VXD_CBIT(xid, 6),
		"vx_exit_init(%p[#%d],%p[#%d,%d,%d])",
		vxi, vxi->vx_id, p, p->xid, p->pid, p->tgid);

	vxi->exit_code = code;
	vxi->vx_initpid = 0;
}


void vx_set_persistent(struct vx_info *vxi)
{
	vxdprintk(VXD_CBIT(xid, 6),
		"vx_set_persistent(%p[#%d])", vxi, vxi->vx_id);

	get_vx_info(vxi);
	claim_vx_info(vxi, NULL);
}

void vx_clear_persistent(struct vx_info *vxi)
{
	vxdprintk(VXD_CBIT(xid, 6),
		"vx_clear_persistent(%p[#%d])", vxi, vxi->vx_id);

	release_vx_info(vxi, NULL);
	put_vx_info(vxi);
}

void vx_update_persistent(struct vx_info *vxi)
{
	if (vx_info_flags(vxi, VXF_PERSISTENT, 0))
		vx_set_persistent(vxi);
	else
		vx_clear_persistent(vxi);
}


/*	task must be current or locked		*/

void	exit_vx_info(struct task_struct *p, int code)
{
	struct vx_info *vxi = p->vx_info;

	if (vxi) {
		atomic_dec(&vxi->cvirt.nr_threads);
		vx_nproc_dec(p);

		vxi->exit_code = code;
		release_vx_info(vxi, p);
	}
}

void	exit_vx_info_early(struct task_struct *p, int code)
{
	struct vx_info *vxi = p->vx_info;

	if (vxi) {
		if (vxi->vx_initpid == p->pid)
			vx_exit_init(vxi, p, code);
		if (vxi->vx_reaper == p)
			vx_set_reaper(vxi, init_pid_ns.child_reaper);
	}
}


/* vserver syscall commands below here */

/* taks xid and vx_info functions */

#include <asm/uaccess.h>


int vc_task_xid(uint32_t id)
{
	vxid_t xid;

	if (id) {
		struct task_struct *tsk;

		rcu_read_lock();
		tsk = find_task_by_real_pid(id);
		xid = (tsk) ? tsk->xid : -ESRCH;
		rcu_read_unlock();
	} else
		xid = vx_current_xid();
	return xid;
}


int vc_vx_info(struct vx_info *vxi, void __user *data)
{
	struct vcmd_vx_info_v0 vc_data;

	vc_data.xid = vxi->vx_id;
	vc_data.initpid = vxi->vx_initpid;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


int vc_ctx_stat(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_stat_v0 vc_data;

	vc_data.usecnt = atomic_read(&vxi->vx_usecnt);
	vc_data.tasks = atomic_read(&vxi->vx_tasks);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


/* context functions */

int vc_ctx_create(uint32_t xid, void __user *data)
{
	struct vcmd_ctx_create vc_data = { .flagword = VXF_INIT_SET };
	struct vx_info *new_vxi;
	int ret;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if ((xid > MAX_S_CONTEXT) || (xid < 2))
		return -EINVAL;

	new_vxi = __create_vx_info(xid);
	if (IS_ERR(new_vxi))
		return PTR_ERR(new_vxi);

	/* initial flags */
	new_vxi->vx_flags = vc_data.flagword;

	ret = -ENOEXEC;
	if (vs_state_change(new_vxi, VSC_STARTUP))
		goto out;

	ret = vx_migrate_task(current, new_vxi, (!data));
	if (ret)
		goto out;

	/* return context id on success */
	ret = new_vxi->vx_id;

	/* get a reference for persistent contexts */
	if ((vc_data.flagword & VXF_PERSISTENT))
		vx_set_persistent(new_vxi);
out:
	release_vx_info(new_vxi, NULL);
	put_vx_info(new_vxi);
	return ret;
}


int vc_ctx_migrate(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_migrate vc_data = { .flagword = 0 };
	int ret;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = vx_migrate_task(current, vxi, 0);
	if (ret)
		return ret;
	if (vc_data.flagword & VXM_SET_INIT)
		ret = vx_set_init(vxi, current);
	if (ret)
		return ret;
	if (vc_data.flagword & VXM_SET_REAPER)
		ret = vx_set_reaper(vxi, current);
	return ret;
}


int vc_get_cflags(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_flags_v0 vc_data;

	vc_data.flagword = vxi->vx_flags;

	/* special STATE flag handling */
	vc_data.mask = vs_mask_flags(~0ULL, vxi->vx_flags, VXF_ONE_TIME);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_cflags(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_flags_v0 vc_data;
	uint64_t mask, trigger;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	/* special STATE flag handling */
	mask = vs_mask_mask(vc_data.mask, vxi->vx_flags, VXF_ONE_TIME);
	trigger = (mask & vxi->vx_flags) ^ (mask & vc_data.flagword);

	if (vxi == current_vx_info()) {
		/* if (trigger & VXF_STATE_SETUP)
			vx_mask_cap_bset(vxi, current); */
		if (trigger & VXF_STATE_INIT) {
			int ret;

			ret = vx_set_init(vxi, current);
			if (ret)
				return ret;
			ret = vx_set_reaper(vxi, current);
			if (ret)
				return ret;
		}
	}

	vxi->vx_flags = vs_mask_flags(vxi->vx_flags,
		vc_data.flagword, mask);
	if (trigger & VXF_PERSISTENT)
		vx_update_persistent(vxi);

	return 0;
}


static inline uint64_t caps_from_cap_t(kernel_cap_t c)
{
	uint64_t v = c.cap[0] | ((uint64_t)c.cap[1] << 32);

	// printk("caps_from_cap_t(%08x:%08x) = %016llx\n", c.cap[1], c.cap[0], v);
	return v;
}

static inline kernel_cap_t cap_t_from_caps(uint64_t v)
{
	kernel_cap_t c = __cap_empty_set;

	c.cap[0] = v & 0xFFFFFFFF;
	c.cap[1] = (v >> 32) & 0xFFFFFFFF;

	// printk("cap_t_from_caps(%016llx) = %08x:%08x\n", v, c.cap[1], c.cap[0]);
	return c;
}


static int do_get_caps(struct vx_info *vxi, uint64_t *bcaps, uint64_t *ccaps)
{
	if (bcaps)
		*bcaps = caps_from_cap_t(vxi->vx_bcaps);
	if (ccaps)
		*ccaps = vxi->vx_ccaps;

	return 0;
}

int vc_get_ccaps(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_caps_v1 vc_data;
	int ret;

	ret = do_get_caps(vxi, NULL, &vc_data.ccaps);
	if (ret)
		return ret;
	vc_data.cmask = ~0ULL;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

static int do_set_caps(struct vx_info *vxi,
	uint64_t bcaps, uint64_t bmask, uint64_t ccaps, uint64_t cmask)
{
	uint64_t bcold = caps_from_cap_t(vxi->vx_bcaps);

#if 0
	printk("do_set_caps(%16llx, %16llx, %16llx, %16llx)\n",
		bcaps, bmask, ccaps, cmask);
#endif
	vxi->vx_bcaps = cap_t_from_caps(
		vs_mask_flags(bcold, bcaps, bmask));
	vxi->vx_ccaps = vs_mask_flags(vxi->vx_ccaps, ccaps, cmask);

	return 0;
}

int vc_set_ccaps(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_caps_v1 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_caps(vxi, 0, 0, vc_data.ccaps, vc_data.cmask);
}

int vc_get_bcaps(struct vx_info *vxi, void __user *data)
{
	struct vcmd_bcaps vc_data;
	int ret;

	ret = do_get_caps(vxi, &vc_data.bcaps, NULL);
	if (ret)
		return ret;
	vc_data.bmask = ~0ULL;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_bcaps(struct vx_info *vxi, void __user *data)
{
	struct vcmd_bcaps vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_caps(vxi, vc_data.bcaps, vc_data.bmask, 0, 0);
}


int vc_get_umask(struct vx_info *vxi, void __user *data)
{
	struct vcmd_umask vc_data;

	vc_data.umask = vxi->vx_umask;
	vc_data.mask = ~0ULL;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_umask(struct vx_info *vxi, void __user *data)
{
	struct vcmd_umask vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi->vx_umask = vs_mask_flags(vxi->vx_umask,
		vc_data.umask, vc_data.mask);
	return 0;
}


int vc_get_wmask(struct vx_info *vxi, void __user *data)
{
	struct vcmd_wmask vc_data;

	vc_data.wmask = vxi->vx_wmask;
	vc_data.mask = ~0ULL;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_wmask(struct vx_info *vxi, void __user *data)
{
	struct vcmd_wmask vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi->vx_wmask = vs_mask_flags(vxi->vx_wmask,
		vc_data.wmask, vc_data.mask);
	return 0;
}


int vc_get_badness(struct vx_info *vxi, void __user *data)
{
	struct vcmd_badness_v0 vc_data;

	vc_data.bias = vxi->vx_badness_bias;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_badness(struct vx_info *vxi, void __user *data)
{
	struct vcmd_badness_v0 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi->vx_badness_bias = vc_data.bias;
	return 0;
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(free_vx_info);

