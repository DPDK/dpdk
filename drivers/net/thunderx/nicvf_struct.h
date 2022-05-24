/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2016 Cavium, Inc
 */

#ifndef _THUNDERX_NICVF_STRUCT_H
#define _THUNDERX_NICVF_STRUCT_H

#include <stdint.h>

#include <rte_spinlock.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <ethdev_driver.h>
#include <rte_memory.h>

struct nicvf_rbdr {
	uintptr_t rbdr_status;
	uintptr_t rbdr_door;
	struct rbdr_entry_t *desc;
	nicvf_iova_addr_t phys;
	uint32_t buffsz;
	uint32_t tail;
	uint32_t next_tail;
	uint32_t head;
	uint32_t qlen_mask;
} __rte_cache_aligned;

struct nicvf_txq {
	union sq_entry_t *desc;
	nicvf_iova_addr_t phys;
	struct rte_mbuf **txbuffs;
	uintptr_t sq_head;
	uintptr_t sq_door;
	struct rte_mempool *pool;
	struct nicvf *nic;
	void (*pool_free)(struct nicvf_txq *sq);
	uint32_t head;
	uint32_t tail;
	int32_t xmit_bufs;
	uint32_t qlen_mask;
	uint64_t offloads;
	uint16_t queue_id;
	uint16_t tx_free_thresh;
} __rte_cache_aligned;

union mbuf_initializer {
	struct {
		uint16_t data_off;
		uint16_t refcnt;
		uint16_t nb_segs;
		uint16_t port;
	} fields;
	uint64_t value;
};

struct nicvf_rxq {
	RTE_MARKER rxq_fastpath_data_start;
	uint8_t  rbptr_offset;
	uint16_t rx_free_thresh;
	uint32_t head;
	uint32_t qlen_mask;
	int32_t recv_buffers;
	int32_t available_space;
	uint64_t mbuf_phys_off;
	uintptr_t cq_status;
	uintptr_t cq_door;
	struct nicvf_rbdr *shared_rbdr;
	struct rte_mempool *pool;
	union cq_entry_t *desc;
	union mbuf_initializer mbuf_initializer;
	RTE_MARKER rxq_fastpath_data_end;
	uint8_t rx_drop_en;
	uint16_t precharge_cnt;
	uint16_t port_id;
	uint16_t queue_id;
	struct nicvf *nic;
	nicvf_iova_addr_t phys;
} __rte_cache_aligned;

struct nicvf {
	uint8_t vf_id;
	uint8_t node;
	uintptr_t reg_base;
	bool tns_mode;
	bool sqs_mode;
	bool loopback_supported;
	bool pf_acked:1;
	bool pf_nacked:1;
	bool offload_cksum:1;
	bool vlan_strip:1;
	uint64_t hwcap;
	uint8_t link_up;
	uint8_t	duplex;
	uint32_t speed;
	uint32_t msg_enable;
	uint16_t device_id;
	uint16_t vendor_id;
	uint16_t subsystem_device_id;
	uint16_t subsystem_vendor_id;
	struct nicvf_rbdr *rbdr;
	struct nicvf_rss_reta_info rss_info;
	struct rte_intr_handle *intr_handle;
	uint8_t cpi_alg;
	uint16_t mtu;
	int skip_bytes;
	bool vlan_filter_en;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	/* secondary queue set support */
	uint8_t sqs_id;
	uint8_t sqs_count;
#define MAX_SQS_PER_VF 11
	struct nicvf *snicvf[MAX_SQS_PER_VF];
} __rte_cache_aligned;

struct change_link_mode {
	bool	   enable;
	uint8_t    qlm_mode;
	bool	   autoneg;
	uint8_t    duplex;
	uint32_t   speed;

};

#endif /* _THUNDERX_NICVF_STRUCT_H */
