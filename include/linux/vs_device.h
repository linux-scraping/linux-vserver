#ifndef _VS_DEVICE_H
#define _VS_DEVICE_H

#include "vserver/base.h"
#include "vserver/device.h"
#include "vserver/debug.h"


int vs_map_device(struct vx_info *, dev_t *, umode_t);

#define	vs_map_chrdev(d,p) \
	((vs_map_device(current_vx_info(), d, S_IFCHR) & (p)) == (p))
#define	vs_map_blkdev(d,p) \
	((vs_map_device(current_vx_info(), d, S_IFBLK) & (p)) == (p))

int vs_device_permission(struct vx_info *, dev_t, umode_t, int);

#define	vs_chrdev_permission(d,p) \
	vs_device_permission(current_vx_info(), d, S_IFCHR, p)
#define	vs_blkdev_permission(d,p) \
	vs_device_permission(current_vx_info(), d, S_IFBLK, p)


#else
#warning duplicate inclusion
#endif
