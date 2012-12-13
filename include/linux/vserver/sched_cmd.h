#ifndef _VSERVER_SCHED_CMD_H
#define _VSERVER_SCHED_CMD_H


#include <linux/compiler.h>
#include <uapi/vserver/sched_cmd.h>

extern int vc_set_prio_bias(struct vx_info *, void __user *);
extern int vc_get_prio_bias(struct vx_info *, void __user *);

#endif	/* _VSERVER_SCHED_CMD_H */
