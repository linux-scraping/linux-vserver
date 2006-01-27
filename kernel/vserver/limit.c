/*
 *  linux/kernel/vserver/limit.c
 *
 *  Virtual Server: Context Limits
 *
 *  Copyright (C) 2004-2005  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
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


static int is_valid_rlimit(int id)
{
	int valid = 0;

	switch (id) {
	case RLIMIT_RSS:
	case RLIMIT_NPROC:
	case RLIMIT_NOFILE:
	case RLIMIT_MEMLOCK:
	case RLIMIT_AS:

	case VLIMIT_NSOCK:
	case VLIMIT_OPENFD:
	case VLIMIT_ANON:
	case VLIMIT_SHMEM:
		valid = 1;
		break;
	}
	return valid;
}

static inline uint64_t vc_get_soft(struct vx_info *vxi, int id)
{
	rlim_t limit = vxi->limit.soft[id];
	return VX_VLIM(limit);
}

static inline uint64_t vc_get_hard(struct vx_info *vxi, int id)
{
	rlim_t limit = vxi->limit.hard[id];
	return VX_VLIM(limit);
}

int vc_get_rlimit(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_rlimit_v0 vc_data;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;
	if (!is_valid_rlimit(vc_data.id))
		return -ENOTSUPP;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	vc_data.minimum = CRLIM_UNSET;
	vc_data.softlimit = vc_get_soft(vxi, vc_data.id);
	vc_data.maximum = vc_get_hard(vxi, vc_data.id);
	put_vx_info(vxi);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_rlimit(uint32_t id, void __user *data)
{
	struct vx_info *vxi;
	struct vcmd_ctx_rlimit_v0 vc_data;

	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RESOURCE))
		return -EPERM;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;
	if (!is_valid_rlimit(vc_data.id))
		return -ENOTSUPP;

	vxi = lookup_vx_info(id);
	if (!vxi)
		return -ESRCH;

	if (vc_data.maximum != CRLIM_KEEP)
		vxi->limit.hard[vc_data.id] = VX_RLIM(vc_data.maximum);
	if (vc_data.softlimit != CRLIM_KEEP)
		vxi->limit.soft[vc_data.id] = VX_RLIM(vc_data.softlimit);
	put_vx_info(vxi);

	return 0;
}

int vc_get_rlimit_mask(uint32_t id, void __user *data)
{
	static struct vcmd_ctx_rlimit_mask_v0 mask = {
			/* minimum */
		0
		,	/* softlimit */
		(1 << RLIMIT_RSS) |
		(1 << VLIMIT_ANON) |
		0
		,	/* maximum */
		(1 << RLIMIT_RSS) |
		(1 << RLIMIT_NPROC) |
		(1 << RLIMIT_NOFILE) |
		(1 << RLIMIT_MEMLOCK) |
		(1 << RLIMIT_LOCKS) |
		(1 << RLIMIT_AS) |
		(1 << VLIMIT_ANON) |
		0
		};

	if (!capable(CAP_SYS_ADMIN) || !capable(CAP_SYS_RESOURCE))
		return -EPERM;
	if (copy_to_user(data, &mask, sizeof(mask)))
		return -EFAULT;
	return 0;
}


void vx_vsi_meminfo(struct sysinfo *val)
{
	struct vx_info *vxi = current->vx_info;
	rlim_t v;

	v = vxi->limit.soft[RLIMIT_RSS];
	if (v == RLIM_INFINITY)
		return;

	val->totalram = min((unsigned long)v,
		(unsigned long)val->totalram);

	v = __rlim_get(&vxi->limit, RLIMIT_RSS);
	val->freeram = (v < val->totalram) ? val->totalram - v : 0;

	val->bufferram = 0;
	val->totalhigh = 0;
	val->freehigh = 0;
	return;
}

void vx_vsi_swapinfo(struct sysinfo *val)
{
	struct vx_info *vxi = current->vx_info;
	rlim_t v, w;

	v = vxi->limit.soft[RLIMIT_RSS];
	if (v == RLIM_INFINITY)
		return;

	w = vxi->limit.hard[RLIMIT_RSS];
	if (w == RLIM_INFINITY)
		return;

	val->totalswap = min((unsigned long)(w - v),
		(unsigned long)val->totalswap);

	w = __rlim_get(&vxi->limit, RLIMIT_RSS) - v;
	val->freeswap = (w < val->totalswap) ? val->totalswap - w : 0;
	return;
}

