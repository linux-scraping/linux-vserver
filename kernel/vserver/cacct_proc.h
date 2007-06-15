#ifndef _VX_CACCT_PROC_H
#define _VX_CACCT_PROC_H

#include <linux/vserver/cacct_int.h>


#define VX_SOCKA_TOP	\
	"Type\t    recv #/bytes\t\t   send #/bytes\t\t    fail #/bytes\n"

static inline int vx_info_proc_cacct(struct _vx_cacct *cacct, char *buffer)
{
	int i, j, length = 0;
	static char *type[VXA_SOCK_SIZE] = {
		"UNSPEC", "UNIX", "INET", "INET6", "PACKET", "OTHER"
	};

	length += sprintf(buffer + length, VX_SOCKA_TOP);
	for (i = 0; i < VXA_SOCK_SIZE; i++) {
		length += sprintf(buffer + length, "%s:", type[i]);
		for (j = 0; j < 3; j++) {
			length += sprintf(buffer + length,
				"\t%10lu/%-10lu",
				vx_sock_count(cacct, i, j),
				vx_sock_total(cacct, i, j));
		}
		buffer[length++] = '\n';
	}

	length += sprintf(buffer + length, "\n");
	length += sprintf(buffer + length,
		"slab:\t %8u %8u %8u %8u\n",
		atomic_read(&cacct->slab[1]),
		atomic_read(&cacct->slab[4]),
		atomic_read(&cacct->slab[0]),
		atomic_read(&cacct->slab[2]));

	length += sprintf(buffer + length, "\n");
	for (i = 0; i < 5; i++) {
		length += sprintf(buffer + length,
			"page[%d]: %8u %8u %8u %8u\t %8u %8u %8u %8u\n", i,
			atomic_read(&cacct->page[i][0]),
			atomic_read(&cacct->page[i][1]),
			atomic_read(&cacct->page[i][2]),
			atomic_read(&cacct->page[i][3]),
			atomic_read(&cacct->page[i][4]),
			atomic_read(&cacct->page[i][5]),
			atomic_read(&cacct->page[i][6]),
			atomic_read(&cacct->page[i][7]));
	}
	return length;
}

#endif	/* _VX_CACCT_PROC_H */
