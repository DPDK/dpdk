/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_hash.h>
#include <rte_jhash.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_rwlock.h>
#include <rte_tailq.h>

#include "rte_ipsec_sad.h"

/*
 * Rules are stored in three hash tables depending on key_type.
 * Each rule will also be stored in SPI_ONLY table.
 * for each data entry within this table last two bits are reserved to
 * indicate presence of entries with the same SPI in DIP and DIP+SIP tables.
 */

#define IPSEC_SAD_NAMESIZE	64
#define SAD_PREFIX		"SAD_"
/* "SAD_<name>" */
#define SAD_FORMAT		SAD_PREFIX "%s"

#define DEFAULT_HASH_FUNC	rte_jhash
#define MIN_HASH_ENTRIES	8U /* From rte_cuckoo_hash.h */

struct hash_cnt {
	uint32_t cnt_dip;
	uint32_t cnt_dip_sip;
};

struct rte_ipsec_sad {
	char name[IPSEC_SAD_NAMESIZE];
	struct rte_hash	*hash[RTE_IPSEC_SAD_KEY_TYPE_MASK];
	/* Array to track number of more specific rules
	 * (spi_dip or spi_dip_sip). Used only in add/delete
	 * as a helper struct.
	 */
	__extension__ struct hash_cnt cnt_arr[];
};

TAILQ_HEAD(rte_ipsec_sad_list, rte_tailq_entry);
static struct rte_tailq_elem rte_ipsec_sad_tailq = {
	.name = "RTE_IPSEC_SAD",
};
EAL_REGISTER_TAILQ(rte_ipsec_sad_tailq)

int
rte_ipsec_sad_add(__rte_unused struct rte_ipsec_sad *sad,
		__rte_unused const union rte_ipsec_sad_key *key,
		__rte_unused int key_type, __rte_unused void *sa)
{
	return -ENOTSUP;
}

int
rte_ipsec_sad_del(__rte_unused struct rte_ipsec_sad *sad,
		__rte_unused const union rte_ipsec_sad_key *key,
		__rte_unused int key_type)
{
	return -ENOTSUP;
}

struct rte_ipsec_sad *
rte_ipsec_sad_create(const char *name, const struct rte_ipsec_sad_conf *conf)
{
	char hash_name[RTE_HASH_NAMESIZE];
	char sad_name[IPSEC_SAD_NAMESIZE];
	struct rte_tailq_entry *te;
	struct rte_ipsec_sad_list *sad_list;
	struct rte_ipsec_sad *sad, *tmp_sad = NULL;
	struct rte_hash_parameters hash_params = {0};
	int ret;
	uint32_t sa_sum;

	RTE_BUILD_BUG_ON(RTE_IPSEC_SAD_KEY_TYPE_MASK != 3);

	if ((name == NULL) || (conf == NULL) ||
			((conf->max_sa[RTE_IPSEC_SAD_SPI_ONLY] == 0) &&
			(conf->max_sa[RTE_IPSEC_SAD_SPI_DIP] == 0) &&
			(conf->max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] == 0))) {
		rte_errno = EINVAL;
		return NULL;
	}

	ret = snprintf(sad_name, IPSEC_SAD_NAMESIZE, SAD_FORMAT, name);
	if (ret < 0 || ret >= IPSEC_SAD_NAMESIZE) {
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	/** Init SAD*/
	sa_sum = RTE_MAX(MIN_HASH_ENTRIES,
		conf->max_sa[RTE_IPSEC_SAD_SPI_ONLY]) +
		RTE_MAX(MIN_HASH_ENTRIES,
		conf->max_sa[RTE_IPSEC_SAD_SPI_DIP]) +
		RTE_MAX(MIN_HASH_ENTRIES,
		conf->max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP]);
	sad = rte_zmalloc_socket(NULL, sizeof(*sad) +
		(sizeof(struct hash_cnt) * sa_sum),
		RTE_CACHE_LINE_SIZE, conf->socket_id);
	if (sad == NULL) {
		rte_errno = ENOMEM;
		return NULL;
	}
	memcpy(sad->name, sad_name, sizeof(sad_name));

	hash_params.hash_func = DEFAULT_HASH_FUNC;
	hash_params.hash_func_init_val = rte_rand();
	hash_params.socket_id = conf->socket_id;
	hash_params.name = hash_name;
	if (conf->flags & RTE_IPSEC_SAD_FLAG_RW_CONCURRENCY)
		hash_params.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY;

	/** Init hash[RTE_IPSEC_SAD_SPI_ONLY] for SPI only */
	snprintf(hash_name, sizeof(hash_name), "sad_1_%p", sad);
	hash_params.key_len = sizeof(((struct rte_ipsec_sadv4_key *)0)->spi);
	hash_params.entries = sa_sum;
	sad->hash[RTE_IPSEC_SAD_SPI_ONLY] = rte_hash_create(&hash_params);
	if (sad->hash[RTE_IPSEC_SAD_SPI_ONLY] == NULL) {
		rte_ipsec_sad_destroy(sad);
		return NULL;
	}

	/** Init hash[RTE_IPSEC_SAD_SPI_DIP] for SPI + DIP */
	snprintf(hash_name, sizeof(hash_name), "sad_2_%p", sad);
	if (conf->flags & RTE_IPSEC_SAD_FLAG_IPV6)
		hash_params.key_len +=
			sizeof(((struct rte_ipsec_sadv6_key *)0)->dip);
	else
		hash_params.key_len +=
			sizeof(((struct rte_ipsec_sadv4_key *)0)->dip);
	hash_params.entries = RTE_MAX(MIN_HASH_ENTRIES,
			conf->max_sa[RTE_IPSEC_SAD_SPI_DIP]);
	sad->hash[RTE_IPSEC_SAD_SPI_DIP] = rte_hash_create(&hash_params);
	if (sad->hash[RTE_IPSEC_SAD_SPI_DIP] == NULL) {
		rte_ipsec_sad_destroy(sad);
		return NULL;
	}

	/** Init hash[[RTE_IPSEC_SAD_SPI_DIP_SIP] for SPI + DIP + SIP */
	snprintf(hash_name, sizeof(hash_name), "sad_3_%p", sad);
	if (conf->flags & RTE_IPSEC_SAD_FLAG_IPV6)
		hash_params.key_len +=
			sizeof(((struct rte_ipsec_sadv6_key *)0)->sip);
	else
		hash_params.key_len +=
			sizeof(((struct rte_ipsec_sadv4_key *)0)->sip);
	hash_params.entries = RTE_MAX(MIN_HASH_ENTRIES,
			conf->max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP]);
	sad->hash[RTE_IPSEC_SAD_SPI_DIP_SIP] = rte_hash_create(&hash_params);
	if (sad->hash[RTE_IPSEC_SAD_SPI_DIP_SIP] == NULL) {
		rte_ipsec_sad_destroy(sad);
		return NULL;
	}

	sad_list = RTE_TAILQ_CAST(rte_ipsec_sad_tailq.head,
			rte_ipsec_sad_list);
	rte_mcfg_tailq_write_lock();
	/* guarantee there's no existing */
	TAILQ_FOREACH(te, sad_list, next) {
		tmp_sad = (struct rte_ipsec_sad *)te->data;
		if (strncmp(sad_name, tmp_sad->name, IPSEC_SAD_NAMESIZE) == 0)
			break;
	}
	if (te != NULL) {
		rte_mcfg_tailq_write_unlock();
		rte_errno = EEXIST;
		rte_ipsec_sad_destroy(sad);
		return NULL;
	}

	/* allocate tailq entry */
	te = rte_zmalloc("IPSEC_SAD_TAILQ_ENTRY", sizeof(*te), 0);
	if (te == NULL) {
		rte_mcfg_tailq_write_unlock();
		rte_errno = ENOMEM;
		rte_ipsec_sad_destroy(sad);
		return NULL;
	}

	te->data = (void *)sad;
	TAILQ_INSERT_TAIL(sad_list, te, next);
	rte_mcfg_tailq_write_unlock();
	return sad;
}

struct rte_ipsec_sad *
rte_ipsec_sad_find_existing(const char *name)
{
	char sad_name[IPSEC_SAD_NAMESIZE];
	struct rte_ipsec_sad *sad = NULL;
	struct rte_tailq_entry *te;
	struct rte_ipsec_sad_list *sad_list;
	int ret;

	ret = snprintf(sad_name, IPSEC_SAD_NAMESIZE, SAD_FORMAT, name);
	if (ret < 0 || ret >= IPSEC_SAD_NAMESIZE) {
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	sad_list = RTE_TAILQ_CAST(rte_ipsec_sad_tailq.head,
		rte_ipsec_sad_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, sad_list, next) {
		sad = (struct rte_ipsec_sad *) te->data;
		if (strncmp(sad_name, sad->name, IPSEC_SAD_NAMESIZE) == 0)
			break;
	}
	rte_mcfg_tailq_read_unlock();

	if (te == NULL) {
		rte_errno = ENOENT;
		return NULL;
	}

	return sad;
}

void
rte_ipsec_sad_destroy(struct rte_ipsec_sad *sad)
{
	struct rte_tailq_entry *te;
	struct rte_ipsec_sad_list *sad_list;

	if (sad == NULL)
		return;

	sad_list = RTE_TAILQ_CAST(rte_ipsec_sad_tailq.head,
			rte_ipsec_sad_list);
	rte_mcfg_tailq_write_lock();
	TAILQ_FOREACH(te, sad_list, next) {
		if (te->data == (void *)sad)
			break;
	}
	if (te != NULL)
		TAILQ_REMOVE(sad_list, te, next);

	rte_mcfg_tailq_write_unlock();

	rte_hash_free(sad->hash[RTE_IPSEC_SAD_SPI_ONLY]);
	rte_hash_free(sad->hash[RTE_IPSEC_SAD_SPI_DIP]);
	rte_hash_free(sad->hash[RTE_IPSEC_SAD_SPI_DIP_SIP]);
	rte_free(sad);
	if (te != NULL)
		rte_free(te);
}

int
rte_ipsec_sad_lookup(__rte_unused const struct rte_ipsec_sad *sad,
		__rte_unused const union rte_ipsec_sad_key *keys[],
		__rte_unused void *sa[], __rte_unused uint32_t n)
{
	return -ENOTSUP;
}
