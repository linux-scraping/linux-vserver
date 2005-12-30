#ifndef _VX_VS_MEMORY_H
#define _VX_VS_MEMORY_H


#include "vserver/limit.h"
#include "vserver/debug.h"
#include "vserver/limit_int.h"


#define __vx_add_long(a,v)	(*(v) += (a))
#define __vx_inc_long(v)	(++*(v))
#define __vx_dec_long(v)	(--*(v))

#if NR_CPUS >= CONFIG_SPLIT_PTLOCK_CPUS
#ifdef ATOMIC64_INIT
#define __vx_add_value(a,v)	atomic64_add(a, v)
#define __vx_inc_value(v)	atomic64_inc(v)
#define __vx_dec_value(v)	atomic64_dec(v)
#else  /* !ATOMIC64_INIT */
#define __vx_add_value(a,v)	atomic_add(a, v)
#define __vx_inc_value(v)	atomic_inc(v)
#define __vx_dec_value(v)	atomic_dec(v)
#endif /* !ATOMIC64_INIT */
#else  /* NR_CPUS < CONFIG_SPLIT_PTLOCK_CPUS */
#define __vx_add_value(a,v)	__vx_add_long(a,v)
#define __vx_inc_value(v)	__vx_inc_long(v)
#define __vx_dec_value(v)	__vx_dec_long(v)
#endif /* NR_CPUS < CONFIG_SPLIT_PTLOCK_CPUS */


#define vx_acc_page(m,d,v,r) do {					\
	if ((d) > 0)							\
		__vx_inc_long(&(m->v));					\
	else								\
		__vx_dec_long(&(m->v));					\
	__vx_acc_cres(m->mm_vx_info, r, d, m, __FILE__, __LINE__);	\
} while (0)

#define vx_acc_page_atomic(m,d,v,r) do {				\
	if ((d) > 0)							\
		__vx_inc_value(&(m->v));				\
	else								\
		__vx_dec_value(&(m->v));				\
	__vx_acc_cres(m->mm_vx_info, r, d, m, __FILE__, __LINE__);	\
} while (0)


#define vx_acc_pages(m,p,v,r) do {					\
	unsigned long __p = (p);					\
	__vx_add_long(__p, &(m->v));					\
	__vx_add_cres(m->mm_vx_info, r, __p, m, __FILE__, __LINE__);	\
} while (0)

#define vx_acc_pages_atomic(m,p,v,r) do {				\
	unsigned long __p = (p);					\
	__vx_add_value(__p, &(m->v));					\
	__vx_add_cres(m->mm_vx_info, r, __p, m, __FILE__, __LINE__);	\
} while (0)



#define vx_acc_vmpage(m,d) \
	vx_acc_page(m, d, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpage(m,d) \
	vx_acc_page(m, d, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_file_rsspage(m,d) \
	vx_acc_page_atomic(m, d, _file_rss, RLIMIT_RSS)
#define vx_acc_anon_rsspage(m,d) \
	vx_acc_page_atomic(m, d, _anon_rss, VLIMIT_ANON)

#define vx_acc_vmpages(m,p) \
	vx_acc_pages(m, p, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpages(m,p) \
	vx_acc_pages(m, p, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_file_rsspages(m,p) \
	vx_acc_pages_atomic(m, p, _file_rss, RLIMIT_RSS)
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
#define vx_rsspages_avail(m,p)	vx_pages_avail(m, p, RLIMIT_RSS)
#define vx_anonpages_avail(m,p)	vx_pages_avail(m, p, VLIMIT_ANON)

#else
#warning duplicate inclusion
#endif
