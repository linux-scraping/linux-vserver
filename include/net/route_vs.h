
static inline int ip_find_src(struct nx_info *nxi, struct rtable **rp, struct flowi *fl)
{
	int err;
	int i, n;
	uint32_t ipv4root;

	if (!nxi)
		return 0;

	ipv4root = nxi->ipv4[0];
	if (ipv4root == 0)
		return 0;

	n = nxi->nbipv4;
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

	if (fl->fl4_dst == IPI_LOOPBACK && !nx_check(0, VS_ADMIN))
		fl->fl4_dst = nxi->ipv4[0];
#ifdef CONFIG_VSERVER_REMAP_SADDR
	if (fl->fl4_src == IPI_LOOPBACK && !nx_check(0, VS_ADMIN))
		fl->fl4_src = nxi->ipv4[0];
#endif
	return 0;
}

