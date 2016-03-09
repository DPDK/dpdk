/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _RTE_LPM_H_
#define _RTE_LPM_H_

/**
 * @file
 * RTE Longest Prefix Match (LPM)
 */

#include <errno.h>
#include <sys/queue.h>
#include <stdint.h>
#include <stdlib.h>
#include <rte_branch_prediction.h>
#include <rte_byteorder.h>
#include <rte_memory.h>
#include <rte_common.h>
#include <rte_vect.h>
#include <rte_compat.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max number of characters in LPM name. */
#define RTE_LPM_NAMESIZE                32

/** Maximum depth value possible for IPv4 LPM. */
#define RTE_LPM_MAX_DEPTH               32

/** @internal Total number of tbl24 entries. */
#define RTE_LPM_TBL24_NUM_ENTRIES       (1 << 24)

/** @internal Number of entries in a tbl8 group. */
#define RTE_LPM_TBL8_GROUP_NUM_ENTRIES  256

/** @internal Total number of tbl8 groups in the tbl8. */
#define RTE_LPM_TBL8_NUM_GROUPS         256

/** @internal Total number of tbl8 entries. */
#define RTE_LPM_TBL8_NUM_ENTRIES        (RTE_LPM_TBL8_NUM_GROUPS * \
					RTE_LPM_TBL8_GROUP_NUM_ENTRIES)

/** @internal Macro to enable/disable run-time checks. */
#if defined(RTE_LIBRTE_LPM_DEBUG)
#define RTE_LPM_RETURN_IF_TRUE(cond, retval) do { \
	if (cond) return (retval);                \
} while (0)
#else
#define RTE_LPM_RETURN_IF_TRUE(cond, retval)
#endif

/** @internal bitmask with valid and valid_group fields set */
#define RTE_LPM_VALID_EXT_ENTRY_BITMASK 0x03000000

/** Bitmask used to indicate successful lookup */
#define RTE_LPM_LOOKUP_SUCCESS          0x01000000

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
/** @internal Tbl24 entry structure. */
struct rte_lpm_tbl_entry_v20 {
	/**
	 * Stores Next hop (tbl8 or tbl24 when valid_group is not set) or
	 * a group index pointing to a tbl8 structure (tbl24 only, when
	 * valid_group is set)
	 */
	union {
		uint8_t next_hop;
		uint8_t group_idx;
	};
	/* Using single uint8_t to store 3 values. */
	uint8_t valid     :1;   /**< Validation flag. */
	/**
	 * For tbl24:
	 *  - valid_group == 0: entry stores a next hop
	 *  - valid_group == 1: entry stores a group_index pointing to a tbl8
	 * For tbl8:
	 *  - valid_group indicates whether the current tbl8 is in use or not
	 */
	uint8_t valid_group :1;
	uint8_t depth       :6; /**< Rule depth. */
};

struct rte_lpm_tbl_entry {
	/**
	 * Stores Next hop (tbl8 or tbl24 when valid_group is not set) or
	 * a group index pointing to a tbl8 structure (tbl24 only, when
	 * valid_group is set)
	 */
	uint32_t next_hop    :24;
	/* Using single uint8_t to store 3 values. */
	uint32_t valid       :1;   /**< Validation flag. */
	/**
	 * For tbl24:
	 *  - valid_group == 0: entry stores a next hop
	 *  - valid_group == 1: entry stores a group_index pointing to a tbl8
	 * For tbl8:
	 *  - valid_group indicates whether the current tbl8 is in use or not
	 */
	uint32_t valid_group :1;
	uint32_t depth       :6; /**< Rule depth. */
};

#else
struct rte_lpm_tbl_entry_v20 {
	uint8_t depth       :6;
	uint8_t valid_group :1;
	uint8_t valid       :1;
	union {
		uint8_t group_idx;
		uint8_t next_hop;
	};
};

struct rte_lpm_tbl_entry {
	uint32_t depth       :6;
	uint32_t valid_group :1;
	uint32_t valid       :1;
	uint32_t next_hop    :24;

};

#endif

/** @internal Rule structure. */
struct rte_lpm_rule_v20 {
	uint32_t ip; /**< Rule IP address. */
	uint8_t  next_hop; /**< Rule next hop. */
};

struct rte_lpm_rule {
	uint32_t ip; /**< Rule IP address. */
	uint32_t next_hop; /**< Rule next hop. */
};

/** @internal Contains metadata about the rules table. */
struct rte_lpm_rule_info {
	uint32_t used_rules; /**< Used rules so far. */
	uint32_t first_rule; /**< Indexes the first rule of a given depth. */
};

/** @internal LPM structure. */
struct rte_lpm_v20 {
	/* LPM metadata. */
	char name[RTE_LPM_NAMESIZE];        /**< Name of the lpm. */
	uint32_t max_rules; /**< Max. balanced rules per lpm. */
	struct rte_lpm_rule_info rule_info[RTE_LPM_MAX_DEPTH]; /**< Rule info table. */

	/* LPM Tables. */
	struct rte_lpm_tbl_entry_v20 tbl24[RTE_LPM_TBL24_NUM_ENTRIES]
			__rte_cache_aligned; /**< LPM tbl24 table. */
	struct rte_lpm_tbl_entry_v20 tbl8[RTE_LPM_TBL8_NUM_ENTRIES]
			__rte_cache_aligned; /**< LPM tbl8 table. */
	struct rte_lpm_rule_v20 rules_tbl[0] \
			__rte_cache_aligned; /**< LPM rules. */
};

struct rte_lpm {
	/* LPM metadata. */
	char name[RTE_LPM_NAMESIZE];        /**< Name of the lpm. */
	uint32_t max_rules; /**< Max. balanced rules per lpm. */
	struct rte_lpm_rule_info rule_info[RTE_LPM_MAX_DEPTH]; /**< Rule info table. */

	/* LPM Tables. */
	struct rte_lpm_tbl_entry tbl24[RTE_LPM_TBL24_NUM_ENTRIES]
			__rte_cache_aligned; /**< LPM tbl24 table. */
	struct rte_lpm_tbl_entry tbl8[RTE_LPM_TBL8_NUM_ENTRIES]
			__rte_cache_aligned; /**< LPM tbl8 table. */
	struct rte_lpm_rule rules_tbl[0] \
			__rte_cache_aligned; /**< LPM rules. */
};

/**
 * Create an LPM object.
 *
 * @param name
 *   LPM object name
 * @param socket_id
 *   NUMA socket ID for LPM table memory allocation
 * @param max_rules
 *   Maximum number of LPM rules that can be added
 * @param flags
 *   This parameter is currently unused
 * @return
 *   Handle to LPM object on success, NULL otherwise with rte_errno set
 *   to an appropriate values. Possible rte_errno values include:
 *    - E_RTE_NO_CONFIG - function could not get pointer to rte_config structure
 *    - E_RTE_SECONDARY - function was called from a secondary process instance
 *    - EINVAL - invalid parameter passed to function
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_lpm *
rte_lpm_create(const char *name, int socket_id, int max_rules, int flags);
struct rte_lpm_v20 *
rte_lpm_create_v20(const char *name, int socket_id, int max_rules, int flags);
struct rte_lpm *
rte_lpm_create_v1604(const char *name, int socket_id, int max_rules, int flags);

/**
 * Find an existing LPM object and return a pointer to it.
 *
 * @param name
 *   Name of the lpm object as passed to rte_lpm_create()
 * @return
 *   Pointer to lpm object or NULL if object not found with rte_errno
 *   set appropriately. Possible rte_errno values include:
 *    - ENOENT - required entry not available to return.
 */
struct rte_lpm *
rte_lpm_find_existing(const char *name);
struct rte_lpm_v20 *
rte_lpm_find_existing_v20(const char *name);
struct rte_lpm *
rte_lpm_find_existing_v1604(const char *name);

/**
 * Free an LPM object.
 *
 * @param lpm
 *   LPM object handle
 * @return
 *   None
 */
void
rte_lpm_free(struct rte_lpm *lpm);
void
rte_lpm_free_v20(struct rte_lpm_v20 *lpm);
void
rte_lpm_free_v1604(struct rte_lpm *lpm);

/**
 * Add a rule to the LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP of the rule to be added to the LPM table
 * @param depth
 *   Depth of the rule to be added to the LPM table
 * @param next_hop
 *   Next hop of the rule to be added to the LPM table
 * @return
 *   0 on success, negative value otherwise
 */
int
rte_lpm_add(struct rte_lpm *lpm, uint32_t ip, uint8_t depth, uint32_t next_hop);
int
rte_lpm_add_v20(struct rte_lpm_v20 *lpm, uint32_t ip, uint8_t depth,
		uint8_t next_hop);
int
rte_lpm_add_v1604(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
		uint32_t next_hop);

/**
 * Check if a rule is present in the LPM table,
 * and provide its next hop if it is.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP of the rule to be searched
 * @param depth
 *   Depth of the rule to searched
 * @param next_hop
 *   Next hop of the rule (valid only if it is found)
 * @return
 *   1 if the rule exists, 0 if it does not, a negative value on failure
 */
int
rte_lpm_is_rule_present(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
uint32_t *next_hop);
int
rte_lpm_is_rule_present_v20(struct rte_lpm_v20 *lpm, uint32_t ip, uint8_t depth,
uint8_t *next_hop);
int
rte_lpm_is_rule_present_v1604(struct rte_lpm *lpm, uint32_t ip, uint8_t depth,
uint32_t *next_hop);

/**
 * Delete a rule from the LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP of the rule to be deleted from the LPM table
 * @param depth
 *   Depth of the rule to be deleted from the LPM table
 * @return
 *   0 on success, negative value otherwise
 */
int
rte_lpm_delete(struct rte_lpm *lpm, uint32_t ip, uint8_t depth);
int
rte_lpm_delete_v20(struct rte_lpm_v20 *lpm, uint32_t ip, uint8_t depth);
int
rte_lpm_delete_v1604(struct rte_lpm *lpm, uint32_t ip, uint8_t depth);

/**
 * Delete all rules from the LPM table.
 *
 * @param lpm
 *   LPM object handle
 */
void
rte_lpm_delete_all(struct rte_lpm *lpm);
void
rte_lpm_delete_all_v20(struct rte_lpm_v20 *lpm);
void
rte_lpm_delete_all_v1604(struct rte_lpm *lpm);

/**
 * Lookup an IP into the LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   IP to be looked up in the LPM table
 * @param next_hop
 *   Next hop of the most specific rule found for IP (valid on lookup hit only)
 * @return
 *   -EINVAL for incorrect arguments, -ENOENT on lookup miss, 0 on lookup hit
 */
static inline int
rte_lpm_lookup(struct rte_lpm *lpm, uint32_t ip, uint32_t *next_hop)
{
	unsigned tbl24_index = (ip >> 8);
	uint32_t tbl_entry;
	const uint32_t *ptbl;

	/* DEBUG: Check user input arguments. */
	RTE_LPM_RETURN_IF_TRUE(((lpm == NULL) || (next_hop == NULL)), -EINVAL);

	/* Copy tbl24 entry */
	ptbl = (const uint32_t *)(&lpm->tbl24[tbl24_index]);
	tbl_entry = *ptbl;

	/* Copy tbl8 entry (only if needed) */
	if (unlikely((tbl_entry & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {

		unsigned tbl8_index = (uint8_t)ip +
				(((uint32_t)tbl_entry & 0x00FFFFFF) *
						RTE_LPM_TBL8_GROUP_NUM_ENTRIES);

		ptbl = (const uint32_t *)&lpm->tbl8[tbl8_index];
		tbl_entry = *ptbl;
	}

	*next_hop = ((uint32_t)tbl_entry & 0x00FFFFFF);
	return (tbl_entry & RTE_LPM_LOOKUP_SUCCESS) ? 0 : -ENOENT;
}

/**
 * Lookup multiple IP addresses in an LPM table. This may be implemented as a
 * macro, so the address of the function should not be used.
 *
 * @param lpm
 *   LPM object handle
 * @param ips
 *   Array of IPs to be looked up in the LPM table
 * @param next_hops
 *   Next hop of the most specific rule found for IP (valid on lookup hit only).
 *   This is an array of two byte values. The most significant byte in each
 *   value says whether the lookup was successful (bitmask
 *   RTE_LPM_LOOKUP_SUCCESS is set). The least significant byte is the
 *   actual next hop.
 * @param n
 *   Number of elements in ips (and next_hops) array to lookup. This should be a
 *   compile time constant, and divisible by 8 for best performance.
 *  @return
 *   -EINVAL for incorrect arguments, otherwise 0
 */
#define rte_lpm_lookup_bulk(lpm, ips, next_hops, n) \
		rte_lpm_lookup_bulk_func(lpm, ips, next_hops, n)

static inline int
rte_lpm_lookup_bulk_func(const struct rte_lpm *lpm, const uint32_t *ips,
		uint32_t *next_hops, const unsigned n)
{
	unsigned i;
	unsigned tbl24_indexes[n];
	const uint32_t *ptbl;

	/* DEBUG: Check user input arguments. */
	RTE_LPM_RETURN_IF_TRUE(((lpm == NULL) || (ips == NULL) ||
			(next_hops == NULL)), -EINVAL);

	for (i = 0; i < n; i++) {
		tbl24_indexes[i] = ips[i] >> 8;
	}

	for (i = 0; i < n; i++) {
		/* Simply copy tbl24 entry to output */
		ptbl = (const uint32_t *)&lpm->tbl24[tbl24_indexes[i]];
		next_hops[i] = *ptbl;

		/* Overwrite output with tbl8 entry if needed */
		if (unlikely((next_hops[i] & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
				RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {

			unsigned tbl8_index = (uint8_t)ips[i] +
					(((uint32_t)next_hops[i] & 0x00FFFFFF) *
					 RTE_LPM_TBL8_GROUP_NUM_ENTRIES);

			ptbl = (const uint32_t *)&lpm->tbl8[tbl8_index];
			next_hops[i] = *ptbl;
		}
	}
	return 0;
}

/* Mask four results. */
#define	 RTE_LPM_MASKX4_RES	UINT64_C(0x00ffffff00ffffff)

/**
 * Lookup four IP addresses in an LPM table.
 *
 * @param lpm
 *   LPM object handle
 * @param ip
 *   Four IPs to be looked up in the LPM table
 * @param hop
 *   Next hop of the most specific rule found for IP (valid on lookup hit only).
 *   This is an 4 elements array of two byte values.
 *   If the lookup was succesfull for the given IP, then least significant byte
 *   of the corresponding element is the  actual next hop and the most
 *   significant byte is zero.
 *   If the lookup for the given IP failed, then corresponding element would
 *   contain default value, see description of then next parameter.
 * @param defv
 *   Default value to populate into corresponding element of hop[] array,
 *   if lookup would fail.
 */
static inline void
rte_lpm_lookupx4(const struct rte_lpm *lpm, __m128i ip, uint32_t hop[4],
	uint32_t defv)
{
	__m128i i24;
	rte_xmm_t i8;
	uint32_t tbl[4];
	uint64_t idx, pt, pt2;
	const uint32_t *ptbl;

	const __m128i mask8 =
		_mm_set_epi32(UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX);

	/*
	 * RTE_LPM_VALID_EXT_ENTRY_BITMASK for 2 LPM entries
	 * as one 64-bit value (0x0300000003000000).
	 */
	const uint64_t mask_xv =
		((uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK |
		(uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK << 32);

	/*
	 * RTE_LPM_LOOKUP_SUCCESS for 2 LPM entries
	 * as one 64-bit value (0x0100000001000000).
	 */
	const uint64_t mask_v =
		((uint64_t)RTE_LPM_LOOKUP_SUCCESS |
		(uint64_t)RTE_LPM_LOOKUP_SUCCESS << 32);

	/* get 4 indexes for tbl24[]. */
	i24 = _mm_srli_epi32(ip, CHAR_BIT);

	/* extract values from tbl24[] */
	idx = _mm_cvtsi128_si64(i24);
	i24 = _mm_srli_si128(i24, sizeof(uint64_t));

	ptbl = (const uint32_t *)&lpm->tbl24[(uint32_t)idx];
	tbl[0] = *ptbl;
	ptbl = (const uint32_t *)&lpm->tbl24[idx >> 32];
	tbl[1] = *ptbl;

	idx = _mm_cvtsi128_si64(i24);

	ptbl = (const uint32_t *)&lpm->tbl24[(uint32_t)idx];
	tbl[2] = *ptbl;
	ptbl = (const uint32_t *)&lpm->tbl24[idx >> 32];
	tbl[3] = *ptbl;

	/* get 4 indexes for tbl8[]. */
	i8.x = _mm_and_si128(ip, mask8);

	pt = (uint64_t)tbl[0] |
		(uint64_t)tbl[1] << 32;
	pt2 = (uint64_t)tbl[2] |
		(uint64_t)tbl[3] << 32;

	/* search successfully finished for all 4 IP addresses. */
	if (likely((pt & mask_xv) == mask_v) &&
			likely((pt2 & mask_xv) == mask_v)) {
		*(uint64_t *)hop = pt & RTE_LPM_MASKX4_RES;
		*(uint64_t *)(hop + 2) = pt2 & RTE_LPM_MASKX4_RES;
		return;
	}

	if (unlikely((pt & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[0] = i8.u32[0] +
			(uint8_t)tbl[0] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[0]];
		tbl[0] = *ptbl;
	}
	if (unlikely((pt >> 32 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[1] = i8.u32[1] +
			(uint8_t)tbl[1] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[1]];
		tbl[1] = *ptbl;
	}
	if (unlikely((pt2 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[2] = i8.u32[2] +
			(uint8_t)tbl[2] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[2]];
		tbl[2] = *ptbl;
	}
	if (unlikely((pt2 >> 32 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[3] = i8.u32[3] +
			(uint8_t)tbl[3] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		ptbl = (const uint32_t *)&lpm->tbl8[i8.u32[3]];
		tbl[3] = *ptbl;
	}

	hop[0] = (tbl[0] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[0] & 0x00FFFFFF : defv;
	hop[1] = (tbl[1] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[1] & 0x00FFFFFF : defv;
	hop[2] = (tbl[2] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[2] & 0x00FFFFFF : defv;
	hop[3] = (tbl[3] & RTE_LPM_LOOKUP_SUCCESS) ? tbl[3] & 0x00FFFFFF : defv;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_H_ */
