#ifndef _VSERVER_LIMIT_INT_H
#define _VSERVER_LIMIT_INT_H

#define VXD_RCRES_COND(r)	VXD_CBIT(cres, r)
#define VXD_RLIMIT_COND(r)	VXD_CBIT(limit, r)

extern const char *vlimit_name[NUM_LIMITS];

static inline void __vx_acc_cres(struct vx_info *vxi,
	int res, int dir, void *_data, char *_file, int _line)
{
	if (VXD_RCRES_COND(res))
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
	if (VXD_RCRES_COND(res))
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

static inline
int __vx_cres_adjust_max(struct _vx_limit *limit, int res, rlim_t value)
{
	int cond = (value > __rlim_rmax(limit, res));

	if (cond)
		__rlim_rmax(limit, res) = value;
	return cond;
}

static inline
int __vx_cres_adjust_min(struct _vx_limit *limit, int res, rlim_t value)
{
	int cond = (value < __rlim_rmin(limit, res));

	if (cond)
		__rlim_rmin(limit, res) = value;
	return cond;
}

static inline
void __vx_cres_fixup(struct _vx_limit *limit, int res, rlim_t value)
{
	if (!__vx_cres_adjust_max(limit, res, value))
		__vx_cres_adjust_min(limit, res, value);
}


/*	return values:
	 +1 ... no limit hit
	 -1 ... over soft limit
	  0 ... over hard limit		*/

static inline int __vx_cres_avail(struct vx_info *vxi,
	int res, int num, char *_file, int _line)
{
	struct _vx_limit *limit;
	rlim_t value;

	if (VXD_RLIMIT_COND(res))
		vxlprintk(1, "vx_cres_avail[%5d,%s,%2d]: %5ld/%5ld > %5ld + %5d",
			(vxi ? vxi->vx_id : -1), vlimit_name[res], res,
			(vxi ? (long)__rlim_soft(&vxi->limit, res) : -1),
			(vxi ? (long)__rlim_hard(&vxi->limit, res) : -1),
			(vxi ? (long)__rlim_get(&vxi->limit, res) : 0),
			num, _file, _line);
	if (!vxi)
		return 1;

	limit = &vxi->limit;
	value = __rlim_get(limit, res);

	if (!__vx_cres_adjust_max(limit, res, value))
		__vx_cres_adjust_min(limit, res, value);

	if (num == 0)
		return 1;

	if (__rlim_soft(limit, res) == RLIM_INFINITY)
		return -1;
	if (value + num <= __rlim_soft(limit, res))
		return -1;

	if (__rlim_hard(limit, res) == RLIM_INFINITY)
		return 1;
	if (value + num <= __rlim_hard(limit, res))
		return 1;

	__rlim_hit(limit, res);
	return 0;
}


static const int VLA_RSS[] = { RLIMIT_RSS, VLIMIT_ANON, VLIMIT_MAPPED, 0 };

static inline
rlim_t __vx_cres_array_sum(struct _vx_limit *limit, const int *array)
{
	rlim_t value, sum = 0;
	int res;

	while ((res = *array++)) {
		value = __rlim_get(limit, res);
		__vx_cres_fixup(limit, res, value);
		sum += value;
	}
	return sum;
}

static inline
rlim_t __vx_cres_array_fixup(struct _vx_limit *limit, const int *array)
{
	rlim_t value = __vx_cres_array_sum(limit, array + 1);
	int res = *array;

	if (value == __rlim_get(limit, res))
		return value;

	__rlim_set(limit, res, value);
	/* now adjust min/max */
	if (!__vx_cres_adjust_max(limit, res, value))
		__vx_cres_adjust_min(limit, res, value);

	return value;
}

static inline int __vx_cres_array_avail(struct vx_info *vxi,
	const int *array, int num, char *_file, int _line)
{
	struct _vx_limit *limit;
	rlim_t value = 0;
	int res;

	if (num == 0)
		return 1;
	if (!vxi)
		return 1;

	limit = &vxi->limit;
	res = *array;
	value = __vx_cres_array_sum(limit, array + 1);

	__rlim_set(limit, res, value);
	__vx_cres_fixup(limit, res, value);

	return __vx_cres_avail(vxi, res, num, _file, _line);
}


static inline void vx_limit_fixup(struct _vx_limit *limit, int id)
{
	rlim_t value;
	int res;

	/* complex resources first */
	if ((id < 0) || (id == RLIMIT_RSS))
		__vx_cres_array_fixup(limit, VLA_RSS);

	for (res = 0; res < NUM_LIMITS; res++) {
		if ((id > 0) && (res != id))
			continue;

		value = __rlim_get(limit, res);
		__vx_cres_fixup(limit, res, value);

		/* not supposed to happen, maybe warn? */
		if (__rlim_rmax(limit, res) > __rlim_hard(limit, res))
			__rlim_rmax(limit, res) = __rlim_hard(limit, res);
	}
}


#endif	/* _VSERVER_LIMIT_INT_H */
