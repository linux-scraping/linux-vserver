#ifndef _VX_VS_MEMORY_H
#define _VX_VS_MEMORY_H


#include "vserver/limit.h"
#include "vserver/debug.h"


#define vx_acc_page(m,d,v,r) \
	__vx_acc_page(&(m->v), m->mm_vx_info, r, d, __FILE__, __LINE__)

static inline void __vx_acc_page(unsigned long *v, struct vx_info *vxi,
		int res, int dir, char *file, int line)
{
	if (VXD_RLIMIT(res, RLIMIT_RSS) ||
		VXD_RLIMIT(res, VLIMIT_ANON) ||
		VXD_RLIMIT(res, RLIMIT_AS) ||
		VXD_RLIMIT(res, RLIMIT_MEMLOCK))
		vxlprintk(1, "vx_acc_page[%5d,%s,%2d]: %5d%s",
			(vxi?vxi->vx_id:-1), vlimit_name[res], res,
			(vxi?atomic_read(&vxi->limit.rcur[res]):0),
			(dir?"++":"--"), file, line);
	if (v) {
		if (dir > 0)
			++(*v);
		else
			--(*v);
	}
	if (vxi) {
		if (dir > 0)
			atomic_inc(&vxi->limit.rcur[res]);
		else
			atomic_dec(&vxi->limit.rcur[res]);
	}
}


#define vx_acc_pages(m,p,v,r) \
	__vx_acc_pages(&(m->v), m->mm_vx_info, r, p, __FILE__, __LINE__)

static inline void __vx_acc_pages(unsigned long *v, struct vx_info *vxi,
		int res, int pages, char *_file, int _line)
{
	if (VXD_RLIMIT(res, RLIMIT_RSS) ||
		VXD_RLIMIT(res, VLIMIT_ANON) ||
		VXD_RLIMIT(res, RLIMIT_AS) ||
		VXD_RLIMIT(res, RLIMIT_MEMLOCK))
		vxlprintk(1, "vx_acc_pages[%5d,%s,%2d]: %5d += %5d",
			(vxi?vxi->vx_id:-1), vlimit_name[res], res,
			(vxi?atomic_read(&vxi->limit.rcur[res]):0),
			pages, _file, _line);
	if (pages == 0)
		return;
	if (v)
		*v += pages;
	if (vxi)
		atomic_add(pages, &vxi->limit.rcur[res]);
}



#define vx_acc_vmpage(m,d) \
	vx_acc_page(m, d, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpage(m,d) \
	vx_acc_page(m, d, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_rsspage(m,d) \
	vx_acc_page(m, d, _rss,      RLIMIT_RSS)
#define vx_acc_anon_rsspage(m,d) \
	vx_acc_page(m, d, _anon_rss, VLIMIT_ANON)

#define vx_acc_vmpages(m,p) \
	vx_acc_pages(m, p, total_vm,  RLIMIT_AS)
#define vx_acc_vmlpages(m,p) \
	vx_acc_pages(m, p, locked_vm, RLIMIT_MEMLOCK)
#define vx_acc_rsspages(m,p) \
	vx_acc_pages(m, p, _rss,      RLIMIT_RSS)
#define vx_acc_anon_rsspages(m,p) \
	vx_acc_pages(m, p, _anon_rss, VLIMIT_ANON)

#define vx_pages_add(s,r,p)	__vx_acc_pages(0, s, r, p, __FILE__, __LINE__)
#define vx_pages_sub(s,r,p)	vx_pages_add(s, r, -(p))

#define vx_vmpages_inc(m)		vx_acc_vmpage(m, 1)
#define vx_vmpages_dec(m)		vx_acc_vmpage(m,-1)
#define vx_vmpages_add(m,p)		vx_acc_vmpages(m, p)
#define vx_vmpages_sub(m,p)		vx_acc_vmpages(m,-(p))

#define vx_vmlocked_inc(m)		vx_acc_vmlpage(m, 1)
#define vx_vmlocked_dec(m)		vx_acc_vmlpage(m,-1)
#define vx_vmlocked_add(m,p)		vx_acc_vmlpages(m, p)
#define vx_vmlocked_sub(m,p)		vx_acc_vmlpages(m,-(p))

#define vx_rsspages_inc(m)		vx_acc_rsspage(m, 1)
#define vx_rsspages_dec(m)		vx_acc_rsspage(m,-1)
#define vx_rsspages_add(m,p)		vx_acc_rsspages(m, p)
#define vx_rsspages_sub(m,p)		vx_acc_rsspages(m,-(p))

#define vx_anon_rsspages_inc(m)		vx_acc_anon_rsspage(m, 1)
#define vx_anon_rsspages_dec(m)		vx_acc_anon_rsspage(m,-1)
#define vx_anon_rsspages_add(m,p)	vx_acc_anon_rsspages(m, p)
#define vx_anon_rsspages_sub(m,p)	vx_acc_anon_rsspages(m,-(p))


#define vx_pages_avail(m,p,r) \
	__vx_pages_avail((m)->mm_vx_info, r, p, __FILE__, __LINE__)

static inline int __vx_pages_avail(struct vx_info *vxi,
		int res, int pages, char *_file, int _line)
{
	unsigned long value;

	if (VXD_RLIMIT(res, RLIMIT_RSS) ||
		VXD_RLIMIT(res, RLIMIT_AS) ||
		VXD_RLIMIT(res, RLIMIT_MEMLOCK))
		vxlprintk(1, "vx_pages_avail[%5d,%s,%2d]: %5ld > %5d + %5d",
			(vxi?vxi->vx_id:-1), vlimit_name[res], res,
			(vxi?vxi->limit.rlim[res]:1),
			(vxi?atomic_read(&vxi->limit.rcur[res]):0),
			pages, _file, _line);
	if (!vxi)
		return 1;
	value = atomic_read(&vxi->limit.rcur[res]);
	if (value > vxi->limit.rmax[res])
		vxi->limit.rmax[res] = value;
	if (vxi->limit.rlim[res] == RLIM_INFINITY)
		return 1;
	if (value + pages <= vxi->limit.rlim[res])
		return 1;
	atomic_inc(&vxi->limit.lhit[res]);
	return 0;
}

#define vx_vmpages_avail(m,p)	vx_pages_avail(m, p, RLIMIT_AS)
#define vx_vmlocked_avail(m,p)	vx_pages_avail(m, p, RLIMIT_MEMLOCK)
#define vx_rsspages_avail(m,p)	vx_pages_avail(m, p, RLIMIT_RSS)
#define vx_anonpages_avail(m,p)	vx_pages_avail(m, p, VLIMIT_ANON)

#else
#warning duplicate inclusion
#endif
