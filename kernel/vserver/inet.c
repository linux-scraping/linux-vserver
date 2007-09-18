
#include <linux/vserver/debug.h>
#include <linux/vs_inet.h>
#include <linux/vs_inet6.h>
#include <net/addrconf.h>


int nx_v4_addr_conflict(struct nx_info *nxi1, struct nx_info *nxi2)
{
	int ret = 0;

	if (!nxi1 || !nxi2 || nxi1 == nxi2)
		ret = 1;
	else {
		struct nx_addr_v4 *ptr;

		for (ptr = &nxi1->v4; ptr; ptr = ptr->next) {
			if (v4_nx_addr_in_nx_info(nxi2, ptr, -1)) {
				ret = 1;
				break;
			}
		}
	}

	vxdprintk(VXD_CBIT(net, 2),
		"nx_v4_addr_conflict(%p,%p): %d",
		nxi1, nxi2, ret);

	return ret;
}


#ifdef	CONFIG_IPV6

int nx_v6_addr_conflict(struct nx_info *nxi1, struct nx_info *nxi2)
{
	int ret = 0;

	if (!nxi1 || !nxi2 || nxi1 == nxi2)
		ret = 1;
	else {
		struct nx_addr_v6 *ptr;

		for (ptr = &nxi1->v6; ptr; ptr = ptr->next) {
			if (v6_nx_addr_in_nx_info(nxi2, ptr, -1)) {
				ret = 1;
				break;
			}
		}
	}

	vxdprintk(VXD_CBIT(net, 2),
		"nx_v6_addr_conflict(%p,%p): %d",
		nxi1, nxi2, ret);

	return ret;
}

#endif

int v4_dev_in_nx_info(struct net_device *dev, struct nx_info *nxi)
{
	struct in_device *in_dev;
	struct in_ifaddr **ifap;
	struct in_ifaddr *ifa;
	int ret = 0;

	if (!dev)
		goto out;
	in_dev = in_dev_get(dev);
	if (!in_dev)
		goto out;

	for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
		ifap = &ifa->ifa_next) {
		if (v4_addr_in_nx_info(nxi, ifa->ifa_local, -1)) {
			ret = 1;
			break;
		}
	}
	in_dev_put(in_dev);
out:
	return ret;
}


#ifdef	CONFIG_IPV6

int v6_dev_in_nx_info(struct net_device *dev, struct nx_info *nxi)
{
	struct inet6_dev *in_dev;
	struct inet6_ifaddr **ifap;
	struct inet6_ifaddr *ifa;
	int ret = 0;

	if (!dev)
		goto out;
	in_dev = in6_dev_get(dev);
	if (!in_dev)
		goto out;

	for (ifap = &in_dev->addr_list; (ifa = *ifap) != NULL;
		ifap = &ifa->if_next) {
		if (v6_addr_in_nx_info(nxi, &ifa->addr, -1)) {
			ret = 1;
			break;
		}
	}
	in6_dev_put(in_dev);
out:
	return ret;
}

#endif

int dev_in_nx_info(struct net_device *dev, struct nx_info *nxi)
{
	int ret = 1;

	if (!nxi)
		goto out;
	if (nxi->v4.type && v4_dev_in_nx_info(dev, nxi))
		goto out;
#ifdef	CONFIG_IPV6
	ret = 2;
	if (nxi->v6.type && v6_dev_in_nx_info(dev, nxi))
		goto out;
#endif
	ret = 0;
out:
	vxdprintk(VXD_CBIT(net, 3),
		"dev_in_nx_info(%p,%p[#%d]) = %d",
		dev, nxi, nxi ? nxi->nx_id : 0, ret);
	return ret;
}

