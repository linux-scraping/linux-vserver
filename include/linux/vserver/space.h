#ifndef _VX_SPACE_H
#define _VX_SPACE_H

#include <linux/types.h>

struct vx_info;

int vx_set_space(struct vx_info *vxi, unsigned long mask, unsigned index);

#else	/* _VX_SPACE_H */
#warning duplicate inclusion
#endif	/* _VX_SPACE_H */
