/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#ifndef _MRVL_QOS_H_
#define _MRVL_QOS_H_

#include <rte_common.h>

#include "mrvl_ethdev.h"

/** Code Points per Traffic Class. Equals max(DSCP, PCP). */
#define MRVL_CP_PER_TC (64)

/** Value used as "unknown". */
#define MRVL_UNKNOWN_TC (0xFF)

/* QoS config. */
struct mrvl_qos_cfg {
	struct port_cfg {
		int rate_limit_enable;
		struct pp2_ppio_rate_limit_params rate_limit_params;
		struct {
			uint8_t inq[MRVL_PP2_RXQ_MAX];
			uint8_t dscp[MRVL_CP_PER_TC];
			uint8_t pcp[MRVL_CP_PER_TC];
			uint8_t inqs;
			uint8_t dscps;
			uint8_t pcps;
			enum pp2_ppio_color color;
		} tc[MRVL_PP2_TC_MAX];
		struct {
			enum pp2_ppio_outq_sched_mode sched_mode;
			uint8_t weight;
			int rate_limit_enable;
			struct pp2_ppio_rate_limit_params rate_limit_params;
		} outq[MRVL_PP2_RXQ_MAX];
		enum pp2_cls_qos_tbl_type mapping_priority;
		uint16_t inqs;
		uint16_t outqs;
		uint8_t default_tc;
		uint8_t use_global_defaults;
		struct pp2_cls_plcr_params policer_params;
		uint8_t setup_policer;
	} port[RTE_MAX_ETHPORTS];
};

/** Global QoS configuration. */
extern struct mrvl_qos_cfg *mrvl_qos_cfg;

/**
 * Parse QoS configuration - rte_kvargs_process handler.
 *
 * Opens configuration file and parses its content.
 *
 * @param key Unused.
 * @param path Path to config file.
 * @param extra_args Pointer to configuration structure.
 * @returns 0 in case of success, exits otherwise.
 */
int
mrvl_get_qoscfg(const char *key __rte_unused, const char *path,
		void *extra_args);

/**
 * Configure RX Queues in a given port.
 *
 * Sets up RX queues, their Traffic Classes and DPDK rxq->(TC,inq) mapping.
 *
 * @param priv Port's private data
 * @param portid DPDK port ID
 * @param max_queues Maximum number of queues to configure.
 * @returns 0 in case of success, negative value otherwise.
 */
int
mrvl_configure_rxqs(struct mrvl_priv *priv, uint16_t portid,
		    uint16_t max_queues);

/**
 * Configure TX Queues in a given port.
 *
 * Sets up TX queues egress scheduler and limiter.
 *
 * @param priv Port's private data
 * @param portid DPDK port ID
 * @param max_queues Maximum number of queues to configure.
 * @returns 0 in case of success, negative value otherwise.
 */
int
mrvl_configure_txqs(struct mrvl_priv *priv, uint16_t portid,
		    uint16_t max_queues);

/**
 * Start QoS mapping.
 *
 * Finalize QoS table configuration and initialize it in SDK. It can be done
 * only after port is started, so we have a valid ppio reference.
 *
 * @param priv Port's private (configuration) data.
 * @returns 0 in case of success, exits otherwise.
 */
int
mrvl_start_qos_mapping(struct mrvl_priv *priv);

#endif /* _MRVL_QOS_H_ */
