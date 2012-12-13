#ifndef _VSERVER_INODE_CMD_H
#define _VSERVER_INODE_CMD_H

#include <uapi/vserver/inode_cmd.h>



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

#endif	/* _VSERVER_INODE_CMD_H */
