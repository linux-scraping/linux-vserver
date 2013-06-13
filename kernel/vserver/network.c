/*
 *  linux/kernel/vserver/network.c
 *
 *  Virtual Server: Network Support
 *
 *  Copyright (C) 2003-2007  Herbert Pötzl
 *
 *  V0.01  broken out from vcontext V0.05
 *  V0.02  cleaned up implementation
 *  V0.03  added equiv nx commands
 *  V0.04  switch to RCU based hash
 *  V0.05  and back to locking again
 *  V0.06  changed vcmds to nxi arg
 *  V0.07  have __create claim() the nxi
 *
 */

#include <linux/err.h>
#include <linux/slab.h>
#include <linux/rcupdate.h>
#include <net/ipv6.h>

#include <linux/vs_network.h>
#include <linux/vs_pid.h>
#include <linux/vserver/network_cmd.h>


atomic_t nx_global_ctotal	= ATOMIC_INIT(0);
atomic_t nx_global_cactive	= ATOMIC_INIT(0);

static struct kmem_cache *nx_addr_v4_cachep = NULL;
static struct kmem_cache *nx_addr_v6_cachep = NULL;


static int __init init_network(void)
{
	nx_addr_v4_cachep = kmem_cache_create("nx_v4_addr_cache",
		sizeof(struct nx_addr_v4), 0,
		SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);
	nx_addr_v6_cachep = kmem_cache_create("nx_v6_addr_cache",
		sizeof(struct nx_addr_v6), 0,
		SLAB_HWCACHE_ALIGN|SLAB_PANIC, NULL);
	return 0;
}


/*	__alloc_nx_addr_v4()					*/

static inline struct nx_addr_v4 *__alloc_nx_addr_v4(void)
{
	struct nx_addr_v4 *nxa = kmem_cache_alloc(
		nx_addr_v4_cachep, GFP_KERNEL);

	if (!IS_ERR(nxa))
		memset(nxa, 0, sizeof(*nxa));
	return nxa;
}

/*	__dealloc_nx_addr_v4()					*/

static inline void __dealloc_nx_addr_v4(struct nx_addr_v4 *nxa)
{
	kmem_cache_free(nx_addr_v4_cachep, nxa);
}

/*	__dealloc_nx_addr_v4_all()				*/

static inline void __dealloc_nx_addr_v4_all(struct nx_addr_v4 *nxa)
{
	while (nxa) {
		struct nx_addr_v4 *next = nxa->next;

		__dealloc_nx_addr_v4(nxa);
		nxa = next;
	}
}


#ifdef CONFIG_IPV6

/*	__alloc_nx_addr_v6()					*/

static inline struct nx_addr_v6 *__alloc_nx_addr_v6(void)
{
	struct nx_addr_v6 *nxa = kmem_cache_alloc(
		nx_addr_v6_cachep, GFP_KERNEL);

	if (!IS_ERR(nxa))
		memset(nxa, 0, sizeof(*nxa));
	return nxa;
}

/*	__dealloc_nx_addr_v6()					*/

static inline void __dealloc_nx_addr_v6(struct nx_addr_v6 *nxa)
{
	kmem_cache_free(nx_addr_v6_cachep, nxa);
}

/*	__dealloc_nx_addr_v6_all()				*/

static inline void __dealloc_nx_addr_v6_all(struct nx_addr_v6 *nxa)
{
	while (nxa) {
		struct nx_addr_v6 *next = nxa->next;

		__dealloc_nx_addr_v6(nxa);
		nxa = next;
	}
}

#endif	/* CONFIG_IPV6 */

/*	__alloc_nx_info()

	* allocate an initialized nx_info struct
	* doesn't make it visible (hash)			*/

static struct nx_info *__alloc_nx_info(vnid_t nid)
{
	struct nx_info *new = NULL;

	vxdprintk(VXD_CBIT(nid, 1), "alloc_nx_info(%d)*", nid);

	/* would this benefit from a slab cache? */
	new = kmalloc(sizeof(struct nx_info), GFP_KERNEL);
	if (!new)
		return 0;

	memset(new, 0, sizeof(struct nx_info));
	new->nx_id = nid;
	INIT_HLIST_NODE(&new->nx_hlist);
	atomic_set(&new->nx_usecnt, 0);
	atomic_set(&new->nx_tasks, 0);
	spin_lock_init(&new->addr_lock);
	new->nx_state = 0;

	new->nx_flags = NXF_INIT_SET;

	/* rest of init goes here */

	new->v4_lback.s_addr = htonl(INADDR_LOOPBACK);
	new->v4_bcast.s_addr = htonl(INADDR_BROADCAST);

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

	__dealloc_nx_addr_v4_all(nxi->v4.next);
#ifdef CONFIG_IPV6
	__dealloc_nx_addr_v6_all(nxi->v6.next);
#endif

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


void __nx_set_lback(struct nx_info *nxi)
{
	int nid = nxi->nx_id;
	__be32 lback = htonl(INADDR_LOOPBACK ^ ((nid & 0xFFFF) << 8));

	nxi->v4_lback.s_addr = lback;
}

extern int __nx_inet_add_lback(__be32 addr);
extern int __nx_inet_del_lback(__be32 addr);


/*	hash table for nx_info hash */

#define NX_HASH_SIZE	13

struct hlist_head nx_info_hash[NX_HASH_SIZE];

static DEFINE_SPINLOCK(nx_info_hash_lock);


static inline unsigned int __hashval(vnid_t nid)
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
		"__unhash_nx_info: %p[#%d.%d.%d]", nxi, nxi->nx_id,
		atomic_read(&nxi->nx_usecnt), atomic_read(&nxi->nx_tasks));

	/* context must be hashed */
	BUG_ON(!nx_info_state(nxi, NXS_HASHED));
	/* but without tasks */
	BUG_ON(atomic_read(&nxi->nx_tasks));

	nxi->nx_state &= ~NXS_HASHED;
	hlist_del(&nxi->nx_hlist);
	atomic_dec(&nx_global_cactive);
}


/*	__lookup_nx_info()

	* requires the hash_lock to be held
	* doesn't increment the nx_refcnt			*/

static inline struct nx_info *__lookup_nx_info(vnid_t nid)
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
		nid, nxi, nxi ? nxi->nx_id : 0);
	return nxi;
}


/*	__create_nx_info()

	* create the requested context
	* get(), claim() and hash it				*/

static struct nx_info *__create_nx_info(int id)
{
	struct nx_info *new, *nxi = NULL;

	vxdprintk(VXD_CBIT(nid, 1), "create_nx_info(%d)*", id);

	if (!(new = __alloc_nx_info(id)))
		return ERR_PTR(-ENOMEM);

	/* required to make dynamic xids unique */
	spin_lock(&nx_info_hash_lock);

	/* static context requested */
	if ((nxi = __lookup_nx_info(id))) {
		vxdprintk(VXD_CBIT(nid, 0),
			"create_nx_info(%d) = %p (already there)", id, nxi);
		if (nx_info_flags(nxi, NXF_STATE_SETUP, 0))
			nxi = ERR_PTR(-EBUSY);
		else
			nxi = ERR_PTR(-EEXIST);
		goto out_unlock;
	}
	/* new context */
	vxdprintk(VXD_CBIT(nid, 0),
		"create_nx_info(%d) = %p (new)", id, new);
	claim_nx_info(new, NULL);
	__nx_set_lback(new);
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

/*	lookup_nx_info()

	* search for a nx_info and get() it
	* negative id means current				*/

struct nx_info *lookup_nx_info(int id)
{
	struct nx_info *nxi = NULL;

	if (id < 0) {
		nxi = get_nx_info(current_nx_info());
	} else if (id > 1) {
		spin_lock(&nx_info_hash_lock);
		nxi = get_nx_info(__lookup_nx_info(id));
		spin_unlock(&nx_info_hash_lock);
	}
	return nxi;
}

/*	nid_is_hashed()

	* verify that nid is still hashed			*/

int nid_is_hashed(vnid_t nid)
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
	if (!nx_check(0, VS_ADMIN | VS_WATCH)) {
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

	if (nx_info_state(nxi, NXS_SHUTDOWN))
		return -EFAULT;

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


void nx_set_persistent(struct nx_info *nxi)
{
	vxdprintk(VXD_CBIT(nid, 6),
		"nx_set_persistent(%p[#%d])", nxi, nxi->nx_id);

	get_nx_info(nxi);
	claim_nx_info(nxi, NULL);
}

void nx_clear_persistent(struct nx_info *nxi)
{
	vxdprintk(VXD_CBIT(nid, 6),
		"nx_clear_persistent(%p[#%d])", nxi, nxi->nx_id);

	release_nx_info(nxi, NULL);
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


int vc_task_nid(uint32_t id)
{
	vnid_t nid;

	if (id) {
		struct task_struct *tsk;

		rcu_read_lock();
		tsk = find_task_by_real_pid(id);
		nid = (tsk) ? tsk->nid : -ESRCH;
		rcu_read_unlock();
	} else
		nid = nx_current_nid();
	return nid;
}


int vc_nx_info(struct nx_info *nxi, void __user *data)
{
	struct vcmd_nx_info_v0 vc_data;

	vc_data.nid = nxi->nx_id;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}


/* network functions */

int vc_net_create(uint32_t nid, void __user *data)
{
	struct vcmd_net_create vc_data = { .flagword = NXF_INIT_SET };
	struct nx_info *new_nxi;
	int ret;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	if ((nid > MAX_S_CONTEXT) || (nid < 2))
		return -EINVAL;

	new_nxi = __create_nx_info(nid);
	if (IS_ERR(new_nxi))
		return PTR_ERR(new_nxi);

	/* initial flags */
	new_nxi->nx_flags = vc_data.flagword;

	ret = -ENOEXEC;
	if (vs_net_change(new_nxi, VSC_NETUP))
		goto out;

	ret = nx_migrate_task(current, new_nxi);
	if (ret)
		goto out;

	/* return context id on success */
	ret = new_nxi->nx_id;

	/* get a reference for persistent contexts */
	if ((vc_data.flagword & NXF_PERSISTENT))
		nx_set_persistent(new_nxi);
out:
	release_nx_info(new_nxi, NULL);
	put_nx_info(new_nxi);
	return ret;
}


int vc_net_migrate(struct nx_info *nxi, void __user *data)
{
	return nx_migrate_task(current, nxi);
}


static inline
struct nx_addr_v4 *__find_v4_addr(struct nx_info *nxi,
	__be32 ip, __be32 ip2, __be32 mask, uint16_t type, uint16_t flags,
	struct nx_addr_v4 **prev)
{
	struct nx_addr_v4 *nxa = &nxi->v4;

	for (; nxa; nxa = nxa->next) {
		if ((nxa->ip[0].s_addr == ip) &&
		    (nxa->ip[1].s_addr == ip2) &&
		    (nxa->mask.s_addr == mask) &&
		    (nxa->type == type) &&
		    (nxa->flags == flags))
		    return nxa;

		/* save previous entry */
		if (prev)
			*prev = nxa;
	}
	return NULL;
}

int do_add_v4_addr(struct nx_info *nxi, __be32 ip, __be32 ip2, __be32 mask,
	uint16_t type, uint16_t flags)
{
	struct nx_addr_v4 *nxa = NULL;
	struct nx_addr_v4 *new = __alloc_nx_addr_v4();
	unsigned long irqflags;
	int ret = -EEXIST;

	if (IS_ERR(new))
		return PTR_ERR(new);

	spin_lock_irqsave(&nxi->addr_lock, irqflags);
	if (__find_v4_addr(nxi, ip, ip2, mask, type, flags, &nxa))
		goto out_unlock;

	if (NX_IPV4(nxi)) {
		nxa->next = new;
		nxa = new;
		new = NULL;

		/* remove single ip for ip list */
		nxi->nx_flags &= ~NXF_SINGLE_IP;
	}

	nxa->ip[0].s_addr = ip;
	nxa->ip[1].s_addr = ip2;
	nxa->mask.s_addr = mask;
	nxa->type = type;
	nxa->flags = flags;
	ret = 0;
out_unlock:
	spin_unlock_irqrestore(&nxi->addr_lock, irqflags);
	if (new)
		__dealloc_nx_addr_v4(new);
	return ret;
}

int do_remove_v4_addr(struct nx_info *nxi, __be32 ip, __be32 ip2, __be32 mask,
	uint16_t type, uint16_t flags)
{
	struct nx_addr_v4 *nxa = NULL;
	struct nx_addr_v4 *old = NULL;
	unsigned long irqflags;
	int ret = 0;

	spin_lock_irqsave(&nxi->addr_lock, irqflags);
	switch (type) {
	case NXA_TYPE_ADDR:
		old = __find_v4_addr(nxi, ip, ip2, mask, type, flags, &nxa);
		if (old) {
			if (nxa) {
				nxa->next = old->next;
				old->next = NULL;
			} else {
				if (old->next) {
					nxa = old;
					old = old->next;
					*nxa = *old;
					old->next = NULL;
				} else {
					memset(old, 0, sizeof(*old));
					old = NULL;
				}
			}
		} else
			ret = -ESRCH;
		break;

	case NXA_TYPE_ANY:
		nxa = &nxi->v4;
		old = nxa->next;
		memset(nxa, 0, sizeof(*nxa));
		break;

	default:
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&nxi->addr_lock, irqflags);
	__dealloc_nx_addr_v4_all(old);
	return ret;
}


int vc_net_add(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_v0 vc_data;
	int index, ret = 0;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_IPV4:
		if ((vc_data.count < 1) || (vc_data.count > 4))
			return -EINVAL;

		index = 0;
		while (index < vc_data.count) {
			ret = do_add_v4_addr(nxi, vc_data.ip[index].s_addr, 0,
				vc_data.mask[index].s_addr, NXA_TYPE_ADDR, 0);
			if (ret)
				return ret;
			index++;
		}
		ret = index;
		break;

	case NXA_TYPE_IPV4|NXA_MOD_BCAST:
		nxi->v4_bcast = vc_data.ip[0];
		ret = 1;
		break;

	case NXA_TYPE_IPV4|NXA_MOD_LBACK:
		nxi->v4_lback = vc_data.ip[0];
		ret = 1;
		break;

	default:
		ret = -EINVAL;
		break;
	}
	return ret;
}

int vc_net_remove(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_v0 vc_data;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_ANY:
		return do_remove_v4_addr(nxi, 0, 0, 0, vc_data.type, 0);
	default:
		return -EINVAL;
	}
	return 0;
}


int vc_net_add_ipv4_v1(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_ipv4_v1 vc_data;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_ADDR:
	case NXA_TYPE_MASK:
		return do_add_v4_addr(nxi, vc_data.ip.s_addr, 0,
			vc_data.mask.s_addr, vc_data.type, vc_data.flags);

	case NXA_TYPE_ADDR | NXA_MOD_BCAST:
		nxi->v4_bcast = vc_data.ip;
		break;

	case NXA_TYPE_ADDR | NXA_MOD_LBACK:
		nxi->v4_lback = vc_data.ip;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

int vc_net_add_ipv4(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_ipv4_v2 vc_data;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_ADDR:
	case NXA_TYPE_MASK:
	case NXA_TYPE_RANGE:
		return do_add_v4_addr(nxi, vc_data.ip.s_addr, vc_data.ip2.s_addr,
			vc_data.mask.s_addr, vc_data.type, vc_data.flags);

	case NXA_TYPE_ADDR | NXA_MOD_BCAST:
		nxi->v4_bcast = vc_data.ip;
		break;

	case NXA_TYPE_ADDR | NXA_MOD_LBACK:
		nxi->v4_lback = vc_data.ip;
		break;

	default:
		return -EINVAL;
	}
	return 0;
}

int vc_net_rem_ipv4_v1(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_ipv4_v1 vc_data;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_remove_v4_addr(nxi, vc_data.ip.s_addr, 0,
		vc_data.mask.s_addr, vc_data.type, vc_data.flags);
}

int vc_net_rem_ipv4(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_ipv4_v2 vc_data;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	return do_remove_v4_addr(nxi, vc_data.ip.s_addr, vc_data.ip2.s_addr,
		vc_data.mask.s_addr, vc_data.type, vc_data.flags);
}

#ifdef CONFIG_IPV6

static inline
struct nx_addr_v6 *__find_v6_addr(struct nx_info *nxi,
	struct in6_addr *ip, struct in6_addr *mask,
	uint32_t prefix, uint16_t type, uint16_t flags,
	struct nx_addr_v6 **prev)
{
	struct nx_addr_v6 *nxa = &nxi->v6;

	for (; nxa; nxa = nxa->next) {
		if (ipv6_addr_equal(&nxa->ip, ip) &&
		    ipv6_addr_equal(&nxa->mask, mask) &&
		    (nxa->prefix == prefix) &&
		    (nxa->type == type) &&
		    (nxa->flags == flags))
		    return nxa;

		/* save previous entry */
		if (prev)
			*prev = nxa;
	}
	return NULL;
}


int do_add_v6_addr(struct nx_info *nxi,
	struct in6_addr *ip, struct in6_addr *mask,
	uint32_t prefix, uint16_t type, uint16_t flags)
{
	struct nx_addr_v6 *nxa = NULL;
	struct nx_addr_v6 *new = __alloc_nx_addr_v6();
	unsigned long irqflags;
	int ret = -EEXIST;

	if (IS_ERR(new))
		return PTR_ERR(new);

	spin_lock_irqsave(&nxi->addr_lock, irqflags);
	if (__find_v6_addr(nxi, ip, mask, prefix, type, flags, &nxa))
		goto out_unlock;

	if (NX_IPV6(nxi)) {
		nxa->next = new;
		nxa = new;
		new = NULL;
	}

	nxa->ip = *ip;
	nxa->mask = *mask;
	nxa->prefix = prefix;
	nxa->type = type;
	nxa->flags = flags;
	ret = 0;
out_unlock:
	spin_unlock_irqrestore(&nxi->addr_lock, irqflags);
	if (new)
		__dealloc_nx_addr_v6(new);
	return ret;
}

int do_remove_v6_addr(struct nx_info *nxi,
	struct in6_addr *ip, struct in6_addr *mask,
	uint32_t prefix, uint16_t type, uint16_t flags)
{
	struct nx_addr_v6 *nxa = NULL;
	struct nx_addr_v6 *old = NULL;
	unsigned long irqflags;
	int ret = 0;

	spin_lock_irqsave(&nxi->addr_lock, irqflags);
	switch (type) {
	case NXA_TYPE_ADDR:
		old = __find_v6_addr(nxi, ip, mask, prefix, type, flags, &nxa);
		if (old) {
			if (nxa) {
				nxa->next = old->next;
				old->next = NULL;
			} else {
				if (old->next) {
					nxa = old;
					old = old->next;
					*nxa = *old;
					old->next = NULL;
				} else {
					memset(old, 0, sizeof(*old));
					old = NULL;
				}
			}
		} else
			ret = -ESRCH;
		break;

	case NXA_TYPE_ANY:
		nxa = &nxi->v6;
		old = nxa->next;
		memset(nxa, 0, sizeof(*nxa));
		break;

	default:
		ret = -EINVAL;
	}
	spin_unlock_irqrestore(&nxi->addr_lock, irqflags);
	__dealloc_nx_addr_v6_all(old);
	return ret;
}

int vc_net_add_ipv6(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_ipv6_v1 vc_data;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_ADDR:
		memset(&vc_data.mask, ~0, sizeof(vc_data.mask));
		/* fallthrough */
	case NXA_TYPE_MASK:
		return do_add_v6_addr(nxi, &vc_data.ip, &vc_data.mask,
			vc_data.prefix, vc_data.type, vc_data.flags);
	default:
		return -EINVAL;
	}
	return 0;
}

int vc_net_remove_ipv6(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_addr_ipv6_v1 vc_data;

	if (data && copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	switch (vc_data.type) {
	case NXA_TYPE_ADDR:
		memset(&vc_data.mask, ~0, sizeof(vc_data.mask));
		/* fallthrough */
	case NXA_TYPE_MASK:
		return do_remove_v6_addr(nxi, &vc_data.ip, &vc_data.mask,
			vc_data.prefix, vc_data.type, vc_data.flags);
	case NXA_TYPE_ANY:
		return do_remove_v6_addr(nxi, NULL, NULL, 0, vc_data.type, 0);
	default:
		return -EINVAL;
	}
	return 0;
}

#endif	/* CONFIG_IPV6 */


int vc_get_nflags(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_flags_v0 vc_data;

	vc_data.flagword = nxi->nx_flags;

	/* special STATE flag handling */
	vc_data.mask = vs_mask_flags(~0ULL, nxi->nx_flags, NXF_ONE_TIME);

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_nflags(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_flags_v0 vc_data;
	uint64_t mask, trigger;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
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
	vc_data.cmask = ~0ULL;

	if (copy_to_user(data, &vc_data, sizeof(vc_data)))
		return -EFAULT;
	return 0;
}

int vc_set_ncaps(struct nx_info *nxi, void __user *data)
{
	struct vcmd_net_caps_v0 vc_data;

	if (copy_from_user(&vc_data, data, sizeof(vc_data)))
		return -EFAULT;

	nxi->nx_ncaps = vs_mask_flags(nxi->nx_ncaps,
		vc_data.ncaps, vc_data.cmask);
	return 0;
}


#include <linux/module.h>

module_init(init_network);

EXPORT_SYMBOL_GPL(free_nx_info);
EXPORT_SYMBOL_GPL(unhash_nx_info);

