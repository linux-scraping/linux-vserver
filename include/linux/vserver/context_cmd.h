#ifndef _VSERVER_CONTEXT_CMD_H
#define _VSERVER_CONTEXT_CMD_H

#include <uapi/vserver/context_cmd.h>

extern int vc_task_xid(uint32_t);

extern int vc_vx_info(struct vx_info *, void __user *);

extern int vc_ctx_stat(struct vx_info *, void __user *);

extern int vc_ctx_create(uint32_t, void __user *);
extern int vc_ctx_migrate(struct vx_info *, void __user *);

extern int vc_get_cflags(struct vx_info *, void __user *);
extern int vc_set_cflags(struct vx_info *, void __user *);

extern int vc_get_ccaps(struct vx_info *, void __user *);
extern int vc_set_ccaps(struct vx_info *, void __user *);

extern int vc_get_bcaps(struct vx_info *, void __user *);
extern int vc_set_bcaps(struct vx_info *, void __user *);

extern int vc_get_umask(struct vx_info *, void __user *);
extern int vc_set_umask(struct vx_info *, void __user *);

extern int vc_get_wmask(struct vx_info *, void __user *);
extern int vc_set_wmask(struct vx_info *, void __user *);

extern int vc_get_badness(struct vx_info *, void __user *);
extern int vc_set_badness(struct vx_info *, void __user *);

#endif	/* _VSERVER_CONTEXT_CMD_H */
