#ifndef _UAPI_VS_LIMIT_CMD_H
#define _UAPI_VS_LIMIT_CMD_H


/*  rlimit vserver commands */

#define VCMD_get_rlimit		VC_CMD(RLIMIT, 1, 0)
#define VCMD_set_rlimit		VC_CMD(RLIMIT, 2, 0)
#define VCMD_get_rlimit_mask	VC_CMD(RLIMIT, 3, 0)
#define VCMD_reset_hits		VC_CMD(RLIMIT, 7, 0)
#define VCMD_reset_minmax	VC_CMD(RLIMIT, 9, 0)

struct	vcmd_ctx_rlimit_v0 {
	uint32_t id;
	uint64_t minimum;
	uint64_t softlimit;
	uint64_t maximum;
};

struct	vcmd_ctx_rlimit_mask_v0 {
	uint32_t minimum;
	uint32_t softlimit;
	uint32_t maximum;
};

#define VCMD_rlimit_stat	VC_CMD(VSTAT, 1, 0)

struct	vcmd_rlimit_stat_v0 {
	uint32_t id;
	uint32_t hits;
	uint64_t value;
	uint64_t minimum;
	uint64_t maximum;
};

#define CRLIM_UNSET		(0ULL)
#define CRLIM_INFINITY		(~0ULL)
#define CRLIM_KEEP		(~1ULL)

#endif /* _UAPI_VS_LIMIT_CMD_H */
