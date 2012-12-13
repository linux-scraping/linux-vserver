#ifndef _VSERVER_NETWORK_CMD_H
#define _VSERVER_NETWORK_CMD_H

#include <uapi/vserver/network_cmd.h>

extern int vc_task_nid(uint32_t);

extern int vc_nx_info(struct nx_info *, void __user *);

extern int vc_net_create(uint32_t, void __user *);
extern int vc_net_migrate(struct nx_info *, void __user *);

extern int vc_net_add(struct nx_info *, void __user *);
extern int vc_net_remove(struct nx_info *, void __user *);

extern int vc_net_add_ipv4_v1(struct nx_info *, void __user *);
extern int vc_net_add_ipv4(struct nx_info *, void __user *);

extern int vc_net_rem_ipv4_v1(struct nx_info *, void __user *);
extern int vc_net_rem_ipv4(struct nx_info *, void __user *);

extern int vc_net_add_ipv6(struct nx_info *, void __user *);
extern int vc_net_remove_ipv6(struct nx_info *, void __user *);

extern int vc_add_match_ipv4(struct nx_info *, void __user *);
extern int vc_get_match_ipv4(struct nx_info *, void __user *);

extern int vc_add_match_ipv6(struct nx_info *, void __user *);
extern int vc_get_match_ipv6(struct nx_info *, void __user *);

extern int vc_get_nflags(struct nx_info *, void __user *);
extern int vc_set_nflags(struct nx_info *, void __user *);

extern int vc_get_ncaps(struct nx_info *, void __user *);
extern int vc_set_ncaps(struct nx_info *, void __user *);

#endif	/* _VSERVER_CONTEXT_CMD_H */
