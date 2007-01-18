/*
 *  linux/kernel/vserver/network.c
 *
 *  Virtual Server: Network Support
 *
 *  Copyright (C) 2003-2006  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *  V0.02  cleaned up implementation
 *  V0.03  added equiv nx commands
 *  V0.04  switch to RCU based hash
 *  V0.05  and back to locking again
 *  V0.06  changed vcmds to nxi arg
 *
 */

#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <net/tcp.h>

#include <asm/errno.h>
#include <linux/vserver/base.h>
#include <linux/vserver/network_cmd.h>


atomic_t nx_global_ctotal	= ATOMIC_INIT(0);
atomic_t nx_global_cactive	= ATOMIC_INIT(0);


/*	__alloc_nx_info()

	* allocate an initialized nx_info struct
	* doesn't make it visible (hash)			*/

static struct nx_info *__alloc_nx_info(nid_t nid)
{
	struct nx_info *new = NULL;

	vxdprintk(VXD_CBIT(nid, 1), "alloc_nx_info(%d)*", nid);

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct nx_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset (new, 0, sizeof(struct nx_info));
	new->nx_id = nid;
	INIT_HLIST_NODE(&new->nx_hlist);
	atomic_set(&new->nx_usecnt, 0);
	atomic_set(&new->nx_tasks, 0);
	new->nx_state = 0;

	new->nx_flags = NXF_INIT_SET;

	/* rest of init goes here */

	vxdprintk(VXD_CBIT(nid, 0),
		"alloc_nx_info(%d) = %p", nid, new);
	atomic_inc(&nx_global_ctotal);
	return new;
}

/*	__dealloc_nx_info()

	* final disposal of nx_info				*/

static void __dealloc_nx_info(struct nx_info *nxi)
{
	vxdprintk(VXD_CBIT(nid, 0),
		"dealloc_nx_info(%p)", nxi);

	nxi->nx_hlist.next = LIST_POISON1;
	nxi->nx_id = -1;

	BUG_ON(atomic_read(&nxi->nx_usecnt));
	BUG_ON(atomic_read(&nxi->nx_tasks));

	nxi->nx_state |= NXS_RELEASED;
	kfree(nxi);
	atomic_dec(&nx_global_ctotal);
}

static void __shutdown_nx_info(struct nx_info *nxi)
{
	nxi->nx_state |= NXS_SHUTDOWN;
	vs_net_change(nxi, VSC_NETDOWN);
}

/*	exported stuff						*/

void free_nx_info(struct nx_info *nxi)
{
	/* context shutdown is mandatory */
	BUG_ON(nxi->nx_state != NXS_SHUTDOWN);

	/* context must not be hashed */
	BUG_ON(nxi->nx_state & NXS_HASHED);

	BUG_ON(atomic_read(&nxi->nx_usecnt));
	BUG_ON(atomic_read(&nxi->nx_tasks));

	__dealloc_nx_info(nxi);
}


/*	hash table for nx_info hash */

#define NX_HASH_SIZE	13

struct hlist_head nx_info_hash[NX_HASH_SIZE];

static spinlock_t nx_info_hash_lock = SPIN_LOCK_UNLOCKED;


static inline unsigned int __hashval(nid_t nid)
{
	return (nid % NX_HASH_SIZE);
}



/*	__hash_nx_info()

	* add the nxi to the global hash table
	* requires the hash_lock to be held			*/

static inline void __hash_nx_info(struct nx_info *nxi)
{
	struct hlist_head *head;

	vxd_assert_lock(&nx_info_hash_lock);
	vxdprintk(VXD_CBIT(nid, 4),
		"__hash_nx_info: %p[#%d]", nxi, nxi->nx_id);

	/* context must not be hashed */
	BUG_ON(nx_info_state(nxi, NXS_HASHED));

	nxi->nx_state |= NXS_HASHED;
	head = &nx_info_hash[__hashval(nxi->nx_id)];
	hlist_add_head(&nxi->nx_hlist, head);
	atomic_inc(&nx_global_cactive);
}

/*	__unhash_nx_info()

	* remove the nxi from the global hash table
	* requires the hash_lock to be held			*/

static inline void __unhash_nx_info(struct nx_info *nxi)
{
	vxd_assert_lock(&nx_info_hash_lock);
	vxdprintk(VXD_CBIT(nid, 4),
		"__unhash_nx_info: %p[#%d]", nxi, nxi->nx_id);

	/* context must be hashed */
	BUG_ON(!nx_info_state(nxi, NXS_HASHED));

	nxi->nx_state &= ~NXS_HASHED;
	hlist_del(&nxi->nx_hlist);
	atomic_dec(&nx_global_cactive);
}


/*	__lookup_nx_info()

	* requires the hash_lock to be held
	* doesn't increment the nx_refcnt			*/

static inline struct nx_info *__lookup_nx_info(nid_t nid)
{
	struct hlist_head *head = &nx_info_hash[__hashval(nid)];
	struct hlist_node *pos;
	struct nx_info *nxi;

	vxd_assert_lock(&nx_info_hash_lock);
	hlist_for_each(pos, head) {
		nxi = hlist_entry(pos, struct nx_info, nx_hlist);

		if (nxi->nx_id == nid)
			goto found;
	}
	nxi = NULL;
found:
	vxdprintk(VXD_CBIT(nid, 0),
		"__lookup_nx_info(#%u): %p[#%u]",
		nid, nxi, nxi?nxi->nx_id:0);
	return nxi;
}


/*	__nx_dynamic_id()

	* find unused dynamic nid
	* requires the hash_lock to be held			*/

static inline nid_t __nx_dynamic_id(void)
{
	static nid_t seq = MAX_N_CONTEXT;
	nid_t barrier = seq;

	vxd_assert_lock(&nx_info_hash_lock);
	do {
		if (++seq > MAX_N_CONTEXT)
			seq = MIN_D_CONTEXT;
		if (!__lookup_nx_info(seq)) {
			vxdprintk(VXD_CBIT(nid, 4),
				"__nx_dynamic_id: [#%d]", seq);
			return seq;
		}
	} while (barrier != seq);
	return 0;
}

/*	__create_nx_info()

	* create the requested context
	* get() and hash it					*/

static struct nx_info * __create_nx_info(int id)
{
	struct nx_info *new, *nxi = NULL;

	vxdprintk(VXD_CBIT(nid, 1), "create_nx_info(%d)*", id);

	if (!(new = __alloc_nx_info(id)))
		return ERR_PTR(-ENOMEM);

	/* required to make dynamic xids unique */
	spin_lock(&nx_info_hash_lock);

	/* dynamic context requested */
	if (id == NX_DYNAMIC_ID) {
#ifdef	CONFIG_VSERVER_DYNAMIC_IDS
		id = __nx_dynamic_id();
		if (!id) {
			printk(KERN_ERR "no dynamic context available.\n");
			nxi = ERR_PTR(-EAGAIN);
			goto out_unlock;
		}
		new->nx_id = id;
#else
		printk(KERN_ERR "dynamic contexts disabled.\n");
		nxi = ERR_PTR(-EINVAL);
		goto out_unlock;
#endif
	}
	/* static context requested */
	else if ((nxi = __lookup_nx_info(id))) {
		vxdprintk(VXD_CBIT(nid, 0),
			"create_nx_info(%d) = %p (already there)", id, nxi);
		if (nx_info_flags(nxi, NXF_STATE_SETUP, 0))
			nxi = ERR_PTR(-EBUSY);
		else
			nxi = ERR_PTR(-EEXIST);
		goto out_unlock;
	}
	/* dynamic nid creation blocker */
	else if (id >= MIN_D_CONTEXT) {
		vxdprintk(VXD_CBIT(nid, 0),
			"create_nx_info(%d) (dynamic rejected)", id);
		nxi = ERR_PTR(-EINVAL);
		goto out_unlock;
	}

	/* new context */
	vxdprintk(VXD_CBIT(nid, 0),
		"create_nx_info(%d) = %p (new)", id, new);
	__hash_nx_info(get_nx_info(new));
	nxi = new, new = NULL;

out_unlock:
	spin_unlock(&nx_info_hash_lock);
	if (new)
		__dealloc_nx_info(new);
	return nxi;
}



/*	exported stuff						*/


void unhash_nx_info(struct nx_info *nxi)
{
	__shutdown_nx_info(nxi);
	spin_lock(&nx_info_hash_lock);
	__unhash_nx_info(nxi);
	spin_unlock(&nx_info_hash_lock);
}

#ifdef  CONFIG_VSERVER_LEGACYNET

struct nx_info *create_nx_info(void)
{
	return __create_nx_info(NX_DYNAMIC_ID);
}

#endif

/*	lookup_nx_info()

	* search for a nx_info and get() it
	* negative id means current				*/

struct nx_info *lookup_nx_info(int id)
{
	struct nx_info *nxi = NULL;

	if (id < 0) {
		nxi = get_nx_info(current->nx_info);
	} else if (id > 1) {
		spin_lock(&nx_info_hash_lock);
		nxi = get_nx_info(__lookup_nx_info(id));
		spin_unlock(&nx_info_hash_lock);
	}
	return nxi;
}

/*	nid_is_hashed()

	* verify that nid is still hashed			*/

int nid_is_hashed(nid_t nid)
{
	int hashed;

	spin_lock(&nx_info_hash_lock);
	hashed = (__lookup_nx_info(nid) != NULL);
	spin_unlock(&nx_info_hash_lock);
	return hashed;
}


#ifdef	CONFIG_PROC_FS

/*	get_nid_list()

	* get a subset of hashed nids for proc
	* assumes size is at least one				*/

int get_nid_list(int index, unsigned int *nids, int size)
{
	int hindex, nr_nids = 0;

	/* only show current and children */
	if (!nx_check(0, VS_ADMIN|VS_WATCH)) {
		if (index > 0)
			return 0;
		nids[nr_nids] = nx_current_nid();
		return 1;
	}

	for (hindex = 0; hindex < NX_HASH_SIZE; hindex++) {
		struct hlist_head *head = &nx_info_hash[hindex];
		struct hlist_node *pos;

		spin_lock(&nx_info_hash_lock);
		hlist_for_each(pos, head) {
			struct nx_info *nxi;

			if (--index > 0)
				continue;

			nxi = hlist_entry(pos, struct nx_info, nx_hlist);
			nids[nr_nids] = nxi->nx_id;
			if (++nr_nids >= size) {
				spin_unlock(&nx_info_hash_lock);
				goto out;
			}
		}
		/* keep the lock time short */
		spin_unlock(&nx_info_hash_lock);
	}
out:
	return nr_nids;
}
#endif


/*
 *	migrate task to new network
 *	gets nxi, puts old_nxi on change
 */

int nx_migrate_task(struct task_struct *p, struct nx_info *nxi)
{
	struct nx_info *old_nxi;
	int ret = 0;

	if (!p || !nxi)
		BUG();

	vxdprintk(VXD_CBIT(nid, 5),
		"nx_migrate_task(%p,%p[#%d.%d.%d])",
		p, nxi, nxi->nx_id,
		atomic_read(&nxi->nx_usecnt),
		atomic_read(&nxi->nx_tasks));

	if (nx_info_flags(nxi, NXF_INFO_PRIVATE, 0) &&
		!nx_info_flags(nxi, NXF_STATE_SETUP, 0))
		return -EACCES;

	/* maybe disallow this completely? */
	old_nxi = task_get_nx_info(p);
	if (old_nxi == nxi)
		goto out;

	task_lock(p);
	if (old_nxi)
		clr_nx_info(&p->nx_info);
	claim_nx_info(nxi, p);
	set_nx_info(&p->nx_info, nxi);
	p->nid = nxi->nx_id;
	task_unlock(p);

	vxdprintk(VXD_CBIT(nid, 5),
		"moved task %p into nxi:%p[#%d]",
		p, nxi, nxi->nx_id);

	if (old_nxi)
		release_nx_info(old_nxi, p);
	ret = 0;
out:
	put_nx_info(old_nxi);
	return ret;
}


#ifdef CONFIG_INET

#include <linux/netdevice.h>
#include <linux/inetdevice.h>

int ifa_in_nx_info(struct in_ifaddr *ifa, struct nx_info *nxi)
{
	if (!nxi)
		return 1;
	if (!ifa)
		return 0;
	return addr_in_nx_info(nxi, ifa->ifa_local);
}

int dev_in_nx_info(struct net_device *dev, struct nx_info *nxi)
{
	struct in_device *in_dev;
	struct in_ifaddr **ifap;
	struct in_ifaddr *ifa;
	int ret = 0;

	if (!nxi)
		return 1;

	in_dev = in_dev_get(dev);
	if (!in_dev)
		goto out;

	for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
		ifap = &ifa->ifa_next) {
		if (addr_in_nx_info(nxi, ifa->ifa_local)) {
			ret = 1;
			break;
		}
	}
	in_dev_put(in_dev);
out:
	return ret;
}

/*
 *	check if address is covered by socket
 *
 *	sk:	the socket to check against
 *	addr:	the address in question (must be != 0)
 */
static inline int __addr_in_socket(struct sock *sk, uint32_t addr)
{
	struct nx_info *nxi = sk->sk_nx_info;
	uint32_t saddr = inet_rcv_saddr(sk);

	vxdprintk(VXD_CBIT(net, 5),
		"__addr_in_socket(%p,%d.%d.%d.%d) %p:%d.%d.%d.%d %p;%lx",
		sk, VXD_QUAD(addr), nxi, VXD_QUAD(saddr), sk->sk_socket,
		(sk->sk_socket?sk->sk_socket->flags:0));

	if (saddr) {
		/* direct address match */
		return (saddr == addr);
	} else if (nxi) {
		/* match against nx_info */
		return addr_in_nx_info(nxi, addr);
	} else {
		/* unrestricted any socket */
		return 1;
	}
}


int nx_addr_conflict(struct nx_info *nxi, uint32_t addr, struct sock *sk)
{
	vxdprintk(VXD_CBIT(net, 2),
		"nx_addr_conflict(%p,%p) %d.%d,%d.%d",
		nxi, sk, VXD_QUAD(addr));

	if (addr) {
		/* check real address */
		return __addr_in_socket(sk, addr);
	} else if (nxi) {
		/* check against nx_info */
		int i, n = nxi->nbipv4;

		for (i=0; i<n; i++)
			if (__addr_in_socket(sk, nxi->ipv4[i]))
				return 1;
		return 0;
	} else {
		/* check against any */
		return 1;
	}
}

#endif /* CONFIG_INET */

void nx_set_persistent(struct nx_info *nxi)
{
	get_nx_info(nxi);
	claim_nx_info(nxi, current);
}

void nx_clear_persistent(struct nx_info *nxi)
{
	vxdprintk(VXD_CBIT(nid, 6),
		"nx_clear_persistent(%p[#%d])", nxi, nxi->nx_id);

	release_nx_info(nxi, current);
	put_nx_info(nxi);
}

void nx_update_persistent(struct nx_info *nxi)
{
	if (nx_info_flags(nxi, NXF_PERSISTENT, 0))
		nx_set_persistent(nxi);
	else
		nx_clear_persistent(nxi);
}

/* vserver syscall commands below here */

/* taks nid and nx_info functions */

#include <asm/uaccess.h>


int vc_task_nid(uint32_t id, void __user *data)
{
	nid_t nid;

	if (id) {
		struct task_struct *tsk;

		if (!nx_check(0, VS_ADMIN|VS_WATCH))
			return -EPERM;

		read_lock(&tasklist_lock);
		tsk = find_task_by_real_pid(id);
		nid = (tsk) ? tsk->nid : -ESRCH;
		read_unlock(&tasklist_lock);
	}
	else
		nid = nx_current_nid();
	return nid;
}


int vc_nx_info(struct nx_info *nxi, void __user *data)
{
	struct vcmd_nx_info_v0 vc_data;

	vc_data.nid = nxi->nx_id;

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


/* network functions */

int vc_net_create(uint32_t nid, void __user *data)
{
	struct vcmd_net_create vc_data = { .flagword = NXF_INIT_SET };
	struct nx_info *new_nxi;
	int ret;

	if (data && copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if ((nid > MAX_S_CONTEXT) && (nid != NX_DYNAMIC_ID))
		return -EINVAL;
	if (nid < 2)
		return -EINVAL;

	new_nxi = __create_nx_info(nid);
	if (IS_ERR(new_nxi))
		return PTR_ERR(new_nxi);

	/* initial flags */
	new_nxi->nx_flags = vc_data.flagword;

	/* get a reference for persistent contexts */
	if ((vc_data.flagword & NXF_PERSISTENT))
		nx_set_persistent(new_nxi);

	ret = -ENOEXEC;
	if (vs_net_change(new_nxi, VSC_NETUP))
		goto out_unhash;
	ret = nx_migrate_task(current, new_nxi);
	if (!ret) {
		/* return context id on success */
		ret = new_nxi->nx_id;
		goto out;
	}
out_unhash:
	/* prepare for context disposal */
	new_nxi->nx_state |= NXS_SHUTDOWN;
	if ((vc_data.flagword & NXF_PERSISTENT))
		nx_clear_persistent(new_nxi);
	__unhash_nx_info(new_nxi);
out:
	put_nx_info(new_nxi);
	return ret;
}


int vc_net_migrate(struct nx_info *nxi, void __user *data)
{
	return nx_migrate_task(current, nxi);
}

int vc_net_add(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_v0 vc_data;
	int index, pos, ret = 0;

	if (data && copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_IPV4:
		if ((vc_data.count < 1) || (vc_data.count > 4))
			return -EINVAL;
		break;

	default:
		break;
	}

	switch (vc_data.type) {
	case NXA_TYPE_IPV4:
		index = 0;
		while ((index < vc_data.count) &&
			((pos = nxi->nbipv4) < NB_IPV4ROOT)) {
			nxi->ipv4[pos] = vc_data.ip[index];
			nxi->mask[pos] = vc_data.mask[index];
			index++;
			nxi->nbipv4++;
		}
		ret = index;
		break;

	case NXA_TYPE_IPV4|NXA_MOD_BCAST:
		nxi->v4_bcast = vc_data.ip[0];
		ret = 1;
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int vc_net_remove(struct nx_info * nxi, void __user *data)
{
	struct vcmd_net_addr_v0 vc_data;

	if (data && copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_ANY:
		nxi->nbipv4 = 0;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

int vc_get_nflags(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_flags_v0 vc_data;

	vc_data.flagword = nxi->nx_flags;

	/* special STATE flag handling */
	vc_data.mask = vs_mask_flags(~0UL, nxi->nx_flags, NXF_ONE_TIME);

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_nflags(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_flags_v0 vc_data;
	uint64_t mask, trigger;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	/* special STATE flag handling */
	mask = vs_mask_mask(vc_data.mask, nxi->nx_flags, NXF_ONE_TIME);
	trigger = (mask & nxi->nx_flags) ^ (mask & vc_data.flagword);

	nxi->nx_flags = vs_mask_flags(nxi->nx_flags,
		vc_data.flagword, mask);
	if (trigger & NXF_PERSISTENT)
		nx_update_persistent(nxi);

	return 0;
}

int vc_get_ncaps(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_caps_v0 vc_data;

	vc_data.ncaps = nxi->nx_ncaps;
	vc_data.cmask = ~0UL;

	if (copy_to_user (data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_ncaps(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_caps_v0 vc_data;

	if (copy_from_user (&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	nxi->nx_ncaps = vs_mask_flags(nxi->nx_ncaps,
		vc_data.ncaps, vc_data.cmask);
	return 0;
}


#include <linux/module.h>

EXPORT_SYMBOL_GPL(free_nx_info);
EXPORT_SYMBOL_GPL(unhash_nx_info);

