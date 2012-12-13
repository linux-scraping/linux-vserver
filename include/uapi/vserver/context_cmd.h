#ifndef _UAPI_VS_CONTEXT_CMD_H
#define _UAPI_VS_CONTEXT_CMD_H


/* vinfo commands */

#define VCMD_task_xid		VC_CMD(VINFO, 1, 0)


#define VCMD_vx_info		VC_CMD(VINFO, 5, 0)

struct	vcmd_vx_info_v0 {
	uint32_t xid;
	uint32_t initpid;
	/* more to come */
};


#define VCMD_ctx_stat		VC_CMD(VSTAT, 0, 0)

struct	vcmd_ctx_stat_v0 {
	uint32_t usecnt;
	uint32_t tasks;
	/* more to come */
};


/* context commands */

#define VCMD_ctx_create_v0	VC_CMD(VPROC, 1, 0)
#define VCMD_ctx_create		VC_CMD(VPROC, 1, 1)

struct	vcmd_ctx_create {
	uint64_t flagword;
};

#define VCMD_ctx_migrate_v0	VC_CMD(PROCMIG, 1, 0)
#define VCMD_ctx_migrate	VC_CMD(PROCMIG, 1, 1)

struct	vcmd_ctx_migrate {
	uint64_t flagword;
};



/* flag commands */

#define VCMD_get_cflags		VC_CMD(FLAGS, 1, 0)
#define VCMD_set_cflags		VC_CMD(FLAGS, 2, 0)

struct	vcmd_ctx_flags_v0 {
	uint64_t flagword;
	uint64_t mask;
};



/* context caps commands */

#define VCMD_get_ccaps		VC_CMD(FLAGS, 3, 1)
#define VCMD_set_ccaps		VC_CMD(FLAGS, 4, 1)

struct	vcmd_ctx_caps_v1 {
	uint64_t ccaps;
	uint64_t cmask;
};



/* bcaps commands */

#define VCMD_get_bcaps		VC_CMD(FLAGS, 9, 0)
#define VCMD_set_bcaps		VC_CMD(FLAGS, 10, 0)

struct	vcmd_bcaps {
	uint64_t bcaps;
	uint64_t bmask;
};



/* umask commands */

#define VCMD_get_umask		VC_CMD(FLAGS, 13, 0)
#define VCMD_set_umask		VC_CMD(FLAGS, 14, 0)

struct	vcmd_umask {
	uint64_t umask;
	uint64_t mask;
};



/* wmask commands */

#define VCMD_get_wmask		VC_CMD(FLAGS, 15, 0)
#define VCMD_set_wmask		VC_CMD(FLAGS, 16, 0)

struct	vcmd_wmask {
	uint64_t wmask;
	uint64_t mask;
};



/* OOM badness */

#define VCMD_get_badness	VC_CMD(MEMCTRL, 5, 0)
#define VCMD_set_badness	VC_CMD(MEMCTRL, 6, 0)

struct	vcmd_badness_v0 {
	int64_t bias;
};

#endif /* _UAPI_VS_CONTEXT_CMD_H */
