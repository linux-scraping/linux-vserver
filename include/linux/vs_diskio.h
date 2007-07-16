#ifndef _VS_DISKIO_H
#define _VS_DISKIO_H

#include "vserver/debug.h"
#include "vserver/base.h"
#include "vserver/cacct.h"
#include "vserver/context.h"


/* disk I/O accounting */

#define vx_acc_diskio(v, t, n, s) \
	__vx_acc_diskio(v, t, n, s, __FILE__, __LINE__)

static inline void __vx_acc_diskio(struct vx_info *vxi,
	int type, int num, int size, char *_file, int _line)
{
	if (vxi) {
		atomic_long_add(num, &vxi->cacct.diskio[type].count);
		atomic_long_add(size, &vxi->cacct.diskio[type].total);
	}
}

#else
#warning duplicate inclusion
#endif
