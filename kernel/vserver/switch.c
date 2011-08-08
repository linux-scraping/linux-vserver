/*
 *  linux/kernel/vserver/switch.c
 *
 *  Virtual Server: Syscall Switch
 *
 *  Copyright (C) 2003-2011  Herbert Pötzl
 *
 *  V0.01  syscall switch
 *  V0.02  added signal to context
 *  V0.03  added rlimit functions
 *  V0.04  added iattr, task/xid functions
 *  V0.05  added debug/history stuff
 *  V0.06  added compat32 layer
 *  V0.07  vcmd args and perms
 *  V0.08  added status commands
 *  V0.09  added tag commands
 *  V0.10  added oom bias
 *  V0.11  added device commands
 *  V0.12  added warn mask
 *
 */

#include <linux/vs_context.h>
#include <linux/vs_network.h>
#include <linux/vserver/switch.h>

#include "vci_config.h"


static inline
int vc_get_version(uint32_t id)
{
	return VCI_VERSION;
}

static inline
int vc_get_vci(uint32_t id)
{
	return vci_kernel_config();
}

#include <linux/vserver/context_cmd.h>
#include <linux/vserver/cvirt_cmd.h>
#include <linux/vserver/cacct_cmd.h>
#include <linux/vserver/limit_cmd.h>
#include <linux/vserver/network_cmd.h>
#include <linux/vserver/sched_cmd.h>
#include <linux/vserver/debug_cmd.h>
#include <linux/vserver/inode_cmd.h>
#include <linux/vserver/dlimit_cmd.h>
#include <linux/vserver/signal_cmd.h>
#include <linux/vserver/space_cmd.h>
#include <linux/vserver/tag_cmd.h>
#include <linux/vserver/device_cmd.h>

#include <linux/vserver/inode.h>
#include <linux/vserver/dlimit.h>


#ifdef	CONFIG_COMPAT
#define __COMPAT(name, id, data, compat)	\
	(compat) ? name ## _x32(id, data) : name(id, data)
#define __COMPAT_NO_ID(name, data, compat)	\
	(compat) ? name ## _x32(data) : name(data)
#else
#define __COMPAT(name, id, data, compat)	\
	name(id, data)
#define __COMPAT_NO_ID(name, data, compat)	\
	name(data)
#endif


static inline
long do_vcmd(uint32_t cmd, uint32_t id,
	struct vx_info *vxi, struct nx_info *nxi,
	void __user *data, int compat)
{
	switch (cmd) {

	case VCMD_get_version:
		return vc_get_version(id);
	case VCMD_get_vci:
		return vc_get_vci(id);

	case VCMD_task_xid:
		return vc_task_xid(id);
	case VCMD_vx_info:
		return vc_vx_info(vxi, data);

	case VCMD_task_nid:
		return vc_task_nid(id);
	case VCMD_nx_info:
		return vc_nx_info(nxi, data);

	case VCMD_task_tag:
		return vc_task_tag(id);

	case VCMD_set_space_v1:
		return vc_set_space_v1(vxi, data);
	/* this is version 2 */
	case VCMD_set_space:
		return vc_set_space(vxi, data);

	case VCMD_get_space_mask_v0:
		return vc_get_space_mask(data, 0);
	/* this is version 1 */
	case VCMD_get_space_mask:
		return vc_get_space_mask(data, 1);

	case VCMD_get_space_default:
		return vc_get_space_mask(data, -1);

	case VCMD_set_umask:
		return vc_set_umask(vxi, data);

	case VCMD_get_umask:
		return vc_get_umask(vxi, data);

	case VCMD_set_wmask:
		return vc_set_wmask(vxi, data);

	case VCMD_get_wmask:
		return vc_get_wmask(vxi, data);
#ifdef	CONFIG_IA32_EMULATION
	case VCMD_get_rlimit:
		return __COMPAT(vc_get_rlimit, vxi, data, compat);
	case VCMD_set_rlimit:
		return __COMPAT(vc_set_rlimit, vxi, data, compat);
#else
	case VCMD_get_rlimit:
		return vc_get_rlimit(vxi, data);
	case VCMD_set_rlimit:
		return vc_set_rlimit(vxi, data);
#endif
	case VCMD_get_rlimit_mask:
		return vc_get_rlimit_mask(id, data);
	case VCMD_reset_hits:
		return vc_reset_hits(vxi, data);
	case VCMD_reset_minmax:
		return vc_reset_minmax(vxi, data);

	case VCMD_get_vhi_name:
		return vc_get_vhi_name(vxi, data);
	case VCMD_set_vhi_name:
		return vc_set_vhi_name(vxi, data);

	case VCMD_ctx_stat:
		return vc_ctx_stat(vxi, data);
	case VCMD_virt_stat:
		return vc_virt_stat(vxi, data);
	case VCMD_sock_stat:
		return vc_sock_stat(vxi, data);
	case VCMD_rlimit_stat:
		return vc_rlimit_stat(vxi, data);

	case VCMD_set_cflags:
		return vc_set_cflags(vxi, data);
	case VCMD_get_cflags:
		return vc_get_cflags(vxi, data);

	/* this is version 1 */
	case VCMD_set_ccaps:
		return vc_set_ccaps(vxi, data);
	/* this is version 1 */
	case VCMD_get_ccaps:
		return vc_get_ccaps(vxi, data);
	case VCMD_set_bcaps:
		return vc_set_bcaps(vxi, data);
	case VCMD_get_bcaps:
		return vc_get_bcaps(vxi, data);

	case VCMD_set_badness:
		return vc_set_badness(vxi, data);
	case VCMD_get_badness:
		return vc_get_badness(vxi, data);

	case VCMD_set_nflags:
		return vc_set_nflags(nxi, data);
	case VCMD_get_nflags:
		return vc_get_nflags(nxi, data);

	case VCMD_set_ncaps:
		return vc_set_ncaps(nxi, data);
	case VCMD_get_ncaps:
		return vc_get_ncaps(nxi, data);

	case VCMD_set_prio_bias:
		return vc_set_prio_bias(vxi, data);
	case VCMD_get_prio_bias:
		return vc_get_prio_bias(vxi, data);
	case VCMD_add_dlimit:
		return __COMPAT(vc_add_dlimit, id, data, compat);
	case VCMD_rem_dlimit:
		return __COMPAT(vc_rem_dlimit, id, data, compat);
	case VCMD_set_dlimit:
		return __COMPAT(vc_set_dlimit, id, data, compat);
	case VCMD_get_dlimit:
		return __COMPAT(vc_get_dlimit, id, data, compat);

	case VCMD_ctx_kill:
		return vc_ctx_kill(vxi, data);

	case VCMD_wait_exit:
		return vc_wait_exit(vxi, data);

	case VCMD_get_iattr:
		return __COMPAT_NO_ID(vc_get_iattr, data, compat);
	case VCMD_set_iattr:
		return __COMPAT_NO_ID(vc_set_iattr, data, compat);

	case VCMD_fget_iattr:
		return vc_fget_iattr(id, data);
	case VCMD_fset_iattr:
		return vc_fset_iattr(id, data);

	case VCMD_enter_space_v0:
		return vc_enter_space_v1(vxi, NULL);
	case VCMD_enter_space_v1:
		return vc_enter_space_v1(vxi, data);
	/* this is version 2 */
	case VCMD_enter_space:
		return vc_enter_space(vxi, data);

	case VCMD_ctx_create_v0:
		return vc_ctx_create(id, NULL);
	case VCMD_ctx_create:
		return vc_ctx_create(id, data);
	case VCMD_ctx_migrate_v0:
		return vc_ctx_migrate(vxi, NULL);
	case VCMD_ctx_migrate:
		return vc_ctx_migrate(vxi, data);

	case VCMD_net_create_v0:
		return vc_net_create(id, NULL);
	case VCMD_net_create:
		return vc_net_create(id, data);
	case VCMD_net_migrate:
		return vc_net_migrate(nxi, data);

	case VCMD_tag_migrate:
		return vc_tag_migrate(id);

	case VCMD_net_add:
		return vc_net_add(nxi, data);
	case VCMD_net_remove:
		return vc_net_remove(nxi, data);

	case VCMD_net_add_ipv4_v1:
		return vc_net_add_ipv4_v1(nxi, data);
	/* this is version 2 */
	case VCMD_net_add_ipv4:
		return vc_net_add_ipv4(nxi, data);

	case VCMD_net_rem_ipv4_v1:
		return vc_net_rem_ipv4_v1(nxi, data);
	/* this is version 2 */
	case VCMD_net_rem_ipv4:
		return vc_net_rem_ipv4(nxi, data);
#ifdef	CONFIG_IPV6
	case VCMD_net_add_ipv6:
		return vc_net_add_ipv6(nxi, data);
	case VCMD_net_remove_ipv6:
		return vc_net_remove_ipv6(nxi, data);
#endif
/*	case VCMD_add_match_ipv4:
		return vc_add_match_ipv4(nxi, data);
	case VCMD_get_match_ipv4:
		return vc_get_match_ipv4(nxi, data);
#ifdef	CONFIG_IPV6
	case VCMD_add_match_ipv6:
		return vc_add_match_ipv6(nxi, data);
	case VCMD_get_match_ipv6:
		return vc_get_match_ipv6(nxi, data);
#endif	*/

#ifdef	CONFIG_VSERVER_DEVICE
	case VCMD_set_mapping:
		return __COMPAT(vc_set_mapping, vxi, data, compat);
	case VCMD_unset_mapping:
		return __COMPAT(vc_unset_mapping, vxi, data, compat);
#endif
#ifdef	CONFIG_VSERVER_HISTORY
	case VCMD_dump_history:
		return vc_dump_history(id);
	case VCMD_read_history:
		return __COMPAT(vc_read_history, id, data, compat);
#endif
	default:
		vxwprintk_task(1, "unimplemented VCMD_%02d_%d[%d]",
			VC_CATEGORY(cmd), VC_COMMAND(cmd), VC_VERSION(cmd));
	}
	return -ENOSYS;
}


#define	__VCMD(vcmd, _perm, _args, _flags)		\
	case VCMD_ ## vcmd: perm = _perm;		\
		args = _args; flags = _flags; break


#define VCA_NONE	0x00
#define VCA_VXI		0x01
#define VCA_NXI		0x02

#define VCF_NONE	0x00
#define VCF_INFO	0x01
#define VCF_ADMIN	0x02
#define VCF_ARES	0x06	/* includes admin */
#define VCF_SETUP	0x08

#define VCF_ZIDOK	0x10	/* zero id okay */


static inline
long do_vserver(uint32_t cmd, uint32_t id, void __user *data, int compat)
{
	long ret;
	int permit = -1, state = 0;
	int perm = -1, args = 0, flags = 0;
	struct vx_info *vxi = NULL;
	struct nx_info *nxi = NULL;

	switch (cmd) {
	/* unpriviledged commands */
	__VCMD(get_version,	 0, VCA_NONE,	0);
	__VCMD(get_vci,		 0, VCA_NONE,	0);
	__VCMD(get_rlimit_mask,	 0, VCA_NONE,	0);
	__VCMD(get_space_mask_v0,0, VCA_NONE,   0);
	__VCMD(get_space_mask,	 0, VCA_NONE,   0);
	__VCMD(get_space_default,0, VCA_NONE,   0);

	/* info commands */
	__VCMD(task_xid,	 2, VCA_NONE,	0);
	__VCMD(reset_hits,	 2, VCA_VXI,	0);
	__VCMD(reset_minmax,	 2, VCA_VXI,	0);
	__VCMD(vx_info,		 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_bcaps,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_ccaps,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_cflags,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_umask,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_wmask,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_badness,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_vhi_name,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(get_rlimit,	 3, VCA_VXI,	VCF_INFO);

	__VCMD(ctx_stat,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(virt_stat,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(sock_stat,	 3, VCA_VXI,	VCF_INFO);
	__VCMD(rlimit_stat,	 3, VCA_VXI,	VCF_INFO);

	__VCMD(task_nid,	 2, VCA_NONE,	0);
	__VCMD(nx_info,		 3, VCA_NXI,	VCF_INFO);
	__VCMD(get_ncaps,	 3, VCA_NXI,	VCF_INFO);
	__VCMD(get_nflags,	 3, VCA_NXI,	VCF_INFO);

	__VCMD(task_tag,	 2, VCA_NONE,	0);

	__VCMD(get_iattr,	 2, VCA_NONE,	0);
	__VCMD(fget_iattr,	 2, VCA_NONE,	0);
	__VCMD(get_dlimit,	 3, VCA_NONE,	VCF_INFO);
	__VCMD(get_prio_bias,	 3, VCA_VXI,	VCF_INFO);

	/* lower admin commands */
	__VCMD(wait_exit,	 4, VCA_VXI,	VCF_INFO);
	__VCMD(ctx_create_v0,	 5, VCA_NONE,	0);
	__VCMD(ctx_create,	 5, VCA_NONE,	0);
	__VCMD(ctx_migrate_v0,	 5, VCA_VXI,	VCF_ADMIN);
	__VCMD(ctx_migrate,	 5, VCA_VXI,	VCF_ADMIN);
	__VCMD(enter_space_v0,	 5, VCA_VXI,	VCF_ADMIN);
	__VCMD(enter_space_v1,	 5, VCA_VXI,	VCF_ADMIN);
	__VCMD(enter_space,	 5, VCA_VXI,	VCF_ADMIN);

	__VCMD(net_create_v0,	 5, VCA_NONE,	0);
	__VCMD(net_create,	 5, VCA_NONE,	0);
	__VCMD(net_migrate,	 5, VCA_NXI,	VCF_ADMIN);

	__VCMD(tag_migrate,	 5, VCA_NONE,	VCF_ADMIN);

	/* higher admin commands */
	__VCMD(ctx_kill,	 6, VCA_VXI,	VCF_ARES);
	__VCMD(set_space_v1,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_space,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);

	__VCMD(set_ccaps,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_bcaps,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_cflags,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_umask,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_wmask,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_badness,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);

	__VCMD(set_vhi_name,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_rlimit,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_prio_bias,	 7, VCA_VXI,	VCF_ARES | VCF_SETUP);

	__VCMD(set_ncaps,	 7, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(set_nflags,	 7, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(net_add,		 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(net_remove,	 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(net_add_ipv4_v1,	 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(net_rem_ipv4_v1,	 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(net_add_ipv4,	 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(net_rem_ipv4,	 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
#ifdef	CONFIG_IPV6
	__VCMD(net_add_ipv6,	 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
	__VCMD(net_remove_ipv6,	 8, VCA_NXI,	VCF_ARES | VCF_SETUP);
#endif
	__VCMD(set_iattr,	 7, VCA_NONE,	0);
	__VCMD(fset_iattr,	 7, VCA_NONE,	0);
	__VCMD(set_dlimit,	 7, VCA_NONE,	VCF_ARES);
	__VCMD(add_dlimit,	 8, VCA_NONE,	VCF_ARES);
	__VCMD(rem_dlimit,	 8, VCA_NONE,	VCF_ARES);

#ifdef	CONFIG_VSERVER_DEVICE
	__VCMD(set_mapping,	 8, VCA_VXI,    VCF_ARES|VCF_ZIDOK);
	__VCMD(unset_mapping,	 8, VCA_VXI,	VCF_ARES|VCF_ZIDOK);
#endif
	/* debug level admin commands */
#ifdef	CONFIG_VSERVER_HISTORY
	__VCMD(dump_history,	 9, VCA_NONE,	0);
	__VCMD(read_history,	 9, VCA_NONE,	0);
#endif

	default:
		perm = -1;
	}

	vxdprintk(VXD_CBIT(switch, 0),
		"vc: VCMD_%02d_%d[%d], %d,%p [%d,%d,%x,%x]",
		VC_CATEGORY(cmd), VC_COMMAND(cmd),
		VC_VERSION(cmd), id, data, compat,
		perm, args, flags);

	ret = -ENOSYS;
	if (perm < 0)
		goto out;

	state = 1;
	if (!capable(CAP_CONTEXT))
		goto out;

	state = 2;
	/* moved here from the individual commands */
	ret = -EPERM;
	if ((perm > 1) && !capable(CAP_SYS_ADMIN))
		goto out;

	state = 3;
	/* vcmd involves resource management  */
	ret = -EPERM;
	if ((flags & VCF_ARES) && !capable(CAP_SYS_RESOURCE))
		goto out;

	state = 4;
	/* various legacy exceptions */
	switch (cmd) {
	/* will go away when spectator is a cap */
	case VCMD_ctx_migrate_v0:
	case VCMD_ctx_migrate:
		if (id == 1) {
			current->xid = 1;
			ret = 1;
			goto out;
		}
		break;

	/* will go away when spectator is a cap */
	case VCMD_net_migrate:
		if (id == 1) {
			current->nid = 1;
			ret = 1;
			goto out;
		}
		break;
	}

	/* vcmds are fine by default */
	permit = 1;

	/* admin type vcmds require admin ... */
	if (flags & VCF_ADMIN)
		permit = vx_check(0, VS_ADMIN) ? 1 : 0;

	/* ... but setup type vcmds override that */
	if (!permit && (flags & VCF_SETUP))
		permit = vx_flags(VXF_STATE_SETUP, 0) ? 2 : 0;

	state = 5;
	ret = -EPERM;
	if (!permit)
		goto out;

	state = 6;
	if (!id && (flags & VCF_ZIDOK))
		goto skip_id;

	ret = -ESRCH;
	if (args & VCA_VXI) {
		vxi = lookup_vx_info(id);
		if (!vxi)
			goto out;

		if ((flags & VCF_ADMIN) &&
			/* special case kill for shutdown */
			(cmd != VCMD_ctx_kill) &&
			/* can context be administrated? */
			!vx_info_flags(vxi, VXF_STATE_ADMIN, 0)) {
			ret = -EACCES;
			goto out_vxi;
		}
	}
	state = 7;
	if (args & VCA_NXI) {
		nxi = lookup_nx_info(id);
		if (!nxi)
			goto out_vxi;

		if ((flags & VCF_ADMIN) &&
			/* can context be administrated? */
			!nx_info_flags(nxi, NXF_STATE_ADMIN, 0)) {
			ret = -EACCES;
			goto out_nxi;
		}
	}
skip_id:
	state = 8;
	ret = do_vcmd(cmd, id, vxi, nxi, data, compat);

out_nxi:
	if ((args & VCA_NXI) && nxi)
		put_nx_info(nxi);
out_vxi:
	if ((args & VCA_VXI) && vxi)
		put_vx_info(vxi);
out:
	vxdprintk(VXD_CBIT(switch, 1),
		"vc: VCMD_%02d_%d[%d] = %08lx(%ld) [%d,%d]",
		VC_CATEGORY(cmd), VC_COMMAND(cmd),
		VC_VERSION(cmd), ret, ret, state, permit);
	return ret;
}

asmlinkage long
sys_vserver(uint32_t cmd, uint32_t id, void __user *data)
{
	return do_vserver(cmd, id, data, 0);
}

#ifdef	CONFIG_COMPAT

asmlinkage long
sys32_vserver(uint32_t cmd, uint32_t id, void __user *data)
{
	return do_vserver(cmd, id, data, 1);
}

#endif	/* CONFIG_COMPAT */
