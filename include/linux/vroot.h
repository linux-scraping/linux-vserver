
/*
 * include/linux/vroot.h
 *
 * written by Herbert Pötzl, 9/11/2002
 * ported to 2.6 by Herbert Pötzl, 30/12/2004
 *
 * Copyright (C) 2002-2007 by Herbert Pötzl.
 * Redistribution of this file is permitted under the
 * GNU General Public License.
 */

#ifndef _LINUX_VROOT_H
#define _LINUX_VROOT_H


#ifdef __KERNEL__

/* Possible states of device */
enum {
	Vr_unbound,
	Vr_bound,
};

struct vroot_device {
	int		vr_number;
	int		vr_refcnt;

	struct semaphore	vr_ctl_mutex;
	struct block_device    *vr_device;
	int			vr_state;
};


typedef struct block_device *(vroot_grb_func)(struct block_device *);

extern int register_vroot_grb(vroot_grb_func *);
extern int unregister_vroot_grb(vroot_grb_func *);

#endif /* __KERNEL__ */

#define MAX_VROOT_DEFAULT	8

/*
 * IOCTL commands --- we will commandeer 0x56 ('V')
 */

#define VROOT_SET_DEV		0x5600
#define VROOT_CLR_DEV		0x5601

#endif /* _LINUX_VROOT_H */
