#ifndef _VX_CVIRT_H
#define _VX_CVIRT_H


#ifdef	__KERNEL__

struct timespec;

void vx_vsi_boottime(struct timespec *);

void vx_vsi_uptime(struct timespec *, struct timespec *);


struct vx_info;

void vx_update_load(struct vx_info *);


int vx_do_syslog(int, char __user *, int);

#endif	/* __KERNEL__ */
#endif	/* _VX_CVIRT_H */
