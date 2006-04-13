/*
 *  linux/kernel/init.c
 *
 *  Virtual Server Init
 *
 *  Copyright (C) 2004-2005  Herbert P�tzl
 *
 *  V0.01  basic structure
 *
 */

#include <linux/errno.h>
#include <linux/init.h>
#include <linux/module.h>

int	vserver_register_sysctl(void);
void	vserver_unregister_sysctl(void);


static int __init init_vserver(void)
{
	int ret = 0;

#ifdef	CONFIG_VSERVER_DEBUG
	vserver_register_sysctl();
#endif
	return ret;
}


static void __exit exit_vserver(void)
{

#ifdef	CONFIG_VSERVER_DEBUG
	vserver_unregister_sysctl();
#endif
	return;
}


module_init(init_vserver);
module_exit(exit_vserver);

