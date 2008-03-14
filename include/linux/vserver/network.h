#ifndef _VX_NETWORK_H
#define _VX_NETWORK_H

#include <linux/types.h>


#define MAX_N_CONTEXT	65535	/* Arbitrary limit */


/* network flags */

#define NXF_INFO_PRIVATE	0x00000008

#define NXF_SINGLE_IP		0x00000100
#define NXF_LBACK_REMAP		0x00000200
#define NXF_LBACK_ALLOW		0x00000400

#define NXF_HIDE_NETIF		0x02000000
#define NXF_HIDE_LBACK		0x04000000

#define NXF_STATE_SETUP		(1ULL << 32)
#define NXF_STATE_ADMIN		(1ULL << 34)

#define NXF_SC_HELPER		(1ULL << 36)
#define NXF_PERSISTENT		(1ULL << 38)

#define NXF_ONE_TIME		(0x0005ULL << 32)


#define	NXF_INIT_SET		(__nxf_init_set())

static inline uint64_t __nxf_init_set(void) {
	return	  NXF_STATE_ADMIN
#ifdef	CONFIG_VSERVER_AUTO_LBACK
		| NXF_LBACK_REMAP
		| NXF_HIDE_LBACK
#endif
#ifdef	CONFIG_VSERVER_AUTO_SINGLE
		| NXF_SINGLE_IP
#endif
		| NXF_HIDE_NETIF;
}


/* network caps */

#define NXC_RAW_ICMP		0x00000100


/* address types */

#define NXA_TYPE_IPV4		0x0001
#define NXA_TYPE_IPV6		0x0002

#define NXA_TYPE_NONE		0x0000
#define NXA_TYPE_ANY		0x00FF

#define NXA_TYPE_ADDR		0x0010
#define NXA_TYPE_MASK		0x0020
#define NXA_TYPE_RANGE		0x0040

#define NXA_MASK_ALL		(NXA_TYPE_ADDR | NXA_TYPE_MASK | NXA_TYPE_RANGE)

#define NXA_MOD_BCAST		0x0100
#define NXA_MOD_LBACK		0x0200

#define NXA_LOOPBACK		0x1000

#define NXA_MASK_BIND		(NXA_MASK_ALL | NXA_MOD_BCAST | NXA_MOD_LBACK)
#define NXA_MASK_SHOW		(NXA_MASK_ALL | NXA_LOOPBACK)

#ifdef	__KERNEL__

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <asm/atomic.h>

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
	nid_t nx_id;			/* vnet id */
	atomic_t nx_usecnt;		/* usage count */
	atomic_t nx_tasks;		/* tasks count */
	int nx_state;			/* context state */

	uint64_t nx_flags;		/* network flag word */
	uint64_t nx_ncaps;		/* network capabilities */

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
extern int nid_is_hashed(nid_t);

extern int nx_migrate_task(struct task_struct *, struct nx_info *);

extern long vs_net_change(struct nx_info *, unsigned int);

struct sock;


#define NX_IPV4(n)	((n)->v4.type != NXA_TYPE_NONE)
#ifdef  CONFIG_IPV6
#define NX_IPV6(n)	((n)->v6.type != NXA_TYPE_NONE)
#else
#define NX_IPV6(n)	(0)
#endif

#endif	/* __KERNEL__ */
#endif	/* _VX_NETWORK_H */
