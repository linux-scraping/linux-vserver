#ifndef _VS_MEMORY_H
#define _VS_MEMORY_H

#include "vserver/limit.h"
#include "vserver/base.h"
#include "vserver/context.h"
#include "vserver/debug.h"
#include "vserver/context.h"
#include "vserver/limit_int.h"

enum {
	VXPT_UNKNOWN = 0,
	VXPT_ANON,
	VXPT_NONE,
	VXPT_FILE,
	VXPT_SWAP,
	VXPT_WRITE
};

#if 0
#define	vx_page_fault(mm, vma, type, ret)
#else

static inline
void __vx_page_fault(struct mm_struct *mm,
	struct vm_area_struct *vma, int type, int ret)
{
	struct vx_info *vxi = mm->mm_vx_info;
	int what;
/*
	static char *page_type[6] =
		{ "UNKNOWN", "ANON", "NONE", "FILE", "SWAP", "WRITE" };
	static char *page_what[4] =
		{ "FAULT_OOM", "FAULT_SIGBUS", "FAULT_MINOR", "FAULT_MAJOR" };
*/

	if (!vxi)
		return;

	what = (ret & 0x3);

/*	printk("[%d] page[%d][%d] %2x %s %s\n", vxi->vx_id,
		type, what, ret, page_type[type], page_what[what]);
*/
	if (ret & VM_FAULT_WRITE)
		what |= 0x4;
	atomic_inc(&vxi->cacct.page[type][what]);
}

#define	vx_page_fault(mm, vma, type, ret)	__vx_page_fault(mm, vma, type, ret)
#endif


extern unsigned long vx_badness(struct task_struct *task, struct mm_struct *mm);

#else
#warning duplicate inclusion
#endif
