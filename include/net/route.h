/*
 * INET		An implementation of the TCP/IP protocol suite for the LINUX
 *		operating system.  INET  is implemented using the  BSD Socket
 *		interface as the means of communication with the user level.
 *
 *		Definitions for the IP router.
 *
 * Version:	@(#)route.h	1.0.4	05/27/93
 *
 * Authors:	Ross Biro
 *		Fred N. van Kempen, <waltje@uWalt.NL.Mugnet.ORG>
 * Fixes:
 *		Alan Cox	:	Reformatted. Added ip_rt_local()
 *		Alan Cox	:	Support for TCP parameters.
 *		Alexey Kuznetsov:	Major changes for new routing code.
 *		Mike McLagan    :	Routing by source
 *		Robert Olsson   :	Added rt_cache statistics
 *
 *		This program is free software; you can redistribute it and/or
 *		modify it under the terms of the GNU General Public License
 *		as published by the Free Software Foundation; either version
 *		2 of the License, or (at your option) any later version.
 */
#ifndef _ROUTE_H
#define _ROUTE_H

#include <net/dst.h>
#include <net/inetpeer.h>
#include <net/flow.h>
#include <net/inet_sock.h>
#include <linux/in_route.h>
#include <linux/rtnetlink.h>
#include <linux/route.h>
#include <linux/ip.h>
#include <linux/cache.h>
#include <linux/security.h>
#include <linux/vs_base.h>
#include <linux/vs_network.h>
#include <linux/in.h>

#ifndef __KERNEL__
#warning This file is not supposed to be used outside of kernel.
#endif

#define RTO_ONLINK	0x01

#define RTO_CONN	0
/* RTO_CONN is not used (being alias for 0), but preserved not to break
 * some modules referring to it. */

#define RT_CONN_FLAGS(sk)   (RT_TOS(inet_sk(sk)->tos) | sock_flag(sk, SOCK_LOCALROUTE))

struct fib_nh;
struct inet_peer;
struct rtable
{
	union
	{
		struct dst_entry	dst;
		struct rtable		*rt_next;
	} u;

	struct in_device	*idev;
	
	unsigned		rt_flags;
	__u16			rt_type;
	__u16			rt_multipath_alg;

	__be32			rt_dst;	/* Path destination	*/
	__be32			rt_src;	/* Path source		*/
	int			rt_iif;

	/* Info on neighbour */
	__be32			rt_gateway;

	/* Cache lookup keys */
	struct flowi		fl;

	/* Miscellaneous cached information */
	__be32			rt_spec_dst; /* RFC1122 specific destination */
	struct inet_peer	*peer; /* long-living peer info */
};

struct ip_rt_acct
{
	__u32 	o_bytes;
	__u32 	o_packets;
	__u32 	i_bytes;
	__u32 	i_packets;
};

struct rt_cache_stat 
{
        unsigned int in_hit;
        unsigned int in_slow_tot;
        unsigned int in_slow_mc;
        unsigned int in_no_route;
        unsigned int in_brd;
        unsigned int in_martian_dst;
        unsigned int in_martian_src;
        unsigned int out_hit;
        unsigned int out_slow_tot;
        unsigned int out_slow_mc;
        unsigned int gc_total;
        unsigned int gc_ignored;
        unsigned int gc_goal_miss;
        unsigned int gc_dst_overflow;
        unsigned int in_hlist_search;
        unsigned int out_hlist_search;
};

extern struct ip_rt_acct *ip_rt_acct;

struct in_device;
extern int		ip_rt_init(void);
extern void		ip_rt_redirect(__be32 old_gw, __be32 dst, __be32 new_gw,
				       __be32 src, struct net_device *dev);
extern void		ip_rt_advice(struct rtable **rp, int advice);
extern void		rt_cache_flush(int how);
extern int		__ip_route_output_key(struct rtable **, const struct flowi *flp);
extern int		ip_route_output_key(struct rtable **, struct flowi *flp);
extern int		ip_route_output_flow(struct rtable **rp, struct flowi *flp, struct sock *sk, int flags);
extern int		ip_route_input(struct sk_buff*, __be32 dst, __be32 src, u8 tos, struct net_device *devin);
extern unsigned short	ip_rt_frag_needed(struct iphdr *iph, unsigned short new_mtu);
extern void		ip_rt_send_redirect(struct sk_buff *skb);

extern unsigned		inet_addr_type(__be32 addr);
extern void		ip_rt_multicast_event(struct in_device *);
extern int		ip_rt_ioctl(unsigned int cmd, void __user *arg);
extern void		ip_rt_get_source(u8 *src, struct rtable *rt);
extern int		ip_rt_dump(struct sk_buff *skb,  struct netlink_callback *cb);

struct in_ifaddr;
extern void fib_add_ifaddr(struct in_ifaddr *);

static inline void ip_rt_put(struct rtable * rt)
{
	if (rt)
		dst_release(&rt->u.dst);
}

#define IPTOS_RT_MASK	(IPTOS_TOS_MASK & ~3)

extern __u8 ip_tos2prio[16];

static inline char rt_tos2priority(u8 tos)
{
	return ip_tos2prio[IPTOS_TOS(tos)>>1];
}

#define IPI_LOOPBACK	htonl(INADDR_LOOPBACK)

static inline int ip_find_src(struct nx_info *nxi, struct rtable **rp, struct flowi *fl)
{
	int err;
	int i, n = nxi->nbipv4;
	u32 ipv4root = nxi->ipv4[0];

	if (ipv4root == 0)
		return 0;

	if (fl->fl4_src == 0) {
		if (n > 1) {
			u32 foundsrc;

			err = __ip_route_output_key(rp, fl);
			if (err) {
				fl->fl4_src = ipv4root;
				err = __ip_route_output_key(rp, fl);
			}
			if (err)
				return err;

			foundsrc = (*rp)->rt_src;
			ip_rt_put(*rp);

			for (i=0; i<n; i++){
				u32 mask = nxi->mask[i];
				u32 ipv4 = nxi->ipv4[i];
				u32 net4 = ipv4 & mask;

				if (foundsrc == ipv4) {
					fl->fl4_src = ipv4;
					break;
				}
				if (!fl->fl4_src && (foundsrc & mask) == net4)
					fl->fl4_src = ipv4;
			}
		}
		if (fl->fl4_src == 0)
			fl->fl4_src = (fl->fl4_dst == IPI_LOOPBACK)
				? IPI_LOOPBACK : ipv4root;
	} else {
		for (i=0; i<n; i++) {
			if (nxi->ipv4[i] == fl->fl4_src)
				break;
		}
		if (i == n)
			return -EPERM;
	}
	return 0;
}

static inline int ip_route_connect(struct rtable **rp, __be32 dst,
				   __be32 src, u32 tos, int oif, u8 protocol,
				   __be16 sport, __be16 dport, struct sock *sk)
{
	struct flowi fl = { .oif = oif,
			    .nl_u = { .ip4_u = { .daddr = dst,
						 .saddr = src,
						 .tos   = tos } },
			    .proto = protocol,
			    .uli_u = { .ports =
				       { .sport = sport,
					 .dport = dport } } };

	int err;
	struct nx_info *nx_info = current->nx_info;

	if (sk)
		nx_info = sk->sk_nx_info;
	vxdprintk(VXD_CBIT(net, 4),
		"ip_route_connect(%p) %p,%p;%lx",
		sk, nx_info, sk->sk_socket,
		(sk->sk_socket?sk->sk_socket->flags:0));

	if (nx_info) {
		err = ip_find_src(nx_info, rp, &fl);
		if (err)
			return err;
		if (fl.fl4_dst == IPI_LOOPBACK && !vx_check(0, VS_ADMIN))
			fl.fl4_dst = nx_info->ipv4[0];
#ifdef CONFIG_VSERVER_REMAP_SADDR
		if (fl.fl4_src == IPI_LOOPBACK && !vx_check(0, VS_ADMIN))
			fl.fl4_src = nx_info->ipv4[0];
#endif
	}
	if (!fl.fl4_dst || !fl.fl4_src) {
		err = __ip_route_output_key(rp, &fl);
		if (err)
			return err;
		fl.fl4_dst = (*rp)->rt_dst;
		fl.fl4_src = (*rp)->rt_src;
		ip_rt_put(*rp);
		*rp = NULL;
	}
	security_sk_classify_flow(sk, &fl);
	return ip_route_output_flow(rp, &fl, sk, 0);
}

static inline int ip_route_newports(struct rtable **rp, u8 protocol,
				    __be16 sport, __be16 dport, struct sock *sk)
{
	if (sport != (*rp)->fl.fl_ip_sport ||
	    dport != (*rp)->fl.fl_ip_dport) {
		struct flowi fl;

		memcpy(&fl, &(*rp)->fl, sizeof(fl));
		fl.fl_ip_sport = sport;
		fl.fl_ip_dport = dport;
		fl.proto = protocol;
		ip_rt_put(*rp);
		*rp = NULL;
		security_sk_classify_flow(sk, &fl);
		return ip_route_output_flow(rp, &fl, sk, 0);
	}
	return 0;
}

extern void rt_bind_peer(struct rtable *rt, int create);

static inline struct inet_peer *rt_get_peer(struct rtable *rt)
{
	if (rt->peer)
		return rt->peer;

	rt_bind_peer(rt, 0);
	return rt->peer;
}

extern ctl_table ipv4_route_table[];

#endif	/* _ROUTE_H */
