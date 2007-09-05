#ifndef _VX_TAG_CMD_H
#define _VX_TAG_CMD_H


/* vinfo commands */

#define VCMD_task_tag		VC_CMD(VINFO, 3, 0)

#ifdef	__KERNEL__
extern int vc_task_tag(uint32_t);

#endif	/* __KERNEL__ */

/* context commands */

#define VCMD_tag_migrate	VC_CMD(TAGMIG, 1, 0)

#ifdef	__KERNEL__
extern int vc_tag_migrate(uint32_t);

#endif	/* __KERNEL__ */
#endif	/* _VX_TAG_CMD_H */
