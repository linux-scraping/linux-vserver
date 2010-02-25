#ifndef _VS_INET6_H
#define _VS_INET6_H

#include "vserver/base.h"
#include "vserver/network.h"
#include "vserver/debug.h"

#include <net/ipv6.h>

#define NXAV6(a)	&(a)->ip, &(a)->mask, (a)->prefix, (a)->type
#define NXAV6_FMT	"[%pI6/%pI6/%d:%04x]"


#ifdef	CONFIG_IPV6

static inline
int v6_addr_match(struct nx_addr_v6 *nxa,
	const struct in6_addr *addr, uint16_t mask)
{
	int ret = 0;

	switch (nxa->type & mask) {
	case NXA_TYPE_MASK:
		ret = ipv6_masked_addr_cmp(&nxa->ip, &nxa->mask, addr);
		break;
	case NXA_TYPE_ADDR:
		ret = ipv6_addr_equal(&nxa->ip, addr);
		break;
	case NXA_TYPE_ANY:
		ret = 1;
		break;
	}
	vxdprintk(VXD_CBIT(net, 0),
		"v6_addr_match(%p" NXAV6_FMT ",%pI6,%04x) = %d",
		nxa, NXAV6(nxa), addr, mask, ret);
	return ret;
}

static inline
int v6_addr_in_nx_info(struct nx_info *nxi,
	const struct in6_addr *addr, uint16_t mask)
{
	struct nx_addr_v6 *nxa;
	int ret = 1;

	if (!nxi)
		goto out;
	for (nxa = &nxi->v6; nxa; nxa = nxa->next)
		if (v6_addr_match(nxa, addr, mask))
			goto out;
	ret = 0;
out:
	vxdprintk(VXD_CBIT(net, 0),
		"v6_addr_in_nx_info(%p[#%u],%pI6,%04x) = %d",
		nxi, nxi ? nxi->nx_id : 0, addr, mask, ret);
	return ret;
}

static inline
int v6_nx_addr_match(struct nx_addr_v6 *nxa, struct nx_addr_v6 *addr, uint16_t mask)
{
	/* FIXME: needs full range checks */
	return v6_addr_match(nxa, &addr->ip, mask);
}

static inline
int v6_nx_addr_in_nx_info(struct nx_info *nxi, struct nx_addr_v6 *nxa, uint16_t mask)
{
	struct nx_addr_v6 *ptr;

	for (ptr = &nxi->v6; ptr; ptr = ptr->next)
		if (v6_nx_addr_match(ptr, nxa, mask))
			return 1;
	return 0;
}


/*
 *	Check if a given address matches for a socket
 *
 *	nxi:		the socket's nx_info if any
 *	addr:		to be verified address
 */
static inline
int v6_sock_addr_match (
	struct nx_info *nxi,
	struct inet_sock *inet,
	struct in6_addr *addr)
{
	struct sock *sk = &inet->sk;
	struct in6_addr *saddr = inet6_rcv_saddr(sk);

	if (!ipv6_addr_any(addr) &&
		ipv6_addr_equal(saddr, addr))
		return 1;
	if (ipv6_addr_any(saddr))
		return v6_addr_in_nx_info(nxi, addr, -1);
	return 0;
}

/*
 *	check if address is covered by socket
 *
 *	sk:	the socket to check against
 *	addr:	the address in question (must be != 0)
 */

static inline
int __v6_addr_match_socket(const struct sock *sk, struct nx_addr_v6 *nxa)
{
	struct nx_info *nxi = sk->sk_nx_info;
	struct in6_addr *saddr = inet6_rcv_saddr(sk);

	vxdprintk(VXD_CBIT(net, 5),
		"__v6_addr_in_socket(%p," NXAV6_FMT ") %p:%pI6 %p;%lx",
		sk, NXAV6(nxa), nxi, saddr, sk->sk_socket,
		(sk->sk_socket?sk->sk_socket->flags:0));

	if (!ipv6_addr_any(saddr)) {	/* direct address match */
		return v6_addr_match(nxa, saddr, -1);
	} else if (nxi) {		/* match against nx_info */
		return v6_nx_addr_in_nx_info(nxi, nxa, -1);
	} else {			/* unrestricted any socket */
		return 1;
	}
}


/* inet related checks and helpers */


struct in_ifaddr;
struct net_device;
struct sock;


#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <net/inet_timewait_sock.h>


int dev_in_nx_info(struct net_device *, struct nx_info *);
int v6_dev_in_nx_info(struct net_device *, struct nx_info *);
int nx_v6_addr_conflict(struct nx_info *, struct nx_info *);



static inline
int v6_ifa_in_nx_info(struct inet6_ifaddr *ifa, struct nx_info *nxi)
{
	if (!nxi)
		return 1;
	if (!ifa)
		return 0;
	return v6_addr_in_nx_info(nxi, &ifa->addr, -1);
}

static inline
int nx_v6_ifa_visible(struct nx_info *nxi, struct inet6_ifaddr *ifa)
{
	vxdprintk(VXD_CBIT(net, 1), "nx_v6_ifa_visible(%p[#%u],%p) %d",
		nxi, nxi ? nxi->nx_id : 0, ifa,
		nxi ? v6_ifa_in_nx_info(ifa, nxi) : 0);

	if (!nx_info_flags(nxi, NXF_HIDE_NETIF, 0))
		return 1;
	if (v6_ifa_in_nx_info(ifa, nxi))
		return 1;
	return 0;
}


struct nx_v6_sock_addr {
	struct in6_addr saddr;	/* Address used for validation */
	struct in6_addr baddr;	/* Address used for socket bind */
};

static inline
int v6_map_sock_addr(struct inet_sock *inet, struct sockaddr_in6 *addr,
	struct nx_v6_sock_addr *nsa)
{
	// struct sock *sk = &inet->sk;
	// struct nx_info *nxi = sk->sk_nx_info;
	struct in6_addr saddr = addr->sin6_addr;
	struct in6_addr baddr = saddr;

	nsa->saddr = saddr;
	nsa->baddr = baddr;
	return 0;
}

static inline
void v6_set_sock_addr(struct inet_sock *inet, struct nx_v6_sock_addr *nsa)
{
	// struct sock *sk = &inet->sk;
	// struct in6_addr *saddr = inet6_rcv_saddr(sk);

	// *saddr = nsa->baddr;
	// inet->inet_saddr = nsa->baddr;
}

static inline
int nx_info_has_v6(struct nx_info *nxi)
{
	if (!nxi)
		return 1;
	if (NX_IPV6(nxi))
		return 1;
	return 0;
}

#else /* CONFIG_IPV6 */

static inline
int nx_v6_dev_visible(struct nx_info *n, struct net_device *d)
{
	return 1;
}


static inline
int nx_v6_addr_conflict(struct nx_info *n, uint32_t a, const struct sock *s)
{
	return 1;
}

static inline
int v6_ifa_in_nx_info(struct in_ifaddr *a, struct nx_info *n)
{
	return 1;
}

static inline
int nx_info_has_v6(struct nx_info *nxi)
{
	return 0;
}

#endif /* CONFIG_IPV6 */

#define current_nx_info_has_v6() \
	nx_info_has_v6(current_nx_info())

#else
#warning duplicate inclusion
#endif
