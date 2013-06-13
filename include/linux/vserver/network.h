#ifndef _VSERVER_NETWORK_H
#define _VSERVER_NETWORK_H


#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <asm/atomic.h>
#include <uapi/vserver/network.h>

struct nx_addr_v4 {
	struct nx_addr_v4 *next;
	struct in_addr ip[2];
	struct in_addr mask;
	uint16_t type;
	uint16_t flags;
};

struct nx_addr_v6 {
	struct nx_addr_v6 *next;
	struct in6_addr ip;
	struct in6_addr mask;
	uint32_t prefix;
	uint16_t type;
	uint16_t flags;
};

struct nx_info {
	struct hlist_node nx_hlist;	/* linked list of nxinfos */
	vnid_t nx_id;			/* vnet id */
	atomic_t nx_usecnt;		/* usage count */
	atomic_t nx_tasks;		/* tasks count */
	int nx_state;			/* context state */

	uint64_t nx_flags;		/* network flag word */
	uint64_t nx_ncaps;		/* network capabilities */

	spinlock_t addr_lock;		/* protect address changes */
	struct in_addr v4_lback;	/* Loopback address */
	struct in_addr v4_bcast;	/* Broadcast address */
	struct nx_addr_v4 v4;		/* First/Single ipv4 address */
#ifdef	CONFIG_IPV6
	struct nx_addr_v6 v6;		/* First/Single ipv6 address */
#endif
	char nx_name[65];		/* network context name */
};


/* status flags */

#define NXS_HASHED      0x0001
#define NXS_SHUTDOWN    0x0100
#define NXS_RELEASED    0x8000

extern struct nx_info *lookup_nx_info(int);

extern int get_nid_list(int, unsigned int *, int);
extern int nid_is_hashed(vnid_t);

extern int nx_migrate_task(struct task_struct *, struct nx_info *);

extern long vs_net_change(struct nx_info *, unsigned int);

struct sock;


#define NX_IPV4(n)	((n)->v4.type != NXA_TYPE_NONE)
#ifdef  CONFIG_IPV6
#define NX_IPV6(n)	((n)->v6.type != NXA_TYPE_NONE)
#else
#define NX_IPV6(n)	(0)
#endif

#endif	/* _VSERVER_NETWORK_H */
