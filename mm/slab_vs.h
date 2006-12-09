
#include <linux/vserver/context.h>

#include <linux/vs_context.h>

static inline
void vx_slab_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	int what = gfp_zone(cachep->gfpflags);

	if (!current->vx_info)
		return;

	atomic_add(cachep->buffer_size, &current->vx_info->cacct.slab[what]);
}

static inline
void vx_slab_free(struct kmem_cache *cachep)
{
	int what = gfp_zone(cachep->gfpflags);

	if (!current->vx_info)
		return;

	atomic_sub(cachep->buffer_size, &current->vx_info->cacct.slab[what]);
}

