#ifndef _VX_CACCT_CMD_H
#define _VX_CACCT_CMD_H


/* virtual host info name commands */

#define VCMD_sock_stat		VC_CMD(VSTAT, 5, 0)

struct	vcmd_sock_stat_v0 {
	uint32_t field;
	uint32_t count;
	uint64_t total;
};


enum sock_stat_field {
	SOCK_UNSPEC=0,
	SOCK_UNIX,
	SOCK_INET,
	SOCK_INET6,
	SOCK_PACKET,
	SOCK_OTHER,
};


#ifdef	__KERNEL__

#include <linux/compiler.h>

extern int vc_sock_stat(struct vx_info *, void __user *);

#endif	/* __KERNEL__ */
#endif	/* _VX_CACCT_CMD_H */
