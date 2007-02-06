#ifndef _VX_SPACE_CMD_H
#define _VX_SPACE_CMD_H


#define VCMD_enter_space_v0	VC_CMD(PROCALT, 1, 0)
#define VCMD_enter_space	VC_CMD(PROCALT, 1, 1)

#define VCMD_set_space_v0	VC_CMD(PROCALT, 3, 0)
#define VCMD_set_space		VC_CMD(PROCALT, 3, 1)

#define VCMD_get_space_mask	VC_CMD(PROCALT, 4, 0)


struct	vcmd_space_mask {
	uint64_t mask;
};


#ifdef	__KERNEL__

extern int vc_enter_space(struct vx_info *, void __user *);
extern int vc_set_space(struct vx_info *, void __user *);
extern int vc_get_space_mask(struct vx_info *, void __user *);

#endif	/* __KERNEL__ */
#endif	/* _VX_SPACE_CMD_H */
