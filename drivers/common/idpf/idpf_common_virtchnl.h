/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _IDPF_COMMON_VIRTCHNL_H_
#define _IDPF_COMMON_VIRTCHNL_H_

#include <idpf_common_device.h>

__rte_internal
int idpf_vc_check_api_version(struct idpf_adapter *adapter);
__rte_internal
int idpf_vc_get_caps(struct idpf_adapter *adapter);
__rte_internal
int idpf_vc_create_vport(struct idpf_vport *vport,
			 struct virtchnl2_create_vport *vport_info);
__rte_internal
int idpf_vc_destroy_vport(struct idpf_vport *vport);
__rte_internal
int idpf_vc_set_rss_key(struct idpf_vport *vport);
__rte_internal
int idpf_vc_set_rss_lut(struct idpf_vport *vport);
__rte_internal
int idpf_vc_set_rss_hash(struct idpf_vport *vport);
__rte_internal
int idpf_vc_switch_queue(struct idpf_vport *vport, uint16_t qid,
			 bool rx, bool on);
__rte_internal
int idpf_vc_ena_dis_queues(struct idpf_vport *vport, bool enable);
__rte_internal
int idpf_vc_ena_dis_vport(struct idpf_vport *vport, bool enable);
__rte_internal
int idpf_vc_config_irq_map_unmap(struct idpf_vport *vport,
				 uint16_t nb_rxq, bool map);
__rte_internal
int idpf_vc_alloc_vectors(struct idpf_vport *vport, uint16_t num_vectors);
__rte_internal
int idpf_vc_dealloc_vectors(struct idpf_vport *vport);
__rte_internal
int idpf_vc_query_ptype_info(struct idpf_adapter *adapter);
__rte_internal
int idpf_vc_read_one_msg(struct idpf_adapter *adapter, uint32_t ops,
			 uint16_t buf_len, uint8_t *buf);
__rte_internal
int idpf_execute_vc_cmd(struct idpf_adapter *adapter,
			struct idpf_cmd_info *args);

#endif /* _IDPF_COMMON_VIRTCHNL_H_ */
