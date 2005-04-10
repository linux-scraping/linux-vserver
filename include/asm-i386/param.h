#ifndef _ASMi386_PARAM_H
#define _ASMi386_PARAM_H

#include <linux/config.h>

#ifdef __KERNEL__
# ifdef CONFIG_KERNEL_HZ
#  define HZ		CONFIG_KERNEL_HZ
# else
#  define HZ		1000		/* Internal kernel timer frequency */
# endif
# define USER_HZ	100		/* .. some user interfaces are in "ticks" */
# define CLOCKS_PER_SEC		(USER_HZ)	/* like times() */
#endif

#ifndef HZ
#define HZ 100
#endif

#define EXEC_PAGESIZE	4096

#ifndef NOGROUP
#define NOGROUP		(-1)
#endif

#define MAXHOSTNAMELEN	64	/* max length of hostname */
#define COMMAND_LINE_SIZE 256

#endif
