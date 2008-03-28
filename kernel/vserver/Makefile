#
# Makefile for the Linux vserver routines.
#


obj-y		+= vserver.o

vserver-y	:= switch.o context.o space.o sched.o network.o inode.o \
		   limit.o cvirt.o cacct.o signal.o helper.o init.o \
		   dlimit.o tag.o

vserver-$(CONFIG_INET) += inet.o
vserver-$(CONFIG_PROC_FS) += proc.o
vserver-$(CONFIG_VSERVER_DEBUG) += sysctl.o debug.o
vserver-$(CONFIG_VSERVER_HISTORY) += history.o
vserver-$(CONFIG_VSERVER_MONITOR) += monitor.o
vserver-$(CONFIG_VSERVER_DEVICE) += device.o

