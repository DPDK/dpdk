/*-
 *   BSD LICENSE
 *
 *   Copyright 2017 6WIND S.A.
 *   Copyright 2017 Mellanox.
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
 *     * Neither the name of 6WIND S.A. nor the names of its
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

#ifndef _TAP_FLOW_H_
#define _TAP_FLOW_H_

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_eth_tap.h>

/**
 * In TC, priority 0 means we require the kernel to allocate one for us.
 * In rte_flow, however, we want the priority 0 to be the most important one.
 * Use an offset to have the most important priority being 1 in TC.
 */
#define PRIORITY_OFFSET 1
#define PRIORITY_MASK (0xfff)
#define MAX_PRIORITY (PRIORITY_MASK - PRIORITY_OFFSET)
#define GROUP_MASK (0xf)
#define GROUP_SHIFT 12
#define MAX_GROUP GROUP_MASK

/**
 * These index are actually in reversed order: their priority is processed
 * by subtracting their value to the lowest priority (PRIORITY_MASK).
 * Thus the first one will have the lowest priority in the end
 * (but biggest value).
 */
enum implicit_rule_index {
	TAP_REMOTE_TX,
	TAP_REMOTE_BROADCASTV6,
	TAP_REMOTE_BROADCAST,
	TAP_REMOTE_ALLMULTI,
	TAP_REMOTE_PROMISC,
	TAP_REMOTE_LOCAL_MAC,
	TAP_REMOTE_MAX_IDX,
};

int tap_dev_filter_ctrl(struct rte_eth_dev *dev,
			enum rte_filter_type filter_type,
			enum rte_filter_op filter_op,
			void *arg);
int tap_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error);

int tap_flow_implicit_create(struct pmd_internals *pmd,
			     enum implicit_rule_index idx);
int tap_flow_implicit_destroy(struct pmd_internals *pmd,
			      enum implicit_rule_index idx);
int tap_flow_implicit_flush(struct pmd_internals *pmd,
			    struct rte_flow_error *error);

#endif /* _TAP_FLOW_H_ */
