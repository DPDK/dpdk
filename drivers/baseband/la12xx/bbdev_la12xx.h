/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020-2021 NXP
 */

#ifndef __BBDEV_LA12XX_H__
#define __BBDEV_LA12XX_H__

#define MAX_CHANNEL_DEPTH 16
/* private data structure */
struct bbdev_la12xx_private {
	ipc_userspace_t *ipc_priv;
	uint8_t num_valid_queues;
	uint8_t max_nb_queues;
	uint8_t num_ldpc_enc_queues;
	uint8_t num_ldpc_dec_queues;
	int8_t modem_id;
	struct bbdev_la12xx_q_priv *queues_priv[32];
};

struct hugepage_info {
	void *vaddr;
	phys_addr_t paddr;
	size_t len;
};

struct bbdev_la12xx_q_priv {
	struct bbdev_la12xx_private *bbdev_priv;
	uint32_t q_id;	/**< Channel ID */
	uint32_t feca_blk_id;	/**< FECA block ID for processing */
	uint32_t feca_blk_id_be32; /**< FECA Block ID for this queue */
	uint8_t en_napi; /**< 0: napi disabled, 1: napi enabled */
	uint16_t queue_size;	/**< Queue depth */
	int32_t eventfd;	/**< Event FD value */
	enum rte_bbdev_op_type op_type; /**< Operation type */
	uint32_t la12xx_core_id;
		/**< LA12xx core ID on which this will be scheduled */
	struct rte_mempool *mp; /**< Pool from where buffers would be cut */
	void *bbdev_op[MAX_CHANNEL_DEPTH];
			/**< Stores bbdev op for each index */
	void *msg_ch_vaddr[MAX_CHANNEL_DEPTH];
			/**< Stores msg channel addr for modem->host */
	uint32_t host_pi;	/**< Producer_Index for HOST->MODEM */
	uint32_t host_ci;	/**< Consumer Index for MODEM->HOST */
	host_ipc_params_t *host_params; /**< Host parameters */
};

#define lower_32_bits(x) ((uint32_t)((uint64_t)x))
#define upper_32_bits(x) ((uint32_t)(((uint64_t)(x) >> 16) >> 16))
#define join_32_bits(upper, lower) \
	((size_t)(((uint64_t)(upper) << 32) | (uint32_t)(lower)))
#endif
