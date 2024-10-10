/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTOS_DRV_H__
#define __NTOS_DRV_H__

#include <unistd.h>
#include <stdlib.h>
#include <stdint.h>
#include <inttypes.h>

#include <rte_ether.h>

#include "stream_binary_flow_api.h"
#include "nthw_drv.h"

#define NUM_MAC_ADDRS_PER_PORT (16U)
#define NUM_MULTICAST_ADDRS_PER_PORT (16U)

#define NUM_ADAPTER_MAX (8)
#define NUM_ADAPTER_PORTS_MAX (128)


/* Max RSS queues */
#define MAX_QUEUES       125

/* Structs: */
struct nthw_memory_descriptor {
	void *virt_addr;
	uint32_t len;
};

struct hwq_s {
	int vf_num;
	struct nthw_memory_descriptor virt_queues_ctrl;
	struct nthw_memory_descriptor *pkt_buffers;
};

struct __rte_cache_aligned ntnic_rx_queue {
	struct flow_queue_id_s queue;    /* queue info - user id and hw queue index */
	struct rte_mempool *mb_pool; /* mbuf memory pool */
	uint16_t buf_size; /* Size of data area in mbuf */
	int  enabled;  /* Enabling/disabling of this queue */

	struct hwq_s           hwq;
	struct nthw_virt_queue *vq;
	nt_meta_port_type_t type;
	uint32_t port;     /* Rx port for this queue */
	enum fpga_info_profile profile;  /* Inline / Capture */

};

struct __rte_cache_aligned ntnic_tx_queue {
	struct flow_queue_id_s queue; /* queue info - user id and hw queue index */
	struct hwq_s hwq;
	struct nthw_virt_queue *vq;
	nt_meta_port_type_t type;

	uint32_t port;     /* Tx port for this queue */
	int  enabled;  /* Enabling/disabling of this queue */
	enum fpga_info_profile profile;  /* Inline / Capture */
};

struct pmd_internals {
	const struct rte_pci_device *pci_dev;
	char name[20];
	int n_intf_no;
	int lpbk_mode;
	unsigned int nb_rx_queues;
	unsigned int nb_tx_queues;
	/* Offset of the VF from the PF */
	uint8_t vf_offset;
	uint32_t port;
	nt_meta_port_type_t type;
	struct flow_queue_id_s vpq[MAX_QUEUES];
	unsigned int           vpq_nb_vq;
	/* Array of Rx queues */
	struct ntnic_rx_queue  rxq_scg[MAX_QUEUES];
	/* Array of Tx queues */
	struct ntnic_tx_queue  txq_scg[MAX_QUEUES];
	struct drv_s *p_drv;
	/* Ethernet (MAC) addresses. Element number zero denotes default address. */
	struct rte_ether_addr eth_addrs[NUM_MAC_ADDRS_PER_PORT];
	/* Multicast ethernet (MAC) addresses. */
	struct rte_ether_addr mc_addrs[NUM_MULTICAST_ADDRS_PER_PORT];
	struct pmd_internals *next;
};

#endif	/* __NTOS_DRV_H__ */
