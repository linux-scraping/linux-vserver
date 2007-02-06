
static inline void vx_info_init_sched(struct _vx_sched *sched)
{
	static struct lock_class_key tokens_lock_key;

	/* scheduling; hard code starting values as constants */
	sched->fill_rate[0]	= 1;
	sched->interval[0]	= 4;
	sched->fill_rate[1]	= 1;
	sched->interval[1]	= 8;
	sched->tokens		= HZ >> 2;
	sched->tokens_min	= HZ >> 4;
	sched->tokens_max	= HZ >> 1;
	sched->tokens_lock	= SPIN_LOCK_UNLOCKED;
	sched->prio_bias	= 0;

	lockdep_set_class(&sched->tokens_lock, &tokens_lock_key);
}

static inline
void vx_info_init_sched_pc(struct _vx_sched_pc *sched_pc, int cpu)
{
	sched_pc->fill_rate[0]	= 1;
	sched_pc->interval[0]	= 4;
	sched_pc->fill_rate[1]	= 1;
	sched_pc->interval[1]	= 8;
	sched_pc->tokens	= HZ >> 2;
	sched_pc->tokens_min	= HZ >> 4;
	sched_pc->tokens_max	= HZ >> 1;
	sched_pc->prio_bias	= 0;
	sched_pc->vavavoom	= 0;
	sched_pc->token_time	= 0;
	sched_pc->idle_time	= 0;
	sched_pc->norm_time	= jiffies;

	sched_pc->user_ticks = 0;
	sched_pc->sys_ticks = 0;
	sched_pc->hold_ticks = 0;
}

static inline void vx_info_exit_sched(struct _vx_sched *sched)
{
	return;
}

static inline
void vx_info_exit_sched_pc(struct _vx_sched_pc *sched_pc, int cpu)
{
	return;
}
