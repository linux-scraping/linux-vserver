
static inline void vx_info_init_sched(struct _vx_sched *sched)
{
	int i;

	/* scheduling; hard code starting values as constants */
	sched->fill_rate	= 1;
	sched->interval		= 4;
	sched->tokens_min	= HZ >> 4;
	sched->tokens_max	= HZ >> 1;
	sched->jiffies		= jiffies;
	sched->tokens_lock	= SPIN_LOCK_UNLOCKED;

	atomic_set(&sched->tokens, HZ >> 2);
	sched->cpus_allowed	= CPU_MASK_ALL;
	sched->priority_bias	= 0;
	sched->vavavoom		= 0;

	for_each_cpu(i) {
		sched->cpu[i].user_ticks	= 0;
		sched->cpu[i].sys_ticks		= 0;
		sched->cpu[i].hold_ticks	= 0;
	}
}

static inline void vx_info_exit_sched(struct _vx_sched *sched)
{
	return;
}

