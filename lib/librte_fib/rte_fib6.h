/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_FIB6_H_
#define _RTE_FIB6_H_

/**
 * @file
 *
 * RTE FIB6 library.
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * FIB (Forwarding information base) implementation
 * for IPv6 Longest Prefix Match
 */

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

#define RTE_FIB6_IPV6_ADDR_SIZE		16
/** Maximum depth value possible for IPv6 FIB. */
#define RTE_FIB6_MAXDEPTH       128

struct rte_fib6;
struct rte_rib6;

/** Type of FIB struct */
enum rte_fib6_type {
	RTE_FIB6_DUMMY,		/**< RIB6 tree based FIB */
	RTE_FIB6_TRIE,		/**< TRIE based fib  */
	RTE_FIB6_TYPE_MAX
};

/** Modify FIB function */
typedef int (*rte_fib6_modify_fn_t)(struct rte_fib6 *fib,
	const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE], uint8_t depth,
	uint64_t next_hop, int op);
/** FIB bulk lookup function */
typedef void (*rte_fib6_lookup_fn_t)(void *fib,
	uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, const unsigned int n);

enum rte_fib6_op {
	RTE_FIB6_ADD,
	RTE_FIB6_DEL,
};

enum rte_fib_trie_nh_sz {
	RTE_FIB6_TRIE_2B = 1,
	RTE_FIB6_TRIE_4B,
	RTE_FIB6_TRIE_8B
};

/** FIB configuration structure */
struct rte_fib6_conf {
	enum rte_fib6_type type; /**< Type of FIB struct */
	/** Default value returned on lookup if there is no route */
	uint64_t default_nh;
	int	max_routes;
	union {
		struct {
			enum rte_fib_trie_nh_sz nh_sz;
			uint32_t	num_tbl8;
		} trie;
	};
};

/**
 * Create FIB
 *
 * @param name
 *  FIB name
 * @param socket_id
 *  NUMA socket ID for FIB table memory allocation
 * @param conf
 *  Structure containing the configuration
 * @return
 *  Handle to FIB object on success
 *  NULL otherwise with rte_errno set to an appropriate values.
 */
__rte_experimental
struct rte_fib6 *
rte_fib6_create(const char *name, int socket_id, struct rte_fib6_conf *conf);

/**
 * Find an existing FIB object and return a pointer to it.
 *
 * @param name
 *  Name of the fib object as passed to rte_fib6_create()
 * @return
 *  Pointer to fib object or NULL if object not found with rte_errno
 *  set appropriately. Possible rte_errno values include:
 *   - ENOENT - required entry not available to return.
 */
__rte_experimental
struct rte_fib6 *
rte_fib6_find_existing(const char *name);

/**
 * Free an FIB object.
 *
 * @param fib
 *   FIB object handle
 * @return
 *   None
 */
__rte_experimental
void
rte_fib6_free(struct rte_fib6 *fib);

/**
 * Add a route to the FIB.
 *
 * @param fib
 *   FIB object handle
 * @param ip
 *   IPv6 prefix address to be added to the FIB
 * @param depth
 *   Prefix length
 * @param next_hop
 *   Next hop to be added to the FIB
 * @return
 *   0 on success, negative value otherwise
 */
__rte_experimental
int
rte_fib6_add(struct rte_fib6 *fib, const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE],
	uint8_t depth, uint64_t next_hop);

/**
 * Delete a rule from the FIB.
 *
 * @param fib
 *   FIB object handle
 * @param ip
 *   IPv6 prefix address to be deleted from the FIB
 * @param depth
 *   Prefix length
 * @return
 *   0 on success, negative value otherwise
 */
__rte_experimental
int
rte_fib6_delete(struct rte_fib6 *fib,
	const uint8_t ip[RTE_FIB6_IPV6_ADDR_SIZE], uint8_t depth);

/**
 * Lookup multiple IP addresses in the FIB.
 *
 * @param fib
 *   FIB object handle
 * @param ips
 *   Array of IPv6s to be looked up in the FIB
 * @param next_hops
 *   Next hop of the most specific rule found for IP.
 *   This is an array of eight byte values.
 *   If the lookup for the given IP failed, then corresponding element would
 *   contain default nexthop value configured for a FIB.
 * @param n
 *   Number of elements in ips (and next_hops) array to lookup.
 *  @return
 *   -EINVAL for incorrect arguments, otherwise 0
 */
__rte_experimental
int
rte_fib6_lookup_bulk(struct rte_fib6 *fib,
	uint8_t ips[][RTE_FIB6_IPV6_ADDR_SIZE],
	uint64_t *next_hops, int n);

/**
 * Get pointer to the dataplane specific struct
 *
 * @param fib
 *   FIB6 object handle
 * @return
 *   Pointer on the dataplane struct on success
 *   NULL othervise
 */
__rte_experimental
void *
rte_fib6_get_dp(struct rte_fib6 *fib);

/**
 * Get pointer to the RIB6
 *
 * @param fib
 *   FIB object handle
 * @return
 *   Pointer on the RIB6 on success
 *   NULL othervise
 */
__rte_experimental
struct rte_rib6 *
rte_fib6_get_rib(struct rte_fib6 *fib);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_FIB6_H_ */
