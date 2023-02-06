/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _IDPF_COMMON_DEVICE_H_
#define _IDPF_COMMON_DEVICE_H_

#include <base/idpf_prototype.h>
#include <base/virtchnl2.h>

struct idpf_adapter {
	struct idpf_hw hw;
	struct virtchnl2_version_info virtchnl_version;
	struct virtchnl2_get_capabilities caps;
	volatile uint32_t pend_cmd; /* pending command not finished */
	uint32_t cmd_retval; /* return value of the cmd response from cp */
	uint8_t *mbx_resp; /* buffer to store the mailbox response from cp */
};

struct idpf_chunks_info {
	uint32_t tx_start_qid;
	uint32_t rx_start_qid;
	/* Valid only if split queue model */
	uint32_t tx_compl_start_qid;
	uint32_t rx_buf_start_qid;

	uint64_t tx_qtail_start;
	uint32_t tx_qtail_spacing;
	uint64_t rx_qtail_start;
	uint32_t rx_qtail_spacing;
	uint64_t tx_compl_qtail_start;
	uint32_t tx_compl_qtail_spacing;
	uint64_t rx_buf_qtail_start;
	uint32_t rx_buf_qtail_spacing;
};

struct idpf_vport {
	struct idpf_adapter *adapter; /* Backreference to associated adapter */
	struct virtchnl2_create_vport *vport_info; /* virtchnl response info handling */
	uint16_t sw_idx; /* SW index in adapter->vports[]*/
	uint16_t vport_id;
	uint32_t txq_model;
	uint32_t rxq_model;
	uint16_t num_tx_q;
	/* valid only if txq_model is split Q */
	uint16_t num_tx_complq;
	uint16_t num_rx_q;
	/* valid only if rxq_model is split Q */
	uint16_t num_rx_bufq;

	uint16_t max_mtu;
	uint8_t default_mac_addr[VIRTCHNL_ETH_LENGTH_OF_ADDRESS];

	enum virtchnl_rss_algorithm rss_algorithm;
	uint16_t rss_key_size;
	uint16_t rss_lut_size;

	void *dev_data; /* Pointer to the device data */
	uint16_t max_pkt_len; /* Maximum packet length */

	/* RSS info */
	uint32_t *rss_lut;
	uint8_t *rss_key;
	uint64_t rss_hf;

	/* MSIX info*/
	struct virtchnl2_queue_vector *qv_map; /* queue vector mapping */
	uint16_t max_vectors;
	struct virtchnl2_alloc_vectors *recv_vectors;

	/* Chunk info */
	struct idpf_chunks_info chunks_info;

	uint16_t devarg_id;

	bool stopped;
};

#endif /* _IDPF_COMMON_DEVICE_H_ */
