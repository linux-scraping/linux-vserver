
static inline void vx_info_init_sched(struct _vx_sched *sched)
{
	/* scheduling; hard code starting values as constants */
	sched->prio_bias = 0;
}

static inline
void vx_info_init_sched_pc(struct _vx_sched_pc *sched_pc, int cpu)
{
	sched_pc->prio_bias = 0;

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
