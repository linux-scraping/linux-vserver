
/*
 *  linux/kernel/vserver/legacynet.c
 *
 *  Virtual Server: Legacy Network Funtions
 *
 *  Copyright (C) 2001-2003  Jacques Gelinas
 *  Copyright (C) 2003-2005  Herbert Pötzl
 *
 *  V0.01  broken out from legacy.c
 *
 */

#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vs_network.h>
#include <linux/vserver/legacy.h>
// #include <linux/mnt_namespace.h>
#include <linux/err.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


extern struct nx_info *create_nx_info(void);

/*  set ipv4 root (syscall) */

int vc_set_ipv4root(uint32_t nbip, void __user *data)
{
	int i, err = -EPERM;
	struct vcmd_set_ipv4root_v3 vc_data;
	struct nx_info *new_nxi, *nxi = current->nx_info;

	if (nbip < 0 || nbip > NB_IPV4ROOT)
		return -EINVAL;
	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if (!nxi || nxi->ipv4[0] == 0 || capable(CAP_NET_ADMIN))
		/* We are allowed to change everything */
		err = 0;
	else if (nxi) {
		int found = 0;

		/* We are allowed to select a subset of the currently
		   installed IP numbers. No new one are allowed
		   We can't change the broadcast address though */
		for (i=0; i<nbip; i++) {
			int j;
			__u32 nxip = vc_data.nx_mask_pair[i].ip;
			for (j=0; j<nxi->nbipv4; j++) {
				if (nxip == nxi->ipv4[j]) {
					found++;
					break;
				}
			}
		}
		if ((found == nbip) &&
			(vc_data.broadcast == nxi->v4_bcast))
			err = 0;
	}
	if (err)
		return err;

	new_nxi = create_nx_info();
	if (IS_ERR(new_nxi))
		return -EINVAL;

	new_nxi->nbipv4 = nbip;
	for (i=0; i<nbip; i++) {
		new_nxi->ipv4[i] = vc_data.nx_mask_pair[i].ip;
		new_nxi->mask[i] = vc_data.nx_mask_pair[i].mask;
	}
	new_nxi->v4_bcast = vc_data.broadcast;
	if (nxi)
		printk("!!! switching nx_info %p->%p\n", nxi, new_nxi);

	nx_migrate_task(current, new_nxi);
	put_nx_info(new_nxi);
	return 0;
}


