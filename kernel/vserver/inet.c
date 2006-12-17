
#include <linux/sched.h>
#include <linux/vs_inet.h>


int nx_addr_conflict(struct nx_info *nxi, uint32_t addr, struct sock *sk)
{
	vxdprintk(VXD_CBIT(net, 2),
		"nx_addr_conflict(%p,%p) %d.%d,%d.%d",
		nxi, sk, VXD_QUAD(addr));

	if (addr) {
		/* check real address */
		return __addr_in_socket(sk, addr);
	} else if (nxi) {
		/* check against nx_info */
		int i, n = nxi->nbipv4;

		for (i=0; i<n; i++)
			if (__addr_in_socket(sk, nxi->ipv4[i]))
				return 1;
		return 0;
	} else {
		/* check against any */
		return 1;
	}
}


int dev_in_nx_info(struct net_device *dev, struct nx_info *nxi)
{
	struct in_device *in_dev;
	struct in_ifaddr **ifap;
	struct in_ifaddr *ifa;
	int ret = 0;

	if (!nxi)
		return 1;

	in_dev = in_dev_get(dev);
	if (!in_dev)
		goto out;

	for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
		ifap = &ifa->ifa_next) {
		if (addr_in_nx_info(nxi, ifa->ifa_local)) {
			ret = 1;
			break;
		}
	}
	in_dev_put(in_dev);
out:
	return ret;
}


