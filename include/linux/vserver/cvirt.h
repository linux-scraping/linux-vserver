#ifndef _VSERVER_CVIRT_H
#define _VSERVER_CVIRT_H

struct timespec;

void vx_vsi_boottime(struct timespec *);

void vx_vsi_uptime(struct timespec *, struct timespec *);


struct vx_info;

void vx_update_load(struct vx_info *);


int vx_do_syslog(int, char __user *, int);

#endif	/* _VSERVER_CVIRT_H */
