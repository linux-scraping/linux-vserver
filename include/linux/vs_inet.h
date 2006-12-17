#ifndef _VS_INET_H
#define _VS_INET_H

#include <linux/vs_network.h>

#define IPI_LOOPBACK	htonl(INADDR_LOOPBACK)


static inline int addr_in_nx_info(struct nx_info *nxi, uint32_t addr)
{
	int n,i;

	if (!nxi)
		return 1;

	n = nxi->nbipv4;
	if (n && (nxi->ipv4[0] == 0))
		return 1;
	for (i=0; i<n; i++) {
		if (nxi->ipv4[i] == addr)
			return 1;
	}
	return 0;
}


/*
 *	Check if a given address matches for a socket
 *
 *	nxi:		the socket's nx_info if any
 *	addr:		to be verified address
 *	saddr/baddr:	socket addresses
 */
static inline int raw_addr_match (
	struct nx_info *nxi,
	uint32_t addr,
	uint32_t saddr,
	uint32_t baddr)
{
	if (addr && (saddr == addr || baddr == addr))
		return 1;
	if (!saddr)
		return addr_in_nx_info(nxi, addr);
	return 0;
}


/* inet related checks and helpers */


struct in_ifaddr;
struct net_device;
struct sock;

#ifdef CONFIG_INET

#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <net/inet_timewait_sock.h>


int dev_in_nx_info(struct net_device *, struct nx_info *);
int nx_addr_conflict(struct nx_info *, uint32_t, struct sock *);


/*
 *	check if address is covered by socket
 *
 *	sk:	the socket to check against
 *	addr:	the address in question (must be != 0)
 */

static inline
int __addr_in_socket(struct sock *sk, uint32_t addr)
{
	struct nx_info *nxi = sk->sk_nx_info;
	uint32_t saddr = inet_rcv_saddr(sk);

	vxdprintk(VXD_CBIT(net, 5),
		"__addr_in_socket(%p,%d.%d.%d.%d) %p:%d.%d.%d.%d %p;%lx",
		sk, VXD_QUAD(addr), nxi, VXD_QUAD(saddr), sk->sk_socket,
		(sk->sk_socket?sk->sk_socket->flags:0));

	if (saddr) {
		/* direct address match */
		return (saddr == addr);
	} else if (nxi) {
		/* match against nx_info */
		return addr_in_nx_info(nxi, addr);
	} else {
		/* unrestricted any socket */
		return 1;
	}
}



static inline
int nx_dev_visible(struct nx_info *nxi, struct net_device *dev)
{
	if (!nx_info_flags(nxi, NXF_HIDE_NETIF, 0))
		return 1;
	if (dev_in_nx_info(dev, nxi))
		return 1;
	return 0;
}


static inline
int ifa_in_nx_info(struct in_ifaddr *ifa, struct nx_info *nxi)
{
	if (!nxi)
		return 1;
	if (!ifa)
		return 0;
	return addr_in_nx_info(nxi, ifa->ifa_local);
}

static inline
int nx_ifa_visible(struct nx_info *nxi, struct in_ifaddr *ifa)
{
	if (!nx_info_flags(nxi, NXF_HIDE_NETIF, 0))
		return 1;
	if (ifa_in_nx_info(ifa, nxi))
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
int nx_addr_conflict(struct nx_info *n, uint32_t a, struct sock *s)
{
	return 1;
}

static inline
int ifa_in_nx_info(struct in_ifaddr *a, struct nx_info *n)
{
	return 1;
}

#endif /* CONFIG_INET */


#else
#warning duplicate inclusion
#endif
