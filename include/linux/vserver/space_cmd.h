#ifndef _VSERVER_SPACE_CMD_H
#define _VSERVER_SPACE_CMD_H

#include <uapi/vserver/space_cmd.h>


extern int vc_enter_space_v1(struct vx_info *, void __user *);
extern int vc_set_space_v1(struct vx_info *, void __user *);
extern int vc_enter_space(struct vx_info *, void __user *);
extern int vc_set_space(struct vx_info *, void __user *);
extern int vc_get_space_mask(void __user *, int);

#endif	/* _VSERVER_SPACE_CMD_H */
