/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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
#include <rte_arp.h>
#include <rte_ip.h>
#include <rte_udp.h>
#include <rte_tcp.h>
#include <rte_sctp.h>

#include "i40e_logs.h"
#include "base/i40e_type.h"
#include "base/i40e_prototype.h"
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
#define I40E_FDIR_IPv6_DEFAULT_VTC_FLOW     0x60000000
#define I40E_FDIR_IPv6_TC_OFFSET            20

#define I40E_FDIR_IPv6_DEFAULT_HOP_LIMITS   0xFF
#define I40E_FDIR_IPv6_PAYLOAD_LEN          380
#define I40E_FDIR_UDP_DEFAULT_LEN           400

/* Wait time for fdir filter programming */
#define I40E_FDIR_MAX_WAIT_US 10000

/* Wait count and interval for fdir filter flush */
#define I40E_FDIR_FLUSH_RETRY       50
#define I40E_FDIR_FLUSH_INTERVAL_MS 5

#define I40E_COUNTER_PF           2
/* Statistic counter index for one pf */
#define I40E_COUNTER_INDEX_FDIR(pf_id)   (0 + (pf_id) * I40E_COUNTER_PF)

#define I40E_FDIR_FLOWS ( \
	(1 << RTE_ETH_FLOW_FRAG_IPV4) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_UDP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_TCP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_SCTP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV4_OTHER) | \
	(1 << RTE_ETH_FLOW_FRAG_IPV6) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_UDP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_TCP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_SCTP) | \
	(1 << RTE_ETH_FLOW_NONFRAG_IPV6_OTHER) | \
	(1 << RTE_ETH_FLOW_L2_PAYLOAD))

static int i40e_fdir_filter_programming(struct i40e_pf *pf,
			enum i40e_filter_pctype pctype,
			const struct rte_eth_fdir_filter *filter,
			bool add);
static int i40e_fdir_filter_convert(const struct rte_eth_fdir_filter *input,
			 struct i40e_fdir_filter *filter);
static struct i40e_fdir_filter *
i40e_sw_fdir_filter_lookup(struct i40e_fdir_info *fdir_info,
			const struct rte_eth_fdir_input *input);
static int i40e_sw_fdir_filter_insert(struct i40e_pf *pf,
				   struct i40e_fdir_filter *filter);

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
			eth_dev->device->driver->name,
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
	pf->fdir.dma_addr = rte_mem_phy2mch(mz->memseg_id, mz->phys_addr);

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
	if (!vsi)
		return;
	int err = i40e_switch_tx_queue(hw, vsi->base_queue, FALSE);
	if (err)
		PMD_DRV_LOG(DEBUG, "Failed to do FDIR TX switch off");
	err = i40e_switch_rx_queue(hw, vsi->base_queue, FALSE);
	if (err)
		PMD_DRV_LOG(DEBUG, "Failed to do FDIR RX switch off");
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
	     pctype <= I40E_FILTER_PCTYPE_L2_PAYLOAD; pctype++) {
		if (hw->mac.type == I40E_MAC_X722) {
			if (!I40E_VALID_PCTYPE_X722(
				 (enum i40e_filter_pctype)pctype))
				continue;
		} else {
			if (!I40E_VALID_PCTYPE(
				 (enum i40e_filter_pctype)pctype))
				continue;
		}
		pf->fdir.flex_mask[pctype].word_mask = 0;
		i40e_write_rx_ctl(hw, I40E_PRTQF_FD_FLXINSET(pctype), 0);
		for (i = 0; i < I40E_FDIR_BITMASK_NUM_WORD; i++) {
			pf->fdir.flex_mask[pctype].bitmask[i].offset = 0;
			pf->fdir.flex_mask[pctype].bitmask[i].mask = 0;
			i40e_write_rx_ctl(hw, I40E_PRTQF_FD_MSK(pctype, i), 0);
		}
	}
}

#define I40E_VALIDATE_FLEX_PIT(flex_pit1, flex_pit2) do { \
	if ((flex_pit2).src_offset < \
		(flex_pit1).src_offset + (flex_pit1).size) { \
		PMD_DRV_LOG(ERR, "src_offset should be not" \
			" less than than previous offset" \
			" + previous FSIZE."); \
		return -EINVAL; \
	} \
} while (0)

/*
 * i40e_srcoff_to_flx_pit - transform the src_offset into flex_pit structure,
 * and the flex_pit will be sorted by it's src_offset value
 */
static inline uint16_t
i40e_srcoff_to_flx_pit(const uint16_t *src_offset,
			struct i40e_fdir_flex_pit *flex_pit)
{
	uint16_t src_tmp, size, num = 0;
	uint16_t i, k, j = 0;

	while (j < I40E_FDIR_MAX_FLEX_LEN) {
		size = 1;
		for (; j < I40E_FDIR_MAX_FLEX_LEN - 1; j++) {
			if (src_offset[j + 1] == src_offset[j] + 1)
				size++;
			else
				break;
		}
		src_tmp = src_offset[j] + 1 - size;
		/* the flex_pit need to be sort by src_offset */
		for (i = 0; i < num; i++) {
			if (src_tmp < flex_pit[i].src_offset)
				break;
		}
		/* if insert required, move backward */
		for (k = num; k > i; k--)
			flex_pit[k] = flex_pit[k - 1];
		/* insert */
		flex_pit[i].dst_offset = j + 1 - size;
		flex_pit[i].src_offset = src_tmp;
		flex_pit[i].size = size;
		j++;
		num++;
	}
	return num;
}

/* i40e_check_fdir_flex_payload -check flex payload configuration arguments */
static inline int
i40e_check_fdir_flex_payload(const struct rte_eth_flex_payload_cfg *flex_cfg)
{
	struct i40e_fdir_flex_pit flex_pit[I40E_FDIR_MAX_FLEX_LEN];
	uint16_t num, i;

	for (i = 0; i < I40E_FDIR_MAX_FLEX_LEN; i++) {
		if (flex_cfg->src_offset[i] >= I40E_MAX_FLX_SOURCE_OFF) {
			PMD_DRV_LOG(ERR, "exceeds maxmial payload limit.");
			return -EINVAL;
		}
	}

	memset(flex_pit, 0, sizeof(flex_pit));
	num = i40e_srcoff_to_flx_pit(flex_cfg->src_offset, flex_pit);
	if (num > I40E_MAX_FLXPLD_FIED) {
		PMD_DRV_LOG(ERR, "exceeds maxmial number of flex fields.");
		return -EINVAL;
	}
	for (i = 0; i < num; i++) {
		if (flex_pit[i].size & 0x01 || flex_pit[i].dst_offset & 0x01 ||
			flex_pit[i].src_offset & 0x01) {
			PMD_DRV_LOG(ERR, "flexpayload should be measured"
				" in word");
			return -EINVAL;
		}
		if (i != num - 1)
			I40E_VALIDATE_FLEX_PIT(flex_pit[i], flex_pit[i + 1]);
	}
	return 0;
}

/*
 * i40e_check_fdir_flex_conf -check if the flex payload and mask configuration
 * arguments are valid
 */
static int
i40e_check_fdir_flex_conf(const struct rte_eth_fdir_flex_conf *conf)
{
	const struct rte_eth_flex_payload_cfg *flex_cfg;
	const struct rte_eth_fdir_flex_mask *flex_mask;
	uint16_t mask_tmp;
	uint8_t nb_bitmask;
	uint16_t i, j;
	int ret = 0;

	if (conf == NULL) {
		PMD_DRV_LOG(INFO, "NULL pointer.");
		return -EINVAL;
	}
	/* check flexible payload setting configuration */
	if (conf->nb_payloads > RTE_ETH_L4_PAYLOAD) {
		PMD_DRV_LOG(ERR, "invalid number of payload setting.");
		return -EINVAL;
	}
	for (i = 0; i < conf->nb_payloads; i++) {
		flex_cfg = &conf->flex_set[i];
		if (flex_cfg->type > RTE_ETH_L4_PAYLOAD) {
			PMD_DRV_LOG(ERR, "invalid payload type.");
			return -EINVAL;
		}
		ret = i40e_check_fdir_flex_payload(flex_cfg);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "invalid flex payload arguments.");
			return -EINVAL;
		}
	}

	/* check flex mask setting configuration */
	if (conf->nb_flexmasks >= RTE_ETH_FLOW_MAX) {
		PMD_DRV_LOG(ERR, "invalid number of flex masks.");
		return -EINVAL;
	}
	for (i = 0; i < conf->nb_flexmasks; i++) {
		flex_mask = &conf->flex_mask[i];
		if (!I40E_VALID_FLOW(flex_mask->flow_type)) {
			PMD_DRV_LOG(WARNING, "invalid flow type.");
			return -EINVAL;
		}
		nb_bitmask = 0;
		for (j = 0; j < I40E_FDIR_MAX_FLEX_LEN; j += sizeof(uint16_t)) {
			mask_tmp = I40E_WORD(flex_mask->mask[j],
					     flex_mask->mask[j + 1]);
			if (mask_tmp != 0x0 && mask_tmp != UINT16_MAX) {
				nb_bitmask++;
				if (nb_bitmask > I40E_FDIR_BITMASK_NUM_WORD) {
					PMD_DRV_LOG(ERR, " exceed maximal"
						" number of bitmasks.");
					return -EINVAL;
				}
			}
		}
	}
	return 0;
}

/*
 * i40e_set_flx_pld_cfg -configure the rule how bytes stream is extracted as flexible payload
 * @pf: board private structure
 * @cfg: the rule how bytes stream is extracted as flexible payload
 */
static void
i40e_set_flx_pld_cfg(struct i40e_pf *pf,
			 const struct rte_eth_flex_payload_cfg *cfg)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_fdir_flex_pit flex_pit[I40E_MAX_FLXPLD_FIED];
	uint32_t flx_pit;
	uint16_t num, min_next_off;  /* in words */
	uint8_t field_idx = 0;
	uint8_t layer_idx = 0;
	uint16_t i;

	if (cfg->type == RTE_ETH_L2_PAYLOAD)
		layer_idx = I40E_FLXPLD_L2_IDX;
	else if (cfg->type == RTE_ETH_L3_PAYLOAD)
		layer_idx = I40E_FLXPLD_L3_IDX;
	else if (cfg->type == RTE_ETH_L4_PAYLOAD)
		layer_idx = I40E_FLXPLD_L4_IDX;

	memset(flex_pit, 0, sizeof(flex_pit));
	num = i40e_srcoff_to_flx_pit(cfg->src_offset, flex_pit);

	for (i = 0; i < RTE_MIN(num, RTE_DIM(flex_pit)); i++) {
		field_idx = layer_idx * I40E_MAX_FLXPLD_FIED + i;
		/* record the info in fdir structure */
		pf->fdir.flex_set[field_idx].src_offset =
			flex_pit[i].src_offset / sizeof(uint16_t);
		pf->fdir.flex_set[field_idx].size =
			flex_pit[i].size / sizeof(uint16_t);
		pf->fdir.flex_set[field_idx].dst_offset =
			flex_pit[i].dst_offset / sizeof(uint16_t);
		flx_pit = MK_FLX_PIT(pf->fdir.flex_set[field_idx].src_offset,
				pf->fdir.flex_set[field_idx].size,
				pf->fdir.flex_set[field_idx].dst_offset);

		I40E_WRITE_REG(hw, I40E_PRTQF_FLX_PIT(field_idx), flx_pit);
	}
	min_next_off = pf->fdir.flex_set[field_idx].src_offset +
				pf->fdir.flex_set[field_idx].size;

	for (; i < I40E_MAX_FLXPLD_FIED; i++) {
		/* set the non-used register obeying register's constrain */
		flx_pit = MK_FLX_PIT(min_next_off, NONUSE_FLX_PIT_FSIZE,
			   NONUSE_FLX_PIT_DEST_OFF);
		I40E_WRITE_REG(hw,
			I40E_PRTQF_FLX_PIT(layer_idx * I40E_MAX_FLXPLD_FIED + i),
			flx_pit);
		min_next_off++;
	}
}

/*
 * i40e_set_flex_mask_on_pctype - configure the mask on flexible payload
 * @pf: board private structure
 * @pctype: packet classify type
 * @flex_masks: mask for flexible payload
 */
static void
i40e_set_flex_mask_on_pctype(struct i40e_pf *pf,
		enum i40e_filter_pctype pctype,
		const struct rte_eth_fdir_flex_mask *mask_cfg)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	struct i40e_fdir_flex_mask *flex_mask;
	uint32_t flxinset, fd_mask;
	uint16_t mask_tmp;
	uint8_t i, nb_bitmask = 0;

	flex_mask = &pf->fdir.flex_mask[pctype];
	memset(flex_mask, 0, sizeof(struct i40e_fdir_flex_mask));
	for (i = 0; i < I40E_FDIR_MAX_FLEX_LEN; i += sizeof(uint16_t)) {
		mask_tmp = I40E_WORD(mask_cfg->mask[i], mask_cfg->mask[i + 1]);
		if (mask_tmp != 0x0) {
			flex_mask->word_mask |=
				I40E_FLEX_WORD_MASK(i / sizeof(uint16_t));
			if (mask_tmp != UINT16_MAX) {
				/* set bit mask */
				flex_mask->bitmask[nb_bitmask].mask = ~mask_tmp;
				flex_mask->bitmask[nb_bitmask].offset =
					i / sizeof(uint16_t);
				nb_bitmask++;
			}
		}
	}
	/* write mask to hw */
	flxinset = (flex_mask->word_mask <<
		I40E_PRTQF_FD_FLXINSET_INSET_SHIFT) &
		I40E_PRTQF_FD_FLXINSET_INSET_MASK;
	i40e_write_rx_ctl(hw, I40E_PRTQF_FD_FLXINSET(pctype), flxinset);

	for (i = 0; i < nb_bitmask; i++) {
		fd_mask = (flex_mask->bitmask[i].mask <<
			I40E_PRTQF_FD_MSK_MASK_SHIFT) &
			I40E_PRTQF_FD_MSK_MASK_MASK;
		fd_mask |= ((flex_mask->bitmask[i].offset +
			I40E_FLX_OFFSET_IN_FIELD_VECTOR) <<
			I40E_PRTQF_FD_MSK_OFFSET_SHIFT) &
			I40E_PRTQF_FD_MSK_OFFSET_MASK;
		i40e_write_rx_ctl(hw, I40E_PRTQF_FD_MSK(pctype, i), fd_mask);
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
	struct rte_eth_fdir_flex_conf *conf;
	enum i40e_filter_pctype pctype;
	uint32_t val;
	uint8_t i;
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

	conf = &dev->data->dev_conf.fdir_conf.flex_conf;
	ret = i40e_check_fdir_flex_conf(conf);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, " invalid configuration arguments.");
		return -EINVAL;
	}
	/* configure flex payload */
	for (i = 0; i < conf->nb_payloads; i++)
		i40e_set_flx_pld_cfg(pf, &conf->flex_set[i]);
	/* configure flex mask*/
	for (i = 0; i < conf->nb_flexmasks; i++) {
		if (hw->mac.type == I40E_MAC_X722) {
			/* get translated pctype value in fd pctype register */
			pctype = (enum i40e_filter_pctype)i40e_read_rx_ctl(
				hw, I40E_GLQF_FD_PCTYPES(
				(int)i40e_flowtype_to_pctype(
				conf->flex_mask[i].flow_type)));
		} else
			pctype = i40e_flowtype_to_pctype(
				conf->flex_mask[i].flow_type);

		i40e_set_flex_mask_on_pctype(pf, pctype, &conf->flex_mask[i]);
	}

	return ret;
}

static inline int
i40e_fdir_fill_eth_ip_head(const struct rte_eth_fdir_input *fdir_input,
			   unsigned char *raw_pkt,
			   bool vlan)
{
	static uint8_t vlan_frame[] = {0x81, 0, 0, 0};
	uint16_t *ether_type;
	uint8_t len = 2 * sizeof(struct ether_addr);
	struct ipv4_hdr *ip;
	struct ipv6_hdr *ip6;
	static const uint8_t next_proto[] = {
		[RTE_ETH_FLOW_FRAG_IPV4] = IPPROTO_IP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_TCP] = IPPROTO_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_UDP] = IPPROTO_UDP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_SCTP] = IPPROTO_SCTP,
		[RTE_ETH_FLOW_NONFRAG_IPV4_OTHER] = IPPROTO_IP,
		[RTE_ETH_FLOW_FRAG_IPV6] = IPPROTO_NONE,
		[RTE_ETH_FLOW_NONFRAG_IPV6_TCP] = IPPROTO_TCP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_UDP] = IPPROTO_UDP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_SCTP] = IPPROTO_SCTP,
		[RTE_ETH_FLOW_NONFRAG_IPV6_OTHER] = IPPROTO_NONE,
	};

	raw_pkt += 2 * sizeof(struct ether_addr);
	if (vlan && fdir_input->flow_ext.vlan_tci) {
		rte_memcpy(raw_pkt, vlan_frame, sizeof(vlan_frame));
		rte_memcpy(raw_pkt + sizeof(uint16_t),
			   &fdir_input->flow_ext.vlan_tci,
			   sizeof(uint16_t));
		raw_pkt += sizeof(vlan_frame);
		len += sizeof(vlan_frame);
	}
	ether_type = (uint16_t *)raw_pkt;
	raw_pkt += sizeof(uint16_t);
	len += sizeof(uint16_t);

	switch (fdir_input->flow_type) {
	case RTE_ETH_FLOW_L2_PAYLOAD:
		*ether_type = fdir_input->flow.l2_flow.ether_type;
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
	case RTE_ETH_FLOW_FRAG_IPV4:
		ip = (struct ipv4_hdr *)raw_pkt;

		*ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv4);
		ip->version_ihl = I40E_FDIR_IP_DEFAULT_VERSION_IHL;
		/* set len to by default */
		ip->total_length = rte_cpu_to_be_16(I40E_FDIR_IP_DEFAULT_LEN);
		ip->next_proto_id = fdir_input->flow.ip4_flow.proto ?
					fdir_input->flow.ip4_flow.proto :
					next_proto[fdir_input->flow_type];
		ip->time_to_live = fdir_input->flow.ip4_flow.ttl ?
					fdir_input->flow.ip4_flow.ttl :
					I40E_FDIR_IP_DEFAULT_TTL;
		ip->type_of_service = fdir_input->flow.ip4_flow.tos;
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		ip->src_addr = fdir_input->flow.ip4_flow.dst_ip;
		ip->dst_addr = fdir_input->flow.ip4_flow.src_ip;
		len += sizeof(struct ipv4_hdr);
		break;
	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_SCTP:
	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
	case RTE_ETH_FLOW_FRAG_IPV6:
		ip6 = (struct ipv6_hdr *)raw_pkt;

		*ether_type = rte_cpu_to_be_16(ETHER_TYPE_IPv6);
		ip6->vtc_flow =
			rte_cpu_to_be_32(I40E_FDIR_IPv6_DEFAULT_VTC_FLOW |
					 (fdir_input->flow.ipv6_flow.tc <<
					  I40E_FDIR_IPv6_TC_OFFSET));
		ip6->payload_len =
			rte_cpu_to_be_16(I40E_FDIR_IPv6_PAYLOAD_LEN);
		ip6->proto = fdir_input->flow.ipv6_flow.proto ?
					fdir_input->flow.ipv6_flow.proto :
					next_proto[fdir_input->flow_type];
		ip6->hop_limits = fdir_input->flow.ipv6_flow.hop_limits ?
					fdir_input->flow.ipv6_flow.hop_limits :
					I40E_FDIR_IPv6_DEFAULT_HOP_LIMITS;
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		rte_memcpy(&(ip6->src_addr),
			   &(fdir_input->flow.ipv6_flow.dst_ip),
			   IPV6_ADDR_LEN);
		rte_memcpy(&(ip6->dst_addr),
			   &(fdir_input->flow.ipv6_flow.src_ip),
			   IPV6_ADDR_LEN);
		len += sizeof(struct ipv6_hdr);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown flow type %u.",
			    fdir_input->flow_type);
		return -1;
	}
	return len;
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
	int len;

	/* fill the ethernet and IP head */
	len = i40e_fdir_fill_eth_ip_head(fdir_input, raw_pkt,
					 !!fdir_input->flow_ext.vlan_tci);
	if (len < 0)
		return -EINVAL;

	/* fill the L4 head */
	switch (fdir_input->flow_type) {
	case RTE_ETH_FLOW_NONFRAG_IPV4_UDP:
		udp = (struct udp_hdr *)(raw_pkt + len);
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

	case RTE_ETH_FLOW_NONFRAG_IPV4_TCP:
		tcp = (struct tcp_hdr *)(raw_pkt + len);
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

	case RTE_ETH_FLOW_NONFRAG_IPV4_SCTP:
		sctp = (struct sctp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)sctp + sizeof(struct sctp_hdr);
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		sctp->src_port = fdir_input->flow.sctp4_flow.dst_port;
		sctp->dst_port = fdir_input->flow.sctp4_flow.src_port;
		sctp->tag = fdir_input->flow.sctp4_flow.verify_tag;
		break;

	case RTE_ETH_FLOW_NONFRAG_IPV4_OTHER:
	case RTE_ETH_FLOW_FRAG_IPV4:
		payload = raw_pkt + len;
		set_idx = I40E_FLXPLD_L3_IDX;
		break;

	case RTE_ETH_FLOW_NONFRAG_IPV6_UDP:
		udp = (struct udp_hdr *)(raw_pkt + len);
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

	case RTE_ETH_FLOW_NONFRAG_IPV6_TCP:
		tcp = (struct tcp_hdr *)(raw_pkt + len);
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

	case RTE_ETH_FLOW_NONFRAG_IPV6_SCTP:
		sctp = (struct sctp_hdr *)(raw_pkt + len);
		payload = (unsigned char *)sctp + sizeof(struct sctp_hdr);
		/*
		 * The source and destination fields in the transmitted packet
		 * need to be presented in a reversed order with respect
		 * to the expected received packets.
		 */
		sctp->src_port = fdir_input->flow.sctp6_flow.dst_port;
		sctp->dst_port = fdir_input->flow.sctp6_flow.src_port;
		sctp->tag = fdir_input->flow.sctp6_flow.verify_tag;
		break;

	case RTE_ETH_FLOW_NONFRAG_IPV6_OTHER:
	case RTE_ETH_FLOW_FRAG_IPV6:
		payload = raw_pkt + len;
		set_idx = I40E_FLXPLD_L3_IDX;
		break;
	case RTE_ETH_FLOW_L2_PAYLOAD:
		payload = raw_pkt + len;
		/*
		 * ARP packet is a special case on which the payload
		 * starts after the whole ARP header
		 */
		if (fdir_input->flow.l2_flow.ether_type ==
				rte_cpu_to_be_16(ETHER_TYPE_ARP))
			payload += sizeof(struct arp_hdr);
		set_idx = I40E_FLXPLD_L2_IDX;
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

static int
i40e_fdir_filter_convert(const struct rte_eth_fdir_filter *input,
			 struct i40e_fdir_filter *filter)
{
	rte_memcpy(&filter->fdir, input, sizeof(struct rte_eth_fdir_filter));
	return 0;
}

/* Check if there exists the flow director filter */
static struct i40e_fdir_filter *
i40e_sw_fdir_filter_lookup(struct i40e_fdir_info *fdir_info,
			const struct rte_eth_fdir_input *input)
{
	int ret;

	ret = rte_hash_lookup(fdir_info->hash_table, (const void *)input);
	if (ret < 0)
		return NULL;

	return fdir_info->hash_map[ret];
}

/* Add a flow director filter into the SW list */
static int
i40e_sw_fdir_filter_insert(struct i40e_pf *pf, struct i40e_fdir_filter *filter)
{
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	int ret;

	ret = rte_hash_add_key(fdir_info->hash_table,
			       &filter->fdir.input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert fdir filter to hash table %d!",
			    ret);
		return ret;
	}
	fdir_info->hash_map[ret] = filter;

	TAILQ_INSERT_TAIL(&fdir_info->fdir_list, filter, rules);

	return 0;
}

/* Delete a flow director filter from the SW list */
int
i40e_sw_fdir_filter_del(struct i40e_pf *pf, struct rte_eth_fdir_input *input)
{
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_filter *filter;
	int ret;

	ret = rte_hash_del_key(fdir_info->hash_table, input);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete fdir filter to hash table %d!",
			    ret);
		return ret;
	}
	filter = fdir_info->hash_map[ret];
	fdir_info->hash_map[ret] = NULL;

	TAILQ_REMOVE(&fdir_info->fdir_list, filter, rules);
	rte_free(filter);

	return 0;
}

/*
 * i40e_add_del_fdir_filter - add or remove a flow director filter.
 * @pf: board private structure
 * @filter: fdir filter entry
 * @add: 0 - delete, 1 - add
 */
int
i40e_add_del_fdir_filter(struct rte_eth_dev *dev,
			    const struct rte_eth_fdir_filter *filter,
			    bool add)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	unsigned char *pkt = (unsigned char *)pf->fdir.prg_pkt;
	enum i40e_filter_pctype pctype;
	struct i40e_fdir_info *fdir_info = &pf->fdir;
	struct i40e_fdir_filter *fdir_filter, *node;
	struct i40e_fdir_filter check_filter; /* Check if the filter exists */
	int ret = 0;

	if (dev->data->dev_conf.fdir_conf.mode != RTE_FDIR_MODE_PERFECT) {
		PMD_DRV_LOG(ERR, "FDIR is not enabled, please"
			" check the mode in fdir_conf.");
		return -ENOTSUP;
	}

	if (!I40E_VALID_FLOW(filter->input.flow_type)) {
		PMD_DRV_LOG(ERR, "invalid flow_type input.");
		return -EINVAL;
	}
	if (filter->action.rx_queue >= pf->dev_data->nb_rx_queues) {
		PMD_DRV_LOG(ERR, "Invalid queue ID");
		return -EINVAL;
	}
	if (filter->input.flow_ext.is_vf &&
		filter->input.flow_ext.dst_id >= pf->vf_num) {
		PMD_DRV_LOG(ERR, "Invalid VF ID");
		return -EINVAL;
	}

	/* Check if there is the filter in SW list */
	memset(&check_filter, 0, sizeof(check_filter));
	i40e_fdir_filter_convert(filter, &check_filter);
	node = i40e_sw_fdir_filter_lookup(fdir_info, &check_filter.fdir.input);
	if (add && node) {
		PMD_DRV_LOG(ERR,
			    "Conflict with existing flow director rules!");
		return -EINVAL;
	}

	if (!add && !node) {
		PMD_DRV_LOG(ERR,
			    "There's no corresponding flow firector filter!");
		return -EINVAL;
	}

	memset(pkt, 0, I40E_FDIR_PKT_LEN);

	ret = i40e_fdir_construct_pkt(pf, &filter->input, pkt);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "construct packet for fdir fails.");
		return ret;
	}

	if (hw->mac.type == I40E_MAC_X722) {
		/* get translated pctype value in fd pctype register */
		pctype = (enum i40e_filter_pctype)i40e_read_rx_ctl(
			hw, I40E_GLQF_FD_PCTYPES(
			(int)i40e_flowtype_to_pctype(
			filter->input.flow_type)));
	} else
		pctype = i40e_flowtype_to_pctype(filter->input.flow_type);

	ret = i40e_fdir_filter_programming(pf, pctype, filter, add);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "fdir programming fails for PCTYPE(%u).",
			    pctype);
		return ret;
	}

	if (add) {
		fdir_filter = rte_zmalloc("fdir_filter",
					  sizeof(*fdir_filter), 0);
		rte_memcpy(fdir_filter, &check_filter, sizeof(check_filter));
		ret = i40e_sw_fdir_filter_insert(pf, fdir_filter);
	} else {
		ret = i40e_sw_fdir_filter_del(pf, &node->fdir.input);
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
 * @add: 0 - delete, 1 - add
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
	uint16_t vsi_id, i;
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

	if (fdir_action->behavior == RTE_ETH_FDIR_REJECT)
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DROP_PACKET;
	else if (fdir_action->behavior == RTE_ETH_FDIR_ACCEPT)
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DIRECT_PACKET_QINDEX;
	else if (fdir_action->behavior == RTE_ETH_FDIR_PASSTHRU)
		dest = I40E_FILTER_PROGRAM_DESC_DEST_DIRECT_PACKET_OTHER;
	else {
		PMD_DRV_LOG(ERR, "Failed to program FDIR filter:"
			    " unsupported fdir behavior.");
		return -EINVAL;
	}

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
			rte_cpu_to_le_32(
			((uint32_t)pf->fdir.match_counter_index <<
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
	for (i = 0; i < I40E_FDIR_MAX_WAIT_US; i++) {
		if ((txdp->cmd_type_offset_bsz &
				rte_cpu_to_le_64(I40E_TXD_QW1_DTYPE_MASK)) ==
				rte_cpu_to_le_64(I40E_TX_DESC_DTYPE_DESC_DONE))
			break;
		rte_delay_us(1);
	}
	if (i >= I40E_FDIR_MAX_WAIT_US) {
		PMD_DRV_LOG(ERR, "Failed to program FDIR filter:"
			    " time out to get DD on tx queue.");
		return -ETIMEDOUT;
	}
	/* totally delay 10 ms to check programming status*/
	for (; i < I40E_FDIR_MAX_WAIT_US; i++) {
		if (i40e_check_fdir_programming_status(rxq) >= 0)
			return 0;
		rte_delay_us(1);
	}
	PMD_DRV_LOG(ERR,
		"Failed to program FDIR filter: programming status reported.");
	return -ETIMEDOUT;
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
		if (layer_idx == I40E_FLXPLD_L2_IDX)
			ptr->type = RTE_ETH_L2_PAYLOAD;
		else if (layer_idx == I40E_FLXPLD_L3_IDX)
			ptr->type = RTE_ETH_L3_PAYLOAD;
		else if (layer_idx == I40E_FLXPLD_L4_IDX)
			ptr->type = RTE_ETH_L4_PAYLOAD;

		for (i = 0; i < I40E_MAX_FLXPLD_FIED; i++) {
			flex_pit = &pf->fdir.flex_set[layer_idx *
				I40E_MAX_FLXPLD_FIED + i];
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
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint16_t flow_type;
	uint8_t i, j;
	uint16_t off_bytes, mask_tmp;

	for (i = I40E_FILTER_PCTYPE_NONF_IPV4_UDP;
	     i <= I40E_FILTER_PCTYPE_L2_PAYLOAD;
	     i++) {
		mask =  &pf->fdir.flex_mask[i];
		if (hw->mac.type == I40E_MAC_X722) {
			if (!I40E_VALID_PCTYPE_X722((enum i40e_filter_pctype)i))
				continue;
		} else {
			if (!I40E_VALID_PCTYPE((enum i40e_filter_pctype)i))
				continue;
		}
		flow_type = i40e_pctype_to_flowtype((enum i40e_filter_pctype)i);
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
static void
i40e_fdir_info_get(struct rte_eth_dev *dev, struct rte_eth_fdir_info *fdir)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint16_t num_flex_set = 0;
	uint16_t num_flex_mask = 0;

	if (dev->data->dev_conf.fdir_conf.mode == RTE_FDIR_MODE_PERFECT)
		fdir->mode = RTE_FDIR_MODE_PERFECT;
	else
		fdir->mode = RTE_FDIR_MODE_NONE;

	fdir->guarant_spc =
		(uint32_t)hw->func_caps.fd_filters_guaranteed;
	fdir->best_spc =
		(uint32_t)hw->func_caps.fd_filters_best_effort;
	fdir->max_flexpayload = I40E_FDIR_MAX_FLEX_LEN;
	fdir->flow_types_mask[0] = I40E_FDIR_FLOWS;
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
static void
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

static int
i40e_fdir_filter_set(struct rte_eth_dev *dev,
		     struct rte_eth_fdir_filter_info *info)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	int ret = 0;

	if (!info) {
		PMD_DRV_LOG(ERR, "Invalid pointer");
		return -EFAULT;
	}

	switch (info->info_type) {
	case RTE_ETH_FDIR_FILTER_INPUT_SET_SELECT:
		ret = i40e_fdir_filter_inset_select(pf,
				&(info->info.input_set_conf));
		break;
	default:
		PMD_DRV_LOG(ERR, "FD filter info type (%d) not supported",
			    info->info_type);
		return -EINVAL;
	}

	return ret;
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

	if ((pf->flags & I40E_FLAG_FDIR) == 0)
		return -ENOTSUP;

	if (filter_op == RTE_ETH_FILTER_NOP)
		return 0;

	if (arg == NULL && filter_op != RTE_ETH_FILTER_FLUSH)
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
	case RTE_ETH_FILTER_FLUSH:
		ret = i40e_fdir_flush(dev);
		break;
	case RTE_ETH_FILTER_INFO:
		i40e_fdir_info_get(dev, (struct rte_eth_fdir_info *)arg);
		break;
	case RTE_ETH_FILTER_SET:
		ret = i40e_fdir_filter_set(dev,
			(struct rte_eth_fdir_filter_info *)arg);
		break;
	case RTE_ETH_FILTER_STATS:
		i40e_fdir_stats_get(dev, (struct rte_eth_fdir_stats *)arg);
		break;
	default:
		PMD_DRV_LOG(ERR, "unknown operation %u.", filter_op);
		ret = -EINVAL;
		break;
	}
	return ret;
}

/* Restore flow director filter */
void
i40e_fdir_filter_restore(struct i40e_pf *pf)
{
	struct rte_eth_dev *dev = I40E_VSI_TO_ETH_DEV(pf->main_vsi);
	struct i40e_fdir_filter_list *fdir_list = &pf->fdir.fdir_list;
	struct i40e_fdir_filter *f;
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint32_t fdstat;
	uint32_t guarant_cnt;  /**< Number of filters in guaranteed spaces. */
	uint32_t best_cnt;     /**< Number of filters in best effort spaces. */

	TAILQ_FOREACH(f, fdir_list, rules)
		i40e_add_del_fdir_filter(dev, &f->fdir, TRUE);

	fdstat = I40E_READ_REG(hw, I40E_PFQF_FDSTAT);
	guarant_cnt =
		(uint32_t)((fdstat & I40E_PFQF_FDSTAT_GUARANT_CNT_MASK) >>
			   I40E_PFQF_FDSTAT_GUARANT_CNT_SHIFT);
	best_cnt =
		(uint32_t)((fdstat & I40E_PFQF_FDSTAT_BEST_CNT_MASK) >>
			   I40E_PFQF_FDSTAT_BEST_CNT_SHIFT);

	PMD_DRV_LOG(INFO, "FDIR: Guarant count: %d,  Best count: %d",
		    guarant_cnt, best_cnt);
}
