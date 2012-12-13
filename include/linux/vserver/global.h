#ifndef _VSERVER_GLOBAL_H
#define _VSERVER_GLOBAL_H


extern atomic_t vx_global_ctotal;
extern atomic_t vx_global_cactive;

extern atomic_t nx_global_ctotal;
extern atomic_t nx_global_cactive;

extern atomic_t vs_global_nsproxy;
extern atomic_t vs_global_fs;
extern atomic_t vs_global_mnt_ns;
extern atomic_t vs_global_uts_ns;
extern atomic_t vs_global_user_ns;
extern atomic_t vs_global_pid_ns;


#endif /* _VSERVER_GLOBAL_H */
