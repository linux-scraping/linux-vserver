
static inline
void vx_slab_alloc(struct kmem_cache *cachep, gfp_t flags)
{
	int what = cachep->gfpflags & GFP_ZONEMASK;

	if (!current->vx_info)
		return;

	atomic_add(cachep->objsize, &current->vx_info->cacct.slab[what]);
}

static inline
void vx_slab_free(struct kmem_cache *cachep)
{
	int what = cachep->gfpflags & GFP_ZONEMASK;

	if (!current->vx_info)
		return;

	atomic_sub(cachep->objsize, &current->vx_info->cacct.slab[what]);
}

