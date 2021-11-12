/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdbool.h>
#include <stdint.h>

#include <rte_eal.h>
#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_rwlock.h>
#include <rte_string_fns.h>
#include <rte_tailq.h>

#include <rte_rib6.h>

#define RTE_RIB_VALID_NODE	1
#define RIB6_MAXDEPTH		128
/* Maximum length of a RIB6 name. */
#define RTE_RIB6_NAMESIZE	64

TAILQ_HEAD(rte_rib6_list, rte_tailq_entry);
static struct rte_tailq_elem rte_rib6_tailq = {
	.name = "RTE_RIB6",
};
EAL_REGISTER_TAILQ(rte_rib6_tailq)

struct rte_rib6_node {
	struct rte_rib6_node	*left;
	struct rte_rib6_node	*right;
	struct rte_rib6_node	*parent;
	uint64_t		nh;
	uint8_t			ip[RTE_RIB6_IPV6_ADDR_SIZE];
	uint8_t			depth;
	uint8_t			flag;
	__extension__ uint64_t		ext[0];
};

struct rte_rib6 {
	char		name[RTE_RIB6_NAMESIZE];
	struct rte_rib6_node	*tree;
	struct rte_mempool	*node_pool;
	uint32_t		cur_nodes;
	uint32_t		cur_routes;
	int			max_nodes;
};

static inline bool
is_valid_node(struct rte_rib6_node *node)
{
	return (node->flag & RTE_RIB_VALID_NODE) == RTE_RIB_VALID_NODE;
}

static inline bool
is_right_node(struct rte_rib6_node *node)
{
	return node->parent->right == node;
}

/*
 * Check if ip1 is covered by ip2/depth prefix
 */
static inline bool
is_covered(const uint8_t ip1[RTE_RIB6_IPV6_ADDR_SIZE],
		const uint8_t ip2[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth)
{
	int i;

	for (i = 0; i < RTE_RIB6_IPV6_ADDR_SIZE; i++)
		if ((ip1[i] ^ ip2[i]) & get_msk_part(depth, i))
			return false;

	return true;
}

static inline int
get_dir(const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth)
{
	uint8_t index, msk;

	/*
	 * depth & 127 clamps depth to values that will not
	 * read off the end of ip.
	 * depth is the number of bits deep into ip to traverse, and
	 * is incremented in blocks of 8 (1 byte). This means the last
	 * 3 bits are irrelevant to what the index of ip should be.
	 */
	index = (depth & INT8_MAX) / CHAR_BIT;

	/*
	 * msk is the bitmask used to extract the bit used to decide the
	 * direction of the next step of the binary search.
	 */
	msk = 1 << (7 - (depth & 7));

	return (ip[index] & msk) != 0;
}

static inline struct rte_rib6_node *
get_nxt_node(struct rte_rib6_node *node,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE])
{
	if (node->depth == RIB6_MAXDEPTH)
		return NULL;

	return (get_dir(ip, node->depth)) ? node->right : node->left;
}

static struct rte_rib6_node *
node_alloc(struct rte_rib6 *rib)
{
	struct rte_rib6_node *ent;
	int ret;

	ret = rte_mempool_get(rib->node_pool, (void *)&ent);
	if (unlikely(ret != 0))
		return NULL;
	++rib->cur_nodes;
	return ent;
}

static void
node_free(struct rte_rib6 *rib, struct rte_rib6_node *ent)
{
	--rib->cur_nodes;
	rte_mempool_put(rib->node_pool, ent);
}

struct rte_rib6_node *
rte_rib6_lookup(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE])
{
	struct rte_rib6_node *cur;
	struct rte_rib6_node *prev = NULL;

	if (unlikely(rib == NULL)) {
		rte_errno = EINVAL;
		return NULL;
	}
	cur = rib->tree;

	while ((cur != NULL) && is_covered(ip, cur->ip, cur->depth)) {
		if (is_valid_node(cur))
			prev = cur;
		cur = get_nxt_node(cur, ip);
	}
	return prev;
}

struct rte_rib6_node *
rte_rib6_lookup_parent(struct rte_rib6_node *ent)
{
	struct rte_rib6_node *tmp;

	if (ent == NULL)
		return NULL;

	tmp = ent->parent;
	while ((tmp != NULL) && (!is_valid_node(tmp)))
		tmp = tmp->parent;

	return tmp;
}

struct rte_rib6_node *
rte_rib6_lookup_exact(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth)
{
	struct rte_rib6_node *cur;
	uint8_t tmp_ip[RTE_RIB6_IPV6_ADDR_SIZE];
	int i;

	if ((rib == NULL) || (ip == NULL) || (depth > RIB6_MAXDEPTH)) {
		rte_errno = EINVAL;
		return NULL;
	}
	cur = rib->tree;

	for (i = 0; i < RTE_RIB6_IPV6_ADDR_SIZE; i++)
		tmp_ip[i] = ip[i] & get_msk_part(depth, i);

	while (cur != NULL) {
		if (rte_rib6_is_equal(cur->ip, tmp_ip) &&
				(cur->depth == depth) &&
				is_valid_node(cur))
			return cur;

		if (!(is_covered(tmp_ip, cur->ip, cur->depth)) ||
				(cur->depth >= depth))
			break;

		cur = get_nxt_node(cur, tmp_ip);
	}

	return NULL;
}

/*
 *  Traverses on subtree and retrieves more specific routes
 *  for a given in args ip/depth prefix
 *  last = NULL means the first invocation
 */
struct rte_rib6_node *
rte_rib6_get_nxt(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE],
	uint8_t depth, struct rte_rib6_node *last, int flag)
{
	struct rte_rib6_node *tmp, *prev = NULL;
	uint8_t tmp_ip[RTE_RIB6_IPV6_ADDR_SIZE];
	int i;

	if ((rib == NULL) || (ip == NULL) || (depth > RIB6_MAXDEPTH)) {
		rte_errno = EINVAL;
		return NULL;
	}

	for (i = 0; i < RTE_RIB6_IPV6_ADDR_SIZE; i++)
		tmp_ip[i] = ip[i] & get_msk_part(depth, i);

	if (last == NULL) {
		tmp = rib->tree;
		while ((tmp) && (tmp->depth < depth))
			tmp = get_nxt_node(tmp, tmp_ip);
	} else {
		tmp = last;
		while ((tmp->parent != NULL) && (is_right_node(tmp) ||
				(tmp->parent->right == NULL))) {
			tmp = tmp->parent;
			if (is_valid_node(tmp) &&
					(is_covered(tmp->ip, tmp_ip, depth) &&
					(tmp->depth > depth)))
				return tmp;
		}
		tmp = (tmp->parent != NULL) ? tmp->parent->right : NULL;
	}
	while (tmp) {
		if (is_valid_node(tmp) &&
				(is_covered(tmp->ip, tmp_ip, depth) &&
				(tmp->depth > depth))) {
			prev = tmp;
			if (flag == RTE_RIB6_GET_NXT_COVER)
				return prev;
		}
		tmp = (tmp->left != NULL) ? tmp->left : tmp->right;
	}
	return prev;
}

void
rte_rib6_remove(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth)
{
	struct rte_rib6_node *cur, *prev, *child;

	cur = rte_rib6_lookup_exact(rib, ip, depth);
	if (cur == NULL)
		return;

	--rib->cur_routes;
	cur->flag &= ~RTE_RIB_VALID_NODE;
	while (!is_valid_node(cur)) {
		if ((cur->left != NULL) && (cur->right != NULL))
			return;
		child = (cur->left == NULL) ? cur->right : cur->left;
		if (child != NULL)
			child->parent = cur->parent;
		if (cur->parent == NULL) {
			rib->tree = child;
			node_free(rib, cur);
			return;
		}
		if (cur->parent->left == cur)
			cur->parent->left = child;
		else
			cur->parent->right = child;
		prev = cur;
		cur = cur->parent;
		node_free(rib, prev);
	}
}

struct rte_rib6_node *
rte_rib6_insert(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth)
{
	struct rte_rib6_node **tmp;
	struct rte_rib6_node *prev = NULL;
	struct rte_rib6_node *new_node = NULL;
	struct rte_rib6_node *common_node = NULL;
	uint8_t common_prefix[RTE_RIB6_IPV6_ADDR_SIZE];
	uint8_t tmp_ip[RTE_RIB6_IPV6_ADDR_SIZE];
	int i, d;
	uint8_t common_depth, ip_xor;

	if (unlikely((rib == NULL) || (ip == NULL) ||
			(depth > RIB6_MAXDEPTH))) {
		rte_errno = EINVAL;
		return NULL;
	}

	tmp = &rib->tree;

	for (i = 0; i < RTE_RIB6_IPV6_ADDR_SIZE; i++)
		tmp_ip[i] = ip[i] & get_msk_part(depth, i);

	new_node = rte_rib6_lookup_exact(rib, tmp_ip, depth);
	if (new_node != NULL) {
		rte_errno = EEXIST;
		return NULL;
	}

	new_node = node_alloc(rib);
	if (new_node == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}
	new_node->left = NULL;
	new_node->right = NULL;
	new_node->parent = NULL;
	rte_rib6_copy_addr(new_node->ip, tmp_ip);
	new_node->depth = depth;
	new_node->flag = RTE_RIB_VALID_NODE;

	/* traverse down the tree to find matching node or closest matching */
	while (1) {
		/* insert as the last node in the branch */
		if (*tmp == NULL) {
			*tmp = new_node;
			new_node->parent = prev;
			++rib->cur_routes;
			return *tmp;
		}
		/*
		 * Intermediate node found.
		 * Previous rte_rib6_lookup_exact() returned NULL
		 * but node with proper search criteria is found.
		 * Validate intermediate node and return.
		 */
		if (rte_rib6_is_equal(tmp_ip, (*tmp)->ip) &&
				(depth == (*tmp)->depth)) {
			node_free(rib, new_node);
			(*tmp)->flag |= RTE_RIB_VALID_NODE;
			++rib->cur_routes;
			return *tmp;
		}

		if (!is_covered(tmp_ip, (*tmp)->ip, (*tmp)->depth) ||
				((*tmp)->depth >= depth)) {
			break;
		}
		prev = *tmp;

		tmp = (get_dir(tmp_ip, (*tmp)->depth)) ? &(*tmp)->right :
				&(*tmp)->left;
	}

	/* closest node found, new_node should be inserted in the middle */
	common_depth = RTE_MIN(depth, (*tmp)->depth);
	for (i = 0, d = 0; i < RTE_RIB6_IPV6_ADDR_SIZE; i++) {
		ip_xor = tmp_ip[i] ^ (*tmp)->ip[i];
		if (ip_xor == 0)
			d += 8;
		else {
			d += __builtin_clz(ip_xor << 24);
			break;
		}
	}

	common_depth = RTE_MIN(d, common_depth);

	for (i = 0; i < RTE_RIB6_IPV6_ADDR_SIZE; i++)
		common_prefix[i] = tmp_ip[i] & get_msk_part(common_depth, i);

	if (rte_rib6_is_equal(common_prefix, tmp_ip) &&
			(common_depth == depth)) {
		/* insert as a parent */
		if (get_dir((*tmp)->ip, depth))
			new_node->right = *tmp;
		else
			new_node->left = *tmp;
		new_node->parent = (*tmp)->parent;
		(*tmp)->parent = new_node;
		*tmp = new_node;
	} else {
		/* create intermediate node */
		common_node = node_alloc(rib);
		if (common_node == NULL) {
			node_free(rib, new_node);
			rte_errno = ENOMEM;
			return NULL;
		}
		rte_rib6_copy_addr(common_node->ip, common_prefix);
		common_node->depth = common_depth;
		common_node->flag = 0;
		common_node->parent = (*tmp)->parent;
		new_node->parent = common_node;
		(*tmp)->parent = common_node;
		if (get_dir((*tmp)->ip, common_depth) == 1) {
			common_node->left = new_node;
			common_node->right = *tmp;
		} else {
			common_node->left = *tmp;
			common_node->right = new_node;
		}
		*tmp = common_node;
	}
	++rib->cur_routes;
	return new_node;
}

int
rte_rib6_get_ip(const struct rte_rib6_node *node,
		uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE])
{
	if ((node == NULL) || (ip == NULL)) {
		rte_errno = EINVAL;
		return -1;
	}
	rte_rib6_copy_addr(ip, node->ip);
	return 0;
}

int
rte_rib6_get_depth(const struct rte_rib6_node *node, uint8_t *depth)
{
	if ((node == NULL) || (depth == NULL)) {
		rte_errno = EINVAL;
		return -1;
	}
	*depth = node->depth;
	return 0;
}

void *
rte_rib6_get_ext(struct rte_rib6_node *node)
{
	return (node == NULL) ? NULL : &node->ext[0];
}

int
rte_rib6_get_nh(const struct rte_rib6_node *node, uint64_t *nh)
{
	if ((node == NULL) || (nh == NULL)) {
		rte_errno = EINVAL;
		return -1;
	}
	*nh = node->nh;
	return 0;
}

int
rte_rib6_set_nh(struct rte_rib6_node *node, uint64_t nh)
{
	if (node == NULL) {
		rte_errno = EINVAL;
		return -1;
	}
	node->nh = nh;
	return 0;
}

struct rte_rib6 *
rte_rib6_create(const char *name, int socket_id,
		const struct rte_rib6_conf *conf)
{
	char mem_name[RTE_RIB6_NAMESIZE];
	struct rte_rib6 *rib = NULL;
	struct rte_tailq_entry *te;
	struct rte_rib6_list *rib6_list;
	struct rte_mempool *node_pool;

	/* Check user arguments. */
	if (name == NULL || conf == NULL || conf->max_nodes <= 0) {
		rte_errno = EINVAL;
		return NULL;
	}

	snprintf(mem_name, sizeof(mem_name), "MP_%s", name);
	node_pool = rte_mempool_create(mem_name, conf->max_nodes,
		sizeof(struct rte_rib6_node) + conf->ext_sz, 0, 0,
		NULL, NULL, NULL, NULL, socket_id, 0);

	if (node_pool == NULL) {
		RTE_LOG(ERR, LPM,
			"Can not allocate mempool for RIB6 %s\n", name);
		return NULL;
	}

	snprintf(mem_name, sizeof(mem_name), "RIB6_%s", name);
	rib6_list = RTE_TAILQ_CAST(rte_rib6_tailq.head, rte_rib6_list);

	rte_mcfg_tailq_write_lock();

	/* guarantee there's no existing */
	TAILQ_FOREACH(te, rib6_list, next) {
		rib = (struct rte_rib6 *)te->data;
		if (strncmp(name, rib->name, RTE_RIB6_NAMESIZE) == 0)
			break;
	}
	rib = NULL;
	if (te != NULL) {
		rte_errno = EEXIST;
		goto exit;
	}

	/* allocate tailq entry */
	te = rte_zmalloc("RIB6_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		RTE_LOG(ERR, LPM,
			"Can not allocate tailq entry for RIB6 %s\n", name);
		rte_errno = ENOMEM;
		goto exit;
	}

	/* Allocate memory to store the RIB6 data structures. */
	rib = rte_zmalloc_socket(mem_name,
		sizeof(struct rte_rib6), RTE_CACHE_LINE_SIZE, socket_id);
	if (rib == NULL) {
		RTE_LOG(ERR, LPM, "RIB6 %s memory allocation failed\n", name);
		rte_errno = ENOMEM;
		goto free_te;
	}

	rte_strlcpy(rib->name, name, sizeof(rib->name));
	rib->tree = NULL;
	rib->max_nodes = conf->max_nodes;
	rib->node_pool = node_pool;

	te->data = (void *)rib;
	TAILQ_INSERT_TAIL(rib6_list, te, next);

	rte_mcfg_tailq_write_unlock();

	return rib;

free_te:
	rte_free(te);
exit:
	rte_mcfg_tailq_write_unlock();
	rte_mempool_free(node_pool);

	return NULL;
}

struct rte_rib6 *
rte_rib6_find_existing(const char *name)
{
	struct rte_rib6 *rib = NULL;
	struct rte_tailq_entry *te;
	struct rte_rib6_list *rib6_list;

	if (unlikely(name == NULL)) {
		rte_errno = EINVAL;
		return NULL;
	}

	rib6_list = RTE_TAILQ_CAST(rte_rib6_tailq.head, rte_rib6_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, rib6_list, next) {
		rib = (struct rte_rib6 *) te->data;
		if (strncmp(name, rib->name, RTE_RIB6_NAMESIZE) == 0)
			break;
	}
	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return rib;
}

void
rte_rib6_free(struct rte_rib6 *rib)
{
	struct rte_tailq_entry *te;
	struct rte_rib6_list *rib6_list;
	struct rte_rib6_node *tmp = NULL;

	if (unlikely(rib == NULL)) {
		rte_errno = EINVAL;
		return;
	}

	rib6_list = RTE_TAILQ_CAST(rte_rib6_tailq.head, rte_rib6_list);

	rte_mcfg_tailq_write_lock();

	/* find our tailq entry */
	TAILQ_FOREACH(te, rib6_list, next) {
		if (te->data == (void *)rib)
			break;
	}
	if (te != NULL)
		TAILQ_REMOVE(rib6_list, te, next);

	rte_mcfg_tailq_write_unlock();

	while ((tmp = rte_rib6_get_nxt(rib, 0, 0, tmp,
			RTE_RIB6_GET_NXT_ALL)) != NULL)
		rte_rib6_remove(rib, tmp->ip, tmp->depth);

	rte_mempool_free(rib->node_pool);

	rte_free(rib);
	rte_free(te);
}
