/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _IDPF_COMMON_VIRTCHNL_H_
#define _IDPF_COMMON_VIRTCHNL_H_

#include <idpf_common_device.h>
#include <idpf_common_rxtx.h>

__rte_internal
int idpf_vc_api_version_check(struct idpf_adapter *adapter);
__rte_internal
int idpf_vc_caps_get(struct idpf_adapter *adapter);
__rte_internal
int idpf_vc_vport_create(struct idpf_vport *vport,
			 struct virtchnl2_create_vport *vport_info);
__rte_internal
int idpf_vc_vport_destroy(struct idpf_vport *vport);
__rte_internal
int idpf_vc_rss_key_set(struct idpf_vport *vport);
__rte_internal
int idpf_vc_rss_lut_set(struct idpf_vport *vport);
__rte_internal
int idpf_vc_rss_hash_set(struct idpf_vport *vport);
__rte_internal
int idpf_vc_irq_map_unmap_config(struct idpf_vport *vport,
				 uint16_t nb_rxq, bool map);
__rte_internal
int idpf_vc_cmd_execute(struct idpf_adapter *adapter,
			struct idpf_cmd_info *args);
__rte_internal
int idpf_vc_queue_switch(struct idpf_vport *vport, uint16_t qid,
			 bool rx, bool on);
__rte_internal
int idpf_vc_queues_ena_dis(struct idpf_vport *vport, bool enable);
__rte_internal
int idpf_vc_vport_ena_dis(struct idpf_vport *vport, bool enable);
__rte_internal
int idpf_vc_vectors_alloc(struct idpf_vport *vport, uint16_t num_vectors);
__rte_internal
int idpf_vc_vectors_dealloc(struct idpf_vport *vport);
__rte_internal
int idpf_vc_ptype_info_query(struct idpf_adapter *adapter);
__rte_internal
int idpf_vc_one_msg_read(struct idpf_adapter *adapter, uint32_t ops,
			 uint16_t buf_len, uint8_t *buf);
__rte_internal
int idpf_vc_rxq_config(struct idpf_vport *vport, struct idpf_rx_queue *rxq);
__rte_internal
int idpf_vc_txq_config(struct idpf_vport *vport, struct idpf_tx_queue *txq);
__rte_internal
int idpf_vc_stats_query(struct idpf_vport *vport,
			struct virtchnl2_vport_stats **pstats);
__rte_internal
int idpf_vc_rss_key_get(struct idpf_vport *vport);
__rte_internal
int idpf_vc_rss_lut_get(struct idpf_vport *vport);
__rte_internal
int idpf_vc_rss_hash_get(struct idpf_vport *vport);
__rte_internal
int idpf_vc_ctlq_recv(struct idpf_ctlq_info *cq, u16 *num_q_msg,
		      struct idpf_ctlq_msg *q_msg);
__rte_internal
int idpf_vc_ctlq_post_rx_buffs(struct idpf_hw *hw, struct idpf_ctlq_info *cq,
			   u16 *buff_count, struct idpf_dma_mem **buffs);
#endif /* _IDPF_COMMON_VIRTCHNL_H_ */
