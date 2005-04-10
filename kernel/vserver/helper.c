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


/*
 *      vshelper path is set via /proc/sys
 *      invoked by vserver sys_reboot(), with
 *      the following arguments
 *
 *      argv [0] = vshelper_path;
 *      argv [1] = action: "restart", "halt", "poweroff", ...
 *      argv [2] = context identifier
 *      argv [3] = additional argument (restart2)
 *
 *      envp [*] = type-specific parameters
 */

long vs_reboot(unsigned int cmd, void * arg)
{
	char id_buf[8], cmd_buf[32];
	char uid_buf[32], pid_buf[32];
	char buffer[256];

	char *argv[] = {vshelper_path, NULL, id_buf, NULL, 0};
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

	case LINUX_REBOOT_CMD_RESTART2:
		if (strncpy_from_user(&buffer[0], (char *)arg, sizeof(buffer) - 1) < 0)
			return -EFAULT;
		argv[3] = buffer;
	default:
		argv[1] = "restart2";
		break;
	}

	/* maybe we should wait ? */
	if (call_usermodehelper(*argv, argv, envp, 0)) {
		printk( KERN_WARNING
			"vs_reboot(): failed to exec (%s %s %s %s)\n",
			vshelper_path, argv[1], argv[2], argv[3]);
		return -EPERM;
	}
	return 0;
}

long vs_context_state(unsigned int cmd)
{
	char id_buf[8], cmd_buf[32];

	char *argv[] = {vshelper_path, NULL, id_buf, NULL, 0};
	char *envp[] = {"HOME=/", "TERM=linux",
			"PATH=/sbin:/usr/sbin:/bin:/usr/bin", cmd_buf, 0};

	snprintf(id_buf, sizeof(id_buf)-1, "%d", vx_current_xid());
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

	if (call_usermodehelper(*argv, argv, envp, 1)) {
		printk( KERN_WARNING
			"vs_context_state(): failed to exec (%s %s %s %s)\n",
			vshelper_path, argv[1], argv[2], argv[3]);
		return 0;
	}
	return 0;
}

