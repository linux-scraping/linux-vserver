#ifndef _VSERVER_SIGNAL_CMD_H
#define _VSERVER_SIGNAL_CMD_H

#include <uapi/vserver/signal_cmd.h>


extern int vc_ctx_kill(struct vx_info *, void __user *);
extern int vc_wait_exit(struct vx_info *, void __user *);


extern int vc_get_pflags(uint32_t pid, void __user *);
extern int vc_set_pflags(uint32_t pid, void __user *);

#endif	/* _VSERVER_SIGNAL_CMD_H */
