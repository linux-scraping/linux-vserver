/*
 *  Copyright (C) 2006 IBM Corporation
 *
 *  Author: Serge Hallyn <serue@us.ibm.com>
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License as
 *  published by the Free Software Foundation, version 2 of the
 *  License.
 *
 *  Jun 2006 - namespaces support
 *             OpenVZ, SWsoft Inc.
 *             Pavel Emelianov <xemul@openvz.org>
 */

#include <linux/module.h>
#include <linux/version.h>
#include <linux/nsproxy.h>
#include <linux/init_task.h>
#include <linux/mnt_namespace.h>
#include <linux/utsname.h>
#include <linux/pid_namespace.h>
#include <linux/vserver/global.h>
#include <linux/vserver/debug.h>

struct nsproxy init_nsproxy = INIT_NSPROXY(init_nsproxy);

void get_task_namespaces(struct task_struct *tsk)
{
	struct nsproxy *ns = tsk->nsproxy;
	if (ns) {
		get_nsproxy(ns);
	}
}

/*
 * creates a copy of "orig" with refcount 1.
 */
static inline struct nsproxy *clone_nsproxy(struct nsproxy *orig)
{
	struct nsproxy *ns;

	ns = kmemdup(orig, sizeof(struct nsproxy), GFP_KERNEL);
	if (ns)
		atomic_set(&ns->count, 1);
	vxdprintk(VXD_CBIT(space, 2), "clone_nsproxy(%p[%u] = %p[1]",
		orig, atomic_read(&orig->count), ns);
	atomic_inc(&vs_global_nsproxy);
	return ns;
}

/*
 * Create new nsproxy and all of its the associated namespaces.
 * Return the newly created nsproxy.  Do not attach this to the task,
 * leave it to the caller to do proper locking and attach it to task.
 */
static struct nsproxy *unshare_namespaces(int flags, struct nsproxy *orig,
			struct fs_struct *new_fs)
{
	struct nsproxy *new_nsp;

	vxdprintk(VXD_CBIT(space, 4),
		"unshare_namespaces(0x%08x,%p,%p)",
		flags, orig, new_fs);

	new_nsp = clone_nsproxy(orig);
	if (!new_nsp)
		return ERR_PTR(-ENOMEM);

	new_nsp->mnt_ns = copy_mnt_ns(flags, orig->mnt_ns, new_fs);
	if (IS_ERR(new_nsp->mnt_ns))
		goto out_ns;

	new_nsp->uts_ns = copy_utsname(flags, orig->uts_ns);
	if (IS_ERR(new_nsp->uts_ns))
		goto out_uts;

	new_nsp->ipc_ns = copy_ipcs(flags, orig->ipc_ns);
	if (IS_ERR(new_nsp->ipc_ns))
		goto out_ipc;

	new_nsp->pid_ns = copy_pid_ns(flags, orig->pid_ns);
	if (IS_ERR(new_nsp->pid_ns))
		goto out_pid;

	return new_nsp;

out_pid:
	if (new_nsp->ipc_ns)
		put_ipc_ns(new_nsp->ipc_ns);
out_ipc:
	if (new_nsp->uts_ns)
		put_uts_ns(new_nsp->uts_ns);
out_uts:
	if (new_nsp->mnt_ns)
		put_mnt_ns(new_nsp->mnt_ns);
out_ns:
	kfree(new_nsp);
	return ERR_PTR(-ENOMEM);
}

static struct nsproxy *create_new_namespaces(int flags, struct task_struct *tsk,
			struct fs_struct *new_fs)
{
	return unshare_namespaces(flags, tsk->nsproxy, new_fs);
}

/*
 * copies the nsproxy, setting refcount to 1, and grabbing a
 * reference to all contained namespaces.
 */
struct nsproxy *copy_nsproxy(struct nsproxy *orig)
{
	struct nsproxy *ns = clone_nsproxy(orig);

	if (ns) {
		if (ns->mnt_ns)
			get_mnt_ns(ns->mnt_ns);
		if (ns->uts_ns)
			get_uts_ns(ns->uts_ns);
		if (ns->ipc_ns)
			get_ipc_ns(ns->ipc_ns);
		if (ns->pid_ns)
			get_pid_ns(ns->pid_ns);
	}
	return ns;
}

/*
 * called from clone.  This now handles copy for nsproxy and all
 * namespaces therein.
 */
int copy_namespaces(int flags, struct task_struct *tsk)
{
	struct nsproxy *old_ns = tsk->nsproxy;
	struct nsproxy *new_ns;
	int err = 0;

	vxdprintk(VXD_CBIT(space, 7), "copy_namespaces(0x%08x,%p[%p])",
		flags, tsk, old_ns);

	if (!old_ns)
		return 0;

	get_nsproxy(old_ns);

	if (!(flags & (CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC)))
		return 0;

	if (!capable(CAP_SYS_ADMIN)) {
		err = -EPERM;
		goto out;
	}

	new_ns = create_new_namespaces(flags, tsk, tsk->fs);
	if (IS_ERR(new_ns)) {
		err = PTR_ERR(new_ns);
		goto out;
	}

	tsk->nsproxy = new_ns;
out:
	put_nsproxy(old_ns);
	vxdprintk(VXD_CBIT(space, 3),
		"copy_namespaces(0x%08x,%p[%p]) = %d [%p]",
		flags, tsk, old_ns, err, new_ns);
	return err;
}

void free_nsproxy(struct nsproxy *ns)
{
	if (ns->mnt_ns)
		put_mnt_ns(ns->mnt_ns);
	if (ns->uts_ns)
		put_uts_ns(ns->uts_ns);
	if (ns->ipc_ns)
		put_ipc_ns(ns->ipc_ns);
	if (ns->pid_ns)
		put_pid_ns(ns->pid_ns);
	atomic_dec(&vs_global_nsproxy);
	kfree(ns);
}

/*
 * Called from unshare. Unshare all the namespaces part of nsproxy.
 * On sucess, returns the new nsproxy and a reference to old nsproxy
 * to make sure it stays around.
 */
int unshare_nsproxy_namespaces(unsigned long unshare_flags,
		struct nsproxy **new_nsp, struct fs_struct *new_fs)
{
	struct nsproxy *old_ns = current->nsproxy;
	int err = 0;

	vxdprintk(VXD_CBIT(space, 4),
		"unshare_nsproxy_namespaces(0x%08x,[%p])",
		unshare_flags, old_ns);

	if (!(unshare_flags & (CLONE_NEWNS | CLONE_NEWUTS | CLONE_NEWIPC)))
		return 0;

#ifndef CONFIG_IPC_NS
	if (unshare_flags & CLONE_NEWIPC)
		return -EINVAL;
#endif

#ifndef CONFIG_UTS_NS
	if (unshare_flags & CLONE_NEWUTS)
		return -EINVAL;
#endif

	if (!capable(CAP_SYS_ADMIN))
		return -EPERM;

	get_nsproxy(old_ns);

	*new_nsp = create_new_namespaces(unshare_flags, current,
				new_fs ? new_fs : current->fs);
	if (IS_ERR(*new_nsp)) {
		err = PTR_ERR(*new_nsp);
		put_nsproxy(old_ns);
	}
	return err;
}
