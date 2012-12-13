#ifndef _UAPI_VS_DLIMIT_CMD_H
#define _UAPI_VS_DLIMIT_CMD_H


/*  dlimit vserver commands */

#define VCMD_add_dlimit		VC_CMD(DLIMIT, 1, 0)
#define VCMD_rem_dlimit		VC_CMD(DLIMIT, 2, 0)

#define VCMD_set_dlimit		VC_CMD(DLIMIT, 5, 0)
#define VCMD_get_dlimit		VC_CMD(DLIMIT, 6, 0)

struct	vcmd_ctx_dlimit_base_v0 {
	const char __user *name;
	uint32_t flags;
};

struct	vcmd_ctx_dlimit_v0 {
	const char __user *name;
	uint32_t space_used;			/* used space in kbytes */
	uint32_t space_total;			/* maximum space in kbytes */
	uint32_t inodes_used;			/* used inodes */
	uint32_t inodes_total;			/* maximum inodes */
	uint32_t reserved;			/* reserved for root in % */
	uint32_t flags;
};

#define CDLIM_UNSET		((uint32_t)0UL)
#define CDLIM_INFINITY		((uint32_t)~0UL)
#define CDLIM_KEEP		((uint32_t)~1UL)

#define DLIME_UNIT	0
#define DLIME_KILO	1
#define DLIME_MEGA	2
#define DLIME_GIGA	3

#define DLIMF_SHIFT	0x10

#define DLIMS_USED	0
#define DLIMS_TOTAL	2

static inline
uint64_t dlimit_space_32to64(uint32_t val, uint32_t flags, int shift)
{
	int exp = (flags & DLIMF_SHIFT) ?
		(flags >> shift) & DLIME_GIGA : DLIME_KILO;
	return ((uint64_t)val) << (10 * exp);
}

static inline
uint32_t dlimit_space_64to32(uint64_t val, uint32_t *flags, int shift)
{
	int exp = 0;

	if (*flags & DLIMF_SHIFT) {
		while (val > (1LL << 32) && (exp < 3)) {
			val >>= 10;
			exp++;
		}
		*flags &= ~(DLIME_GIGA << shift);
		*flags |= exp << shift;
	} else
		val >>= 10;
	return val;
}

#endif /* _UAPI_VS_DLIMIT_CMD_H */
