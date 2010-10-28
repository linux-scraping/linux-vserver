#ifndef _VX_SCHED_CMD_H
#define _VX_SCHED_CMD_H


struct	vcmd_prio_bias {
	int32_t cpu_id;
	int32_t prio_bias;
};

#define VCMD_set_prio_bias	VC_CMD(SCHED, 4, 0)
#define VCMD_get_prio_bias	VC_CMD(SCHED, 5, 0)

#ifdef	__KERNEL__

#include <linux/compiler.h>

extern int vc_set_prio_bias(struct vx_info *, void __user *);
extern int vc_get_prio_bias(struct vx_info *, void __user *);

#endif	/* __KERNEL__ */
#endif	/* _VX_SCHED_CMD_H */
