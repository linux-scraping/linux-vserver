/*
 *  linux/kernel/vserver/limit.c
 *
 *  Virtual Server: Context Limits
 *
 *  Copyright (C) 2004-2007  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *  V0.02  changed vcmds to vxi arg
 *
 */

#include <linux/sched.h>
#include <linux/module.h>
#include <linux/memcontrol.h>
#include <linux/res_counter.h>
#include <linux/vs_limit.h>
#include <linux/vserver/limit.h>
#include <linux/vserver/limit_cmd.h>

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

#define MASK_ENTRY(x)	(1 << (x))

const struct vcmd_ctx_rlimit_mask_v0 vlimit_mask = {
		/* minimum */
	0
	,	/* softlimit */
	MASK_ENTRY( RLIMIT_RSS		) |
	MASK_ENTRY( VLIMIT_ANON		) |
	0
	,       /* maximum */
	MASK_ENTRY( RLIMIT_RSS		) |
	MASK_ENTRY( RLIMIT_NPROC	) |
	MASK_ENTRY( RLIMIT_NOFILE	) |
	MASK_ENTRY( RLIMIT_MEMLOCK	) |
	MASK_ENTRY( RLIMIT_AS		) |
	MASK_ENTRY( RLIMIT_LOCKS	) |
	MASK_ENTRY( RLIMIT_MSGQUEUE	) |

	MASK_ENTRY( VLIMIT_NSOCK	) |
	MASK_ENTRY( VLIMIT_OPENFD	) |
	MASK_ENTRY( VLIMIT_ANON		) |
	MASK_ENTRY( VLIMIT_SHMEM	) |
	MASK_ENTRY( VLIMIT_DENTRY	) |
	0
};
		/* accounting only */
uint32_t account_mask =
	MASK_ENTRY( VLIMIT_SEMARY	) |
	MASK_ENTRY( VLIMIT_NSEMS	) |
	MASK_ENTRY( VLIMIT_MAPPED	) |
	0;


static int is_valid_vlimit(int id)
{
	uint32_t mask = vlimit_mask.minimum |
		vlimit_mask.softlimit | vlimit_mask.maximum;
	return mask & (1 << id);
}

static int is_accounted_vlimit(int id)
{
	if (is_valid_vlimit(id))
		return 1;
	return account_mask & (1 << id);
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
	if (!is_valid_vlimit(id))
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

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_rlimit(vxi, vc_data.id,
		&vc_data.minimum, &vc_data.softlimit, &vc_data.maximum);
	if (ret)
		return ret;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

static int do_set_rlimit(struct vx_info *vxi, uint32_t id,
	uint64_t minimum, uint64_t softlimit, uint64_t maximum)
{
	if (!is_valid_vlimit(id))
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

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_rlimit(vxi, vc_data.id,
		vc_data.minimum, vc_data.softlimit, vc_data.maximum);
}

#ifdef	CONFIG_IA32_EMULATION

int vc_set_rlimit_x32(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_rlimit_v0_x32 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_set_rlimit(vxi, vc_data.id,
		vc_data.minimum, vc_data.softlimit, vc_data.maximum);
}

int vc_get_rlimit_x32(struct vx_info *vxi, void __user *data)
{
	struct vcmd_ctx_rlimit_v0_x32 vc_data;
	int ret;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	ret = do_get_rlimit(vxi, vc_data.id,
		&vc_data.minimum, &vc_data.softlimit, &vc_data.maximum);
	if (ret)
		return ret;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
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


static inline void vx_reset_hits(struct _vx_limit *limit)
{
	int lim;

	for (lim = 0; lim < NUM_LIMITS; lim++) {
		atomic_set(&__rlim_lhit(limit, lim), 0);
	}
}

int vc_reset_hits(struct vx_info *vxi, void __user *data)
{
	vx_reset_hits(&vxi->limit);
	return 0;
}

static inline void vx_reset_minmax(struct _vx_limit *limit)
{
	rlim_t value;
	int lim;

	for (lim = 0; lim < NUM_LIMITS; lim++) {
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


int vc_rlimit_stat(struct vx_info *vxi, void __user *data)
{
	struct vcmd_rlimit_stat_v0 vc_data;
	struct _vx_limit *limit = &vxi->limit;
	int id;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	id = vc_data.id;
	if (!is_accounted_vlimit(id))
		return -EINVAL;

	vx_limit_fixup(limit, id);
	vc_data.hits = atomic_read(&__rlim_lhit(limit, id));
	vc_data.value = __rlim_get(limit, id);
	vc_data.minimum = __rlim_rmin(limit, id);
	vc_data.maximum = __rlim_rmax(limit, id);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


void vx_vsi_meminfo(struct sysinfo *val)
{
#if 0
	struct vx_info *vxi = current_vx_info();
	unsigned long totalram, freeram;
	rlim_t v;

	/* we blindly accept the max */
	v = __rlim_soft(&vxi->limit, RLIMIT_RSS);
	totalram = (v != RLIM_INFINITY) ? v : val->totalram;

	/* total minus used equals free */
	v = __vx_cres_array_fixup(&vxi->limit, VLA_RSS);
	freeram = (v < totalram) ? totalram - v : 0;

	val->totalram = totalram;
	val->freeram = freeram;
#else
	struct mem_cgroup *mcg = mem_cgroup_from_task(current);
	u64 res_limit, res_usage;

	if (!mcg)
		return;

	res_limit = mem_cgroup_res_read_u64(mcg, RES_LIMIT);
	res_usage = mem_cgroup_res_read_u64(mcg, RES_USAGE);

	if (res_limit != RESOURCE_MAX)
		val->totalram = (res_limit >> PAGE_SHIFT);
	val->freeram = val->totalram - (res_usage >> PAGE_SHIFT);
	val->bufferram = 0;
#endif
	val->totalhigh = 0;
	val->freehigh = 0;
	return;
}

void vx_vsi_swapinfo(struct sysinfo *val)
{
#if 0
	struct vx_info *vxi = current_vx_info();
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
	w = __vx_cres_array_fixup(&vxi->limit, VLA_RSS);
	w -= (w > v) ? v : w;

	/* total minus used equals free */
	freeswap = (w < totalswap) ? totalswap - w : 0;

	val->totalswap = totalswap;
	val->freeswap = freeswap;
#else
	struct mem_cgroup *mcg = mem_cgroup_from_task(current);
	u64 res_limit, res_usage, memsw_limit, memsw_usage;
	s64 swap_limit, swap_usage;

	if (!mcg)
		return;

	res_limit = mem_cgroup_res_read_u64(mcg, RES_LIMIT);
	res_usage = mem_cgroup_res_read_u64(mcg, RES_USAGE);
	memsw_limit = mem_cgroup_memsw_read_u64(mcg, RES_LIMIT);
	memsw_usage = mem_cgroup_memsw_read_u64(mcg, RES_USAGE);

	if (res_limit == RESOURCE_MAX)
		return;

	swap_limit = memsw_limit - res_limit;
	if (memsw_limit != RESOURCE_MAX)
		val->totalswap = swap_limit >> PAGE_SHIFT;

	swap_usage = memsw_usage - res_usage;
	val->freeswap = (swap_usage < swap_limit) ?
		val->totalswap - (swap_usage >> PAGE_SHIFT) : 0;
#endif
	return;
}

long vx_vsi_cached(struct sysinfo *val)
{
	struct mem_cgroup *mcg = mem_cgroup_from_task(current);

	return mem_cgroup_stat_read_cache(mcg);
}


unsigned long vx_badness(struct task_struct *task, struct mm_struct *mm)
{
	struct vx_info *vxi = mm->mm_vx_info;
	unsigned long points;
	rlim_t v, w;

	if (!vxi)
		return 0;

	points = vxi->vx_badness_bias;

	v = __vx_cres_array_fixup(&vxi->limit, VLA_RSS);
	w = __rlim_soft(&vxi->limit, RLIMIT_RSS);
	points += (v > w) ? (v - w) : 0;

	return points;
}

