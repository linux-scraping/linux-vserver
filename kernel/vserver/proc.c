/*
 *  linux/kernel/vserver/proc.c
 *
 *  Virtual Context Support
 *
 *  Copyright (C) 2003-2005  Herbert Pötzl
 *
 *  V0.01  basic structure
 *  V0.02  adaptation vs1.3.0
 *  V0.03  proc permissions
 *  V0.04  locking/generic
 *  V0.05  next generation procfs
 *  V0.06  inode validation
 *  V0.07  generic rewrite vid
 *
 */

#include <linux/errno.h>
#include <linux/proc_fs.h>
#include <linux/sched.h>
#include <linux/vs_context.h>
#include <linux/vs_network.h>
#include <linux/vs_cvirt.h>

#include <linux/vserver/switch.h>
#include <linux/vserver/global.h>

#include <asm/uaccess.h>
#include <asm/unistd.h>

#include "cvirt_proc.h"
#include "cacct_proc.h"
#include "limit_proc.h"
#include "sched_proc.h"
#include "vci_config.h"

static struct proc_dir_entry *proc_virtual;

static struct proc_dir_entry *proc_vnet;


enum vid_directory_inos {
	PROC_XID_INO = 32,
	PROC_XID_INFO,
	PROC_XID_STATUS,
	PROC_XID_LIMIT,
	PROC_XID_SCHED,
	PROC_XID_CVIRT,
	PROC_XID_CACCT,

	PROC_NID_INO = 64,
	PROC_NID_INFO,
	PROC_NID_STATUS,
};

#define PROC_VID_MASK	0x60


/* first the actual feeds */


static int proc_virtual_info(int vid, char *buffer)
{
	return sprintf(buffer,
		"VCIVersion:\t%04x:%04x\n"
		"VCISyscall:\t%d\n"
		"VCIKernel:\t%08x\n"
		,VCI_VERSION >> 16
		,VCI_VERSION & 0xFFFF
		,__NR_vserver
		,vci_kernel_config()
		);
}

static int proc_virtual_status(int vid, char *buffer)
{
	return sprintf(buffer,
		"#CTotal:\t%d\n"
		"#CActive:\t%d\n"
		,atomic_read(&vx_global_ctotal)
		,atomic_read(&vx_global_cactive)
		);
}


int proc_xid_info (int vid, char *buffer)
{
	struct vx_info *vxi;
	int length;

	vxi = lookup_vx_info(vid);
	if (!vxi)
		return 0;
	length = sprintf(buffer,
		"ID:\t%d\n"
		"Info:\t%p\n"
		"Init:\t%d\n"
		,vxi->vx_id
		,vxi
		,vxi->vx_initpid
		);
	put_vx_info(vxi);
	return length;
}

int proc_xid_status (int vid, char *buffer)
{
	struct vx_info *vxi;
	int length;

	vxi = lookup_vx_info(vid);
	if (!vxi)
		return 0;
	length = sprintf(buffer,
		"UseCnt:\t%d\n"
		"Tasks:\t%d\n"
		"Flags:\t%016llx\n"
		"BCaps:\t%016llx\n"
		"CCaps:\t%016llx\n"
//		"Ticks:\t%d\n"
		,atomic_read(&vxi->vx_usecnt)
		,atomic_read(&vxi->vx_tasks)
		,(unsigned long long)vxi->vx_flags
		,(unsigned long long)vxi->vx_bcaps
		,(unsigned long long)vxi->vx_ccaps
//		,atomic_read(&vxi->limit.ticks)
		);
	put_vx_info(vxi);
	return length;
}

int proc_xid_limit (int vid, char *buffer)
{
	struct vx_info *vxi;
	int length;

	vxi = lookup_vx_info(vid);
	if (!vxi)
		return 0;
	length = vx_info_proc_limit(&vxi->limit, buffer);
	put_vx_info(vxi);
	return length;
}

int proc_xid_sched (int vid, char *buffer)
{
	struct vx_info *vxi;
	int cpu, length;

	vxi = lookup_vx_info(vid);
	if (!vxi)
		return 0;
	length = vx_info_proc_sched(&vxi->sched, buffer);
	for_each_online_cpu(cpu) {
		length += vx_info_proc_sched_pc(
			&vx_per_cpu(vxi, sched_pc, cpu),
			buffer + length, cpu);
	}
	put_vx_info(vxi);
	return length;
}

int proc_xid_cvirt (int vid, char *buffer)
{
	struct vx_info *vxi;
	int cpu, length;

	vxi = lookup_vx_info(vid);
	if (!vxi)
		return 0;
	vx_update_load(vxi);
	length = vx_info_proc_cvirt(&vxi->cvirt, buffer);
	for_each_online_cpu(cpu) {
		length += vx_info_proc_cvirt_pc(
			&vx_per_cpu(vxi, cvirt_pc, cpu),
			buffer + length, cpu);
	}
	put_vx_info(vxi);
	return length;
}

int proc_xid_cacct (int vid, char *buffer)
{
	struct vx_info *vxi;
	int length;

	vxi = lookup_vx_info(vid);
	if (!vxi)
		return 0;
	length = vx_info_proc_cacct(&vxi->cacct, buffer);
	put_vx_info(vxi);
	return length;
}


static int proc_vnet_info(int vid, char *buffer)
{
	return sprintf(buffer,
		"VCIVersion:\t%04x:%04x\n"
		"VCISyscall:\t%d\n"
		,VCI_VERSION >> 16
		,VCI_VERSION & 0xFFFF
		,__NR_vserver
		);
}


int proc_nid_info (int vid, char *buffer)
{
	struct nx_info *nxi;
	int length, i;

	nxi = lookup_nx_info(vid);
	if (!nxi)
		return 0;
	length = sprintf(buffer,
		"ID:\t%d\n"
		"Info:\t%p\n"
		,nxi->nx_id
		,nxi
		);
	for (i=0; i<nxi->nbipv4; i++) {
		length += sprintf(buffer + length,
			"%d:\t" NIPQUAD_FMT "/" NIPQUAD_FMT "\n", i,
			NIPQUAD(nxi->ipv4[i]), NIPQUAD(nxi->mask[i]));
	}
	put_nx_info(nxi);
	return length;
}

int proc_nid_status (int vid, char *buffer)
{
	struct nx_info *nxi;
	int length;

	nxi = lookup_nx_info(vid);
	if (!nxi)
		return 0;
	length = sprintf(buffer,
		"UseCnt:\t%d\n"
		"Tasks:\t%d\n"
		,atomic_read(&nxi->nx_usecnt)
		,atomic_read(&nxi->nx_tasks)
		);
	put_nx_info(nxi);
	return length;
}

/* here the inode helpers */


#define fake_ino(id,nr) (((nr) & 0xFFFF) | \
			(((id) & 0xFFFF) << 16))

#define inode_vid(i)	(((i)->i_ino >> 16) & 0xFFFF)
#define inode_type(i)	((i)->i_ino & 0xFFFF)

#define MAX_MULBY10	((~0U-9)/10)


static struct inode *proc_vid_make_inode(struct super_block * sb,
	int vid, int ino)
{
	struct inode *inode = new_inode(sb);

	if (!inode)
		goto out;

	inode->i_mtime = inode->i_atime =
		inode->i_ctime = CURRENT_TIME;
	inode->i_ino = fake_ino(vid, ino);

	inode->i_uid = 0;
	inode->i_gid = 0;
out:
	return inode;
}

static int proc_vid_revalidate(struct dentry * dentry, struct nameidata *nd)
{
	struct inode * inode = dentry->d_inode;
	int vid, hashed=0;

	vid = inode_vid(inode);
	switch (inode_type(inode) & PROC_VID_MASK) {
		case PROC_XID_INO:
			hashed = xid_is_hashed(vid);
			break;
		case PROC_NID_INO:
			hashed = nid_is_hashed(vid);
			break;
	}
	if (hashed)
		return 1;
	d_drop(dentry);
	return 0;
}


#define PROC_BLOCK_SIZE (PAGE_SIZE - 1024)

static ssize_t proc_vid_info_read(struct file * file, char __user * buf,
			  size_t count, loff_t *ppos)
{
	struct inode * inode = file->f_dentry->d_inode;
	unsigned long page;
	ssize_t length;
	int vid;

	if (count > PROC_BLOCK_SIZE)
		count = PROC_BLOCK_SIZE;
	if (!(page = __get_free_page(GFP_KERNEL)))
		return -ENOMEM;

	vid = inode_vid(inode);
	length = PROC_I(inode)->op.proc_vid_read(vid, (char*)page);

	if (length >= 0)
		length = simple_read_from_buffer(buf, count, ppos,
			(char *)page, length);
	free_page(page);
	return length;
}





/* here comes the lower level (vid) */

static struct file_operations proc_vid_info_file_operations = {
	.read =		proc_vid_info_read,
};

static struct dentry_operations proc_vid_dentry_operations = {
	.d_revalidate =	proc_vid_revalidate,
};


struct vid_entry {
	int type;
	int len;
	char *name;
	mode_t mode;
};

#define E(type,name,mode) {(type),sizeof(name)-1,(name),(mode)}

static struct vid_entry vx_base_stuff[] = {
	E(PROC_XID_INFO,	"info",		S_IFREG|S_IRUGO),
	E(PROC_XID_STATUS,	"status",	S_IFREG|S_IRUGO),
	E(PROC_XID_LIMIT,	"limit",	S_IFREG|S_IRUGO),
	E(PROC_XID_SCHED,	"sched",	S_IFREG|S_IRUGO),
	E(PROC_XID_CVIRT,	"cvirt",	S_IFREG|S_IRUGO),
	E(PROC_XID_CACCT,	"cacct",	S_IFREG|S_IRUGO),
	{0,0,NULL,0}
};

static struct vid_entry vn_base_stuff[] = {
	E(PROC_NID_INFO,	"info",		S_IFREG|S_IRUGO),
	E(PROC_NID_STATUS,	"status",	S_IFREG|S_IRUGO),
	{0,0,NULL,0}
};



static struct dentry *proc_vid_lookup(struct inode *dir,
	struct dentry *dentry, struct nameidata *nd)
{
	struct inode *inode;
	struct vid_entry *p;
	int error;

	error = -ENOENT;
	inode = NULL;

	switch (inode_type(dir)) {
		case PROC_XID_INO:
			p = vx_base_stuff;
			break;
		case PROC_NID_INO:
			p = vn_base_stuff;
			break;
		default:
			goto out;
	}

	for (; p->name; p++) {
		if (p->len != dentry->d_name.len)
			continue;
		if (!memcmp(dentry->d_name.name, p->name, p->len))
			break;
	}
	if (!p->name)
		goto out;

	error = -EINVAL;
	inode = proc_vid_make_inode(dir->i_sb, inode_vid(dir), p->type);
	if (!inode)
		goto out;

	switch(p->type) {
	case PROC_XID_INFO:
		PROC_I(inode)->op.proc_vid_read = proc_xid_info;
		break;
	case PROC_XID_STATUS:
		PROC_I(inode)->op.proc_vid_read = proc_xid_status;
		break;
	case PROC_XID_LIMIT:
		PROC_I(inode)->op.proc_vid_read = proc_xid_limit;
		break;
	case PROC_XID_SCHED:
		PROC_I(inode)->op.proc_vid_read = proc_xid_sched;
		break;
	case PROC_XID_CVIRT:
		PROC_I(inode)->op.proc_vid_read = proc_xid_cvirt;
		break;
	case PROC_XID_CACCT:
		PROC_I(inode)->op.proc_vid_read = proc_xid_cacct;
		break;

	case PROC_NID_INFO:
		PROC_I(inode)->op.proc_vid_read = proc_nid_info;
		break;
	case PROC_NID_STATUS:
		PROC_I(inode)->op.proc_vid_read = proc_nid_status;
		break;

	default:
		printk("procfs: impossible type (%d)",p->type);
		iput(inode);
		return ERR_PTR(-EINVAL);
	}
	inode->i_mode = p->mode;
	inode->i_fop = &proc_vid_info_file_operations;
	inode->i_nlink = 1;
	inode->i_flags|=S_IMMUTABLE;

	dentry->d_op = &proc_vid_dentry_operations;
	d_add(dentry, inode);
	error = 0;
out:
	return ERR_PTR(error);
}


static int proc_vid_readdir(struct file * filp,
	void * dirent, filldir_t filldir)
{
	int i, size;
	struct inode *inode = filp->f_dentry->d_inode;
	struct vid_entry *p;

	i = filp->f_pos;
	switch (i) {
	case 0:
		if (filldir(dirent, ".", 1, i,
			inode->i_ino, DT_DIR) < 0)
			return 0;
		i++;
		filp->f_pos++;
		/* fall through */
	case 1:
		if (filldir(dirent, "..", 2, i,
			PROC_ROOT_INO, DT_DIR) < 0)
			return 0;
		i++;
		filp->f_pos++;
		/* fall through */
	default:
		i -= 2;
		switch (inode_type(inode)) {
		case PROC_XID_INO:
			size = sizeof(vx_base_stuff);
			p = vx_base_stuff + i;
			break;
		case PROC_NID_INO:
			size = sizeof(vn_base_stuff);
			p = vn_base_stuff + i;
			break;
		default:
			return 1;
		}
		if (i >= size/sizeof(struct vid_entry))
			return 1;
		while (p->name) {
			if (filldir(dirent, p->name, p->len,
				filp->f_pos, fake_ino(inode_vid(inode),
				p->type), p->mode >> 12) < 0)
				return 0;
			filp->f_pos++;
			p++;
		}
	}
	return 1;
}




/* now the upper level (virtual) */

static struct file_operations proc_vid_file_operations = {
	.read =		generic_read_dir,
	.readdir =	proc_vid_readdir,
};

static struct inode_operations proc_vid_inode_operations = {
	.lookup =	proc_vid_lookup,
};



static __inline__ int atovid(const char *str, int len)
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

static __inline__ unsigned long atoaddr(const char *str, int len)
{
	unsigned long addr, c;

	addr = 0;
	while (len-- > 0) {
		c = *str - '0';
		if (c > 9)
			c -= 'A'-'0'+10;
		if (c > 15)
			c -= 'a'-'A';
		if (c > 15)
			return -1;
		str++;
		if (addr >= ((1 << 28) - 1))
			return -1;
		addr = (addr << 4) | c;
		if (!addr)
			return -1;
	}
	return addr;
}


struct dentry *proc_virtual_lookup(struct inode *dir,
	struct dentry * dentry, struct nameidata *nd)
{
	int xid, len, ret;
	struct vx_info *vxi;
	const char *name;
	struct inode *inode;

	name = dentry->d_name.name;
	len = dentry->d_name.len;
	ret = -ENOMEM;

#if 0
	if (len == 7 && !memcmp(name, "current", 7)) {
		inode = new_inode(dir->i_sb);
		if (!inode)
			goto out;
		inode->i_mtime = inode->i_atime =
			inode->i_ctime = CURRENT_TIME;
		inode->i_ino = fake_ino(1, PROC_XID_INO);
		inode->i_mode = S_IFLNK|S_IRWXUGO;
		inode->i_uid = inode->i_gid = 0;
		d_add(dentry, inode);
		return NULL;
	}
#endif
	if (len == 4 && !memcmp(name, "info", 4)) {
		inode = proc_vid_make_inode(dir->i_sb, 0, PROC_XID_INFO);
		if (!inode)
			goto out;
		inode->i_fop = &proc_vid_info_file_operations;
		PROC_I(inode)->op.proc_vid_read = proc_virtual_info;
		inode->i_mode = S_IFREG|S_IRUGO;
		d_add(dentry, inode);
		return NULL;
	}
	if (len == 6 && !memcmp(name, "status", 6)) {
		inode = proc_vid_make_inode(dir->i_sb, 0, PROC_XID_STATUS);
		if (!inode)
			goto out;
		inode->i_fop = &proc_vid_info_file_operations;
		PROC_I(inode)->op.proc_vid_read = proc_virtual_status;
		inode->i_mode = S_IFREG|S_IRUGO;
		d_add(dentry, inode);
		return NULL;
	}

	ret = -ENOENT;
	xid = atovid(name, len);
	if (xid < 0)
		goto out;
	vxi = lookup_vx_info(xid);
	if (!vxi)
		goto out;

	inode = NULL;
	if (vx_check(xid, VX_ADMIN|VX_WATCH|VX_IDENT))
		inode = proc_vid_make_inode(dir->i_sb,
			vxi->vx_id, PROC_XID_INO);
	if (!inode)
		goto out_release;

	inode->i_mode = S_IFDIR|S_IRUGO|S_IXUGO;
	inode->i_op = &proc_vid_inode_operations;
	inode->i_fop = &proc_vid_file_operations;
	inode->i_nlink = 2;
	inode->i_flags|=S_IMMUTABLE;

	dentry->d_op = &proc_vid_dentry_operations;
	d_add(dentry, inode);
	ret = 0;

out_release:
	put_vx_info(vxi);
out:
	return ERR_PTR(ret);
}


struct dentry *proc_vnet_lookup(struct inode *dir,
	struct dentry * dentry, struct nameidata *nd)
{
	int nid, len, ret;
	struct nx_info *nxi;
	const char *name;
	struct inode *inode;

	name = dentry->d_name.name;
	len = dentry->d_name.len;
	ret = -ENOMEM;
#if 0
	if (len == 7 && !memcmp(name, "current", 7)) {
		inode = new_inode(dir->i_sb);
		if (!inode)
			goto out;
		inode->i_mtime = inode->i_atime =
			inode->i_ctime = CURRENT_TIME;
		inode->i_ino = fake_ino(1, PROC_NID_INO);
		inode->i_mode = S_IFLNK|S_IRWXUGO;
		inode->i_uid = inode->i_gid = 0;
		d_add(dentry, inode);
		return NULL;
	}
#endif
	if (len == 4 && !memcmp(name, "info", 4)) {
		inode = proc_vid_make_inode(dir->i_sb, 0, PROC_NID_INFO);
		if (!inode)
			goto out;
		inode->i_fop = &proc_vid_info_file_operations;
		PROC_I(inode)->op.proc_vid_read = proc_vnet_info;
		inode->i_mode = S_IFREG|S_IRUGO;
		d_add(dentry, inode);
		return NULL;
	}

	ret = -ENOENT;
	nid = atovid(name, len);
	if (nid < 0)
		goto out;
	nxi = lookup_nx_info(nid);
	if (!nxi)
		goto out;

	inode = NULL;
	if (1)
		inode = proc_vid_make_inode(dir->i_sb,
			nxi->nx_id, PROC_NID_INO);
	if (!inode)
		goto out_release;

	inode->i_mode = S_IFDIR|S_IRUGO|S_IXUGO;
	inode->i_op = &proc_vid_inode_operations;
	inode->i_fop = &proc_vid_file_operations;
	inode->i_nlink = 2;
	inode->i_flags|=S_IMMUTABLE;

	dentry->d_op = &proc_vid_dentry_operations;
	d_add(dentry, inode);
	ret = 0;

out_release:
	put_nx_info(nxi);
out:
	return ERR_PTR(ret);
}




#define PROC_NUMBUF 10
#define PROC_MAXVIDS 32

int proc_virtual_readdir(struct file * filp,
	void * dirent, filldir_t filldir)
{
	unsigned int xid_array[PROC_MAXVIDS];
	char buf[PROC_NUMBUF];
	unsigned int nr = filp->f_pos-3;
	unsigned int nr_xids, i;
	int visible = vx_check(0, VX_ADMIN|VX_WATCH);
	ino_t ino;

	switch ((long)filp->f_pos) {
	case 0:
		ino = fake_ino(0, PROC_XID_INO);
		if (filldir(dirent, ".", 1,
			filp->f_pos, ino, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */
	case 1:
		ino = filp->f_dentry->d_parent->d_inode->i_ino;
		if (filldir(dirent, "..", 2,
			filp->f_pos, ino, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */
	case 2:
		if (visible) {
			ino = fake_ino(0, PROC_XID_INFO);
			if (filldir(dirent, "info", 4,
				filp->f_pos, ino, DT_REG) < 0)
				return 0;
		}
		filp->f_pos++;
		/* fall through */
	case 3:
		ino = fake_ino(0, PROC_XID_STATUS);
		if (filldir(dirent, "status", 6,
			filp->f_pos, ino, DT_REG) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */
	}

	nr_xids = get_xid_list(nr, xid_array, PROC_MAXVIDS);
	for (i = 0; i < nr_xids; i++) {
		int xid = xid_array[i];
		ino_t ino = fake_ino(xid, PROC_XID_INO);
		unsigned int j = PROC_NUMBUF;

		do buf[--j] = '0' + (xid % 10); while (xid/=10);

		if (filldir(dirent, buf+j, PROC_NUMBUF-j,
			filp->f_pos, ino, DT_DIR) < 0)
			break;
		filp->f_pos++;
	}
	return 0;
}


static struct file_operations proc_virtual_dir_operations = {
	.read =		generic_read_dir,
	.readdir =	proc_virtual_readdir,
};

static struct inode_operations proc_virtual_dir_inode_operations = {
	.lookup =	proc_virtual_lookup,
};


int proc_vnet_readdir(struct file * filp,
	void * dirent, filldir_t filldir)
{
	unsigned int nid_array[PROC_MAXVIDS];
	char buf[PROC_NUMBUF];
	unsigned int nr = filp->f_pos-2;
	unsigned int nr_nids, i;
//	int visible = vx_check(0, VX_ADMIN|VX_WATCH);
	ino_t ino;

	switch ((long)filp->f_pos) {
	case 0:
		ino = fake_ino(0, PROC_NID_INO);
		if (filldir(dirent, ".", 1,
			filp->f_pos, ino, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */
	case 1:
		ino = filp->f_dentry->d_parent->d_inode->i_ino;
		if (filldir(dirent, "..", 2,
			filp->f_pos, ino, DT_DIR) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */
	case 2:
		ino = fake_ino(0, PROC_NID_INFO);
		if (filldir(dirent, "info", 4,
			filp->f_pos, ino, DT_REG) < 0)
			return 0;
		filp->f_pos++;
		/* fall through */
	}

	nr_nids = get_nid_list(nr, nid_array, PROC_MAXVIDS);
	for (i = 0; i < nr_nids; i++) {
		int nid = nid_array[i];
		ino_t ino = fake_ino(nid, PROC_NID_INO);
		unsigned long j = PROC_NUMBUF;

		do buf[--j] = '0' + (nid % 10); while (nid/=10);

		if (filldir(dirent, buf+j, PROC_NUMBUF-j,
			filp->f_pos, ino, DT_DIR) < 0)
			break;
		filp->f_pos++;
	}
	return 0;
}


static struct file_operations proc_vnet_dir_operations = {
	.read =		generic_read_dir,
	.readdir =	proc_vnet_readdir,
};

static struct inode_operations proc_vnet_dir_inode_operations = {
	.lookup =	proc_vnet_lookup,
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
		ent->proc_fops = &proc_vnet_dir_operations;
		ent->proc_iops = &proc_vnet_dir_inode_operations;
	}
	proc_vnet = ent;
}




/* per pid info */


int proc_pid_vx_info(struct task_struct *p, char *buffer)
{
	struct vx_info *vxi;
	char * orig = buffer;

	buffer += sprintf (buffer,"XID:\t%d\n", vx_task_xid(p));
	if (vx_flags(VXF_INFO_HIDE, 0))
		goto out;

	vxi = task_get_vx_info(p);
	if (!vxi)
		goto out;

	buffer += sprintf (buffer,"BCaps:\t%016llx\n"
		,(unsigned long long)vxi->vx_bcaps);
	buffer += sprintf (buffer,"CCaps:\t%016llx\n"
		,(unsigned long long)vxi->vx_ccaps);
	buffer += sprintf (buffer,"CFlags:\t%016llx\n"
		,(unsigned long long)vxi->vx_flags);
	buffer += sprintf (buffer,"CIPid:\t%d\n"
		,vxi->vx_initpid);

	put_vx_info(vxi);
out:
	return buffer - orig;
}


int proc_pid_nx_info(struct task_struct *p, char *buffer)
{
	struct nx_info *nxi;
	char * orig = buffer;
	int i;

	buffer += sprintf (buffer,"NID:\t%d\n", nx_task_nid(p));
	if (vx_flags(VXF_INFO_HIDE, 0))
		goto out;
	nxi = task_get_nx_info(p);
	if (!nxi)
		goto out;

	for (i=0; i<nxi->nbipv4; i++){
		buffer += sprintf (buffer,
			"V4Root[%d]:\t%d.%d.%d.%d/%d.%d.%d.%d\n", i
			,NIPQUAD(nxi->ipv4[i])
			,NIPQUAD(nxi->mask[i]));
	}
	buffer += sprintf (buffer,
		"V4Root[bcast]:\t%d.%d.%d.%d\n"
		,NIPQUAD(nxi->v4_bcast));

	put_nx_info(nxi);
out:
	return buffer - orig;
}

