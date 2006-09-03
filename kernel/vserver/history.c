/*
 *  kernel/vserver/history.c
 *
 *  Virtual Context History Backtrace
 *
 *  Copyright (C) 2004-2005  Herbert P�tzl
 *
 *  V0.01  basic structure
 *  V0.02  hash/unhash and trace
 *  V0.03  preemption fixes
 *
 */

#include <linux/errno.h>
#include <linux/module.h>
#include <linux/types.h>
#include <linux/ctype.h>

#include <asm/uaccess.h>
#include <asm/atomic.h>
#include <asm/unistd.h>

#include <linux/vserver/debug.h>


#ifdef	CONFIG_VSERVER_HISTORY
#define VXH_SIZE	CONFIG_VSERVER_HISTORY_SIZE
#else
#define VXH_SIZE	64
#endif

struct _vx_history {
	unsigned int counter;

	struct _vx_hist_entry entry[VXH_SIZE+1];
};


DEFINE_PER_CPU(struct _vx_history, vx_history_buffer);

unsigned volatile int vxh_active = 1;

static atomic_t sequence = ATOMIC_INIT(0);


/*	vxh_advance()

	* requires disabled preemption				*/

struct _vx_hist_entry *vxh_advance(void *loc)
{
	unsigned int cpu = smp_processor_id();
	struct _vx_history *hist = &per_cpu(vx_history_buffer, cpu);
	struct _vx_hist_entry *entry;
	unsigned int index;

	index = vxh_active ? (hist->counter++ % VXH_SIZE) : VXH_SIZE;
	entry = &hist->entry[index];

	entry->seq = atomic_inc_return(&sequence);
	entry->loc = loc;
	return entry;
}


#define VXH_LOC_FMTS	"(#%04x,*%d):%p"

#define VXH_LOC_ARGS(e)	(e)->seq, cpu, (e)->loc


#define VXH_VXI_FMTS	"%p[#%d,%d.%d]"

#define VXH_VXI_ARGS(e)	(e)->vxi.ptr,			\
			(e)->vxi.ptr?(e)->vxi.xid:0,	\
			(e)->vxi.ptr?(e)->vxi.usecnt:0,	\
			(e)->vxi.ptr?(e)->vxi.tasks:0

void	vxh_dump_entry(struct _vx_hist_entry *e, unsigned cpu)
{
	switch (e->type) {
	case VXH_THROW_OOPS:
		printk( VXH_LOC_FMTS " oops \n", VXH_LOC_ARGS(e));
		break;

	case VXH_GET_VX_INFO:
	case VXH_PUT_VX_INFO:
		printk( VXH_LOC_FMTS " %s_vx_info " VXH_VXI_FMTS "\n",
			VXH_LOC_ARGS(e),
			(e->type==VXH_GET_VX_INFO)?"get":"put",
			VXH_VXI_ARGS(e));
		break;

	case VXH_INIT_VX_INFO:
	case VXH_SET_VX_INFO:
	case VXH_CLR_VX_INFO:
		printk( VXH_LOC_FMTS " %s_vx_info " VXH_VXI_FMTS " @%p\n",
			VXH_LOC_ARGS(e),
			(e->type==VXH_INIT_VX_INFO)?"init":
			((e->type==VXH_SET_VX_INFO)?"set":"clr"),
			VXH_VXI_ARGS(e), e->sc.data);
		break;

	case VXH_CLAIM_VX_INFO:
	case VXH_RELEASE_VX_INFO:
		printk( VXH_LOC_FMTS " %s_vx_info " VXH_VXI_FMTS " @%p\n",
			VXH_LOC_ARGS(e),
			(e->type==VXH_CLAIM_VX_INFO)?"claim":"release",
			VXH_VXI_ARGS(e), e->sc.data);
		break;

	case VXH_ALLOC_VX_INFO:
	case VXH_DEALLOC_VX_INFO:
		printk( VXH_LOC_FMTS " %s_vx_info " VXH_VXI_FMTS "\n",
			VXH_LOC_ARGS(e),
			(e->type==VXH_ALLOC_VX_INFO)?"alloc":"dealloc",
			VXH_VXI_ARGS(e));
		break;

	case VXH_HASH_VX_INFO:
	case VXH_UNHASH_VX_INFO:
		printk( VXH_LOC_FMTS " __%s_vx_info " VXH_VXI_FMTS "\n",
			VXH_LOC_ARGS(e),
			(e->type==VXH_HASH_VX_INFO)?"hash":"unhash",
			VXH_VXI_ARGS(e));
		break;

	case VXH_LOC_VX_INFO:
	case VXH_LOOKUP_VX_INFO:
	case VXH_CREATE_VX_INFO:
		printk( VXH_LOC_FMTS " __%s_vx_info [#%d] -> " VXH_VXI_FMTS "\n",
			VXH_LOC_ARGS(e),
			(e->type==VXH_CREATE_VX_INFO)?"create":
			((e->type==VXH_LOC_VX_INFO)?"loc":"lookup"),
			e->ll.arg, VXH_VXI_ARGS(e));
		break;
	}
}

static void __vxh_dump_history(void)
{
	unsigned int i,j;

	printk("History:\tSEQ: %8x\tNR_CPUS: %d\n",
		atomic_read(&sequence), NR_CPUS);

	for (i=0; i < VXH_SIZE; i++) {
		for (j=0; j < NR_CPUS; j++) {
			struct _vx_history *hist =
				&per_cpu(vx_history_buffer, j);
			unsigned int index = (hist->counter-i) % VXH_SIZE;
			struct _vx_hist_entry *entry = &hist->entry[index];

			vxh_dump_entry(entry, j);
		}
	}
}

void	vxh_dump_history(void)
{
	vxh_active = 0;
#ifdef CONFIG_SMP
	local_irq_enable();
	smp_send_stop();
	local_irq_disable();
#endif
	__vxh_dump_history();
}


/* vserver syscall commands below here */


int	vc_dump_history(uint32_t id)
{
	vxh_active = 0;
	__vxh_dump_history();
	vxh_active = 1;

	return 0;
}

EXPORT_SYMBOL_GPL(vxh_advance);

