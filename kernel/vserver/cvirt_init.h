

extern uint64_t vx_idle_jiffies(void);

static inline void vx_info_init_cvirt(struct _vx_cvirt *cvirt)
{
	uint64_t idle_jiffies = vx_idle_jiffies();
	uint64_t nsuptime;

	do_posix_clock_monotonic_gettime(&cvirt->bias_uptime);
	nsuptime = (unsigned long long)cvirt->bias_uptime.tv_sec
		* NSEC_PER_SEC + cvirt->bias_uptime.tv_nsec;
	cvirt->bias_clock = nsec_to_clock_t(nsuptime);
	cvirt->bias_ts.tv_sec = 0;
	cvirt->bias_ts.tv_nsec = 0;

	jiffies_to_timespec(idle_jiffies, &cvirt->bias_idle);
	atomic_set(&cvirt->nr_threads, 0);
	atomic_set(&cvirt->nr_running, 0);
	atomic_set(&cvirt->nr_uninterruptible, 0);
	atomic_set(&cvirt->nr_onhold, 0);

	spin_lock_init(&cvirt->load_lock);
	cvirt->load_last = jiffies;
	atomic_set(&cvirt->load_updates, 0);
	cvirt->load[0] = 0;
	cvirt->load[1] = 0;
	cvirt->load[2] = 0;
	atomic_set(&cvirt->total_forks, 0);

	spin_lock_init(&cvirt->syslog.logbuf_lock);
	init_waitqueue_head(&cvirt->syslog.log_wait);
	cvirt->syslog.log_start = 0;
	cvirt->syslog.log_end = 0;
	cvirt->syslog.con_start = 0;
	cvirt->syslog.logged_chars = 0;
}

static inline
void vx_info_init_cvirt_pc(struct _vx_cvirt_pc *cvirt_pc, int cpu)
{
	// cvirt_pc->cpustat = { 0 };
}

static inline void vx_info_exit_cvirt(struct _vx_cvirt *cvirt)
{
#ifdef	CONFIG_VSERVER_WARN
	int value;
#endif
	vxwprintk_xid((value = atomic_read(&cvirt->nr_threads)),
		"!!! cvirt: %p[nr_threads] = %d on exit.",
		cvirt, value);
	vxwprintk_xid((value = atomic_read(&cvirt->nr_running)),
		"!!! cvirt: %p[nr_running] = %d on exit.",
		cvirt, value);
	vxwprintk_xid((value = atomic_read(&cvirt->nr_uninterruptible)),
		"!!! cvirt: %p[nr_uninterruptible] = %d on exit.",
		cvirt, value);
	vxwprintk_xid((value = atomic_read(&cvirt->nr_onhold)),
		"!!! cvirt: %p[nr_onhold] = %d on exit.",
		cvirt, value);
	return;
}

static inline
void vx_info_exit_cvirt_pc(struct _vx_cvirt_pc *cvirt_pc, int cpu)
{
	return;
}

