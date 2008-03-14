
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <linux/vs_inet6.h>
#include <linux/vserver/debug.h>
#include <net/route.h>
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
		if (v4_addr_in_nx_info(nxi, ifa->ifa_local, NXA_MASK_SHOW)) {
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

int ip_v4_find_src(struct nx_info *nxi, struct rtable **rp, struct flowi *fl)
{
	if (!nxi)
		return 0;

	/* FIXME: handle lback only case */
	if (!NX_IPV4(nxi))
		return -EPERM;

	vxdprintk(VXD_CBIT(net, 4),
		"ip_v4_find_src(%p[#%u]) " NIPQUAD_FMT " -> " NIPQUAD_FMT,
		nxi, nxi ? nxi->nx_id : 0,
		NIPQUAD(fl->fl4_src), NIPQUAD(fl->fl4_dst));

	/* single IP is unconditional */
	if (nx_info_flags(nxi, NXF_SINGLE_IP, 0) &&
		(fl->fl4_src == INADDR_ANY))
		fl->fl4_src = nxi->v4.ip[0].s_addr;

	if (fl->fl4_src == INADDR_ANY) {
		struct nx_addr_v4 *ptr;
		__be32 found;
		int err;

		err = __ip_route_output_key(rp, fl);
		if (!err) {
			found = (*rp)->rt_src;
			ip_rt_put(*rp);
			vxdprintk(VXD_CBIT(net, 4),
				"ip_v4_find_src(%p[#%u]) rok[%u]: " NIPQUAD_FMT,
				nxi, nxi ? nxi->nx_id : 0, fl->oif, NIPQUAD(found));
			if (v4_addr_in_nx_info(nxi, found, NXA_MASK_BIND))
				goto found;
		}

		for (ptr = &nxi->v4; ptr; ptr = ptr->next) {
			__be32 primary = ptr->ip[0].s_addr;
			__be32 mask = ptr->mask.s_addr;
			__be32 net = primary & mask;

			vxdprintk(VXD_CBIT(net, 4), "ip_v4_find_src(%p[#%u]) chk: "
				NIPQUAD_FMT "/" NIPQUAD_FMT "/" NIPQUAD_FMT,
				nxi, nxi ? nxi->nx_id : 0, NIPQUAD(primary),
				NIPQUAD(mask), NIPQUAD(net));
			if ((found & mask) != net)
				continue;

			fl->fl4_src = primary;
			err = __ip_route_output_key(rp, fl);
			vxdprintk(VXD_CBIT(net, 4),
				"ip_v4_find_src(%p[#%u]) rok[%u]: " NIPQUAD_FMT,
				nxi, nxi ? nxi->nx_id : 0, fl->oif, NIPQUAD(primary));
			if (!err) {
				found = (*rp)->rt_src;
				ip_rt_put(*rp);
				if (found == primary)
					goto found;
			}
		}
		/* still no source ip? */
		found = LOOPBACK(fl->fl4_dst)
			? IPI_LOOPBACK : nxi->v4.ip[0].s_addr;
	found:
		/* assign src ip to flow */
		fl->fl4_src = found;

	} else {
		if (!v4_addr_in_nx_info(nxi, fl->fl4_src, NXA_MASK_BIND))
			return -EPERM;
	}

	if (nx_info_flags(nxi, NXF_LBACK_REMAP, 0)) {
		if (LOOPBACK(fl->fl4_dst))
			fl->fl4_dst = nxi->v4_lback.s_addr;
		if (LOOPBACK(fl->fl4_src))
			fl->fl4_src = nxi->v4_lback.s_addr;
	} else if (LOOPBACK(fl->fl4_dst) &&
		!nx_info_flags(nxi, NXF_LBACK_ALLOW, 0))
		return -EPERM;

	return 0;
}

EXPORT_SYMBOL_GPL(ip_v4_find_src);

