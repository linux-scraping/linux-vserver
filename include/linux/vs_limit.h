#ifndef _VS_LIMIT_H
#define _VS_LIMIT_H

#include "vserver/limit.h"
#include "vserver/base.h"
#include "vserver/context.h"
#include "vserver/debug.h"
#include "vserver/context.h"
#include "vserver/limit_int.h"


#define vx_acc_cres(v, d, p, r) \
	__vx_acc_cres(v, r, d, p, __FILE__, __LINE__)

#define vx_acc_cres_cond(x, d, p, r) \
	__vx_acc_cres(((x) == vx_current_xid()) ? current_vx_info() : 0, \
	r, d, p, __FILE__, __LINE__)


#define vx_add_cres(v, a, p, r) \
	__vx_add_cres(v, r, a, p, __FILE__, __LINE__)
#define vx_sub_cres(v, a, p, r)		vx_add_cres(v, -(a), p, r)

#define vx_add_cres_cond(x, a, p, r) \
	__vx_add_cres(((x) == vx_current_xid()) ? current_vx_info() : 0, \
	r, a, p, __FILE__, __LINE__)
#define vx_sub_cres_cond(x, a, p, r)	vx_add_cres_cond(x, -(a), p, r)


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
	vx_acc_cres(current_vx_info(), 1, (void *)(long)(f), VLIMIT_OPENFD)

#define vx_openfd_dec(f) \
	vx_acc_cres(current_vx_info(),-1, (void *)(long)(f), VLIMIT_OPENFD)


#define vx_cres_avail(v, n, r) \
	__vx_cres_avail(v, r, n, __FILE__, __LINE__)


#define vx_nproc_avail(n) \
	vx_cres_avail(current_vx_info(), n, RLIMIT_NPROC)

#define vx_files_avail(n) \
	vx_cres_avail(current_vx_info(), n, RLIMIT_NOFILE)

#define vx_locks_avail(n) \
	vx_cres_avail(current_vx_info(), n, RLIMIT_LOCKS)

#define vx_openfd_avail(n) \
	vx_cres_avail(current_vx_info(), n, VLIMIT_OPENFD)


/* dentry limits */

#define vx_dentry_inc(d) do {						\
	if (d_count(d) == 1)						\
		vx_acc_cres(current_vx_info(), 1, d, VLIMIT_DENTRY);	\
	} while (0)

#define vx_dentry_dec(d) do {						\
	if (d_count(d) == 0)						\
		vx_acc_cres(current_vx_info(),-1, d, VLIMIT_DENTRY);	\
	} while (0)

#define vx_dentry_avail(n) \
	vx_cres_avail(current_vx_info(), n, VLIMIT_DENTRY)


/* socket limits */

#define vx_sock_inc(s) \
	vx_acc_cres((s)->sk_vx_info, 1, s, VLIMIT_NSOCK)

#define vx_sock_dec(s) \
	vx_acc_cres((s)->sk_vx_info,-1, s, VLIMIT_NSOCK)

#define vx_sock_avail(n) \
	vx_cres_avail(current_vx_info(), n, VLIMIT_NSOCK)


/* ipc resource limits */

#define vx_ipcmsg_add(v, u, a) \
	vx_add_cres(v, a, u, RLIMIT_MSGQUEUE)

#define vx_ipcmsg_sub(v, u, a) \
	vx_sub_cres(v, a, u, RLIMIT_MSGQUEUE)

#define vx_ipcmsg_avail(v, a) \
	vx_cres_avail(v, a, RLIMIT_MSGQUEUE)


#define vx_ipcshm_add(v, k, a) \
	vx_add_cres(v, a, (void *)(long)(k), VLIMIT_SHMEM)

#define vx_ipcshm_sub(v, k, a) \
	vx_sub_cres(v, a, (void *)(long)(k), VLIMIT_SHMEM)

#define vx_ipcshm_avail(v, a) \
	vx_cres_avail(v, a, VLIMIT_SHMEM)


#define vx_semary_inc(a) \
	vx_acc_cres(current_vx_info(), 1, a, VLIMIT_SEMARY)

#define vx_semary_dec(a) \
	vx_acc_cres(current_vx_info(), -1, a, VLIMIT_SEMARY)


#define vx_nsems_add(a,n) \
	vx_add_cres(current_vx_info(), n, a, VLIMIT_NSEMS)

#define vx_nsems_sub(a,n) \
	vx_sub_cres(current_vx_info(), n, a, VLIMIT_NSEMS)


#else
#warning duplicate inclusion
#endif
