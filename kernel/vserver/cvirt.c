/*
 *  linux/kernel/vserver/cvirt.c
 *
 *  Virtual Server: Context Virtualization
 *
 *  Copyright (C) 2004-2007  Herbert Pötzl
 *
 *  V0.01  broken out from limit.c
 *  V0.02  added utsname stuff
 *  V0.03  changed vcmds to vxi arg
 *
 */

#include <linux/types.h>
#include <linux/utsname.h>
#include <linux/vs_cvirt.h>
#include <linux/vserver/switch.h>
#include <linux/vserver/cvirt_cmd.h>

#include <asm/uaccess.h>


void vx_vsi_boottime(struct timespec *boottime)
{
	struct vx_info *vxi = current_vx_info();

	set_normalized_timespec(boottime,
		boottime->tv_sec + vxi->cvirt.bias_uptime.tv_sec,
		boottime->tv_nsec + vxi->cvirt.bias_uptime.tv_nsec);
	return;
}

void vx_vsi_uptime(struct timespec *uptime, struct timespec *idle)
{
	struct vx_info *vxi = current_vx_info();

	set_normalized_timespec(uptime,
		uptime->tv_sec - vxi->cvirt.bias_uptime.tv_sec,
		uptime->tv_nsec - vxi->cvirt.bias_uptime.tv_nsec);
	if (!idle)
		return;
	set_normalized_timespec(idle,
		idle->tv_sec - vxi->cvirt.bias_idle.tv_sec,
		idle->tv_nsec - vxi->cvirt.bias_idle.tv_nsec);
	return;
}

uint64_t vx_idle_jiffies(void)
{
	return init_task.utime + init_task.stime;
}



static inline uint32_t __update_loadavg(uint32_t load,
	int wsize, int delta, int n)
{
	unsigned long long calc, prev;

	/* just set it to n */
	if (unlikely(delta >= wsize))
		return (n << FSHIFT);

	calc = delta * n;
	calc <<= FSHIFT;
	prev = (wsize - delta);
	prev *= load;
	calc += prev;
	do_div(calc, wsize);
	return calc;
}


void vx_update_load(struct vx_info *vxi)
{
	uint32_t now, last, delta;
	unsigned int nr_running, nr_uninterruptible;
	unsigned int total;
	unsigned long flags;

	spin_lock_irqsave(&vxi->cvirt.load_lock, flags);

	now = jiffies;
	last = vxi->cvirt.load_last;
	delta = now - last;

	if (delta < 5*HZ)
		goto out;

	nr_running = atomic_read(&vxi->cvirt.nr_running);
	nr_uninterruptible = atomic_read(&vxi->cvirt.nr_uninterruptible);
	total = nr_running + nr_uninterruptible;

	vxi->cvirt.load[0] = __update_loadavg(vxi->cvirt.load[0],
		60*HZ, delta, total);
	vxi->cvirt.load[1] = __update_loadavg(vxi->cvirt.load[1],
		5*60*HZ, delta, total);
	vxi->cvirt.load[2] = __update_loadavg(vxi->cvirt.load[2],
		15*60*HZ, delta, total);

	vxi->cvirt.load_last = now;
out:
	atomic_inc(&vxi->cvirt.load_updates);
	spin_unlock_irqrestore(&vxi->cvirt.load_lock, flags);
}


/*
 * Commands to do_syslog:
 *
 *      0 -- Close the log.  Currently a NOP.
 *      1 -- Open the log. Currently a NOP.
 *      2 -- Read from the log.
 *      3 -- Read all messages remaining in the ring buffer.
 *      4 -- Read and clear all messages remaining in the ring buffer
 *      5 -- Clear ring buffer.
 *      6 -- Disable printk's to console
 *      7 -- Enable printk's to console
 *      8 -- Set level of messages printed to console
 *      9 -- Return number of unread characters in the log buffer
 *     10 -- Return size of the log buffer
 */
int vx_do_syslog(int type, char __user *buf, int len)
{
	int error = 0;
	int do_clear = 0;
	struct vx_info *vxi = current_vx_info();
	struct _vx_syslog *log;

	if (!vxi)
		return -EINVAL;
	log = &vxi->cvirt.syslog;

	switch (type) {
	case 0:		/* Close log */
	case 1:		/* Open log */
		break;
	case 2:		/* Read from log */
		error = wait_event_interruptible(log->log_wait,
			(log->log_start - log->log_end));
		if (error)
			break;
		spin_lock_irq(&log->logbuf_lock);
		spin_unlock_irq(&log->logbuf_lock);
		break;
	case 4:		/* Read/clear last kernel messages */
		do_clear = 1;
		/* fall through */
	case 3:		/* Read last kernel messages */
		return 0;

	case 5:		/* Clear ring buffer */
		return 0;

	case 6:		/* Disable logging to console */
	case 7:		/* Enable logging to console */
	case 8:		/* Set level of messages printed to console */
		break;

	case 9:		/* Number of chars in the log buffer */
		return 0;
	case 10:	/* Size of the log buffer */
		return 0;
	default:
		error = -EINVAL;
		break;
	}
	return error;
}


/* virtual host info names */

static char *vx_vhi_name(struct vx_info *vxi, int id)
{
	struct nsproxy *nsproxy;
	struct uts_namespace *uts;

	if (id == VHIN_CONTEXT)
		return vxi->vx_name;

	nsproxy = vxi->space[0].vx_nsproxy;
	if (!nsproxy)
		return NULL;

	uts = nsproxy->uts_ns;
	if (!uts)
		return NULL;

	switch (id) {
	case VHIN_SYSNAME:
		return uts->name.sysname;
	case VHIN_NODENAME:
		return uts->name.nodename;
	case VHIN_RELEASE:
		return uts->name.release;
	case VHIN_VERSION:
		return uts->name.version;
	case VHIN_MACHINE:
		return uts->name.machine;
	case VHIN_DOMAINNAME:
		return uts->name.domainname;
	default:
		return NULL;
	}
	return NULL;
}

int vc_set_vhi_name(struct vx_info *vxi, void __user *data)
{
	struct vcmd_vhi_name_v0 vc_data;
	char *name;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	name = vx_vhi_name(vxi, vc_data.field);
	if (!name)
		return -EINVAL;

	memcpy(name, vc_data.name, 65);
	return 0;
}

int vc_get_vhi_name(struct vx_info *vxi, void __user *data)
{
	struct vcmd_vhi_name_v0 vc_data;
	char *name;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	name = vx_vhi_name(vxi, vc_data.field);
	if (!name)
		return -EINVAL;

	memcpy(vc_data.name, name, 65);
	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


int vc_virt_stat(struct vx_info *vxi, void __user *data)
{
	struct vcmd_virt_stat_v0 vc_data;
	struct _vx_cvirt *cvirt = &vxi->cvirt;
	struct timespec uptime;

	do_posix_clock_monotonic_gettime(&uptime);
	set_normalized_timespec(&uptime,
		uptime.tv_sec - cvirt->bias_uptime.tv_sec,
		uptime.tv_nsec - cvirt->bias_uptime.tv_nsec);

	vc_data.offset = timespec_to_ns(&cvirt->bias_ts);
	vc_data.uptime = timespec_to_ns(&uptime);
	vc_data.nr_threads = atomic_read(&cvirt->nr_threads);
	vc_data.nr_running = atomic_read(&cvirt->nr_running);
	vc_data.nr_uninterruptible = atomic_read(&cvirt->nr_uninterruptible);
	vc_data.nr_onhold = atomic_read(&cvirt->nr_onhold);
	vc_data.nr_forks = atomic_read(&cvirt->total_forks);
	vc_data.load[0] = cvirt->load[0];
	vc_data.load[1] = cvirt->load[1];
	vc_data.load[2] = cvirt->load[2];

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


#ifdef CONFIG_VSERVER_VTIME

/* virtualized time base */

void vx_adjust_timespec(struct timespec *ts)
{
	struct vx_info *vxi;

	if (!vx_flags(VXF_VIRT_TIME, 0))
		return;

	vxi = current_vx_info();
	ts->tv_sec += vxi->cvirt.bias_ts.tv_sec;
	ts->tv_nsec += vxi->cvirt.bias_ts.tv_nsec;

	if (ts->tv_nsec >= NSEC_PER_SEC) {
		ts->tv_sec++;
		ts->tv_nsec -= NSEC_PER_SEC;
	} else if (ts->tv_nsec < 0) {
		ts->tv_sec--;
		ts->tv_nsec += NSEC_PER_SEC;
	}
}

int vx_settimeofday(const struct timespec *ts)
{
	struct timespec ats, delta;
	struct vx_info *vxi;

	if (!vx_flags(VXF_VIRT_TIME, 0))
		return do_settimeofday(ts);

	getnstimeofday(&ats);
	delta = timespec_sub(*ts, ats);

	vxi = current_vx_info();
	vxi->cvirt.bias_ts = timespec_add(vxi->cvirt.bias_ts, delta);
	return 0;
}

#endif

