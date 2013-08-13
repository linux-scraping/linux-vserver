#ifndef _VX_CVIRT_PROC_H
#define _VX_CVIRT_PROC_H

#include <linux/nsproxy.h>
#include <linux/mnt_namespace.h>
#include <linux/ipc_namespace.h>
#include <linux/utsname.h>
#include <linux/ipc.h>

extern int vx_info_mnt_namespace(struct mnt_namespace *, char *);

static inline
int vx_info_proc_nsproxy(struct nsproxy *nsproxy, char *buffer)
{
	struct mnt_namespace *ns;
	struct uts_namespace *uts;
	struct ipc_namespace *ipc;
	int length = 0;

	if (!nsproxy)
		goto out;

	length += sprintf(buffer + length,
		"NSProxy:\t%p [%p,%p,%p]\n",
		nsproxy, nsproxy->mnt_ns,
		nsproxy->uts_ns, nsproxy->ipc_ns);

	ns = nsproxy->mnt_ns;
	if (!ns)
		goto skip_ns;

	length += vx_info_mnt_namespace(ns, buffer + length);

skip_ns:

	uts = nsproxy->uts_ns;
	if (!uts)
		goto skip_uts;

	length += sprintf(buffer + length,
		"SysName:\t%.*s\n"
		"NodeName:\t%.*s\n"
		"Release:\t%.*s\n"
		"Version:\t%.*s\n"
		"Machine:\t%.*s\n"
		"DomainName:\t%.*s\n",
		__NEW_UTS_LEN, uts->name.sysname,
		__NEW_UTS_LEN, uts->name.nodename,
		__NEW_UTS_LEN, uts->name.release,
		__NEW_UTS_LEN, uts->name.version,
		__NEW_UTS_LEN, uts->name.machine,
		__NEW_UTS_LEN, uts->name.domainname);
skip_uts:

	ipc = nsproxy->ipc_ns;
	if (!ipc)
		goto skip_ipc;

	length += sprintf(buffer + length,
		"SEMS:\t\t%d %d %d %d  %d\n"
		"MSG:\t\t%d %d %d\n"
		"SHM:\t\t%lu %lu  %d %ld\n",
		ipc->sem_ctls[0], ipc->sem_ctls[1],
		ipc->sem_ctls[2], ipc->sem_ctls[3],
		ipc->used_sems,
		ipc->msg_ctlmax, ipc->msg_ctlmnb, ipc->msg_ctlmni,
		(unsigned long)ipc->shm_ctlmax,
		(unsigned long)ipc->shm_ctlall,
		ipc->shm_ctlmni, ipc->shm_tot);
skip_ipc:
out:
	return length;
}


#include <linux/sched.h>

#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1 - 1)) * 100)

static inline
int vx_info_proc_cvirt(struct _vx_cvirt *cvirt, char *buffer)
{
	int length = 0;
	int a, b, c;

	length += sprintf(buffer + length,
		"BiasUptime:\t%lu.%02lu\n",
		(unsigned long)cvirt->bias_uptime.tv_sec,
		(cvirt->bias_uptime.tv_nsec / (NSEC_PER_SEC / 100)));

	a = cvirt->load[0] + (FIXED_1 / 200);
	b = cvirt->load[1] + (FIXED_1 / 200);
	c = cvirt->load[2] + (FIXED_1 / 200);
	length += sprintf(buffer + length,
		"nr_threads:\t%d\n"
		"nr_running:\t%d\n"
		"nr_unintr:\t%d\n"
		"nr_onhold:\t%d\n"
		"load_updates:\t%d\n"
		"loadavg:\t%d.%02d %d.%02d %d.%02d\n"
		"total_forks:\t%d\n",
		atomic_read(&cvirt->nr_threads),
		atomic_read(&cvirt->nr_running),
		atomic_read(&cvirt->nr_uninterruptible),
		atomic_read(&cvirt->nr_onhold),
		atomic_read(&cvirt->load_updates),
		LOAD_INT(a), LOAD_FRAC(a),
		LOAD_INT(b), LOAD_FRAC(b),
		LOAD_INT(c), LOAD_FRAC(c),
		atomic_read(&cvirt->total_forks));
	return length;
}

static inline
int vx_info_proc_cvirt_pc(struct _vx_cvirt_pc *cvirt_pc,
	char *buffer, int cpu)
{
	int length = 0;
	return length;
}

#endif	/* _VX_CVIRT_PROC_H */
