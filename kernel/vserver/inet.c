
#include <linux/in.h>
#include <linux/inetdevice.h>
#include <linux/export.h>
#include <linux/vs_inet.h>
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
		unsigned long irqflags;

		spin_lock_irqsave(&nxi1->addr_lock, irqflags);
		for (ptr = &nxi1->v4; ptr; ptr = ptr->next) {
			if (v4_nx_addr_in_nx_info(nxi2, ptr, -1)) {
				ret = 1;
				break;
			}
		}
		spin_unlock_irqrestore(&nxi1->addr_lock, irqflags);
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
		unsigned long irqflags;

		spin_lock_irqsave(&nxi1->addr_lock, irqflags);
		for (ptr = &nxi1->v6; ptr; ptr = ptr->next) {
			if (v6_nx_addr_in_nx_info(nxi2, ptr, -1)) {
				ret = 1;
				break;
			}
		}
		spin_unlock_irqrestore(&nxi1->addr_lock, irqflags);
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
	struct inet6_ifaddr *ifa;
	int ret = 0;

	if (!dev)
		goto out;
	in_dev = in6_dev_get(dev);
	if (!in_dev)
		goto out;

	// for (ifap = &in_dev->addr_list; (ifa = *ifap) != NULL;
	list_for_each_entry(ifa, &in_dev->addr_list, if_list) {
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

struct rtable *ip_v4_find_src(struct net *net, struct nx_info *nxi,
	struct flowi4 *fl4)
{
	struct rtable *rt;

	if (!nxi)
		return NULL;

	/* FIXME: handle lback only case */
	if (!NX_IPV4(nxi))
		return ERR_PTR(-EPERM);

	vxdprintk(VXD_CBIT(net, 4),
		"ip_v4_find_src(%p[#%u]) " NIPQUAD_FMT " -> " NIPQUAD_FMT,
		nxi, nxi ? nxi->nx_id : 0,
		NIPQUAD(fl4->saddr), NIPQUAD(fl4->daddr));

	/* single IP is unconditional */
	if (nx_info_flags(nxi, NXF_SINGLE_IP, 0) &&
		(fl4->saddr == INADDR_ANY))
		fl4->saddr = nxi->v4.ip[0].s_addr;

	if (fl4->saddr == INADDR_ANY) {
		struct nx_addr_v4 *ptr;
		__be32 found = 0;

		rt = __ip_route_output_key(net, fl4);
		if (!IS_ERR(rt)) {
			found = fl4->saddr;
			ip_rt_put(rt);
			vxdprintk(VXD_CBIT(net, 4),
				"ip_v4_find_src(%p[#%u]) rok[%u]: " NIPQUAD_FMT,
				nxi, nxi ? nxi->nx_id : 0, fl4->flowi4_oif, NIPQUAD(found));
			if (v4_addr_in_nx_info(nxi, found, NXA_MASK_BIND))
				goto found;
		}

		WARN_ON_ONCE(in_irq());
		spin_lock_bh(&nxi->addr_lock);
		for (ptr = &nxi->v4; ptr; ptr = ptr->next) {
			__be32 primary = ptr->ip[0].s_addr;
			__be32 mask = ptr->mask.s_addr;
			__be32 neta = primary & mask;

			vxdprintk(VXD_CBIT(net, 4), "ip_v4_find_src(%p[#%u]) chk: "
				NIPQUAD_FMT "/" NIPQUAD_FMT "/" NIPQUAD_FMT,
				nxi, nxi ? nxi->nx_id : 0, NIPQUAD(primary),
				NIPQUAD(mask), NIPQUAD(neta));
			if ((found & mask) != neta)
				continue;

			fl4->saddr = primary;
			rt = __ip_route_output_key(net, fl4);
			vxdprintk(VXD_CBIT(net, 4),
				"ip_v4_find_src(%p[#%u]) rok[%u]: " NIPQUAD_FMT,
				nxi, nxi ? nxi->nx_id : 0, fl4->flowi4_oif, NIPQUAD(primary));
			if (!IS_ERR(rt)) {
				found = fl4->saddr;
				ip_rt_put(rt);
				if (found == primary)
					goto found_unlock;
			}
		}
		/* still no source ip? */
		found = ipv4_is_loopback(fl4->daddr)
			? IPI_LOOPBACK : nxi->v4.ip[0].s_addr;
	found_unlock:
		spin_unlock_bh(&nxi->addr_lock);
	found:
		/* assign src ip to flow */
		fl4->saddr = found;

	} else {
		if (!v4_addr_in_nx_info(nxi, fl4->saddr, NXA_MASK_BIND))
			return ERR_PTR(-EPERM);
	}

	if (nx_info_flags(nxi, NXF_LBACK_REMAP, 0)) {
		if (ipv4_is_loopback(fl4->daddr))
			fl4->daddr = nxi->v4_lback.s_addr;
		if (ipv4_is_loopback(fl4->saddr))
			fl4->saddr = nxi->v4_lback.s_addr;
	} else if (ipv4_is_loopback(fl4->daddr) &&
		!nx_info_flags(nxi, NXF_LBACK_ALLOW, 0))
		return ERR_PTR(-EPERM);

	return NULL;
}

EXPORT_SYMBOL_GPL(ip_v4_find_src);

