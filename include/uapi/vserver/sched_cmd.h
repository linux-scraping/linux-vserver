#ifndef _UAPI_VS_SCHED_CMD_H
#define _UAPI_VS_SCHED_CMD_H


struct	vcmd_prio_bias {
	int32_t cpu_id;
	int32_t prio_bias;
};

#define VCMD_set_prio_bias	VC_CMD(SCHED, 4, 0)
#define VCMD_get_prio_bias	VC_CMD(SCHED, 5, 0)

#endif /* _UAPI_VS_SCHED_CMD_H */
