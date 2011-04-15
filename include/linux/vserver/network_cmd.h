#ifndef _VX_NETWORK_CMD_H
#define _VX_NETWORK_CMD_H


/* vinfo commands */

#define VCMD_task_nid		VC_CMD(VINFO, 2, 0)

#ifdef	__KERNEL__
extern int vc_task_nid(uint32_t);

#endif	/* __KERNEL__ */

#define VCMD_nx_info		VC_CMD(VINFO, 6, 0)

struct	vcmd_nx_info_v0 {
	uint32_t nid;
	/* more to come */
};

#ifdef	__KERNEL__
extern int vc_nx_info(struct nx_info *, void __user *);

#endif	/* __KERNEL__ */

#include <linux/in.h>
#include <linux/in6.h>

#define VCMD_net_create_v0	VC_CMD(VNET, 1, 0)
#define VCMD_net_create		VC_CMD(VNET, 1, 1)

struct  vcmd_net_create {
	uint64_t flagword;
};

#define VCMD_net_migrate	VC_CMD(NETMIG, 1, 0)

#define VCMD_net_add		VC_CMD(NETALT, 1, 0)
#define VCMD_net_remove		VC_CMD(NETALT, 2, 0)

struct	vcmd_net_addr_v0 {
	uint16_t type;
	uint16_t count;
	struct in_addr ip[4];
	struct in_addr mask[4];
};

#define VCMD_net_add_ipv4_v1	VC_CMD(NETALT, 1, 1)
#define VCMD_net_rem_ipv4_v1	VC_CMD(NETALT, 2, 1)

struct	vcmd_net_addr_ipv4_v1 {
	uint16_t type;
	uint16_t flags;
	struct in_addr ip;
	struct in_addr mask;
};

#define VCMD_net_add_ipv4	VC_CMD(NETALT, 1, 2)
#define VCMD_net_rem_ipv4	VC_CMD(NETALT, 2, 2)

struct	vcmd_net_addr_ipv4_v2 {
	uint16_t type;
	uint16_t flags;
	struct in_addr ip;
	struct in_addr ip2;
	struct in_addr mask;
};

#define VCMD_net_add_ipv6	VC_CMD(NETALT, 3, 1)
#define VCMD_net_remove_ipv6	VC_CMD(NETALT, 4, 1)

struct	vcmd_net_addr_ipv6_v1 {
	uint16_t type;
	uint16_t flags;
	uint32_t prefix;
	struct in6_addr ip;
	struct in6_addr mask;
};

#define VCMD_add_match_ipv4	VC_CMD(NETALT, 5, 0)
#define VCMD_get_match_ipv4	VC_CMD(NETALT, 6, 0)

struct	vcmd_match_ipv4_v0 {
	uint16_t type;
	uint16_t flags;
	uint16_t parent;
	uint16_t prefix;
	struct in_addr ip;
	struct in_addr ip2;
	struct in_addr mask;
};

#define VCMD_add_match_ipv6	VC_CMD(NETALT, 7, 0)
#define VCMD_get_match_ipv6	VC_CMD(NETALT, 8, 0)

struct	vcmd_match_ipv6_v0 {
	uint16_t type;
	uint16_t flags;
	uint16_t parent;
	uint16_t prefix;
	struct in6_addr ip;
	struct in6_addr ip2;
	struct in6_addr mask;
};


#ifdef	__KERNEL__
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

#endif	/* __KERNEL__ */


/* flag commands */

#define VCMD_get_nflags		VC_CMD(FLAGS, 5, 0)
#define VCMD_set_nflags		VC_CMD(FLAGS, 6, 0)

struct	vcmd_net_flags_v0 {
	uint64_t flagword;
	uint64_t mask;
};

#ifdef	__KERNEL__
extern int vc_get_nflags(struct nx_info *, void __user *);
extern int vc_set_nflags(struct nx_info *, void __user *);

#endif	/* __KERNEL__ */


/* network caps commands */

#define VCMD_get_ncaps		VC_CMD(FLAGS, 7, 0)
#define VCMD_set_ncaps		VC_CMD(FLAGS, 8, 0)

struct	vcmd_net_caps_v0 {
	uint64_t ncaps;
	uint64_t cmask;
};

#ifdef	__KERNEL__
extern int vc_get_ncaps(struct nx_info *, void __user *);
extern int vc_set_ncaps(struct nx_info *, void __user *);

#endif	/* __KERNEL__ */
#endif	/* _VX_CONTEXT_CMD_H */
