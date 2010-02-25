/*
 *  kernel/vserver/sysctl.c
 *
 *  Virtual Context Support
 *
 *  Copyright (C) 2004-2007  Herbert Pötzl
 *
 *  V0.01  basic structure
 *
 */

#include <linux/module.h>
#include <linux/ctype.h>
#include <linux/sysctl.h>
#include <linux/parser.h>
#include <asm/uaccess.h>

enum {
	CTL_DEBUG_ERROR		= 0,
	CTL_DEBUG_SWITCH	= 1,
	CTL_DEBUG_XID,
	CTL_DEBUG_NID,
	CTL_DEBUG_TAG,
	CTL_DEBUG_NET,
	CTL_DEBUG_LIMIT,
	CTL_DEBUG_CRES,
	CTL_DEBUG_DLIM,
	CTL_DEBUG_QUOTA,
	CTL_DEBUG_CVIRT,
	CTL_DEBUG_SPACE,
	CTL_DEBUG_MISC,
};


unsigned int vx_debug_switch	= 0;
unsigned int vx_debug_xid	= 0;
unsigned int vx_debug_nid	= 0;
unsigned int vx_debug_tag	= 0;
unsigned int vx_debug_net	= 0;
unsigned int vx_debug_limit	= 0;
unsigned int vx_debug_cres	= 0;
unsigned int vx_debug_dlim	= 0;
unsigned int vx_debug_quota	= 0;
unsigned int vx_debug_cvirt	= 0;
unsigned int vx_debug_space	= 0;
unsigned int vx_debug_misc	= 0;


static struct ctl_table_header *vserver_table_header;
static ctl_table vserver_root_table[];


void vserver_register_sysctl(void)
{
	if (!vserver_table_header) {
		vserver_table_header = register_sysctl_table(vserver_root_table);
	}

}

void vserver_unregister_sysctl(void)
{
	if (vserver_table_header) {
		unregister_sysctl_table(vserver_table_header);
		vserver_table_header = NULL;
	}
}


static int proc_dodebug(ctl_table *table, int write,
	void __user *buffer, size_t *lenp, loff_t *ppos)
{
	char		tmpbuf[20], *p, c;
	unsigned int	value;
	size_t		left, len;

	if ((*ppos && !write) || !*lenp) {
		*lenp = 0;
		return 0;
	}

	left = *lenp;

	if (write) {
		if (!access_ok(VERIFY_READ, buffer, left))
			return -EFAULT;
		p = (char *)buffer;
		while (left && __get_user(c, p) >= 0 && isspace(c))
			left--, p++;
		if (!left)
			goto done;

		if (left > sizeof(tmpbuf) - 1)
			return -EINVAL;
		if (copy_from_user(tmpbuf, p, left))
			return -EFAULT;
		tmpbuf[left] = '\0';

		for (p = tmpbuf, value = 0; '0' <= *p && *p <= '9'; p++, left--)
			value = 10 * value + (*p - '0');
		if (*p && !isspace(*p))
			return -EINVAL;
		while (left && isspace(*p))
			left--, p++;
		*(unsigned int *)table->data = value;
	} else {
		if (!access_ok(VERIFY_WRITE, buffer, left))
			return -EFAULT;
		len = sprintf(tmpbuf, "%d", *(unsigned int *)table->data);
		if (len > left)
			len = left;
		if (__copy_to_user(buffer, tmpbuf, len))
			return -EFAULT;
		if ((left -= len) > 0) {
			if (put_user('\n', (char *)buffer + len))
				return -EFAULT;
			left--;
		}
	}

done:
	*lenp -= left;
	*ppos += *lenp;
	return 0;
}

static int zero;

#define	CTL_ENTRY(ctl, name)				\
	{						\
		.procname	= #name,		\
		.data		= &vx_ ## name,		\
		.maxlen		= sizeof(int),		\
		.mode		= 0644,			\
		.proc_handler	= &proc_dodebug,	\
		.extra1		= &zero,		\
		.extra2		= &zero,		\
	}

static ctl_table vserver_debug_table[] = {
	CTL_ENTRY(CTL_DEBUG_SWITCH,	debug_switch),
	CTL_ENTRY(CTL_DEBUG_XID,	debug_xid),
	CTL_ENTRY(CTL_DEBUG_NID,	debug_nid),
	CTL_ENTRY(CTL_DEBUG_TAG,	debug_tag),
	CTL_ENTRY(CTL_DEBUG_NET,	debug_net),
	CTL_ENTRY(CTL_DEBUG_LIMIT,	debug_limit),
	CTL_ENTRY(CTL_DEBUG_CRES,	debug_cres),
	CTL_ENTRY(CTL_DEBUG_DLIM,	debug_dlim),
	CTL_ENTRY(CTL_DEBUG_QUOTA,	debug_quota),
	CTL_ENTRY(CTL_DEBUG_CVIRT,	debug_cvirt),
	CTL_ENTRY(CTL_DEBUG_SPACE,	debug_space),
	CTL_ENTRY(CTL_DEBUG_MISC,	debug_misc),
	{ 0 }
};

static ctl_table vserver_root_table[] = {
	{
		.procname	= "vserver",
		.mode		= 0555,
		.child		= vserver_debug_table
	},
	{ 0 }
};


static match_table_t tokens = {
	{ CTL_DEBUG_SWITCH,	"switch=%x"	},
	{ CTL_DEBUG_XID,	"xid=%x"	},
	{ CTL_DEBUG_NID,	"nid=%x"	},
	{ CTL_DEBUG_TAG,	"tag=%x"	},
	{ CTL_DEBUG_NET,	"net=%x"	},
	{ CTL_DEBUG_LIMIT,	"limit=%x"	},
	{ CTL_DEBUG_CRES,	"cres=%x"	},
	{ CTL_DEBUG_DLIM,	"dlim=%x"	},
	{ CTL_DEBUG_QUOTA,	"quota=%x"	},
	{ CTL_DEBUG_CVIRT,	"cvirt=%x"	},
	{ CTL_DEBUG_SPACE,	"space=%x"	},
	{ CTL_DEBUG_MISC,	"misc=%x"	},
	{ CTL_DEBUG_ERROR,	NULL		}
};

#define	HANDLE_CASE(id, name, val)				\
	case CTL_DEBUG_ ## id:					\
		vx_debug_ ## name = val;			\
		printk("vs_debug_" #name "=0x%x\n", val);	\
		break


static int __init vs_debug_setup(char *str)
{
	char *p;
	int token;

	printk("vs_debug_setup(%s)\n", str);
	while ((p = strsep(&str, ",")) != NULL) {
		substring_t args[MAX_OPT_ARGS];
		unsigned int value;

		if (!*p)
			continue;

		token = match_token(p, tokens, args);
		value = (token > 0) ? simple_strtoul(args[0].from, NULL, 0) : 0;

		switch (token) {
		HANDLE_CASE(SWITCH, switch, value);
		HANDLE_CASE(XID,    xid,    value);
		HANDLE_CASE(NID,    nid,    value);
		HANDLE_CASE(TAG,    tag,    value);
		HANDLE_CASE(NET,    net,    value);
		HANDLE_CASE(LIMIT,  limit,  value);
		HANDLE_CASE(CRES,   cres,   value);
		HANDLE_CASE(DLIM,   dlim,   value);
		HANDLE_CASE(QUOTA,  quota,  value);
		HANDLE_CASE(CVIRT,  cvirt,  value);
		HANDLE_CASE(SPACE,  space,  value);
		HANDLE_CASE(MISC,   misc,   value);
		default:
			return -EINVAL;
			break;
		}
	}
	return 1;
}

__setup("vsdebug=", vs_debug_setup);



EXPORT_SYMBOL_GPL(vx_debug_switch);
EXPORT_SYMBOL_GPL(vx_debug_xid);
EXPORT_SYMBOL_GPL(vx_debug_nid);
EXPORT_SYMBOL_GPL(vx_debug_net);
EXPORT_SYMBOL_GPL(vx_debug_limit);
EXPORT_SYMBOL_GPL(vx_debug_cres);
EXPORT_SYMBOL_GPL(vx_debug_dlim);
EXPORT_SYMBOL_GPL(vx_debug_quota);
EXPORT_SYMBOL_GPL(vx_debug_cvirt);
EXPORT_SYMBOL_GPL(vx_debug_space);
EXPORT_SYMBOL_GPL(vx_debug_misc);

