#ifndef _VSERVER_CVIRT_CMD_H
#define _VSERVER_CVIRT_CMD_H


#include <linux/compiler.h>
#include <uapi/vserver/cvirt_cmd.h>

extern int vc_set_vhi_name(struct vx_info *, void __user *);
extern int vc_get_vhi_name(struct vx_info *, void __user *);

extern int vc_virt_stat(struct vx_info *, void __user *);

#endif	/* _VSERVER_CVIRT_CMD_H */
