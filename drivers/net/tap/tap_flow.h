/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2017 6WIND S.A.
 * Copyright 2017 Mellanox Technologies, Ltd
 */

#ifndef _TAP_FLOW_H_
#define _TAP_FLOW_H_

#include <rte_flow.h>
#include <rte_flow_driver.h>
#include <rte_eth_tap.h>
#include <tap_autoconf.h>

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
#define RSS_PRIORITY_OFFSET RTE_PMD_TAP_MAX_QUEUES

/**
 * These index are actually in reversed order: their priority is processed
 * by subtracting their value to the lowest priority (PRIORITY_MASK).
 * Thus the first one will have the lowest priority in the end
 * (but biggest value).
 */
enum implicit_rule_index {
	TAP_REMOTE_TX,
	TAP_ISOLATE,
	TAP_REMOTE_BROADCASTV6,
	TAP_REMOTE_BROADCAST,
	TAP_REMOTE_ALLMULTI,
	TAP_REMOTE_PROMISC,
	TAP_REMOTE_LOCAL_MAC,
	TAP_REMOTE_MAX_IDX,
};

enum bpf_fd_idx {
	SEC_L3_L4,
	SEC_MAX,
};

int tap_dev_flow_ops_get(struct rte_eth_dev *dev,
			 const struct rte_flow_ops **ops);
int tap_flow_flush(struct rte_eth_dev *dev, struct rte_flow_error *error);

int tap_flow_implicit_create(struct pmd_internals *pmd,
			     enum implicit_rule_index idx);
int tap_flow_implicit_destroy(struct pmd_internals *pmd,
			      enum implicit_rule_index idx);
int tap_flow_implicit_flush(struct pmd_internals *pmd,
			    struct rte_flow_error *error);

int tap_flow_bpf_cls_q(__u32 queue_idx);
int tap_flow_bpf_calc_l3_l4_hash(__u32 key_idx, int map_fd);
int tap_flow_bpf_rss_map_create(unsigned int key_size, unsigned int value_size,
			unsigned int max_entries);
int tap_flow_bpf_update_rss_elem(int fd, void *key, void *value);

#endif /* _TAP_FLOW_H_ */
