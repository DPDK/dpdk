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

#ifndef __INCLUDE_RTE_TABLE_LPM_IPV6_H__
#define __INCLUDE_RTE_TABLE_LPM_IPV6_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Table LPM for IPv6
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

#define RTE_LPM_IPV6_ADDR_SIZE 16

/** LPM table parameters */
struct rte_table_lpm_ipv6_params {
	/** Table name */
	const char *name;

	/** Maximum number of LPM rules (i.e. IP routes) */
	uint32_t n_rules;

	uint32_t number_tbl8s;

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
struct rte_table_lpm_ipv6_key {
	/** IP address */
	uint8_t ip[RTE_LPM_IPV6_ADDR_SIZE];

	/** IP address depth. The most significant "depth" bits of the IP
	address specify the network part of the IP address, while the rest of
	the bits specify the host part of the address and are ignored for the
	purpose of route specification. */
	uint8_t depth;
};

/** LPM table operations */
extern struct rte_table_ops rte_table_lpm_ipv6_ops;

#ifdef __cplusplus
}
#endif

#endif
