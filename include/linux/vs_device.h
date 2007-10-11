#ifndef _VS_DEVICE_H
#define _VS_DEVICE_H

#include "vserver/base.h"
#include "vserver/device.h"
#include "vserver/debug.h"


#ifdef CONFIG_VSERVER_DEVICE

int vs_map_device(struct vx_info *, dev_t, dev_t *, umode_t);

#define vs_device_perm(v, d, m, p) \
	((vs_map_device(current_vx_info(), d, NULL, m) & (p)) == (p))

#else

static inline
int vs_map_device(struct vx_info *vxi,
	dev_t device, dev_t *target, umode_t mode)
{
	if (target)
		*target = device;
	return ~0;
}

#define vs_device_perm(v, d, m, p) ((p) == (p))

#endif


#define vs_map_chrdev(d, t, p) \
	((vs_map_device(current_vx_info(), d, t, S_IFCHR) & (p)) == (p))
#define vs_map_blkdev(d, t, p) \
	((vs_map_device(current_vx_info(), d, t, S_IFBLK) & (p)) == (p))

#define vs_chrdev_perm(d, p) \
	vs_device_perm(current_vx_info(), d, S_IFCHR, p)
#define vs_blkdev_perm(d, p) \
	vs_device_perm(current_vx_info(), d, S_IFBLK, p)


#else
#warning duplicate inclusion
#endif
