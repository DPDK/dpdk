/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _RTE_ACC100_CFG_H_
#define _RTE_ACC100_CFG_H_

/**
 * @file rte_acc100_cfg.h
 *
 * Functions for configuring ACC100 HW, exposed directly to applications.
 * Configuration related to encoding/decoding is done through the
 * librte_bbdev library.
 *
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 */

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif
/**< Number of Virtual Functions ACC100 supports */
#define RTE_ACC100_NUM_VFS 16

/**
 * Definition of Queue Topology for ACC100 Configuration
 * Some level of details is abstracted out to expose a clean interface
 * given that comprehensive flexibility is not required
 */
struct rte_acc100_queue_topology {
	/** Number of QGroups in incremental order of priority */
	uint16_t num_qgroups;
	/**
	 * All QGroups have the same number of AQs here.
	 * Note : Could be made a 16-array if more flexibility is really
	 * required
	 */
	uint16_t num_aqs_per_groups;
	/**
	 * Depth of the AQs is the same of all QGroups here. Log2 Enum : 2^N
	 * Note : Could be made a 16-array if more flexibility is really
	 * required
	 */
	uint16_t aq_depth_log2;
	/**
	 * Index of the first Queue Group Index - assuming contiguity
	 * Initialized as -1
	 */
	int8_t first_qgroup_index;
};

/**
 * Definition of Arbitration related parameters for ACC100 Configuration
 */
struct rte_acc100_arbitration {
	/** Default Weight for VF Fairness Arbitration */
	uint16_t round_robin_weight;
	uint32_t gbr_threshold1; /**< Guaranteed Bitrate Threshold 1 */
	uint32_t gbr_threshold2; /**< Guaranteed Bitrate Threshold 2 */
};

/**
 * Structure to pass ACC100 configuration.
 * Note: all VF Bundles will have the same configuration.
 */
struct rte_acc100_conf {
	bool pf_mode_en; /**< 1 if PF is used for dataplane, 0 for VFs */
	/** 1 if input '1' bit is represented by a positive LLR value, 0 if '1'
	 * bit is represented by a negative value.
	 */
	bool input_pos_llr_1_bit;
	/** 1 if output '1' bit is represented by a positive value, 0 if '1'
	 * bit is represented by a negative value.
	 */
	bool output_pos_llr_1_bit;
	uint16_t num_vf_bundles; /**< Number of VF bundles to setup */
	/** Queue topology for each operation type */
	struct rte_acc100_queue_topology q_ul_4g;
	struct rte_acc100_queue_topology q_dl_4g;
	struct rte_acc100_queue_topology q_ul_5g;
	struct rte_acc100_queue_topology q_dl_5g;
	/** Arbitration configuration for each operation type */
	struct rte_acc100_arbitration arb_ul_4g[RTE_ACC100_NUM_VFS];
	struct rte_acc100_arbitration arb_dl_4g[RTE_ACC100_NUM_VFS];
	struct rte_acc100_arbitration arb_ul_5g[RTE_ACC100_NUM_VFS];
	struct rte_acc100_arbitration arb_dl_5g[RTE_ACC100_NUM_VFS];
};

/**
 * Configure a ACC100 device
 *
 * @param dev_name
 *   The name of the device. This is the short form of PCI BDF, e.g. 00:01.0.
 *   It can also be retrieved for a bbdev device from the dev_name field in the
 *   rte_bbdev_info structure returned by rte_bbdev_info_get().
 * @param conf
 *   Configuration to apply to ACC100 HW.
 *
 * @return
 *   Zero on success, negative value on failure.
 */
__rte_experimental
int
rte_acc100_configure(const char *dev_name, struct rte_acc100_conf *conf);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ACC100_CFG_H_ */
