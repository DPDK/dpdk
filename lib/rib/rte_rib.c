/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdbool.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_mempool.h>
#include <rte_string_fns.h>
#include <rte_tailq.h>

#include <rte_rib.h>

#include "rib_log.h"

RTE_LOG_REGISTER_DEFAULT(rib_logtype, INFO);

TAILQ_HEAD(rte_rib_list, rte_tailq_entry);
static struct rte_tailq_elem rte_rib_tailq = {
	.name = "RTE_RIB",
};
EAL_REGISTER_TAILQ(rte_rib_tailq)

#define RTE_RIB_VALID_NODE	1
/* Maximum depth value possible for IPv4 RIB. */
#define RIB_MAXDEPTH		32
/* Maximum length of a RIB name. */
#define RTE_RIB_NAMESIZE	64

struct rte_rib_node {
	struct rte_rib_node	*left;
	struct rte_rib_node	*right;
	struct rte_rib_node	*parent;
	uint32_t	ip;
	uint8_t		depth;
	uint8_t		flag;
	uint64_t	nh;
	__extension__ uint64_t ext[];
};

struct rte_rib {
	char		name[RTE_RIB_NAMESIZE];
	struct rte_rib_node	*tree;
	struct rte_mempool	*node_pool;
	uint32_t		cur_nodes;
	uint32_t		cur_routes;
	uint32_t		max_nodes;
};

static inline bool
is_valid_node(const struct rte_rib_node *node)
{
	return (node->flag & RTE_RIB_VALID_NODE) == RTE_RIB_VALID_NODE;
}

static inline bool
is_right_node(const struct rte_rib_node *node)
{
	return node->parent->right == node;
}

/*
 * Check if ip1 is covered by ip2/depth prefix
 */
static inline bool
is_covered(uint32_t ip1, uint32_t ip2, uint8_t depth)
{
	return ((ip1 ^ ip2) & rte_rib_depth_to_mask(depth)) == 0;
}

static inline struct rte_rib_node *
get_nxt_node(struct rte_rib_node *node, uint32_t ip)
{
	if (node->depth == RIB_MAXDEPTH)
		return NULL;
	return (ip & (1 << (31 - node->depth))) ? node->right : node->left;
}

static struct rte_rib_node *
node_alloc(struct rte_rib *rib)
{
	struct rte_rib_node *ent;
	int ret;

	ret = rte_mempool_get(rib->node_pool, (void *)&ent);
	if (unlikely(ret != 0))
		return NULL;
	++rib->cur_nodes;
	return ent;
}

static void
node_free(struct rte_rib *rib, struct rte_rib_node *ent)
{
	--rib->cur_nodes;
	rte_mempool_put(rib->node_pool, ent);
}

struct rte_rib_node *
rte_rib_lookup(struct rte_rib *rib, uint32_t ip)
{
	struct rte_rib_node *cur, *prev = NULL;

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

struct rte_rib_node *
rte_rib_lookup_parent(struct rte_rib_node *ent)
{
	struct rte_rib_node *tmp;

	if (ent == NULL)
		return NULL;
	tmp = ent->parent;
	while ((tmp != NULL) &&	!is_valid_node(tmp))
		tmp = tmp->parent;
	return tmp;
}

static struct rte_rib_node *
__rib_lookup_exact(struct rte_rib *rib, uint32_t ip, uint8_t depth)
{
	struct rte_rib_node *cur;

	cur = rib->tree;
	while (cur != NULL) {
		if ((cur->ip == ip) && (cur->depth == depth) &&
				is_valid_node(cur))
			return cur;
		if ((cur->depth > depth) ||
				!is_covered(ip, cur->ip, cur->depth))
			break;
		cur = get_nxt_node(cur, ip);
	}
	return NULL;
}

struct rte_rib_node *
rte_rib_lookup_exact(struct rte_rib *rib, uint32_t ip, uint8_t depth)
{
	if (unlikely(rib == NULL || depth > RIB_MAXDEPTH)) {
		rte_errno = EINVAL;
		return NULL;
	}
	ip &= rte_rib_depth_to_mask(depth);

	return __rib_lookup_exact(rib, ip, depth);
}

/*
 *  Traverses on subtree and retrieves more specific routes
 *  for a given in args ip/depth prefix
 *  last = NULL means the first invocation
 */
struct rte_rib_node *
rte_rib_get_nxt(struct rte_rib *rib, uint32_t ip,
	uint8_t depth, struct rte_rib_node *last, int flag)
{
	struct rte_rib_node *tmp, *prev = NULL;

	if (unlikely(rib == NULL || depth > RIB_MAXDEPTH)) {
		rte_errno = EINVAL;
		return NULL;
	}

	if (last == NULL) {
		tmp = rib->tree;
		while ((tmp) && (tmp->depth < depth))
			tmp = get_nxt_node(tmp, ip);
	} else {
		tmp = last;
		while ((tmp->parent != NULL) && (is_right_node(tmp) ||
				(tmp->parent->right == NULL))) {
			tmp = tmp->parent;
			if (is_valid_node(tmp) &&
					(is_covered(tmp->ip, ip, depth) &&
					(tmp->depth > depth)))
				return tmp;
		}
		tmp = (tmp->parent) ? tmp->parent->right : NULL;
	}
	while (tmp) {
		if (is_valid_node(tmp) &&
				(is_covered(tmp->ip, ip, depth) &&
				(tmp->depth > depth))) {
			prev = tmp;
			if (flag == RTE_RIB_GET_NXT_COVER)
				return prev;
		}
		tmp = (tmp->left) ? tmp->left : tmp->right;
	}
	return prev;
}

void
rte_rib_remove(struct rte_rib *rib, uint32_t ip, uint8_t depth)
{
	struct rte_rib_node *cur, *prev, *child;

	cur = rte_rib_lookup_exact(rib, ip, depth);
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

struct rte_rib_node *
rte_rib_insert(struct rte_rib *rib, uint32_t ip, uint8_t depth)
{
	struct rte_rib_node **tmp;
	struct rte_rib_node *prev = NULL;
	struct rte_rib_node *new_node = NULL;
	struct rte_rib_node *common_node = NULL;
	int d = 0;
	uint32_t common_prefix;
	uint8_t common_depth;

	if (unlikely(rib == NULL || depth > RIB_MAXDEPTH)) {
		rte_errno = EINVAL;
		return NULL;
	}

	tmp = &rib->tree;
	ip &= rte_rib_depth_to_mask(depth);
	new_node = __rib_lookup_exact(rib, ip, depth);
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
	new_node->ip = ip;
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
		 * Previous rte_rib_lookup_exact() returned NULL
		 * but node with proper search criteria is found.
		 * Validate intermediate node and return.
		 */
		if ((ip == (*tmp)->ip) && (depth == (*tmp)->depth)) {
			node_free(rib, new_node);
			(*tmp)->flag |= RTE_RIB_VALID_NODE;
			++rib->cur_routes;
			return *tmp;
		}
		d = (*tmp)->depth;
		if ((d >= depth) || !is_covered(ip, (*tmp)->ip, d))
			break;
		prev = *tmp;
		tmp = (ip & (1 << (31 - d))) ? &(*tmp)->right : &(*tmp)->left;
	}
	/* closest node found, new_node should be inserted in the middle */
	common_depth = RTE_MIN(depth, (*tmp)->depth);
	common_prefix = ip ^ (*tmp)->ip;
	d = (common_prefix == 0) ? 32 : rte_clz32(common_prefix);

	common_depth = RTE_MIN(d, common_depth);
	common_prefix = ip & rte_rib_depth_to_mask(common_depth);
	if ((common_prefix == ip) && (common_depth == depth)) {
		/* insert as a parent */
		if ((*tmp)->ip & (1 << (31 - depth)))
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
		common_node->ip = common_prefix;
		common_node->depth = common_depth;
		common_node->flag = 0;
		common_node->parent = (*tmp)->parent;
		new_node->parent = common_node;
		(*tmp)->parent = common_node;
		if ((new_node->ip & (1 << (31 - common_depth))) == 0) {
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
rte_rib_get_ip(const struct rte_rib_node *node, uint32_t *ip)
{
	if (unlikely(node == NULL || ip == NULL)) {
		rte_errno = EINVAL;
		return -1;
	}
	*ip = node->ip;
	return 0;
}

int
rte_rib_get_depth(const struct rte_rib_node *node, uint8_t *depth)
{
	if (unlikely(node == NULL || depth == NULL)) {
		rte_errno = EINVAL;
		return -1;
	}
	*depth = node->depth;
	return 0;
}

void *
rte_rib_get_ext(struct rte_rib_node *node)
{
	return (node == NULL) ? NULL : &node->ext[0];
}

int
rte_rib_get_nh(const struct rte_rib_node *node, uint64_t *nh)
{
	if (unlikely(node == NULL || nh == NULL)) {
		rte_errno = EINVAL;
		return -1;
	}
	*nh = node->nh;
	return 0;
}

int
rte_rib_set_nh(struct rte_rib_node *node, uint64_t nh)
{
	if (unlikely(node == NULL)) {
		rte_errno = EINVAL;
		return -1;
	}
	node->nh = nh;
	return 0;
}

struct rte_rib *
rte_rib_create(const char *name, int socket_id, const struct rte_rib_conf *conf)
{
	char mem_name[RTE_RIB_NAMESIZE];
	struct rte_rib *rib = NULL;
	struct rte_tailq_entry *te;
	struct rte_rib_list *rib_list;
	struct rte_mempool *node_pool;

	/* Check user arguments. */
	if (unlikely(name == NULL || conf == NULL || conf->max_nodes <= 0)) {
		rte_errno = EINVAL;
		return NULL;
	}

	snprintf(mem_name, sizeof(mem_name), "MP_%s", name);
	node_pool = rte_mempool_create(mem_name, conf->max_nodes,
		sizeof(struct rte_rib_node) + conf->ext_sz, 0, 0,
		NULL, NULL, NULL, NULL, socket_id, 0);

	if (node_pool == NULL) {
		RIB_LOG(ERR,
			"Can not allocate mempool for RIB %s", name);
		return NULL;
	}

	snprintf(mem_name, sizeof(mem_name), "RIB_%s", name);
	rib_list = RTE_TAILQ_CAST(rte_rib_tailq.head, rte_rib_list);

	rte_mcfg_tailq_write_lock();

	/* guarantee there's no existing */
	TAILQ_FOREACH(te, rib_list, next) {
		rib = (struct rte_rib *)te->data;
		if (strncmp(name, rib->name, RTE_RIB_NAMESIZE) == 0)
			break;
	}
	rib = NULL;
	if (te != NULL) {
		rte_errno = EEXIST;
		goto exit;
	}

	/* allocate tailq entry */
	te = rte_zmalloc("RIB_TAILQ_ENTRY", sizeof(*te), 0);
	if (unlikely(te == NULL)) {
		RIB_LOG(ERR,
			"Can not allocate tailq entry for RIB %s", name);
		rte_errno = ENOMEM;
		goto exit;
	}

	/* Allocate memory to store the RIB data structures. */
	rib = rte_zmalloc_socket(mem_name,
		sizeof(struct rte_rib),	RTE_CACHE_LINE_SIZE, socket_id);
	if (unlikely(rib == NULL)) {
		RIB_LOG(ERR, "RIB %s memory allocation failed", name);
		rte_errno = ENOMEM;
		goto free_te;
	}

	rte_strlcpy(rib->name, name, sizeof(rib->name));
	rib->tree = NULL;
	rib->max_nodes = conf->max_nodes;
	rib->node_pool = node_pool;
	te->data = (void *)rib;
	TAILQ_INSERT_TAIL(rib_list, te, next);

	rte_mcfg_tailq_write_unlock();

	return rib;

free_te:
	rte_free(te);
exit:
	rte_mcfg_tailq_write_unlock();
	rte_mempool_free(node_pool);

	return NULL;
}

struct rte_rib *
rte_rib_find_existing(const char *name)
{
	struct rte_rib *rib = NULL;
	struct rte_tailq_entry *te;
	struct rte_rib_list *rib_list;

	rib_list = RTE_TAILQ_CAST(rte_rib_tailq.head, rte_rib_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, rib_list, next) {
		rib = (struct rte_rib *) te->data;
		if (strncmp(name, rib->name, RTE_RIB_NAMESIZE) == 0)
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
rte_rib_free(struct rte_rib *rib)
{
	struct rte_tailq_entry *te;
	struct rte_rib_list *rib_list;
	struct rte_rib_node *tmp = NULL;

	if (rib == NULL)
		return;

	rib_list = RTE_TAILQ_CAST(rte_rib_tailq.head, rte_rib_list);

	rte_mcfg_tailq_write_lock();

	/* find our tailq entry */
	TAILQ_FOREACH(te, rib_list, next) {
		if (te->data == (void *)rib)
			break;
	}
	if (te != NULL)
		TAILQ_REMOVE(rib_list, te, next);

	rte_mcfg_tailq_write_unlock();

	while ((tmp = rte_rib_get_nxt(rib, 0, 0, tmp,
			RTE_RIB_GET_NXT_ALL)) != NULL)
		rte_rib_remove(rib, tmp->ip, tmp->depth);

	rte_mempool_free(rib->node_pool);
	rte_free(rib);
	rte_free(te);
}
