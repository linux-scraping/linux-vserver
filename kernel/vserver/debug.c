/*
 *  kernel/vserver/debug.c
 *
 *  Copyright (C) 2005-2007 Herbert Pötzl
 *
 *  V0.01  vx_info dump support
 *
 */

#include <linux/module.h>

#include <linux/vserver/context.h>


void	dump_vx_info(struct vx_info *vxi, int level)
{
	printk("vx_info %p[#%d, %d.%d, %4x]\n", vxi, vxi->vx_id,
		atomic_read(&vxi->vx_usecnt),
		atomic_read(&vxi->vx_tasks),
		vxi->vx_state);
	if (level > 0) {
		__dump_vx_limit(&vxi->limit);
		__dump_vx_sched(&vxi->sched);
		__dump_vx_cvirt(&vxi->cvirt);
		__dump_vx_cacct(&vxi->cacct);
	}
	printk("---\n");
}


EXPORT_SYMBOL_GPL(dump_vx_info);

