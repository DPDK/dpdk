/*
 *   BSD LICENSE
 *
 *   Copyright (C) Cavium, Inc. 2016.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Cavium, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _THUNDERX_NICVF_STRUCT_H
#define _THUNDERX_NICVF_STRUCT_H

#include <stdint.h>

#include <rte_spinlock.h>
#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_interrupts.h>
#include <rte_ethdev.h>
#include <rte_memory.h>

struct nicvf_rbdr {
	uintptr_t rbdr_status;
	uintptr_t rbdr_door;
	struct rbdr_entry_t *desc;
	nicvf_phys_addr_t phys;
	uint32_t buffsz;
	uint32_t tail;
	uint32_t next_tail;
	uint32_t head;
	uint32_t qlen_mask;
} __rte_cache_aligned;

struct nicvf_txq {
	union sq_entry_t *desc;
	nicvf_phys_addr_t phys;
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
	uint32_t txq_flags;
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
	uint64_t mbuf_phys_off;
	uintptr_t cq_status;
	uintptr_t cq_door;
	union mbuf_initializer mbuf_initializer;
	nicvf_phys_addr_t phys;
	union cq_entry_t *desc;
	struct nicvf_rbdr *shared_rbdr;
	struct nicvf *nic;
	struct rte_mempool *pool;
	uint32_t head;
	uint32_t qlen_mask;
	int32_t available_space;
	int32_t recv_buffers;
	uint16_t rx_free_thresh;
	uint16_t queue_id;
	uint16_t precharge_cnt;
	uint8_t rx_drop_en;
	uint8_t  port_id;
	uint8_t  rbptr_offset;
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
	struct rte_intr_handle intr_handle;
	uint8_t cpi_alg;
	uint16_t mtu;
	bool vlan_filter_en;
	uint8_t mac_addr[ETHER_ADDR_LEN];
	/* secondary queue set support */
	uint8_t sqs_id;
	uint8_t sqs_count;
#define MAX_SQS_PER_VF 11
	struct nicvf *snicvf[MAX_SQS_PER_VF];
} __rte_cache_aligned;

#endif /* _THUNDERX_NICVF_STRUCT_H */
