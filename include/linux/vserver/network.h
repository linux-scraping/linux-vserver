#ifndef _VX_NETWORK_H
#define _VX_NETWORK_H

#include <linux/types.h>


#define MAX_N_CONTEXT	65535	/* Arbitrary limit */

#define NB_IPV4ROOT	16


/* network flags */

#define NXF_INFO_PRIVATE	0x00000008

#define NXF_SINGLE_IP		0x00000100

#define NXF_HIDE_NETIF		0x02000000

#define NXF_STATE_SETUP		(1ULL<<32)
#define NXF_STATE_ADMIN		(1ULL<<34)

#define NXF_SC_HELPER		(1ULL<<36)
#define NXF_PERSISTENT		(1ULL<<38)

#define NXF_ONE_TIME		(0x0005ULL<<32)

#define NXF_INIT_SET		(NXF_STATE_ADMIN)


/* address types */

#define NXA_TYPE_IPV4		0x0001
#define NXA_TYPE_IPV6		0x0002

#define NXA_MOD_BCAST		0x0100
#define NXA_MOD_LBACK		0x0200

#define NXA_TYPE_ANY		((uint16_t)-1)

#define	NXA_TYPE_ADDR		0x0003
#define NXA_TYPE_MASK		0x0013
#define NXA_TYPE_RANGE		0x0023


#ifdef	__KERNEL__

#include <linux/list.h>
#include <linux/spinlock.h>
#include <linux/rcupdate.h>
#include <asm/atomic.h>


struct nx_info {
	struct hlist_node nx_hlist;	/* linked list of nxinfos */
	nid_t nx_id;			/* vnet id */
	atomic_t nx_usecnt;		/* usage count */
	atomic_t nx_tasks;		/* tasks count */
	int nx_state;			/* context state */

	uint64_t nx_flags;		/* network flag word */
	uint64_t nx_ncaps;		/* network capabilities */

	int nbipv4;
	__be32 ipv4[NB_IPV4ROOT];	/* Process can only bind to these IPs */
					/* The first one is used to connect */
					/* and for bind any service */
					/* The other must be used explicity */
	__be32 mask[NB_IPV4ROOT];	/* Netmask for each ipv4 */
					/* Used to select the proper source */
					/* address for sockets */
	__be32 v4_bcast;		/* Broadcast address to receive UDP  */
	__be32 v4_lback;		/* Loopback address */

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

#endif	/* __KERNEL__ */
#else	/* _VX_NETWORK_H */
#warning duplicate inclusion
#endif	/* _VX_NETWORK_H */
