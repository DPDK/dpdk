/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ether.h>
#include <ethdev_driver.h>
#include <rte_log.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_arp.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>
#include <rte_hash_crc.h>
#include <rte_bitmap.h>
#include <rte_os_shim.h>

#include "i40e_logs.h"
#include "base/i40e_type.h"
#include "base/i40e_prototype.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"

#define I40E_FDIR_MZ_NAME          "FDIR_MEMZONE"
#ifndef IPV6_ADDR_LEN
#define IPV6_ADDR_LEN              16
#endif

#ifndef IPPROTO_L2TP
#define IPPROTO_L2TP		  115
#endif

#define I40E_FDIR_PKT_LEN                   512
#define I40E_FDIR_IP_DEFAULT_LEN            420
#define I40E_FDIR_IP_DEFAULT_TTL            0x40
#define I40E_FDIR_IP_DEFAULT_VERSION_IHL    0x45
#define I40E_FDIR_TCP_DEFAULT_DATAOFF       0x50
#define I40E_FDIR_IPv6_DEFAULT_VTC_FLOW     0x60000000

#define I40E_FDIR_IPv6_DEFAULT_HOP_LIMITS   0xFF
#define I40E_FDIR_IPv6_PAYLOAD_LEN          380
#define I40E_FDIR_UDP_DEFAULT_LEN           400
#define I40E_FDIR_GTP_DEFAULT_LEN           384
#define I40E_FDIR_INNER_IP_DEFAULT_LEN      384
#define I40E_FDIR_INNER_IPV6_DEFAULT_LEN    344

#define I40E_FDIR_GTPC_DST_PORT             2123
#define I40E_FDIR_GTPU_DST_PORT             2152
#define I40E_FDIR_GTP_VER_FLAG_0X30         0x30
#define I40E_FDIR_GTP_VER_FLAG_0X32         0x32
#define I40E_FDIR_GTP_MSG_TYPE_0X01         0x01
#define I40E_FDIR_GTP_MSG_TYPE_0XFF         0xFF

#define I40E_FDIR_ESP_DST_PORT              4500

/* Wait time for fdir filter programming */
#define I40E_FDIR_MAX_WAIT_US 10000

/* Wait count and interval for fdir filter flush */
#define I40E_FDIR_FLUSH_RETRY       50
#define I40E_FDIR_FLUSH_INTERVAL_MS 5

#define I40E_COUNTER_PF           2
/* Statistic counter index for one pf */
#define I40E_COUNTER_INDEX_FDIR(pf_id)   (0 + (pf_id) * I40E_COUNTER_PF)

#define I40E_FDIR_FLOWS ( \
	(1ULL << RTE_ETH_FLOW_FRAG_IPV4) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_UDP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_TCP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) | \
	(1ULL << RTE_ETH_FLOW_FRAG_IPV6) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_UDP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_TCP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) | \
	(1ULL << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER) | \
	(1ULL << RTE_ETH_FLOW_L2_PAYLOAD))

static int
i40e_flow_fdir_filter_programming(struct i40e_pf *pf,
				  enum i40e_filter_pctype pctype,
				  const struct i40e_fdir_filter *filter,
				  bool add, bool wait_status);

static int
i40e_fdir_rx_queue_init(struct ci_rx_queue *rxq)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(rxq->i40e_vsi);
	struct i40e_hmc_obj_rxq rx_ctx;
	int err = I40E_SUCCESS;

	memset(&rx_ctx, 0, sizeof(struct i40e_hmc_obj_rxq));
	/* Init the RX queue in hardware */
	rx_ctx.dbuff = I40E_RXBUF_SZ_1024 >> I40E_RXQ_CTX_DBUFF_SHIFT;
	rx_ctx.hbuff = 0;
	rx_ctx.base = rxq->rx_ring_phys_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->nb_rx_desc;
#ifndef RTE_NET_INTEL_USE_16BYTE_DESC
	rx_ctx.dsize = 1;
#endif
	rx_ctx.dtype = i40e_header_split_none;
	rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_NONE;
	rx_ctx.rxmax = I40E_ETH_MAX_LEN;
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	rx_ctx.lrxqthresh = 2;
	rx_ctx.crcstrip = 0;
	rx_ctx.l2tsel = 1;
	rx_ctx.showiv = 0;
	rx_ctx.prefena = 1;

	err = i40e_clear_lan_rx_queue_context(hw, rxq->reg_idx);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to clear FDIR RX queue context.");
		return err;
	}
	err = i40e_set_lan_rx_queue_context(hw, rxq->reg_idx, &rx_ctx);
	if (err != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to set FDIR RX queue context.");
		return err;
	}
	rxq->qrx_tail = hw->hw_addr +
		I40E_QRX_TAIL(rxq->i40e_vsi->base_queue);

	rte_wmb();
	/* Init the RX tail register. */
	I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);

	return err;
}

/*
 * i40e_fdir_setup - reserve and initialize the Flow Director resources
 * @pf: board private structure
 */
int
i40e_fdir_setup(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi;
	int err = I40E_SUCCESS;
	char z_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz = NULL;
	struct rte_eth_dev *eth_dev = &rte_eth_devices[pf->dev_data->port_id];
	uint16_t i;

	if ((pf->flags & I40E_FLAG_FDIR) == 0) {
		PMD_INIT_LOG(ERR, "HW doesn't support FDIR");
		return I40E_NOT_SUPPORTED;
	}

	PMD_DRV_LOG(INFO, "FDIR HW Capabilities: num_filters_guaranteed = %u,"
			" num_filters_best_effort = %u.",
			hw->func_caps.fd_filters_guaranteed,
			hw->func_caps.fd_filters_best_effort);

	vsi = pf->fdir.fdir_vsi;
	if (vsi) {
		PMD_DRV_LOG(INFO, "FDIR initialization has been done.");
		return I40E_SUCCESS;
	}

	/* make new FDIR VSI */
	vsi = i40e_vsi_setup(pf, I40E_VSI_FDIR, pf->main_vsi, 0);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Couldn't create FDIR VSI.");
		return I40E_ERR_NO_AVAILABLE_VSI;
	}
	pf->fdir.fdir_vsi = vsi;

	/*Fdir tx queue setup*/
	err = i40e_fdir_setup_tx_resources(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to setup FDIR TX resources.");
		goto fail_setup_tx;
	}

	/*Fdir rx queue setup*/
	err = i40e_fdir_setup_rx_resources(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to setup FDIR RX resources.");
		goto fail_setup_rx;
	}

	err = i40e_tx_queue_init(pf->fdir.txq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do FDIR TX initialization.");
		goto fail_mem;
	}

	/* need switch on before dev start*/
	err = i40e_switch_tx_queue(hw, vsi->base_queue, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do fdir TX switch on.");
		goto fail_tx_queue;
	}

	/* Init the rx queue in hardware */
	err = i40e_fdir_rx_queue_init(pf->fdir.rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do FDIR RX initialization.");
		goto fail_tx_queue;
	}

	/* switch on rx queue */
	err = i40e_switch_rx_queue(hw, vsi->base_queue, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do FDIR RX switch on.");
		goto fail_rx_queue;
	}

	/* enable FDIR MSIX interrupt */
	vsi->nb_used_qps = 1;
	i40e_vsi_queues_bind_intr(vsi, I40E_ITR_INDEX_NONE);
	i40e_vsi_enable_queues_intr(vsi);

	/* reserve memory for the fdir programming packet */
	snprintf(z_name, sizeof(z_name), "%s_%s_%d",
			eth_dev->device->driver->name,
			I40E_FDIR_MZ_NAME,
			eth_dev->data->port_id);
	mz = i40e_memzone_reserve(z_name, I40E_FDIR_PKT_LEN *
			I40E_FDIR_PRG_PKT_CNT, SOCKET_ID_ANY);
	if (!mz) {
		PMD_DRV_LOG(ERR, "Cannot init memzone for "
				 "flow director program packet.");
		err = I40E_ERR_NO_MEMORY;
		goto fail_intr;
	}

	for (i = 0; i < I40E_FDIR_PRG_PKT_CNT; i++) {
		pf->fdir.prg_pkt[i] = (uint8_t *)mz->addr +
			I40E_FDIR_PKT_LEN * i;
		pf->fdir.dma_addr[i] = mz->iova +
			I40E_FDIR_PKT_LEN * i;
	}

	pf->fdir.match_counter_index = I40E_COUNTER_INDEX_FDIR(hw->pf_id);

	PMD_DRV_LOG(INFO, "FDIR setup successfully, with programming queue %u.",
		    vsi->base_queue);
	return I40E_SUCCESS;

fail_intr:
	i40e_vsi_queues_unbind_intr(vsi);
	i40e_vsi_disable_queues_intr(vsi);
fail_rx_queue:
	if (i40e_switch_rx_queue(hw, vsi->base_queue, FALSE) != I40E_SUCCESS)
		PMD_DRV_LOG(DEBUG, "Failed to do FDIR RX switch off");
fail_tx_queue:
	if (i40e_switch_tx_queue(hw, vsi->base_queue, FALSE) != I40E_SUCCESS)
		PMD_DRV_LOG(DEBUG, "Failed to do FDIR TX switch off");
fail_mem:
	i40e_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
fail_setup_rx:
	i40e_tx_queue_release(pf->fdir.txq);
	pf->fdir.txq = NULL;
fail_setup_tx:
	i40e_vsi_release(vsi);
	pf->fdir.fdir_vsi = NULL;
	return err;
}

/*
 * i40e_fdir_teardown - release the Flow Director resources
 * @pf: board private structure
 */
void
i40e_fdir_teardown(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_vsi *vsi;

	vsi = pf->fdir.fdir_vsi;
	if (!vsi)
		return;

	/* disable FDIR MSIX interrupt */
	i40e_vsi_queues_unbind_intr(vsi);
	i40e_vsi_disable_queues_intr(vsi);

	int err = i40e_switch_tx_queue(hw, vsi->base_queue, FALSE);
	if (err)
		PMD_DRV_LOG(DEBUG, "Failed to do FDIR TX switch off");
	err = i40e_switch_rx_queue(hw, vsi->base_queue, FALSE);
	if (err)
		PMD_DRV_LOG(DEBUG, "Failed to do FDIR RX switch off");

	i40e_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
	i40e_tx_queue_release(pf->fdir.txq);
	pf->fdir.txq = NULL;
	i40e_vsi_release(vsi);
	pf->fdir.fdir_vsi = NULL;
}

/* check whether the flow director table in empty */
static inline int
i40e_fdir_empty(struct i40e_hw *hw)
{
	uint32_t guarant_cnt, best_cnt;

	guarant_cnt = (uint32_t)((I40E_READ_REG(hw, I40E_PFQF_FDSTAT) &
				 I40E_PFQF_FDSTAT_GUARANT_CNT_MASK) >>
				 I40E_PFQF_FDSTAT_GUARANT_CNT_SHIFT);
	best_cnt = (uint32_t)((I40E_READ_REG(hw, I40E_PFQF_FDSTAT) &
			      I40E_PFQF_FDSTAT_BEST_CNT_MASK) >>
			      I40E_PFQF_FDSTAT_BEST_CNT_SHIFT);
	if (best_cnt + guarant_cnt > 0)
		return -1;

	return 0;
}

/*
 * Initialize the configuration about bytes stream extracted as flexible payload
 * and mask setting
 */
static inline void
i40e_init_flx_pld(struct i40e_pf *pf)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint8_t pctype;
	int i, index;
	uint16_t flow_type;

	/*
	 * Define the bytes stream extracted as flexible payload in
	 * field vector. By default, select 8 words from the beginning
	 * of payload as flexible payload.
	 */
	for (i = I40E_FLXPLD_L2_IDX; i < I40E_MAX_FLXPLD_LAYER; i++) {
		struct i40e_fdir_layer_state *layer = &pf->fdir.flows.layer[i];
		struct i40e_fdir_flex_pit *flex_set = &layer->flex_set[0];
		index = i * I40E_MAX_FLXPLD_FIED;
		flex_set->src_offset = 0;
		flex_set->size = I40E_FDIR_MAX_FLEXWORD_NUM;
		flex_set->dst_offset = 0;
		I40E_WRITE_REG(hw, I40E_PRTQF_FLX_PIT(index), 0x0000C900);
		I40E_WRITE_REG(hw,
			I40E_PRTQF_FLX_PIT(index + 1), 0x0000FC29);/*non-used*/
		I40E_WRITE_REG(hw,
			I40E_PRTQF_FLX_PIT(index + 2), 0x0000FC2A);/*non-used*/
		layer->flex_pit_flag = false;
	}

	/* initialize the masks */
	for (pctype = I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	     pctype <= I40E_FILTER_PCTYPE_L2_PAYLOAD; pctype++) {
		struct i40e_fdir_pctype_state *state = &pf->fdir.flows.pctype[pctype];
		flow_type = i40e_pctype_to_flowtype(pf->adapter, pctype);

		if (flow_type == RTE_ETH_FLOW_UNKNOWN)
			continue;
		state->flex_mask.word_mask = 0;
		i40e_write_rx_ctl(hw, I40E_PRTQF_FD_FLXINSET(pctype), 0);
		for (i = 0; i < I40E_FDIR_BITMASK_NUM_WORD; i++) {
			state->flex_mask.bitmask[i].offset = 0;
			state->flex_mask.bitmask[i].mask = 0;
			i40e_write_rx_ctl(hw, I40E_PRTQF_FD_MSK(pctype, i), 0);
		}
	}
}

/*
 * Match flow director RX processing to whether any filter is registered.
 */
void
i40e_fdir_rx_proc_sync(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	bool on = pf->fdir.fdir_actual_cnt > 0;
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		struct ci_rx_queue *rxq = dev->data->rx_queues[i];

		if (rxq == NULL)
			continue;
		rxq->fdir_enabled = on;
	}
	PMD_DRV_LOG(DEBUG, "Flow Director processing on RX set to %d", on);
}

/*
 * Configure flow director related setting
 */
int
i40e_fdir_configure(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t val;
	int ret = 0;

	/*
	* configuration need to be done before
	* flow director filters are added
	* If filters exist, flush them.
	*/
	if (i40e_fdir_empty(hw) < 0) {
		ret = i40e_fdir_flush(dev);
		if (ret) {
			PMD_DRV_LOG(ERR, "failed to flush fdir table.");
			return ret;
		}
	}

	/* enable FDIR filter */
	val = i40e_read_rx_ctl(hw, I40E_PFQF_CTL_0);
	val |= I40E_PFQF_CTL_0_FD_ENA_MASK;
	i40e_write_rx_ctl(hw, I40E_PFQF_CTL_0, val);

	i40e_init_flx_pld(pf); /* set flex config to default value */

	return ret;
}

/*
 * Bring up the flow director engine on first use.
 */
int
i40e_fdir_engine_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret;

	if (pf->fdir.fdir_vsi != NULL)
		return 0;

	ret = i40e_fdir_setup(pf);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to setup fdir.");
		return -ENOTSUP;
	}

	ret = i40e_fdir_configure(dev);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to configure fdir.");
		i40e_fdir_teardown(pf);
		return ret;
	}

	return 0;
}


static struct i40e_customized_pctype *
i40e_flow_fdir_find_customized_pctype(struct i40e_pf *pf, uint8_t pctype)
{
	struct i40e_customized_pctype *cus_pctype;
	enum i40e_new_pctype i = I40E_CUSTOMIZED_GTPC;

	for (; i < I40E_CUSTOMIZED_MAX; i++) {
		cus_pctype = &pf->customized_pctype[i];
		if (pctype == cus_pctype->pctype)
			return cus_pctype;
	}
	return NULL;
}

static inline int
fill_ip6_head(const struct i40e_fdir_input *fdir_input, unsigned char *raw_pkt,
		uint8_t next_proto, uint8_t len, uint16_t *ether_type)
{
	struct rte_ipv6_hdr *ip6;

	ip6 = (struct rte_ipv6_hdr *)raw_pkt;

	*ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV6);
	ip6->vtc_flow = rte_cpu_to_be_32(I40E_FDIR_IPv6_DEFAULT_VTC_FLOW |
		(fdir_input->flow.ipv6_flow.tc << I40E_FDIR_IPv6_TC_OFFSET));
	ip6->payload_len = rte_cpu_to_be_16(I40E_FDIR_IPv6_PAYLOAD_LEN);
	ip6->proto = fdir_input->flow.ipv6_flow.proto ?
		fdir_input->flow.ipv6_flow.proto : next_proto;
	ip6->hop_limits = fdir_input->flow.ipv6_flow.hop_limits ?
		fdir_input->flow.ipv6_flow.hop_limits :
		I40E_FDIR_IPv6_DEFAULT_HOP_LIMITS;
	/**
	 * The source and destination fields in the transmitted packet
	 * need to be presented in a reversed order with respect
	 * to the expected received packets.
	 */
	memcpy(&ip6->src_addr, &fdir_input->flow.ipv6_flow.dst_ip,
		IPV6_ADDR_LEN);
	memcpy(&ip6->dst_addr, &fdir_input->flow.ipv6_flow.src_ip,
		IPV6_ADDR_LEN);
	len += sizeof(struct rte_ipv6_hdr);

	return len;
}

static inline int
fill_ip4_head(const struct i40e_fdir_input *fdir_input, unsigned char *raw_pkt,
		uint8_t next_proto, uint8_t len, uint16_t *ether_type)
{
	struct rte_ipv4_hdr *ip4;

	ip4 = (struct rte_ipv4_hdr *)raw_pkt;

	*ether_type = rte_cpu_to_be_16(RTE_ETHER_TYPE_IPV4);
	ip4->version_ihl = I40E_FDIR_IP_DEFAULT_VERSION_IHL;
	/* set len to by default */
	ip4->total_length = rte_cpu_to_be_16(I40E_FDIR_IP_DEFAULT_LEN);
	ip4->time_to_live = fdir_input->flow.ip4_flow.ttl ?
		fdir_input->flow.ip4_flow.ttl :
		I40E_FDIR_IP_DEFAULT_TTL;
	ip4->type_of_service = fdir_input->flow.ip4_flow.tos;
	ip4->next_proto_id = fdir_input->flow.ip4_flow.proto ?
		fdir_input->flow.ip4_flow.proto : next_proto;
	/**
	 * The source and destination fields in the transmitted packet
	 * need to be presented in a reversed order with respect
	 * to the expected received packets.
	 */
	ip4->src_addr = fdir_input->flow.ip4_flow.dst_ip;
	ip4->dst_addr = fdir_input->flow.ip4_flow.src_ip;
	len += sizeof(struct rte_ipv4_hdr);

	return len;
}

static inline int
i40e_flow_fdir_fill_eth_ip_head(struct i40e_pf *pf,
				const struct i40e_fdir_input *fdir_input,
				unsigned char *raw_pkt,
				bool vlan)
{
	struct i40e_customized_pctype *cus_pctype = NULL;
	static uint8_t vlan_frame[] = {0x81, 0, 0, 0};
	uint16_t *ether_type;
	uint8_t len = 2 * sizeof(struct rte_ether_addr);
	uint8_t pctype = fdir_input->pctype;
	bool is_customized_pctype = fdir_input->flow_ext.customized_pctype;
	static const uint8_t next_proto[] = {
		[I40E_FILTER_PCTYPE_FRAG_IPV4] = IPPROTO_IP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_TCP] = IPPROTO_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_UDP] = IPPROTO_UDP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_SCTP] = IPPROTO_SCTP,
		[I40E_FILTER_PCTYPE_NONF_IPV4_OTHER] = IPPROTO_IP,
		[I40E_FILTER_PCTYPE_FRAG_IPV6] = IPPROTO_NONE,
		[I40E_FILTER_PCTYPE_NONF_IPV6_TCP] = IPPROTO_TCP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_UDP] = IPPROTO_UDP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_SCTP] = IPPROTO_SCTP,
		[I40E_FILTER_PCTYPE_NONF_IPV6_OTHER] = IPPROTO_NONE,
	};

	memcpy(raw_pkt, &fdir_input->flow.l2_flow.dst,
		sizeof(struct rte_ether_addr));
	memcpy(raw_pkt + sizeof(struct rte_ether_addr),
		&fdir_input->flow.l2_flow.src,
		sizeof(struct rte_ether_addr));
	raw_pkt += 2 * sizeof(struct rte_ether_addr);

	if (vlan && fdir_input->flow_ext.vlan_tci) {
		memcpy(raw_pkt, vlan_frame, sizeof(vlan_frame));
		memcpy(raw_pkt + sizeof(uint16_t),
			   &fdir_input->flow_ext.vlan_tci,
			   sizeof(uint16_t));
		raw_pkt += sizeof(vlan_frame);
		len += sizeof(vlan_frame);
	}
	ether_type = (uint16_t *)raw_pkt;
	raw_pkt += sizeof(uint16_t);
	len += sizeof(uint16_t);

	if (is_customized_pctype) {
		cus_pctype = i40e_flow_fdir_find_customized_pctype(pf, pctype);
		if (!cus_pctype) {
			PMD_DRV_LOG(ERR, "unknown pctype %u.",
				    fdir_input->pctype);
			return -1;
		}
	}

	if (pctype == I40E_FILTER_PCTYPE_L2_PAYLOAD)
		*ether_type = fdir_input->flow.l2_flow.ether_type;
	else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_TCP ||
		 pctype == I40E_FILTER_PCTYPE_NONF_IPV4_UDP ||
		 pctype == I40E_FILTER_PCTYPE_NONF_IPV4_SCTP ||
		 pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER ||
		 pctype == I40E_FILTER_PCTYPE_FRAG_IPV4 ||
		 pctype == I40E_FILTER_PCTYPE_NONF_IPV6_TCP ||
		 pctype == I40E_FILTER_PCTYPE_NONF_IPV6_UDP ||
		 pctype == I40E_FILTER_PCTYPE_NONF_IPV6_SCTP ||
		 pctype == I40E_FILTER_PCTYPE_NONF_IPV6_OTHER ||
		 pctype == I40E_FILTER_PCTYPE_FRAG_IPV6 ||
		 is_customized_pctype) {
		if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_TCP ||
			pctype == I40E_FILTER_PCTYPE_NONF_IPV4_UDP ||
			pctype == I40E_FILTER_PCTYPE_NONF_IPV4_SCTP ||
			pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER ||
			pctype == I40E_FILTER_PCTYPE_FRAG_IPV4) {
			len = fill_ip4_head(fdir_input, raw_pkt,
					next_proto[pctype], len, ether_type);
		} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV6_TCP ||
			pctype == I40E_FILTER_PCTYPE_NONF_IPV6_UDP ||
			pctype == I40E_FILTER_PCTYPE_NONF_IPV6_SCTP ||
			pctype == I40E_FILTER_PCTYPE_NONF_IPV6_OTHER ||
			pctype == I40E_FILTER_PCTYPE_FRAG_IPV6) {
			len = fill_ip6_head(fdir_input, raw_pkt,
					next_proto[pctype], len,
					ether_type);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_GTPC ||
			 cus_pctype->index == I40E_CUSTOMIZED_GTPU_IPV4 ||
			 cus_pctype->index == I40E_CUSTOMIZED_GTPU_IPV6 ||
			 cus_pctype->index == I40E_CUSTOMIZED_GTPU) {
			len = fill_ip4_head(fdir_input, raw_pkt, IPPROTO_UDP,
					len, ether_type);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_IPV4_L2TPV3) {
			len = fill_ip4_head(fdir_input, raw_pkt, IPPROTO_L2TP,
					len, ether_type);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV4) {
			len = fill_ip4_head(fdir_input, raw_pkt, IPPROTO_ESP,
					len, ether_type);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV4_UDP) {
			len = fill_ip4_head(fdir_input, raw_pkt, IPPROTO_UDP,
					len, ether_type);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV6) {
			len = fill_ip6_head(fdir_input, raw_pkt, IPPROTO_ESP,
					len, ether_type);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV6_UDP) {
			len = fill_ip6_head(fdir_input, raw_pkt, IPPROTO_UDP,
					len, ether_type);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_IPV6_L2TPV3) {
			len = fill_ip6_head(fdir_input, raw_pkt, IPPROTO_L2TP,
					len, ether_type);
		}
	} else {
		PMD_DRV_LOG(ERR, "unknown pctype %u.", fdir_input->pctype);
		return -1;
	}

	return len;
}

/**
 * i40e_flow_fdir_construct_pkt - construct packet based on fields in input
 * @pf: board private structure
 * @fdir_input: input set of the flow director entry
 * @raw_pkt: a packet to be constructed
 */
static int
i40e_flow_fdir_construct_pkt(struct i40e_pf *pf,
			     const struct i40e_fdir_input *fdir_input,
			     unsigned char *raw_pkt)
{
	unsigned char *payload = NULL;
	unsigned char *ptr;
	struct rte_udp_hdr *udp;
	struct rte_tcp_hdr *tcp;
	struct rte_sctp_hdr *sctp;
	struct rte_flow_item_gtp *gtp;
	struct rte_ipv4_hdr *gtp_ipv4;
	struct rte_ipv6_hdr *gtp_ipv6;
	struct rte_flow_item_l2tpv3oip *l2tpv3oip;
	struct rte_flow_item_esp *esp;
	struct rte_ipv4_hdr *esp_ipv4;
	struct rte_ipv6_hdr *esp_ipv6;

	uint8_t size, dst = 0;
	uint8_t i, set_idx = I40E_FLXPLD_L4_IDX; /* use l4 by default*/
	int len;
	uint8_t pctype = fdir_input->pctype;
	struct i40e_customized_pctype *cus_pctype;

	/* raw packet template - just copy contents of the raw packet */
	if (fdir_input->flow_ext.pkt_template) {
		memcpy(raw_pkt, fdir_input->flow.raw_flow.packet,
		       fdir_input->flow.raw_flow.length);
		return 0;
	}

	/* fill the ethernet and IP head */
	len = i40e_flow_fdir_fill_eth_ip_head(pf, fdir_input, raw_pkt,
					      !!fdir_input->flow_ext.vlan_tci);
	if (len < 0)
		return -EINVAL;

	/* fill the L4 head */
	if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_UDP) {
		udp = (struct rte_udp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)udp + sizeof(struct rte_udp_hdr);
		/**
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		udp->src_port = fdir_input->flow.udp4_flow.dst_port;
		udp->dst_port = fdir_input->flow.udp4_flow.src_port;
		udp->dgram_len = rte_cpu_to_be_16(I40E_FDIR_UDP_DEFAULT_LEN);
	} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_TCP) {
		tcp = (struct rte_tcp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)tcp + sizeof(struct rte_tcp_hdr);
		/**
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		tcp->src_port = fdir_input->flow.tcp4_flow.dst_port;
		tcp->dst_port = fdir_input->flow.tcp4_flow.src_port;
		tcp->data_off = I40E_FDIR_TCP_DEFAULT_DATAOFF;
	} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_SCTP) {
		sctp = (struct rte_sctp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)sctp + sizeof(struct rte_sctp_hdr);
		/**
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		sctp->src_port = fdir_input->flow.sctp4_flow.dst_port;
		sctp->dst_port = fdir_input->flow.sctp4_flow.src_port;
		sctp->tag = fdir_input->flow.sctp4_flow.verify_tag;
	} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV4_OTHER ||
		   pctype == I40E_FILTER_PCTYPE_FRAG_IPV4) {
		payload = raw_pkt + len;
		set_idx = I40E_FLXPLD_L3_IDX;
	} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV6_UDP) {
		udp = (struct rte_udp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)udp + sizeof(struct rte_udp_hdr);
		/**
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		udp->src_port = fdir_input->flow.udp6_flow.dst_port;
		udp->dst_port = fdir_input->flow.udp6_flow.src_port;
		udp->dgram_len = rte_cpu_to_be_16(I40E_FDIR_IPv6_PAYLOAD_LEN);
	} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV6_TCP) {
		tcp = (struct rte_tcp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)tcp + sizeof(struct rte_tcp_hdr);
		/**
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		tcp->data_off = I40E_FDIR_TCP_DEFAULT_DATAOFF;
		tcp->src_port = fdir_input->flow.udp6_flow.dst_port;
		tcp->dst_port = fdir_input->flow.udp6_flow.src_port;
	} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV6_SCTP) {
		sctp = (struct rte_sctp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)sctp + sizeof(struct rte_sctp_hdr);
		/**
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		sctp->src_port = fdir_input->flow.sctp6_flow.dst_port;
		sctp->dst_port = fdir_input->flow.sctp6_flow.src_port;
		sctp->tag = fdir_input->flow.sctp6_flow.verify_tag;
	} else if (pctype == I40E_FILTER_PCTYPE_NONF_IPV6_OTHER ||
		   pctype == I40E_FILTER_PCTYPE_FRAG_IPV6) {
		payload = raw_pkt + len;
		set_idx = I40E_FLXPLD_L3_IDX;
	} else if (pctype == I40E_FILTER_PCTYPE_L2_PAYLOAD) {
		payload = raw_pkt + len;
		/**
		 * ARP packet is a special case on which the payload
		 * starts after the whole ARP header
		 */
		if (fdir_input->flow.l2_flow.ether_type ==
				rte_cpu_to_be_16(RTE_ETHER_TYPE_ARP))
			payload += sizeof(struct rte_arp_hdr);
		set_idx = I40E_FLXPLD_L2_IDX;
	} else if (fdir_input->flow_ext.customized_pctype) {
		/* If customized pctype is used */
		cus_pctype = i40e_flow_fdir_find_customized_pctype(pf, pctype);
		if (cus_pctype->index == I40E_CUSTOMIZED_GTPC ||
		    cus_pctype->index == I40E_CUSTOMIZED_GTPU_IPV4 ||
		    cus_pctype->index == I40E_CUSTOMIZED_GTPU_IPV6 ||
		    cus_pctype->index == I40E_CUSTOMIZED_GTPU) {
			udp = (struct rte_udp_hdr *)(raw_pkt + len);
			udp->dgram_len =
				rte_cpu_to_be_16(I40E_FDIR_UDP_DEFAULT_LEN);

			gtp = (struct rte_flow_item_gtp *)
				((unsigned char *)udp +
					sizeof(struct rte_udp_hdr));
			gtp->hdr.plen =
				rte_cpu_to_be_16(I40E_FDIR_GTP_DEFAULT_LEN);
			gtp->hdr.teid = fdir_input->flow.gtp_flow.teid;
			gtp->hdr.msg_type = I40E_FDIR_GTP_MSG_TYPE_0X01;

			/* GTP-C message type is not supported. */
			if (cus_pctype->index == I40E_CUSTOMIZED_GTPC) {
				udp->dst_port =
				      rte_cpu_to_be_16(I40E_FDIR_GTPC_DST_PORT);
				gtp->hdr.gtp_hdr_info =
					I40E_FDIR_GTP_VER_FLAG_0X32;
			} else {
				udp->dst_port =
				      rte_cpu_to_be_16(I40E_FDIR_GTPU_DST_PORT);
				gtp->hdr.gtp_hdr_info =
					I40E_FDIR_GTP_VER_FLAG_0X30;
			}

			if (cus_pctype->index == I40E_CUSTOMIZED_GTPU_IPV4) {
				gtp->hdr.msg_type = I40E_FDIR_GTP_MSG_TYPE_0XFF;
				gtp_ipv4 = (struct rte_ipv4_hdr *)
					((unsigned char *)gtp +
					 sizeof(struct rte_flow_item_gtp));
				gtp_ipv4->version_ihl =
					I40E_FDIR_IP_DEFAULT_VERSION_IHL;
				gtp_ipv4->next_proto_id = IPPROTO_IP;
				gtp_ipv4->total_length =
					rte_cpu_to_be_16(
						I40E_FDIR_INNER_IP_DEFAULT_LEN);
				payload = (unsigned char *)gtp_ipv4 +
					sizeof(struct rte_ipv4_hdr);
			} else if (cus_pctype->index ==
				   I40E_CUSTOMIZED_GTPU_IPV6) {
				gtp->hdr.msg_type = I40E_FDIR_GTP_MSG_TYPE_0XFF;
				gtp_ipv6 = (struct rte_ipv6_hdr *)
					((unsigned char *)gtp +
					 sizeof(struct rte_flow_item_gtp));
				gtp_ipv6->vtc_flow =
					rte_cpu_to_be_32(
					       I40E_FDIR_IPv6_DEFAULT_VTC_FLOW |
					       (0 << I40E_FDIR_IPv6_TC_OFFSET));
				gtp_ipv6->proto = IPPROTO_NONE;
				gtp_ipv6->payload_len =
					rte_cpu_to_be_16(
					      I40E_FDIR_INNER_IPV6_DEFAULT_LEN);
				gtp_ipv6->hop_limits =
					I40E_FDIR_IPv6_DEFAULT_HOP_LIMITS;
				payload = (unsigned char *)gtp_ipv6 +
					sizeof(struct rte_ipv6_hdr);
			} else
				payload = (unsigned char *)gtp +
					sizeof(struct rte_flow_item_gtp);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_IPV4_L2TPV3 ||
			   cus_pctype->index == I40E_CUSTOMIZED_IPV6_L2TPV3) {
			l2tpv3oip = (struct rte_flow_item_l2tpv3oip *)(raw_pkt
								       + len);

			if (cus_pctype->index == I40E_CUSTOMIZED_IPV4_L2TPV3)
				l2tpv3oip->session_id =
				 fdir_input->flow.ip4_l2tpv3oip_flow.session_id;
			else
				l2tpv3oip->session_id =
				 fdir_input->flow.ip6_l2tpv3oip_flow.session_id;
			payload = (unsigned char *)l2tpv3oip +
				sizeof(struct rte_flow_item_l2tpv3oip);
		} else if (cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV4 ||
			cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV6 ||
			cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV4_UDP ||
			cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV6_UDP) {
			if (cus_pctype->index == I40E_CUSTOMIZED_ESP_IPV4) {
				esp_ipv4 = (struct rte_ipv4_hdr *)
					(raw_pkt + len);
				esp = (struct rte_flow_item_esp *)esp_ipv4;
				esp->hdr.spi =
					fdir_input->flow.esp_ipv4_flow.spi;
				payload = (unsigned char *)esp +
					sizeof(struct rte_esp_hdr);
				len += sizeof(struct rte_esp_hdr);
			} else if (cus_pctype->index ==
					I40E_CUSTOMIZED_ESP_IPV4_UDP) {
				esp_ipv4 = (struct rte_ipv4_hdr *)
					(raw_pkt + len);
				udp = (struct rte_udp_hdr *)esp_ipv4;
				udp->dst_port = rte_cpu_to_be_16
					(I40E_FDIR_ESP_DST_PORT);

				udp->dgram_len = rte_cpu_to_be_16
						(I40E_FDIR_UDP_DEFAULT_LEN);
				esp = (struct rte_flow_item_esp *)
					((unsigned char *)esp_ipv4 +
						sizeof(struct rte_udp_hdr));
				esp->hdr.spi =
					fdir_input->flow.esp_ipv4_udp_flow.spi;
				payload = (unsigned char *)esp +
					sizeof(struct rte_esp_hdr);
				len += sizeof(struct rte_udp_hdr) +
						sizeof(struct rte_esp_hdr);
			} else if (cus_pctype->index ==
					I40E_CUSTOMIZED_ESP_IPV6) {
				esp_ipv6 = (struct rte_ipv6_hdr *)
					(raw_pkt + len);
				esp = (struct rte_flow_item_esp *)esp_ipv6;
				esp->hdr.spi =
					fdir_input->flow.esp_ipv6_flow.spi;
				payload = (unsigned char *)esp +
					sizeof(struct rte_esp_hdr);
				len += sizeof(struct rte_esp_hdr);
			} else if (cus_pctype->index ==
					I40E_CUSTOMIZED_ESP_IPV6_UDP) {
				esp_ipv6 = (struct rte_ipv6_hdr *)
					(raw_pkt + len);
				udp = (struct rte_udp_hdr *)esp_ipv6;
				udp->dst_port =	rte_cpu_to_be_16
					(I40E_FDIR_ESP_DST_PORT);

				udp->dgram_len = rte_cpu_to_be_16
					(I40E_FDIR_UDP_DEFAULT_LEN);
				esp = (struct rte_flow_item_esp *)
					((unsigned char *)esp_ipv6 +
						sizeof(struct rte_udp_hdr));
				esp->hdr.spi =
					fdir_input->flow.esp_ipv6_udp_flow.spi;
				payload = (unsigned char *)esp +
					sizeof(struct rte_esp_hdr);
				len += sizeof(struct rte_udp_hdr) +
						sizeof(struct rte_esp_hdr);
			}
		}
	} else {
		PMD_DRV_LOG(ERR, "unknown pctype %u.", fdir_input->pctype);
		return -1;
	}

	/* fill the flexbytes to payload */
	for (i = 0; i < I40E_MAX_FLXPLD_FIED; i++) {
		const struct i40e_fdir_flex_pit *pit;
		struct i40e_fdir_layer_state *layer = &pf->fdir.flows.layer[set_idx];

		pit = &layer->flex_set[i];
		size = pit->size;
		if (size == 0)
			continue;
		dst = pit->dst_offset * sizeof(uint16_t);
		ptr = payload + pit->src_offset * sizeof(uint16_t);
		(void)memcpy(ptr,
				 &fdir_input->flow_ext.flexbytes[dst],
				 size * sizeof(uint16_t));
	}

	return 0;
}

/* Construct the tx flags */
static inline uint64_t
i40e_build_ctob(uint32_t td_cmd,
		uint32_t td_offset,
		unsigned int size,
		uint32_t td_tag)
{
	return rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DATA |
			((uint64_t)td_cmd  << CI_TXD_QW1_CMD_S) |
			((uint64_t)td_offset << CI_TXD_QW1_OFFSET_S) |
			((uint64_t)size  << CI_TXD_QW1_TX_BUF_SZ_S) |
			((uint64_t)td_tag  << CI_TXD_QW1_L2TAG1_S));
}

/*
 * check the programming status descriptor in rx queue.
 * done after Programming Flow Director is programmed on
 * tx queue
 */
static inline int
i40e_check_fdir_programming_status(struct ci_rx_queue *rxq)
{
	volatile union ci_rx_desc *rxdp;
	uint64_t qword1;
	uint32_t rx_status;
	uint32_t len, id;
	uint32_t error;
	int ret = 0;

	rxdp = &rxq->rx_ring[rxq->rx_tail];
	qword1 = rte_le_to_cpu_64(rxdp->wb.qword1.status_error_len);
	rx_status = (qword1 & I40E_RXD_QW1_STATUS_MASK)
			>> I40E_RXD_QW1_STATUS_SHIFT;

	if (rx_status & (1 << I40E_RX_DESC_STATUS_DD_SHIFT)) {
		len = qword1 >> I40E_RX_PROG_STATUS_DESC_LENGTH_SHIFT;
		id = (qword1 & I40E_RX_PROG_STATUS_DESC_QW1_PROGID_MASK) >>
			    I40E_RX_PROG_STATUS_DESC_QW1_PROGID_SHIFT;

		if (len  == I40E_RX_PROG_STATUS_DESC_LENGTH &&
		    id == I40E_RX_PROG_STATUS_DESC_FD_FILTER_STATUS) {
			error = (qword1 &
				I40E_RX_PROG_STATUS_DESC_QW1_ERROR_MASK) >>
				I40E_RX_PROG_STATUS_DESC_QW1_ERROR_SHIFT;
			if (error == (0x1 <<
				I40E_RX_PROG_STATUS_DESC_FD_TBL_FULL_SHIFT)) {
				PMD_DRV_LOG(ERR, "Failed to add FDIR filter"
					    " (FD_ID %u): programming status"
					    " reported.",
					    rxdp->wb.qword0.hi_dword.fd_id);
				ret = -1;
			} else if (error == (0x1 <<
				I40E_RX_PROG_STATUS_DESC_NO_FD_ENTRY_SHIFT)) {
				PMD_DRV_LOG(ERR, "Failed to delete FDIR filter"
					    " (FD_ID %u): programming status"
					    " reported.",
					    rxdp->wb.qword0.hi_dword.fd_id);
				ret = -1;
			} else
				PMD_DRV_LOG(ERR, "invalid programming status"
					    " reported, error = %u.", error);
		} else
			PMD_DRV_LOG(INFO, "unknown programming status"
				    " reported, len = %d, id = %u.", len, id);
		rxdp->wb.qword1.status_error_len = 0;
		rxq->rx_tail++;
		if (unlikely(rxq->rx_tail == rxq->nb_rx_desc))
			rxq->rx_tail = 0;
		if (rxq->rx_tail == 0)
			I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
		else
			I40E_PCI_REG_WRITE(rxq->qrx_tail, rxq->rx_tail - 1);
	}

	return ret;
}

static inline void
i40e_fdir_programming_status_cleanup(struct ci_rx_queue *rxq)
{
	uint16_t retry_count = 0;

	/* capture the previous error report(if any) from rx ring */
	while ((i40e_check_fdir_programming_status(rxq) < 0) &&
			(++retry_count < I40E_FDIR_NUM_RX_DESC))
		PMD_DRV_LOG(INFO, "error report captured.");
}

/* Add a flow director filter into the SW hash table */
static int
i40e_fdir_filter_hash_add(struct i40e_pf *pf,
			  const struct i40e_fdir_filter *filter,
			  struct i40e_fdir_filter **node)
{
	struct i40e_fdir_flow_store *flows = &pf->fdir.flows;
	int ret;

	ret = rte_hash_lookup(flows->hash_table, &filter->input);
	if (ret >= 0) {
		PMD_DRV_LOG(ERR, "Failed to add fdir filter to hash table %d!",
			    ret);
		return -EEXIST;
	}

	ret = rte_hash_add_key(flows->hash_table, &filter->input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert fdir filter to hash table %d!",
			    ret);
		return ret;
	}

	**node = *filter;

	return 0;
}

/* Delete a flow director filter from the SW hash table */
static int
i40e_fdir_filter_hash_del(struct i40e_pf *pf,
			  const struct i40e_fdir_filter *node)
{
	int ret;

	ret = rte_hash_del_key(pf->fdir.flows.hash_table, &node->input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete fdir filter from hash table %d!",
			    ret);
		return ret;
	}

	return 0;
}

int
i40e_fdir_flow_store_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_flow_store *flows = &fdir_info->flows;
	char fdir_hash_name[RTE_HASH_NAMESIZE];
	uint32_t alloc = hw->func_caps.fd_filters_guaranteed;
	uint32_t best = hw->func_caps.fd_filters_best_effort;
	struct rte_bitmap *bmp = NULL;
	uint32_t bmp_size;
	void *mem = NULL;
	uint32_t i = 0;
	int ret;

	struct rte_hash_parameters fdir_hash_params = {
		.name = fdir_hash_name,
		.entries = I40E_MAX_FDIR_FILTER_NUM,
		.key_len = sizeof(struct i40e_fdir_input),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	snprintf(fdir_hash_name, RTE_HASH_NAMESIZE,
		 "fdir_%s", dev->device->name);
	flows->hash_table = rte_hash_create(&fdir_hash_params);
	if (!flows->hash_table) {
		PMD_INIT_LOG(ERR, "Failed to create fdir hash table!");
		return -EINVAL;
	}

	fdir_info->fdir_space_size = alloc + best;
	fdir_info->fdir_actual_cnt = 0;
	fdir_info->fdir_guarantee_total_space = alloc;

	PMD_DRV_LOG(INFO, "FDIR guarantee space: %u, best_effort space %u.", alloc, best);

	flows->flow_pool.pool =
			rte_zmalloc("i40e_fdir_entry",
				sizeof(struct i40e_fdir_entry) *
				fdir_info->fdir_space_size,
				0);

	if (!flows->flow_pool.pool) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for bitmap flow!");
		ret = -ENOMEM;
		goto err_fdir_bitmap_flow_alloc;
	}

	for (i = 0; i < fdir_info->fdir_space_size; i++)
		flows->flow_pool.pool[i].idx = i;

	bmp_size =
		rte_bitmap_get_memory_footprint(fdir_info->fdir_space_size);
	mem = rte_zmalloc("fdir_bmap", bmp_size, RTE_CACHE_LINE_SIZE);
	if (mem == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for fdir bitmap!");
		ret = -ENOMEM;
		goto err_fdir_mem_alloc;
	}
	bmp = rte_bitmap_init(fdir_info->fdir_space_size, mem, bmp_size);
	if (bmp == NULL) {
		PMD_INIT_LOG(ERR,
			     "Failed to initialization fdir bitmap!");
		ret = -ENOMEM;
		goto err_fdir_bmp_alloc;
	}
	for (i = 0; i < fdir_info->fdir_space_size; i++)
		rte_bitmap_set(bmp, i);

	flows->flow_pool.bitmap = bmp;

	return 0;

err_fdir_bmp_alloc:
	rte_free(mem);
err_fdir_mem_alloc:
	rte_free(flows->flow_pool.pool);
err_fdir_bitmap_flow_alloc:
	rte_hash_free(flows->hash_table);

	return ret;
}

void
i40e_fdir_flow_store_free(struct i40e_fdir_info *fdir_info)
{
	struct i40e_fdir_flow_store *flows = &fdir_info->flows;

	rte_free(flows->flow_pool.bitmap);
	rte_free(flows->flow_pool.pool);
	rte_hash_free(flows->hash_table);
}

static uint32_t
i40e_fdir_tmpl_sig(const struct i40e_raw_flow *raw)
{
	return rte_hash_crc(raw->packet, raw->length, raw->pctype);
}

static int
i40e_fdir_tmpl_cmp(const void *key1, const void *key2,
		   size_t key_len __rte_unused)
{
	const struct i40e_fdir_tmpl_key *k1 = key1;
	const struct i40e_fdir_tmpl_key *k2 = key2;

	if (k1->pctype != k2->pctype || k1->length != k2->length)
		return 1;

	return memcmp(k1->packet, k2->packet, k1->length);
}

static void
i40e_fdir_tmpl_key_fill(struct i40e_fdir_tmpl_key *key,
			const struct i40e_raw_flow *raw,
			const uint8_t *packet)
{
	memset(key, 0, sizeof(*key));
	key->pctype = raw->pctype;
	key->length = raw->length;
	key->packet = packet;
}

/* The template store is only worth its memory once the API is actually used */
static int
i40e_fdir_tmpl_store_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_fdir_tmpl_store *tmpls = &pf->fdir.tmpls;
	char name[RTE_HASH_NAMESIZE];
	struct rte_hash_parameters params = {
		.name = name,
		.entries = I40E_MAX_FDIR_FILTER_NUM,
		.key_len = sizeof(struct i40e_fdir_tmpl_key),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
	};

	if (tmpls->hash_table != NULL)
		return 0;

	/* the packet template store is created on first use */
	TAILQ_INIT(&tmpls->list);

	snprintf(name, sizeof(name), "fdir_tmpl_%s", dev->data->name);
	tmpls->hash_table = rte_hash_create(&params);
	if (tmpls->hash_table == NULL) {
		PMD_DRV_LOG(ERR, "Failed to create fdir template hash table.");
		return -ENOMEM;
	}
	/* the key only borrows the packet, so contents drive the comparison */
	rte_hash_set_cmp_func(tmpls->hash_table, i40e_fdir_tmpl_cmp);

	tmpls->filter_array = rte_zmalloc("fdir_tmpl_filter",
			sizeof(*tmpls->filter_array) * I40E_MAX_FDIR_FILTER_NUM,
			0);
	if (tmpls->filter_array == NULL) {
		PMD_DRV_LOG(ERR,
			     "Failed to allocate fdir template filter array.");
		rte_hash_free(tmpls->hash_table);
		tmpls->hash_table = NULL;
		return -ENOMEM;
	}

	return 0;
}

void
i40e_fdir_tmpl_store_free(struct i40e_fdir_info *fdir_info)
{
	struct i40e_fdir_tmpl_store *tmpls = &fdir_info->tmpls;
	struct i40e_fdir_tmpl_filter *tmpl, *tmp;
	uint32_t i;

	/* rules in this list point into filter array so no free() needed */
	RTE_TAILQ_FOREACH_SAFE(tmpl, &tmpls->list, rules, tmp)
		TAILQ_REMOVE(&tmpls->list, tmpl, rules);

	if (tmpls->filter_array != NULL) {
		for (i = 0; i < I40E_MAX_FDIR_FILTER_NUM; i++)
			rte_free(tmpls->filter_array[i].packet);
		rte_free(tmpls->filter_array);
		tmpls->filter_array = NULL;
	}
	rte_hash_free(tmpls->hash_table);
	tmpls->hash_table = NULL;
}

static struct i40e_fdir_tmpl_filter *
i40e_fdir_tmpl_lookup(struct i40e_fdir_info *fdir_info,
		      const struct i40e_raw_flow *raw)
{
	struct i40e_fdir_tmpl_key key;
	int ret;

	if (fdir_info->tmpls.hash_table == NULL)
		return NULL;

	i40e_fdir_tmpl_key_fill(&key, raw, raw->packet);

	ret = rte_hash_lookup_with_hash(fdir_info->tmpls.hash_table, &key,
					i40e_fdir_tmpl_sig(raw));
	if (ret < 0)
		return NULL;

	return &fdir_info->tmpls.filter_array[ret];
}

struct rte_flow *
i40e_fdir_entry_pool_get(struct i40e_fdir_info *fdir_info)
{
	struct i40e_fdir_flow_pool *pool = &fdir_info->flows.flow_pool;
	struct rte_flow *flow = NULL;
	uint64_t slab = 0;
	uint32_t pos = 0;
	uint32_t i = 0;
	int ret;

	if (fdir_info->fdir_actual_cnt >=
			fdir_info->fdir_space_size) {
		PMD_DRV_LOG(ERR, "Fdir space full");
		return NULL;
	}

	ret = rte_bitmap_scan(pool->bitmap, &pos, &slab);

	/* normally this won't happen as the fdir_actual_cnt should be
	 * same with the number of the set bits in fdir_flow_pool,
	 * but anyway handle this error condition here for safe
	 */
	if (ret == 0) {
		PMD_DRV_LOG(ERR, "fdir_actual_cnt out of sync");
		return NULL;
	}

	i = rte_bsf64(slab);
	pos += i;
	rte_bitmap_clear(pool->bitmap, pos);
	flow = &pool->pool[pos].flow;

	memset(flow, 0, sizeof(struct rte_flow));

	return flow;
}

void
i40e_fdir_entry_pool_put(struct i40e_fdir_info *fdir_info,
		struct rte_flow *flow)
{
	struct i40e_fdir_entry *f;

	f = FLOW_TO_FLOW_BITMAP(flow);
	rte_bitmap_set(fdir_info->flows.flow_pool.bitmap, f->idx);
}

static int
i40e_fdir_check_flex_pit(struct i40e_pf *pf,
			 const struct i40e_fdir_flex_pit *flex_pit,
			 enum i40e_flxpld_layer_idx layer_idx,
			 uint8_t raw_id)
{
	struct i40e_fdir_flow_store *flows = &pf->fdir.flows;
	struct i40e_fdir_layer_state *layer = &flows->layer[layer_idx];

	if (!layer->flex_pit_flag)
		return 0;

	if (memcmp(&layer->flex_set[raw_id], flex_pit, sizeof(*flex_pit)) != 0)
		return -1;

	return 1;
}

static void
i40e_fdir_flex_pit_program(struct i40e_pf *pf,
			   const struct i40e_fdir_filter *filter)
{
	enum i40e_flxpld_layer_idx layer_idx = filter->input.flow_ext.layer_idx;
	uint8_t raw_id = filter->input.flow_ext.raw_id;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	const struct i40e_fdir_flex_pit *pit;
	uint32_t flx_pit, flx_ort;
	uint16_t min_next_off = 0;
	uint8_t field_idx;
	uint8_t i;

	if (raw_id) {
		flx_ort = (1 << I40E_GLQF_ORT_FLX_PAYLOAD_SHIFT) |
			  (raw_id << I40E_GLQF_ORT_FIELD_CNT_SHIFT) |
			  (layer_idx * I40E_MAX_FLXPLD_FIED);
		I40E_WRITE_GLB_REG(hw, I40E_GLQF_ORT(33 + layer_idx), flx_ort);
	} else {
		I40E_WRITE_GLB_REG(hw, I40E_GLQF_ORT(33 + layer_idx), 0);
	}

	/* Set flex pit */
	for (i = 0; i < raw_id; i++) {
		field_idx = layer_idx * I40E_MAX_FLXPLD_FIED + i;
		pit = &filter->input.flow_ext.flex_pit[field_idx];
		flx_pit = MK_FLX_PIT(pit->src_offset, pit->size,
				     pit->dst_offset);

		I40E_WRITE_REG(hw, I40E_PRTQF_FLX_PIT(field_idx), flx_pit);
		min_next_off = pit->src_offset + pit->size;
	}

	for (; i < I40E_MAX_FLXPLD_FIED; i++) {
		/* set the non-used register obeying register's constrain */
		field_idx = layer_idx * I40E_MAX_FLXPLD_FIED + i;
		flx_pit = MK_FLX_PIT(min_next_off, NONUSE_FLX_PIT_FSIZE,
				     NONUSE_FLX_PIT_DEST_OFF);
		I40E_WRITE_REG(hw, I40E_PRTQF_FLX_PIT(field_idx), flx_pit);
		min_next_off++;
	}
}

/* Translate the byte-granular mask supplied by the caller into register form */
static int
i40e_fdir_flex_mask_convert(const uint8_t *mask,
			    struct i40e_fdir_flex_mask *flex_mask)
{
	uint8_t nb_bitmask = 0;
	uint16_t mask_tmp;
	uint8_t i;

	memset(flex_mask, 0, sizeof(*flex_mask));
	for (i = 0; i < I40E_FDIR_MAX_FLEX_LEN; i += sizeof(uint16_t)) {
		mask_tmp = I40E_WORD(mask[i], mask[i + 1]);
		if (mask_tmp) {
			flex_mask->word_mask |=
				I40E_FLEX_WORD_MASK(i / sizeof(uint16_t));
			if (mask_tmp != UINT16_MAX) {
				if (nb_bitmask >= I40E_FDIR_BITMASK_NUM_WORD)
					return -1;
				flex_mask->bitmask[nb_bitmask].mask = ~mask_tmp;
				flex_mask->bitmask[nb_bitmask].offset =
					i / sizeof(uint16_t);
				nb_bitmask++;
			}
		}
	}
	flex_mask->nb_bitmask = nb_bitmask;

	return 0;
}

static int
i40e_fdir_check_flex_mask(struct i40e_pf *pf,
			  enum i40e_filter_pctype pctype,
			  const uint8_t *mask,
			  struct i40e_fdir_flex_mask *flex_mask)
{
	struct i40e_fdir_flow_store *flows = &pf->fdir.flows;
	struct i40e_fdir_pctype_state *state = &flows->pctype[pctype];

	if (i40e_fdir_flex_mask_convert(mask, flex_mask) < 0)
		return -1;

	if (!state->flex_mask_flag)
		return 0;

	if (memcmp(flex_mask, &state->flex_mask, sizeof(*flex_mask)) != 0)
		return -2;

	return 0;
}

static int
i40e_fdir_flex_msk_program(struct i40e_pf *pf,
			   enum i40e_filter_pctype pctype,
			   const struct i40e_fdir_filter *filter)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_fdir_flex_mask flex_mask;
	uint32_t flxinset, fd_mask;
	uint8_t i;

	if (i40e_fdir_flex_mask_convert(filter->input.flow_ext.flex_mask,
					&flex_mask) < 0)
		return -EINVAL;

	flxinset = (flex_mask.word_mask <<
		    I40E_PRTQF_FD_FLXINSET_INSET_SHIFT) &
		I40E_PRTQF_FD_FLXINSET_INSET_MASK;
	i40e_write_rx_ctl(hw, I40E_PRTQF_FD_FLXINSET(pctype), flxinset);

	for (i = 0; i < flex_mask.nb_bitmask; i++) {
		fd_mask = (flex_mask.bitmask[i].mask <<
			   I40E_PRTQF_FD_MSK_MASK_SHIFT) &
			   I40E_PRTQF_FD_MSK_MASK_MASK;
		fd_mask |= ((flex_mask.bitmask[i].offset +
			     I40E_FLX_OFFSET_IN_FIELD_VECTOR) <<
			    I40E_PRTQF_FD_MSK_OFFSET_SHIFT) &
				I40E_PRTQF_FD_MSK_OFFSET_MASK;
		i40e_write_rx_ctl(hw, I40E_PRTQF_FD_MSK(pctype, i), fd_mask);
	}

	/* zero out remaining fields */
	for (; i < I40E_FDIR_BITMASK_NUM_WORD; i++)
		i40e_write_rx_ctl(hw, I40E_PRTQF_FD_MSK(pctype, i), 0);

	return 0;
}

static int
i40e_fdir_flex_check(struct i40e_pf *pf,
		     const struct i40e_fdir_filter *filter,
		     enum i40e_filter_pctype pctype,
		     struct i40e_fdir_flex_mask *flex_mask)
{
	enum i40e_flxpld_layer_idx layer_idx = filter->input.flow_ext.layer_idx;
	int ret;
	int i;

	for (i = 0; i < filter->input.flow_ext.raw_id; i++) {
		uint8_t field_idx;

		field_idx = layer_idx * I40E_MAX_FLXPLD_FIED + i;
		ret = i40e_fdir_check_flex_pit(pf,
				&filter->input.flow_ext.flex_pit[field_idx],
				layer_idx, i);
		if (ret < 0) {
			PMD_DRV_LOG(ERR,
				    "Conflict with the first flexible rule.");
			return -EINVAL;
		}
	}

	ret = i40e_fdir_check_flex_mask(pf, pctype,
					filter->input.flow_ext.flex_mask,
					flex_mask);
	if (ret == -1) {
		PMD_DRV_LOG(ERR, "Exceed maximal number of bitmasks");
		return -EINVAL;
	} else if (ret == -2) {
		PMD_DRV_LOG(ERR, "Conflict with the first flexible rule");
		return -EINVAL;
	}

	return 0;
}

static void
i40e_fdir_flex_store(struct i40e_pf *pf,
		      const struct i40e_fdir_filter *filter,
		      enum i40e_filter_pctype pctype,
		      const struct i40e_fdir_flex_mask *flex_mask)
{
	enum i40e_flxpld_layer_idx layer_idx = filter->input.flow_ext.layer_idx;
	struct i40e_fdir_flow_store *flows = &pf->fdir.flows;
	struct i40e_fdir_pctype_state *state = &flows->pctype[pctype];
	struct i40e_fdir_layer_state *layer = &flows->layer[layer_idx];
	int i;

	for (i = 0; i < filter->input.flow_ext.raw_id; i++) {
		uint8_t field_idx;

		field_idx = layer_idx * I40E_MAX_FLXPLD_FIED + i;
		layer->flex_set[i] = filter->input.flow_ext.flex_pit[field_idx];
	}

	state->flex_mask = *flex_mask;
}

/* Validate an input set against what earlier filters on this pctype established */
static int
i40e_fdir_inset_check(struct i40e_pf *pf,
		      enum i40e_filter_pctype pctype,
		      uint64_t input_set)
{
	struct i40e_fdir_flow_store *flows = &pf->fdir.flows;
	struct i40e_fdir_pctype_state *state = &flows->pctype[pctype];

	if (i40e_validate_input_set(pctype, RTE_ETH_FILTER_FDIR,
				    input_set) != 0) {
		PMD_DRV_LOG(ERR, "Invalid input set");
		return -EINVAL;
	}

	if (state->flow_count == 0)
		return 0;

	if (state->input_set != input_set) {
		PMD_DRV_LOG(ERR, "Conflict with the first rule's input set.");
		return -EINVAL;
	}

	return 0;
}

static int
i40e_fdir_inset_program(struct i40e_pf *pf,
			enum i40e_filter_pctype pctype,
			uint64_t input_set)
{
	uint32_t mask_reg[I40E_INSET_MASK_NUM_REG] = {0};
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint64_t inset_reg = 0;
	int i, num;

	num = i40e_generate_inset_mask_reg(hw, input_set, mask_reg,
						 I40E_INSET_MASK_NUM_REG);
	if (num < 0) {
		PMD_DRV_LOG(ERR, "Invalid pattern mask.");
		return -EINVAL;
	}

	if (pf->support_multi_driver) {
		for (i = 0; i < num; i++)
			if (i40e_read_rx_ctl(hw,
					I40E_GLQF_FD_MSK(i, pctype)) !=
					mask_reg[i]) {
				PMD_DRV_LOG(ERR, "Input set setting is not"
						" supported with"
						" `support-multi-driver`"
						" enabled!");
				return -EPERM;
			}
		for (i = num; i < I40E_INSET_MASK_NUM_REG; i++)
			if (i40e_read_rx_ctl(hw,
					I40E_GLQF_FD_MSK(i, pctype)) != 0) {
				PMD_DRV_LOG(ERR, "Input set setting is not"
						" supported with"
						" `support-multi-driver`"
						" enabled!");
				return -EPERM;
			}

	} else {
		for (i = 0; i < num; i++)
			i40e_check_write_reg(hw, I40E_GLQF_FD_MSK(i, pctype),
				mask_reg[i]);
		/*clear unused mask registers of the pctype */
		for (i = num; i < I40E_INSET_MASK_NUM_REG; i++)
			i40e_check_write_reg(hw,
					I40E_GLQF_FD_MSK(i, pctype), 0);
	}

	inset_reg |= i40e_translate_input_set_reg(hw->mac.type, input_set);

	i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 0),
			     (uint32_t)(inset_reg & UINT32_MAX));
	i40e_check_write_reg(hw, I40E_PRTQF_FD_INSET(pctype, 1),
			     (uint32_t)((inset_reg >>
					 I40E_32_BIT_WIDTH) & UINT32_MAX));

	I40E_WRITE_FLUSH(hw);

	return 0;
}

static inline unsigned char *
i40e_find_available_buffer(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct ci_tx_queue *txq = pf->fdir.txq;

	/* no available buffer
	 * search for more available buffers from the current
	 * descriptor, until an unavailable one
	 */
	if (fdir_info->txq_available_buf_count <= 0) {
		uint16_t tmp_tail;
		volatile struct ci_tx_desc *tmp_txdp;

		tmp_tail = txq->tx_tail;
		tmp_txdp = &txq->ci_tx_ring[tmp_tail + 1];

		do {
			if ((tmp_txdp->cmd_type_offset_bsz &
					rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) ==
					rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE))
				fdir_info->txq_available_buf_count++;
			else
				break;

			tmp_tail += 2;
			if (tmp_tail >= txq->nb_tx_desc)
				tmp_tail = 0;
		} while (tmp_tail != txq->tx_tail);
	}

	if (fdir_info->txq_available_buf_count > 0)
		fdir_info->txq_available_buf_count--;
	else
		return NULL;
	return (unsigned char *)fdir_info->prg_pkt[txq->tx_tail >> 1];
}

static enum i40e_filter_pctype
i40e_fdir_filter_pctype(const struct i40e_fdir_filter *filter)
{
	if (filter->input.flow_ext.pkt_template)
		return filter->input.flow.raw_flow.pctype;

	return filter->input.pctype;
}

bool
i40e_fdir_filter_needs_status_wait(const struct i40e_pf *pf,
				   uint32_t filter_count)
{
	if (pf->fdir.fdir_invalprio != 1)
		return true;

	return filter_count >= pf->fdir.fdir_guarantee_total_space;
}

/**
 * i40e_fdir_filter_validate - check whether a filter can be accepted at all.
 *
 * Only inspects the filter itself; conflicts against already registered
 * filters are detected by i40e_fdir_filter_register().
 */
int
i40e_fdir_filter_validate(struct rte_eth_dev *dev,
			  const struct i40e_fdir_filter *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);

	if (filter->action.rx_queue >= pf->dev_data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid queue ID");
		return -EINVAL;
	}

	if (filter->input.flow_ext.is_vf &&
	    filter->input.flow_ext.dst_id >= pf->vf_num) {
		PMD_DRV_LOG(ERR, "Invalid VF ID");
		return -EINVAL;
	}

	if (filter->input.flow_ext.pkt_template &&
	    (filter->input.flow.raw_flow.length > I40E_FDIR_PKT_LEN ||
	     filter->input.flow.raw_flow.packet == NULL)) {
		PMD_DRV_LOG(ERR,
			    "Invalid raw packet template flow filter parameters!");
		return -EINVAL;
	}

	return 0;
}

/**
 * Records the filter and everything derived from it.
 */
int
i40e_fdir_filter_register(struct rte_eth_dev *dev,
			  const struct i40e_fdir_filter *filter,
			  struct i40e_fdir_filter **node)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum i40e_filter_pctype pctype = i40e_fdir_filter_pctype(filter);
	uint64_t input_set = filter->input.flow_ext.input_set;
	struct i40e_fdir_flow_store *flows = &pf->fdir.flows;
	struct i40e_fdir_pctype_state *state = &flows->pctype[pctype];
	struct i40e_fdir_layer_state *layer = NULL;
	struct i40e_fdir_flex_mask flex_mask;
	bool common_pctype;
	int ret;

	/* check input set if the packet type is common */
	common_pctype = !filter->input.flow_ext.customized_pctype;

	if (common_pctype) {
		ret = i40e_fdir_inset_check(pf, pctype, input_set);
		if (ret < 0)
			return ret;
	}

	/* check if flex flow configuration is valid */
	if (filter->input.flow_ext.is_flex_flow) {
		ret = i40e_fdir_flex_check(pf, filter, pctype, &flex_mask);
		if (ret < 0)
			return ret;
	}

	/* register the flow with the hash table */
	ret = i40e_fdir_filter_hash_add(pf, filter, node);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Conflict with existing flow director rules!");
		return ret;
	}

	/* save additional configuration */
	if (common_pctype)
		state->input_set = input_set;

	if (filter->input.flow_ext.is_flex_flow) {
		i40e_fdir_flex_store(pf, filter, pctype, &flex_mask);
		layer = &flows->layer[filter->input.flow_ext.layer_idx];
		layer->flex_flow_count++;
		layer->flex_pit_flag = true;
		state->flex_mask_flag = true;
	}

	state->flow_count++;
	pf->fdir.fdir_actual_cnt++;

	i40e_fdir_rx_proc_sync(dev);

	return 0;
}

/**
 * i40e_fdir_filter_unregister - drop software ownership of a filter.
 */
int
i40e_fdir_filter_unregister(struct rte_eth_dev *dev,
			    struct i40e_fdir_filter *node)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum i40e_filter_pctype pctype = i40e_fdir_filter_pctype(node);
	enum i40e_flxpld_layer_idx layer_idx = node->input.flow_ext.layer_idx;
	bool is_flex_flow = node->input.flow_ext.is_flex_flow;
	struct i40e_fdir_flow_store *flows = &pf->fdir.flows;
	struct i40e_fdir_pctype_state *state = &flows->pctype[pctype];
	struct i40e_fdir_layer_state *layer = &flows->layer[layer_idx];
	int ret;

	ret = i40e_fdir_filter_hash_del(pf, node);
	if (ret < 0)
		return ret;

	if (is_flex_flow && --layer->flex_flow_count == 0)
		layer->flex_pit_flag = false;

	if (--state->flow_count == 0)
		state->flex_mask_flag = false;

	pf->fdir.fdir_actual_cnt--;

	i40e_fdir_rx_proc_sync(dev);
	if (pf->fdir.fdir_actual_cnt == 0)
		i40e_fdir_teardown(pf);

	return 0;
}

/* Take software ownership of a packet template filter, owning its packet */
static int
i40e_fdir_tmpl_register(struct rte_eth_dev *dev,
			const struct i40e_fdir_filter *filter,
			struct i40e_fdir_tmpl_filter **node)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct i40e_raw_flow *raw = &filter->input.flow.raw_flow;
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_tmpl_filter *tmpl;
	struct i40e_fdir_tmpl_key key;
	uint8_t *packet;
	int ret;

	ret = i40e_fdir_tmpl_store_init(dev);
	if (ret < 0)
		return ret;

	if (i40e_fdir_tmpl_lookup(fdir_info, raw) != NULL) {
		PMD_DRV_LOG(ERR, "Conflict with existing flow director rules!");
		return -EEXIST;
	}

	packet = rte_malloc("fdir_tmpl_packet", raw->length, 0);
	if (packet == NULL)
		return -ENOMEM;
	memcpy(packet, raw->packet, raw->length);

	/* the stored key borrows the packet, so it must point at our copy */
	i40e_fdir_tmpl_key_fill(&key, raw, packet);

	ret = rte_hash_add_key_with_hash(fdir_info->tmpls.hash_table, &key,
					 i40e_fdir_tmpl_sig(raw));
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert fdir template to hash table %d!",
			    ret);
		rte_free(packet);
		return ret;
	}

	tmpl = &fdir_info->tmpls.filter_array[ret];
	tmpl->fdir = *filter;
	tmpl->packet = packet;
	tmpl->fdir.input.flow.raw_flow.packet = packet;

	fdir_info->fdir_actual_cnt++;

	i40e_fdir_rx_proc_sync(dev);

	*node = tmpl;

	return 0;
}

static int
i40e_fdir_tmpl_unregister(struct rte_eth_dev *dev,
			  struct i40e_fdir_tmpl_filter *node)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	const struct i40e_raw_flow *raw = &node->fdir.input.flow.raw_flow;
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_tmpl_key key;
	int ret;

	i40e_fdir_tmpl_key_fill(&key, raw, raw->packet);

	ret = rte_hash_del_key_with_hash(fdir_info->tmpls.hash_table, &key,
					 i40e_fdir_tmpl_sig(raw));
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete fdir template from hash table %d!",
			    ret);
		return ret;
	}

	rte_free(node->packet);
	node->packet = NULL;

	fdir_info->fdir_actual_cnt--;

	i40e_fdir_rx_proc_sync(dev);
	if (fdir_info->fdir_actual_cnt == 0)
		i40e_fdir_teardown(pf);

	return 0;
}

/**
 * i40e_fdir_filter_program - apply a filter to the hardware.
 *
 * Derives everything it needs from the filter itself and touches no software
 * bookkeeping, so it may be called repeatedly to reapply an already registered
 * filter. The caller is responsible for having validated and registered it.
 */
int
i40e_fdir_filter_program(struct rte_eth_dev *dev,
			 const struct i40e_fdir_filter *filter,
			 bool add, bool wait_status)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	enum i40e_filter_pctype pctype = i40e_fdir_filter_pctype(filter);
	unsigned char *pkt;
	int ret;

	if (pf->fdir.fdir_vsi == NULL) {
		PMD_DRV_LOG(ERR, "FDIR is not enabled");
		return -ENOTSUP;
	}

	if (add) {
		if (!filter->input.flow_ext.customized_pctype &&
		    !filter->input.flow_ext.pkt_template) {
			ret = i40e_fdir_inset_program(pf, pctype,
					filter->input.flow_ext.input_set);
			if (ret < 0)
				return ret;
		}

		if (filter->input.flow_ext.is_flex_flow) {
			i40e_fdir_flex_pit_program(pf, filter);
			ret = i40e_fdir_flex_msk_program(pf, pctype, filter);
			if (ret < 0)
				return ret;
		}
	}

	pkt = i40e_find_available_buffer(dev);
	if (pkt == NULL) {
		PMD_DRV_LOG(ERR, "No buffer available to program fdir filter.");
		return -ENOSPC;
	}

	memset(pkt, 0, I40E_FDIR_PKT_LEN);
	ret = i40e_flow_fdir_construct_pkt(pf, &filter->input, pkt);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "construct packet for fdir fails.");
		return ret;
	}

	if (hw->mac.type == I40E_MAC_X722) {
		/* get translated pctype value in fd pctype register */
		pctype = (enum i40e_filter_pctype)i40e_read_rx_ctl(
			hw, I40E_GLQF_FD_PCTYPES((int)pctype));
	}

	ret = i40e_flow_fdir_filter_programming(pf, pctype, filter, add,
			wait_status);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "fdir programming fails for PCTYPE(%u).",
			    pctype);
		return ret;
	}

	return 0;
}

/**
 * i40e_fdir_tmpl_add - add a packet template filter.
 * @dev: ethernet device
 * @filter: fdir filter entry
 *
 * Entry point for the PMD packet template API, whose filters have no rte_flow
 * handle and so are tracked entirely here.
 */
int
i40e_fdir_tmpl_add(struct rte_eth_dev *dev,
			   const struct i40e_fdir_filter *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_fdir_tmpl_filter *node;
	int ret;

	ret = i40e_fdir_filter_validate(dev, filter);
	if (ret < 0)
		return ret;

	/* register updates count, so store it */
	uint32_t cnt = pf->fdir.fdir_actual_cnt;

	ret = i40e_fdir_tmpl_register(dev, filter, &node);
	if (ret < 0)
		return ret;

	ret = i40e_fdir_engine_init(dev);
	if (ret < 0) {
		i40e_fdir_tmpl_unregister(dev, node);
		return ret;
	}

	ret = i40e_fdir_filter_program(dev, &node->fdir, true,
			i40e_fdir_filter_needs_status_wait(pf, cnt));
	if (ret < 0) {
		i40e_fdir_tmpl_unregister(dev, node);
		return ret;
	}

	TAILQ_INSERT_TAIL(&pf->fdir.tmpls.list, node, rules);

	return 0;
}

int
i40e_fdir_tmpl_del(struct rte_eth_dev *dev,
			   const struct i40e_fdir_filter *filter)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_fdir_tmpl_filter *node;
	int ret;

	ret = i40e_fdir_filter_validate(dev, filter);
	if (ret < 0)
		return ret;

	node = i40e_fdir_tmpl_lookup(&pf->fdir, &filter->input.flow.raw_flow);
	if (node == NULL) {
		PMD_DRV_LOG(ERR,
			    "There's no corresponding flow director filter!");
		return -EINVAL;
	}

	ret = i40e_fdir_filter_program(dev, &node->fdir, false, false);
	if (ret < 0)
		return ret;

	ret = i40e_fdir_tmpl_unregister(dev, node);
	if (ret < 0)
		return ret;

	TAILQ_REMOVE(&pf->fdir.tmpls.list, node, rules);

	return 0;
}

/*
 * i40e_flow_fdir_filter_programming - Program a flow director filter rule.
 * Is done by Flow Director Programming Descriptor followed by packet
 * structure that contains the filter fields need to match.
 * @pf: board private structure
 * @pctype: pctype
 * @filter: fdir filter entry
 * @add: 0 - delete, 1 - add
 */
static int
i40e_flow_fdir_filter_programming(struct i40e_pf *pf,
				  enum i40e_filter_pctype pctype,
				  const struct i40e_fdir_filter *filter,
				  bool add, bool wait_status)
{
	struct ci_tx_queue *txq = pf->fdir.txq;
	struct ci_rx_queue *rxq = pf->fdir.rxq;
	const struct i40e_fdir_action *fdir_action = &filter->action;
	volatile struct ci_tx_desc *txdp;
	volatile struct i40e_filter_program_desc *fdirdp;
	uint32_t td_cmd;
	uint16_t vsi_id;
	uint8_t dest;
	uint32_t i;

	PMD_DRV_LOG(INFO, "filling filter programming descriptor.");
	fdirdp = (volatile struct i40e_filter_program_desc *)
				(&txq->ci_tx_ring[txq->tx_tail]);

	fdirdp->qindex_flex_ptype_vsi =
			rte_cpu_to_le_32((fdir_action->rx_queue <<
					  I40E_TXD_FLTR_QW0_QINDEX_SHIFT) &
					  I40E_TXD_FLTR_QW0_QINDEX_MASK);

	fdirdp->qindex_flex_ptype_vsi |=
			rte_cpu_to_le_32((fdir_action->flex_off <<
					  I40E_TXD_FLTR_QW0_FLEXOFF_SHIFT) &
					  I40E_TXD_FLTR_QW0_FLEXOFF_MASK);

	fdirdp->qindex_flex_ptype_vsi |=
			rte_cpu_to_le_32((pctype <<
					  I40E_TXD_FLTR_QW0_PCTYPE_SHIFT) &
					  I40E_TXD_FLTR_QW0_PCTYPE_MASK);

	if (filter->input.flow_ext.is_vf)
		vsi_id = pf->vfs[filter->input.flow_ext.dst_id].vsi->vsi_id;
	else
		/* Use LAN VSI Id by default */
		vsi_id = pf->main_vsi->vsi_id;
	fdirdp->qindex_flex_ptype_vsi |=
		rte_cpu_to_le_32(((uint32_t)vsi_id <<
				  I40E_TXD_FLTR_QW0_DEST_VSI_SHIFT) &
				  I40E_TXD_FLTR_QW0_DEST_VSI_MASK);

	fdirdp->dtype_cmd_cntindex =
			rte_cpu_to_le_32(I40E_TX_DESC_DTYPE_FILTER_PROG);

	if (add)
		fdirdp->dtype_cmd_cntindex |= rte_cpu_to_le_32(
				I40E_FILTER_PROGRAM_DESC_PCMD_ADD_UPDATE <<
				I40E_TXD_FLTR_QW1_PCMD_SHIFT);
	else
		fdirdp->dtype_cmd_cntindex |= rte_cpu_to_le_32(
				I40E_FILTER_PROGRAM_DESC_PCMD_REMOVE <<
				I40E_TXD_FLTR_QW1_PCMD_SHIFT);

	if (fdir_action->behavior == I40E_FDIR_REJECT)
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DROP_PACKET;
	else if (fdir_action->behavior == I40E_FDIR_ACCEPT)
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DIRECT_PACKET_QINDEX;
	else if (fdir_action->behavior == I40E_FDIR_PASSTHRU)
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DIRECT_PACKET_OTHER;
	else {
		PMD_DRV_LOG(ERR, "Failed to program FDIR filter: unsupported fdir behavior.");
		return -EINVAL;
	}

	fdirdp->dtype_cmd_cntindex |= rte_cpu_to_le_32((dest <<
				I40E_TXD_FLTR_QW1_DEST_SHIFT) &
				I40E_TXD_FLTR_QW1_DEST_MASK);

	fdirdp->dtype_cmd_cntindex |=
		rte_cpu_to_le_32((fdir_action->report_status <<
				I40E_TXD_FLTR_QW1_FD_STATUS_SHIFT) &
				I40E_TXD_FLTR_QW1_FD_STATUS_MASK);

	fdirdp->dtype_cmd_cntindex |=
			rte_cpu_to_le_32(I40E_TXD_FLTR_QW1_CNT_ENA_MASK);
	fdirdp->dtype_cmd_cntindex |=
			rte_cpu_to_le_32(
			((uint32_t)pf->fdir.match_counter_index <<
			I40E_TXD_FLTR_QW1_CNTINDEX_SHIFT) &
			I40E_TXD_FLTR_QW1_CNTINDEX_MASK);

	fdirdp->fd_id = rte_cpu_to_le_32(filter->soft_id);

	PMD_DRV_LOG(INFO, "filling transmit descriptor.");
	txdp = &txq->ci_tx_ring[txq->tx_tail + 1];
	txdp->buffer_addr = rte_cpu_to_le_64(pf->fdir.dma_addr[txq->tx_tail >> 1]);

	td_cmd = CI_TX_DESC_CMD_EOP |
		 CI_TX_DESC_CMD_RS  |
		 CI_TX_DESC_CMD_DUMMY;

	txdp->cmd_type_offset_bsz =
		i40e_build_ctob(td_cmd, 0, I40E_FDIR_PKT_LEN, 0);

	txq->tx_tail += 2; /* set 2 descriptors above, fdirdp and txdp */
	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;
	/* Update the tx tail register */
	rte_wmb();

	/* fdir program rx queue cleanup */
	i40e_fdir_programming_status_cleanup(rxq);

	I40E_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	if (wait_status) {
		for (i = 0; i < I40E_FDIR_MAX_WAIT_US; i++) {
			if ((txdp->cmd_type_offset_bsz &
					rte_cpu_to_le_64(CI_TXD_QW1_DTYPE_M)) ==
					rte_cpu_to_le_64(CI_TX_DESC_DTYPE_DESC_DONE))
				break;
			rte_delay_us(1);
		}
		if (i >= I40E_FDIR_MAX_WAIT_US) {
			PMD_DRV_LOG(ERR,
			    "Failed to program FDIR filter: time out to get DD on tx queue.");
			return -ETIMEDOUT;
		}
		/* totally delay 10 ms to check programming status*/
		rte_delay_us(I40E_FDIR_MAX_WAIT_US);
		if (i40e_check_fdir_programming_status(rxq) < 0) {
			PMD_DRV_LOG(ERR,
			    "Failed to program FDIR filter: programming status reported.");
			return -ETIMEDOUT;
		}
	}

	return 0;
}

/*
 * i40e_fdir_flush - clear all filters of Flow Director table
 * @pf: board private structure
 */
int
i40e_fdir_flush(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint32_t reg;
	uint16_t guarant_cnt, best_cnt;
	uint16_t i;

	I40E_WRITE_REG(hw, I40E_PFQF_CTL_1, I40E_PFQF_CTL_1_CLEARFDTABLE_MASK);
	I40E_WRITE_FLUSH(hw);

	for (i = 0; i < I40E_FDIR_FLUSH_RETRY; i++) {
		rte_delay_ms(I40E_FDIR_FLUSH_INTERVAL_MS);
		reg = I40E_READ_REG(hw, I40E_PFQF_CTL_1);
		if (!(reg & I40E_PFQF_CTL_1_CLEARFDTABLE_MASK))
			break;
	}
	if (i >= I40E_FDIR_FLUSH_RETRY) {
		PMD_DRV_LOG(ERR, "FD table did not flush, may need more time.");
		return -ETIMEDOUT;
	}
	guarant_cnt = (uint16_t)((I40E_READ_REG(hw, I40E_PFQF_FDSTAT) &
				I40E_PFQF_FDSTAT_GUARANT_CNT_MASK) >>
				I40E_PFQF_FDSTAT_GUARANT_CNT_SHIFT);
	best_cnt = (uint16_t)((I40E_READ_REG(hw, I40E_PFQF_FDSTAT) &
				I40E_PFQF_FDSTAT_BEST_CNT_MASK) >>
				I40E_PFQF_FDSTAT_BEST_CNT_SHIFT);
	if (guarant_cnt != 0 || best_cnt != 0) {
		PMD_DRV_LOG(ERR, "Failed to flush FD table.");
		return -ENOSYS;
	} else
		PMD_DRV_LOG(INFO, "FD table Flush success.");
	return 0;
}

static inline void
i40e_fdir_info_get_flex_set(struct i40e_pf *pf,
			struct rte_eth_flex_payload_cfg *flex_set,
			uint16_t *num)
{
	struct i40e_fdir_flex_pit *flex_pit;
	struct rte_eth_flex_payload_cfg *ptr = flex_set;
	uint16_t src, dst, size, j, k;
	uint8_t i, layer_idx;

	for (layer_idx = I40E_FLXPLD_L2_IDX;
	     layer_idx <= I40E_FLXPLD_L4_IDX;
	     layer_idx++) {
		struct i40e_fdir_layer_state *layer =
			&pf->fdir.flows.layer[layer_idx];

		if (layer_idx == I40E_FLXPLD_L2_IDX)
			ptr->type = RTE_ETH_L2_PAYLOAD;
		else if (layer_idx == I40E_FLXPLD_L3_IDX)
			ptr->type = RTE_ETH_L3_PAYLOAD;
		else if (layer_idx == I40E_FLXPLD_L4_IDX)
			ptr->type = RTE_ETH_L4_PAYLOAD;

		for (i = 0; i < I40E_MAX_FLXPLD_FIED; i++) {
			flex_pit = &layer->flex_set[i];
			if (flex_pit->size == 0)
				continue;
			src = flex_pit->src_offset * sizeof(uint16_t);
			dst = flex_pit->dst_offset * sizeof(uint16_t);
			size = flex_pit->size * sizeof(uint16_t);
			for (j = src, k = dst; j < src + size; j++, k++)
				ptr->src_offset[k] = j;
		}
		(*num)++;
		ptr++;
	}
}

static inline void
i40e_fdir_info_get_flex_mask(struct i40e_pf *pf,
			struct rte_eth_fdir_flex_mask *flex_mask,
			uint16_t *num)
{
	struct i40e_fdir_flex_mask *mask;
	struct rte_eth_fdir_flex_mask *ptr = flex_mask;
	uint16_t flow_type;
	uint8_t i, j;
	uint16_t off_bytes, mask_tmp;

	for (i = I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	     i <= I40E_FILTER_PCTYPE_L2_PAYLOAD;
	     i++) {
		struct i40e_fdir_pctype_state *state = &pf->fdir.flows.pctype[i];

		mask = &state->flex_mask;
		flow_type = i40e_pctype_to_flowtype(pf->adapter,
						    (enum i40e_filter_pctype)i);
		if (flow_type == RTE_ETH_FLOW_UNKNOWN)
			continue;

		for (j = 0; j < I40E_FDIR_MAX_FLEXWORD_NUM; j++) {
			if (mask->word_mask & I40E_FLEX_WORD_MASK(j)) {
				ptr->mask[j * sizeof(uint16_t)] = UINT8_MAX;
				ptr->mask[j * sizeof(uint16_t) + 1] = UINT8_MAX;
			} else {
				ptr->mask[j * sizeof(uint16_t)] = 0x0;
				ptr->mask[j * sizeof(uint16_t) + 1] = 0x0;
			}
		}
		for (j = 0; j < I40E_FDIR_BITMASK_NUM_WORD; j++) {
			off_bytes = mask->bitmask[j].offset * sizeof(uint16_t);
			mask_tmp = ~mask->bitmask[j].mask;
			ptr->mask[off_bytes] &= I40E_HI_BYTE(mask_tmp);
			ptr->mask[off_bytes + 1] &= I40E_LO_BYTE(mask_tmp);
		}
		ptr->flow_type = flow_type;
		ptr++;
		(*num)++;
	}
}

/*
 * i40e_fdir_info_get - get information of Flow Director
 * @pf: ethernet device to get info from
 * @fdir: a pointer to a structure of type *rte_eth_fdir_info* to be filled with
 *    the flow director information.
 */
void
i40e_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir_info *fdir)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint16_t num_flex_set = 0;
	uint16_t num_flex_mask = 0;
	uint16_t i;

	fdir->mode = RTE_FDIR_MODE_NONE;

	fdir->guarant_spc =
		(uint32_t)hw->func_caps.fd_filters_guaranteed;
	fdir->best_spc =
		(uint32_t)hw->func_caps.fd_filters_best_effort;
	fdir->max_flexpayload = I40E_FDIR_MAX_FLEX_LEN;
	fdir->flow_types_mask[0] = I40E_FDIR_FLOWS;
	for (i = 1; i < RTE_FLOW_MASK_ARRAY_SIZE; i++)
		fdir->flow_types_mask[i] = 0ULL;
	fdir->flex_payload_unit = sizeof(uint16_t);
	fdir->flex_bitmask_unit = sizeof(uint16_t);
	fdir->max_flex_payload_segment_num = I40E_MAX_FLXPLD_FIED;
	fdir->flex_payload_limit = I40E_MAX_FLX_SOURCE_OFF;
	fdir->max_flex_bitmask_num = I40E_FDIR_BITMASK_NUM_WORD;

	i40e_fdir_info_get_flex_set(pf,
				fdir->flex_conf.flex_set,
				&num_flex_set);
	i40e_fdir_info_get_flex_mask(pf,
				fdir->flex_conf.flex_mask,
				&num_flex_mask);

	fdir->flex_conf.nb_payloads = num_flex_set;
	fdir->flex_conf.nb_flexmasks = num_flex_mask;
}

/*
 * i40e_fdir_stat_get - get statistics of Flow Director
 * @pf: ethernet device to get info from
 * @stat: a pointer to a structure of type *rte_eth_fdir_stats* to be filled with
 *    the flow director statistics.
 */
void
i40e_fdir_stats_get(struct rte_eth_dev *dev, struct rte_eth_fdir_stats *stat)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint32_t fdstat;

	fdstat = I40E_READ_REG(hw, I40E_PFQF_FDSTAT);
	stat->guarant_cnt =
		(uint32_t)((fdstat & I40E_PFQF_FDSTAT_GUARANT_CNT_MASK) >>
			    I40E_PFQF_FDSTAT_GUARANT_CNT_SHIFT);
	stat->best_cnt =
		(uint32_t)((fdstat & I40E_PFQF_FDSTAT_BEST_CNT_MASK) >>
			    I40E_PFQF_FDSTAT_BEST_CNT_SHIFT);
}

void
i40e_fdir_filter_restore(struct i40e_pf *pf)
{
	struct rte_eth_dev *dev = I40E_VSI_TO_ETH_DEV(pf->main_vsi);
	struct i40e_fdir_tmpl_filter *tmpl;
	struct rte_flow *flow;
	int ret;

	i40e_fdir_rx_proc_sync(dev);

	if (pf->fdir.fdir_vsi == NULL)
		return;

	TAILQ_FOREACH(flow, &pf->flow_list, node) {
		struct i40e_fdir_filter *node = flow->rule;

		if (flow->filter_type != RTE_ETH_FILTER_FDIR)
			continue;

		ret = i40e_fdir_filter_program(dev, node, true, false);
		if (ret < 0)
			PMD_DRV_LOG(ERR,
				    "Failed to restore flow director filter: %d", ret);
	}

	TAILQ_FOREACH(tmpl, &pf->fdir.tmpls.list, rules) {
		ret = i40e_fdir_filter_program(dev, &tmpl->fdir, true, false);
		if (ret < 0)
			PMD_DRV_LOG(ERR,
				    "Failed to restore flow director template: %d", ret);
	}
}
