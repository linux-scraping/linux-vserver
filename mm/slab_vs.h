
#include <linux/vserver/context.h>

#include <linux/vs_context.h>

static inline
void vx_slab_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	int what = gfp_zone(cachep->gfpflags);
	struct vx_info *vxi = current_vx_info();

	if (!vxi)
		return;

	atomic_add(cachep->buffer_size, &vxi->cacct.slab[what]);
}

static inline
void vx_slab_free(struct kmem_cache *cachep)
{
	int what = gfp_zone(cachep->gfpflags);
	struct vx_info *vxi = current_vx_info();

	if (!vxi)
		return;

	atomic_sub(cachep->buffer_size, &vxi->cacct.slab[what]);
}

