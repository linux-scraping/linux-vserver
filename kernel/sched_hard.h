#ifndef _VX_SCHED_HARD_H
#define _VX_SCHED_HARD_H

#ifndef CONFIG_VSERVER
#warning config options missing
#endif


#ifdef CONFIG_VSERVER_IDLELIMIT

/*
 * vx_idle_resched - reschedule after maxidle
 */
static inline
void vx_idle_resched(runqueue_t *rq)
{
	if (!--rq->idle_tokens && !list_empty(&rq->hold_queue))
		set_need_resched();
}

#else /* !CONFIG_VSERVER_IDLELIMIT */

#define vx_idle_resched(rq)

#endif /* CONFIG_VSERVER_IDLELIMIT */



#ifdef CONFIG_VSERVER_IDLETIME

#define vx_set_rq_min_skip(rq, min)		\
	(rq)->idle_skip = (min)

#define vx_save_min_skip(ret, min, val)		\
	__vx_save_min_skip(ret, min, val)

static inline
void __vx_save_min_skip(int ret, int *min, int val)
{
	if (ret > -2)
		return;
	if ((*min > val) || !*min)
		*min = val;
}

static inline
int vx_try_skip(runqueue_t *rq)
{
	/* artificially advance time */
	if (rq->idle_skip && !list_empty(&rq->hold_queue)) {
		rq->idle_time += rq->idle_skip;
		return 1;
	}
	return 0;
}

#else /* !CONFIG_VSERVER_IDLETIME */

#define vx_set_rq_min_skip(rq, min)		\
	({ int dummy = (min); dummy; })

#define vx_save_min_skip(ret, min, val)

static inline
int vx_try_skip(runqueue_t *rq)
{
	return 0;
}

#endif /* CONFIG_VSERVER_IDLETIME */



#ifdef CONFIG_VSERVER_HARDCPU

#define vx_set_rq_max_idle(rq, max)		\
	(rq)->idle_tokens = (max)

#define vx_save_max_idle(ret, min, val)		\
	__vx_save_max_idle(ret, min, val)

static inline
void __vx_save_max_idle(int ret, int *min, int val)
{
	if (*min > val)
		*min = val;
}


/*
 * vx_hold_task - put a task on the hold queue
 */
static inline
void vx_hold_task(struct task_struct *p, runqueue_t *rq)
{
	__deactivate_task(p, rq);
	p->state |= TASK_ONHOLD;
	/* a new one on hold */
	rq->nr_onhold++;
	list_add_tail(&p->run_list, &rq->hold_queue);
}

/*
 * vx_unhold_task - put a task back to the runqueue
 */
static inline
void vx_unhold_task(struct task_struct *p, runqueue_t *rq)
{
	list_del(&p->run_list);
	/* one less waiting */
	rq->nr_onhold--;
	p->state &= ~TASK_ONHOLD;
	enqueue_task(p, rq->expired);
	rq->nr_running++;

	if (p->static_prio < rq->best_expired_prio)
		rq->best_expired_prio = p->static_prio;
}

unsigned long nr_onhold(void)
{
	unsigned long i, sum = 0;

	for_each_online_cpu(i)
		sum += cpu_rq(i)->nr_onhold;

	return sum;
}



static inline
int __vx_tokens_avail(struct _vx_sched_pc *sched_pc)
{
	return sched_pc->tokens;
}

static inline
void __vx_consume_token(struct _vx_sched_pc *sched_pc)
{
	sched_pc->tokens--;
}

static inline
int vx_need_resched(struct task_struct *p, int slice, int cpu)
{
	struct vx_info *vxi = p->vx_info;

	if (vxi) {
		struct _vx_sched_pc *sched_pc =
			&vx_per_cpu(vxi, sched_pc, cpu);
		int tokens;

		/* maybe we can simplify that to decrement
		   the token counter unconditional? */

		if ((tokens = __vx_tokens_avail(sched_pc)) > 0)
			__vx_consume_token(sched_pc);

		/* for tokens > 0, one token was consumed */
		if (tokens < 2)
			return 1;
	}
	return (slice == 0);
}


#define vx_set_rq_time(rq, time) do {	\
	rq->norm_time = time;		\
} while (0)


static inline
void vx_try_unhold(runqueue_t *rq, int cpu)
{
	struct vx_info *vxi = NULL;
	struct list_head *l, *n;
	int maxidle = HZ;
	int minskip = 0;

	/* nothing to do? */
	if (list_empty(&rq->hold_queue))
		return;

	list_for_each_safe(l, n, &rq->hold_queue) {
		int ret, delta_min[2];
		struct _vx_sched_pc *sched_pc;
		struct task_struct *p;

		p = list_entry(l, task_t, run_list);
		if (vxi == p->vx_info)
			continue;

		vxi = p->vx_info;
		/* ignore paused contexts */
		if (vx_info_flags(vxi, VXF_SCHED_PAUSE, 0))
			continue;

		sched_pc = &vx_per_cpu(vxi, sched_pc, cpu);

		/* recalc tokens */
		ret = vx_tokens_recalc(sched_pc,
			&rq->norm_time, &rq->idle_time, delta_min);

		if (ret > 0) {
			/* we found a runable context */
			vx_unhold_task(p, rq);
			break;
		}
		vx_save_max_idle(ret, &maxidle, delta_min[0]);
		vx_save_min_skip(ret, &minskip, delta_min[1]);
	}
	vx_set_rq_max_idle(rq, maxidle);
	vx_set_rq_min_skip(rq, minskip);
}


static inline
int vx_schedule(struct task_struct *next, runqueue_t *rq, int cpu)
{
	struct vx_info *vxi = next->vx_info;
	struct _vx_sched_pc *sched_pc;
	int delta_min[2];
	int flags, ret;

	if (!vxi)
		return 1;

	flags = vxi->vx_flags;

	if (unlikely(vx_check_flags(flags , VXF_SCHED_PAUSE, 0)))
		goto put_on_hold;
	if (!vx_check_flags(flags , VXF_SCHED_HARD|VXF_SCHED_PRIO, 0))
		return 1;

	sched_pc = &vx_per_cpu(vxi, sched_pc, cpu);
#ifdef CONFIG_SMP
	/* update scheduler params */
	if (cpu_isset(cpu, vxi->sched.update)) {
		vx_update_sched_param(&vxi->sched, sched_pc);
		cpu_clear(cpu, vxi->sched.update);
	}
#endif
	ret  = vx_tokens_recalc(sched_pc,
		&rq->norm_time, &rq->idle_time, delta_min);

	if (!vx_check_flags(flags , VXF_SCHED_HARD, 0))
		return 1;

	if (unlikely(ret < 0)) {
		vx_save_max_idle(ret, &rq->idle_tokens, delta_min[0]);
		vx_save_min_skip(ret, &rq->idle_skip, delta_min[1]);
	put_on_hold:
		vx_hold_task(next, rq);
		return 0;
	}
	return 1;
}


#else /* CONFIG_VSERVER_HARDCPU */

static inline
void vx_hold_task(struct task_struct *p, runqueue_t *rq)
{
	return;
}

static inline
void vx_unhold_task(struct task_struct *p, runqueue_t *rq)
{
	return;
}

unsigned long nr_onhold(void)
{
	return 0;
}


static inline
int vx_need_resched(struct task_struct *p, int slice, int cpu)
{
	return (slice == 0);
}


#define vx_set_rq_time(rq, time)

static inline
void vx_try_unhold(runqueue_t *rq, int cpu)
{
	return;
}

static inline
int vx_schedule(struct task_struct *next, runqueue_t *rq, int cpu)
{
	struct vx_info *vxi = next->vx_info;
	struct _vx_sched_pc *sched_pc;
	int delta_min[2];
	int ret;

	if (!vx_info_flags(vxi, VXF_SCHED_PRIO, 0))
		return 1;

	sched_pc = &vx_per_cpu(vxi, sched_pc, cpu);
	ret  = vx_tokens_recalc(sched_pc,
		&rq->norm_time, &rq->idle_time, delta_min);
	return 1;
}

#endif /* CONFIG_VSERVER_HARDCPU */

#endif /* _VX_SCHED_HARD_H */
