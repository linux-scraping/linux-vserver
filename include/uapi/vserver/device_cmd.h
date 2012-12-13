#ifndef _UAPI_VS_DEVICE_CMD_H
#define _UAPI_VS_DEVICE_CMD_H


/*  device vserver commands */

#define VCMD_set_mapping	VC_CMD(DEVICE, 1, 0)
#define VCMD_unset_mapping	VC_CMD(DEVICE, 2, 0)

struct	vcmd_set_mapping_v0 {
	const char __user *device;
	const char __user *target;
	uint32_t flags;
};

#endif /* _UAPI_VS_DEVICE_CMD_H */
