/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <string.h>

#include <rte_eal_memconfig.h>
#include <rte_errno.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include <rte_malloc.h>
#include <rte_random.h>
#include <rte_tailq.h>

#include "rte_ipsec_sad.h"

/*
 * Rules are stored in three hash tables depending on key_type.
 * Each rule will also be stored in SPI_ONLY table.
 * for each data entry within this table last two bits are reserved to
 * indicate presence of entries with the same SPI in DIP and DIP+SIP tables.
 */

#define SAD_PREFIX		"SAD_"
/* "SAD_<name>" */
#define SAD_FORMAT		SAD_PREFIX "%s"

#define DEFAULT_HASH_FUNC	rte_hash_crc
#define MIN_HASH_ENTRIES	8U /* From rte_cuckoo_hash.h */

struct hash_cnt {
	uint32_t cnt_dip;
	uint32_t cnt_dip_sip;
};

struct rte_ipsec_sad {
	char name[RTE_IPSEC_SAD_NAMESIZE];
	struct rte_hash	*hash[RTE_IPSEC_SAD_KEY_TYPE_MASK];
	uint32_t keysize[RTE_IPSEC_SAD_KEY_TYPE_MASK];
	uint32_t init_val;
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

#define SET_BIT(ptr, bit)	(void *)((uintptr_t)(ptr) | (uintptr_t)(bit))
#define CLEAR_BIT(ptr, bit)	(void *)((uintptr_t)(ptr) & ~(uintptr_t)(bit))
#define GET_BIT(ptr, bit)	(void *)((uintptr_t)(ptr) & (uintptr_t)(bit))

/*
 * @internal helper function
 * Add a rule of type SPI_DIP or SPI_DIP_SIP.
 * Inserts a rule into an appropriate hash table,
 * updates the value for a given SPI in SPI_ONLY hash table
 * reflecting presence of more specific rule type in two LSBs.
 * Updates a counter that reflects the number of rules with the same SPI.
 */
static inline int
add_specific(struct rte_ipsec_sad *sad, const void *key,
		int key_type, void *sa)
{
	void *tmp_val;
	int ret, notexist;

	/* Check if the key is present in the table.
	 * Need for further accaunting in cnt_arr
	 */
	ret = rte_hash_lookup_with_hash(sad->hash[key_type], key,
		rte_hash_crc(key, sad->keysize[key_type], sad->init_val));
	notexist = (ret == -ENOENT);

	/* Add an SA to the corresponding table.*/
	ret = rte_hash_add_key_with_hash_data(sad->hash[key_type], key,
		rte_hash_crc(key, sad->keysize[key_type], sad->init_val), sa);
	if (ret != 0)
		return ret;

	/* Check if there is an entry in SPI only table with the same SPI */
	ret = rte_hash_lookup_with_hash_data(sad->hash[RTE_IPSEC_SAD_SPI_ONLY],
		key, rte_hash_crc(key, sad->keysize[RTE_IPSEC_SAD_SPI_ONLY],
		sad->init_val), &tmp_val);
	if (ret < 0)
		tmp_val = NULL;
	tmp_val = SET_BIT(tmp_val, key_type);

	/* Add an entry into SPI only table */
	ret = rte_hash_add_key_with_hash_data(
		sad->hash[RTE_IPSEC_SAD_SPI_ONLY], key,
		rte_hash_crc(key, sad->keysize[RTE_IPSEC_SAD_SPI_ONLY],
		sad->init_val), tmp_val);
	if (ret != 0)
		return ret;

	/* Update a counter for a given SPI */
	ret = rte_hash_lookup_with_hash(sad->hash[RTE_IPSEC_SAD_SPI_ONLY], key,
		rte_hash_crc(key, sad->keysize[RTE_IPSEC_SAD_SPI_ONLY],
		sad->init_val));
	if (ret < 0)
		return ret;
	if (key_type == RTE_IPSEC_SAD_SPI_DIP)
		sad->cnt_arr[ret].cnt_dip += notexist;
	else
		sad->cnt_arr[ret].cnt_dip_sip += notexist;

	return 0;
}

int
rte_ipsec_sad_add(struct rte_ipsec_sad *sad,
		const union rte_ipsec_sad_key *key,
		int key_type, void *sa)
{
	void *tmp_val;
	int ret;

	if ((sad == NULL) || (key == NULL) || (sa == NULL) ||
			/* sa must be 4 byte aligned */
			(GET_BIT(sa, RTE_IPSEC_SAD_KEY_TYPE_MASK) != 0))
		return -EINVAL;

	/*
	 * Rules are stored in three hash tables depending on key_type.
	 * All rules will also have an entry in SPI_ONLY table, with entry
	 * value's two LSB's also indicating presence of rule with this SPI
	 * in other tables.
	 */
	switch (key_type) {
	case(RTE_IPSEC_SAD_SPI_ONLY):
		ret = rte_hash_lookup_with_hash_data(sad->hash[key_type],
			key, rte_hash_crc(key, sad->keysize[key_type],
			sad->init_val), &tmp_val);
		if (ret >= 0)
			tmp_val = SET_BIT(sa, GET_BIT(tmp_val,
				RTE_IPSEC_SAD_KEY_TYPE_MASK));
		else
			tmp_val = sa;
		ret = rte_hash_add_key_with_hash_data(sad->hash[key_type],
			key, rte_hash_crc(key, sad->keysize[key_type],
			sad->init_val), tmp_val);
		return ret;
	case(RTE_IPSEC_SAD_SPI_DIP):
	case(RTE_IPSEC_SAD_SPI_DIP_SIP):
		return add_specific(sad, key, key_type, sa);
	default:
		return -EINVAL;
	}
}

/*
 * @internal helper function
 * Delete a rule of type SPI_DIP or SPI_DIP_SIP.
 * Deletes an entry from an appropriate hash table and decrements
 * an entry counter for given SPI.
 * If entry to remove is the last one with given SPI within the table,
 * then it will also update related entry in SPI_ONLY table.
 * Removes an entry from SPI_ONLY hash table if there no rule left
 * for this SPI in any table.
 */
static inline int
del_specific(struct rte_ipsec_sad *sad, const void *key, int key_type)
{
	void *tmp_val;
	int ret;
	uint32_t *cnt;

	/* Remove an SA from the corresponding table.*/
	ret = rte_hash_del_key_with_hash(sad->hash[key_type], key,
		rte_hash_crc(key, sad->keysize[key_type], sad->init_val));
	if (ret < 0)
		return ret;

	/* Get an index of cnt_arr entry for a given SPI */
	ret = rte_hash_lookup_with_hash_data(sad->hash[RTE_IPSEC_SAD_SPI_ONLY],
		key, rte_hash_crc(key, sad->keysize[RTE_IPSEC_SAD_SPI_ONLY],
		sad->init_val), &tmp_val);
	if (ret < 0)
		return ret;
	cnt = (key_type == RTE_IPSEC_SAD_SPI_DIP) ?
			&sad->cnt_arr[ret].cnt_dip :
			&sad->cnt_arr[ret].cnt_dip_sip;
	if (--(*cnt) != 0)
		return 0;

	/* corresponding counter is 0, clear the bit indicating
	 * the presence of more specific rule for a given SPI.
	 */
	tmp_val = CLEAR_BIT(tmp_val, key_type);

	/* if there are no rules left with same SPI,
	 * remove an entry from SPI_only table
	 */
	if (tmp_val == NULL)
		ret = rte_hash_del_key_with_hash(
			sad->hash[RTE_IPSEC_SAD_SPI_ONLY], key,
			rte_hash_crc(key, sad->keysize[RTE_IPSEC_SAD_SPI_ONLY],
			sad->init_val));
	else
		ret = rte_hash_add_key_with_hash_data(
			sad->hash[RTE_IPSEC_SAD_SPI_ONLY], key,
			rte_hash_crc(key, sad->keysize[RTE_IPSEC_SAD_SPI_ONLY],
			sad->init_val), tmp_val);
	if (ret < 0)
		return ret;
	return 0;
}

int
rte_ipsec_sad_del(struct rte_ipsec_sad *sad,
		const union rte_ipsec_sad_key *key,
		int key_type)
{
	void *tmp_val;
	int ret;

	if ((sad == NULL) || (key == NULL))
		return -EINVAL;
	switch (key_type) {
	case(RTE_IPSEC_SAD_SPI_ONLY):
		ret = rte_hash_lookup_with_hash_data(sad->hash[key_type],
			key, rte_hash_crc(key, sad->keysize[key_type],
			sad->init_val), &tmp_val);
		if (ret < 0)
			return ret;
		if (GET_BIT(tmp_val, RTE_IPSEC_SAD_KEY_TYPE_MASK) == 0) {
			ret = rte_hash_del_key_with_hash(sad->hash[key_type],
				key, rte_hash_crc(key, sad->keysize[key_type],
				sad->init_val));
			ret = ret < 0 ? ret : 0;
		} else {
			tmp_val = GET_BIT(tmp_val,
				RTE_IPSEC_SAD_KEY_TYPE_MASK);
			ret = rte_hash_add_key_with_hash_data(
				sad->hash[key_type], key,
				rte_hash_crc(key, sad->keysize[key_type],
				sad->init_val), tmp_val);
		}
		return ret;
	case(RTE_IPSEC_SAD_SPI_DIP):
	case(RTE_IPSEC_SAD_SPI_DIP_SIP):
		return del_specific(sad, key, key_type);
	default:
		return -EINVAL;
	}
}

struct rte_ipsec_sad *
rte_ipsec_sad_create(const char *name, const struct rte_ipsec_sad_conf *conf)
{
	char hash_name[RTE_HASH_NAMESIZE];
	char sad_name[RTE_IPSEC_SAD_NAMESIZE];
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

	ret = snprintf(sad_name, RTE_IPSEC_SAD_NAMESIZE, SAD_FORMAT, name);
	if (ret < 0 || ret >= RTE_IPSEC_SAD_NAMESIZE) {
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
	sad->init_val = hash_params.hash_func_init_val;
	hash_params.socket_id = conf->socket_id;
	hash_params.name = hash_name;
	if (conf->flags & RTE_IPSEC_SAD_FLAG_RW_CONCURRENCY)
		hash_params.extra_flag = RTE_HASH_EXTRA_FLAGS_RW_CONCURRENCY;

	/** Init hash[RTE_IPSEC_SAD_SPI_ONLY] for SPI only */
	snprintf(hash_name, sizeof(hash_name), "sad_1_%p", sad);
	hash_params.key_len = sizeof(((struct rte_ipsec_sadv4_key *)0)->spi);
	sad->keysize[RTE_IPSEC_SAD_SPI_ONLY] = hash_params.key_len;
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
	sad->keysize[RTE_IPSEC_SAD_SPI_DIP] = hash_params.key_len;
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
	sad->keysize[RTE_IPSEC_SAD_SPI_DIP_SIP] = hash_params.key_len;
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
		if (strncmp(sad_name, tmp_sad->name,
				RTE_IPSEC_SAD_NAMESIZE) == 0)
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
	char sad_name[RTE_IPSEC_SAD_NAMESIZE];
	struct rte_ipsec_sad *sad = NULL;
	struct rte_tailq_entry *te;
	struct rte_ipsec_sad_list *sad_list;
	int ret;

	ret = snprintf(sad_name, RTE_IPSEC_SAD_NAMESIZE, SAD_FORMAT, name);
	if (ret < 0 || ret >= RTE_IPSEC_SAD_NAMESIZE) {
		rte_errno = ENAMETOOLONG;
		return NULL;
	}

	sad_list = RTE_TAILQ_CAST(rte_ipsec_sad_tailq.head,
		rte_ipsec_sad_list);

	rte_mcfg_tailq_read_lock();
	TAILQ_FOREACH(te, sad_list, next) {
		sad = (struct rte_ipsec_sad *) te->data;
		if (strncmp(sad_name, sad->name, RTE_IPSEC_SAD_NAMESIZE) == 0)
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
	rte_free(te);
}

/*
 * @internal helper function
 * Lookup a batch of keys in three hash tables.
 * First lookup key in SPI_ONLY table.
 * If there is an entry for the corresponding SPI check its value.
 * Two least significant bits of the value indicate
 * the presence of more specific rule in other tables.
 * Perform additional lookup in corresponding hash tables
 * and update the value if lookup succeeded.
 */
static int
__ipsec_sad_lookup(const struct rte_ipsec_sad *sad,
		const union rte_ipsec_sad_key *keys[], void *sa[], uint32_t n)
{
	const void *keys_2[RTE_HASH_LOOKUP_BULK_MAX];
	const void *keys_3[RTE_HASH_LOOKUP_BULK_MAX];
	void *vals_2[RTE_HASH_LOOKUP_BULK_MAX] = {NULL};
	void *vals_3[RTE_HASH_LOOKUP_BULK_MAX] = {NULL};
	uint32_t idx_2[RTE_HASH_LOOKUP_BULK_MAX];
	uint32_t idx_3[RTE_HASH_LOOKUP_BULK_MAX];
	uint64_t mask_1, mask_2, mask_3;
	uint64_t map, map_spec;
	uint32_t n_2 = 0;
	uint32_t n_3 = 0;
	uint32_t i;
	int found = 0;
	hash_sig_t hash_sig[RTE_HASH_LOOKUP_BULK_MAX];
	hash_sig_t hash_sig_2[RTE_HASH_LOOKUP_BULK_MAX];
	hash_sig_t hash_sig_3[RTE_HASH_LOOKUP_BULK_MAX];

	for (i = 0; i < n; i++) {
		sa[i] = NULL;
		hash_sig[i] = rte_hash_crc_4byte(keys[i]->v4.spi,
			sad->init_val);
	}

	/*
	 * Lookup keys in SPI only hash table first.
	 */
	rte_hash_lookup_with_hash_bulk_data(sad->hash[RTE_IPSEC_SAD_SPI_ONLY],
		(const void **)keys, hash_sig, n, &mask_1, sa);
	for (map = mask_1; map; map &= (map - 1)) {
		i = rte_bsf64(map);
		/*
		 * if returned value indicates presence of a rule in other
		 * tables save a key for further lookup.
		 */
		if ((uintptr_t)sa[i] & RTE_IPSEC_SAD_SPI_DIP_SIP) {
			idx_3[n_3] = i;
			hash_sig_3[n_3] = rte_hash_crc(keys[i],
				sad->keysize[RTE_IPSEC_SAD_SPI_DIP_SIP],
				sad->init_val);
			keys_3[n_3++] = keys[i];
		}
		if ((uintptr_t)sa[i] & RTE_IPSEC_SAD_SPI_DIP) {
			idx_2[n_2] = i;
			hash_sig_2[n_2] = rte_hash_crc(keys[i],
				sad->keysize[RTE_IPSEC_SAD_SPI_DIP],
				sad->init_val);
			keys_2[n_2++] = keys[i];
		}
		/* clear 2 LSB's which indicate the presence
		 * of more specific rules
		 */
		sa[i] = CLEAR_BIT(sa[i], RTE_IPSEC_SAD_KEY_TYPE_MASK);
	}

	/* Lookup for more specific rules in SPI_DIP table */
	if (n_2 != 0) {
		rte_hash_lookup_with_hash_bulk_data(
			sad->hash[RTE_IPSEC_SAD_SPI_DIP],
			keys_2, hash_sig_2, n_2, &mask_2, vals_2);
		for (map_spec = mask_2; map_spec; map_spec &= (map_spec - 1)) {
			i = rte_bsf64(map_spec);
			sa[idx_2[i]] = vals_2[i];
		}
	}
	/* Lookup for more specific rules in SPI_DIP_SIP table */
	if (n_3 != 0) {
		rte_hash_lookup_with_hash_bulk_data(
			sad->hash[RTE_IPSEC_SAD_SPI_DIP_SIP],
			keys_3, hash_sig_3, n_3, &mask_3, vals_3);
		for (map_spec = mask_3; map_spec; map_spec &= (map_spec - 1)) {
			i = rte_bsf64(map_spec);
			sa[idx_3[i]] = vals_3[i];
		}
	}

	for (i = 0; i < n; i++)
		found += (sa[i] != NULL);

	return found;
}

int
rte_ipsec_sad_lookup(const struct rte_ipsec_sad *sad,
		const union rte_ipsec_sad_key *keys[], void *sa[], uint32_t n)
{
	uint32_t num, i = 0;
	int found = 0;

	if (unlikely((sad == NULL) || (keys == NULL) || (sa == NULL)))
		return -EINVAL;

	do {
		num = RTE_MIN(n - i, (uint32_t)RTE_HASH_LOOKUP_BULK_MAX);
		found += __ipsec_sad_lookup(sad,
			&keys[i], &sa[i], num);
		i += num;
	} while (i != n);

	return found;
}
