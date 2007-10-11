#ifndef _VX_DEVICE_CMD_H
#define _VX_DEVICE_CMD_H


/*  device vserver commands */

#define VCMD_set_mapping	VC_CMD(DEVICE, 1, 0)
#define VCMD_unset_mapping	VC_CMD(DEVICE, 2, 0)

struct	vcmd_set_mapping_v0 {
	const char __user *device;
	const char __user *target;
	uint32_t flags;
};


#ifdef	__KERNEL__

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

#endif	/* __KERNEL__ */
#endif	/* _VX_DEVICE_CMD_H */
