#ifndef _VSERVER_INODE_H
#define _VSERVER_INODE_H

#include <uapi/vserver/inode.h>


#ifdef	CONFIG_VSERVER_PROC_SECURE
#define IATTR_PROC_DEFAULT	( IATTR_ADMIN | IATTR_HIDE )
#define IATTR_PROC_SYMLINK	( IATTR_ADMIN )
#else
#define IATTR_PROC_DEFAULT	( IATTR_ADMIN )
#define IATTR_PROC_SYMLINK	( IATTR_ADMIN )
#endif

#define vx_hide_check(c, m)	(((m) & IATTR_HIDE) ? vx_check(c, m) : 1)

#else	/* _VSERVER_INODE_H */
#warning duplicate inclusion
#endif	/* _VSERVER_INODE_H */
