/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>

#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_log.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>

#include "i40e_logs.h"
#include "i40e/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"

#define I40E_FDIR_MZ_NAME          "FDIR_MEMZONE"
#ifndef IPV6_ADDR_LEN
#define IPV6_ADDR_LEN              16
#endif

#define I40E_FDIR_PKT_LEN                   512
#define I40E_FDIR_IP_DEFAULT_LEN            420
#define I40E_FDIR_IP_DEFAULT_TTL            0x40
#define I40E_FDIR_IP_DEFAULT_VERSION_IHL    0x45
#define I40E_FDIR_TCP_DEFAULT_DATAOFF       0x50
#define I40E_FDIR_IPv6_DEFAULT_VTC_FLOW     0x60300000
#define I40E_FDIR_IPv6_DEFAULT_HOP_LIMITS   0xFF
#define I40E_FDIR_IPv6_PAYLOAD_LEN          380
#define I40E_FDIR_UDP_DEFAULT_LEN           400

/* Wait count and interval for fdir filter programming */
#define I40E_FDIR_WAIT_COUNT       10
#define I40E_FDIR_WAIT_INTERVAL_US 1000

/* Wait count and interval for fdir filter flush */
#define I40E_FDIR_FLUSH_RETRY       50
#define I40E_FDIR_FLUSH_INTERVAL_MS 5

#define I40E_COUNTER_PF           2
/* Statistic counter index for one pf */
#define I40E_COUNTER_INDEX_FDIR(pf_id)   (0 + (pf_id) * I40E_COUNTER_PF)
#define I40E_FLX_OFFSET_IN_FIELD_VECTOR   50

static int i40e_fdir_rx_queue_init(struct i40e_rx_queue *rxq);
static int i40e_fdir_construct_pkt(struct i40e_pf *pf,
				     const struct rte_eth_fdir_input *fdir_input,
				     unsigned char *raw_pkt);
static int i40e_add_del_fdir_filter(struct rte_eth_dev *dev,
			    const struct rte_eth_fdir_filter *filter,
			    bool add);
static int i40e_fdir_filter_programming(struct i40e_pf *pf,
			enum i40e_filter_pctype pctype,
			const struct rte_eth_fdir_filter *filter,
			bool add);
static int i40e_fdir_flush(struct rte_eth_dev *dev);

static int
i40e_fdir_rx_queue_init(struct i40e_rx_queue *rxq)
{
	struct i40e_hw *hw = I40E_VSI_TO_HW(rxq->vsi);
	struct i40e_hmc_obj_rxq rx_ctx;
	int err = I40E_SUCCESS;

	memset(&rx_ctx, 0, sizeof(struct i40e_hmc_obj_rxq));
	/* Init the RX queue in hardware */
	rx_ctx.dbuff = I40E_RXBUF_SZ_1024 >> I40E_RXQ_CTX_DBUFF_SHIFT;
	rx_ctx.hbuff = 0;
	rx_ctx.base = rxq->rx_ring_phys_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->nb_rx_desc;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	rx_ctx.dsize = 1;
#endif
	rx_ctx.dtype = i40e_header_split_none;
	rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_NONE;
	rx_ctx.rxmax = ETHER_MAX_LEN;
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	rx_ctx.lrxqthresh = 2;
	rx_ctx.crcstrip = 0;
	rx_ctx.l2tsel = 1;
	rx_ctx.showiv = 1;
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
		I40E_QRX_TAIL(rxq->vsi->base_queue);

	rte_wmb();
	/* Init the RX tail regieter. */
	I40E_PCI_REG_WRITE(rxq->qrx_tail, 0);
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
	struct rte_eth_dev *eth_dev = pf->adapter->eth_dev;

	PMD_DRV_LOG(INFO, "FDIR HW Capabilities: num_filters_guaranteed = %u,"
			" num_filters_best_effort = %u.",
			hw->func_caps.fd_filters_guaranteed,
			hw->func_caps.fd_filters_best_effort);

	vsi = pf->fdir.fdir_vsi;
	if (vsi) {
		PMD_DRV_LOG(ERR, "FDIR vsi pointer needs "
				 "to be null before creation.");
		return I40E_ERR_BAD_PTR;
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
		goto fail_mem;
	}

	/* Init the rx queue in hardware */
	err = i40e_fdir_rx_queue_init(pf->fdir.rxq);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do FDIR RX initialization.");
		goto fail_mem;
	}

	/* switch on rx queue */
	err = i40e_switch_rx_queue(hw, vsi->base_queue, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to do FDIR RX switch on.");
		goto fail_mem;
	}

	/* reserve memory for the fdir programming packet */
	snprintf(z_name, sizeof(z_name), "%s_%s_%d",
			eth_dev->driver->pci_drv.name,
			I40E_FDIR_MZ_NAME,
			eth_dev->data->port_id);
	mz = i40e_memzone_reserve(z_name, I40E_FDIR_PKT_LEN, SOCKET_ID_ANY);
	if (!mz) {
		PMD_DRV_LOG(ERR, "Cannot init memzone for "
				 "flow director program packet.");
		err = I40E_ERR_NO_MEMORY;
		goto fail_mem;
	}
	pf->fdir.prg_pkt = mz->addr;
#ifdef RTE_LIBRTE_XEN_DOM0
	pf->fdir.dma_addr = rte_mem_phy2mch(mz->memseg_id, mz->phys_addr);
#else
	pf->fdir.dma_addr = (uint64_t)mz->phys_addr;
#endif
	pf->fdir.match_counter_index = I40E_COUNTER_INDEX_FDIR(hw->pf_id);
	PMD_DRV_LOG(INFO, "FDIR setup successfully, with programming queue %u.",
		    vsi->base_queue);
	return I40E_SUCCESS;

fail_mem:
	i40e_dev_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
fail_setup_rx:
	i40e_dev_tx_queue_release(pf->fdir.txq);
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
	i40e_switch_tx_queue(hw, vsi->base_queue, FALSE);
	i40e_switch_rx_queue(hw, vsi->base_queue, FALSE);
	i40e_dev_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
	i40e_dev_tx_queue_release(pf->fdir.txq);
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

	/*
	 * Define the bytes stream extracted as flexible payload in
	 * field vector. By default, select 8 words from the beginning
	 * of payload as flexible payload.
	 */
	for (i = I40E_FLXPLD_L2_IDX; i < I40E_MAX_FLXPLD_LAYER; i++) {
		index = i * I40E_MAX_FLXPLD_FIED;
		pf->fdir.flex_set[index].src_offset = 0;
		pf->fdir.flex_set[index].size = I40E_FDIR_MAX_FLEXWORD_NUM;
		pf->fdir.flex_set[index].dst_offset = 0;
		I40E_WRITE_REG(hw, I40E_PRTQF_FLX_PIT(index), 0x0000C900);
		I40E_WRITE_REG(hw,
			I40E_PRTQF_FLX_PIT(index + 1), 0x0000FC29);/*non-used*/
		I40E_WRITE_REG(hw,
			I40E_PRTQF_FLX_PIT(index + 2), 0x0000FC2A);/*non-used*/
	}

	/* initialize the masks */
	for (pctype = I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	     pctype <= I40E_FILTER_PCTYPE_FRAG_IPV6; pctype++) {
		pf->fdir.flex_mask[pctype].word_mask = 0;
		I40E_WRITE_REG(hw, I40E_PRTQF_FD_FLXINSET(pctype), 0);
		for (i = 0; i < I40E_FDIR_BITMASK_NUM_WORD; i++) {
			pf->fdir.flex_mask[pctype].bitmask[i].offset = 0;
			pf->fdir.flex_mask[pctype].bitmask[i].mask = 0;
			I40E_WRITE_REG(hw, I40E_PRTQF_FD_MSK(pctype, i), 0);
		}
	}
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

	val = I40E_READ_REG(hw, I40E_PFQF_CTL_0);
	if ((pf->flags & I40E_FLAG_FDIR) &&
		dev->data->dev_conf.fdir_conf.mode == RTE_FDIR_MODE_PERFECT) {
		/* enable FDIR filter */
		val |= I40E_PFQF_CTL_0_FD_ENA_MASK;
		I40E_WRITE_REG(hw, I40E_PFQF_CTL_0, val);

		i40e_init_flx_pld(pf); /* set flex config to default value */
	} else {
		/* disable FDIR filter */
		val &= ~I40E_PFQF_CTL_0_FD_ENA_MASK;
		I40E_WRITE_REG(hw, I40E_PFQF_CTL_0, val);
		pf->flags &= ~I40E_FLAG_FDIR;
	}

	return ret;
}

static inline void
i40e_fdir_fill_eth_ip_head(const struct rte_eth_fdir_input *fdir_input,
			       unsigned char *raw_pkt)
{
	struct ether_hdr *ether = (struct ether_hdr *)raw_pkt;
	struct ipv4_hdr *ip;
	struct ipv6_hdr *ip6;
	static const uint8_t next_proto[] = {
		[RTE_ETH_FLOW_TYPE_UDPV4] = IPPROTO_UDP,
		[RTE_ETH_FLOW_TYPE_TCPV4] = IPPROTO_TCP,
		[RTE_ETH_FLOW_TYPE_SCTPV4] = IPPROTO_SCTP,
		[RTE_ETH_FLOW_TYPE_IPV4_OTHER] = IPPROTO_IP,
		[RTE_ETH_FLOW_TYPE_FRAG_IPV4] = IPPROTO_IP,
		[RTE_ETH_FLOW_TYPE_UDPV6] = IPPROTO_UDP,
		[RTE_ETH_FLOW_TYPE_TCPV6] = IPPROTO_TCP,
		[RTE_ETH_FLOW_TYPE_SCTPV6] = IPPROTO_SCTP,
		[RTE_ETH_FLOW_TYPE_IPV6_OTHER] = IPPROTO_NONE,
		[RTE_ETH_FLOW_TYPE_FRAG_IPV6] = IPPROTO_NONE,
	};

	switch (fdir_input->flow_type) {
	case RTE_ETH_FLOW_TYPE_UDPV4:
	case RTE_ETH_FLOW_TYPE_TCPV4:
	case RTE_ETH_FLOW_TYPE_SCTPV4:
	case RTE_ETH_FLOW_TYPE_IPV4_OTHER:
	case RTE_ETH_FLOW_TYPE_FRAG_IPV4:
		ip = (struct ipv4_hdr *)(raw_pkt + sizeof(struct ether_hdr));

		ether->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		ip->version_ihl = I40E_FDIR_IP_DEFAULT_VERSION_IHL;
		/* set len to by default */
		ip->total_length = rte_cpu_to_be_16(I40E_FDIR_IP_DEFAULT_LEN);
		ip->time_to_live = I40E_FDIR_IP_DEFAULT_TTL;
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		ip->src_addr = fdir_input->flow.ip4_flow.dst_ip;
		ip->dst_addr = fdir_input->flow.ip4_flow.src_ip;
		ip->next_proto_id = next_proto[fdir_input->flow_type];
		break;
	case RTE_ETH_FLOW_TYPE_UDPV6:
	case RTE_ETH_FLOW_TYPE_TCPV6:
	case RTE_ETH_FLOW_TYPE_SCTPV6:
	case RTE_ETH_FLOW_TYPE_IPV6_OTHER:
	case RTE_ETH_FLOW_TYPE_FRAG_IPV6:
		ip6 = (struct ipv6_hdr *)(raw_pkt + sizeof(struct ether_hdr));

		ether->ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
		ip6->vtc_flow =
			rte_cpu_to_be_32(I40E_FDIR_IPv6_DEFAULT_VTC_FLOW);
		ip6->payload_len =
			rte_cpu_to_be_16(I40E_FDIR_IPv6_PAYLOAD_LEN);
		ip6->hop_limits = I40E_FDIR_IPv6_DEFAULT_HOP_LIMITS;

		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		rte_memcpy(&(ip6->src_addr),
			   &(fdir_input->flow.ip6_flow.dst_ip),
			   IPV6_ADDR_LEN);
		rte_memcpy(&(ip6->dst_addr),
			   &(fdir_input->flow.ip6_flow.src_ip),
			   IPV6_ADDR_LEN);
		ip6->proto = next_proto[fdir_input->flow_type];
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown flow type %u.",
			    fdir_input->flow_type);
		break;
	}
}


/*
 * i40e_fdir_construct_pkt - construct packet based on fields in input
 * @pf: board private structure
 * @fdir_input: input set of the flow director entry
 * @raw_pkt: a packet to be constructed
 */
static int
i40e_fdir_construct_pkt(struct i40e_pf *pf,
			     const struct rte_eth_fdir_input *fdir_input,
			     unsigned char *raw_pkt)
{
	unsigned char *payload, *ptr;
	struct udp_hdr *udp;
	struct tcp_hdr *tcp;
	struct sctp_hdr *sctp;
	uint8_t size, dst = 0;
	uint8_t i, pit_idx, set_idx = I40E_FLXPLD_L4_IDX; /* use l4 by default*/

	/* fill the ethernet and IP head */
	i40e_fdir_fill_eth_ip_head(fdir_input, raw_pkt);

	/* fill the L4 head */
	switch (fdir_input->flow_type) {
	case RTE_ETH_FLOW_TYPE_UDPV4:
		udp = (struct udp_hdr *)(raw_pkt + sizeof(struct ether_hdr) +
				sizeof(struct ipv4_hdr));
		payload = (unsigned char *)udp + sizeof(struct udp_hdr);
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		udp->src_port = fdir_input->flow.udp4_flow.dst_port;
		udp->dst_port = fdir_input->flow.udp4_flow.src_port;
		udp->dgram_len = rte_cpu_to_be_16(I40E_FDIR_UDP_DEFAULT_LEN);
		break;

	case RTE_ETH_FLOW_TYPE_TCPV4:
		tcp = (struct tcp_hdr *)(raw_pkt + sizeof(struct ether_hdr) +
					 sizeof(struct ipv4_hdr));
		payload = (unsigned char *)tcp + sizeof(struct tcp_hdr);
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		tcp->src_port = fdir_input->flow.tcp4_flow.dst_port;
		tcp->dst_port = fdir_input->flow.tcp4_flow.src_port;
		tcp->data_off = I40E_FDIR_TCP_DEFAULT_DATAOFF;
		break;

	case RTE_ETH_FLOW_TYPE_SCTPV4:
		sctp = (struct sctp_hdr *)(raw_pkt + sizeof(struct ether_hdr) +
					   sizeof(struct ipv4_hdr));
		payload = (unsigned char *)sctp + sizeof(struct sctp_hdr);
		sctp->tag = fdir_input->flow.sctp4_flow.verify_tag;
		break;

	case RTE_ETH_FLOW_TYPE_IPV4_OTHER:
	case RTE_ETH_FLOW_TYPE_FRAG_IPV4:
		payload = raw_pkt + sizeof(struct ether_hdr) +
			  sizeof(struct ipv4_hdr);
		set_idx = I40E_FLXPLD_L3_IDX;
		break;

	case RTE_ETH_FLOW_TYPE_UDPV6:
		udp = (struct udp_hdr *)(raw_pkt + sizeof(struct ether_hdr) +
					 sizeof(struct ipv6_hdr));
		payload = (unsigned char *)udp + sizeof(struct udp_hdr);
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		udp->src_port = fdir_input->flow.udp6_flow.dst_port;
		udp->dst_port = fdir_input->flow.udp6_flow.src_port;
		udp->dgram_len = rte_cpu_to_be_16(I40E_FDIR_IPv6_PAYLOAD_LEN);
		break;

	case RTE_ETH_FLOW_TYPE_TCPV6:
		tcp = (struct tcp_hdr *)(raw_pkt + sizeof(struct ether_hdr) +
					 sizeof(struct ipv6_hdr));
		payload = (unsigned char *)tcp + sizeof(struct tcp_hdr);
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		tcp->data_off = I40E_FDIR_TCP_DEFAULT_DATAOFF;
		tcp->src_port = fdir_input->flow.udp6_flow.dst_port;
		tcp->dst_port = fdir_input->flow.udp6_flow.src_port;
		break;

	case RTE_ETH_FLOW_TYPE_SCTPV6:
		sctp = (struct sctp_hdr *)(raw_pkt + sizeof(struct ether_hdr) +
					   sizeof(struct ipv6_hdr));
		payload = (unsigned char *)sctp + sizeof(struct sctp_hdr);
		sctp->tag = fdir_input->flow.sctp6_flow.verify_tag;
		break;

	case RTE_ETH_FLOW_TYPE_IPV6_OTHER:
	case RTE_ETH_FLOW_TYPE_FRAG_IPV6:
		payload = raw_pkt + sizeof(struct ether_hdr) +
			  sizeof(struct ipv6_hdr);
		set_idx = I40E_FLXPLD_L3_IDX;
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown flow type %u.", fdir_input->flow_type);
		return -EINVAL;
	}

	/* fill the flexbytes to payload */
	for (i = 0; i < I40E_MAX_FLXPLD_FIED; i++) {
		pit_idx = set_idx * I40E_MAX_FLXPLD_FIED + i;
		size = pf->fdir.flex_set[pit_idx].size;
		if (size == 0)
			continue;
		dst = pf->fdir.flex_set[pit_idx].dst_offset * sizeof(uint16_t);
		ptr = payload +
			pf->fdir.flex_set[pit_idx].src_offset * sizeof(uint16_t);
		(void)rte_memcpy(ptr,
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
	return rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DATA |
			((uint64_t)td_cmd  << I40E_TXD_QW1_CMD_SHIFT) |
			((uint64_t)td_offset << I40E_TXD_QW1_OFFSET_SHIFT) |
			((uint64_t)size  << I40E_TXD_QW1_TX_BUF_SZ_SHIFT) |
			((uint64_t)td_tag  << I40E_TXD_QW1_L2TAG1_SHIFT));
}

/*
 * check the programming status descriptor in rx queue.
 * done after Programming Flow Director is programmed on
 * tx queue
 */
static inline int
i40e_check_fdir_programming_status(struct i40e_rx_queue *rxq)
{
	volatile union i40e_rx_desc *rxdp;
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
			PMD_DRV_LOG(ERR, "unknown programming status"
				    " reported, len = %d, id = %u.", len, id);
		rxdp->wb.qword1.status_error_len = 0;
		rxq->rx_tail++;
		if (unlikely(rxq->rx_tail == rxq->nb_rx_desc))
			rxq->rx_tail = 0;
	}
	return ret;
}

/*
 * i40e_add_del_fdir_filter - add or remove a flow director filter.
 * @pf: board private structure
 * @filter: fdir filter entry
 * @add: 0 - delete, 1 - add
 */
static int
i40e_add_del_fdir_filter(struct rte_eth_dev *dev,
			    const struct rte_eth_fdir_filter *filter,
			    bool add)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	unsigned char *pkt = (unsigned char *)pf->fdir.prg_pkt;
	enum i40e_filter_pctype pctype;
	int ret = 0;

	if (!(pf->flags & I40E_FLAG_FDIR)) {
		PMD_DRV_LOG(ERR, "FDIR is not enabled.");
		return -ENOTSUP;
	}
	if (!I40E_VALID_FLOW_TYPE(filter->input.flow_type)) {
		PMD_DRV_LOG(ERR, "invalid flow_type input.");
		return -EINVAL;
	}
	if (filter->action.rx_queue >= pf->dev_data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid queue ID");
		return -EINVAL;
	}

	memset(pkt, 0, I40E_FDIR_PKT_LEN);

	ret = i40e_fdir_construct_pkt(pf, &filter->input, pkt);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "construct packet for fdir fails.");
		return ret;
	}
	pctype = i40e_flowtype_to_pctype(filter->input.flow_type);
	ret = i40e_fdir_filter_programming(pf, pctype, filter, add);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "fdir programming fails for PCTYPE(%u).",
			    pctype);
		return ret;
	}
	return ret;
}

/*
 * i40e_fdir_filter_programming - Program a flow director filter rule.
 * Is done by Flow Director Programming Descriptor followed by packet
 * structure that contains the filter fields need to match.
 * @pf: board private structure
 * @pctype: pctype
 * @filter: fdir filter entry
 * @add: 0 - delelet, 1 - add
 */
static int
i40e_fdir_filter_programming(struct i40e_pf *pf,
			enum i40e_filter_pctype pctype,
			const struct rte_eth_fdir_filter *filter,
			bool add)
{
	struct i40e_tx_queue *txq = pf->fdir.txq;
	struct i40e_rx_queue *rxq = pf->fdir.rxq;
	const struct rte_eth_fdir_action *fdir_action = &filter->action;
	volatile struct i40e_tx_desc *txdp;
	volatile struct i40e_filter_program_desc *fdirdp;
	uint32_t td_cmd;
	uint16_t i;
	uint8_t dest;

	PMD_DRV_LOG(INFO, "filling filter programming descriptor.");
	fdirdp = (volatile struct i40e_filter_program_desc *)
			(&(txq->tx_ring[txq->tx_tail]));

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

	/* Use LAN VSI Id by default */
	fdirdp->qindex_flex_ptype_vsi |=
		rte_cpu_to_le_32((pf->main_vsi->vsi_id <<
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

	if (fdir_action->behavior == RTE_ETH_FDIR_REJECT)
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DROP_PACKET;
	else
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DIRECT_PACKET_QINDEX;
	fdirdp->dtype_cmd_cntindex |= rte_cpu_to_le_32((dest <<
				I40E_TXD_FLTR_QW1_DEST_SHIFT) &
				I40E_TXD_FLTR_QW1_DEST_MASK);

	fdirdp->dtype_cmd_cntindex |=
		rte_cpu_to_le_32((fdir_action->report_status<<
				I40E_TXD_FLTR_QW1_FD_STATUS_SHIFT) &
				I40E_TXD_FLTR_QW1_FD_STATUS_MASK);

	fdirdp->dtype_cmd_cntindex |=
			rte_cpu_to_le_32(I40E_TXD_FLTR_QW1_CNT_ENA_MASK);
	fdirdp->dtype_cmd_cntindex |=
			rte_cpu_to_le_32((pf->fdir.match_counter_index <<
			I40E_TXD_FLTR_QW1_CNTINDEX_SHIFT) &
			I40E_TXD_FLTR_QW1_CNTINDEX_MASK);

	fdirdp->fd_id = rte_cpu_to_le_32(filter->soft_id);

	PMD_DRV_LOG(INFO, "filling transmit descriptor.");
	txdp = &(txq->tx_ring[txq->tx_tail + 1]);
	txdp->buffer_addr = rte_cpu_to_le_64(pf->fdir.dma_addr);
	td_cmd = I40E_TX_DESC_CMD_EOP |
		 I40E_TX_DESC_CMD_RS  |
		 I40E_TX_DESC_CMD_DUMMY;

	txdp->cmd_type_offset_bsz =
		i40e_build_ctob(td_cmd, 0, I40E_FDIR_PKT_LEN, 0);

	txq->tx_tail += 2; /* set 2 descriptors above, fdirdp and txdp */
	if (txq->tx_tail >= txq->nb_tx_desc)
		txq->tx_tail = 0;
	/* Update the tx tail register */
	rte_wmb();
	I40E_PCI_REG_WRITE(txq->qtx_tail, txq->tx_tail);

	for (i = 0; i < I40E_FDIR_WAIT_COUNT; i++) {
		rte_delay_us(I40E_FDIR_WAIT_INTERVAL_US);
		if (txdp->cmd_type_offset_bsz &
				rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE))
			break;
	}
	if (i >= I40E_FDIR_WAIT_COUNT) {
		PMD_DRV_LOG(ERR, "Failed to program FDIR filter:"
			    " time out to get DD on tx queue.");
		return -ETIMEDOUT;
	}
	/* totally delay 10 ms to check programming status*/
	rte_delay_us((I40E_FDIR_WAIT_COUNT - i) * I40E_FDIR_WAIT_INTERVAL_US);
	if (i40e_check_fdir_programming_status(rxq) < 0) {
		PMD_DRV_LOG(ERR, "Failed to program FDIR filter:"
			    " programming status reported.");
		return -ENOSYS;
	}

	return 0;
}

/*
 * i40e_fdir_flush - clear all filters of Flow Director table
 * @pf: board private structure
 */
static int
i40e_fdir_flush(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint32_t reg;
	uint16_t guarant_cnt, best_cnt;
	int i;

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

/*
 * i40e_fdir_ctrl_func - deal with all operations on flow director.
 * @pf: board private structure
 * @filter_op:operation will be taken.
 * @arg: a pointer to specific structure corresponding to the filter_op
 */
int
i40e_fdir_ctrl_func(struct rte_eth_dev *dev,
		       enum rte_filter_op filter_op,
		       void *arg)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret = 0;

	if (filter_op == RTE_ETH_FILTER_NOP) {
		if (!(pf->flags & I40E_FLAG_FDIR))
			ret = -ENOTSUP;
		return ret;
	}

	if (arg == NULL)
		return -EINVAL;

	switch (filter_op) {
	case RTE_ETH_FILTER_ADD:
		ret = i40e_add_del_fdir_filter(dev,
			(struct rte_eth_fdir_filter *)arg,
			TRUE);
		break;
	case RTE_ETH_FILTER_DELETE:
		ret = i40e_add_del_fdir_filter(dev,
			(struct rte_eth_fdir_filter *)arg,
			FALSE);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u.", filter_op);
		ret = -EINVAL;
		break;
	}
	return ret;
}
