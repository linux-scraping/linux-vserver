#ifndef _VX_SCHED_PROC_H
#define _VX_SCHED_PROC_H


static inline int vx_info_proc_sched(struct _vx_sched *sched, char *buffer)
{
	int length = 0;
	int i;

	length += sprintf(buffer,
		"Token:\t\t%8d\n"
		"FillRate:\t%8d\n"
		"Interval:\t%8d\n"
		"TokensMin:\t%8d\n"
		"TokensMax:\t%8d\n"
		"PrioBias:\t%8d\n"
		"VaVaVoom:\t%8d\n"
		,atomic_read(&sched->tokens)
		,sched->fill_rate
		,sched->interval
		,sched->tokens_min
		,sched->tokens_max
		,sched->priority_bias
		,sched->vavavoom
		);

	for_each_online_cpu(i) {
		length += sprintf(buffer + length,
			"cpu %d: %lld %lld %lld\n"
			,i
			,(long long)sched->cpu[i].user_ticks
			,(long long)sched->cpu[i].sys_ticks
			,(long long)sched->cpu[i].hold_ticks
			);
	}

	return length;
}

#endif	/* _VX_SCHED_PROC_H */
