#ifndef _VX_VS_LIMIT_H
#define _VX_VS_LIMIT_H


#include "vserver/limit.h"
#include "vserver/debug.h"


#define vx_acc_cres(v,d,p,r) \
	__vx_acc_cres(v, r, d, p, __FILE__, __LINE__)

#define vx_acc_cres_cond(x,d,p,r) \
	__vx_acc_cres(((x) == vx_current_xid()) ? current->vx_info : 0, \
	r, d, p, __FILE__, __LINE__)


static inline void __vx_acc_cres(struct vx_info *vxi,
	int res, int dir, void *_data, char *_file, int _line)
{
	vxlprintk(VXD_RLIMIT_COND(res),
		"vx_acc_cres[%5d,%s,%2d]: %5d%s (%p)",
		(vxi?vxi->vx_id:-1), vlimit_name[res], res,
		(vxi?atomic_read(&vxi->limit.rcur[res]):0),
		(dir>0)?"++":"--", _data, _file, _line);
	if (vxi) {
		if (dir > 0)
			atomic_inc(&vxi->limit.rcur[res]);
		else
			atomic_dec(&vxi->limit.rcur[res]);
	}
}

#define vx_add_cres(v,a,p,r) \
	__vx_add_cres(v, r, a, p, __FILE__, __LINE__)
#define vx_sub_cres(v,a,p,r)		vx_add_cres(v,-(a),p,r)

#define vx_add_cres_cond(x,a,p,r) \
	__vx_add_cres(((x) == vx_current_xid()) ? current->vx_info : 0, \
	r, a, p, __FILE__, __LINE__)
#define vx_sub_cres_cond(x,a,p,r)	vx_add_cres_cond(x,-(a),p,r)


static inline void __vx_add_cres(struct vx_info *vxi,
	int res, int amount, void *_data, char *_file, int _line)
{
	vxlprintk(VXD_RLIMIT_COND(res),
		"vx_add_cres[%5d,%s,%2d]: %5d += %5d (%p)",
		(vxi?vxi->vx_id:-1), vlimit_name[res], res,
		(vxi?atomic_read(&vxi->limit.rcur[res]):0),
		amount, _data, _file, _line);
	if (amount == 0)
		return;
	if (vxi)
		atomic_add(amount, &vxi->limit.rcur[res]);
}


/* process and file limits */

#define vx_nproc_inc(p) \
	vx_acc_cres((p)->vx_info, 1, p, RLIMIT_NPROC)

#define vx_nproc_dec(p) \
	vx_acc_cres((p)->vx_info,-1, p, RLIMIT_NPROC)

#define vx_files_inc(f) \
	vx_acc_cres_cond((f)->f_xid, 1, f, RLIMIT_NOFILE)

#define vx_files_dec(f) \
	vx_acc_cres_cond((f)->f_xid,-1, f, RLIMIT_NOFILE)

#define vx_locks_inc(l) \
	vx_acc_cres_cond((l)->fl_xid, 1, l, RLIMIT_LOCKS)

#define vx_locks_dec(l) \
	vx_acc_cres_cond((l)->fl_xid,-1, l, RLIMIT_LOCKS)

#define vx_openfd_inc(f) \
	vx_acc_cres(current->vx_info, 1, (void *)(long)(f), VLIMIT_OPENFD)

#define vx_openfd_dec(f) \
	vx_acc_cres(current->vx_info,-1, (void *)(long)(f), VLIMIT_OPENFD)

#define vx_cres_avail(v,n,r) \
	__vx_cres_avail(v, r, n, __FILE__, __LINE__)

static inline int __vx_cres_avail(struct vx_info *vxi,
		int res, int num, char *_file, int _line)
{
	unsigned long value;

	vxlprintk(VXD_RLIMIT_COND(res),
		"vx_cres_avail[%5d,%s,%2d]: %5ld > %5d + %5d",
		(vxi?vxi->vx_id:-1), vlimit_name[res], res,
		(vxi?vxi->limit.rlim[res]:1),
		(vxi?atomic_read(&vxi->limit.rcur[res]):0),
		num, _file, _line);
	if (!vxi)
		return 1;
	value = atomic_read(&vxi->limit.rcur[res]);
	if (value > vxi->limit.rmax[res])
		vxi->limit.rmax[res] = value;
	if (vxi->limit.rlim[res] == RLIM_INFINITY)
		return 1;
	if (value + num <= vxi->limit.rlim[res])
		return 1;
	atomic_inc(&vxi->limit.lhit[res]);
	return 0;
}

#define vx_nproc_avail(n) \
	vx_cres_avail(current->vx_info, n, RLIMIT_NPROC)

#define vx_files_avail(n) \
	vx_cres_avail(current->vx_info, n, RLIMIT_NOFILE)

#define vx_locks_avail(n) \
	vx_cres_avail(current->vx_info, n, RLIMIT_LOCKS)

#define vx_openfd_avail(n) \
	vx_cres_avail(current->vx_info, n, VLIMIT_OPENFD)


/* socket limits */

#define vx_sock_inc(s) \
	vx_acc_cres((s)->sk_vx_info, 1, s, VLIMIT_NSOCK)

#define vx_sock_dec(s) \
	vx_acc_cres((s)->sk_vx_info,-1, s, VLIMIT_NSOCK)

#define vx_sock_avail(n) \
	vx_cres_avail(current->vx_info, n, VLIMIT_NSOCK)


/* ipc resource limits */

#define vx_ipcmsg_add(v,u,a) \
	vx_add_cres(v, a, u, RLIMIT_MSGQUEUE)

#define vx_ipcmsg_sub(v,u,a) \
	vx_sub_cres(v, a, u, RLIMIT_MSGQUEUE)

#define vx_ipcmsg_avail(v,a) \
	vx_cres_avail(v, a, RLIMIT_MSGQUEUE)


#define vx_ipcshm_add(v,k,a) \
	vx_add_cres(v, a, (void *)(long)(k), VLIMIT_SHMEM)

#define vx_ipcshm_sub(v,k,a) \
	vx_sub_cres(v, a, (void *)(long)(k), VLIMIT_SHMEM)

#define vx_ipcshm_avail(v,a) \
	vx_cres_avail(v, a, VLIMIT_SHMEM)


#define vx_semary_inc(a) \
	vx_acc_cres(current->vx_info, 1, a, VLIMIT_SEMARY)

#define vx_semary_dec(a) \
	vx_acc_cres(current->vx_info,-1, a, VLIMIT_SEMARY)


#define vx_nsems_add(a,n) \
	vx_add_cres(current->vx_info, n, a, VLIMIT_NSEMS)

#define vx_nsems_sub(a,n) \
	vx_sub_cres(current->vx_info, n, a, VLIMIT_NSEMS)


#else
#warning duplicate inclusion
#endif
