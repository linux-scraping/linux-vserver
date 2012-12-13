#ifndef _VSERVER_DEVICE_CMD_H
#define _VSERVER_DEVICE_CMD_H

#include <uapi/vserver/device_cmd.h>


#ifdef	CONFIG_COMPAT

#include <asm/compat.h>

struct	vcmd_set_mapping_v0_x32 {
	compat_uptr_t device_ptr;
	compat_uptr_t target_ptr;
	uint32_t flags;
};

#endif	/* CONFIG_COMPAT */

#include <linux/compiler.h>

extern int vc_set_mapping(struct vx_info *, void __user *);
extern int vc_unset_mapping(struct vx_info *, void __user *);

#ifdef	CONFIG_COMPAT

extern int vc_set_mapping_x32(struct vx_info *, void __user *);
extern int vc_unset_mapping_x32(struct vx_info *, void __user *);

#endif	/* CONFIG_COMPAT */

#endif	/* _VSERVER_DEVICE_CMD_H */
