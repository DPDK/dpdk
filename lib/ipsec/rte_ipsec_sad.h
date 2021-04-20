
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_IPSEC_SAD_H_
#define _RTE_IPSEC_SAD_H_

#include <stdint.h>

#include <rte_compat.h>

/**
 * @file rte_ipsec_sad.h
 *
 * RTE IPsec security association database (SAD) support.
 * Contains helper functions to lookup and maintain SAD
 */

#ifdef __cplusplus
extern "C" {
#endif

struct rte_ipsec_sad;

/** Type of key */
enum {
	RTE_IPSEC_SAD_SPI_ONLY = 0,
	RTE_IPSEC_SAD_SPI_DIP,
	RTE_IPSEC_SAD_SPI_DIP_SIP,
	RTE_IPSEC_SAD_KEY_TYPE_MASK,
};

struct rte_ipsec_sadv4_key {
	uint32_t spi;
	uint32_t dip;
	uint32_t sip;
};

struct rte_ipsec_sadv6_key {
	uint32_t spi;
	uint8_t dip[16];
	uint8_t sip[16];
};

union rte_ipsec_sad_key {
	struct rte_ipsec_sadv4_key	v4;
	struct rte_ipsec_sadv6_key	v6;
};

/** Max number of characters in SAD name. */
#define RTE_IPSEC_SAD_NAMESIZE		64
/** Flag to create SAD with ipv6 dip and sip addresses */
#define RTE_IPSEC_SAD_FLAG_IPV6			0x1
/** Flag to support reader writer concurrency */
#define RTE_IPSEC_SAD_FLAG_RW_CONCURRENCY	0x2

/** IPsec SAD configuration structure */
struct rte_ipsec_sad_conf {
	/** CPU socket ID where rte_ipsec_sad should be allocated */
	int		socket_id;
	/** maximum number of SA for each type of key */
	uint32_t	max_sa[RTE_IPSEC_SAD_KEY_TYPE_MASK];
	/** RTE_IPSEC_SAD_FLAG_* flags */
	uint32_t	flags;
};

/**
 * Add a rule into the SAD. Could be safely called with concurrent lookups
 *  if RTE_IPSEC_SAD_FLAG_RW_CONCURRENCY flag was configured on creation time.
 *  While with this flag multi-reader - one-writer model Is MT safe,
 *  multi-writer model is not and required extra synchronisation.
 *
 * @param sad
 *   SAD object handle
 * @param key
 *   pointer to the key
 * @param key_type
 *   key type (spi only/spi+dip/spi+dip+sip)
 * @param sa
 *   Pointer associated with the key to save in a SAD
 *   Must be 4 bytes aligned.
 * @return
 *   0 on success, negative value otherwise
 */
int
rte_ipsec_sad_add(struct rte_ipsec_sad *sad,
	const union rte_ipsec_sad_key *key,
	int key_type, void *sa);

/**
 * Delete a rule from the SAD. Could be safely called with concurrent lookups
 *  if RTE_IPSEC_SAD_FLAG_RW_CONCURRENCY flag was configured on creation time.
 *  While with this flag multi-reader - one-writer model Is MT safe,
 *  multi-writer model is not and required extra synchronisation.
 *
 * @param sad
 *   SAD object handle
 * @param key
 *   pointer to the key
 * @param key_type
 *   key type (spi only/spi+dip/spi+dip+sip)
 * @return
 *   0 on success, negative value otherwise
 */
int
rte_ipsec_sad_del(struct rte_ipsec_sad *sad,
	const union rte_ipsec_sad_key *key,
	int key_type);
/*
 * Create SAD
 *
 * @param name
 *  SAD name
 * @param conf
 *  Structure containing the configuration
 * @return
 *  Handle to SAD object on success
 *  NULL otherwise with rte_errno set to an appropriate values.
 */
struct rte_ipsec_sad *
rte_ipsec_sad_create(const char *name, const struct rte_ipsec_sad_conf *conf);

/**
 * Find an existing SAD object and return a pointer to it.
 *
 * @param name
 *  Name of the SAD object as passed to rte_ipsec_sad_create()
 * @return
 *  Pointer to sad object or NULL if object not found with rte_errno
 *  set appropriately. Possible rte_errno values include:
 *   - ENOENT - required entry not available to return.
 */
struct rte_ipsec_sad *
rte_ipsec_sad_find_existing(const char *name);

/**
 * Destroy SAD object.
 *
 * @param sad
 *   pointer to the SAD object
 * @return
 *   None
 */
void
rte_ipsec_sad_destroy(struct rte_ipsec_sad *sad);

/**
 * Lookup multiple keys in the SAD.
 *
 * @param sad
 *   SAD object handle
 * @param keys
 *   Array of keys to be looked up in the SAD
 * @param sa
 *   Pointer assocoated with the keys.
 *   If the lookup for the given key failed, then corresponding sa
 *   will be NULL
 * @param n
 *   Number of elements in keys array to lookup.
 *  @return
 *   -EINVAL for incorrect arguments, otherwise number of successful lookups.
 */
int
rte_ipsec_sad_lookup(const struct rte_ipsec_sad *sad,
	const union rte_ipsec_sad_key *keys[],
	void *sa[], uint32_t n);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_IPSEC_SAD_H_ */
