/*
 *  linux/kernel/vserver/cacct.c
 *
 *  Virtual Server: Context Accounting
 *
 *  Copyright (C) 2006-2007 Herbert Pötzl
 *
 *  V0.01  added accounting stats
 *
 */

#include <linux/types.h>
#include <linux/vs_context.h>
#include <linux/vserver/cacct_cmd.h>
#include <linux/vserver/cacct_int.h>

#include <asm/errno.h>
#include <asm/uaccess.h>


int vc_sock_stat(struct vx_info *vxi, void __user *data)
{
	struct vcmd_sock_stat_v0 vc_data;
	int j, field;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	field = vc_data.field;
	if ((field < 0) || (field >= VXA_SOCK_SIZE))
		return -EINVAL;

	for (j = 0; j < 3; j++) {
		vc_data.count[j] = vx_sock_count(&vxi->cacct, field, j);
		vc_data.total[j] = vx_sock_total(&vxi->cacct, field, j);
	}

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

