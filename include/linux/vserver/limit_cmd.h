#ifndef _VSERVER_LIMIT_CMD_H
#define _VSERVER_LIMIT_CMD_H

#include <uapi/vserver/limit_cmd.h>


#ifdef	CONFIG_IA32_EMULATION

struct	vcmd_ctx_rlimit_v0_x32 {
	uint32_t id;
	uint64_t minimum;
	uint64_t softlimit;
	uint64_t maximum;
} __attribute__ ((packed));

#endif	/* CONFIG_IA32_EMULATION */

#include <linux/compiler.h>

extern int vc_get_rlimit_mask(uint32_t, void __user *);
extern int vc_get_rlimit(struct vx_info *, void __user *);
extern int vc_set_rlimit(struct vx_info *, void __user *);
extern int vc_reset_hits(struct vx_info *, void __user *);
extern int vc_reset_minmax(struct vx_info *, void __user *);

extern int vc_rlimit_stat(struct vx_info *, void __user *);

#ifdef	CONFIG_IA32_EMULATION

extern int vc_get_rlimit_x32(struct vx_info *, void __user *);
extern int vc_set_rlimit_x32(struct vx_info *, void __user *);

#endif	/* CONFIG_IA32_EMULATION */

#endif	/* _VSERVER_LIMIT_CMD_H */
