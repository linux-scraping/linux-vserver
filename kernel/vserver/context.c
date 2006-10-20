/*
 *  linux/kernel/vserver/context.c
 *
 *  Virtual Server: Context Support
 *
 *  Copyright (C) 2003-2005  Herbert P�tzl
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
 *
 */

#include <linux/slab.h>
#include <linux/types.h>
#include <linux/namespace.h>

#include <linux/sched.h>
#include <linux/vserver/network.h>
#include <linux/vserver/legacy.h>
#include <linux/vserver/limit.h>
#include <linux/vserver/debug.h>
#include <linux/vserver/limit_int.h>

#include <linux/vs_context.h>
#include <linux/vs_limit.h>
#include <linux/vserver/context_cmd.h>

#include <linux/err.h>
#include <asm/errno.h>

#include "cvirt_init.h"
#include "limit_init.h"
#include "sched_init.h"


/*	__alloc_vx_info()

	* allocate an initialized vx_info struct
	* doesn't make it visible (hash)			*/

static struct vx_info *__alloc_vx_info(xid_t xid)
{
	struct vx_info *new = NULL;

	vxdprintk(VXD_CBIT(xid, 0), "alloc_vx_info(%d)*", xid);

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct vx_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset (new, 0, sizeof(struct vx_info));
	new->vx_id = xid;
	INIT_HLIST_NODE(&new->vx_hlist);
	atomic_set(&new->vx_usecnt, 0);
	atomic_set(&new->vx_tasks, 0);
	new->vx_parent = NULL;
	new->vx_state = 0;
	init_waitqueue_head(&new->vx_wait);

	/* prepare reaper */
	get_task_struct(child_reaper);
	new->vx_reaper = child_reaper;

	/* rest of init goes here */
	vx_info_init_limit(&new->limit);
	vx_info_init_sched(&new->sched);
	vx_info_init_cvirt(&new->cvirt);
	vx_info_init_cacct(&new->cacct);

	new->vx_flags = VXF_INIT_SET;
	new->vx_bcaps = CAP_INIT_EFF_SET;
	new->vx_ccaps = 0;

	new->reboot_cmd = 0;
	new->exit_code = 0;

	vxdprintk(VXD_CBIT(xid, 0),
		"alloc_vx_info(%d) = %p", xid, new);
	vxh_alloc_vx_info(new);
	return new;
}

/*	__dealloc_vx_info()

	* final disposal of vx_info				*/

static void __dealloc_vx_info(struct vx_info *vxi)
{
	vxdprintk(VXD_CBIT(xid, 0),
		"dealloc_vx_info(%p)", vxi);
	vxh_dealloc_vx_info(vxi);

	vxi->vx_hlist.next = LIST_POISON1;
	vxi->vx_id = -1;

	vx_info_exit_limit(&vxi->limit);
	vx_info_exit_sched(&vxi->sched);
	vx_info_exit_cvirt(&vxi->cvirt);
	vx_info_exit_cacct(&vxi->cacct);

	vxi->vx_state |= VXS_RELEASED;
	kfree(vxi);
}

static void __shutdown_vx_info(struct vx_info *vxi)
{
	struct namespace *namespace;
	struct fs_struct *fs;

	might_sleep();

	vxi->vx_state |= VXS_SHUTDOWN;
	vs_state_change(vxi, VSC_SHUTDOWN);

	namespace = xchg(&vxi->vx_namespace, NULL);
	if (namespace)
		put_namespace(namespace);

	fs = xchg(&vxi->vx_fs, NULL);
	if (fs)
		put_fs_struct(fs);
}

/* exported stuff */

void free_vx_info(struct vx_info *vxi)
{
	/* context shutdown is mandatory */
	BUG_ON(!vx_info_state(vxi, VXS_SHUTDOWN));

	BUG_ON(atomic_read(&vxi->vx_usecnt));
	BUG_ON(atomic_read(&vxi->vx_tasks));

	BUG_ON(vx_info_state(vxi, VXS_HASHED));

	BUG_ON(vxi->vx_namespace);
	BUG_ON(vxi->vx_fs);

	__dealloc_vx_info(vxi);
}


/*	hash table for vx_info hash */

#define VX_HASH_SIZE	13

struct hlist_head vx_info_hash[VX_HASH_SIZE];

static spinlock_t vx_info_hash_lock = SPIN_LOCK_UNLOCKED;


static inline unsigned int __hashval(xid_t xid)
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
}

/*	__unhash_vx_info()

	* remove the vxi from the global hash table
	* requires the hash_lock to be held			*/

static inline void __unhash_vx_info(struct vx_info *vxi)
{
	vxd_assert_lock(&vx_info_hash_lock);
	vxdprintk(VXD_CBIT(xid, 4),
		"__unhash_vx_info: %p[#%d]", vxi, vxi->vx_id);
	vxh_unhash_vx_info(vxi);

	/* context must be hashed */
	BUG_ON(!vx_info_state(vxi, VXS_HASHED));

	vxi->vx_state &= ~VXS_HASHED;
	hlist_del(&vxi->vx_hlist);
}


/*	__lookup_vx_info()

	* requires the hash_lock to be held
	* doesn't increment the vx_refcnt			*/

static inline struct vx_info *__lookup_vx_info(xid_t xid)
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
		xid, vxi, vxi?vxi->vx_id:0);
	vxh_lookup_vx_info(vxi, xid);
	return vxi;
}


/*	__vx_dynamic_id()

	* find unused dynamic xid
	* requires the hash_lock to be held			*/

static inline xid_t __vx_dynamic_id(void)
{
	static xid_t seq = MAX_S_CONTEXT;
	xid_t barrier = seq;

	vxd_assert_lock(&vx_info_hash_lock);
	do {
		if (++seq > MAX_S_CONTEXT)
			seq = MIN_D_CONTEXT;
		if (!__lookup_vx_info(seq)) {
			vxdprintk(VXD_CBIT(xid, 4),
				"__vx_dynamic_id: [#%d]", seq);
			return seq;
		}
	} while (barrier != seq);
	return 0;
}

#ifdef	CONFIG_VSERVER_LEGACY

/*	__loc_vx_info()

	* locate or create the requested context
	* get() it and if new hash it				*/

static struct vx_info * __loc_vx_info(int id, int *err)
{
	struct vx_info *new, *vxi = NULL;

	vxdprintk(VXD_CBIT(xid, 1), "loc_vx_info(%d)*", id);

	if (!(new = __alloc_vx_info(id))) {
		*err = -ENOMEM;
		return NULL;
	}

	/* required to make dynamic xids unique */
	spin_lock(&vx_info_hash_lock);

	/* dynamic context requested */
	if (id == VX_DYNAMIC_ID) {
		id = __vx_dynamic_id();
		if (!id) {
			printk(KERN_ERR "no dynamic context available.\n");
			goto out_unlock;
		}
		new->vx_id = id;
	}
	/* existing context requested */
	else if ((vxi = __lookup_vx_info(id))) {
		/* context in setup is not available */
		if (vxi->vx_flags & VXF_STATE_SETUP) {
			vxdprintk(VXD_CBIT(xid, 0),
				"loc_vx_info(%d) = %p (not available)", id, vxi);
			vxi = NULL;
			*err = -EBUSY;
		} else {
			vxdprintk(VXD_CBIT(xid, 0),
				"loc_vx_info(%d) = %p (found)", id, vxi);
			get_vx_info(vxi);
			*err = 0;
		}
		goto out_unlock;
	}

	/* new context requested */
	vxdprintk(VXD_CBIT(xid, 0),
		"loc_vx_info(%d) = %p (new)", id, new);
	__hash_vx_info(get_vx_info(new));
	vxi = new, new = NULL;
	*err = 1;

out_unlock:
	spin_unlock(&vx_info_hash_lock);
	vxh_loc_vx_info(vxi, id);
	if (new)
		__dealloc_vx_info(new);
	return vxi;
}

#endif

/*	__create_vx_info()

	* create the requested context
	* get() and hash it					*/

static struct vx_info * __create_vx_info(int id)
{
	struct vx_info *new, *vxi = NULL;

	vxdprintk(VXD_CBIT(xid, 1), "create_vx_info(%d)*", id);

	if (!(new = __alloc_vx_info(id)))
		return ERR_PTR(-ENOMEM);

	/* required to make dynamic xids unique */
	spin_lock(&vx_info_hash_lock);

	/* dynamic context requested */
	if (id == VX_DYNAMIC_ID) {
		id = __vx_dynamic_id();
		if (!id) {
			printk(KERN_ERR "no dynamic context available.\n");
			vxi = ERR_PTR(-EAGAIN);
			goto out_unlock;
		}
		new->vx_id = id;
	}
	/* static context requested */
	else if ((vxi = __lookup_vx_info(id))) {
		vxdprintk(VXD_CBIT(xid, 0),
			"create_vx_info(%d) = %p (already there)", id, vxi);
		if (vx_info_flags(vxi, VXF_STATE_SETUP, 0))
			vxi = ERR_PTR(-EBUSY);
		else
			vxi = ERR_PTR(-EEXIST);
		goto out_unlock;
	}
	/* dynamic xid creation blocker */
	else if (id >= MIN_D_CONTEXT) {
		vxdprintk(VXD_CBIT(xid, 0),
			"create_vx_info(%d) (dynamic rejected)", id);
		vxi = ERR_PTR(-EINVAL);
		goto out_unlock;
	}

	/* new context */
	vxdprintk(VXD_CBIT(xid, 0),
		"create_vx_info(%d) = %p (new)", id, new);
	__hash_vx_info(get_vx_info(new));
	vxi = new, new = NULL;

out_unlock:
	spin_unlock(&vx_info_hash_lock);
	vxh_create_vx_info(IS_ERR(vxi)?NULL:vxi, id);
	if (new)
		__dealloc_vx_info(new);
	return vxi;
}


/*	exported stuff						*/


void unhash_vx_info(struct vx_info *vxi)
{
	__shutdown_vx_info(vxi);
	spin_lock(&vx_info_hash_lock);
	__unhash_vx_info(vxi);
	spin_unlock(&vx_info_hash_lock);
	__wakeup_vx_info(vxi);
}


/*	lookup_vx_info()

	* search for a vx_info and get() it
	* negative id means current				*/

struct vx_info *lookup_vx_info(int id)
{
	struct vx_info *vxi = NULL;

	if (id < 0) {
		vxi = get_vx_info(current->vx_info);
	} else if (id > 1) {
		spin_lock(&vx_info_hash_lock);
		vxi = get_vx_info(__lookup_vx_info(id));
		spin_unlock(&vx_info_hash_lock);
	}
	return vxi;
}

/*	xid_is_hashed()

	* verify that xid is still hashed			*/

int xid_is_hashed(xid_t xid)
{
	int hashed;

	spin_lock(&vx_info_hash_lock);
	hashed = (__lookup_vx_info(xid) != NULL);
	spin_unlock(&vx_info_hash_lock);
	return hashed;
}

#ifdef	CONFIG_VSERVER_LEGACY

struct vx_info *lookup_or_create_vx_info(int id)
{
	int err;

	return __loc_vx_info(id, &err);
}

#endif

#ifdef	CONFIG_PROC_FS

int get_xid_list(int index, unsigned int *xids, int size)
{
	int hindex, nr_xids = 0;

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


int vx_migrate_user(struct task_struct *p, struct vx_info *vxi)
{
	struct user_struct *new_user, *old_user;

	if (!p || !vxi)
		BUG();
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

void vx_mask_bcaps(struct vx_info *vxi, struct task_struct *p)
{
	p->cap_effective &= vxi->vx_bcaps;
	p->cap_inheritable &= vxi->vx_bcaps;
	p->cap_permitted &= vxi->vx_bcaps;
}


#include <linux/file.h>

static int vx_openfd_task(struct task_struct *tsk)
{
	struct files_struct *files = tsk->files;
	struct fdtable *fdt;
	const unsigned long *bptr;
	int count, total;

	/* no rcu_read_lock() because of spin_lock() */
	spin_lock(&files->file_lock);
	fdt = files_fdtable(files);
	bptr = fdt->open_fds->fds_bits;
	count = fdt->max_fds / (sizeof(unsigned long) * 8);
	for (total = 0; count > 0; count--) {
		if (*bptr)
			total += hweight_long(*bptr);
		bptr++;
	}
	spin_unlock(&files->file_lock);
	return total;
}

/*
 *	migrate task to new context
 *	gets vxi, puts old_vxi on change
 */

int vx_migrate_task(struct task_struct *p, struct vx_info *vxi)
{
	struct vx_info *old_vxi;
	int ret = 0;

	if (!p || !vxi)
		BUG();

	old_vxi = task_get_vx_info(p);
	if (old_vxi == vxi)
		goto out;

	vxdprintk(VXD_CBIT(xid, 5),
		"vx_migrate_task(%p,%p[#%d.%d])", p, vxi,
		vxi->vx_id, atomic_read(&vxi->vx_usecnt));

	if (!(ret = vx_migrate_user(p, vxi))) {
		int openfd;

		task_lock(p);
		openfd = vx_openfd_task(p);

		if (old_vxi) {
			atomic_dec(&old_vxi->cvirt.nr_threads);
			atomic_dec(&old_vxi->cvirt.nr_running);
			atomic_dec(&old_vxi->limit.rcur[RLIMIT_NPROC]);
			/* FIXME: what about the struct files here? */
			atomic_sub(openfd, &old_vxi->limit.rcur[VLIMIT_OPENFD]);
		}
		atomic_inc(&vxi->cvirt.nr_threads);
		atomic_inc(&vxi->cvirt.nr_running);
		atomic_inc(&vxi->limit.rcur[RLIMIT_NPROC]);
		/* FIXME: what about the struct files here? */
		atomic_add(openfd, &vxi->limit.rcur[VLIMIT_OPENFD]);

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

		vx_mask_bcaps(vxi, p);
		task_unlock(p);
	}
out:
	put_vx_info(old_vxi);
	return ret;
}

int vx_set_reaper(struct vx_info *vxi, struct task_struct *p)
{
	struct task_struct *old_reaper;

	if (!vxi)
		return -EINVAL;

	vxdprintk(VXD_CBIT(xid, 6),
		"vx_set_reaper(%p[#%d],%p[#%d,%d])",
		vxi, vxi->vx_id, p, p->xid, p->pid);

	old_reaper = vxi->vx_reaper;
	if (old_reaper == p)
		return 0;

	/* set new child reaper */
	get_task_struct(p);
	vxi->vx_reaper = p;
	put_task_struct(old_reaper);
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
	vxi->vx_initpid = p->tgid;
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
	claim_vx_info(vxi, current);
}

void vx_clear_persistent(struct vx_info *vxi)
{
	vxdprintk(VXD_CBIT(xid, 6),
		"vx_clear_persistent(%p[#%d])", vxi, vxi->vx_id);

	release_vx_info(vxi, current);
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
		if (vxi->vx_initpid == p->tgid)
			vx_exit_init(vxi, p, code);
		if (vxi->vx_reaper == p)
			vx_set_reaper(vxi, child_reaper);
	}
}


/* vserver syscall commands below here */

/* taks xid and vx_info functions */

#include <asm/uaccess.h>


int vc_task_xid(uint32_t id, void __user *data)
{
	xid_t xid;

	if (id) {
		struct task_struct *tsk;

		if (!vx_check(0, VX_ADMIN|VX_WATCH))
			return -EPERM;

		read_lock(&tasklist_lock);
		tsk = find_task_by_real_pid(id);
		xid = (tsk) ? tsk->xid : -ESRCH;
		read_unlock(&tasklist_lock);
	}
	else
		xid = vx_current_xid();
	return xid;
}


int vc_vx_info(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_vx_info_v0 vc_data;

	if (!vx_check(0, VX_ADMIN))
		return -ENOSYS;
	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RESOURCE))
		return -EPERM;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.xid = vxi->vx_id;
	vc_data.initpid = vxi->vx_initpid;
	put_vx_info(vxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


/* context functions */

int vc_ctx_create(uint32_t xid, void __user *data)
{
	struct vcmd_ctx_create vc_data = { .flagword = VXF_INIT_SET };
	struct vx_info *new_vxi;
	int ret;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (data && copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if ((xid > MAX_S_CONTEXT) && (xid != VX_DYNAMIC_ID))
		return -EINVAL;
	if (xid < 2)
		return -EINVAL;

	new_vxi = __create_vx_info(xid);
	if (IS_ERR(new_vxi))
		return PTR_ERR(new_vxi);

	/* initial flags */
	new_vxi->vx_flags = vc_data.flagword;

	/* get a reference for persistent contexts */
	if ((vc_data.flagword & VXF_PERSISTENT))
		vx_set_persistent(new_vxi);

	ret = -ENOEXEC;
	if (vs_state_change(new_vxi, VSC_STARTUP))
		goto out_unhash;
	ret = vx_migrate_task(current, new_vxi);
	if (!ret) {
		/* return context id on success */
		ret = new_vxi->vx_id;
		goto out;
	}
out_unhash:
	/* prepare for context disposal */
	new_vxi->vx_state |= VXS_SHUTDOWN;
	if ((vc_data.flagword & VXF_PERSISTENT))
		vx_clear_persistent(new_vxi);
	__unhash_vx_info(new_vxi);
out:
	put_vx_info(new_vxi);
	return ret;
}


int vc_ctx_migrate(uint32_t id, void __user *data)
{
	struct vcmd_ctx_migrate vc_data = { .flagword = 0 };
	struct vx_info *vxi;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (data && copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	/* dirty hack until Spectator becomes a cap */
	if (id == 1) {
		current->xid = 1;
		return 0;
	}

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;
	vx_migrate_task(current, vxi);
	if (vc_data.flagword & VXM_SET_INIT)
		vx_set_init(vxi, current);
	if (vc_data.flagword & VXM_SET_REAPER)
		vx_set_reaper(vxi, current);
	put_vx_info(vxi);
	return 0;
}


int vc_get_cflags(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_flags_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.flagword = vxi->vx_flags;

	/* special STATE flag handling */
	vc_data.mask = vx_mask_flags(~0UL, vxi->vx_flags, VXF_ONE_TIME);

	put_vx_info(vxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_cflags(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_flags_v0 vc_data;
	uint64_t mask, trigger;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	/* special STATE flag handling */
	mask = vx_mask_mask(vc_data.mask, vxi->vx_flags, VXF_ONE_TIME);
	trigger = (mask & vxi->vx_flags) ^ (mask & vc_data.flagword);

	if (vxi == current->vx_info) {
		if (trigger & VXF_STATE_SETUP)
			vx_mask_bcaps(vxi, current);
		if (trigger & VXF_STATE_INIT) {
			vx_set_init(vxi, current);
			vx_set_reaper(vxi, current);
		}
	}

	vxi->vx_flags = vx_mask_flags(vxi->vx_flags,
		vc_data.flagword, mask);
	if (trigger & VXF_PERSISTENT)
		vx_update_persistent(vxi);

	put_vx_info(vxi);
	return 0;
}

int vc_get_ccaps(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_caps_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.bcaps = vxi->vx_bcaps;
	vc_data.ccaps = vxi->vx_ccaps;
	vc_data.cmask = ~0UL;
	put_vx_info(vxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_ccaps(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_caps_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vxi->vx_bcaps &= vc_data.bcaps;
	vxi->vx_ccaps = vx_mask_flags(vxi->vx_ccaps,
		vc_data.ccaps, vc_data.cmask);
	put_vx_info(vxi);
	return 0;
}

#include <linux/module.h>

EXPORT_SYMBOL_GPL(free_vx_info);

