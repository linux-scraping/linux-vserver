#ifndef _VX_SCHED_PROC_H
#define _VX_SCHED_PROC_H


static inline
int vx_info_proc_sched(struct _vx_sched *sched, char *buffer)
{
	int length = 0;

	length += sprintf(buffer,
		"FillRate:\t%8d,%d\n"
		"Interval:\t%8d,%d\n"
		"TokensMin:\t%8d\n"
		"TokensMax:\t%8d\n"
		"PrioBias:\t%8d\n"
		,sched->fill_rate[0]
		,sched->fill_rate[1]
		,sched->interval[0]
		,sched->interval[1]
		,sched->tokens_min
		,sched->tokens_max
		,sched->prio_bias
		);
	return length;
}

static inline
int vx_info_proc_sched_pc(struct _vx_sched_pc *sched_pc,
	char *buffer, int cpu)
{
	int length = 0;

	length += sprintf(buffer + length,
		"cpu %d: %lld %lld %lld %ld %ld"
		,cpu
		,(unsigned long long)sched_pc->user_ticks
		,(unsigned long long)sched_pc->sys_ticks
		,(unsigned long long)sched_pc->hold_ticks
		,sched_pc->token_time
		,sched_pc->idle_time
		);
	length += sprintf(buffer + length,
		" %c%c %d %d %d %d/%d %d/%d"
		,(sched_pc->flags & VXSF_ONHOLD) ? 'H' : 'R'
		,(sched_pc->flags & VXSF_IDLE_TIME) ? 'I' : '-'
		,sched_pc->tokens
		,sched_pc->tokens_min
		,sched_pc->tokens_max
		,sched_pc->fill_rate[0]
		,sched_pc->interval[0]
		,sched_pc->fill_rate[1]
		,sched_pc->interval[1]
		);
	length += sprintf(buffer + length,
		" %d %d\n"
		,sched_pc->prio_bias
		,sched_pc->vavavoom
		);
	return length;
}

#endif	/* _VX_SCHED_PROC_H */
