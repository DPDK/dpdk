/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2017 Cisco Systems, Inc.  All rights reserved.
 * Copyright 2007 Nuova Systems, Inc.  All rights reserved.
 */

#ifndef _ENIC_H_
#define _ENIC_H_

#include <rte_vxlan.h>
#include <rte_ether.h>
#include "vnic_enet.h"
#include "vnic_dev.h"
#include "vnic_flowman.h"
#include "vnic_wq.h"
#include "vnic_rq.h"
#include "vnic_cq.h"
#include "vnic_intr.h"
#include "vnic_stats.h"
#include "vnic_nic.h"
#include "vnic_rss.h"
#include "enic_res.h"
#include "cq_enet_desc.h"
#include <stdbool.h>
#include <sys/queue.h>
#include <rte_spinlock.h>

#define DRV_NAME		"enic_pmd"
#define DRV_DESCRIPTION		"Cisco VIC Ethernet NIC Poll-mode Driver"
#define DRV_COPYRIGHT		"Copyright 2008-2015 Cisco Systems, Inc"

#define VLAN_ETH_HLEN           18

#define ENICPMD_SETTING(enic, f) ((enic->config.flags & VENETF_##f) ? 1 : 0)

#define ENICPMD_BDF_LENGTH      13   /* 0000:00:00.0'\0' */
#define ENIC_CALC_IP_CKSUM      1
#define ENIC_CALC_TCP_UDP_CKSUM 2
#define ENIC_MAX_MTU            9000
#define ENIC_PAGE_SIZE          4096
#define PAGE_ROUND_UP(x) \
	((((unsigned long)(x)) + ENIC_PAGE_SIZE-1) & (~(ENIC_PAGE_SIZE-1)))

#define ENICPMD_VFIO_PATH          "/dev/vfio/vfio"
/*#define ENIC_DESC_COUNT_MAKE_ODD (x) do{if ((~(x)) & 1) { (x)--; } }while(0)*/

#define PCI_DEVICE_ID_CISCO_VIC_ENET         0x0043  /* ethernet vnic */
#define PCI_DEVICE_ID_CISCO_VIC_ENET_VF      0x0071  /* enet SRIOV VF */
/* enet SRIOV Standalone vNic VF */
#define PCI_DEVICE_ID_CISCO_VIC_ENET_SN      0x02B7

/* Special Filter id for non-specific packet flagging. Don't change value */
#define ENIC_MAGIC_FILTER_ID 0xffff

/*
 * Interrupt 0: LSC and errors
 * Interrupt 1: rx queue 0
 * Interrupt 2: rx queue 1
 * ...
 */
#define ENICPMD_LSC_INTR_OFFSET 0
#define ENICPMD_RXQ_INTR_OFFSET 1

struct enic_soft_stats {
	rte_atomic64_t rx_nombuf;
	rte_atomic64_t rx_packet_errors;
	rte_atomic64_t tx_oversized;
};

struct enic_memzone_entry {
	const struct rte_memzone *rz;
	LIST_ENTRY(enic_memzone_entry) entries;
};

/* Defined in enic_fm_flow.c */
struct enic_flowman;
struct enic_fm_flow;

struct rte_flow {
	LIST_ENTRY(rte_flow) next;
	/* Data for filter API based flow (enic_flow.c) */
	uint16_t enic_filter_id;
	struct filter_v2 enic_filter;
	/* Data for flow manager based flow (enic_fm_flow.c) */
	struct enic_fm_flow *fm;
	int internal;
};

/* Per-instance private data structure */
struct enic {
	struct rte_pci_device *pdev;
	struct vnic_enet_config config;
	struct vnic_dev_bar bar0;
	struct vnic_dev *vdev;

	/*
	 * mbuf_initializer contains 64 bits of mbuf rearm_data, used by
	 * the avx2 handler at this time.
	 */
	uint64_t mbuf_initializer;
	unsigned int port_id;
	bool overlay_offload;
	struct rte_eth_dev *rte_dev;
	struct rte_eth_dev_data *dev_data;
	char bdf_name[ENICPMD_BDF_LENGTH];
	int dev_fd;
	int iommu_group_fd;
	int iommu_groupid;
	int eventfd;
	uint8_t mac_addr[RTE_ETHER_ADDR_LEN];
	pthread_t err_intr_thread;
	int promisc;
	int allmulti;
	uint8_t ig_vlan_strip_en;
	int link_status;
	uint8_t hw_ip_checksum;
	uint16_t max_mtu;
	uint8_t adv_filters;
	uint32_t flow_filter_mode;
	uint8_t filter_actions; /* HW supported actions */
	uint64_t cq_entry_sizes; /* supported CQ entry sizes */
	bool geneve;
	bool vxlan;
	bool cq64;            /* actually using 64B CQ entry */
	bool cq64_request;    /* devargs cq64=1 */
	bool disable_overlay; /* devargs disable_overlay=1 */
	uint8_t enable_avx2_rx;  /* devargs enable-avx2-rx=1 */
	uint8_t geneve_opt_request;  /* devargs geneve-opt=1 */
	bool nic_cfg_chk;     /* NIC_CFG_CHK available */
	bool udp_rss_weak;    /* Bodega style UDP RSS */
	uint8_t ig_vlan_rewrite_mode; /* devargs ig-vlan-rewrite */
	uint16_t geneve_port; /* current geneve port pushed to NIC */
	uint16_t vxlan_port;  /* current vxlan port pushed to NIC */
	int use_simple_tx_handler;
	int use_noscatter_vec_rx_handler;

	unsigned int flags;
	unsigned int priv_flags;

	/* work queue (len = conf_wq_count) */
	struct vnic_wq *wq;
	unsigned int wq_count; /* equals eth_dev nb_tx_queues */

	/* receive queue (len = conf_rq_count) */
	struct vnic_rq *rq;
	unsigned int rq_count; /* equals eth_dev nb_rx_queues */

	/* completion queue (len = conf_cq_count) */
	struct vnic_cq *cq;
	unsigned int cq_count; /* equals rq_count + wq_count */

	/* interrupt vectors (len = conf_intr_count) */
	struct vnic_intr *intr;
	unsigned int intr_count; /* equals enabled interrupts (lsc + rxqs) */

	/* software counters */
	struct enic_soft_stats soft_stats;

	/* configured resources on vic */
	unsigned int conf_rq_count;
	unsigned int conf_wq_count;
	unsigned int conf_cq_count;
	unsigned int conf_intr_count;

	/* linked list storing memory allocations */
	LIST_HEAD(enic_memzone_list, enic_memzone_entry) memzone_list;
	rte_spinlock_t memzone_list_lock;
	rte_spinlock_t mtu_lock;

	LIST_HEAD(enic_flows, rte_flow) flows;

	/* RSS */
	uint16_t reta_size;
	uint8_t hash_key_size;
	uint64_t flow_type_rss_offloads; /* 0 indicates RSS not supported */
	/*
	 * Keep a copy of current RSS config for queries, as we cannot retrieve
	 * it from the NIC.
	 */
	uint8_t rss_hash_type; /* NIC_CFG_RSS_HASH_TYPE flags */
	uint8_t rss_enable;
	uint64_t rss_hf; /* RTE_ETH_RSS flags */
	union vnic_rss_key rss_key;
	union vnic_rss_cpu rss_cpu;

	uint64_t rx_offload_capa; /* DEV_RX_OFFLOAD flags */
	uint64_t tx_offload_capa; /* DEV_TX_OFFLOAD flags */
	uint64_t tx_queue_offload_capa; /* DEV_TX_OFFLOAD flags */
	uint64_t tx_offload_mask; /* PKT_TX flags accepted */

	/* Multicast MAC addresses added to the NIC */
	uint32_t mc_count;
	struct rte_ether_addr mc_addrs[ENIC_MULTICAST_PERFECT_FILTERS];

	/* Flow manager API */
	struct enic_flowman *fm;
	uint64_t fm_vnic_handle;
	uint32_t fm_vnic_uif;
	/* switchdev */
	uint8_t switchdev_mode;
	uint16_t switch_domain_id;
	uint16_t max_vf_id;
	/* Number of queues needed for VF representor paths */
	uint32_t vf_required_wq;
	uint32_t vf_required_cq;
	uint32_t vf_required_rq;
	/*
	 * Lock to serialize devcmds from PF, VF representors as they all share
	 * the same PF devcmd instance in firmware.
	 */
	rte_spinlock_t devcmd_lock;
};

struct enic_vf_representor {
	struct enic enic;
	struct vnic_enet_config config;
	struct rte_eth_dev *eth_dev;
	struct rte_ether_addr mac_addr;
	struct rte_pci_addr bdf;
	struct enic *pf;
	uint16_t switch_domain_id;
	uint16_t vf_id;
	int allmulti;
	int promisc;
	/* Representor path uses PF queues. These are reserved during init */
	uint16_t pf_wq_idx;      /* WQ dedicated to VF rep */
	uint16_t pf_wq_cq_idx;   /* CQ for WQ */
	uint16_t pf_rq_sop_idx;  /* SOP RQ dedicated to VF rep */
	uint16_t pf_rq_data_idx; /* Data RQ */
	/* Representor flows managed by flowman */
	struct rte_flow *vf2rep_flow[2];
	struct rte_flow *rep2vf_flow[2];
};

#define VF_ENIC_TO_VF_REP(vf_enic) \
	container_of(vf_enic, struct enic_vf_representor, enic)

static inline int enic_is_vf_rep(struct enic *enic)
{
	return !!(enic->rte_dev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR);
}

/* Compute ethdev's max packet size from MTU */
static inline uint32_t enic_mtu_to_max_rx_pktlen(uint32_t mtu)
{
	/* ethdev max size includes eth whereas NIC MTU does not */
	return mtu + RTE_ETHER_HDR_LEN;
}

/* Get the CQ index from a Start of Packet(SOP) RQ index */
static inline unsigned int enic_sop_rq_idx_to_cq_idx(unsigned int sop_idx)
{
	return sop_idx;
}

/* Get the RTE RQ index from a Start of Packet(SOP) RQ index */
static inline unsigned int enic_sop_rq_idx_to_rte_idx(unsigned int sop_idx)
{
	return sop_idx;
}

/* Get the Start of Packet(SOP) RQ index from a RTE RQ index */
static inline unsigned int enic_rte_rq_idx_to_sop_idx(unsigned int rte_idx)
{
	return rte_idx;
}

/* Get the Data RQ index from a RTE RQ index */
static inline unsigned int enic_rte_rq_idx_to_data_idx(unsigned int rte_idx,
						       struct enic *enic)
{
	return enic->rq_count + rte_idx;
}

static inline unsigned int enic_vnic_rq_count(struct enic *enic)
{
	return enic->rq_count * 2;
}

static inline unsigned int enic_cq_rq(__rte_unused struct enic *enic, unsigned int rq)
{
	return rq;
}

static inline unsigned int enic_cq_wq(struct enic *enic, unsigned int wq)
{
	return enic->rq_count + wq;
}

/*
 * WQ, RQ, CQ allocation scheme. Firmware gives the driver an array of
 * WQs, an array of RQs, and an array of CQs. Fow now, these are
 * statically allocated between PF app send/receive queues and VF
 * representor app send/receive queues. VF representor supports only 1
 * send and 1 receive queue. The number of PF app queue is not known
 * until the queue setup time.
 *
 * R = number of receive queues for PF app
 * S = number of send queues for PF app
 * V = number of VF representors
 *
 * wI = WQ for PF app send queue I
 * rI = SOP RQ for PF app receive queue I
 * dI = Data RQ for rI
 * cwI = CQ for wI
 * crI = CQ for rI
 * vwI = WQ for VF representor send queue I
 * vrI = SOP RQ for VF representor receive queue I
 * vdI = Data RQ for vrI
 * vcwI = CQ for vwI
 * vcrI = CQ for vrI
 *
 * WQ array: | w0 |..| wS-1 |..| vwV-1 |..| vw0 |
 *             ^         ^         ^         ^
 *    index    0        S-1       W-V       W-1    W=len(WQ array)
 *
 * RQ array: | r0  |..| rR-1  |d0 |..|dR-1|  ..|vdV-1 |..| vd0 |vrV-1 |..|vr0 |
 *             ^         ^     ^       ^         ^          ^     ^        ^
 *    index    0        R-1    R      2R-1      X-2V    X-(V+1)  X-V      X-1
 * X=len(RQ array)
 *
 * CQ array: | cr0 |..| crR-1 |cw0|..|cwS-1|..|vcwV-1|..| vcw0|vcrV-1|..|vcr0|..
 *              ^         ^     ^       ^        ^         ^      ^        ^
 *    index     0        R-1    R     R+S-1     X-2V    X-(V+1)  X-V      X-1
 * X is not a typo. It really is len(RQ array) to accommodate enic_cq_rq() used
 * throughout RX handlers. The current scheme requires
 * len(CQ array) >= len(RQ array).
 */

static inline unsigned int vf_wq_cq_idx(struct enic_vf_representor *vf)
{
	/* rq is not a typo. index(vcwI) coincides with index(vdI) */
	return vf->pf->conf_rq_count - (vf->pf->max_vf_id + vf->vf_id + 2);
}

static inline unsigned int vf_wq_idx(struct enic_vf_representor *vf)
{
	return vf->pf->conf_wq_count - vf->vf_id - 1;
}

static inline unsigned int vf_rq_sop_idx(struct enic_vf_representor *vf)
{
	return vf->pf->conf_rq_count - vf->vf_id - 1;
}

static inline unsigned int vf_rq_data_idx(struct enic_vf_representor *vf)
{
	return vf->pf->conf_rq_count - (vf->pf->max_vf_id + vf->vf_id + 2);
}

static inline struct enic *pmd_priv(struct rte_eth_dev *eth_dev)
{
	return eth_dev->data->dev_private;
}

static inline uint32_t
enic_ring_add(uint32_t n_descriptors, uint32_t i0, uint32_t i1)
{
	uint32_t d = i0 + i1;
	d -= (d >= n_descriptors) ? n_descriptors : 0;
	return d;
}

static inline uint32_t
enic_ring_sub(uint32_t n_descriptors, uint32_t i0, uint32_t i1)
{
	int32_t d = i1 - i0;
	return (uint32_t)((d < 0) ? ((int32_t)n_descriptors + d) : d);
}

static inline uint32_t
enic_ring_incr(uint32_t n_descriptors, uint32_t idx)
{
	idx++;
	if (unlikely(idx == n_descriptors))
		idx = 0;
	return idx;
}

int dev_is_enic(struct rte_eth_dev *dev);
void enic_free_wq(void *txq);
int enic_alloc_intr_resources(struct enic *enic);
int enic_setup_finish(struct enic *enic);
int enic_alloc_wq(struct enic *enic, uint16_t queue_idx,
		  unsigned int socket_id, uint16_t nb_desc);
void enic_start_wq(struct enic *enic, uint16_t queue_idx);
int enic_stop_wq(struct enic *enic, uint16_t queue_idx);
void enic_start_rq(struct enic *enic, uint16_t queue_idx);
int enic_stop_rq(struct enic *enic, uint16_t queue_idx);
void enic_free_rq(void *rxq);
int enic_alloc_rq(struct enic *enic, uint16_t queue_idx,
		  unsigned int socket_id, struct rte_mempool *mp,
		  uint16_t nb_desc, uint16_t free_thresh);
int enic_set_vnic_res(struct enic *enic);
int enic_init_rss_nic_cfg(struct enic *enic);
int enic_set_rss_conf(struct enic *enic,
		      struct rte_eth_rss_conf *rss_conf);
int enic_set_rss_reta(struct enic *enic, union vnic_rss_cpu *rss_cpu);
int enic_set_vlan_strip(struct enic *enic);
int enic_enable(struct enic *enic);
int enic_disable(struct enic *enic);
void enic_remove(struct enic *enic);
int enic_get_link_status(struct enic *enic);
int enic_dev_stats_get(struct enic *enic,
		       struct rte_eth_stats *r_stats);
int enic_dev_stats_clear(struct enic *enic);
int enic_add_packet_filter(struct enic *enic);
int enic_set_mac_address(struct enic *enic, uint8_t *mac_addr);
int enic_del_mac_address(struct enic *enic, int mac_index);
unsigned int enic_cleanup_wq(struct enic *enic, struct vnic_wq *wq);
void enic_send_pkt(struct enic *enic, struct vnic_wq *wq,
		   struct rte_mbuf *tx_pkt, unsigned short len,
		   uint8_t sop, uint8_t eop, uint8_t cq_entry,
		   uint16_t ol_flags, uint16_t vlan_tag);

void enic_post_wq_index(struct vnic_wq *wq);
int enic_probe(struct enic *enic);
int enic_fm_init(struct enic *enic);
void enic_fm_destroy(struct enic *enic);
void *enic_alloc_consistent(void *priv, size_t size, dma_addr_t *dma_handle,
			    uint8_t *name);
void enic_free_consistent(void *priv, size_t size, void *vaddr,
			  dma_addr_t dma_handle);
uint16_t enic_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
			uint16_t nb_pkts);
uint16_t enic_recv_pkts_64(void *rx_queue, struct rte_mbuf **rx_pkts,
			   uint16_t nb_pkts);
uint16_t enic_noscatter_recv_pkts(void *rx_queue, struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts);
uint16_t enic_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
uint16_t enic_simple_xmit_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			       uint16_t nb_pkts);
uint16_t enic_prep_pkts(void *tx_queue, struct rte_mbuf **tx_pkts,
			uint16_t nb_pkts);
int enic_set_mtu(struct enic *enic, uint16_t new_mtu);
int enic_link_update(struct rte_eth_dev *eth_dev);
bool enic_use_vector_rx_handler(struct rte_eth_dev *eth_dev);
void enic_pick_rx_handler(struct rte_eth_dev *eth_dev);
void enic_pick_tx_handler(struct rte_eth_dev *eth_dev);
int enic_vf_representor_init(struct rte_eth_dev *eth_dev, void *init_params);
int enic_vf_representor_uninit(struct rte_eth_dev *ethdev);
int enic_fm_allocate_switch_domain(struct enic *pf);
int enic_fm_add_rep2vf_flow(struct enic_vf_representor *vf);
int enic_fm_add_vf2rep_flow(struct enic_vf_representor *vf);
int enic_alloc_rx_queue_mbufs(struct enic *enic, struct vnic_rq *rq);
void enic_rxmbuf_queue_release(struct enic *enic, struct vnic_rq *rq);
void enic_free_wq_buf(struct rte_mbuf **buf);
void enic_free_rq_buf(struct rte_mbuf **mbuf);
extern const struct rte_flow_ops enic_flow_ops;
extern const struct rte_flow_ops enic_fm_flow_ops;

#endif /* _ENIC_H_ */
