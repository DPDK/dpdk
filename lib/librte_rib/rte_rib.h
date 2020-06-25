/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_RIB_H_
#define _RTE_RIB_H_

/**
 * @file
 *
 * RTE RIB library.
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * Level compressed tree implementation for IPv4 Longest Prefix Match
 */

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * rte_rib_get_nxt() flags
 */
enum {
	/** flag to get all subroutes in a RIB tree */
	RTE_RIB_GET_NXT_ALL,
	/** flag to get first matched subroutes in a RIB tree */
	RTE_RIB_GET_NXT_COVER
};

struct rte_rib;
struct rte_rib_node;

/** RIB configuration structure */
struct rte_rib_conf {
	/**
	 * Size of extension block inside rte_rib_node.
	 * This space could be used to store additional user
	 * defined data.
	 */
	size_t	ext_sz;
	/* size of rte_rib_node's pool */
	int	max_nodes;
};

/**
 * Get an IPv4 mask from prefix length
 * It is caller responsibility to make sure depth is not bigger than 32
 *
 * @param depth
 *   prefix length
 * @return
 *  IPv4 mask
 */
static inline uint32_t
rte_rib_depth_to_mask(uint8_t depth)
{
	return (uint32_t)(UINT64_MAX << (32 - depth));
}

/**
 * Lookup an IP into the RIB structure
 *
 * @param rib
 *  RIB object handle
 * @param ip
 *  IP to be looked up in the RIB
 * @return
 *  pointer to struct rte_rib_node on success
 *  NULL otherwise
 */
__rte_experimental
struct rte_rib_node *
rte_rib_lookup(struct rte_rib *rib, uint32_t ip);

/**
 * Lookup less specific route into the RIB structure
 *
 * @param ent
 *  Pointer to struct rte_rib_node that represents target route
 * @return
 *  pointer to struct rte_rib_node that represents
 *   less specific route on success
 *  NULL otherwise
 */
__rte_experimental
struct rte_rib_node *
rte_rib_lookup_parent(struct rte_rib_node *ent);

/**
 * Lookup prefix into the RIB structure
 *
 * @param rib
 *  RIB object handle
 * @param ip
 *  net to be looked up in the RIB
 * @param depth
 *  prefix length
 * @return
 *  pointer to struct rte_rib_node on success
 *  NULL otherwise
 */
__rte_experimental
struct rte_rib_node *
rte_rib_lookup_exact(struct rte_rib *rib, uint32_t ip, uint8_t depth);

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
 *  -RTE_RIB_GET_NXT_ALL
 *   get all prefixes from subtrie
 *  -RTE_RIB_GET_NXT_COVER
 *   get only first more specific prefix even if it have more specifics
 * @return
 *  pointer to the next more specific prefix
 *  NULL if there is no prefixes left
 */
__rte_experimental
struct rte_rib_node *
rte_rib_get_nxt(struct rte_rib *rib, uint32_t ip, uint8_t depth,
	struct rte_rib_node *last, int flag);

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
__rte_experimental
void
rte_rib_remove(struct rte_rib *rib, uint32_t ip, uint8_t depth);

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
 *  pointer to new rte_rib_node on success
 *  NULL otherwise
 */
__rte_experimental
struct rte_rib_node *
rte_rib_insert(struct rte_rib *rib, uint32_t ip, uint8_t depth);

/**
 * Get an ip from rte_rib_node
 *
 * @param node
 *  pointer to the rib node
 * @param ip
 *  pointer to the ip to save
 * @return
 *  0 on success.
 *  -1 on failure with rte_errno indicating reason for failure.
 */
__rte_experimental
int
rte_rib_get_ip(const struct rte_rib_node *node, uint32_t *ip);

/**
 * Get a depth from rte_rib_node
 *
 * @param node
 *  pointer to the rib node
 * @param depth
 *  pointer to the depth to save
 * @return
 *  0 on success.
 *  -1 on failure with rte_errno indicating reason for failure.
 */
__rte_experimental
int
rte_rib_get_depth(const struct rte_rib_node *node, uint8_t *depth);

/**
 * Get ext field from the rib node
 * It is caller responsibility to make sure there are necessary space
 * for the ext field inside rib node.
 *
 * @param node
 *  pointer to the rib node
 * @return
 *  pointer to the ext
 */
__rte_experimental
void *
rte_rib_get_ext(struct rte_rib_node *node);

/**
 * Get nexthop from the rib node
 *
 * @param node
 *  pointer to the rib node
 * @param nh
 *  pointer to the nexthop to save
 * @return
 *  0 on success.
 *  -1 on failure with rte_errno indicating reason for failure.
 */
__rte_experimental
int
rte_rib_get_nh(const struct rte_rib_node *node, uint64_t *nh);

/**
 * Set nexthop into the rib node
 *
 * @param node
 *  pointer to the rib node
 * @param nh
 *  nexthop value to set to the rib node
 * @return
 *  0 on success.
 *  -1 on failure with rte_errno indicating reason for failure.
 */
__rte_experimental
int
rte_rib_set_nh(struct rte_rib_node *node, uint64_t nh);

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
 *  Handle to RIB object on success
 *  NULL otherwise with rte_errno indicating reason for failure.
 */
__rte_experimental
struct rte_rib *
rte_rib_create(const char *name, int socket_id,
	       const struct rte_rib_conf *conf);

/**
 * Find an existing RIB object and return a pointer to it.
 *
 * @param name
 *  Name of the rib object as passed to rte_rib_create()
 * @return
 *  Pointer to RIB object on success
 *  NULL otherwise with rte_errno indicating reason for failure.
 */
__rte_experimental
struct rte_rib *
rte_rib_find_existing(const char *name);

/**
 * Free an RIB object.
 *
 * @param rib
 *   RIB object handle
 * @return
 *   None
 */
__rte_experimental
void
rte_rib_free(struct rte_rib *rib);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_RIB_H_ */
