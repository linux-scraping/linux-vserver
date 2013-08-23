/*
 *  linux/kernel/vserver/helper.c
 *
 *  Virtual Context Support
 *
 *  Copyright (C) 2004-2007  Herbert Pötzl
 *
 *  V0.01  basic helper
 *
 */

#include <linux/kmod.h>
#include <linux/reboot.h>
#include <linux/vs_context.h>
#include <linux/vs_network.h>
#include <linux/vserver/signal.h>


char vshelper_path[255] = "/sbin/vshelper";

static int vshelper_init(struct subprocess_info *info, struct cred *new_cred)
{
	current->flags &= ~PF_NO_SETAFFINITY;
	return 0;
}

static int vs_call_usermodehelper(char *path, char **argv, char **envp, int wait)
{
	struct subprocess_info *info;
	gfp_t gfp_mask = (wait == UMH_NO_WAIT) ? GFP_ATOMIC : GFP_KERNEL;

	info = call_usermodehelper_setup(path, argv, envp, gfp_mask,
					 vshelper_init, NULL, NULL);
	if (info == NULL)
		return -ENOMEM;

	return call_usermodehelper_exec(info, wait);
}

static int do_vshelper(char *name, char *argv[], char *envp[], int sync)
{
	int ret;

	if ((ret = vs_call_usermodehelper(name, argv, envp,
		sync ? UMH_WAIT_PROC : UMH_WAIT_EXEC))) {
		printk(KERN_WARNING "%s: (%s %s) returned %s with %d\n",
			name, argv[1], argv[2],
			sync ? "sync" : "async", ret);
	}
	vxdprintk(VXD_CBIT(switch, 4),
		"%s: (%s %s) returned %s with %d",
		name, argv[1], argv[2], sync ? "sync" : "async", ret);
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

long vs_reboot_helper(struct vx_info *vxi, int cmd, void __user *arg)
{
	char id_buf[8], cmd_buf[16];
	char uid_buf[16], pid_buf[16];
	int ret;

	char *argv[] = {vshelper_path, NULL, id_buf, 0};
	char *envp[] = {"HOME=/", "TERM=linux",
			"PATH=/sbin:/usr/sbin:/bin:/usr/bin",
			uid_buf, pid_buf, cmd_buf, 0};

	if (vx_info_state(vxi, VXS_HELPER))
		return -EAGAIN;
	vxi->vx_state |= VXS_HELPER;

	snprintf(id_buf, sizeof(id_buf), "%d", vxi->vx_id);

	snprintf(cmd_buf, sizeof(cmd_buf), "VS_CMD=%08x", cmd);
	snprintf(uid_buf, sizeof(uid_buf), "VS_UID=%d",
		from_kuid(&init_user_ns, current_uid()));
	snprintf(pid_buf, sizeof(pid_buf), "VS_PID=%d", current->pid);

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

	case LINUX_REBOOT_CMD_OOM:
		argv[1] = "oom";
		break;

	default:
		vxi->vx_state &= ~VXS_HELPER;
		return 0;
	}

	ret = do_vshelper(vshelper_path, argv, envp, 0);
	vxi->vx_state &= ~VXS_HELPER;
	__wakeup_vx_info(vxi);
	return (ret) ? -EPERM : 0;
}


long vs_reboot(unsigned int cmd, void __user *arg)
{
	struct vx_info *vxi = current_vx_info();
	long ret = 0;

	vxdprintk(VXD_CBIT(misc, 5),
		"vs_reboot(%p[#%d],%u)",
		vxi, vxi ? vxi->vx_id : 0, cmd);

	ret = vs_reboot_helper(vxi, cmd, arg);
	if (ret)
		return ret;

	vxi->reboot_cmd = cmd;
	if (vx_info_flags(vxi, VXF_REBOOT_KILL, 0)) {
		switch (cmd) {
		case LINUX_REBOOT_CMD_RESTART:
		case LINUX_REBOOT_CMD_HALT:
		case LINUX_REBOOT_CMD_POWER_OFF:
			vx_info_kill(vxi, 0, SIGKILL);
			vx_info_kill(vxi, 1, SIGKILL);
		default:
			break;
		}
	}
	return 0;
}

long vs_oom_action(unsigned int cmd)
{
	struct vx_info *vxi = current_vx_info();
	long ret = 0;

	vxdprintk(VXD_CBIT(misc, 5),
		"vs_oom_action(%p[#%d],%u)",
		vxi, vxi ? vxi->vx_id : 0, cmd);

	ret = vs_reboot_helper(vxi, cmd, NULL);
	if (ret)
		return ret;

	vxi->reboot_cmd = cmd;
	if (vx_info_flags(vxi, VXF_REBOOT_KILL, 0)) {
		vx_info_kill(vxi, 0, SIGKILL);
		vx_info_kill(vxi, 1, SIGKILL);
	}
	return 0;
}

/*
 *      argv [0] = vshelper_path;
 *      argv [1] = action: "startup", "shutdown"
 *      argv [2] = context identifier
 *
 *      envp [*] = type-specific parameters
 */

long vs_state_change(struct vx_info *vxi, unsigned int cmd)
{
	char id_buf[8], cmd_buf[16];
	char *argv[] = {vshelper_path, NULL, id_buf, 0};
	char *envp[] = {"HOME=/", "TERM=linux",
			"PATH=/sbin:/usr/sbin:/bin:/usr/bin", cmd_buf, 0};

	if (!vx_info_flags(vxi, VXF_SC_HELPER, 0))
		return 0;

	snprintf(id_buf, sizeof(id_buf), "%d", vxi->vx_id);
	snprintf(cmd_buf, sizeof(cmd_buf), "VS_CMD=%08x", cmd);

	switch (cmd) {
	case VSC_STARTUP:
		argv[1] = "startup";
		break;
	case VSC_SHUTDOWN:
		argv[1] = "shutdown";
		break;
	default:
		return 0;
	}

	return do_vshelper(vshelper_path, argv, envp, 1);
}


/*
 *      argv [0] = vshelper_path;
 *      argv [1] = action: "netup", "netdown"
 *      argv [2] = context identifier
 *
 *      envp [*] = type-specific parameters
 */

long vs_net_change(struct nx_info *nxi, unsigned int cmd)
{
	char id_buf[8], cmd_buf[16];
	char *argv[] = {vshelper_path, NULL, id_buf, 0};
	char *envp[] = {"HOME=/", "TERM=linux",
			"PATH=/sbin:/usr/sbin:/bin:/usr/bin", cmd_buf, 0};

	if (!nx_info_flags(nxi, NXF_SC_HELPER, 0))
		return 0;

	snprintf(id_buf, sizeof(id_buf), "%d", nxi->nx_id);
	snprintf(cmd_buf, sizeof(cmd_buf), "VS_CMD=%08x", cmd);

	switch (cmd) {
	case VSC_NETUP:
		argv[1] = "netup";
		break;
	case VSC_NETDOWN:
		argv[1] = "netdown";
		break;
	default:
		return 0;
	}

	return do_vshelper(vshelper_path, argv, envp, 1);
}

