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
#include <rte_memory.h>
#include <rte_common.h>
#include <rte_common_vect.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Max number of characters in LPM name. */
#define RTE_LPM_NAMESIZE                32

/** @deprecated Possible location to allocate memory. This was for last
 * parameter of rte_lpm_create(), but is now redundant. The LPM table is always
 * allocated in memory using librte_malloc which uses a memzone. */
#define RTE_LPM_HEAP                    0

/** @deprecated Possible location to allocate memory. This was for last
 * parameter of rte_lpm_create(), but is now redundant. The LPM table is always
 * allocated in memory using librte_malloc which uses a memzone. */
#define RTE_LPM_MEMZONE                 1

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

/** @internal bitmask with valid and ext_entry/valid_group fields set */
#define RTE_LPM_VALID_EXT_ENTRY_BITMASK 0x0300

/** Bitmask used to indicate successful lookup */
#define RTE_LPM_LOOKUP_SUCCESS          0x0100

/** @internal Tbl24 entry structure. */
struct rte_lpm_tbl24_entry {
	/* Stores Next hop or group index (i.e. gindex)into tbl8. */
	union {
		uint8_t next_hop;
		uint8_t tbl8_gindex;
	};
	/* Using single uint8_t to store 3 values. */
	uint8_t valid     :1; /**< Validation flag. */
	uint8_t ext_entry :1; /**< External entry. */
	uint8_t depth     :6; /**< Rule depth. */
};

/** @internal Tbl8 entry structure. */
struct rte_lpm_tbl8_entry {
	uint8_t next_hop; /**< next hop. */
	/* Using single uint8_t to store 3 values. */
	uint8_t valid       :1; /**< Validation flag. */
	uint8_t valid_group :1; /**< Group validation flag. */
	uint8_t depth       :6; /**< Rule depth. */
};

/** @internal Rule structure. */
struct rte_lpm_rule {
	uint32_t ip; /**< Rule IP address. */
	uint8_t  next_hop; /**< Rule next hop. */
};

/** @internal Contains metadata about the rules table. */
struct rte_lpm_rule_info {
	uint32_t used_rules; /**< Used rules so far. */
	uint32_t first_rule; /**< Indexes the first rule of a given depth. */
};

/** @internal LPM structure. */
struct rte_lpm {
	TAILQ_ENTRY(rte_lpm) next;      /**< Next in list. */

	/* LPM metadata. */
	char name[RTE_LPM_NAMESIZE];        /**< Name of the lpm. */
	int mem_location; /**< @deprecated @see RTE_LPM_HEAP and RTE_LPM_MEMZONE. */
	uint32_t max_rules; /**< Max. balanced rules per lpm. */
	struct rte_lpm_rule_info rule_info[RTE_LPM_MAX_DEPTH]; /**< Rule info table. */

	/* LPM Tables. */
	struct rte_lpm_tbl24_entry tbl24[RTE_LPM_TBL24_NUM_ENTRIES] \
			__rte_cache_aligned; /**< LPM tbl24 table. */
	struct rte_lpm_tbl8_entry tbl8[RTE_LPM_TBL8_NUM_ENTRIES] \
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
 *    - E_RTE_NO_TAILQ - no tailq list could be got for the lpm object list
 *    - EINVAL - invalid parameter passed to function
 *    - ENOSPC - the maximum number of memzones has already been allocated
 *    - EEXIST - a memzone with the same name already exists
 *    - ENOMEM - no appropriate memory area found in which to create memzone
 */
struct rte_lpm *
rte_lpm_create(const char *name, int socket_id, int max_rules, int flags);

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
rte_lpm_add(struct rte_lpm *lpm, uint32_t ip, uint8_t depth, uint8_t next_hop);

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
uint8_t *next_hop);

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

/**
 * Delete all rules from the LPM table.
 *
 * @param lpm
 *   LPM object handle
 */
void
rte_lpm_delete_all(struct rte_lpm *lpm);

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
rte_lpm_lookup(struct rte_lpm *lpm, uint32_t ip, uint8_t *next_hop)
{
	unsigned tbl24_index = (ip >> 8);
	uint16_t tbl_entry;

	/* DEBUG: Check user input arguments. */
	RTE_LPM_RETURN_IF_TRUE(((lpm == NULL) || (next_hop == NULL)), -EINVAL);

	/* Copy tbl24 entry */
	tbl_entry = *(const uint16_t *)&lpm->tbl24[tbl24_index];

	/* Copy tbl8 entry (only if needed) */
	if (unlikely((tbl_entry & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {

		unsigned tbl8_index = (uint8_t)ip +
				((uint8_t)tbl_entry * RTE_LPM_TBL8_GROUP_NUM_ENTRIES);

		tbl_entry = *(const uint16_t *)&lpm->tbl8[tbl8_index];
	}

	*next_hop = (uint8_t)tbl_entry;
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
rte_lpm_lookup_bulk_func(const struct rte_lpm *lpm, const uint32_t * ips,
		uint16_t * next_hops, const unsigned n)
{
	unsigned i;
	unsigned tbl24_indexes[n];

	/* DEBUG: Check user input arguments. */
	RTE_LPM_RETURN_IF_TRUE(((lpm == NULL) || (ips == NULL) ||
			(next_hops == NULL)), -EINVAL);

	for (i = 0; i < n; i++) {
		tbl24_indexes[i] = ips[i] >> 8;
	}

	for (i = 0; i < n; i++) {
		/* Simply copy tbl24 entry to output */
		next_hops[i] = *(const uint16_t *)&lpm->tbl24[tbl24_indexes[i]];

		/* Overwrite output with tbl8 entry if needed */
		if (unlikely((next_hops[i] & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
				RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {

			unsigned tbl8_index = (uint8_t)ips[i] +
					((uint8_t)next_hops[i] *
					 RTE_LPM_TBL8_GROUP_NUM_ENTRIES);

			next_hops[i] = *(const uint16_t *)&lpm->tbl8[tbl8_index];
		}
	}
	return 0;
}

/* Mask four results. */
#define	 RTE_LPM_MASKX4_RES	UINT64_C(0x00ff00ff00ff00ff)

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
rte_lpm_lookupx4(const struct rte_lpm *lpm, __m128i ip, uint16_t hop[4],
	uint16_t defv)
{
	__m128i i24;
	rte_xmm_t i8;
	uint16_t tbl[4];
	uint64_t idx, pt;

	const __m128i mask8 =
		_mm_set_epi32(UINT8_MAX, UINT8_MAX, UINT8_MAX, UINT8_MAX);

	/*
	 * RTE_LPM_VALID_EXT_ENTRY_BITMASK for 4 LPM entries
	 * as one 64-bit value (0x0300030003000300).
	 */
	const uint64_t mask_xv =
		((uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK |
		(uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK << 16 |
		(uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK << 32 |
		(uint64_t)RTE_LPM_VALID_EXT_ENTRY_BITMASK << 48);

	/*
	 * RTE_LPM_LOOKUP_SUCCESS for 4 LPM entries
	 * as one 64-bit value (0x0100010001000100).
	 */
	const uint64_t mask_v =
		((uint64_t)RTE_LPM_LOOKUP_SUCCESS |
		(uint64_t)RTE_LPM_LOOKUP_SUCCESS << 16 |
		(uint64_t)RTE_LPM_LOOKUP_SUCCESS << 32 |
		(uint64_t)RTE_LPM_LOOKUP_SUCCESS << 48);

	/* get 4 indexes for tbl24[]. */
	i24 = _mm_srli_epi32(ip, CHAR_BIT);

	/* extract values from tbl24[] */
	idx = _mm_cvtsi128_si64(i24);
	i24 = _mm_srli_si128(i24, sizeof(uint64_t));

	tbl[0] = *(const uint16_t *)&lpm->tbl24[(uint32_t)idx];
	tbl[1] = *(const uint16_t *)&lpm->tbl24[idx >> 32];

	idx = _mm_cvtsi128_si64(i24);

	tbl[2] = *(const uint16_t *)&lpm->tbl24[(uint32_t)idx];
	tbl[3] = *(const uint16_t *)&lpm->tbl24[idx >> 32];

	/* get 4 indexes for tbl8[]. */
	i8.m = _mm_and_si128(ip, mask8);

	pt = (uint64_t)tbl[0] |
		(uint64_t)tbl[1] << 16 |
		(uint64_t)tbl[2] << 32 |
		(uint64_t)tbl[3] << 48;

	/* search successfully finished for all 4 IP addresses. */
	if (likely((pt & mask_xv) == mask_v)) {
		uintptr_t ph = (uintptr_t)hop;
		*(uint64_t *)ph = pt & RTE_LPM_MASKX4_RES;
		return;
	}

	if (unlikely((pt & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[0] = i8.u32[0] +
			(uint8_t)tbl[0] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl[0] = *(const uint16_t *)&lpm->tbl8[i8.u32[0]];
	}
	if (unlikely((pt >> 16 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[1] = i8.u32[1] +
			(uint8_t)tbl[1] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl[1] = *(const uint16_t *)&lpm->tbl8[i8.u32[1]];
	}
	if (unlikely((pt >> 32 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[2] = i8.u32[2] +
			(uint8_t)tbl[2] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl[2] = *(const uint16_t *)&lpm->tbl8[i8.u32[2]];
	}
	if (unlikely((pt >> 48 & RTE_LPM_VALID_EXT_ENTRY_BITMASK) ==
			RTE_LPM_VALID_EXT_ENTRY_BITMASK)) {
		i8.u32[3] = i8.u32[3] +
			(uint8_t)tbl[3] * RTE_LPM_TBL8_GROUP_NUM_ENTRIES;
		tbl[3] = *(const uint16_t *)&lpm->tbl8[i8.u32[3]];
	}

	hop[0] = (tbl[0] & RTE_LPM_LOOKUP_SUCCESS) ? (uint8_t)tbl[0] : defv;
	hop[1] = (tbl[1] & RTE_LPM_LOOKUP_SUCCESS) ? (uint8_t)tbl[1] : defv;
	hop[2] = (tbl[2] & RTE_LPM_LOOKUP_SUCCESS) ? (uint8_t)tbl[2] : defv;
	hop[3] = (tbl[3] & RTE_LPM_LOOKUP_SUCCESS) ? (uint8_t)tbl[3] : defv;
}

#ifdef __cplusplus
}
#endif

#endif /* _RTE_LPM_H_ */
