#ifndef _VS_MEMORY_H
#define _VS_MEMORY_H

#include "vserver/limit.h"
#include "vserver/base.h"
#include "vserver/context.h"
#include "vserver/debug.h"
#include "vserver/context.h"
#include "vserver/limit_int.h"


#define __acc_add_long(a,v)	(*(v) += (a))
#define __acc_inc_long(v)	(++*(v))
#define __acc_dec_long(v)	(--*(v))

#if	NR_CPUS >= CONFIG_SPLIT_PTLOCK_CPUS
#define __acc_add_atomic(a,v)	atomic_long_add(a,v)
#define __acc_inc_atomic(v)	atomic_long_inc(v)
#define __acc_dec_atomic(v)	atomic_long_dec(v)
#else  /* NR_CPUS < CONFIG_SPLIT_PTLOCK_CPUS */
#define __acc_add_atomic(a,v)	__acc_add_long(a,v)
#define __acc_inc_atomic(v)	__acc_inc_long(v)
#define __acc_dec_atomic(v)	__acc_dec_long(v)
#endif /* NR_CPUS < CONFIG_SPLIT_PTLOCK_CPUS */


#define vx_acc_page(m,d,v,r) do {					\
	if ((d) > 0)							\
		__acc_inc_long(&(m->v));				\
	else								\
		__acc_dec_long(&(m->v));				\
	__vx_acc_cres(m->mm_vx_info, r, d, m, __FILE__, __LINE__);	\
} while (0)

#define vx_acc_page_atomic(m,d,v,r) do {				\
	if ((d) > 0)							\
		__acc_inc_atomic(&(m->v));				\
	else								\
		__acc_dec_atomic(&(m->v));				\
	__vx_acc_cres(m->mm_vx_info, r, d, m, __FILE__, __LINE__);	\
} while (0)


#define vx_acc_pages(m,p,v,r) do {					\
	unsigned long __p = (p);					\
	__acc_add_long(__p, &(m->v));					\
	__vx_add_cres(m->mm_vx_info, r, __p, m, __FILE__, __LINE__);	\
} while (0)

#define vx_acc_pages_atomic(m,p,v,r) do {				\
	unsigned long __p = (p);					\
	__acc_add_atomic(__p, &(m->v));					\
	__vx_add_cres(m->mm_vx_info, r, __p, m, __FILE__, __LINE__);	\
} while (0)



#define vx_acc_vmpage(m,d) \
	vx_acc_page(m, d, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpage(m,d) \
	vx_acc_page(m, d, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_file_rsspage(m,d) \
	vx_acc_page_atomic(m, d, _file_rss, VLIMIT_MAPPED)
#define vx_acc_anon_rsspage(m,d) \
	vx_acc_page_atomic(m, d, _anon_rss, VLIMIT_ANON)

#define vx_acc_vmpages(m,p) \
	vx_acc_pages(m, p, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpages(m,p) \
	vx_acc_pages(m, p, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_file_rsspages(m,p) \
	vx_acc_pages_atomic(m, p, _file_rss, VLIMIT_MAPPED)
#define vx_acc_anon_rsspages(m,p) \
	vx_acc_pages_atomic(m, p, _anon_rss, VLIMIT_ANON)

#define vx_pages_add(s,r,p)	__vx_add_cres(s, r, p, 0, __FILE__, __LINE__)
#define vx_pages_sub(s,r,p)	vx_pages_add(s, r, -(p))

#define vx_vmpages_inc(m)		vx_acc_vmpage(m, 1)
#define vx_vmpages_dec(m)		vx_acc_vmpage(m,-1)
#define vx_vmpages_add(m,p)		vx_acc_vmpages(m, p)
#define vx_vmpages_sub(m,p)		vx_acc_vmpages(m,-(p))

#define vx_vmlocked_inc(m)		vx_acc_vmlpage(m, 1)
#define vx_vmlocked_dec(m)		vx_acc_vmlpage(m,-1)
#define vx_vmlocked_add(m,p)		vx_acc_vmlpages(m, p)
#define vx_vmlocked_sub(m,p)		vx_acc_vmlpages(m,-(p))

#define vx_file_rsspages_inc(m)		vx_acc_file_rsspage(m, 1)
#define vx_file_rsspages_dec(m)		vx_acc_file_rsspage(m,-1)
#define vx_file_rsspages_add(m,p)	vx_acc_file_rsspages(m, p)
#define vx_file_rsspages_sub(m,p)	vx_acc_file_rsspages(m,-(p))

#define vx_anon_rsspages_inc(m)		vx_acc_anon_rsspage(m, 1)
#define vx_anon_rsspages_dec(m)		vx_acc_anon_rsspage(m,-1)
#define vx_anon_rsspages_add(m,p)	vx_acc_anon_rsspages(m, p)
#define vx_anon_rsspages_sub(m,p)	vx_acc_anon_rsspages(m,-(p))


#define vx_pages_avail(m,p,r) \
	__vx_cres_avail((m)->mm_vx_info, r, p, __FILE__, __LINE__)

#define vx_vmpages_avail(m,p)	vx_pages_avail(m, p, RLIMIT_AS)
#define vx_vmlocked_avail(m,p)	vx_pages_avail(m, p, RLIMIT_MEMLOCK)
#define vx_anon_avail(m,p)	vx_pages_avail(m, p, VLIMIT_ANON)
#define vx_mapped_avail(m,p)	vx_pages_avail(m, p, VLIMIT_MAPPED)

#define vx_rss_avail(m,p) \
	__vx_cres_array_avail((m)->mm_vx_info, VLA_RSS, p, __FILE__, __LINE__)


enum {
	VXPT_UNKNOWN = 0,
	VXPT_ANON,
	VXPT_NONE,
	VXPT_FILE,
	VXPT_SWAP,
	VXPT_WRITE
};

#if 0
#define	vx_page_fault(mm,vma,type,ret)
#else

static inline
void __vx_page_fault(struct mm_struct *mm,
	struct vm_area_struct *vma, int type, int ret)
{
	struct vx_info *vxi = mm->mm_vx_info;
	int what;
/*
	static char *page_type[6] =
		{ "UNKNOWN", "ANON","NONE", "FILE", "SWAP", "WRITE" };
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

#define	vx_page_fault(mm,vma,type,ret)	__vx_page_fault(mm,vma,type,ret)
#endif


extern unsigned long vx_badness(struct task_struct *task, struct mm_struct *mm);

#else
#warning duplicate inclusion
#endif
