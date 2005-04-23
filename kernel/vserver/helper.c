/*
 *  linux/kernel/vserver/helper.c
 *
 *  Virtual Context Support
 *
 *  Copyright (C) 2004-2005  Herbert Pötzl
 *
 *  V0.01  basic helper
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/reboot.h>
#include <linux/kmod.h>
#include <linux/sched.h>
#include <linux/vs_context.h>

#include <asm/uaccess.h>
#include <asm/unistd.h>


char vshelper_path[255] = "/sbin/vshelper";


int do_vshelper(char *name, char *argv[], char *envp[], int sync)
{
	int ret;

	if ((ret = call_usermodehelper(name, argv, envp, sync))) {
		printk(	KERN_WARNING
			"%s: (%s %s) returned %s with %d\n",
			name, argv[1], argv[2],
			sync?"sync":"async", ret);
	}
	vxdprintk(VXD_CBIT(switch, 1),
		"%s: (%s %s) returned %s with %d",
		name, argv[1], argv[2], sync?"sync":"async", ret);
	return ret;
}

/*
 *      vshelper path is set via /proc/sys
 *      invoked by vserver sys_reboot(), with
 *      the following arguments
 *
 *      argv [0] = vshelper_path;
 *      argv [1] = action: "restart", "halt", "poweroff", ...
 *      argv [2] = context identifier
 *
 *      envp [*] = type-specific parameters
 */

long vs_reboot(unsigned int cmd, void * arg)
{
	char id_buf[8], cmd_buf[16];
	char uid_buf[16], pid_buf[16];

	char *argv[] = {vshelper_path, NULL, id_buf, 0};
	char *envp[] = {"HOME=/", "TERM=linux",
			"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
			uid_buf, pid_buf, cmd_buf, 0};

	snprintf(id_buf, sizeof(id_buf)-1, "%d", vx_current_xid());

	snprintf(cmd_buf, sizeof(cmd_buf)-1, "VS_CMD=%08x", cmd);
	snprintf(uid_buf, sizeof(uid_buf)-1, "VS_UID=%d", current->uid);
	snprintf(pid_buf, sizeof(pid_buf)-1, "VS_PID=%d", current->pid);

	switch (cmd) {
	case LINUX_REBOOT_CMD_RESTART:
		argv[1] = "restart";
		break;

	case LINUX_REBOOT_CMD_HALT:
		argv[1] = "halt";
		break;

	case LINUX_REBOOT_CMD_POWER_OFF:
		argv[1] = "poweroff";
		break;

	case LINUX_REBOOT_CMD_SW_SUSPEND:
		argv[1] = "swsusp";
		break;

	default:
		return 0;
	}

	if (do_vshelper(vshelper_path, argv, envp, 1))
		return -EPERM;
	return 0;
}


/*
 *      invoked by vserver sys_reboot(), with
 *      the following arguments
 *
 *      argv [0] = vshelper_path;
 *      argv [1] = action: "startup", "shutdown"
 *      argv [2] = context identifier
 *
 *      envp [*] = type-specific parameters
 */

long vs_context_state(struct vx_info *vxi, unsigned int cmd)
{
	char id_buf[8], cmd_buf[16];
	char *argv[] = {vshelper_path, NULL, id_buf, 0};
	char *envp[] = {"HOME=/", "TERM=linux",
			"PATH=/sbin:/usr/sbin:/bin:/usr/bin", cmd_buf, 0};

	snprintf(id_buf, sizeof(id_buf)-1, "%d", vxi->vx_id);
	snprintf(cmd_buf, sizeof(cmd_buf)-1, "VS_CMD=%08x", cmd);

	switch (cmd) {
	case VS_CONTEXT_CREATED:
		argv[1] = "startup";
		break;
	case VS_CONTEXT_DESTROY:
		argv[1] = "shutdown";
		break;
	default:
		return 0;
	}

	do_vshelper(vshelper_path, argv, envp, 1);
	return 0;
}

