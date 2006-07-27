/*
 *  linux/kernel/vserver/switch.c
 *
 *  Virtual Server: Syscall Switch
 *
 *  Copyright (C) 2003-2005  Herbert Pötzl
 *
 *  V0.01  syscall switch
 *  V0.02  added signal to context
 *  V0.03  added rlimit functions
 *  V0.04  added iattr, task/xid functions
 *  V0.05  added debug/history stuff
 *  V0.06  added compat32 layer
 *
 */

#include <linux/linkage.h>
#include <linux/sched.h>
#include <linux/compat.h>
#include <asm/errno.h>

#include <linux/vserver/network.h>
#include <linux/vserver/switch.h>
#include <linux/vserver/debug.h>


static inline
int vc_get_version(uint32_t id)
{
#ifdef	CONFIG_VSERVER_LEGACY_VERSION
	if (id == 63)
		return VCI_LEGACY_VERSION;
#endif
	return VCI_VERSION;
}

#include <linux/vserver/context_cmd.h>
#include <linux/vserver/cvirt_cmd.h>
#include <linux/vserver/limit_cmd.h>
#include <linux/vserver/network_cmd.h>
#include <linux/vserver/sched_cmd.h>
#include <linux/vserver/debug_cmd.h>
#include <linux/vserver/inode_cmd.h>
#include <linux/vserver/dlimit_cmd.h>
#include <linux/vserver/signal_cmd.h>
#include <linux/vserver/namespace_cmd.h>

#include <linux/vserver/legacy.h>
#include <linux/vserver/inode.h>
#include <linux/vserver/dlimit.h>


#ifdef	CONFIG_COMPAT
#define __COMPAT(name, id, data, compat)	\
	(compat) ? name ## _x32 (id, data) : name (id, data)
#else
#define __COMPAT(name, id, data, compat)	\
	name (id, data)
#endif


static inline
long do_vserver(uint32_t cmd, uint32_t id, void __user *data, int compat)
{
	vxdprintk(VXD_CBIT(switch, 0),
		"vc: VCMD_%02d_%d[%d], %d,%p,%d",
		VC_CATEGORY(cmd), VC_COMMAND(cmd),
		VC_VERSION(cmd), id, data, compat);

#ifdef	CONFIG_VSERVER_LEGACY
	if (!capable(CAP_CONTEXT) &&
		/* dirty hack for capremove */
		!(cmd==VCMD_new_s_context && id==-2))
		return -EPERM;
#else
	if (!capable(CAP_CONTEXT))
		return -EPERM;
#endif

	switch (cmd) {
	case VCMD_get_version:
		return vc_get_version(id);

	case VCMD_dump_history:
#ifdef	CONFIG_VSERVER_HISTORY
		return vc_dump_history(id);
#else
		return -ENOSYS;
#endif

#ifdef	CONFIG_VSERVER_LEGACY
	case VCMD_new_s_context:
		return vc_new_s_context(id, data);
#endif
#ifdef	CONFIG_VSERVER_LEGACYNET
	case VCMD_set_ipv4root:
		return vc_set_ipv4root(id, data);
#endif

	case VCMD_task_xid:
		return vc_task_xid(id, data);
	case VCMD_vx_info:
		return vc_vx_info(id, data);

	case VCMD_task_nid:
		return vc_task_nid(id, data);
	case VCMD_nx_info:
		return vc_nx_info(id, data);

	case VCMD_set_namespace_v0:
		return vc_set_namespace(-1, data);
	case VCMD_set_namespace:
		return vc_set_namespace(id, data);
	}

	/* those are allowed while in setup too */
	if (!vx_check(0, VX_ADMIN|VX_WATCH) &&
		!vx_flags(VXF_STATE_SETUP,0))
		return -EPERM;

#ifdef	CONFIG_VSERVER_LEGACY
	switch (cmd) {
	case VCMD_set_cflags:
	case VCMD_set_ccaps:
		if (vx_check(0, VX_WATCH))
			return 0;
	}
#endif

	switch (cmd) {
#ifdef	CONFIG_IA32_EMULATION
	case VCMD_get_rlimit:
		return __COMPAT(vc_get_rlimit, id, data, compat);
	case VCMD_set_rlimit:
		return __COMPAT(vc_set_rlimit, id, data, compat);
#else
	case VCMD_get_rlimit:
		return vc_get_rlimit(id, data);
	case VCMD_set_rlimit:
		return vc_set_rlimit(id, data);
#endif
	case VCMD_get_rlimit_mask:
		return vc_get_rlimit_mask(id, data);

	case VCMD_get_vhi_name:
		return vc_get_vhi_name(id, data);
	case VCMD_set_vhi_name:
		return vc_set_vhi_name(id, data);

	case VCMD_set_cflags:
		return vc_set_cflags(id, data);
	case VCMD_get_cflags:
		return vc_get_cflags(id, data);

	case VCMD_set_ccaps:
		return vc_set_ccaps(id, data);
	case VCMD_get_ccaps:
		return vc_get_ccaps(id, data);

	case VCMD_set_nflags:
		return vc_set_nflags(id, data);
	case VCMD_get_nflags:
		return vc_get_nflags(id, data);

	case VCMD_set_ncaps:
		return vc_set_ncaps(id, data);
	case VCMD_get_ncaps:
		return vc_get_ncaps(id, data);

	case VCMD_set_sched_v2:
		return vc_set_sched_v2(id, data);
	/* this is version 3 */
	case VCMD_set_sched:
		return vc_set_sched(id, data);

	case VCMD_add_dlimit:
		return __COMPAT(vc_add_dlimit, id, data, compat);
	case VCMD_rem_dlimit:
		return __COMPAT(vc_rem_dlimit, id, data, compat);
	case VCMD_set_dlimit:
		return __COMPAT(vc_set_dlimit, id, data, compat);
	case VCMD_get_dlimit:
		return __COMPAT(vc_get_dlimit, id, data, compat);
	}

	/* below here only with VX_ADMIN */
	if (!vx_check(0, VX_ADMIN|VX_WATCH))
		return -EPERM;

	switch (cmd) {
	case VCMD_ctx_kill:
		return vc_ctx_kill(id, data);

	case VCMD_wait_exit:
		return vc_wait_exit(id, data);

	case VCMD_create_context:
#ifdef	CONFIG_VSERVER_LEGACY
		return vc_ctx_create(id, NULL);
#else
		return -ENOSYS;
#endif

	case VCMD_get_iattr:
		return __COMPAT(vc_get_iattr, id, data, compat);
	case VCMD_set_iattr:
		return __COMPAT(vc_set_iattr, id, data, compat);

	case VCMD_enter_namespace:
		return vc_enter_namespace(id, data);

	case VCMD_ctx_create_v0:
#ifdef	CONFIG_VSERVER_LEGACY
		if (id == 1) {
			current->xid = 1;
			return 1;
		}
#endif
		return vc_ctx_create(id, NULL);
	case VCMD_ctx_create:
		return vc_ctx_create(id, data);
	case VCMD_ctx_migrate_v0:
		return vc_ctx_migrate(id, NULL);
	case VCMD_ctx_migrate:
		return vc_ctx_migrate(id, data);

	case VCMD_net_create_v0:
		return vc_net_create(id, NULL);
	case VCMD_net_create:
		return vc_net_create(id, data);
	case VCMD_net_migrate:
		return vc_net_migrate(id, data);
	case VCMD_net_add:
		return vc_net_add(id, data);
	case VCMD_net_remove:
		return vc_net_remove(id, data);

	}
	return -ENOSYS;
}

extern asmlinkage long
sys_vserver(uint32_t cmd, uint32_t id, void __user *data)
{
	long ret = do_vserver(cmd, id, data, 0);

	vxdprintk(VXD_CBIT(switch, 1),
		"vc: VCMD_%02d_%d[%d] = %08lx(%ld)",
		VC_CATEGORY(cmd), VC_COMMAND(cmd),
		VC_VERSION(cmd), ret, ret);
	return ret;
}

#ifdef	CONFIG_COMPAT

extern asmlinkage long
sys32_vserver(uint32_t cmd, uint32_t id, void __user *data)
{
	long ret = do_vserver(cmd, id, data, 1);

	vxdprintk(VXD_CBIT(switch, 1),
		"vc: VCMD_%02d_%d[%d] = %08lx(%ld)",
		VC_CATEGORY(cmd), VC_COMMAND(cmd),
		VC_VERSION(cmd), ret, ret);
	return ret;
}

#endif	/* CONFIG_COMPAT */
