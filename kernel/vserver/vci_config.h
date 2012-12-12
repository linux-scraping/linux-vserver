
/*  interface version */

#define VCI_VERSION		0x00020308


enum {
	VCI_KCBIT_NO_DYNAMIC = 0,

	VCI_KCBIT_PROC_SECURE = 4,
	/* VCI_KCBIT_HARDCPU = 5, */
	/* VCI_KCBIT_IDLELIMIT = 6, */
	/* VCI_KCBIT_IDLETIME = 7, */

	VCI_KCBIT_COWBL = 8,
	VCI_KCBIT_FULLCOWBL = 9,
	VCI_KCBIT_SPACES = 10,
	VCI_KCBIT_NETV2 = 11,
	VCI_KCBIT_MEMCG = 12,
	VCI_KCBIT_MEMCG_SWAP = 13,

	VCI_KCBIT_DEBUG = 16,
	VCI_KCBIT_HISTORY = 20,
	VCI_KCBIT_TAGGED = 24,
	VCI_KCBIT_PPTAG = 28,

	VCI_KCBIT_MORE = 31,
};


static inline uint32_t vci_kernel_config(void)
{
	return
	(1 << VCI_KCBIT_NO_DYNAMIC) |

	/* configured features */
#ifdef	CONFIG_VSERVER_PROC_SECURE
	(1 << VCI_KCBIT_PROC_SECURE) |
#endif
#ifdef	CONFIG_VSERVER_COWBL
	(1 << VCI_KCBIT_COWBL) |
	(1 << VCI_KCBIT_FULLCOWBL) |
#endif
	(1 << VCI_KCBIT_SPACES) |
	(1 << VCI_KCBIT_NETV2) |
#ifdef	CONFIG_MEMCG
	(1 << VCI_KCBIT_MEMCG) |
#endif
#ifdef	CONFIG_MEMCG_SWAP
	(1 << VCI_KCBIT_MEMCG_SWAP) |
#endif

	/* debug options */
#ifdef	CONFIG_VSERVER_DEBUG
	(1 << VCI_KCBIT_DEBUG) |
#endif
#ifdef	CONFIG_VSERVER_HISTORY
	(1 << VCI_KCBIT_HISTORY) |
#endif

	/* inode context tagging */
#if	defined(CONFIG_TAGGING_NONE)
	(0 << VCI_KCBIT_TAGGED) |
#elif	defined(CONFIG_TAGGING_UID16)
	(1 << VCI_KCBIT_TAGGED) |
#elif	defined(CONFIG_TAGGING_GID16)
	(2 << VCI_KCBIT_TAGGED) |
#elif	defined(CONFIG_TAGGING_ID24)
	(3 << VCI_KCBIT_TAGGED) |
#elif	defined(CONFIG_TAGGING_INTERN)
	(4 << VCI_KCBIT_TAGGED) |
#elif	defined(CONFIG_TAGGING_RUNTIME)
	(5 << VCI_KCBIT_TAGGED) |
#else
	(7 << VCI_KCBIT_TAGGED) |
#endif
	(1 << VCI_KCBIT_PPTAG) |
	0;
}

