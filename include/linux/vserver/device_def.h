#ifndef _VSERVER_DEVICE_DEF_H
#define _VSERVER_DEVICE_DEF_H

#include <linux/types.h>

struct vx_dmap_target {
	dev_t target;
	uint32_t flags;
};

struct _vx_device {
#ifdef CONFIG_VSERVER_DEVICE
	struct vx_dmap_target targets[2];
#endif
};

#endif	/* _VSERVER_DEVICE_DEF_H */
