/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef _IDPF_ETHDEV_H_
#define _IDPF_ETHDEV_H_

#include <stdint.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include <rte_ethdev.h>
#include <rte_kvargs.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>

#include "idpf_logs.h"

#include <base/idpf_prototype.h>
#include <base/virtchnl2.h>

#define IDPF_MAX_VPORT_NUM	8

#define IDPF_DEFAULT_RXQ_NUM	16
#define IDPF_DEFAULT_TXQ_NUM	16

#define IDPF_INVALID_VPORT_IDX	0xffff
#define IDPF_TXQ_PER_GRP	1
#define IDPF_TX_COMPLQ_PER_GRP	1
#define IDPF_RXQ_PER_GRP	1
#define IDPF_RX_BUFQ_PER_GRP	2

#define IDPF_CTLQ_ID		-1
#define IDPF_CTLQ_LEN		64
#define IDPF_DFLT_MBX_BUF_SIZE	4096

#define IDPF_DFLT_Q_VEC_NUM	1
#define IDPF_DFLT_INTERVAL	16

#define IDPF_MIN_BUF_SIZE	1024
#define IDPF_MAX_FRAME_SIZE	9728
#define IDPF_MIN_FRAME_SIZE	14

#define IDPF_NUM_MACADDR_MAX	64

#define IDPF_MAX_PKT_TYPE	1024

#define IDPF_VLAN_TAG_SIZE	4
#define IDPF_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + IDPF_VLAN_TAG_SIZE * 2)

#define IDPF_RSS_OFFLOAD_ALL (				\
		RTE_ETH_RSS_IPV4                |	\
		RTE_ETH_RSS_FRAG_IPV4           |	\
		RTE_ETH_RSS_NONFRAG_IPV4_TCP    |	\
		RTE_ETH_RSS_NONFRAG_IPV4_UDP    |	\
		RTE_ETH_RSS_NONFRAG_IPV4_SCTP   |	\
		RTE_ETH_RSS_NONFRAG_IPV4_OTHER  |	\
		RTE_ETH_RSS_IPV6                |	\
		RTE_ETH_RSS_FRAG_IPV6           |	\
		RTE_ETH_RSS_NONFRAG_IPV6_TCP    |	\
		RTE_ETH_RSS_NONFRAG_IPV6_UDP    |	\
		RTE_ETH_RSS_NONFRAG_IPV6_SCTP   |	\
		RTE_ETH_RSS_NONFRAG_IPV6_OTHER)

#define IDPF_ADAPTER_NAME_LEN	(PCI_PRI_STR_SIZE + 1)

/* Message type read in virtual channel from PF */
enum idpf_vc_result {
	IDPF_MSG_ERR = -1, /* Meet error when accessing admin queue */
	IDPF_MSG_NON,      /* Read nothing from admin queue */
	IDPF_MSG_SYS,      /* Read system msg from admin queue */
	IDPF_MSG_CMD,      /* Read async command result */
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

	uint16_t sw_idx; /* SW idx */

	struct rte_eth_dev_data *dev_data; /* Pointer to the device data */
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

struct idpf_adapter {
	TAILQ_ENTRY(idpf_adapter) next;
	struct idpf_hw hw;
	char name[IDPF_ADAPTER_NAME_LEN];

	struct virtchnl2_version_info virtchnl_version;
	struct virtchnl2_get_capabilities *caps;

	volatile uint32_t pend_cmd; /* pending command not finished */
	uint32_t cmd_retval; /* return value of the cmd response from ipf */
	uint8_t *mbx_resp; /* buffer to store the mailbox response from ipf */

	uint32_t txq_model; /* 0 - split queue model, non-0 - single queue model */
	uint32_t rxq_model; /* 0 - split queue model, non-0 - single queue model */

	/* Vport info */
	uint8_t **vport_req_info;
	uint8_t **vport_recv_info;
	struct idpf_vport **vports;
	uint16_t max_vport_nb;
	uint16_t req_vports[IDPF_MAX_VPORT_NUM];
	uint16_t req_vport_nb;
	uint16_t cur_vports;
	uint16_t cur_vport_nb;
	uint16_t cur_vport_idx;

	uint16_t used_vecs_num;

	/* Max config queue number per VC message */
	uint32_t max_rxq_per_msg;
	uint32_t max_txq_per_msg;

	uint32_t ptype_tbl[IDPF_MAX_PKT_TYPE] __rte_cache_min_aligned;

	bool rx_vec_allowed;
	bool tx_vec_allowed;
	bool rx_use_avx512;
	bool tx_use_avx512;

	/* For PTP */
	uint64_t time_hw;
};

TAILQ_HEAD(idpf_adapter_list, idpf_adapter);

#define IDPF_DEV_TO_PCI(eth_dev)		\
	RTE_DEV_TO_PCI((eth_dev)->device)

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
		PMD_DRV_LOG(ERR, "There is incomplete cmd %d", adapter->pend_cmd);

	return !ret;
}

struct idpf_adapter *idpf_find_adapter(struct rte_pci_device *pci_dev);
void idpf_handle_virtchnl_msg(struct rte_eth_dev *dev);
int idpf_vc_check_api_version(struct idpf_adapter *adapter);
int idpf_get_pkt_type(struct idpf_adapter *adapter);
int idpf_vc_get_caps(struct idpf_adapter *adapter);
int idpf_vc_create_vport(struct idpf_adapter *adapter);
int idpf_vc_destroy_vport(struct idpf_vport *vport);
int idpf_vc_set_rss_key(struct idpf_vport *vport);
int idpf_vc_set_rss_lut(struct idpf_vport *vport);
int idpf_vc_set_rss_hash(struct idpf_vport *vport);
int idpf_vc_config_rxqs(struct idpf_vport *vport);
int idpf_vc_config_rxq(struct idpf_vport *vport, uint16_t rxq_id);
int idpf_vc_config_txqs(struct idpf_vport *vport);
int idpf_vc_config_txq(struct idpf_vport *vport, uint16_t txq_id);
int idpf_switch_queue(struct idpf_vport *vport, uint16_t qid,
		      bool rx, bool on);
int idpf_vc_ena_dis_queues(struct idpf_vport *vport, bool enable);
int idpf_vc_ena_dis_vport(struct idpf_vport *vport, bool enable);
int idpf_vc_config_irq_map_unmap(struct idpf_vport *vport, bool map);
int idpf_vc_alloc_vectors(struct idpf_vport *vport, uint16_t num_vectors);
int idpf_vc_dealloc_vectors(struct idpf_vport *vport);
int idpf_vc_query_ptype_info(struct idpf_adapter *adapter);
int idpf_read_one_msg(struct idpf_adapter *adapter, uint32_t ops,
		      uint16_t buf_len, uint8_t *buf);

#endif /* _IDPF_ETHDEV_H_ */
