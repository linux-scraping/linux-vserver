#ifndef _VX_LIMIT_INT_H
#define _VX_LIMIT_INT_H


#ifdef	__KERNEL__

#define VXD_RLIMIT_COND(r)	VXD_CBIT(limit, (r))

extern const char *vlimit_name[NUM_LIMITS];

static inline void __vx_acc_cres(struct vx_info *vxi,
	int res, int dir, void *_data, char *_file, int _line)
{
	if (VXD_RLIMIT_COND(res))
		vxlprintk(1, "vx_acc_cres[%5d,%s,%2d]: %5ld%s (%p)",
			(vxi ? vxi->vx_id : -1), vlimit_name[res], res,
			(vxi ? (long)__rlim_get(&vxi->limit, res) : 0),
			(dir > 0) ? "++" : "--", _data, _file, _line);
	if (!vxi)
		return;

	if (dir > 0)
		__rlim_inc(&vxi->limit, res);
	else
		__rlim_dec(&vxi->limit, res);
}

static inline void __vx_add_cres(struct vx_info *vxi,
	int res, int amount, void *_data, char *_file, int _line)
{
	if (VXD_RLIMIT_COND(res))
		vxlprintk(1, "vx_add_cres[%5d,%s,%2d]: %5ld += %5d (%p)",
			(vxi ? vxi->vx_id : -1), vlimit_name[res], res,
			(vxi ? (long)__rlim_get(&vxi->limit, res) : 0),
			amount, _data, _file, _line);
	if (amount == 0)
		return;
	if (!vxi)
		return;
	__rlim_add(&vxi->limit, res, amount);
}

static inline int __vx_cres_avail(struct vx_info *vxi,
		int res, int num, char *_file, int _line)
{
	rlim_t value;

	if (VXD_RLIMIT_COND(res))
		vxlprintk(1, "vx_cres_avail[%5d,%s,%2d]: %5ld/%5ld > %5ld + %5d",
			(vxi ? vxi->vx_id : -1), vlimit_name[res], res,
			(vxi ? (long)vxi->limit.soft[res] : -1),
			(vxi ? (long)vxi->limit.hard[res] : -1),
			(vxi ? (long)__rlim_get(&vxi->limit, res) : 0),
			num, _file, _line);
	if (num == 0)
		return 1;
	if (!vxi)
		return 1;

	value = __rlim_get(&vxi->limit, res);

	if (value > vxi->limit.rmax[res])
		vxi->limit.rmax[res] = value;
	else if (value < vxi->limit.rmin[res])
		vxi->limit.rmin[res] = value;

	if (vxi->limit.soft[res] == RLIM_INFINITY)
		return -1;
	if (value + num <= vxi->limit.soft[res])
		return -1;

	if (vxi->limit.hard[res] == RLIM_INFINITY)
		return 1;
	if (value + num <= vxi->limit.hard[res])
		return 1;

	__rlim_hit(&vxi->limit, res);
	return 0;
}

#endif	/* __KERNEL__ */
#endif	/* _VX_LIMIT_INT_H */
