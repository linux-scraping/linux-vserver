#ifndef _VS_INET_H
#define _VS_INET_H

#include "vserver/base.h"
#include "vserver/network.h"
#include "vserver/debug.h"

#define IPI_LOOPBACK	htonl(INADDR_LOOPBACK)

#define NXAV4(a)	NIPQUAD((a)->ip[0]), NIPQUAD((a)->ip[1]), \
			NIPQUAD((a)->mask), (a)->type
#define NXAV4_FMT	"[" NIPQUAD_FMT "-" NIPQUAD_FMT "/" NIPQUAD_FMT ":%04x]"

#define NIPQUAD(addr) \
	((unsigned char *)&addr)[0], \
	((unsigned char *)&addr)[1], \
	((unsigned char *)&addr)[2], \
	((unsigned char *)&addr)[3]

#define NIPQUAD_FMT "%u.%u.%u.%u"


static inline
int v4_addr_match(struct nx_addr_v4 *nxa, __be32 addr, uint16_t tmask)
{
	__be32 ip = nxa->ip[0].s_addr;
	__be32 mask = nxa->mask.s_addr;
	__be32 bcast = ip | ~mask;
	int ret = 0;

	switch (nxa->type & tmask) {
	case NXA_TYPE_MASK:
		ret = (ip == (addr & mask));
		break;
	case NXA_TYPE_ADDR:
		ret = 3;
		if (addr == ip)
			break;
		/* fall through to broadcast */
	case NXA_MOD_BCAST:
		ret = ((tmask & NXA_MOD_BCAST) && (addr == bcast));
		break;
	case NXA_TYPE_RANGE:
		ret = ((nxa->ip[0].s_addr <= addr) &&
			(nxa->ip[1].s_addr > addr));
		break;
	case NXA_TYPE_ANY:
		ret = 2;
		break;
	}

	vxdprintk(VXD_CBIT(net, 0),
		"v4_addr_match(%p" NXAV4_FMT "," NIPQUAD_FMT ",%04x) = %d",
		nxa, NXAV4(nxa), NIPQUAD(addr), tmask, ret);
	return ret;
}

static inline
int v4_addr_in_nx_info(struct nx_info *nxi, __be32 addr, uint16_t tmask)
{
	struct nx_addr_v4 *nxa;
	unsigned long irqflags;
	int ret = 1;

	if (!nxi)
		goto out;

	ret = 2;
	/* allow 127.0.0.1 when remapping lback */
	if ((tmask & NXA_LOOPBACK) &&
		(addr == IPI_LOOPBACK) &&
		nx_info_flags(nxi, NXF_LBACK_REMAP, 0))
		goto out;
	ret = 3;
	/* check for lback address */
	if ((tmask & NXA_MOD_LBACK) &&
		(nxi->v4_lback.s_addr == addr))
		goto out;
	ret = 4;
	/* check for broadcast address */
	if ((tmask & NXA_MOD_BCAST) &&
		(nxi->v4_bcast.s_addr == addr))
		goto out;
	ret = 5;

	/* check for v4 addresses */
	spin_lock_irqsave(&nxi->addr_lock, irqflags);
	for (nxa = &nxi->v4; nxa; nxa = nxa->next)
		if (v4_addr_match(nxa, addr, tmask))
			goto out_unlock;
	ret = 0;
out_unlock:
	spin_unlock_irqrestore(&nxi->addr_lock, irqflags);
out:
	vxdprintk(VXD_CBIT(net, 0),
		"v4_addr_in_nx_info(%p[#%u]," NIPQUAD_FMT ",%04x) = %d",
		nxi, nxi ? nxi->nx_id : 0, NIPQUAD(addr), tmask, ret);
	return ret;
}

static inline
int v4_nx_addr_match(struct nx_addr_v4 *nxa, struct nx_addr_v4 *addr, uint16_t mask)
{
	/* FIXME: needs full range checks */
	return v4_addr_match(nxa, addr->ip[0].s_addr, mask);
}

static inline
int v4_nx_addr_in_nx_info(struct nx_info *nxi, struct nx_addr_v4 *nxa, uint16_t mask)
{
	struct nx_addr_v4 *ptr;
	unsigned long irqflags;
	int ret = 1;

	spin_lock_irqsave(&nxi->addr_lock, irqflags);
	for (ptr = &nxi->v4; ptr; ptr = ptr->next)
		if (v4_nx_addr_match(ptr, nxa, mask))
			goto out_unlock;
	ret = 0;
out_unlock:
	spin_unlock_irqrestore(&nxi->addr_lock, irqflags);
	return ret;
}

#include <net/inet_sock.h>

/*
 *	Check if a given address matches for a socket
 *
 *	nxi:		the socket's nx_info if any
 *	addr:		to be verified address
 */
static inline
int v4_sock_addr_match (
	struct nx_info *nxi,
	struct inet_sock *inet,
	__be32 addr)
{
	__be32 saddr = inet->inet_rcv_saddr;
	__be32 bcast = nxi ? nxi->v4_bcast.s_addr : INADDR_BROADCAST;

	if (addr && (saddr == addr || bcast == addr))
		return 1;
	if (!saddr)
		return v4_addr_in_nx_info(nxi, addr, NXA_MASK_BIND);
	return 0;
}


/* inet related checks and helpers */


struct in_ifaddr;
struct net_device;
struct sock;

#ifdef CONFIG_INET

#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <net/inet_sock.h>
#include <net/inet_timewait_sock.h>


int dev_in_nx_info(struct net_device *, struct nx_info *);
int v4_dev_in_nx_info(struct net_device *, struct nx_info *);
int nx_v4_addr_conflict(struct nx_info *, struct nx_info *);


/*
 *	check if address is covered by socket
 *
 *	sk:	the socket to check against
 *	addr:	the address in question (must be != 0)
 */

static inline
int __v4_addr_match_socket(const struct sock *sk, struct nx_addr_v4 *nxa)
{
	struct nx_info *nxi = sk->sk_nx_info;
	__be32 saddr = sk->sk_rcv_saddr;

	vxdprintk(VXD_CBIT(net, 5),
		"__v4_addr_in_socket(%p," NXAV4_FMT ") %p:" NIPQUAD_FMT " %p;%lx",
		sk, NXAV4(nxa), nxi, NIPQUAD(saddr), sk->sk_socket,
		(sk->sk_socket?sk->sk_socket->flags:0));

	if (saddr) {		/* direct address match */
		return v4_addr_match(nxa, saddr, -1);
	} else if (nxi) {	/* match against nx_info */
		return v4_nx_addr_in_nx_info(nxi, nxa, -1);
	} else {		/* unrestricted any socket */
		return 1;
	}
}



static inline
int nx_dev_visible(struct nx_info *nxi, struct net_device *dev)
{
	vxdprintk(VXD_CBIT(net, 1),
		"nx_dev_visible(%p[#%u],%p " VS_Q("%s") ") %d",
		nxi, nxi ? nxi->nx_id : 0, dev, dev->name,
		nxi ? dev_in_nx_info(dev, nxi) : 0);

	if (!nx_info_flags(nxi, NXF_HIDE_NETIF, 0))
		return 1;
	if (dev_in_nx_info(dev, nxi))
		return 1;
	return 0;
}


static inline
int v4_ifa_in_nx_info(struct in_ifaddr *ifa, struct nx_info *nxi)
{
	if (!nxi)
		return 1;
	if (!ifa)
		return 0;
	return v4_addr_in_nx_info(nxi, ifa->ifa_local, NXA_MASK_SHOW);
}

static inline
int nx_v4_ifa_visible(struct nx_info *nxi, struct in_ifaddr *ifa)
{
	vxdprintk(VXD_CBIT(net, 1), "nx_v4_ifa_visible(%p[#%u],%p) %d",
		nxi, nxi ? nxi->nx_id : 0, ifa,
		nxi ? v4_ifa_in_nx_info(ifa, nxi) : 0);

	if (!nx_info_flags(nxi, NXF_HIDE_NETIF, 0))
		return 1;
	if (v4_ifa_in_nx_info(ifa, nxi))
		return 1;
	return 0;
}


struct nx_v4_sock_addr {
	__be32 saddr;	/* Address used for validation */
	__be32 baddr;	/* Address used for socket bind */
};

static inline
int v4_map_sock_addr(struct inet_sock *inet, struct sockaddr_in *addr,
	struct nx_v4_sock_addr *nsa)
{
	struct sock *sk = &inet->sk;
	struct nx_info *nxi = sk->sk_nx_info;
	__be32 saddr = addr->sin_addr.s_addr;
	__be32 baddr = saddr;

	vxdprintk(VXD_CBIT(net, 3),
		"inet_bind(%p)* %p,%p;%lx " NIPQUAD_FMT,
		sk, sk->sk_nx_info, sk->sk_socket,
		(sk->sk_socket ? sk->sk_socket->flags : 0),
		NIPQUAD(saddr));

	if (nxi) {
		if (saddr == INADDR_ANY) {
			if (nx_info_flags(nxi, NXF_SINGLE_IP, 0))
				baddr = nxi->v4.ip[0].s_addr;
		} else if (saddr == IPI_LOOPBACK) {
			if (nx_info_flags(nxi, NXF_LBACK_REMAP, 0))
				baddr = nxi->v4_lback.s_addr;
		} else if (!ipv4_is_multicast(saddr) ||
			!nx_info_ncaps(nxi, NXC_MULTICAST)) {
			/* normal address bind */
			if (!v4_addr_in_nx_info(nxi, saddr, NXA_MASK_BIND))
				return -EADDRNOTAVAIL;
		}
	}

	vxdprintk(VXD_CBIT(net, 3),
		"inet_bind(%p) " NIPQUAD_FMT ", " NIPQUAD_FMT,
		sk, NIPQUAD(saddr), NIPQUAD(baddr));

	nsa->saddr = saddr;
	nsa->baddr = baddr;
	return 0;
}

static inline
void v4_set_sock_addr(struct inet_sock *inet, struct nx_v4_sock_addr *nsa)
{
	inet->inet_saddr = nsa->baddr;
	inet->inet_rcv_saddr = nsa->baddr;
}


/*
 *      helper to simplify inet_lookup_listener
 *
 *      nxi:	the socket's nx_info if any
 *      addr:	to be verified address
 *      saddr:	socket address
 */
static inline int v4_inet_addr_match (
	struct nx_info *nxi,
	__be32 addr,
	__be32 saddr)
{
	if (addr && (saddr == addr))
		return 1;
	if (!saddr)
		return nxi ? v4_addr_in_nx_info(nxi, addr, NXA_MASK_BIND) : 1;
	return 0;
}

static inline __be32 nx_map_sock_lback(struct nx_info *nxi, __be32 addr)
{
	if (nx_info_flags(nxi, NXF_HIDE_LBACK, 0) &&
		(addr == nxi->v4_lback.s_addr))
		return IPI_LOOPBACK;
	return addr;
}

static inline
int nx_info_has_v4(struct nx_info *nxi)
{
	if (!nxi)
		return 1;
	if (NX_IPV4(nxi))
		return 1;
	if (nx_info_flags(nxi, NXF_LBACK_REMAP, 0))
		return 1;
	return 0;
}

#else /* CONFIG_INET */

static inline
int nx_dev_visible(struct nx_info *n, struct net_device *d)
{
	return 1;
}

static inline
int nx_v4_addr_conflict(struct nx_info *n, uint32_t a, const struct sock *s)
{
	return 1;
}

static inline
int v4_ifa_in_nx_info(struct in_ifaddr *a, struct nx_info *n)
{
	return 1;
}

static inline
int nx_info_has_v4(struct nx_info *nxi)
{
	return 0;
}

#endif /* CONFIG_INET */

#define current_nx_info_has_v4() \
	nx_info_has_v4(current_nx_info())

#else
// #warning duplicate inclusion
#endif
