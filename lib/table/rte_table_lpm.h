/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_TABLE_LPM_H__
#define __INCLUDE_RTE_TABLE_LPM_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Table LPM for IPv4
 *
 * This table uses the Longest Prefix Match (LPM) algorithm to uniquely
 * associate data to lookup keys.
 *
 * Use-case: IP routing table. Routes that are added to the table associate a
 * next hop to an IP prefix. The IP prefix is specified as IP address and depth
 * and cover for a multitude of lookup keys (i.e. destination IP addresses)
 * that all share the same data (i.e. next hop). The next hop information
 * typically contains the output interface ID, the IP address of the next hop
 * station (which is part of the same IP network the output interface is
 * connected to) and other flags and counters.
 *
 * The LPM primitive only allows associating an 8-bit number (next hop ID) to
 * an IP prefix, while a routing table can potentially contain thousands of
 * routes or even more. This means that the same next hop ID (and next hop
 * information) has to be shared by multiple routes, which makes sense, as
 * multiple remote networks could be reached through the same next hop.
 * Therefore, when a route is added or updated, the LPM table has to check
 * whether the same next hop is already in use before using a new next hop ID
 * for this route.
 *
 * The comparison between different next hops is done for the first
 * “entry_unique_size” bytes of the next hop information (configurable
 * parameter), which have to uniquely identify the next hop, therefore the user
 * has to carefully manage the format of the LPM table entry (i.e.  the next
 * hop information) so that any next hop data that changes value during
 * run-time (e.g. counters) is placed outside of this area.
 *
 ***/

#include <stdint.h>

#include "rte_table.h"

/** LPM table parameters */
struct rte_table_lpm_params {
	/** Table name */
	const char *name;

	/** Maximum number of LPM rules (i.e. IP routes) */
	uint32_t n_rules;

	/**< Number of tbl8s to allocate. */
	uint32_t number_tbl8s;

	/**< This field is currently unused. */
	int flags;

	/** Number of bytes at the start of the table entry that uniquely
	identify the entry. Cannot be bigger than table entry size. */
	uint32_t entry_unique_size;

	/** Byte offset within input packet meta-data where lookup key (i.e.
	the destination IP address) is located. */
	uint32_t offset;
};

/** LPM table rule (i.e. route), specified as IP prefix. While the key used by
the lookup operation is the destination IP address (read from the input packet
meta-data), the entry add and entry delete operations work with LPM rules, with
each rule covering for a multitude of lookup keys (destination IP addresses)
that share the same data (next hop). */
struct rte_table_lpm_key {
	/** IP address */
	uint32_t ip;

	/** IP address depth. The most significant "depth" bits of the IP
	address specify the network part of the IP address, while the rest of
	the bits specify the host part of the address and are ignored for the
	purpose of route specification. */
	uint8_t depth;
};

/** LPM table operations */
extern struct rte_table_ops rte_table_lpm_ops;

#ifdef __cplusplus
}
#endif

#endif
