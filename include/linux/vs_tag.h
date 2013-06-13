#ifndef _VS_TAG_H
#define _VS_TAG_H

#include <linux/vserver/tag.h>

/* check conditions */

#define DX_ADMIN	0x0001
#define DX_WATCH	0x0002
#define DX_HOSTID	0x0008

#define DX_IDENT	0x0010

#define DX_ARG_MASK	0x0010


#define dx_task_tag(t)	((t)->tag)

#define dx_current_tag() dx_task_tag(current)

#define dx_check(c, m)	__dx_check(dx_current_tag(), c, m)

#define dx_weak_check(c, m)	((m) ? dx_check(c, m) : 1)


/*
 * check current context for ADMIN/WATCH and
 * optionally against supplied argument
 */
static inline int __dx_check(vtag_t cid, vtag_t id, unsigned int mode)
{
	if (mode & DX_ARG_MASK) {
		if ((mode & DX_IDENT) && (id == cid))
			return 1;
	}
	return (((mode & DX_ADMIN) && (cid == 0)) ||
		((mode & DX_WATCH) && (cid == 1)) ||
		((mode & DX_HOSTID) && (id == 0)));
}

struct inode;
int dx_permission(const struct inode *inode, int mask);


#else
#warning duplicate inclusion
#endif
