#ifndef _VX_SCHED_PROC_H
#define _VX_SCHED_PROC_H


static inline
int vx_info_proc_sched(struct _vx_sched *sched, char *buffer)
{
	int length = 0;

	length += sprintf(buffer,
		"PrioBias:\t%8d\n",
		sched->prio_bias);
	return length;
}

static inline
int vx_info_proc_sched_pc(struct _vx_sched_pc *sched_pc,
	char *buffer, int cpu)
{
	int length = 0;

	length += sprintf(buffer + length,
		"cpu %d: %lld %lld %lld", cpu,
		(unsigned long long)sched_pc->user_ticks,
		(unsigned long long)sched_pc->sys_ticks,
		(unsigned long long)sched_pc->hold_ticks);
	length += sprintf(buffer + length,
		" %d\n", sched_pc->prio_bias);
	return length;
}

#endif	/* _VX_SCHED_PROC_H */
