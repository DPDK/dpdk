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
#include "rte_mtr.h"

#include "stream_binary_flow_api.h"
#include "nthw_drv.h"

#define NUM_MAC_ADDRS_PER_PORT (16U)
#define NUM_MULTICAST_ADDRS_PER_PORT (16U)

#define NUM_ADAPTER_MAX (1)
#define NUM_ADAPTER_PORTS_MAX (2)


/* Max RSS queues */
#define MAX_QUEUES       125

/* Structs: */
#define SG_HDR_SIZE    12

struct _pkt_hdr_rx {
	uint32_t cap_len:14;
	uint32_t fid:10;
	uint32_t ofs1:8;
	uint32_t ip_prot:8;
	uint32_t port:13;
	uint32_t descr:8;
	uint32_t descr_12b:1;
	uint32_t color_type:2;
	uint32_t color:32;
};

struct nthw_memory_descriptor {
	void *phys_addr;
	void *virt_addr;
	uint32_t len;
};

struct hwq_s {
	int vf_num;
	struct nthw_memory_descriptor virt_queues_ctrl;
	struct nthw_memory_descriptor pkt_buffers_ctrl;
	struct nthw_memory_descriptor *pkt_buffers;
};

struct __rte_cache_aligned ntnic_rx_queue {
	struct flow_queue_id_s queue;    /* queue info - user id and hw queue index */
	struct rte_mempool *mb_pool; /* mbuf memory pool */
	uint16_t buf_size; /* Size of data area in mbuf */
	unsigned long rx_pkts;	/* Rx packet statistics */
	unsigned long rx_bytes;	/* Rx bytes statistics */
	unsigned long err_pkts;	/* Rx error packet statistics */
	int  enabled;  /* Enabling/disabling of this queue */

	struct hwq_s           hwq;
	struct nthw_virt_queue *vq;
	int nb_hw_rx_descr;
	nt_meta_port_type_t type;
	uint32_t port;     /* Rx port for this queue */
	enum fpga_info_profile profile;  /* Inline / Capture */
	uint8_t rx_deferred_start;
};

struct __rte_cache_aligned ntnic_tx_queue {
	struct flow_queue_id_s queue; /* queue info - user id and hw queue index */
	struct hwq_s hwq;
	struct nthw_virt_queue *vq;
	int nb_hw_tx_descr;
	/* Used for bypass in NTDVIO0 header on  Tx - pre calculated */
	int target_id;
	nt_meta_port_type_t type;
	/* only used for exception tx queue from OVS SW switching */
	int rss_target_id;

	uint32_t port;     /* Tx port for this queue */
	unsigned long tx_pkts;	/* Tx packet statistics */
	unsigned long tx_bytes;	/* Tx bytes statistics */
	unsigned long err_pkts;	/* Tx error packet stat */
	int  enabled;  /* Enabling/disabling of this queue */
	enum fpga_info_profile profile;  /* Inline / Capture */
	uint8_t tx_deferred_start;
};

struct nt_mtr_profile {
	LIST_ENTRY(nt_mtr_profile) next;
	uint32_t profile_id;
	struct rte_mtr_meter_profile profile;
};

struct nt_mtr {
	LIST_ENTRY(nt_mtr) next;
	uint32_t mtr_id;
	int shared;
	struct nt_mtr_profile *profile;
};

struct pmd_internals {
	const struct rte_pci_device *pci_dev;
	struct flow_eth_dev *flw_dev;
	char name[20];
	int n_intf_no;
	int lpbk_mode;
	unsigned int nb_rx_queues;
	unsigned int nb_tx_queues;
	/* Offset of the VF from the PF */
	uint8_t vf_offset;
	uint32_t port;
	uint32_t port_id;
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
	uint64_t last_stat_rtc;
	uint64_t rx_missed;
	struct pmd_internals *next;
};

#endif	/* __NTOS_DRV_H__ */
