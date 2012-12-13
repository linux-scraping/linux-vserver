#ifndef _VSERVER_DLIMIT_CMD_H
#define _VSERVER_DLIMIT_CMD_H

#include <uapi/vserver/dlimit_cmd.h>


#ifdef	CONFIG_COMPAT

#include <asm/compat.h>

struct	vcmd_ctx_dlimit_base_v0_x32 {
	compat_uptr_t name_ptr;
	uint32_t flags;
};

struct	vcmd_ctx_dlimit_v0_x32 {
	compat_uptr_t name_ptr;
	uint32_t space_used;			/* used space in kbytes */
	uint32_t space_total;			/* maximum space in kbytes */
	uint32_t inodes_used;			/* used inodes */
	uint32_t inodes_total;			/* maximum inodes */
	uint32_t reserved;			/* reserved for root in % */
	uint32_t flags;
};

#endif	/* CONFIG_COMPAT */

#include <linux/compiler.h>

extern int vc_add_dlimit(uint32_t, void __user *);
extern int vc_rem_dlimit(uint32_t, void __user *);

extern int vc_set_dlimit(uint32_t, void __user *);
extern int vc_get_dlimit(uint32_t, void __user *);

#ifdef	CONFIG_COMPAT

extern int vc_add_dlimit_x32(uint32_t, void __user *);
extern int vc_rem_dlimit_x32(uint32_t, void __user *);

extern int vc_set_dlimit_x32(uint32_t, void __user *);
extern int vc_get_dlimit_x32(uint32_t, void __user *);

#endif	/* CONFIG_COMPAT */

#endif	/* _VSERVER_DLIMIT_CMD_H */
