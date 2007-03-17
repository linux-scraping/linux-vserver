#ifndef _VX_CVIRT_PROC_H
#define _VX_CVIRT_PROC_H

#include <linux/sched.h>


#define LOAD_INT(x) ((x) >> FSHIFT)
#define LOAD_FRAC(x) LOAD_INT(((x) & (FIXED_1-1)) * 100)

static inline int vx_info_proc_cvirt(struct _vx_cvirt *cvirt, char *buffer)
{
	int length = 0;
	int a, b, c;

	length += sprintf(buffer + length,
		"BiasUptime:\t%lu.%02lu\n",
			(unsigned long)cvirt->bias_uptime.tv_sec,
			(cvirt->bias_uptime.tv_nsec / (NSEC_PER_SEC / 100)));
	length += sprintf(buffer + length,
		"SysName:\t%.*s\n"
		"NodeName:\t%.*s\n"
		"Release:\t%.*s\n"
		"Version:\t%.*s\n"
		"Machine:\t%.*s\n"
		"DomainName:\t%.*s\n"
		,__NEW_UTS_LEN, cvirt->utsname.sysname
		,__NEW_UTS_LEN, cvirt->utsname.nodename
		,__NEW_UTS_LEN, cvirt->utsname.release
		,__NEW_UTS_LEN, cvirt->utsname.version
		,__NEW_UTS_LEN, cvirt->utsname.machine
		,__NEW_UTS_LEN, cvirt->utsname.domainname
		);

	a = cvirt->load[0] + (FIXED_1/200);
	b = cvirt->load[1] + (FIXED_1/200);
	c = cvirt->load[2] + (FIXED_1/200);
	length += sprintf(buffer + length,
		"nr_threads:\t%d\n"
		"nr_running:\t%d\n"
		"nr_unintr:\t%d\n"
		"nr_onhold:\t%d\n"
		"load_updates:\t%d\n"
		"loadavg:\t%d.%02d %d.%02d %d.%02d\n"
		"total_forks:\t%d\n"
		,atomic_read(&cvirt->nr_threads)
		,atomic_read(&cvirt->nr_running)
		,atomic_read(&cvirt->nr_uninterruptible)
		,atomic_read(&cvirt->nr_onhold)
		,atomic_read(&cvirt->load_updates)
		,LOAD_INT(a), LOAD_FRAC(a)
		,LOAD_INT(b), LOAD_FRAC(b)
		,LOAD_INT(c), LOAD_FRAC(c)
		,atomic_read(&cvirt->total_forks)
		);
	return length;
}


static inline long vx_sock_count(struct _vx_cacct *cacct, int type, int pos)
{
	return atomic_long_read(&cacct->sock[type][pos].count);
}


static inline long vx_sock_total(struct _vx_cacct *cacct, int type, int pos)
{
	return atomic_long_read(&cacct->sock[type][pos].total);
}

static inline int vx_info_proc_cacct(struct _vx_cacct *cacct, char *buffer)
{
	int i,j, length = 0;
	static char *type[] = { "UNSPEC", "UNIX", "INET", "INET6", "OTHER" };

	for (i=0; i<5; i++) {
		length += sprintf(buffer + length,
			"%s:", type[i]);
		for (j=0; j<3; j++) {
			length += sprintf(buffer + length,
				"\t%12lu/%-12lu"
				,vx_sock_count(cacct, i, j)
				,vx_sock_total(cacct, i, j)
				);
		}
		buffer[length++] = '\n';
	}
	length += sprintf(buffer + length,
		"forks:\t%lu\n", cacct->total_forks);
	return length;
}

#endif	/* _VX_CVIRT_PROC_H */
