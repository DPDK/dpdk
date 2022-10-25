/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_RIB6_H_
#define _RTE_RIB6_H_

/**
 * @file
 *
 * RTE rib6 library.
 *
 * Level compressed tree implementation for IPv6 Longest Prefix Match
 */

#include <rte_memcpy.h>
#include <rte_common.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_RIB6_IPV6_ADDR_SIZE	16

/**
 * rte_rib6_get_nxt() flags
 */
enum {
	/** flag to get all subroutes in a RIB tree */
	RTE_RIB6_GET_NXT_ALL,
	/** flag to get first matched subroutes in a RIB tree */
	RTE_RIB6_GET_NXT_COVER
};

struct rte_rib6;
struct rte_rib6_node;

/** RIB configuration structure */
struct rte_rib6_conf {
	/**
	 * Size of extension block inside rte_rib6_node.
	 * This space could be used to store additional user
	 * defined data.
	 */
	size_t	ext_sz;
	/* size of rte_rib6_node's pool */
	int	max_nodes;
};

/**
 * Copy IPv6 address from one location to another
 *
 * @param dst
 *  pointer to the place to copy
 * @param src
 *  pointer from where to copy
 */
static inline void
rte_rib6_copy_addr(uint8_t *dst, const uint8_t *src)
{
	if ((dst == NULL) || (src == NULL))
		return;
	rte_memcpy(dst, src, RTE_RIB6_IPV6_ADDR_SIZE);
}

/**
 * Compare two IPv6 addresses
 *
 * @param ip1
 *  pointer to the first ipv6 address
 * @param ip2
 *  pointer to the second ipv6 address
 *
 * @return
 *  1 if equal
 *  0 otherwise
 */
static inline int
rte_rib6_is_equal(const uint8_t *ip1, const uint8_t *ip2) {
	int i;

	if ((ip1 == NULL) || (ip2 == NULL))
		return 0;
	for (i = 0; i < RTE_RIB6_IPV6_ADDR_SIZE; i++) {
		if (ip1[i] != ip2[i])
			return 0;
	}
	return 1;
}

/**
 * Get 8-bit part of 128-bit IPv6 mask
 *
 * @param depth
 *  ipv6 prefix length
 * @param byte
 *  position of a 8-bit chunk in the 128-bit mask
 *
 * @return
 *  8-bit chunk of the 128-bit IPv6 mask
 */
static inline uint8_t
get_msk_part(uint8_t depth, int byte) {
	uint8_t part;

	byte &= 0xf;
	depth = RTE_MIN(depth, 128);
	part = RTE_MAX((int16_t)depth - (byte * 8), 0);
	part = (part > 8) ? 8 : part;
	return (uint16_t)(~UINT8_MAX) >> part;
}

/**
 * Lookup an IP into the RIB structure
 *
 * @param rib
 *  RIB object handle
 * @param ip
 *  IP to be looked up in the RIB
 * @return
 *  pointer to struct rte_rib6_node on success
 *  NULL otherwise
 */
struct rte_rib6_node *
rte_rib6_lookup(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE]);

/**
 * Lookup less specific route into the RIB structure
 *
 * @param ent
 *  Pointer to struct rte_rib6_node that represents target route
 * @return
 *  pointer to struct rte_rib6_node that represents
 *   less specific route on success
 *  NULL otherwise
 */
struct rte_rib6_node *
rte_rib6_lookup_parent(struct rte_rib6_node *ent);

/**
 * Provides exact mach lookup of the prefix into the RIB structure
 *
 * @param rib
 *  RIB object handle
 * @param ip
 *  net to be looked up in the RIB
 * @param depth
 *  prefix length
 * @return
 *  pointer to struct rte_rib6_node on success
 *  NULL otherwise
 */
struct rte_rib6_node *
rte_rib6_lookup_exact(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth);

/**
 * Retrieve next more specific prefix from the RIB
 * that is covered by ip/depth supernet in an ascending order
 *
 * @param rib
 *  RIB object handle
 * @param ip
 *  net address of supernet prefix that covers returned more specific prefixes
 * @param depth
 *  supernet prefix length
 * @param last
 *   pointer to the last returned prefix to get next prefix
 *   or
 *   NULL to get first more specific prefix
 * @param flag
 *  -RTE_RIB6_GET_NXT_ALL
 *   get all prefixes from subtrie
 *  -RTE_RIB6_GET_NXT_COVER
 *   get only first more specific prefix even if it have more specifics
 * @return
 *  pointer to the next more specific prefix
 *  NULL if there is no prefixes left
 */
struct rte_rib6_node *
rte_rib6_get_nxt(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE],
	uint8_t depth, struct rte_rib6_node *last, int flag);

/**
 * Remove prefix from the RIB
 *
 * @param rib
 *  RIB object handle
 * @param ip
 *  net to be removed from the RIB
 * @param depth
 *  prefix length
 */
void
rte_rib6_remove(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth);

/**
 * Insert prefix into the RIB
 *
 * @param rib
 *  RIB object handle
 * @param ip
 *  net to be inserted to the RIB
 * @param depth
 *  prefix length
 * @return
 *  pointer to new rte_rib6_node on success
 *  NULL otherwise
 */
struct rte_rib6_node *
rte_rib6_insert(struct rte_rib6 *rib,
	const uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE], uint8_t depth);

/**
 * Get an ip from rte_rib6_node
 *
 * @param node
 *  pointer to the rib6 node
 * @param ip
 *  pointer to the ipv6 to save
 * @return
 *  0 on success
 *  -1 on failure with rte_errno indicating reason for failure.
 */
int
rte_rib6_get_ip(const struct rte_rib6_node *node,
		uint8_t ip[RTE_RIB6_IPV6_ADDR_SIZE]);

/**
 * Get a depth from rte_rib6_node
 *
 * @param node
 *  pointer to the rib6 node
 * @param depth
 *  pointer to the depth to save
 * @return
 *  0 on success
 *  -1 on failure with rte_errno indicating reason for failure.
 */
int
rte_rib6_get_depth(const struct rte_rib6_node *node, uint8_t *depth);

/**
 * Get ext field from the rte_rib6_node
 * It is caller responsibility to make sure there are necessary space
 * for the ext field inside rib6 node.
 *
 * @param node
 *  pointer to the rte_rib6_node
 * @return
 *  pointer to the ext
 */
void *
rte_rib6_get_ext(struct rte_rib6_node *node);

/**
 * Get nexthop from the rte_rib6_node
 *
 * @param node
 *  pointer to the rib6 node
 * @param nh
 *  pointer to the nexthop to save
 * @return
 *  0 on success
 *  -1 on failure, with rte_errno indicating reason for failure.
 */
int
rte_rib6_get_nh(const struct rte_rib6_node *node, uint64_t *nh);

/**
 * Set nexthop into the rte_rib6_node
 *
 * @param node
 *  pointer to the rib6 node
 * @param nh
 *  nexthop value to set to the rib6 node
 * @return
 *  0 on success
 *  -1 on failure, with rte_errno indicating reason for failure.
 */
int
rte_rib6_set_nh(struct rte_rib6_node *node, uint64_t nh);

/**
 * Create RIB
 *
 * @param name
 *  RIB name
 * @param socket_id
 *  NUMA socket ID for RIB table memory allocation
 * @param conf
 *  Structure containing the configuration
 * @return
 *  Pointer to RIB object on success
 *  NULL otherwise with rte_errno indicating reason for failure.
 */
struct rte_rib6 *
rte_rib6_create(const char *name, int socket_id,
		const struct rte_rib6_conf *conf);

/**
 * Find an existing RIB object and return a pointer to it.
 *
 * @param name
 *  Name of the rib object as passed to rte_rib6_create()
 * @return
 *  Pointer to RIB object on success
 *  NULL otherwise with rte_errno indicating reason for failure.
 */
struct rte_rib6 *
rte_rib6_find_existing(const char *name);

/**
 * Free an RIB object.
 *
 * @param rib
 *   RIB object handle created with rte_rib6_create().
 *   If rib is NULL, no operation is performed.
 */
void
rte_rib6_free(struct rte_rib6 *rib);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RIB6_H_ */
