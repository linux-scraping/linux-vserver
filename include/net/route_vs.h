
static inline
int ip_v4_find_src(struct nx_info *nxi, struct rtable **rp, struct flowi *fl)
{
	if (!nxi || !NX_IPV4(nxi))
		return 0;

	/* single IP is unconditional */
	if (nx_info_flags(nxi, NXF_SINGLE_IP, 0) &&
		(fl->fl4_src == INADDR_ANY))
		fl->fl4_src = nxi->v4.ip.s_addr;

	if (fl->fl4_src == INADDR_ANY) {
		struct nx_addr_v4 *ptr;
		__be32 found;
		int err;

		err = __ip_route_output_key(rp, fl);
		if (!err) {
			found = (*rp)->rt_src;
			ip_rt_put(*rp);
			if (v4_addr_in_nx_info(nxi, found, -1))
				goto found;
		}

		for (ptr = &nxi->v4; ptr; ptr = ptr->next) {
			fl->fl4_src = ptr->ip.s_addr;
			err = __ip_route_output_key(rp, fl);
			if (err)
				continue;

			found = (*rp)->rt_src;
			ip_rt_put(*rp);
			if (v4_addr_in_nx_info(nxi, found, -1))
				goto found;
		}
		/* still no source ip? */
		found = (fl->fl4_dst == IPI_LOOPBACK)
			? IPI_LOOPBACK : nxi->v4_lback.s_addr;
	found:
		/* assign src ip to flow */
		fl->fl4_src = found;

	} else {
		if (!v4_addr_in_nx_info(nxi, fl->fl4_src, -1))
			return -EPERM;
	}

	if (nx_info_flags(nxi, NXF_LBACK_REMAP, 0)) {
		if (fl->fl4_dst == IPI_LOOPBACK)
			fl->fl4_dst = nxi->v4_lback.s_addr;
		if (fl->fl4_src == IPI_LOOPBACK)
			fl->fl4_src = nxi->v4_lback.s_addr;
	}
	return 0;
}

