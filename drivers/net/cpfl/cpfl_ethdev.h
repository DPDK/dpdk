/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Intel Corporation
 */

#ifndef _CPFL_ETHDEV_H_
#define _CPFL_ETHDEV_H_

#include <stdint.h>
#include <rte_malloc.h>
#include <rte_spinlock.h>
#include <rte_ethdev.h>
#include <rte_kvargs.h>
#include <rte_hash.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>

#include <idpf_common_device.h>
#include <idpf_common_virtchnl.h>
#include <base/idpf_prototype.h>
#include <base/virtchnl2.h>

#include "cpfl_logs.h"
#include "cpfl_cpchnl.h"
#include "cpfl_representor.h"

/* Currently, backend supports up to 8 vports */
#define CPFL_MAX_VPORT_NUM	8

#define CPFL_INVALID_VPORT_IDX	0xffff

#define CPFL_DFLT_Q_VEC_NUM	1

#define CPFL_MIN_BUF_SIZE	1024
#define CPFL_MAX_FRAME_SIZE	9728
#define CPFL_DEFAULT_MTU	RTE_ETHER_MTU

#define CPFL_VLAN_TAG_SIZE	4
#define CPFL_ETH_OVERHEAD \
	(RTE_ETHER_HDR_LEN + RTE_ETHER_CRC_LEN + CPFL_VLAN_TAG_SIZE * 2)

#define CPFL_RSS_OFFLOAD_ALL (				\
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
		RTE_ETH_RSS_NONFRAG_IPV6_OTHER  |	\
		RTE_ETH_RSS_L2_PAYLOAD)

#define CPFL_ADAPTER_NAME_LEN	(PCI_PRI_STR_SIZE + 1)

#define CPFL_ALARM_INTERVAL	50000 /* us */

/* Device IDs */
#define IDPF_DEV_ID_CPF			0x1453
#define VIRTCHNL2_QUEUE_GROUP_P2P	0x100

#define CPFL_HOST_ID_NUM	2
#define CPFL_PF_TYPE_NUM	2
#define CPFL_HOST_ID_HOST	0
#define CPFL_HOST_ID_ACC	1
#define CPFL_PF_TYPE_APF	0
#define CPFL_PF_TYPE_CPF	1

/* Function IDs on IMC side */
#define CPFL_HOST0_APF		0
#define CPFL_ACC_APF_ID		4
#define CPFL_HOST0_CPF_ID	8
#define CPFL_ACC_CPF_ID		12

#define CPFL_VPORT_LAN_PF	0
#define CPFL_VPORT_LAN_VF	1

/* bit[15:14] type
 * bit[13] host/accelerator core
 * bit[12] apf/cpf
 * bit[11:0] vf
 */
#define CPFL_REPRESENTOR_ID(type, host_id, pf_id, vf_id)	\
	((((type) & 0x3) << 14) + (((host_id) & 0x1) << 13) +	\
	 (((pf_id) & 0x1) << 12) + ((vf_id) & 0xfff))

struct cpfl_vport_param {
	struct cpfl_adapter_ext *adapter;
	uint16_t devarg_id; /* arg id from user */
	uint16_t idx;       /* index in adapter->vports[]*/
};

#define CPFL_REPR_ARG_NUM_MAX	4
/* Struct used when parse driver specific devargs */
struct cpfl_devargs {
	uint16_t req_vports[CPFL_MAX_VPORT_NUM];
	uint16_t req_vport_nb;
	uint8_t repr_args_num;
	struct rte_eth_devargs repr_args[CPFL_REPR_ARG_NUM_MAX];
};

struct p2p_queue_chunks_info {
	uint32_t tx_start_qid;
	uint32_t rx_start_qid;
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

struct cpfl_vport_id {
	uint32_t vport_id;
	uint8_t func_type;
	uint8_t pf_id;
	uint16_t vf_id;
};

struct cpfl_vport_info {
	struct cpchnl2_event_vport_created vport;
	bool enabled;
};

enum cpfl_itf_type {
	CPFL_ITF_TYPE_VPORT,
	CPFL_ITF_TYPE_REPRESENTOR,
};

struct cpfl_itf {
	enum cpfl_itf_type type;
	struct cpfl_adapter_ext *adapter;
	void *data;
};

struct cpfl_vport {
	struct cpfl_itf itf;
	struct idpf_vport base;
	struct p2p_queue_chunks_info *p2p_q_chunks_info;

	struct rte_mempool *p2p_mp;

	uint16_t nb_data_rxq;
	uint16_t nb_data_txq;
	uint16_t nb_p2p_rxq;
	uint16_t nb_p2p_txq;

	struct idpf_rx_queue *p2p_rx_bufq;
	struct idpf_tx_queue *p2p_tx_complq;
	bool p2p_manual_bind;
};

struct cpfl_repr {
	struct cpfl_itf itf;
	struct cpfl_repr_id repr_id;
	struct rte_ether_addr mac_addr;
	struct cpfl_vport_info *vport_info;
};

struct cpfl_adapter_ext {
	TAILQ_ENTRY(cpfl_adapter_ext) next;
	struct idpf_adapter base;

	char name[CPFL_ADAPTER_NAME_LEN];

	struct cpfl_vport **vports;
	uint16_t max_vport_nb;

	uint16_t cur_vports; /* bit mask of created vport */
	uint16_t cur_vport_nb;

	uint16_t used_vecs_num;
	struct cpfl_devargs devargs;

	rte_spinlock_t vport_map_lock;
	struct rte_hash *vport_map_hash;

	rte_spinlock_t repr_lock;
	struct rte_hash *repr_allowlist_hash;
};

TAILQ_HEAD(cpfl_adapter_list, cpfl_adapter_ext);

int cpfl_vport_info_create(struct cpfl_adapter_ext *adapter,
			   struct cpfl_vport_id *vport_identity,
			   struct cpchnl2_event_vport_created *vport);
int cpfl_cc_vport_list_get(struct cpfl_adapter_ext *adapter,
			   struct cpfl_vport_id *vi,
			   struct cpchnl2_get_vport_list_response *response);
int cpfl_cc_vport_info_get(struct cpfl_adapter_ext *adapter,
			   struct cpchnl2_vport_id *vport_id,
			   struct cpfl_vport_id *vi,
			   struct cpchnl2_get_vport_info_response *response);

#define CPFL_DEV_TO_PCI(eth_dev)		\
	RTE_DEV_TO_PCI((eth_dev)->device)
#define CPFL_ADAPTER_TO_EXT(p)					\
	container_of((p), struct cpfl_adapter_ext, base)
#define CPFL_DEV_TO_VPORT(dev)					\
	((struct cpfl_vport *)((dev)->data->dev_private))
#define CPFL_DEV_TO_REPR(dev)					\
	((struct cpfl_repr *)((dev)->data->dev_private))
#define CPFL_DEV_TO_ITF(dev)				\
	((struct cpfl_itf *)((dev)->data->dev_private))

#endif /* _CPFL_ETHDEV_H_ */
