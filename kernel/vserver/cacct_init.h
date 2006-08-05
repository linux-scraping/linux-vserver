

static inline void vx_info_init_cacct(struct _vx_cacct *cacct)
{
	int i,j;


	for (i=0; i<6; i++) {
		for (j=0; j<3; j++) {
			atomic_set(&cacct->sock[i][j].count, 0);
			atomic_set(&cacct->sock[i][j].total, 0);
		}
	}
	for (i=0; i<8; i++)
		atomic_set(&cacct->slab[i], 0);
	for (i=0; i<5; i++)
		for (j=0; j<4; j++)
			atomic_set(&cacct->page[i][j], 0);
}

static inline void vx_info_exit_cacct(struct _vx_cacct *cacct)
{
	return;
}

