#ifndef _DX_TAG_H
#define _DX_TAG_H

#include <linux/types.h>
#include <linux/uidgid.h>


#define DX_TAG(in)	(IS_TAGGED(in))


#ifdef CONFIG_TAG_NFSD
#define DX_TAG_NFSD	1
#else
#define DX_TAG_NFSD	0
#endif


#ifdef CONFIG_TAGGING_NONE

#define MAX_UID		0xFFFFFFFF
#define MAX_GID		0xFFFFFFFF

#define INOTAG_TAG(cond, uid, gid, tag)	(0)

#define TAGINO_UID(cond, uid, tag)	(uid)
#define TAGINO_GID(cond, gid, tag)	(gid)

#endif


#ifdef CONFIG_TAGGING_GID16

#define MAX_UID		0xFFFFFFFF
#define MAX_GID		0x0000FFFF

#define INOTAG_TAG(cond, uid, gid, tag)	\
	((cond) ? (((gid) >> 16) & 0xFFFF) : 0)

#define TAGINO_UID(cond, uid, tag)	(uid)
#define TAGINO_GID(cond, gid, tag)	\
	((cond) ? (((gid) & 0xFFFF) | ((tag) << 16)) : (gid))

#endif


#ifdef CONFIG_TAGGING_ID24

#define MAX_UID		0x00FFFFFF
#define MAX_GID		0x00FFFFFF

#define INOTAG_TAG(cond, uid, gid, tag)	\
	((cond) ? ((((uid) >> 16) & 0xFF00) | (((gid) >> 24) & 0xFF)) : 0)

#define TAGINO_UID(cond, uid, tag)	\
	((cond) ? (((uid) & 0xFFFFFF) | (((tag) & 0xFF00) << 16)) : (uid))
#define TAGINO_GID(cond, gid, tag)	\
	((cond) ? (((gid) & 0xFFFFFF) | (((tag) & 0x00FF) << 24)) : (gid))

#endif


#ifdef CONFIG_TAGGING_UID16

#define MAX_UID		0x0000FFFF
#define MAX_GID		0xFFFFFFFF

#define INOTAG_TAG(cond, uid, gid, tag)	\
	((cond) ? (((uid) >> 16) & 0xFFFF) : 0)

#define TAGINO_UID(cond, uid, tag)	\
	((cond) ? (((uid) & 0xFFFF) | ((tag) << 16)) : (uid))
#define TAGINO_GID(cond, gid, tag)	(gid)

#endif


#ifdef CONFIG_TAGGING_INTERN

#define MAX_UID		0xFFFFFFFF
#define MAX_GID		0xFFFFFFFF

#define INOTAG_TAG(cond, uid, gid, tag)	\
	((cond) ? (tag) : 0)

#define TAGINO_UID(cond, uid, tag)	(uid)
#define TAGINO_GID(cond, gid, tag)	(gid)

#endif


#ifndef CONFIG_TAGGING_NONE
#define dx_current_fstag(sb)	\
	((sb)->s_flags & MS_TAGGED ? dx_current_tag() : 0)
#else
#define dx_current_fstag(sb)	(0)
#endif

#ifndef CONFIG_TAGGING_INTERN
#define TAGINO_TAG(cond, tag)	(0)
#else
#define TAGINO_TAG(cond, tag)	((cond) ? (tag) : 0)
#endif

#define TAGINO_KUID(cond, kuid, ktag)	\
	KUIDT_INIT(TAGINO_UID(cond, __kuid_val(kuid), __ktag_val(ktag)))
#define TAGINO_KGID(cond, kgid, ktag)	\
	KGIDT_INIT(TAGINO_GID(cond, __kgid_val(kgid), __ktag_val(ktag)))
#define TAGINO_KTAG(cond, ktag)		\
	KTAGT_INIT(TAGINO_TAG(cond, __ktag_val(ktag)))


#define INOTAG_UID(cond, uid, gid)	\
	((cond) ? ((uid) & MAX_UID) : (uid))
#define INOTAG_GID(cond, uid, gid)	\
	((cond) ? ((gid) & MAX_GID) : (gid))

#define INOTAG_KUID(cond, kuid, kgid)	\
	KUIDT_INIT(INOTAG_UID(cond, __kuid_val(kuid), __kgid_val(kgid)))
#define INOTAG_KGID(cond, kuid, kgid)	\
	KGIDT_INIT(INOTAG_GID(cond, __kuid_val(kuid), __kgid_val(kgid)))
#define INOTAG_KTAG(cond, kuid, kgid, ktag) \
	KTAGT_INIT(INOTAG_TAG(cond, \
		__kuid_val(kuid), __kgid_val(kgid), __ktag_val(ktag)))


static inline uid_t dx_map_uid(uid_t uid)
{
	if ((uid > MAX_UID) && (uid != -1))
		uid = -2;
	return (uid & MAX_UID);
}

static inline gid_t dx_map_gid(gid_t gid)
{
	if ((gid > MAX_GID) && (gid != -1))
		gid = -2;
	return (gid & MAX_GID);
}

struct peer_tag {
	int32_t xid;
	int32_t nid;
};

#define dx_notagcheck(sb) ((sb) && ((sb)->s_flags & MS_NOTAGCHECK))

int dx_parse_tag(char *string, vtag_t *tag, int remove, int *mnt_flags,
		 unsigned long *flags);

#ifdef	CONFIG_PROPAGATE

void __dx_propagate_tag(struct nameidata *nd, struct inode *inode);

#define dx_propagate_tag(n, i)	__dx_propagate_tag(n, i)

#else
#define dx_propagate_tag(n, i)	do { } while (0)
#endif

#endif /* _DX_TAG_H */
