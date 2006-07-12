/*
 *  linux/kernel/vserver/limit.c
 *
 *  Virtual Server: Context Limits
 *
 *  Copyright (C) 2004-2006  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *  V0.02  changed vcmds to vxi arg
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
	[VLIMIT_DENTRY]		= "DENTRY",
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
	case RLIMIT_LOCKS:
	case RLIMIT_MSGQUEUE:

	case VLIMIT_NSOCK:
	case VLIMIT_OPENFD:
	case VLIMIT_ANON:
	case VLIMIT_SHMEM:
	case VLIMIT_DENTRY:
		valid = 1;
		break;
	}
	return valid;
}

static inline uint64_t vc_get_soft(struct vx_info *vxi, int id)
{
	rlim_t limit = __rlim_soft(&vxi->limit, id);
	return VX_VLIM(limit);
}

static inline uint64_t vc_get_hard(struct vx_info *vxi, int id)
{
	rlim_t limit = __rlim_hard(&vxi->limit, id);
	return VX_VLIM(limit);
}

static int do_get_rlimit(struct vx_info *vxi, uint32_t id,
	uint64_t *minimum, uint64_t *softlimit, uint64_t *maximum)
{
	if (!is_valid_rlimit(id))
		return -EINVAL;

	if (minimum)
		*minimum = CRLIM_UNSET;
	if (softlimit)
		*softlimit = vc_get_soft(vxi, id);
	if (maximum)
		*maximum = vc_get_hard(vxi, id);
	return 0;
}

int vc_get_rlimit(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_rlimit_v0 vc_data;
	int ret;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_rlimit(vxi, vc_data.id,
		&vc_data.minimum, &vc_data.softlimit, &vc_data.maximum);
	if (ret)
		return ret;

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

static int do_set_rlimit(struct vx_info *vxi, uint32_t id,
	uint64_t minimum, uint64_t softlimit, uint64_t maximum)
{
	if (!is_valid_rlimit(id))
		return -EINVAL;

	if (maximum != CRLIM_KEEP)
		__rlim_hard(&vxi->limit, id) = VX_RLIM(maximum);
	if (softlimit != CRLIM_KEEP)
		__rlim_soft(&vxi->limit, id) = VX_RLIM(softlimit);

	/* clamp soft limit */
	if (__rlim_soft(&vxi->limit, id) > __rlim_hard(&vxi->limit, id))
		__rlim_soft(&vxi->limit, id) = __rlim_hard(&vxi->limit, id);

	return 0;
}

int vc_set_rlimit(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_rlimit_v0 vc_data;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_rlimit(vxi, vc_data.id,
		vc_data.minimum, vc_data.softlimit, vc_data.maximum);
}

#ifdef	CONFIG_IA32_EMULATION

int vc_set_rlimit_x32(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_rlimit_v0_x32 vc_data;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_rlimit(vxi, vc_data.id,
		vc_data.minimum, vc_data.softlimit, vc_data.maximum);
}

int vc_get_rlimit_x32(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_rlimit_v0_x32 vc_data;
	int ret;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_rlimit(vxi, vc_data.id,
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
		(1 << VLIMIT_DENTRY) |
		0
		};

	if (copy_to_user(data, &mask, sizeof(mask)))
		return -EFAULT;
	return 0;
}


static inline void vx_reset_minmax(struct _vx_limit *limit)
{
	rlim_t value;
	int lim;

	for (lim=0; lim<NUM_LIMITS; lim++) {
		value = __rlim_get(limit, lim);
		__rlim_rmax(limit, lim) = value;
		__rlim_rmin(limit, lim) = value;
	}
}


int vc_reset_minmax(struct vx_info *vxi, void __user *data)
{
	vx_reset_minmax(&vxi->limit);
	return 0;
}


void vx_vsi_meminfo(struct sysinfo *val)
{
	struct vx_info *vxi = current->vx_info;
	unsigned long totalram, freeram;
	rlim_t v;

	/* we blindly accept the max */
	v = __rlim_soft(&vxi->limit, RLIMIT_RSS);
	totalram = (v != RLIM_INFINITY) ? v : val->totalram;

	/* total minus used equals free */
	v = __rlim_get(&vxi->limit, RLIMIT_RSS);
	freeram = (v < totalram) ? totalram - v : 0;

	val->totalram = totalram;
	val->freeram = freeram;
	val->bufferram = 0;
	val->totalhigh = 0;
	val->freehigh = 0;
	return;
}

void vx_vsi_swapinfo(struct sysinfo *val)
{
	struct vx_info *vxi = current->vx_info;
	unsigned long totalswap, freeswap;
	rlim_t v, w;

	v = __rlim_soft(&vxi->limit, RLIMIT_RSS);
	if (v == RLIM_INFINITY) {
		val->freeswap = val->totalswap;
		return;
	}

	/* we blindly accept the max */
	w = __rlim_hard(&vxi->limit, RLIMIT_RSS);
	totalswap = (w != RLIM_INFINITY) ? (w - v) : val->totalswap;

	/* currently 'used' swap */
	w = __rlim_get(&vxi->limit, RLIMIT_RSS);
	w -= (w > v) ? v : w;

	/* total minus used equals free */
	freeswap = (w < totalswap) ? totalswap - w : 0;

	val->totalswap = totalswap;
	val->freeswap = freeswap;
	return;
}

