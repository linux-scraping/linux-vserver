#ifndef _VS_TIME_H
#define _VS_TIME_H


/* time faking stuff */

#ifdef CONFIG_VSERVER_VTIME

extern void vx_gettimeofday(struct timeval *tv);
extern int vx_settimeofday(struct timespec *ts);

#else
#define	vx_gettimeofday(t)	do_gettimeofday(t)
#define	vx_settimeofday(t)	do_settimeofday(t)
#endif

#else
#warning duplicate inclusion
#endif
