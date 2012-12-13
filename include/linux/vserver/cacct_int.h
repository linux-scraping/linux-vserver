#ifndef _VSERVER_CACCT_INT_H
#define _VSERVER_CACCT_INT_H

static inline
unsigned long vx_sock_count(struct _vx_cacct *cacct, int type, int pos)
{
	return atomic_long_read(&cacct->sock[type][pos].count);
}


static inline
unsigned long vx_sock_total(struct _vx_cacct *cacct, int type, int pos)
{
	return atomic_long_read(&cacct->sock[type][pos].total);
}

#endif	/* _VSERVER_CACCT_INT_H */
