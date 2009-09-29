

static inline void vx_info_init_cacct(struct _vx_cacct *cacct)
{
	int i, j;


	for (i = 0; i < VXA_SOCK_SIZE; i++) {
		for (j = 0; j < 3; j++) {
			atomic_long_set(&cacct->sock[i][j].count, 0);
			atomic_long_set(&cacct->sock[i][j].total, 0);
		}
	}
	for (i = 0; i < 8; i++)
		atomic_set(&cacct->slab[i], 0);
	for (i = 0; i < 5; i++)
		for (j = 0; j < 4; j++)
			atomic_set(&cacct->page[i][j], 0);
}

static inline void vx_info_exit_cacct(struct _vx_cacct *cacct)
{
	return;
}

