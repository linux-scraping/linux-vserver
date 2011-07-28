#ifndef _VS_TIME_H
#define _VS_TIME_H


/* time faking stuff */

#ifdef CONFIG_VSERVER_VTIME

extern void vx_adjust_timespec(struct timespec *ts);
extern int vx_settimeofday(const struct timespec *ts);

#else
#define	vx_adjust_timespec(t)	do { } while (0)
#define	vx_settimeofday(t)	do_settimeofday(t)
#endif

#else
#warning duplicate inclusion
#endif
