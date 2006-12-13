/*
 *  linux/kernel/vserver/limit.c
 *
 *  Virtual Server: Context Limits
 *
 *  Copyright (C) 2004-2006  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *  V0.02  sync to valid limit check from 2.1.1
 *
 */

#include <linux/module.h>
#include <linux/vs_context.h>
#include <linux/vs_limit.h>
#include <linux/vserver/limit.h>
#include <linux/vserver/switch.h>
#include <linux/vserver/limit_cmd.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


const char *vlimit_name[NUM_LIMITS] = {
	[RLIMIT_CPU]		= "CPU",
	[RLIMIT_RSS]		= "RSS",
	[RLIMIT_NPROC]		= "NPROC",
	[RLIMIT_NOFILE]		= "NOFILE",
	[RLIMIT_MEMLOCK]	= "VML",
	[RLIMIT_AS]		= "VM",
	[RLIMIT_LOCKS]		= "LOCKS",
	[RLIMIT_SIGPENDING]	= "SIGP",
	[RLIMIT_MSGQUEUE]	= "MSGQ",

	[VLIMIT_NSOCK]		= "NSOCK",
	[VLIMIT_OPENFD]		= "OPENFD",
	[VLIMIT_ANON]		= "ANON",
	[VLIMIT_SHMEM]		= "SHMEM",
};

EXPORT_SYMBOL_GPL(vlimit_name);


#define MASK_ENTRY(x)	(1 << (x))

const struct vcmd_ctx_rlimit_mask_v0 vlimit_mask = {
		/* minimum */
	0
	,	/* softlimit */
	0
	,       /* maximum */
	MASK_ENTRY( RLIMIT_RSS		) |
	MASK_ENTRY( RLIMIT_NPROC	) |
	MASK_ENTRY( RLIMIT_NOFILE	) |
	MASK_ENTRY( RLIMIT_MEMLOCK	) |
	MASK_ENTRY( RLIMIT_AS		) |
	MASK_ENTRY( RLIMIT_LOCKS	) |
	MASK_ENTRY( RLIMIT_MSGQUEUE	) |

	MASK_ENTRY( VLIMIT_ANON		) |
	MASK_ENTRY( VLIMIT_SHMEM	) |
	0
};


static int is_valid_vlimit(int id)
{
	uint32_t mask = vlimit_mask.minimum |
		vlimit_mask.softlimit | vlimit_mask.maximum;
	return mask & (1 << id);
}

static inline uint64_t vc_get_rlim(struct vx_info *vxi, int id)
{
	unsigned long limit;

	limit = vxi->limit.rlim[id];
	if (limit == RLIM_INFINITY)
		return CRLIM_INFINITY;
	return limit;
}

static int do_get_rlimit(xid_t xid, uint32_t id,
	uint64_t *minimum, uint64_t *softlimit, uint64_t *maximum)
{
	struct vx_info *vxi;

	if (!is_valid_vlimit(id))
		return -EINVAL;

	vxi = lookup_vx_info(xid);
	if (!vxi)
		return -ESRCH;

	if (minimum)
		*minimum = CRLIM_UNSET;
	if (softlimit)
		*softlimit = CRLIM_UNSET;
	if (maximum)
		*maximum = vc_get_rlim(vxi, id);
	put_vx_info(vxi);
	return 0;
}

int vc_get_rlimit(uint32_t id, void __user *data)
{
	struct vcmd_ctx_rlimit_v0 vc_data;
	int ret;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_rlimit(id, vc_data.id,
		&vc_data.minimum, &vc_data.softlimit, &vc_data.maximum);
	if (ret)
		return ret;

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

static int do_set_rlimit(xid_t xid, uint32_t id,
	uint64_t minimum, uint64_t softlimit, uint64_t maximum)
{
	struct vx_info *vxi;

	if (!is_valid_vlimit(id))
		return -EINVAL;

	vxi = lookup_vx_info(xid);
	if (!vxi)
		return -ESRCH;

	if (maximum != CRLIM_KEEP)
		vxi->limit.rlim[id] = maximum;

	put_vx_info(vxi);
	return 0;
}

int vc_set_rlimit(uint32_t id, void __user *data)
{
	struct vcmd_ctx_rlimit_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RESOURCE))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_rlimit(id, vc_data.id,
		vc_data.minimum, vc_data.softlimit, vc_data.maximum);
}

#ifdef	CONFIG_IA32_EMULATION

int vc_set_rlimit_x32(uint32_t id, void __user *data)
{
	struct vcmd_ctx_rlimit_v0_x32 vc_data;

	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RESOURCE))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_rlimit(id, vc_data.id,
		vc_data.minimum, vc_data.softlimit, vc_data.maximum);
}

int vc_get_rlimit_x32(uint32_t id, void __user *data)
{
	struct vcmd_ctx_rlimit_v0_x32 vc_data;
	int ret;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_rlimit(id, vc_data.id,
		&vc_data.minimum, &vc_data.softlimit, &vc_data.maximum);
	if (ret)
		return ret;

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

#endif	/* CONFIG_IA32_EMULATION */


int vc_get_rlimit_mask(uint32_t id, void __user *data)
{
	if (copy_to_user(data, &vlimit_mask, sizeof(vlimit_mask)))
		return -EFAULT;
	return 0;
}


void vx_vsi_meminfo(struct sysinfo *val)
{
	struct vx_info *vxi = current->vx_info;
	unsigned long v;

	v = vxi->limit.rlim[RLIMIT_RSS];
	if (v != RLIM_INFINITY)
		val->totalram = min(val->totalram, v);
	v = atomic_read(&vxi->limit.rcur[RLIMIT_RSS]);
	val->freeram = (v < val->totalram) ? val->totalram - v : 0;
	val->bufferram = 0;
	val->totalhigh = 0;
	val->freehigh = 0;
	return;
}

void vx_vsi_swapinfo(struct sysinfo *val)
{
	struct vx_info *vxi = current->vx_info;
	unsigned long v, w;

	v = vxi->limit.rlim[RLIMIT_RSS];
	w = vxi->limit.rlim[RLIMIT_AS];
	if (w != RLIM_INFINITY)
		val->totalswap = min(val->totalswap, w -
		((v != RLIM_INFINITY) ? v : 0));
	w = atomic_read(&vxi->limit.rcur[RLIMIT_AS]);
	val->freeswap = (w < val->totalswap) ? val->totalswap - w : 0;
	return;
}

