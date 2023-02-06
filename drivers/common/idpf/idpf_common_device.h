/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _IDPF_COMMON_DEVICE_H_
#define _IDPF_COMMON_DEVICE_H_

#include <base/idpf_prototype.h>
#include <base/virtchnl2.h>
#include <idpf_common_logs.h>

#define IDPF_RSS_KEY_LEN	52

#define IDPF_CTLQ_ID		-1
#define IDPF_CTLQ_LEN		64
#define IDPF_DFLT_MBX_BUF_SIZE	4096

#define IDPF_MAX_PKT_TYPE	1024

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
	union {
		struct virtchnl2_create_vport info; /* virtchnl response info handling */
		uint8_t data[IDPF_DFLT_MBX_BUF_SIZE];
	} vport_info;
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

/* Message type read in virtual channel from PF */
enum idpf_vc_result {
	IDPF_MSG_ERR = -1, /* Meet error when accessing admin queue */
	IDPF_MSG_NON,      /* Read nothing from admin queue */
	IDPF_MSG_SYS,      /* Read system msg from admin queue */
	IDPF_MSG_CMD,      /* Read async command result */
};

/* structure used for sending and checking response of virtchnl ops */
struct idpf_cmd_info {
	uint32_t ops;
	uint8_t *in_args;       /* buffer for sending */
	uint32_t in_args_size;  /* buffer size for sending */
	uint8_t *out_buffer;    /* buffer for response */
	uint32_t out_size;      /* buffer size for response */
};

/* notify current command done. Only call in case execute
 * _atomic_set_cmd successfully.
 */
static inline void
notify_cmd(struct idpf_adapter *adapter, int msg_ret)
{
	adapter->cmd_retval = msg_ret;
	/* Return value may be checked in anither thread, need to ensure the coherence. */
	rte_wmb();
	adapter->pend_cmd = VIRTCHNL2_OP_UNKNOWN;
}

/* clear current command. Only call in case execute
 * _atomic_set_cmd successfully.
 */
static inline void
clear_cmd(struct idpf_adapter *adapter)
{
	/* Return value may be checked in anither thread, need to ensure the coherence. */
	rte_wmb();
	adapter->pend_cmd = VIRTCHNL2_OP_UNKNOWN;
	adapter->cmd_retval = VIRTCHNL_STATUS_SUCCESS;
}

/* Check there is pending cmd in execution. If none, set new command. */
static inline bool
atomic_set_cmd(struct idpf_adapter *adapter, uint32_t ops)
{
	uint32_t op_unk = VIRTCHNL2_OP_UNKNOWN;
	bool ret = __atomic_compare_exchange(&adapter->pend_cmd, &op_unk, &ops,
					    0, __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);

	if (!ret)
		DRV_LOG(ERR, "There is incomplete cmd %d", adapter->pend_cmd);

	return !ret;
}

__rte_internal
int idpf_adapter_init(struct idpf_adapter *adapter);
__rte_internal
int idpf_adapter_deinit(struct idpf_adapter *adapter);
__rte_internal
int idpf_vport_init(struct idpf_vport *vport,
		    struct virtchnl2_create_vport *vport_req_info,
		    void *dev_data);
__rte_internal
int idpf_vport_deinit(struct idpf_vport *vport);

#endif /* _IDPF_COMMON_DEVICE_H_ */
