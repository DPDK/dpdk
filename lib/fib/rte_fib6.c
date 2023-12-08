/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdint.h>
#include <string.h>
#include <sys/queue.h>

#include <rte_eal_memconfig.h>
#include <rte_tailq.h>
#include <rte_errno.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>

#include <rte_rib6.h>
#include <rte_fib6.h>

#include "trie.h"
#include "fib_log.h"

TAILQ_HEAD(rte_fib6_list, rte_tailq_entry);
static struct rte_tailq_elem rte_fib6_tailq = {
	.name = "RTE_FIB6",
};
EAL_REGISTER_TAILQ(rte_fib6_tailq)

/* Maximum length of a FIB name. */
#define FIB6_NAMESIZE	64

#if defined(RTE_LIBRTE_FIB_DEBUG)
#define FIB6_RETURN_IF_TRUE(cond, retval) do {		\
	if (cond)					\
		return retval;				\
} while (0)
#else
#define FIB6_RETURN_IF_TRUE(cond, retval)
#endif

struct rte_fib6 {
	char			name[FIB6_NAMESIZE];
	enum rte_fib6_type	type;	/**< Type of FIB struct */
	struct rte_rib6		*rib;	/**< RIB helper datastructure */
	void			*dp;	/**< pointer to the dataplane struct*/
	rte_fib6_lookup_fn_t	lookup;	/**< FIB lookup function */
	rte_fib6_modify_fn_t	modify; /**< modify FIB datastructure */
	uint64_t		def_nh;
};

static void
dummy_lookup(void *fib_p, uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n)
{
	unsigned int i;
	struct rte_fib6 *fib = fib_p;
	struct rte_rib6_node *node;

	for (i = 0; i < n; i++) {
		node = rte_rib6_lookup(fib->rib, ips[i]);
		if (node != NULL)
			rte_rib6_get_nh(node, &next_hops[i]);
		else
			next_hops[i] = fib->def_nh;
	}
}

static int
dummy_modify(struct rte_fib6 *fib, const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t depth, uint64_t next_hop, int op)
{
	struct rte_rib6_node *node;
	if ((fib == NULL) || (depth > RTE_FIB6_MAXDEPTH))
		return -EINVAL;

	node = rte_rib6_lookup_exact(fib->rib, ip, depth);

	switch (op) {
	case RTE_FIB6_ADD:
		if (node == NULL)
			node = rte_rib6_insert(fib->rib, ip, depth);
		if (node == NULL)
			return -rte_errno;
		return rte_rib6_set_nh(node, next_hop);
	case RTE_FIB6_DEL:
		if (node == NULL)
			return -ENOENT;
		rte_rib6_remove(fib->rib, ip, depth);
		return 0;
	}
	return -EINVAL;
}

static int
init_dataplane(struct rte_fib6 *fib, __rte_unused int socket_id,
	struct rte_fib6_conf *conf)
{
	char dp_name[sizeof(void *)];

	snprintf(dp_name, sizeof(dp_name), "%p", fib);
	switch (conf->type) {
	case RTE_FIB6_DUMMY:
		fib->dp = fib;
		fib->lookup = dummy_lookup;
		fib->modify = dummy_modify;
		return 0;
	case RTE_FIB6_TRIE:
		fib->dp = trie_create(dp_name, socket_id, conf);
		if (fib->dp == NULL)
			return -rte_errno;
		fib->lookup = trie_get_lookup_fn(fib->dp, RTE_FIB6_LOOKUP_DEFAULT);
		fib->modify = trie_modify;
		return 0;
	default:
		return -EINVAL;
	}
	return 0;
}

int
rte_fib6_add(struct rte_fib6 *fib, const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t depth, uint64_t next_hop)
{
	if ((fib == NULL) || (ip == NULL) || (fib->modify == NULL) ||
			(depth > RTE_FIB6_MAXDEPTH))
		return -EINVAL;
	return fib->modify(fib, ip, depth, next_hop, RTE_FIB6_ADD);
}

int
rte_fib6_delete(struct rte_fib6 *fib, const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t depth)
{
	if ((fib == NULL) || (ip == NULL) || (fib->modify == NULL) ||
			(depth > RTE_FIB6_MAXDEPTH))
		return -EINVAL;
	return fib->modify(fib, ip, depth, 0, RTE_FIB6_DEL);
}

int
rte_fib6_lookup_bulk(struct rte_fib6 *fib,
	uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, int n)
{
	FIB6_RETURN_IF_TRUE((fib == NULL) || (ips == NULL) ||
		(next_hops == NULL) || (fib->lookup == NULL), -EINVAL);
	fib->lookup(fib->dp, ips, next_hops, n);
	return 0;
}

struct rte_fib6 *
rte_fib6_create(const char *name, int socket_id, struct rte_fib6_conf *conf)
{
	char mem_name[FIB6_NAMESIZE];
	int ret;
	struct rte_fib6 *fib = NULL;
	struct rte_rib6 *rib = NULL;
	struct rte_tailq_entry *te;
	struct rte_fib6_list *fib_list;
	struct rte_rib6_conf rib_conf;

	/* Check user arguments. */
	if ((name == NULL) || (conf == NULL) || (conf->max_routes < 0) ||
			(conf->type > RTE_FIB6_TRIE)) {
		rte_errno = EINVAL;
		return NULL;
	}

	rib_conf.ext_sz = conf->rib_ext_sz;
	rib_conf.max_nodes = conf->max_routes * 2;

	rib = rte_rib6_create(name, socket_id, &rib_conf);
	if (rib == NULL) {
		FIB_LOG(ERR,
			"Can not allocate RIB %s", name);
		return NULL;
	}

	snprintf(mem_name, sizeof(mem_name), "FIB6_%s", name);
	fib_list = RTE_TAILQ_CAST(rte_fib6_tailq.head, rte_fib6_list);

	rte_mcfg_tailq_write_lock();

	/* guarantee there's no existing */
	TAILQ_FOREACH(te, fib_list, next) {
		fib = (struct rte_fib6 *)te->data;
		if (strncmp(name, fib->name, FIB6_NAMESIZE) == 0)
			break;
	}
	fib = NULL;
	if (te != NULL) {
		rte_errno = EEXIST;
		goto exit;
	}

	/* allocate tailq entry */
	te = rte_zmalloc("FIB_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		FIB_LOG(ERR,
			"Can not allocate tailq entry for FIB %s", name);
		rte_errno = ENOMEM;
		goto exit;
	}

	/* Allocate memory to store the FIB data structures. */
	fib = rte_zmalloc_socket(mem_name,
		sizeof(struct rte_fib6), RTE_CACHE_LINE_SIZE, socket_id);
	if (fib == NULL) {
		FIB_LOG(ERR, "FIB %s memory allocation failed", name);
		rte_errno = ENOMEM;
		goto free_te;
	}

	rte_strlcpy(fib->name, name, sizeof(fib->name));
	fib->rib = rib;
	fib->type = conf->type;
	fib->def_nh = conf->default_nh;
	ret = init_dataplane(fib, socket_id, conf);
	if (ret < 0) {
		FIB_LOG(ERR,
			"FIB dataplane struct %s memory allocation failed",
			name);
		rte_errno = -ret;
		goto free_fib;
	}

	te->data = (void *)fib;
	TAILQ_INSERT_TAIL(fib_list, te, next);

	rte_mcfg_tailq_write_unlock();

	return fib;

free_fib:
	rte_free(fib);
free_te:
	rte_free(te);
exit:
	rte_mcfg_tailq_write_unlock();
	rte_rib6_free(rib);

	return NULL;
}

struct rte_fib6 *
rte_fib6_find_existing(const char *name)
{
	struct rte_fib6 *fib = NULL;
	struct rte_tailq_entry *te;
	struct rte_fib6_list *fib_list;

	fib_list = RTE_TAILQ_CAST(rte_fib6_tailq.head, rte_fib6_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, fib_list, next) {
		fib = (struct rte_fib6 *) te->data;
		if (strncmp(name, fib->name, FIB6_NAMESIZE) == 0)
			break;
	}
	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return fib;
}

static void
free_dataplane(struct rte_fib6 *fib)
{
	switch (fib->type) {
	case RTE_FIB6_DUMMY:
		return;
	case RTE_FIB6_TRIE:
		trie_free(fib->dp);
	default:
		return;
	}
}

void
rte_fib6_free(struct rte_fib6 *fib)
{
	struct rte_tailq_entry *te;
	struct rte_fib6_list *fib_list;

	if (fib == NULL)
		return;

	fib_list = RTE_TAILQ_CAST(rte_fib6_tailq.head, rte_fib6_list);

	rte_mcfg_tailq_write_lock();

	/* find our tailq entry */
	TAILQ_FOREACH(te, fib_list, next) {
		if (te->data == (void *)fib)
			break;
	}
	if (te != NULL)
		TAILQ_REMOVE(fib_list, te, next);

	rte_mcfg_tailq_write_unlock();

	free_dataplane(fib);
	rte_rib6_free(fib->rib);
	rte_free(fib);
	rte_free(te);
}

void *
rte_fib6_get_dp(struct rte_fib6 *fib)
{
	return (fib == NULL) ? NULL : fib->dp;
}

struct rte_rib6 *
rte_fib6_get_rib(struct rte_fib6 *fib)
{
	return (fib == NULL) ? NULL : fib->rib;
}

int
rte_fib6_select_lookup(struct rte_fib6 *fib,
	enum rte_fib6_lookup_type type)
{
	rte_fib6_lookup_fn_t fn;

	switch (fib->type) {
	case RTE_FIB6_TRIE:
		fn = trie_get_lookup_fn(fib->dp, type);
		if (fn == NULL)
			return -EINVAL;
		fib->lookup = fn;
		return 0;
	default:
		return -EINVAL;
	}
}
