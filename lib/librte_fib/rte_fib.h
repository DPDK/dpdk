/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Vladimir Medvedkin <medvedkinv@gmail.com>
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _RTE_FIB_H_
#define _RTE_FIB_H_

/**
 * @file
 *
 * RTE FIB library.
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * FIB (Forwarding information base) implementation
 * for IPv4 Longest Prefix Match
 */

#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

struct rte_fib;
struct rte_rib;

/** Maximum depth value possible for IPv4 FIB. */
#define RTE_FIB_MAXDEPTH	32

/** Type of FIB struct */
enum rte_fib_type {
	RTE_FIB_DUMMY,		/**< RIB tree based FIB */
	RTE_FIB_DIR24_8,	/**< DIR24_8 based FIB */
	RTE_FIB_TYPE_MAX
};

/** Modify FIB function */
typedef int (*rte_fib_modify_fn_t)(struct rte_fib *fib, uint32_t ip,
	uint8_t depth, uint64_t next_hop, int op);
/** FIB bulk lookup function */
typedef void (*rte_fib_lookup_fn_t)(void *fib, const uint32_t *ips,
	uint64_t *next_hops, const unsigned int n);

enum rte_fib_op {
	RTE_FIB_ADD,
	RTE_FIB_DEL,
};

/** Size of nexthop (1 << nh_sz) bits for DIR24_8 based FIB */
enum rte_fib_dir24_8_nh_sz {
	RTE_FIB_DIR24_8_1B,
	RTE_FIB_DIR24_8_2B,
	RTE_FIB_DIR24_8_4B,
	RTE_FIB_DIR24_8_8B
};

/** FIB configuration structure */
struct rte_fib_conf {
	enum rte_fib_type type; /**< Type of FIB struct */
	/** Default value returned on lookup if there is no route */
	uint64_t default_nh;
	int	max_routes;
	union {
		struct {
			enum rte_fib_dir24_8_nh_sz nh_sz;
			uint32_t	num_tbl8;
		} dir24_8;
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
 *  Handle to the FIB object on success
 *  NULL otherwise with rte_errno set to an appropriate values.
 */
__rte_experimental
struct rte_fib *
rte_fib_create(const char *name, int socket_id, struct rte_fib_conf *conf);

/**
 * Find an existing FIB object and return a pointer to it.
 *
 * @param name
 *  Name of the fib object as passed to rte_fib_create()
 * @return
 *  Pointer to fib object or NULL if object not found with rte_errno
 *  set appropriately. Possible rte_errno values include:
 *   - ENOENT - required entry not available to return.
 */
__rte_experimental
struct rte_fib *
rte_fib_find_existing(const char *name);

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
rte_fib_free(struct rte_fib *fib);

/**
 * Add a route to the FIB.
 *
 * @param fib
 *   FIB object handle
 * @param ip
 *   IPv4 prefix address to be added to the FIB
 * @param depth
 *   Prefix length
 * @param next_hop
 *   Next hop to be added to the FIB
 * @return
 *   0 on success, negative value otherwise
 */
__rte_experimental
int
rte_fib_add(struct rte_fib *fib, uint32_t ip, uint8_t depth, uint64_t next_hop);

/**
 * Delete a rule from the FIB.
 *
 * @param fib
 *   FIB object handle
 * @param ip
 *   IPv4 prefix address to be deleted from the FIB
 * @param depth
 *   Prefix length
 * @return
 *   0 on success, negative value otherwise
 */
__rte_experimental
int
rte_fib_delete(struct rte_fib *fib, uint32_t ip, uint8_t depth);

/**
 * Lookup multiple IP addresses in the FIB.
 *
 * @param fib
 *   FIB object handle
 * @param ips
 *   Array of IPs to be looked up in the FIB
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
rte_fib_lookup_bulk(struct rte_fib *fib, uint32_t *ips,
		uint64_t *next_hops, int n);
/**
 * Get pointer to the dataplane specific struct
 *
 * @param fib
 *   FIB object handle
 * @return
 *   Pointer on the dataplane struct on success
 *   NULL othervise
 */
__rte_experimental
void *
rte_fib_get_dp(struct rte_fib *fib);

/**
 * Get pointer to the RIB
 *
 * @param fib
 *   FIB object handle
 * @return
 *   Pointer on the RIB on success
 *   NULL othervise
 */
__rte_experimental
struct rte_rib *
rte_fib_get_rib(struct rte_fib *fib);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_FIB_H_ */
