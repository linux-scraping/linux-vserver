#ifndef _VSERVER_DEBUG_CMD_H
#define _VSERVER_DEBUG_CMD_H

#include <uapi/vserver/debug_cmd.h>


#ifdef	CONFIG_COMPAT

#include <asm/compat.h>

struct	vcmd_read_history_v0_x32 {
	uint32_t index;
	uint32_t count;
	compat_uptr_t data_ptr;
};

struct	vcmd_read_monitor_v0_x32 {
	uint32_t index;
	uint32_t count;
	compat_uptr_t data_ptr;
};

#endif  /* CONFIG_COMPAT */

extern int vc_dump_history(uint32_t);

extern int vc_read_history(uint32_t, void __user *);
extern int vc_read_monitor(uint32_t, void __user *);

#ifdef	CONFIG_COMPAT

extern int vc_read_history_x32(uint32_t, void __user *);
extern int vc_read_monitor_x32(uint32_t, void __user *);

#endif  /* CONFIG_COMPAT */

#endif	/* _VSERVER_DEBUG_CMD_H */
