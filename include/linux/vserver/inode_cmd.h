#ifndef _VX_INODE_CMD_H
#define _VX_INODE_CMD_H


/*  inode vserver commands */

#define VCMD_get_iattr		VC_CMD(INODE, 1, 1)
#define VCMD_set_iattr		VC_CMD(INODE, 2, 1)

#define VCMD_fget_iattr		VC_CMD(INODE, 3, 0)
#define VCMD_fset_iattr		VC_CMD(INODE, 4, 0)

struct	vcmd_ctx_iattr_v1 {
	const char __user *name;
	uint32_t tag;
	uint32_t flags;
	uint32_t mask;
};

struct	vcmd_ctx_fiattr_v0 {
	uint32_t tag;
	uint32_t flags;
	uint32_t mask;
};


#ifdef	__KERNEL__


#ifdef	CONFIG_COMPAT

#include <asm/compat.h>

struct	vcmd_ctx_iattr_v1_x32 {
	compat_uptr_t name_ptr;
	uint32_t tag;
	uint32_t flags;
	uint32_t mask;
};

#endif	/* CONFIG_COMPAT */

#include <linux/compiler.h>

extern int vc_get_iattr(void __user *);
extern int vc_set_iattr(void __user *);

extern int vc_fget_iattr(uint32_t, void __user *);
extern int vc_fset_iattr(uint32_t, void __user *);

#ifdef	CONFIG_COMPAT

extern int vc_get_iattr_x32(void __user *);
extern int vc_set_iattr_x32(void __user *);

#endif	/* CONFIG_COMPAT */

#endif	/* __KERNEL__ */
#endif	/* _VX_INODE_CMD_H */
