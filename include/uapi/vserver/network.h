#ifndef _UAPI_VS_NETWORK_H
#define _UAPI_VS_NETWORK_H

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

#define NXC_TUN_CREATE		0x00000001

#define NXC_RAW_ICMP		0x00000100

#define NXC_MULTICAST		0x00001000


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

#endif /* _UAPI_VS_NETWORK_H */
