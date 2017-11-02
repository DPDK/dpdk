/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Marvell International Ltd.
 *   Copyright(c) 2017 Semihalf.
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
 *     * Neither the name of the copyright holder nor the names of its
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
		struct {
			uint8_t inq[MRVL_PP2_RXQ_MAX];
			uint8_t dscp[MRVL_CP_PER_TC];
			uint8_t pcp[MRVL_CP_PER_TC];
			uint8_t inqs;
			uint8_t dscps;
			uint8_t pcps;
		} tc[MRVL_PP2_TC_MAX];
		struct {
			uint8_t weight;
		} outq[MRVL_PP2_RXQ_MAX];
		enum pp2_cls_qos_tbl_type mapping_priority;
		uint16_t inqs;
		uint16_t outqs;
		uint8_t default_tc;
		uint8_t use_global_defaults;
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
