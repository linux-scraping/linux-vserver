

extern uint64_t vx_idle_jiffies(void);

static inline void vx_info_init_cvirt(struct _vx_cvirt *cvirt)
{
	uint64_t idle_jiffies = vx_idle_jiffies();
	uint64_t nsuptime;

	do_posix_clock_monotonic_gettime(&cvirt->bias_uptime);
	nsuptime = (unsigned long long)cvirt->bias_uptime.tv_sec
		* NSEC_PER_SEC + cvirt->bias_uptime.tv_nsec;
	cvirt->bias_clock = nsec_to_clock_t(nsuptime);

	jiffies_to_timespec(idle_jiffies, &cvirt->bias_idle);
	atomic_set(&cvirt->nr_threads, 0);
	atomic_set(&cvirt->nr_running, 0);
	atomic_set(&cvirt->nr_uninterruptible, 0);
	atomic_set(&cvirt->nr_onhold, 0);

	down_read(&uts_sem);
	cvirt->utsname = system_utsname;
	up_read(&uts_sem);

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

static inline void vx_info_exit_cvirt(struct _vx_cvirt *cvirt)
{
#ifdef	CONFIG_VSERVER_DEBUG
	int value;

	vxwprintk((value = atomic_read(&cvirt->nr_threads)),
		"!!! cvirt: %p[nr_threads] = %d on exit.",
		cvirt, value);
	vxwprintk((value = atomic_read(&cvirt->nr_running)),
		"!!! cvirt: %p[nr_running] = %d on exit.",
		cvirt, value);
	vxwprintk((value = atomic_read(&cvirt->nr_uninterruptible)),
		"!!! cvirt: %p[nr_uninterruptible] = %d on exit.",
		cvirt, value);
	vxwprintk((value = atomic_read(&cvirt->nr_onhold)),
		"!!! cvirt: %p[nr_onhold] = %d on exit.",
		cvirt, value);
#endif
	return;
}

static inline void vx_info_init_cacct(struct _vx_cacct *cacct)
{
	int i,j;

	for (i=0; i<5; i++) {
		for (j=0; j<3; j++) {
			atomic_set(&cacct->sock[i][j].count, 0);
			atomic_set(&cacct->sock[i][j].total, 0);
		}
	}
}

static inline void vx_info_exit_cacct(struct _vx_cacct *cacct)
{
	return;
}

