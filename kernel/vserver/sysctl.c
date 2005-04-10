/*
 *  linux/kernel/sysctl.c
 *
 *  Virtual Context Support
 *
 *  Copyright (C) 2004-2005  Herbert Pötzl
 *
 *  V0.01  basic structure
 *
 */

#include <linux/config.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ctype.h>
#include <linux/sysctl.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/unistd.h>


#define CTL_VSERVER	4242    /* unused? */

enum {
	CTL_DEBUG_SWITCH = 1,
	CTL_DEBUG_XID,
	CTL_DEBUG_NID,
	CTL_DEBUG_NET,
	CTL_DEBUG_LIMIT,
	CTL_DEBUG_DLIM,
	CTL_DEBUG_CVIRT,
};


unsigned int vx_debug_switch = 0;
unsigned int vx_debug_xid = 0;
unsigned int vx_debug_nid = 0;
unsigned int vx_debug_net = 0;
unsigned int vx_debug_limit = 0;
unsigned int vx_debug_dlim = 0;
unsigned int vx_debug_cvirt = 0;


static struct ctl_table_header *vserver_table_header;
static ctl_table vserver_table[];


void vserver_register_sysctl(void)
{
	if (!vserver_table_header) {
		vserver_table_header = register_sysctl_table(vserver_table, 1);
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
	struct file *filp, void __user *buffer, size_t *lenp, loff_t *ppos)
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
		p = (char *) buffer;
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
		*(unsigned int *) table->data = value;
	} else {
		if (!access_ok(VERIFY_WRITE, buffer, left))
			return -EFAULT;
		len = sprintf(tmpbuf, "%d", *(unsigned int *) table->data);
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



static ctl_table debug_table[] = {
	{
		.ctl_name	= CTL_DEBUG_SWITCH,
		.procname	= "debug_switch",
		.data		= &vx_debug_switch,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dodebug
	},
	{
		.ctl_name	= CTL_DEBUG_XID,
		.procname	= "debug_xid",
		.data		= &vx_debug_xid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dodebug
	},
	{
		.ctl_name	= CTL_DEBUG_NID,
		.procname	= "debug_nid",
		.data		= &vx_debug_nid,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dodebug
	},
	{
		.ctl_name	= CTL_DEBUG_NET,
		.procname	= "debug_net",
		.data		= &vx_debug_net,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dodebug
	},
	{
		.ctl_name	= CTL_DEBUG_LIMIT,
		.procname	= "debug_limit",
		.data		= &vx_debug_limit,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dodebug
	},
	{
		.ctl_name	= CTL_DEBUG_DLIM,
		.procname	= "debug_dlim",
		.data		= &vx_debug_dlim,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dodebug
	},
	{
		.ctl_name	= CTL_DEBUG_CVIRT,
		.procname	= "debug_cvirt",
		.data		= &vx_debug_cvirt,
		.maxlen		= sizeof(int),
		.mode		= 0644,
		.proc_handler	= &proc_dodebug
	},
	{ .ctl_name = 0 }
};

static ctl_table vserver_table[] = {
	{
		.ctl_name	= CTL_VSERVER,
		.procname	= "vserver",
		.mode		= 0555,
		.child		= debug_table
	},
	{ .ctl_name = 0 }
};


EXPORT_SYMBOL_GPL(vx_debug_switch);
EXPORT_SYMBOL_GPL(vx_debug_xid);
EXPORT_SYMBOL_GPL(vx_debug_nid);
EXPORT_SYMBOL_GPL(vx_debug_net);
EXPORT_SYMBOL_GPL(vx_debug_limit);
EXPORT_SYMBOL_GPL(vx_debug_dlim);
EXPORT_SYMBOL_GPL(vx_debug_cvirt);

