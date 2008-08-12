/*
 * linux/fs/nfsd/auth.c
 *
 * Copyright (C) 1995, 1996 Olaf Kirch <okir@monad.swb.de>
 */

#include <linux/types.h>
#include <linux/sched.h>
#include <linux/sunrpc/svc.h>
#include <linux/sunrpc/svcauth.h>
#include <linux/nfsd/nfsd.h>
#include <linux/nfsd/export.h>
#include <linux/vs_tag.h>
#include "auth.h"

int nfsexp_flags(struct svc_rqst *rqstp, struct svc_export *exp)
{
	struct exp_flavor_info *f;
	struct exp_flavor_info *end = exp->ex_flavors + exp->ex_nflavors;

	for (f = exp->ex_flavors; f < end; f++) {
		if (f->pseudoflavor == rqstp->rq_flavor)
			return f->flags;
	}
	return exp->ex_flags;

}

int nfsd_setuser(struct svc_rqst *rqstp, struct svc_export *exp)
{
	struct svc_cred	cred = rqstp->rq_cred;
	int i;
	int flags = nfsexp_flags(rqstp, exp);
	int ret;

	if (flags & NFSEXP_ALLSQUASH) {
		cred.cr_uid = exp->ex_anon_uid;
		cred.cr_gid = exp->ex_anon_gid;
		cred.cr_group_info = groups_alloc(0);
	} else if (flags & NFSEXP_ROOTSQUASH) {
		struct group_info *gi;
		if (!cred.cr_uid)
			cred.cr_uid = exp->ex_anon_uid;
		if (!cred.cr_gid)
			cred.cr_gid = exp->ex_anon_gid;
		gi = groups_alloc(cred.cr_group_info->ngroups);
		if (gi)
			for (i = 0; i < cred.cr_group_info->ngroups; i++) {
				if (!GROUP_AT(cred.cr_group_info, i))
					GROUP_AT(gi, i) = exp->ex_anon_gid;
				else
					GROUP_AT(gi, i) = GROUP_AT(cred.cr_group_info, i);
			}
		cred.cr_group_info = gi;
	} else
		get_group_info(cred.cr_group_info);

	if (cred.cr_uid != (uid_t) -1)
		current->fsuid = INOTAG_UID(DX_TAG_NFSD, cred.cr_uid, cred.cr_gid);
	else
		current->fsuid = exp->ex_anon_uid;
	if (cred.cr_gid != (gid_t) -1)
		current->fsgid = INOTAG_GID(DX_TAG_NFSD, cred.cr_uid, cred.cr_gid);
	else
		current->fsgid = exp->ex_anon_gid;

	/* this desperately needs a tag :) */
	current->xid = (xid_t)INOTAG_TAG(DX_TAG_NFSD, cred.cr_uid, cred.cr_gid, 0);

	if (!cred.cr_group_info)
		return -ENOMEM;
	ret = set_current_groups(cred.cr_group_info);
	put_group_info(cred.cr_group_info);

	if (INOTAG_UID(DX_TAG_NFSD, cred.cr_uid, cred.cr_gid)) {
		current->cap_effective =
			cap_drop_nfsd_set(current->cap_effective);
	} else {
		current->cap_effective =
			cap_raise_nfsd_set(current->cap_effective,
					   current->cap_permitted);
	}
	return ret;
}
