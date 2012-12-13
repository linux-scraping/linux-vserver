#ifndef _VSERVER_PERCPU_H
#define _VSERVER_PERCPU_H

#include "cvirt_def.h"
#include "sched_def.h"

struct	_vx_percpu {
	struct _vx_cvirt_pc cvirt;
	struct _vx_sched_pc sched;
};

#define	PERCPU_PERCTX	(sizeof(struct _vx_percpu))

#endif	/* _VSERVER_PERCPU_H */
