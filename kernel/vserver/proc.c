/*
 *  linux/kernel/vserver/proc.c
 *
 *  Virtual Context Support
 *
 *  Copyright (C) 2003-2011  Herbert Pötzl
 *
 *  V0.01  basic structure
 *  V0.02  adaptation vs1.3.0
 *  V0.03  proc permissions
 *  V0.04  locking/generic
 *  V0.05  next generation procfs
 *  V0.06  inode validation
 *  V0.07  generic rewrite vid
 *  V0.08  remove inode type
 *  V0.09  added u/wmask info
 *
 */

#include <linux/proc_fs.h>
#include <linux/fs_struct.h>
#include <linux/mount.h>
#include <linux/namei.h>
#include <asm/unistd.h>

#include <linux/vs_context.h>
#include <linux/vs_network.h>
#include <linux/vs_cvirt.h>

#include <linux/in.h>
#include <linux/inetdevice.h>
#include <linux/vs_inet.h>
#include <linux/vs_inet6.h>

#include <linux/vserver/global.h>

#include "cvirt_proc.h"
#include "cacct_proc.h"
#include "limit_proc.h"
#include "sched_proc.h"
#include "vci_config.h"

#include <../../fs/proc/internal.h>


static inline char *print_cap_t(char *buffer, kernel_cap_t *c)
{
	unsigned __capi;

	CAP_FOR_EACH_U32(__capi) {
		buffer += sprintf(buffer, "%08x",
			c->cap[(_KERNEL_CAPABILITY_U32S-1) - __capi]);
	}
	return buffer;
}


static struct proc_dir_entry *proc_virtual;

static struct proc_dir_entry *proc_virtnet;


/* first the actual feeds */


static int proc_vci(char *buffer)
{
	return sprintf(buffer,
		"VCIVersion:\t%04x:%04x\n"
		"VCISyscall:\t%d\n"
		"VCIKernel:\t%08x\n",
		VCI_VERSION >> 16,
		VCI_VERSION & 0xFFFF,
		__NR_vserver,
		vci_kernel_config());
}

static int proc_virtual_info(char *buffer)
{
	return proc_vci(buffer);
}

static int proc_virtual_status(char *buffer)
{
	return sprintf(buffer,
		"#CTotal:\t%d\n"
		"#CActive:\t%d\n"
		"#NSProxy:\t%d\t%d %d %d %d %d %d\n"
		"#InitTask:\t%d\t%d %d\n",
		atomic_read(&vx_global_ctotal),
		atomic_read(&vx_global_cactive),
		atomic_read(&vs_global_nsproxy),
		atomic_read(&vs_global_fs),
		atomic_read(&vs_global_mnt_ns),
		atomic_read(&vs_global_uts_ns),
		atomic_read(&nr_ipc_ns),
		atomic_read(&vs_global_user_ns),
		atomic_read(&vs_global_pid_ns),
		atomic_read(&init_task.usage),
		atomic_read(&init_task.nsproxy->count),
		init_task.fs->users);
}


int proc_vxi_info(struct vx_info *vxi, char *buffer)
{
	int length;

	length = sprintf(buffer,
		"ID:\t%d\n"
		"Info:\t%p\n"
		"Init:\t%d\n"
		"OOM:\t%lld\n",
		vxi->vx_id,
		vxi,
		vxi->vx_initpid,
		vxi->vx_badness_bias);
	return length;
}

int proc_vxi_status(struct vx_info *vxi, char *buffer)
{
	char *orig = buffer;

	buffer += sprintf(buffer,
		"UseCnt:\t%d\n"
		"Tasks:\t%d\n"
		"Flags:\t%016llx\n",
		atomic_read(&vxi->vx_usecnt),
		atomic_read(&vxi->vx_tasks),
		(unsigned long long)vxi->vx_flags);

	buffer += sprintf(buffer, "BCaps:\t");
	buffer = print_cap_t(buffer, &vxi->vx_bcaps);
	buffer += sprintf(buffer, "\n");

	buffer += sprintf(buffer,
		"CCaps:\t%016llx\n"
		"Umask:\t%16llx\n"
		"Wmask:\t%16llx\n"
		"Spaces:\t%08lx %08lx\n",
		(unsigned long long)vxi->vx_ccaps,
		(unsigned long long)vxi->vx_umask,
		(unsigned long long)vxi->vx_wmask,
		vxi->space[0].vx_nsmask, vxi->space[1].vx_nsmask);
	return buffer - orig;
}

int proc_vxi_limit(struct vx_info *vxi, char *buffer)
{
	return vx_info_proc_limit(&vxi->limit, buffer);
}

int proc_vxi_sched(struct vx_info *vxi, char *buffer)
{
	int cpu, length;

	length = vx_info_proc_sched(&vxi->sched, buffer);
	for_each_online_cpu(cpu) {
		length += vx_info_proc_sched_pc(
			&vx_per_cpu(vxi, sched_pc, cpu),
			buffer + length, cpu);
	}
	return length;
}

int proc_vxi_nsproxy0(struct vx_info *vxi, char *buffer)
{
	return vx_info_proc_nsproxy(vxi->space[0].vx_nsproxy, buffer);
}

int proc_vxi_nsproxy1(struct vx_info *vxi, char *buffer)
{
	return vx_info_proc_nsproxy(vxi->space[1].vx_nsproxy, buffer);
}

int proc_vxi_cvirt(struct vx_info *vxi, char *buffer)
{
	int cpu, length;

	vx_update_load(vxi);
	length = vx_info_proc_cvirt(&vxi->cvirt, buffer);
	for_each_online_cpu(cpu) {
		length += vx_info_proc_cvirt_pc(
			&vx_per_cpu(vxi, cvirt_pc, cpu),
			buffer + length, cpu);
	}
	return length;
}

int proc_vxi_cacct(struct vx_info *vxi, char *buffer)
{
	return vx_info_proc_cacct(&vxi->cacct, buffer);
}


static int proc_virtnet_info(char *buffer)
{
	return proc_vci(buffer);
}

static int proc_virtnet_status(char *buffer)
{
	return sprintf(buffer,
		"#CTotal:\t%d\n"
		"#CActive:\t%d\n",
		atomic_read(&nx_global_ctotal),
		atomic_read(&nx_global_cactive));
}

int proc_nxi_info(struct nx_info *nxi, char *buffer)
{
	struct nx_addr_v4 *v4a;
#ifdef	CONFIG_IPV6
	struct nx_addr_v6 *v6a;
#endif
	int length, i;

	length = sprintf(buffer,
		"ID:\t%d\n"
		"Info:\t%p\n"
		"Bcast:\t" NIPQUAD_FMT "\n"
		"Lback:\t" NIPQUAD_FMT "\n",
		nxi->nx_id,
		nxi,
		NIPQUAD(nxi->v4_bcast.s_addr),
		NIPQUAD(nxi->v4_lback.s_addr));

	if (!NX_IPV4(nxi))
		goto skip_v4;
	for (i = 0, v4a = &nxi->v4; v4a; i++, v4a = v4a->next)
		length += sprintf(buffer + length, "%d:\t" NXAV4_FMT "\n",
			i, NXAV4(v4a));
skip_v4:
#ifdef	CONFIG_IPV6
	if (!NX_IPV6(nxi))
		goto skip_v6;
	for (i = 0, v6a = &nxi->v6; v6a; i++, v6a = v6a->next)
		length += sprintf(buffer + length, "%d:\t" NXAV6_FMT "\n",
			i, NXAV6(v6a));
skip_v6:
#endif
	return length;
}

int proc_nxi_status(struct nx_info *nxi, char *buffer)
{
	int length;

	length = sprintf(buffer,
		"UseCnt:\t%d\n"
		"Tasks:\t%d\n"
		"Flags:\t%016llx\n"
		"NCaps:\t%016llx\n",
		atomic_read(&nxi->nx_usecnt),
		atomic_read(&nxi->nx_tasks),
		(unsigned long long)nxi->nx_flags,
		(unsigned long long)nxi->nx_ncaps);
	return length;
}



/* here the inode helpers */

struct vs_entry {
	int len;
	char *name;
	mode_t mode;
	struct inode_operations *iop;
	struct file_operations *fop;
	union proc_op op;
};

static struct inode *vs_proc_make_inode(struct super_block *sb, struct vs_entry *p)
{
	struct inode *inode = new_inode(sb);

	if (!inode)
		goto out;

	inode->i_mode = p->mode;
	if (p->iop)
		inode->i_op = p->iop;
	if (p->fop)
		inode->i_fop = p->fop;

	set_nlink(inode, (p->mode & S_IFDIR) ? 2 : 1);
	inode->i_flags |= S_IMMUTABLE;

	inode->i_mtime = inode->i_atime = inode->i_ctime = CURRENT_TIME;

	i_uid_write(inode, 0);
	i_gid_write(inode, 0);
	i_tag_write(inode, 0);
out:
	return inode;
}

static struct dentry *vs_proc_instantiate(struct inode *dir,
	struct dentry *dentry, int id, void *ptr)
{
	struct vs_entry *p = ptr;
	struct inode *inode = vs_proc_make_inode(dir->i_sb, p);
	struct dentry *error = ERR_PTR(-EINVAL);

	if (!inode)
		goto out;

	PROC_I(inode)->op = p->op;
	PROC_I(inode)->fd = id;
	d_add(dentry, inode);
	error = NULL;
out:
	return error;
}

/* Lookups */

typedef struct dentry *vx_instantiate_t(struct inode *, struct dentry *, int, void *);


/*
 * Fill a directory entry.
 *
 * If possible create the dcache entry and derive our inode number and
 * file type from dcache entry.
 *
 * Since all of the proc inode numbers are dynamically generated, the inode
 * numbers do not exist until the inode is cache.  This means creating the
 * the dcache entry in iterate is necessary to keep the inode numbers
 * reported by iterate in sync with the inode numbers reported
 * by stat.
 */
static int vx_proc_fill_cache(struct file *filp, struct dir_context *ctx,
	char *name, int len, vx_instantiate_t instantiate, int id, void *ptr)
{
	struct dentry *child, *dir = filp->f_dentry;
	struct inode *inode;
	struct qstr qname;
	ino_t ino = 0;
	unsigned type = DT_UNKNOWN;

	qname.name = name;
	qname.len  = len;
	qname.hash = full_name_hash(name, len);

	child = d_lookup(dir, &qname);
	if (!child) {
		struct dentry *new;
		new = d_alloc(dir, &qname);
		if (new) {
			child = instantiate(dir->d_inode, new, id, ptr);
			if (child)
				dput(new);
			else
				child = new;
		}
	}
	if (!child || IS_ERR(child) || !child->d_inode)
		goto end_instantiate;
	inode = child->d_inode;
	if (inode) {
		ino = inode->i_ino;
		type = inode->i_mode >> 12;
	}
	dput(child);
end_instantiate:
	if (!ino)
		ino = 1;
	return !dir_emit(ctx, name, len, ino, type);
}



/* get and revalidate vx_info/xid */

static inline
struct vx_info *get_proc_vx_info(struct inode *inode)
{
	return lookup_vx_info(PROC_I(inode)->fd);
}

static int proc_xid_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct inode *inode = dentry->d_inode;
	vxid_t xid = PROC_I(inode)->fd;

	if (flags & LOOKUP_RCU)	/* FIXME: can be dropped? */
		return -ECHILD;

	if (!xid || xid_is_hashed(xid))
		return 1;
	d_drop(dentry);
	return 0;
}


/* get and revalidate nx_info/nid */

static int proc_nid_revalidate(struct dentry *dentry, unsigned int flags)
{
	struct inode *inode = dentry->d_inode;
	vnid_t nid = PROC_I(inode)->fd;

	if (flags & LOOKUP_RCU)	/* FIXME: can be dropped? */
		return -ECHILD;

	if (!nid || nid_is_hashed(nid))
		return 1;
	d_drop(dentry);
	return 0;
}



#define PROC_BLOCK_SIZE (PAGE_SIZE - 1024)

static ssize_t proc_vs_info_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	unsigned long page;
	ssize_t length = 0;

	if (count > PROC_BLOCK_SIZE)
		count = PROC_BLOCK_SIZE;

	/* fade that out as soon as stable */
	WARN_ON(PROC_I(inode)->fd);

	if (!(page = __get_free_page(GFP_KERNEL)))
		return -ENOMEM;

	BUG_ON(!PROC_I(inode)->op.proc_vs_read);
	length = PROC_I(inode)->op.proc_vs_read((char *)page);

	if (length >= 0)
		length = simple_read_from_buffer(buf, count, ppos,
			(char *)page, length);

	free_page(page);
	return length;
}

static ssize_t proc_vx_info_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct vx_info *vxi = NULL;
	vxid_t xid = PROC_I(inode)->fd;
	unsigned long page;
	ssize_t length = 0;

	if (count > PROC_BLOCK_SIZE)
		count = PROC_BLOCK_SIZE;

	/* fade that out as soon as stable */
	WARN_ON(!xid);
	vxi = lookup_vx_info(xid);
	if (!vxi)
		goto out;

	length = -ENOMEM;
	if (!(page = __get_free_page(GFP_KERNEL)))
		goto out_put;

	BUG_ON(!PROC_I(inode)->op.proc_vxi_read);
	length = PROC_I(inode)->op.proc_vxi_read(vxi, (char *)page);

	if (length >= 0)
		length = simple_read_from_buffer(buf, count, ppos,
			(char *)page, length);

	free_page(page);
out_put:
	put_vx_info(vxi);
out:
	return length;
}

static ssize_t proc_nx_info_read(struct file *file, char __user *buf,
			  size_t count, loff_t *ppos)
{
	struct inode *inode = file->f_dentry->d_inode;
	struct nx_info *nxi = NULL;
	vnid_t nid = PROC_I(inode)->fd;
	unsigned long page;
	ssize_t length = 0;

	if (count > PROC_BLOCK_SIZE)
		count = PROC_BLOCK_SIZE;

	/* fade that out as soon as stable */
	WARN_ON(!nid);
	nxi = lookup_nx_info(nid);
	if (!nxi)
		goto out;

	length = -ENOMEM;
	if (!(page = __get_free_page(GFP_KERNEL)))
		goto out_put;

	BUG_ON(!PROC_I(inode)->op.proc_nxi_read);
	length = PROC_I(inode)->op.proc_nxi_read(nxi, (char *)page);

	if (length >= 0)
		length = simple_read_from_buffer(buf, count, ppos,
			(char *)page, length);

	free_page(page);
out_put:
	put_nx_info(nxi);
out:
	return length;
}



/* here comes the lower level */


#define NOD(NAME, MODE, IOP, FOP, OP) {	\
	.len  = sizeof(NAME) - 1,	\
	.name = (NAME),			\
	.mode = MODE,			\
	.iop  = IOP,			\
	.fop  = FOP,			\
	.op   = OP,			\
}


#define DIR(NAME, MODE, OTYPE)				\
	NOD(NAME, (S_IFDIR | (MODE)),			\
		&proc_ ## OTYPE ## _inode_operations,	\
		&proc_ ## OTYPE ## _file_operations, { } )

#define INF(NAME, MODE, OTYPE)				\
	NOD(NAME, (S_IFREG | (MODE)), NULL,		\
		&proc_vs_info_file_operations,		\
		{ .proc_vs_read = &proc_##OTYPE } )

#define VINF(NAME, MODE, OTYPE)				\
	NOD(NAME, (S_IFREG | (MODE)), NULL,		\
		&proc_vx_info_file_operations,		\
		{ .proc_vxi_read = &proc_##OTYPE } )

#define NINF(NAME, MODE, OTYPE)				\
	NOD(NAME, (S_IFREG | (MODE)), NULL,		\
		&proc_nx_info_file_operations,		\
		{ .proc_nxi_read = &proc_##OTYPE } )


static struct file_operations proc_vs_info_file_operations = {
	.read =		proc_vs_info_read,
};

static struct file_operations proc_vx_info_file_operations = {
	.read =		proc_vx_info_read,
};

static struct dentry_operations proc_xid_dentry_operations = {
	.d_revalidate =	proc_xid_revalidate,
};

static struct vs_entry vx_base_stuff[] = {
	VINF("info",	S_IRUGO, vxi_info),
	VINF("status",	S_IRUGO, vxi_status),
	VINF("limit",	S_IRUGO, vxi_limit),
	VINF("sched",	S_IRUGO, vxi_sched),
	VINF("nsproxy",	S_IRUGO, vxi_nsproxy0),
	VINF("nsproxy1",S_IRUGO, vxi_nsproxy1),
	VINF("cvirt",	S_IRUGO, vxi_cvirt),
	VINF("cacct",	S_IRUGO, vxi_cacct),
	{}
};




static struct dentry *proc_xid_instantiate(struct inode *dir,
	struct dentry *dentry, int id, void *ptr)
{
	dentry->d_op = &proc_xid_dentry_operations;
	return vs_proc_instantiate(dir, dentry, id, ptr);
}

static struct dentry *proc_xid_lookup(struct inode *dir,
	struct dentry *dentry, unsigned int flags)
{
	struct vs_entry *p = vx_base_stuff;
	struct dentry *error = ERR_PTR(-ENOENT);

	for (; p->name; p++) {
		if (p->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, p->name, p->len))
			break;
	}
	if (!p->name)
		goto out;

	error = proc_xid_instantiate(dir, dentry, PROC_I(dir)->fd, p);
out:
	return error;
}

static int proc_xid_iterate(struct file *filp, struct dir_context *ctx)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct vs_entry *p = vx_base_stuff;
	int size = sizeof(vx_base_stuff) / sizeof(struct vs_entry);
	int index;
	u64 ino;

	switch (ctx->pos) {
	case 0:
		ino = inode->i_ino;
		if (!dir_emit(ctx, ".", 1, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	case 1:
		ino = parent_ino(dentry);
		if (!dir_emit(ctx, "..", 2, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	default:
		index = ctx->pos - 2;
		if (index >= size)
			goto out;
		for (p += index; p->name; p++) {
			if (vx_proc_fill_cache(filp, ctx, p->name, p->len,
				vs_proc_instantiate, PROC_I(inode)->fd, p))
				goto out;
			ctx->pos++;
		}
	}
out:
	return 1;
}



static struct file_operations proc_nx_info_file_operations = {
	.read =		proc_nx_info_read,
};

static struct dentry_operations proc_nid_dentry_operations = {
	.d_revalidate =	proc_nid_revalidate,
};

static struct vs_entry nx_base_stuff[] = {
	NINF("info",	S_IRUGO, nxi_info),
	NINF("status",	S_IRUGO, nxi_status),
	{}
};


static struct dentry *proc_nid_instantiate(struct inode *dir,
	struct dentry *dentry, int id, void *ptr)
{
	dentry->d_op = &proc_nid_dentry_operations;
	return vs_proc_instantiate(dir, dentry, id, ptr);
}

static struct dentry *proc_nid_lookup(struct inode *dir,
	struct dentry *dentry, unsigned int flags)
{
	struct vs_entry *p = nx_base_stuff;
	struct dentry *error = ERR_PTR(-ENOENT);

	for (; p->name; p++) {
		if (p->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, p->name, p->len))
			break;
	}
	if (!p->name)
		goto out;

	error = proc_nid_instantiate(dir, dentry, PROC_I(dir)->fd, p);
out:
	return error;
}

static int proc_nid_iterate(struct file *filp, struct dir_context *ctx)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct vs_entry *p = nx_base_stuff;
	int size = sizeof(nx_base_stuff) / sizeof(struct vs_entry);
	int index;
	u64 ino;

	switch (ctx->pos) {
	case 0:
		ino = inode->i_ino;
		if (!dir_emit(ctx, ".", 1, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	case 1:
		ino = parent_ino(dentry);
		if (!dir_emit(ctx, "..", 2, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	default:
		index = ctx->pos - 2;
		if (index >= size)
			goto out;
		for (p += index; p->name; p++) {
			if (vx_proc_fill_cache(filp, ctx, p->name, p->len,
				vs_proc_instantiate, PROC_I(inode)->fd, p))
				goto out;
			ctx->pos++;
		}
	}
out:
	return 1;
}


#define MAX_MULBY10	((~0U - 9) / 10)

static inline int atovid(const char *str, int len)
{
	int vid, c;

	vid = 0;
	while (len-- > 0) {
		c = *str - '0';
		str++;
		if (c > 9)
			return -1;
		if (vid >= MAX_MULBY10)
			return -1;
		vid *= 10;
		vid += c;
		if (!vid)
			return -1;
	}
	return vid;
}

/* now the upper level (virtual) */


static struct file_operations proc_xid_file_operations = {
	.read =		generic_read_dir,
	.iterate =	proc_xid_iterate,
};

static struct inode_operations proc_xid_inode_operations = {
	.lookup =	proc_xid_lookup,
};

static struct vs_entry vx_virtual_stuff[] = {
	INF("info",	S_IRUGO, virtual_info),
	INF("status",	S_IRUGO, virtual_status),
	DIR(NULL,	S_IRUGO | S_IXUGO, xid),
};


static struct dentry *proc_virtual_lookup(struct inode *dir,
	struct dentry *dentry, unsigned int flags)
{
	struct vs_entry *p = vx_virtual_stuff;
	struct dentry *error = ERR_PTR(-ENOENT);
	int id = 0;

	for (; p->name; p++) {
		if (p->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, p->name, p->len))
			break;
	}
	if (p->name)
		goto instantiate;

	id = atovid(dentry->d_name.name, dentry->d_name.len);
	if ((id < 0) || !xid_is_hashed(id))
		goto out;

instantiate:
	error = proc_xid_instantiate(dir, dentry, id, p);
out:
	return error;
}

static struct file_operations proc_nid_file_operations = {
	.read =		generic_read_dir,
	.iterate =	proc_nid_iterate,
};

static struct inode_operations proc_nid_inode_operations = {
	.lookup =	proc_nid_lookup,
};

static struct vs_entry nx_virtnet_stuff[] = {
	INF("info",	S_IRUGO, virtnet_info),
	INF("status",	S_IRUGO, virtnet_status),
	DIR(NULL,	S_IRUGO | S_IXUGO, nid),
};


static struct dentry *proc_virtnet_lookup(struct inode *dir,
	struct dentry *dentry, unsigned int flags)
{
	struct vs_entry *p = nx_virtnet_stuff;
	struct dentry *error = ERR_PTR(-ENOENT);
	int id = 0;

	for (; p->name; p++) {
		if (p->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, p->name, p->len))
			break;
	}
	if (p->name)
		goto instantiate;

	id = atovid(dentry->d_name.name, dentry->d_name.len);
	if ((id < 0) || !nid_is_hashed(id))
		goto out;

instantiate:
	error = proc_nid_instantiate(dir, dentry, id, p);
out:
	return error;
}


#define PROC_MAXVIDS 32

int proc_virtual_iterate(struct file *filp, struct dir_context *ctx)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct vs_entry *p = vx_virtual_stuff;
	int size = sizeof(vx_virtual_stuff) / sizeof(struct vs_entry);
	int index;
	unsigned int xid_array[PROC_MAXVIDS];
	char buf[PROC_NUMBUF];
	unsigned int nr_xids, i;
	u64 ino;

	switch (ctx->pos) {
	case 0:
		ino = inode->i_ino;
		if (!dir_emit(ctx, ".", 1, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	case 1:
		ino = parent_ino(dentry);
		if (!dir_emit(ctx, "..", 2, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	default:
		index = ctx->pos - 2;
		if (index >= size)
			goto entries;
		for (p += index; p->name; p++) {
			if (vx_proc_fill_cache(filp, ctx, p->name, p->len,
				vs_proc_instantiate, 0, p))
				goto out;
			ctx->pos++;
		}
	entries:
		index = ctx->pos - size;
		p = &vx_virtual_stuff[size - 1];
		nr_xids = get_xid_list(index, xid_array, PROC_MAXVIDS);
		for (i = 0; i < nr_xids; i++) {
			int n, xid = xid_array[i];
			unsigned int j = PROC_NUMBUF;

			n = xid;
			do
				buf[--j] = '0' + (n % 10);
			while (n /= 10);

			if (vx_proc_fill_cache(filp, ctx,
				buf + j, PROC_NUMBUF - j,
				vs_proc_instantiate, xid, p))
				goto out;
			ctx->pos++;
		}
	}
out:
	return 0;
}

static int proc_virtual_getattr(struct vfsmount *mnt,
	struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;

	generic_fillattr(inode, stat);
	stat->nlink = 2 + atomic_read(&vx_global_cactive);
	return 0;
}

static struct file_operations proc_virtual_dir_operations = {
	.read =		generic_read_dir,
	.iterate =	proc_virtual_iterate,
};

static struct inode_operations proc_virtual_dir_inode_operations = {
	.getattr =	proc_virtual_getattr,
	.lookup =	proc_virtual_lookup,
};



int proc_virtnet_iterate(struct file *filp, struct dir_context *ctx)
{
	struct dentry *dentry = filp->f_dentry;
	struct inode *inode = dentry->d_inode;
	struct vs_entry *p = nx_virtnet_stuff;
	int size = sizeof(nx_virtnet_stuff) / sizeof(struct vs_entry);
	int index;
	unsigned int nid_array[PROC_MAXVIDS];
	char buf[PROC_NUMBUF];
	unsigned int nr_nids, i;
	u64 ino;

	switch (ctx->pos) {
	case 0:
		ino = inode->i_ino;
		if (!dir_emit(ctx, ".", 1, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	case 1:
		ino = parent_ino(dentry);
		if (!dir_emit(ctx, "..", 2, ino, DT_DIR) < 0)
			goto out;
		ctx->pos++;
		/* fall through */
	default:
		index = ctx->pos - 2;
		if (index >= size)
			goto entries;
		for (p += index; p->name; p++) {
			if (vx_proc_fill_cache(filp, ctx, p->name, p->len,
				vs_proc_instantiate, 0, p))
				goto out;
			ctx->pos++;
		}
	entries:
		index = ctx->pos - size;
		p = &nx_virtnet_stuff[size - 1];
		nr_nids = get_nid_list(index, nid_array, PROC_MAXVIDS);
		for (i = 0; i < nr_nids; i++) {
			int n, nid = nid_array[i];
			unsigned int j = PROC_NUMBUF;

			n = nid;
			do
				buf[--j] = '0' + (n % 10);
			while (n /= 10);

			if (vx_proc_fill_cache(filp, ctx,
				buf + j, PROC_NUMBUF - j,
				vs_proc_instantiate, nid, p))
				goto out;
			ctx->pos++;
		}
	}
out:
	return 0;
}

static int proc_virtnet_getattr(struct vfsmount *mnt,
	struct dentry *dentry, struct kstat *stat)
{
	struct inode *inode = dentry->d_inode;

	generic_fillattr(inode, stat);
	stat->nlink = 2 + atomic_read(&nx_global_cactive);
	return 0;
}

static struct file_operations proc_virtnet_dir_operations = {
	.read =		generic_read_dir,
	.iterate =	proc_virtnet_iterate,
};

static struct inode_operations proc_virtnet_dir_inode_operations = {
	.getattr =	proc_virtnet_getattr,
	.lookup =	proc_virtnet_lookup,
};



void proc_vx_init(void)
{
	struct proc_dir_entry *ent;

	ent = proc_mkdir("virtual", 0);
	if (ent) {
		ent->proc_fops = &proc_virtual_dir_operations;
		ent->proc_iops = &proc_virtual_dir_inode_operations;
	}
	proc_virtual = ent;

	ent = proc_mkdir("virtnet", 0);
	if (ent) {
		ent->proc_fops = &proc_virtnet_dir_operations;
		ent->proc_iops = &proc_virtnet_dir_inode_operations;
	}
	proc_virtnet = ent;
}




/* per pid info */


int proc_pid_vx_info(struct task_struct *p, char *buffer)
{
	struct vx_info *vxi;
	char *orig = buffer;

	buffer += sprintf(buffer, "XID:\t%d\n", vx_task_xid(p));

	vxi = task_get_vx_info(p);
	if (!vxi)
		goto out;

	buffer += sprintf(buffer, "BCaps:\t");
	buffer = print_cap_t(buffer, &vxi->vx_bcaps);
	buffer += sprintf(buffer, "\n");
	buffer += sprintf(buffer, "CCaps:\t%016llx\n",
		(unsigned long long)vxi->vx_ccaps);
	buffer += sprintf(buffer, "CFlags:\t%016llx\n",
		(unsigned long long)vxi->vx_flags);
	buffer += sprintf(buffer, "CIPid:\t%d\n", vxi->vx_initpid);

	put_vx_info(vxi);
out:
	return buffer - orig;
}


int proc_pid_nx_info(struct task_struct *p, char *buffer)
{
	struct nx_info *nxi;
	struct nx_addr_v4 *v4a;
#ifdef	CONFIG_IPV6
	struct nx_addr_v6 *v6a;
#endif
	char *orig = buffer;
	int i;

	buffer += sprintf(buffer, "NID:\t%d\n", nx_task_nid(p));

	nxi = task_get_nx_info(p);
	if (!nxi)
		goto out;

	buffer += sprintf(buffer, "NCaps:\t%016llx\n",
		(unsigned long long)nxi->nx_ncaps);
	buffer += sprintf(buffer, "NFlags:\t%016llx\n",
		(unsigned long long)nxi->nx_flags);

	buffer += sprintf(buffer,
		"V4Root[bcast]:\t" NIPQUAD_FMT "\n",
		NIPQUAD(nxi->v4_bcast.s_addr));
	buffer += sprintf (buffer,
		"V4Root[lback]:\t" NIPQUAD_FMT "\n",
		NIPQUAD(nxi->v4_lback.s_addr));
	if (!NX_IPV4(nxi))
		goto skip_v4;
	for (i = 0, v4a = &nxi->v4; v4a; i++, v4a = v4a->next)
		buffer += sprintf(buffer, "V4Root[%d]:\t" NXAV4_FMT "\n",
			i, NXAV4(v4a));
skip_v4:
#ifdef	CONFIG_IPV6
	if (!NX_IPV6(nxi))
		goto skip_v6;
	for (i = 0, v6a = &nxi->v6; v6a; i++, v6a = v6a->next)
		buffer += sprintf(buffer, "V6Root[%d]:\t" NXAV6_FMT "\n",
			i, NXAV6(v6a));
skip_v6:
#endif
	put_nx_info(nxi);
out:
	return buffer - orig;
}

