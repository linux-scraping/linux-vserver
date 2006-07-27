/*
 *  linux/kernel/vserver/cvirt.c
 *
 *  Virtual Server: Context Virtualization
 *
 *  Copyright (C) 2004-2006  Herbert Pötzl
 *
 *  V0.01  broken out from limit.c
 *  V0.02  added utsname stuff
 *  V0.03  changed vcmds to vxi arg
 *
 */

#include <linux/sched.h>
#include <linux/sysctl.h>
#include <linux/types.h>
#include <linux/vs_context.h>
#include <linux/vs_cvirt.h>
#include <linux/vserver/switch.h>
#include <linux/vserver/cvirt_cmd.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


void vx_vsi_uptime(struct timespec *uptime, struct timespec *idle)
{
	struct vx_info *vxi = current->vx_info;

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


int vx_uts_virt_handler(struct ctl_table *ctl, int write, xid_t xid,
	void **datap, size_t *lenp)
{
	switch (ctl->ctl_name) {
	case KERN_OSTYPE:
		*datap = vx_new_uts(sysname);
		break;
	case KERN_OSRELEASE:
		*datap = vx_new_uts(release);
		break;
	case KERN_VERSION:
		*datap = vx_new_uts(version);
		break;
	case KERN_NODENAME:
		*datap = vx_new_uts(nodename);
		break;
	case KERN_DOMAINNAME:
		*datap = vx_new_uts(domainname);
		break;
	}

	return 0;
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
	struct vx_info *vxi = current->vx_info;
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

static char * vx_vhi_name(struct vx_info *vxi, int id)
{
	switch (id) {
	case VHIN_CONTEXT:
		return vxi->vx_name;
	case VHIN_SYSNAME:
		return vxi->cvirt.utsname.sysname;
	case VHIN_NODENAME:
		return vxi->cvirt.utsname.nodename;
	case VHIN_RELEASE:
		return vxi->cvirt.utsname.release;
	case VHIN_VERSION:
		return vxi->cvirt.utsname.version;
	case VHIN_MACHINE:
		return vxi->cvirt.utsname.machine;
	case VHIN_DOMAINNAME:
		return vxi->cvirt.utsname.domainname;
	default:
		return NULL;
	}
	return NULL;
}

int vc_set_vhi_name(struct vx_info *vxi, void __user *data)
{
	struct vcmd_vhi_name_v0 vc_data;
	char *name;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	name = vx_vhi_name(vxi, vc_data.field);
	if (!name)
		return -EFAULT;

	memcpy(name, vc_data.name, 65);
	return 0;
}

int vc_get_vhi_name(struct vx_info *vxi, void __user *data)
{
	struct vcmd_vhi_name_v0 vc_data;
	char *name;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	name = vx_vhi_name(vxi, vc_data.field);
	if (!name)
		return -EFAULT;

	memcpy(vc_data.name, name, 65);
	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

#ifdef CONFIG_VSERVER_VTIME

/* virtualized time base */

void vx_gettimeofday(struct timeval *tv)
{
	do_gettimeofday(tv);
	if (!vx_flags(VXF_VIRT_TIME, 0))
		return;

	tv->tv_sec += current->vx_info->cvirt.bias_tv.tv_sec;
	tv->tv_usec += current->vx_info->cvirt.bias_tv.tv_usec;

	if (tv->tv_usec >= USEC_PER_SEC) {
		tv->tv_sec++;
		tv->tv_usec -= USEC_PER_SEC;
	} else if (tv->tv_usec < 0) {
		tv->tv_sec--;
		tv->tv_usec += USEC_PER_SEC;
	}
}

int vx_settimeofday(struct timespec *ts)
{
	struct timeval tv;

	if (!vx_flags(VXF_VIRT_TIME, 0))
		return do_settimeofday(ts);

	do_gettimeofday(&tv);
	current->vx_info->cvirt.bias_tv.tv_sec =
		ts->tv_sec - tv.tv_sec;
	current->vx_info->cvirt.bias_tv.tv_usec =
		(ts->tv_nsec/NSEC_PER_USEC) - tv.tv_usec;
	return 0;
}

#endif

