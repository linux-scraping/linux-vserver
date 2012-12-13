#ifndef _VSERVER_CACCT_CMD_H
#define _VSERVER_CACCT_CMD_H


#include <linux/compiler.h>
#include <uapi/vserver/cacct_cmd.h>

extern int vc_sock_stat(struct vx_info *, void __user *);

#endif	/* _VSERVER_CACCT_CMD_H */
