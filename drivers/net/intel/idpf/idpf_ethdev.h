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

#include <idpf_common_device.h>
#include <idpf_common_virtchnl.h>
#include <base/idpf_prototype.h>
#include <base/virtchnl2.h>

#define IDPF_MAX_VPORT_NUM	8

#define IDPF_INVALID_VPORT_IDX	0xffff

#define IDPF_DFLT_Q_VEC_NUM	1

#define IDPF_MIN_BUF_SIZE	1024
#define IDPF_MAX_FRAME_SIZE	9728
#define IDPF_DEFAULT_MTU	RTE_ETHER_MTU

#define IDPF_NUM_MACADDR_MAX	64

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
		RTE_ETH_RSS_NONFRAG_IPV6_OTHER  |	\
		RTE_ETH_RSS_L2_PAYLOAD)

#define IDPF_ADAPTER_NAME_LEN	(PCI_PRI_STR_SIZE + 1)

#define IDPF_ALARM_INTERVAL	50000 /* us */

struct idpf_vport_param {
	struct idpf_adapter_ext *adapter;
	uint16_t devarg_id; /* arg id from user */
	uint16_t idx;       /* index in adapter->vports[]*/
};

/* Struct used when parse driver specific devargs */
struct idpf_devargs {
	uint16_t req_vports[IDPF_MAX_VPORT_NUM];
	uint16_t req_vport_nb;
};

struct idpf_adapter_ext {
	TAILQ_ENTRY(idpf_adapter_ext) next;
	struct idpf_adapter base;

	char name[IDPF_ADAPTER_NAME_LEN];

	struct idpf_vport **vports;
	uint16_t max_vport_nb;

	uint16_t cur_vports; /* bit mask of created vport */
	uint16_t cur_vport_nb;

	uint16_t used_vecs_num;
};

TAILQ_HEAD(idpf_adapter_list, idpf_adapter_ext);

#define IDPF_DEV_TO_PCI(eth_dev)		\
	RTE_DEV_TO_PCI((eth_dev)->device)
#define IDPF_ADAPTER_TO_EXT(p)					\
	container_of((p), struct idpf_adapter_ext, base)

#endif /* _IDPF_ETHDEV_H_ */
