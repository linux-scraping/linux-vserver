/*
 * Quota code necessary even when VFS quota support is not compiled
 * into the kernel.  The interesting stuff is over in dquot.c, here
 * we have symbols for initial quotactl(2) handling, the sysctl(2)
 * variables, etc - things needed even when quota support disabled.
 */

#include <linux/fs.h>
#include <linux/namei.h>
#include <linux/slab.h>
#include <asm/current.h>
#include <asm/uaccess.h>
#include <linux/kernel.h>
#include <linux/smp_lock.h>
#include <linux/security.h>
#include <linux/syscalls.h>
#include <linux/buffer_head.h>
#include <linux/capability.h>
#include <linux/quotaops.h>
#include <linux/major.h>
#include <linux/blkdev.h>
#include <linux/vs_context.h>


/* Dquota Hash Management Functions */

static LIST_HEAD(dqhash_list);

static spinlock_t dqhash_lock = SPIN_LOCK_UNLOCKED;


struct dqhash *new_dqhash(struct super_block *sb, unsigned int id)
{
	struct dqhash *hash;
	unsigned long flags;
	int err;

	err = -ENOMEM;
	hash = kmalloc(sizeof(struct dqhash),  GFP_USER);
	if (!hash)
		goto out;

	memset(hash, 0, sizeof(struct dqhash));
	hash->dqh_id = id;
	atomic_set(&hash->dqh_count, 1);

	INIT_LIST_HEAD(&hash->dqh_list);

	mutex_init(&hash->dqh_dqopt.dqio_mutex);
	mutex_init(&hash->dqh_dqopt.dqonoff_mutex);
	init_rwsem(&hash->dqh_dqopt.dqptr_sem);
	hash->dqh_qop = sb->s_qop;
	hash->dqh_qcop = sb->s_qcop;
	hash->dqh_sb = sb;

	spin_lock_irqsave(&dqhash_lock, flags);
	list_add(&hash->dqh_list, &dqhash_list);
	spin_unlock_irqrestore(&dqhash_lock, flags);
	vxdprintk(VXD_CBIT(misc, 0),
		"new_dqhash: %p [#0x%08x]", hash, hash->dqh_id);
	return hash;

	// kfree(hash);
out:
	return ERR_PTR(err);
}

void destroy_dqhash(struct dqhash *hash)
{
	unsigned long flags;

	vxdprintk(VXD_CBIT(misc, 0),
		"destroy_dqhash: %p [#0x%08x] c=%d",
		hash, hash->dqh_id, atomic_read(&hash->dqh_count));
	spin_lock_irqsave(&dqhash_lock, flags);
	list_del_init(&hash->dqh_list);
	spin_unlock_irqrestore(&dqhash_lock, flags);
	kfree(hash);
}


struct dqhash *find_dqhash(unsigned int id)
{
	struct list_head *head;
	unsigned long flags;
	struct dqhash *hash;

	spin_lock_irqsave(&dqhash_lock, flags);
	list_for_each(head, &dqhash_list) {
		hash = list_entry(head, struct dqhash, dqh_list);
		if (hash->dqh_id == id)
			goto dqh_found;
	}
	hash = NULL;
dqh_found:
	spin_unlock_irqrestore(&dqhash_lock, flags);
	if (hash)
		dqhget(hash);
	return hash;
}


/* Check validity of generic quotactl commands */
static int generic_quotactl_valid(struct dqhash *hash, int type, int cmd, qid_t id)
{
	if (type >= MAXQUOTAS)
		return -EINVAL;
	if (!hash && cmd != Q_SYNC)
		return -ENODEV;
	/* Is operation supported? */
	if (hash && !hash->dqh_qcop)
		return -ENOSYS;

	switch (cmd) {
		case Q_GETFMT:
			break;
		case Q_QUOTAON:
			if (!hash->dqh_qcop->quota_on)
				return -ENOSYS;
			break;
		case Q_QUOTAOFF:
			if (!hash->dqh_qcop->quota_off)
				return -ENOSYS;
			break;
		case Q_SETINFO:
			if (!hash->dqh_qcop->set_info)
				return -ENOSYS;
			break;
		case Q_GETINFO:
			if (!hash->dqh_qcop->get_info)
				return -ENOSYS;
			break;
		case Q_SETQUOTA:
			if (!hash->dqh_qcop->set_dqblk)
				return -ENOSYS;
			break;
		case Q_GETQUOTA:
			if (!hash->dqh_qcop->get_dqblk)
				return -ENOSYS;
			break;
		case Q_SYNC:
			if (hash && !hash->dqh_qcop->quota_sync)
				return -ENOSYS;
			break;
		default:
			return -EINVAL;
	}

	/* Is quota turned on for commands which need it? */
	switch (cmd) {
		case Q_GETFMT:
		case Q_GETINFO:
		case Q_QUOTAOFF:
		case Q_SETINFO:
		case Q_SETQUOTA:
		case Q_GETQUOTA:
			/* This is just informative test so we are satisfied without a lock */
			if (!dqh_has_quota_enabled(hash, type))
				return -ESRCH;
	}

	/* Check privileges */
	if (cmd == Q_GETQUOTA) {
		if (((type == USRQUOTA && current->euid != id) ||
		     (type == GRPQUOTA && !in_egroup_p(id))) &&
		    !vx_capable(CAP_SYS_ADMIN, VXC_QUOTA_CTL))
			return -EPERM;
	}
	else if (cmd != Q_GETFMT && cmd != Q_SYNC && cmd != Q_GETINFO)
		if (!vx_capable(CAP_SYS_ADMIN, VXC_QUOTA_CTL))
			return -EPERM;

	return 0;
}

/* Check validity of XFS Quota Manager commands */
static int xqm_quotactl_valid(struct dqhash *hash, int type, int cmd, qid_t id)
{
	if (type >= XQM_MAXQUOTAS)
		return -EINVAL;
	if (!hash)
		return -ENODEV;
	if (!hash->dqh_qcop)
		return -ENOSYS;

	switch (cmd) {
		case Q_XQUOTAON:
		case Q_XQUOTAOFF:
		case Q_XQUOTARM:
			if (!hash->dqh_qcop->set_xstate)
				return -ENOSYS;
			break;
		case Q_XGETQSTAT:
			if (!hash->dqh_qcop->get_xstate)
				return -ENOSYS;
			break;
		case Q_XSETQLIM:
			if (!hash->dqh_qcop->set_xquota)
				return -ENOSYS;
			break;
		case Q_XGETQUOTA:
			if (!hash->dqh_qcop->get_xquota)
				return -ENOSYS;
			break;
		case Q_XQUOTASYNC:
			if (!hash->dqh_qcop->quota_sync)
				return -ENOSYS;
			break;
		default:
			return -EINVAL;
	}

	/* Check privileges */
	if (cmd == Q_XGETQUOTA) {
		if (((type == XQM_USRQUOTA && current->euid != id) ||
		     (type == XQM_GRPQUOTA && !in_egroup_p(id))) &&
		     !vx_capable(CAP_SYS_ADMIN, VXC_QUOTA_CTL))
			return -EPERM;
	} else if (cmd != Q_XGETQSTAT && cmd != Q_XQUOTASYNC) {
		if (!vx_capable(CAP_SYS_ADMIN, VXC_QUOTA_CTL))
			return -EPERM;
	}

	return 0;
}

static int check_quotactl_valid(struct dqhash *hash, int type, int cmd, qid_t id)
{
	int error;

	if (XQM_COMMAND(cmd))
		error = xqm_quotactl_valid(hash, type, cmd, id);
	else
		error = generic_quotactl_valid(hash, type, cmd, id);
	if (!error)
		error = security_quotactl(cmd, type, id, hash);
	return error;
}

static void quota_sync_sb(struct super_block *sb)
{
	/* This is not very clever (and fast) but currently I don't know about
	 * any other simple way of getting quota data to disk and we must get
	 * them there for userspace to be visible... */
	if (sb->s_op->sync_fs)
		sb->s_op->sync_fs(sb, 1);
	sync_blockdev(sb->s_bdev);
}

static void quota_sync_dqh(struct dqhash *hash, int type)
{
	int cnt;
	struct inode *discard[MAXQUOTAS];

	vxdprintk(VXD_CBIT(quota, 1),
		"quota_sync_dqh(%p,%d)", hash, type);
	hash->dqh_qcop->quota_sync(hash, type);

	quota_sync_sb(hash->dqh_sb);

	/* Now when everything is written we can discard the pagecache so
	 * that userspace sees the changes. We need i_mutex and so we could
	 * not do it inside dqonoff_mutex. Moreover we need to be carefull
	 * about races with quotaoff() (that is the reason why we have own
	 * reference to inode). */
	mutex_lock(&dqh_dqopt(hash)->dqonoff_mutex);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		discard[cnt] = NULL;
		if (type != -1 && cnt != type)
			continue;
		if (!dqh_has_quota_enabled(hash, cnt))
			continue;
		vxdprintk(VXD_CBIT(quota, 0),
			"quota_sync_dqh(%p,%d) discard inode %p",
			hash, type, dqh_dqopt(hash)->files[cnt]);
		discard[cnt] = igrab(dqh_dqopt(hash)->files[cnt]);
	}
	mutex_unlock(&dqh_dqopt(hash)->dqonoff_mutex);
	for (cnt = 0; cnt < MAXQUOTAS; cnt++) {
		if (discard[cnt]) {
			mutex_lock(&discard[cnt]->i_mutex);
			truncate_inode_pages(&discard[cnt]->i_data, 0);
			mutex_unlock(&discard[cnt]->i_mutex);
			iput(discard[cnt]);
		}
	}
}

void sync_dquots_dqh(struct dqhash *hash, int type)
{
	vxdprintk(VXD_CBIT(quota, 1),
		"sync_dquots_dqh(%p,%d)", hash, type);

	if (hash->dqh_qcop->quota_sync)
		quota_sync_dqh(hash, type);
}

void sync_dquots(struct dqhash *hash, int type)

{
	vxdprintk(VXD_CBIT(quota, 1),
		"sync_dquots(%p,%d)", hash, type);

	if (hash) {
		if (hash->dqh_qcop->quota_sync)
			quota_sync_dqh(hash, type);
		return;
	}
}

/* Copy parameters and call proper function */
static int do_quotactl(struct dqhash *hash, int type, int cmd, qid_t id, void __user *addr)
{
	int ret;

	vxdprintk(VXD_CBIT(quota, 3),
		"do_quotactl(%p,%d,cmd=%d,id=%d,%p)", hash, type, cmd, id, addr);

	switch (cmd) {
		case Q_QUOTAON: {
			char *pathname;

			if (IS_ERR(pathname = getname(addr)))
				return PTR_ERR(pathname);
			ret = hash->dqh_qcop->quota_on(hash, type, id, pathname);
			putname(pathname);
			return ret;
		}
		case Q_QUOTAOFF:
			return hash->dqh_qcop->quota_off(hash, type);

		case Q_GETFMT: {
			__u32 fmt;

			down_read(&dqh_dqopt(hash)->dqptr_sem);
			if (!dqh_has_quota_enabled(hash, type)) {
				up_read(&dqh_dqopt(hash)->dqptr_sem);
				return -ESRCH;
			}
			fmt = dqh_dqopt(hash)->info[type].dqi_format->qf_fmt_id;
			up_read(&dqh_dqopt(hash)->dqptr_sem);
			if (copy_to_user(addr, &fmt, sizeof(fmt)))
				return -EFAULT;
			return 0;
		}
		case Q_GETINFO: {
			struct if_dqinfo info;

			if ((ret = hash->dqh_qcop->get_info(hash, type, &info)))
				return ret;
			if (copy_to_user(addr, &info, sizeof(info)))
				return -EFAULT;
			return 0;
		}
		case Q_SETINFO: {
			struct if_dqinfo info;

			if (copy_from_user(&info, addr, sizeof(info)))
				return -EFAULT;
			return hash->dqh_qcop->set_info(hash, type, &info);
		}
		case Q_GETQUOTA: {
			struct if_dqblk idq;

			if ((ret = hash->dqh_qcop->get_dqblk(hash, type, id, &idq)))
				return ret;
			if (copy_to_user(addr, &idq, sizeof(idq)))
				return -EFAULT;
			return 0;
		}
		case Q_SETQUOTA: {
			struct if_dqblk idq;

			if (copy_from_user(&idq, addr, sizeof(idq)))
				return -EFAULT;
			return hash->dqh_qcop->set_dqblk(hash, type, id, &idq);
		}
		case Q_SYNC:
			sync_dquots_dqh(hash, type);
			return 0;

		case Q_XQUOTAON:
		case Q_XQUOTAOFF:
		case Q_XQUOTARM: {
			__u32 flags;

			if (copy_from_user(&flags, addr, sizeof(flags)))
				return -EFAULT;
			return hash->dqh_qcop->set_xstate(hash, flags, cmd);
		}
		case Q_XGETQSTAT: {
			struct fs_quota_stat fqs;
		
			if ((ret = hash->dqh_qcop->get_xstate(hash, &fqs)))
				return ret;
			if (copy_to_user(addr, &fqs, sizeof(fqs)))
				return -EFAULT;
			return 0;
		}
		case Q_XSETQLIM: {
			struct fs_disk_quota fdq;

			if (copy_from_user(&fdq, addr, sizeof(fdq)))
				return -EFAULT;
		       return hash->dqh_qcop->set_xquota(hash, type, id, &fdq);
		}
		case Q_XGETQUOTA: {
			struct fs_disk_quota fdq;

			if ((ret = hash->dqh_qcop->get_xquota(hash, type, id, &fdq)))
				return ret;
			if (copy_to_user(addr, &fdq, sizeof(fdq)))
				return -EFAULT;
			return 0;
		}
		case Q_XQUOTASYNC:
			return hash->dqh_qcop->quota_sync(hash, type);
		/* We never reach here unless validity check is broken */
		default:
			BUG();
	}
	return 0;
}

/*
 * look up a superblock on which quota ops will be performed
 * - use the name of a block device to find the superblock thereon
 */
static inline struct super_block *quotactl_block(const char __user *special)
{
#ifdef CONFIG_BLOCK
	struct block_device *bdev;
	struct super_block *sb;
	char *tmp = getname(special);

	if (IS_ERR(tmp))
		return ERR_PTR(PTR_ERR(tmp));
	bdev = lookup_bdev(tmp);
	putname(tmp);
	if (IS_ERR(bdev))
		return ERR_PTR(PTR_ERR(bdev));
	sb = get_super(bdev);
	bdput(bdev);
	if (!sb)
		return ERR_PTR(-ENODEV);

	return sb;
#else
	return ERR_PTR(-ENODEV);
#endif
}

#if defined(CONFIG_BLK_DEV_VROOT) || defined(CONFIG_BLK_DEV_VROOT_MODULE)

#include <linux/vroot.h>
#include <linux/kallsyms.h>

static vroot_grb_func *vroot_get_real_bdev = NULL;

static spinlock_t vroot_grb_lock = SPIN_LOCK_UNLOCKED;

int register_vroot_grb(vroot_grb_func *func) {
	int ret = -EBUSY;

	spin_lock(&vroot_grb_lock);
	if (!vroot_get_real_bdev) {
		vroot_get_real_bdev = func;
		ret = 0;
	}
	spin_unlock(&vroot_grb_lock);
	return ret;
}
EXPORT_SYMBOL(register_vroot_grb);

int unregister_vroot_grb(vroot_grb_func *func) {
	int ret = -EINVAL;

	spin_lock(&vroot_grb_lock);
	if (vroot_get_real_bdev) {
		vroot_get_real_bdev = NULL;
		ret = 0;
	}
	spin_unlock(&vroot_grb_lock);
	return ret;
}
EXPORT_SYMBOL(unregister_vroot_grb);

#endif

/*
 * This is the system call interface. This communicates with
 * the user-level programs. Currently this only supports diskquota
 * calls. Maybe we need to add the process quotas etc. in the future,
 * but we probably should use rlimits for that.
 */
asmlinkage long sys_quotactl(unsigned int cmd, const char __user *special, qid_t id, void __user *addr)
{
	uint cmds, type;
	struct super_block *sb = NULL;
	struct dqhash *dqh = NULL;
	int ret;

	cmds = cmd >> SUBCMDSHIFT;
	type = cmd & SUBCMDMASK;

	if (cmds != Q_SYNC || special) {
		sb = quotactl_block(special);
		if (IS_ERR(sb))
			return PTR_ERR(sb);
	}

	if (sb)
		dqh = sb->s_dqh;
	ret = check_quotactl_valid(dqh, type, cmds, id);
	if (ret >= 0)
		ret = do_quotactl(dqh, type, cmds, id, addr);
	if (sb)
		drop_super(sb);

	return ret;
}
