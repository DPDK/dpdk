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

#include "i40e_logs.h"
#include "i40e/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"

#define I40E_FDIR_MZ_NAME          "FDIR_MEMZONE"
#define I40E_FDIR_PKT_LEN                   512


/* Wait count and interval for fdir filter flush */
#define I40E_FDIR_FLUSH_RETRY       50
#define I40E_FDIR_FLUSH_INTERVAL_MS 5

#define I40E_COUNTER_PF           2
/* Statistic counter index for one pf */
#define I40E_COUNTER_INDEX_FDIR(pf_id)   (0 + (pf_id) * I40E_COUNTER_PF)
#define I40E_FLX_OFFSET_IN_FIELD_VECTOR   50

static int i40e_fdir_rx_queue_init(struct i40e_rx_queue *rxq);
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
