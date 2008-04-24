#ifndef _VS_SOCKET_H
#define _VS_SOCKET_H

#include "vserver/debug.h"
#include "vserver/base.h"
#include "vserver/cacct.h"
#include "vserver/context.h"
#include "vserver/tag.h"


/* socket accounting */

#include <linux/socket.h>

static inline int vx_sock_type(int family)
{
	switch (family) {
	case PF_UNSPEC:
		return VXA_SOCK_UNSPEC;
	case PF_UNIX:
		return VXA_SOCK_UNIX;
	case PF_INET:
		return VXA_SOCK_INET;
	case PF_INET6:
		return VXA_SOCK_INET6;
	case PF_PACKET:
		return VXA_SOCK_PACKET;
	default:
		return VXA_SOCK_OTHER;
	}
}

#define vx_acc_sock(v, f, p, s) \
	__vx_acc_sock(v, f, p, s, __FILE__, __LINE__)

static inline void __vx_acc_sock(struct vx_info *vxi,
	int family, int pos, int size, char *file, int line)
{
	if (vxi) {
		int type = vx_sock_type(family);

		atomic_long_inc(&vxi->cacct.sock[type][pos].count);
		atomic_long_add(size, &vxi->cacct.sock[type][pos].total);
	}
}

#define vx_sock_recv(sk, s) \
	vx_acc_sock((sk)->sk_vx_info, (sk)->sk_family, 0, s)
#define vx_sock_send(sk, s) \
	vx_acc_sock((sk)->sk_vx_info, (sk)->sk_family, 1, s)
#define vx_sock_fail(sk, s) \
	vx_acc_sock((sk)->sk_vx_info, (sk)->sk_family, 2, s)


#define sock_vx_init(s) do {		\
	(s)->sk_xid = 0;		\
	(s)->sk_vx_info = NULL;		\
	} while (0)

#define sock_nx_init(s) do {		\
	(s)->sk_nid = 0;		\
	(s)->sk_nx_info = NULL;		\
	} while (0)

#else
#warning duplicate inclusion
#endif
