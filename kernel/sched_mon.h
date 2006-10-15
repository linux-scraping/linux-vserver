
#include <linux/vserver/monitor.h>

#ifdef  CONFIG_VSERVER_MONITOR

#ifdef	CONFIG_VSERVER_HARDCPU
#define HARDCPU(x) (x)
#else
#define HARDCPU(x) (0)
#endif

#ifdef	CONFIG_VSERVER_IDLETIME
#define IDLETIME(x) (x)
#else
#define IDLETIME(x) (0)
#endif

struct _vx_mon_entry *vxm_advance(int cpu);


static inline
void	__vxm_basic(struct _vx_mon_entry *entry, xid_t xid, int type)
{
	entry->type = type;
	entry->xid = xid;
}

static inline
void	__vxm_sync(int cpu)
{
	struct _vx_mon_entry *entry = vxm_advance(cpu);

	__vxm_basic(entry, 0, VXM_SYNC);
	entry->ev.sec = xtime.tv_sec;
	entry->ev.nsec = xtime.tv_nsec;
}

static inline
void	__vxm_task(struct task_struct *p, int type)
{
	struct _vx_mon_entry *entry = vxm_advance(task_cpu(p));

	__vxm_basic(entry, p->xid, type);
	entry->ev.tsk.pid = p->pid;
	entry->ev.tsk.state = p->state;
}

static inline
void	__vxm_sched(struct _vx_sched_pc *s, struct vx_info *vxi, int cpu)
{
	struct _vx_mon_entry *entry = vxm_advance(cpu);

	__vxm_basic(entry, vxi->vx_id, (VXM_SCHED | s->flags));
	entry->sd.tokens = s->tokens;
	entry->sd.norm_time = s->norm_time;
	entry->sd.idle_time = s->idle_time;
}

static inline
void	__vxm_rqinfo1(struct rq *q, int cpu)
{
	struct _vx_mon_entry *entry = vxm_advance(cpu);

	entry->type = VXM_RQINFO_1;
	entry->xid = ((unsigned long)q >> 16) & 0xffff;
	entry->q1.running = q->nr_running;
	entry->q1.onhold = HARDCPU(q->nr_onhold);
	entry->q1.iowait = atomic_read(&q->nr_iowait);
	entry->q1.uintr = q->nr_uninterruptible;
	entry->q1.idle_tokens = IDLETIME(q->idle_tokens);
}

static inline
void	__vxm_rqinfo2(struct rq *q, int cpu)
{
	struct _vx_mon_entry *entry = vxm_advance(cpu);

	entry->type = VXM_RQINFO_2;
	entry->xid = (unsigned long)q & 0xffff;
	entry->q2.norm_time = q->norm_time;
	entry->q2.idle_time = q->idle_time;
	entry->q2.idle_skip = IDLETIME(q->idle_skip);
}

static inline
void	__vxm_update(struct _vx_sched_pc *s, struct vx_info *vxi, int cpu)
{
	struct _vx_mon_entry *entry = vxm_advance(cpu);

	__vxm_basic(entry, vxi->vx_id, VXM_UPDATE);
	entry->ev.tokens = s->tokens;
}

static inline
void	__vxm_update1(struct _vx_sched_pc *s, struct vx_info *vxi, int cpu)
{
	struct _vx_mon_entry *entry = vxm_advance(cpu);

	__vxm_basic(entry, vxi->vx_id, VXM_UPDATE_1);
	entry->u1.tokens_max = s->tokens_max;
	entry->u1.fill_rate = s->fill_rate[0];
	entry->u1.interval = s->interval[0];
}

static inline
void	__vxm_update2(struct _vx_sched_pc *s, struct vx_info *vxi, int cpu)
{
	struct _vx_mon_entry *entry = vxm_advance(cpu);

	__vxm_basic(entry, vxi->vx_id, VXM_UPDATE_2);
	entry->u2.tokens_min = s->tokens_min;
	entry->u2.fill_rate = s->fill_rate[1];
	entry->u2.interval = s->interval[1];
}


#define	vxm_activate_task(p,q)		__vxm_task(p, VXM_ACTIVATE)
#define	vxm_activate_idle(p,q)		__vxm_task(p, VXM_IDLE)
#define	vxm_deactivate_task(p,q)	__vxm_task(p, VXM_DEACTIVATE)
#define	vxm_hold_task(p,q)		__vxm_task(p, VXM_HOLD)
#define	vxm_unhold_task(p,q)		__vxm_task(p, VXM_UNHOLD)

static inline
void	vxm_migrate_task(struct task_struct *p, struct rq *rq, int dest)
{
	__vxm_task(p, VXM_MIGRATE);
	__vxm_rqinfo1(rq, task_cpu(p));
	__vxm_rqinfo2(rq, task_cpu(p));
}

static inline
void	vxm_idle_skip(struct rq *rq, int cpu)
{
	__vxm_rqinfo1(rq, cpu);
	__vxm_rqinfo2(rq, cpu);
}

static inline
void	vxm_need_resched(struct task_struct *p, int slice, int cpu)
{
	if (slice)
		return;

	__vxm_task(p, VXM_RESCHED);
}

static inline
void	vxm_sync(unsigned long now, int cpu)
{
	if (!CONFIG_VSERVER_MONITOR_SYNC ||
		(now % CONFIG_VSERVER_MONITOR_SYNC))
		return;

	__vxm_sync(cpu);
}

#define	vxm_sched_info(s,v,c)		__vxm_sched(s,v,c)

static inline
void	vxm_tokens_recalc(struct _vx_sched_pc *s, struct rq *rq,
	struct vx_info *vxi, int cpu)
{
	__vxm_sched(s, vxi, cpu);
	__vxm_rqinfo2(rq, cpu);
}

static inline
void	vxm_update_sched(struct _vx_sched_pc *s, struct vx_info *vxi, int cpu)
{
	__vxm_sched(s, vxi, cpu);
	__vxm_update(s, vxi, cpu);
	__vxm_update1(s, vxi, cpu);
	__vxm_update2(s, vxi, cpu);
}

static inline
void	vxm_rq_max_min(struct rq *rq, int cpu)
{
	__vxm_rqinfo1(rq, cpu);
	__vxm_rqinfo2(rq, cpu);
}

#else  /* CONFIG_VSERVER_MONITOR */

#define	vxm_activate_task(t,q)		do { } while (0)
#define	vxm_activate_idle(t,q)		do { } while (0)
#define	vxm_deactivate_task(t,q)	do { } while (0)
#define	vxm_hold_task(t,q)		do { } while (0)
#define	vxm_unhold_task(t,q)		do { } while (0)
#define	vxm_migrate_task(t,q,d)		do { } while (0)
#define	vxm_idle_skip(q,c)		do { } while (0)
#define	vxm_need_resched(t,s,c)		do { } while (0)
#define	vxm_sync(s,c)			do { } while (0)
#define	vxm_sched_info(s,v,c)		do { } while (0)
#define	vxm_tokens_recalc(s,q,v,c)	do { } while (0)
#define	vxm_update_sched(s,v,c)		do { } while (0)
#define	vxm_rq_max_min(q,c)		do { } while (0)

#endif /* CONFIG_VSERVER_MONITOR */

