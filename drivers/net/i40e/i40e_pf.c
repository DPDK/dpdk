/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
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
#include <inttypes.h>

#include <rte_string_fns.h>
#include <rte_pci.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_memzone.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>

#include "i40e_logs.h"
#include "base/i40e_prototype.h"
#include "base/i40e_adminq_cmd.h"
#include "base/i40e_type.h"
#include "i40e_ethdev.h"
#include "i40e_rxtx.h"
#include "i40e_pf.h"
#include "rte_pmd_i40e.h"

#define I40E_CFG_CRCSTRIP_DEFAULT 1

static int
i40e_pf_host_switch_queues(struct i40e_pf_vf *vf,
			   struct virtchnl_queue_select *qsel,
			   bool on);

/**
 * Bind PF queues with VSI and VF.
 **/
static int
i40e_pf_vf_queues_mapping(struct i40e_pf_vf *vf)
{
	int i;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	uint16_t vsi_id = vf->vsi->vsi_id;
	uint16_t vf_id  = vf->vf_idx;
	uint16_t nb_qps = vf->vsi->nb_qps;
	uint16_t qbase  = vf->vsi->base_queue;
	uint16_t q1, q2;
	uint32_t val;

	/*
	 * VF should use scatter range queues. So, it needn't
	 * to set QBASE in this register.
	 */
	i40e_write_rx_ctl(hw, I40E_VSILAN_QBASE(vsi_id),
			  I40E_VSILAN_QBASE_VSIQTABLE_ENA_MASK);

	/* Set to enable VFLAN_QTABLE[] registers valid */
	I40E_WRITE_REG(hw, I40E_VPLAN_MAPENA(vf_id),
		I40E_VPLAN_MAPENA_TXRX_ENA_MASK);

	/* map PF queues to VF */
	for (i = 0; i < nb_qps; i++) {
		val = ((qbase + i) & I40E_VPLAN_QTABLE_QINDEX_MASK);
		I40E_WRITE_REG(hw, I40E_VPLAN_QTABLE(i, vf_id), val);
	}

	/* map PF queues to VSI */
	for (i = 0; i < I40E_MAX_QP_NUM_PER_VF / 2; i++) {
		if (2 * i > nb_qps - 1)
			q1 = I40E_VSILAN_QTABLE_QINDEX_0_MASK;
		else
			q1 = qbase + 2 * i;

		if (2 * i + 1 > nb_qps - 1)
			q2 = I40E_VSILAN_QTABLE_QINDEX_0_MASK;
		else
			q2 = qbase + 2 * i + 1;

		val = (q2 << I40E_VSILAN_QTABLE_QINDEX_1_SHIFT) + q1;
		i40e_write_rx_ctl(hw, I40E_VSILAN_QTABLE(i, vsi_id), val);
	}
	I40E_WRITE_FLUSH(hw);

	return I40E_SUCCESS;
}


/**
 * Proceed VF reset operation.
 */
int
i40e_pf_host_vf_reset(struct i40e_pf_vf *vf, bool do_hw_reset)
{
	uint32_t val, i;
	struct i40e_hw *hw;
	struct i40e_pf *pf;
	uint16_t vf_id, abs_vf_id, vf_msix_num;
	int ret;
	struct virtchnl_queue_select qsel;

	if (vf == NULL)
		return -EINVAL;

	pf = vf->pf;
	hw = I40E_PF_TO_HW(vf->pf);
	vf_id = vf->vf_idx;
	abs_vf_id = vf_id + hw->func_caps.vf_base_id;

	/* Notify VF that we are in VFR progress */
	I40E_WRITE_REG(hw, I40E_VFGEN_RSTAT1(vf_id), VIRTCHNL_VFR_INPROGRESS);

	/*
	 * If require a SW VF reset, a VFLR interrupt will be generated,
	 * this function will be called again. To avoid it,
	 * disable interrupt first.
	 */
	if (do_hw_reset) {
		vf->state = I40E_VF_INRESET;
		val = I40E_READ_REG(hw, I40E_VPGEN_VFRTRIG(vf_id));
		val |= I40E_VPGEN_VFRTRIG_VFSWR_MASK;
		I40E_WRITE_REG(hw, I40E_VPGEN_VFRTRIG(vf_id), val);
		I40E_WRITE_FLUSH(hw);
	}

#define VFRESET_MAX_WAIT_CNT 100
	/* Wait until VF reset is done */
	for (i = 0; i < VFRESET_MAX_WAIT_CNT; i++) {
		rte_delay_us(10);
		val = I40E_READ_REG(hw, I40E_VPGEN_VFRSTAT(vf_id));
		if (val & I40E_VPGEN_VFRSTAT_VFRD_MASK)
			break;
	}

	if (i >= VFRESET_MAX_WAIT_CNT) {
		PMD_DRV_LOG(ERR, "VF reset timeout");
		return -ETIMEDOUT;
	}
	/* This is not first time to do reset, do cleanup job first */
	if (vf->vsi) {
		/* Disable queues */
		memset(&qsel, 0, sizeof(qsel));
		for (i = 0; i < vf->vsi->nb_qps; i++)
			qsel.rx_queues |= 1 << i;
		qsel.tx_queues = qsel.rx_queues;
		ret = i40e_pf_host_switch_queues(vf, &qsel, false);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Disable VF queues failed");
			return -EFAULT;
		}

		/* Disable VF interrupt setting */
		vf_msix_num = hw->func_caps.num_msix_vectors_vf;
		for (i = 0; i < vf_msix_num; i++) {
			if (!i)
				val = I40E_VFINT_DYN_CTL0(vf_id);
			else
				val = I40E_VFINT_DYN_CTLN(((vf_msix_num - 1) *
							(vf_id)) + (i - 1));
			I40E_WRITE_REG(hw, val, I40E_VFINT_DYN_CTLN_CLEARPBA_MASK);
		}
		I40E_WRITE_FLUSH(hw);

		/* remove VSI */
		ret = i40e_vsi_release(vf->vsi);
		if (ret != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Release VSI failed");
			return -EFAULT;
		}
	}

#define I40E_VF_PCI_ADDR  0xAA
#define I40E_VF_PEND_MASK 0x20
	/* Check the pending transactions of this VF */
	/* Use absolute VF id, refer to datasheet for details */
	I40E_WRITE_REG(hw, I40E_PF_PCI_CIAA, I40E_VF_PCI_ADDR |
		(abs_vf_id << I40E_PF_PCI_CIAA_VF_NUM_SHIFT));
	for (i = 0; i < VFRESET_MAX_WAIT_CNT; i++) {
		rte_delay_us(1);
		val = I40E_READ_REG(hw, I40E_PF_PCI_CIAD);
		if ((val & I40E_VF_PEND_MASK) == 0)
			break;
	}

	if (i >= VFRESET_MAX_WAIT_CNT) {
		PMD_DRV_LOG(ERR, "Wait VF PCI transaction end timeout");
		return -ETIMEDOUT;
	}

	/* Reset done, Set COMPLETE flag and clear reset bit */
	I40E_WRITE_REG(hw, I40E_VFGEN_RSTAT1(vf_id), VIRTCHNL_VFR_COMPLETED);
	val = I40E_READ_REG(hw, I40E_VPGEN_VFRTRIG(vf_id));
	val &= ~I40E_VPGEN_VFRTRIG_VFSWR_MASK;
	I40E_WRITE_REG(hw, I40E_VPGEN_VFRTRIG(vf_id), val);
	vf->reset_cnt++;
	I40E_WRITE_FLUSH(hw);

	/* Allocate resource again */
	if (pf->floating_veb && pf->floating_veb_list[vf_id]) {
		vf->vsi = i40e_vsi_setup(vf->pf, I40E_VSI_SRIOV,
					 NULL, vf->vf_idx);
	} else {
		vf->vsi = i40e_vsi_setup(vf->pf, I40E_VSI_SRIOV,
					 vf->pf->main_vsi, vf->vf_idx);
	}

	if (vf->vsi == NULL) {
		PMD_DRV_LOG(ERR, "Add vsi failed");
		return -EFAULT;
	}

	ret = i40e_pf_vf_queues_mapping(vf);
	if (ret != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "queue mapping error");
		i40e_vsi_release(vf->vsi);
		return -EFAULT;
	}

	I40E_WRITE_REG(hw, I40E_VFGEN_RSTAT1(vf_id), VIRTCHNL_VFR_VFACTIVE);

	return ret;
}

int
i40e_pf_host_send_msg_to_vf(struct i40e_pf_vf *vf,
			    uint32_t opcode,
			    uint32_t retval,
			    uint8_t *msg,
			    uint16_t msglen)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	uint16_t abs_vf_id = hw->func_caps.vf_base_id + vf->vf_idx;
	int ret;

	ret = i40e_aq_send_msg_to_vf(hw, abs_vf_id, opcode, retval,
						msg, msglen, NULL);
	if (ret) {
		PMD_INIT_LOG(ERR, "Fail to send message to VF, err %u",
			     hw->aq.asq_last_status);
	}

	return ret;
}

static void
i40e_pf_host_process_cmd_version(struct i40e_pf_vf *vf, bool b_op)
{
	struct virtchnl_version_info info;

	/* Respond like a Linux PF host in order to support both DPDK VF and
	 * Linux VF driver. The expense is original DPDK host specific feature
	 * like CFG_VLAN_PVID and CONFIG_VSI_QUEUES_EXT will not available.
	 *
	 * DPDK VF also can't identify host driver by version number returned.
	 * It always assume talking with Linux PF.
	 */
	info.major = VIRTCHNL_VERSION_MAJOR;
	info.minor = VIRTCHNL_VERSION_MINOR_NO_VF_CAPS;

	if (b_op)
		i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_VERSION,
					    I40E_SUCCESS,
					    (uint8_t *)&info,
					    sizeof(info));
	else
		i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_VERSION,
					    I40E_NOT_SUPPORTED,
					    (uint8_t *)&info,
					    sizeof(info));
}

static int
i40e_pf_host_process_cmd_reset_vf(struct i40e_pf_vf *vf)
{
	i40e_pf_host_vf_reset(vf, 1);

	/* No feedback will be sent to VF for VFLR */
	return I40E_SUCCESS;
}

static int
i40e_pf_host_process_cmd_get_vf_resource(struct i40e_pf_vf *vf, bool b_op)
{
	struct virtchnl_vf_resource *vf_res = NULL;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	uint32_t len = 0;
	int ret = I40E_SUCCESS;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(vf,
					    VIRTCHNL_OP_GET_VF_RESOURCES,
					    I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	/* only have 1 VSI by default */
	len =  sizeof(struct virtchnl_vf_resource) +
				I40E_DEFAULT_VF_VSI_NUM *
		sizeof(struct virtchnl_vsi_resource);

	vf_res = rte_zmalloc("i40e_vf_res", len, 0);
	if (vf_res == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate mem");
		ret = I40E_ERR_NO_MEMORY;
		vf_res = NULL;
		len = 0;
		goto send_msg;
	}

	vf_res->vf_offload_flags = VIRTCHNL_VF_OFFLOAD_L2 |
				VIRTCHNL_VF_OFFLOAD_VLAN;
	vf_res->max_vectors = hw->func_caps.num_msix_vectors_vf;
	vf_res->num_queue_pairs = vf->vsi->nb_qps;
	vf_res->num_vsis = I40E_DEFAULT_VF_VSI_NUM;

	/* Change below setting if PF host can support more VSIs for VF */
	vf_res->vsi_res[0].vsi_type = VIRTCHNL_VSI_SRIOV;
	vf_res->vsi_res[0].vsi_id = vf->vsi->vsi_id;
	vf_res->vsi_res[0].num_queue_pairs = vf->vsi->nb_qps;
	ether_addr_copy(&vf->mac_addr,
		(struct ether_addr *)vf_res->vsi_res[0].default_mac_addr);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_GET_VF_RESOURCES,
					ret, (uint8_t *)vf_res, len);
	rte_free(vf_res);

	return ret;
}

static int
i40e_pf_host_hmc_config_rxq(struct i40e_hw *hw,
			    struct i40e_pf_vf *vf,
			    struct virtchnl_rxq_info *rxq,
			    uint8_t crcstrip)
{
	int err = I40E_SUCCESS;
	struct i40e_hmc_obj_rxq rx_ctx;
	uint16_t abs_queue_id = vf->vsi->base_queue + rxq->queue_id;

	/* Clear the context structure first */
	memset(&rx_ctx, 0, sizeof(struct i40e_hmc_obj_rxq));
	rx_ctx.dbuff = rxq->databuffer_size >> I40E_RXQ_CTX_DBUFF_SHIFT;
	rx_ctx.hbuff = rxq->hdr_size >> I40E_RXQ_CTX_HBUFF_SHIFT;
	rx_ctx.base = rxq->dma_ring_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	rx_ctx.qlen = rxq->ring_len;
#ifndef RTE_LIBRTE_I40E_16BYTE_RX_DESC
	rx_ctx.dsize = 1;
#endif

	if (rxq->splithdr_enabled) {
		rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_ALL;
		rx_ctx.dtype = i40e_header_split_enabled;
	} else {
		rx_ctx.hsplit_0 = I40E_HEADER_SPLIT_NONE;
		rx_ctx.dtype = i40e_header_split_none;
	}
	rx_ctx.rxmax = rxq->max_pkt_size;
	rx_ctx.tphrdesc_ena = 1;
	rx_ctx.tphwdesc_ena = 1;
	rx_ctx.tphdata_ena = 1;
	rx_ctx.tphhead_ena = 1;
	rx_ctx.lrxqthresh = 2;
	rx_ctx.crcstrip = crcstrip;
	rx_ctx.l2tsel = 1;
	rx_ctx.prefena = 1;

	err = i40e_clear_lan_rx_queue_context(hw, abs_queue_id);
	if (err != I40E_SUCCESS)
		return err;
	err = i40e_set_lan_rx_queue_context(hw, abs_queue_id, &rx_ctx);

	return err;
}

static inline uint8_t
i40e_vsi_get_tc_of_queue(struct i40e_vsi *vsi,
		uint16_t queue_id)
{
	struct i40e_aqc_vsi_properties_data *info = &vsi->info;
	uint16_t bsf, qp_idx;
	uint8_t i;

	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (vsi->enabled_tc & (1 << i)) {
			qp_idx = rte_le_to_cpu_16((info->tc_mapping[i] &
				I40E_AQ_VSI_TC_QUE_OFFSET_MASK) >>
				I40E_AQ_VSI_TC_QUE_OFFSET_SHIFT);
			bsf = rte_le_to_cpu_16((info->tc_mapping[i] &
				I40E_AQ_VSI_TC_QUE_NUMBER_MASK) >>
				I40E_AQ_VSI_TC_QUE_NUMBER_SHIFT);
			if (queue_id >= qp_idx && queue_id < qp_idx + (1 << bsf))
				return i;
		}
	}
	return 0;
}

static int
i40e_pf_host_hmc_config_txq(struct i40e_hw *hw,
			    struct i40e_pf_vf *vf,
			    struct virtchnl_txq_info *txq)
{
	int err = I40E_SUCCESS;
	struct i40e_hmc_obj_txq tx_ctx;
	struct i40e_vsi *vsi = vf->vsi;
	uint32_t qtx_ctl;
	uint16_t abs_queue_id = vsi->base_queue + txq->queue_id;
	uint8_t dcb_tc;

	/* clear the context structure first */
	memset(&tx_ctx, 0, sizeof(tx_ctx));
	tx_ctx.base = txq->dma_ring_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	tx_ctx.qlen = txq->ring_len;
	dcb_tc = i40e_vsi_get_tc_of_queue(vsi, txq->queue_id);
	tx_ctx.rdylist = rte_le_to_cpu_16(vsi->info.qs_handle[dcb_tc]);
	tx_ctx.head_wb_ena = txq->headwb_enabled;
	tx_ctx.head_wb_addr = txq->dma_headwb_addr;

	err = i40e_clear_lan_tx_queue_context(hw, abs_queue_id);
	if (err != I40E_SUCCESS)
		return err;

	err = i40e_set_lan_tx_queue_context(hw, abs_queue_id, &tx_ctx);
	if (err != I40E_SUCCESS)
		return err;

	/* bind queue with VF function, since TX/QX will appear in pair,
	 * so only has QTX_CTL to set.
	 */
	qtx_ctl = (I40E_QTX_CTL_VF_QUEUE << I40E_QTX_CTL_PFVF_Q_SHIFT) |
				((hw->pf_id << I40E_QTX_CTL_PF_INDX_SHIFT) &
				I40E_QTX_CTL_PF_INDX_MASK) |
				(((vf->vf_idx + hw->func_caps.vf_base_id) <<
				I40E_QTX_CTL_VFVM_INDX_SHIFT) &
				I40E_QTX_CTL_VFVM_INDX_MASK);
	I40E_WRITE_REG(hw, I40E_QTX_CTL(abs_queue_id), qtx_ctl);
	I40E_WRITE_FLUSH(hw);

	return I40E_SUCCESS;
}

static int
i40e_pf_host_process_cmd_config_vsi_queues(struct i40e_pf_vf *vf,
					   uint8_t *msg,
					   uint16_t msglen,
					   bool b_op)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	struct i40e_vsi *vsi = vf->vsi;
	struct virtchnl_vsi_queue_config_info *vc_vqci =
		(struct virtchnl_vsi_queue_config_info *)msg;
	struct virtchnl_queue_pair_info *vc_qpi;
	int i, ret = I40E_SUCCESS;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(vf,
					    VIRTCHNL_OP_CONFIG_VSI_QUEUES,
					    I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (!msg || vc_vqci->num_queue_pairs > vsi->nb_qps ||
		vc_vqci->num_queue_pairs > I40E_MAX_VSI_QP ||
		msglen < I40E_VIRTCHNL_CONFIG_VSI_QUEUES_SIZE(vc_vqci,
					vc_vqci->num_queue_pairs)) {
		PMD_DRV_LOG(ERR, "vsi_queue_config_info argument wrong");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	vc_qpi = vc_vqci->qpair;
	for (i = 0; i < vc_vqci->num_queue_pairs; i++) {
		if (vc_qpi[i].rxq.queue_id > vsi->nb_qps - 1 ||
			vc_qpi[i].txq.queue_id > vsi->nb_qps - 1) {
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}

		/*
		 * Apply VF RX queue setting to HMC.
		 * If the opcode is VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
		 * then the extra information of
		 * 'struct virtchnl_queue_pair_extra_info' is needed,
		 * otherwise set the last parameter to NULL.
		 */
		if (i40e_pf_host_hmc_config_rxq(hw, vf, &vc_qpi[i].rxq,
			I40E_CFG_CRCSTRIP_DEFAULT) != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Configure RX queue HMC failed");
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}

		/* Apply VF TX queue setting to HMC */
		if (i40e_pf_host_hmc_config_txq(hw, vf,
			&vc_qpi[i].txq) != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Configure TX queue HMC failed");
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_CONFIG_VSI_QUEUES,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_config_vsi_queues_ext(struct i40e_pf_vf *vf,
					       uint8_t *msg,
					       uint16_t msglen,
					       bool b_op)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	struct i40e_vsi *vsi = vf->vsi;
	struct virtchnl_vsi_queue_config_ext_info *vc_vqcei =
		(struct virtchnl_vsi_queue_config_ext_info *)msg;
	struct virtchnl_queue_pair_ext_info *vc_qpei;
	int i, ret = I40E_SUCCESS;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (!msg || vc_vqcei->num_queue_pairs > vsi->nb_qps ||
		vc_vqcei->num_queue_pairs > I40E_MAX_VSI_QP ||
		msglen < I40E_VIRTCHNL_CONFIG_VSI_QUEUES_SIZE(vc_vqcei,
					vc_vqcei->num_queue_pairs)) {
		PMD_DRV_LOG(ERR, "vsi_queue_config_ext_info argument wrong");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	vc_qpei = vc_vqcei->qpair;
	for (i = 0; i < vc_vqcei->num_queue_pairs; i++) {
		if (vc_qpei[i].rxq.queue_id > vsi->nb_qps - 1 ||
			vc_qpei[i].txq.queue_id > vsi->nb_qps - 1) {
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}
		/*
		 * Apply VF RX queue setting to HMC.
		 * If the opcode is VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
		 * then the extra information of
		 * 'struct virtchnl_queue_pair_ext_info' is needed,
		 * otherwise set the last parameter to NULL.
		 */
		if (i40e_pf_host_hmc_config_rxq(hw, vf, &vc_qpei[i].rxq,
			vc_qpei[i].rxq_ext.crcstrip) != I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Configure RX queue HMC failed");
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}

		/* Apply VF TX queue setting to HMC */
		if (i40e_pf_host_hmc_config_txq(hw, vf, &vc_qpei[i].txq) !=
							I40E_SUCCESS) {
			PMD_DRV_LOG(ERR, "Configure TX queue HMC failed");
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
								ret, NULL, 0);

	return ret;
}

static void
i40e_pf_config_irq_link_list(struct i40e_pf_vf *vf,
			      struct virtchnl_vector_map *vvm)
{
#define BITS_PER_CHAR 8
	uint64_t linklistmap = 0, tempmap;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	uint16_t qid;
	bool b_first_q = true;
	enum i40e_queue_type qtype;
	uint16_t vector_id;
	uint32_t reg, reg_idx;
	uint16_t itr_idx = 0, i;

	vector_id = vvm->vector_id;
	/* setup the head */
	if (!vector_id)
		reg_idx = I40E_VPINT_LNKLST0(vf->vf_idx);
	else
		reg_idx = I40E_VPINT_LNKLSTN(
		((hw->func_caps.num_msix_vectors_vf - 1) * vf->vf_idx)
		+ (vector_id - 1));

	if (vvm->rxq_map == 0 && vvm->txq_map == 0) {
		I40E_WRITE_REG(hw, reg_idx,
			I40E_VPINT_LNKLST0_FIRSTQ_INDX_MASK);
		goto cfg_irq_done;
	}

	/* sort all rx and tx queues */
	tempmap = vvm->rxq_map;
	for (i = 0; i < sizeof(vvm->rxq_map) * BITS_PER_CHAR; i++) {
		if (tempmap & 0x1)
			linklistmap |= (1 << (2 * i));
		tempmap >>= 1;
	}

	tempmap = vvm->txq_map;
	for (i = 0; i < sizeof(vvm->txq_map) * BITS_PER_CHAR; i++) {
		if (tempmap & 0x1)
			linklistmap |= (1 << (2 * i + 1));
		tempmap >>= 1;
	}

	/* Link all rx and tx queues into a chained list */
	tempmap = linklistmap;
	i = 0;
	b_first_q = true;
	do {
		if (tempmap & 0x1) {
			qtype = (enum i40e_queue_type)(i % 2);
			qid = vf->vsi->base_queue + i / 2;
			if (b_first_q) {
				/* This is header */
				b_first_q = false;
				reg = ((qtype <<
				I40E_VPINT_LNKLSTN_FIRSTQ_TYPE_SHIFT)
				| qid);
			} else {
				/* element in the link list */
				reg = (vector_id) |
				(qtype << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT) |
				(qid << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
				BIT(I40E_QINT_RQCTL_CAUSE_ENA_SHIFT) |
				(itr_idx << I40E_QINT_RQCTL_ITR_INDX_SHIFT);
			}
			I40E_WRITE_REG(hw, reg_idx, reg);
			/* find next register to program */
			switch (qtype) {
			case I40E_QUEUE_TYPE_RX:
				reg_idx = I40E_QINT_RQCTL(qid);
				itr_idx = vvm->rxitr_idx;
				break;
			case I40E_QUEUE_TYPE_TX:
				reg_idx = I40E_QINT_TQCTL(qid);
				itr_idx = vvm->txitr_idx;
				break;
			default:
				break;
			}
		}
		i++;
		tempmap >>= 1;
	} while (tempmap);

	/* Terminate the link list */
	reg = (vector_id) |
		(0 << I40E_QINT_RQCTL_NEXTQ_TYPE_SHIFT) |
		(0x7FF << I40E_QINT_RQCTL_NEXTQ_INDX_SHIFT) |
		BIT(I40E_QINT_RQCTL_CAUSE_ENA_SHIFT) |
		(itr_idx << I40E_QINT_RQCTL_ITR_INDX_SHIFT);
	I40E_WRITE_REG(hw, reg_idx, reg);

cfg_irq_done:
	I40E_WRITE_FLUSH(hw);
}

static int
i40e_pf_host_process_cmd_config_irq_map(struct i40e_pf_vf *vf,
					uint8_t *msg, uint16_t msglen,
					bool b_op)
{
	int ret = I40E_SUCCESS;
	struct i40e_pf *pf = vf->pf;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	struct virtchnl_irq_map_info *irqmap =
	    (struct virtchnl_irq_map_info *)msg;
	struct virtchnl_vector_map *map;
	int i;
	uint16_t vector_id;
	unsigned long qbit_max;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_CONFIG_IRQ_MAP,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (msg == NULL || msglen < sizeof(struct virtchnl_irq_map_info)) {
		PMD_DRV_LOG(ERR, "buffer too short");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	/* PF host will support both DPDK VF or Linux VF driver, identify by
	 * number of vectors requested.
	 */

	/* DPDK VF only requires single vector */
	if (irqmap->num_vectors == 1) {
		/* This MSIX intr store the intr in VF range */
		vf->vsi->msix_intr = irqmap->vecmap[0].vector_id;
		vf->vsi->nb_msix = irqmap->num_vectors;
		vf->vsi->nb_used_qps = vf->vsi->nb_qps;

		/* Don't care how the TX/RX queue mapping with this vector.
		 * Link all VF RX queues together. Only did mapping work.
		 * VF can disable/enable the intr by itself.
		 */
		i40e_vsi_queues_bind_intr(vf->vsi);
		goto send_msg;
	}

	/* Then, it's Linux VF driver */
	qbit_max = 1 << pf->vf_nb_qp_max;
	for (i = 0; i < irqmap->num_vectors; i++) {
		map = &irqmap->vecmap[i];

		vector_id = map->vector_id;
		/* validate msg params */
		if (vector_id >= hw->func_caps.num_msix_vectors_vf) {
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}

		if ((map->rxq_map < qbit_max) && (map->txq_map < qbit_max)) {
			i40e_pf_config_irq_link_list(vf, map);
		} else {
			/* configured queue size excceed limit */
			ret = I40E_ERR_PARAM;
			goto send_msg;
		}
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_CONFIG_IRQ_MAP,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_switch_queues(struct i40e_pf_vf *vf,
			   struct virtchnl_queue_select *qsel,
			   bool on)
{
	int ret = I40E_SUCCESS;
	int i;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	uint16_t baseq = vf->vsi->base_queue;

	if (qsel->rx_queues + qsel->tx_queues == 0)
		return I40E_ERR_PARAM;

	/* always enable RX first and disable last */
	/* Enable RX if it's enable */
	if (on) {
		for (i = 0; i < I40E_MAX_QP_NUM_PER_VF; i++)
			if (qsel->rx_queues & (1 << i)) {
				ret = i40e_switch_rx_queue(hw, baseq + i, on);
				if (ret != I40E_SUCCESS)
					return ret;
			}
	}

	/* Enable/Disable TX */
	for (i = 0; i < I40E_MAX_QP_NUM_PER_VF; i++)
		if (qsel->tx_queues & (1 << i)) {
			ret = i40e_switch_tx_queue(hw, baseq + i, on);
			if (ret != I40E_SUCCESS)
				return ret;
		}

	/* disable RX last if it's disable */
	if (!on) {
		/* disable RX */
		for (i = 0; i < I40E_MAX_QP_NUM_PER_VF; i++)
			if (qsel->rx_queues & (1 << i)) {
				ret = i40e_switch_rx_queue(hw, baseq + i, on);
				if (ret != I40E_SUCCESS)
					return ret;
			}
	}

	return ret;
}

static int
i40e_pf_host_process_cmd_enable_queues(struct i40e_pf_vf *vf,
				       uint8_t *msg,
				       uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_queue_select *q_sel =
		(struct virtchnl_queue_select *)msg;

	if (msg == NULL || msglen != sizeof(*q_sel)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}
	ret = i40e_pf_host_switch_queues(vf, q_sel, true);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_ENABLE_QUEUES,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_disable_queues(struct i40e_pf_vf *vf,
					uint8_t *msg,
					uint16_t msglen,
					bool b_op)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_queue_select *q_sel =
		(struct virtchnl_queue_select *)msg;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_DISABLE_QUEUES,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (msg == NULL || msglen != sizeof(*q_sel)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}
	ret = i40e_pf_host_switch_queues(vf, q_sel, false);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_DISABLE_QUEUES,
							ret, NULL, 0);

	return ret;
}


static int
i40e_pf_host_process_cmd_add_ether_address(struct i40e_pf_vf *vf,
					   uint8_t *msg,
					   uint16_t msglen,
					   bool b_op)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_ether_addr_list *addr_list =
			(struct virtchnl_ether_addr_list *)msg;
	struct i40e_mac_filter_info filter;
	int i;
	struct ether_addr *mac;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_ADD_ETH_ADDR,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	memset(&filter, 0 , sizeof(struct i40e_mac_filter_info));

	if (msg == NULL || msglen <= sizeof(*addr_list)) {
		PMD_DRV_LOG(ERR, "add_ether_address argument too short");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	for (i = 0; i < addr_list->num_elements; i++) {
		mac = (struct ether_addr *)(addr_list->list[i].addr);
		(void)rte_memcpy(&filter.mac_addr, mac, ETHER_ADDR_LEN);
		filter.filter_type = RTE_MACVLAN_PERFECT_MATCH;
		if (is_zero_ether_addr(mac) ||
		    i40e_vsi_add_mac(vf->vsi, &filter)) {
			ret = I40E_ERR_INVALID_MAC_ADDR;
			goto send_msg;
		}
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_ADD_ETH_ADDR,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_del_ether_address(struct i40e_pf_vf *vf,
					   uint8_t *msg,
					   uint16_t msglen,
					   bool b_op)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_ether_addr_list *addr_list =
		(struct virtchnl_ether_addr_list *)msg;
	int i;
	struct ether_addr *mac;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_DEL_ETH_ADDR,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (msg == NULL || msglen <= sizeof(*addr_list)) {
		PMD_DRV_LOG(ERR, "delete_ether_address argument too short");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	for (i = 0; i < addr_list->num_elements; i++) {
		mac = (struct ether_addr *)(addr_list->list[i].addr);
		if(is_zero_ether_addr(mac) ||
			i40e_vsi_delete_mac(vf->vsi, mac)) {
			ret = I40E_ERR_INVALID_MAC_ADDR;
			goto send_msg;
		}
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_DEL_ETH_ADDR,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_add_vlan(struct i40e_pf_vf *vf,
				uint8_t *msg, uint16_t msglen,
				bool b_op)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_vlan_filter_list *vlan_filter_list =
		(struct virtchnl_vlan_filter_list *)msg;
	int i;
	uint16_t *vid;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_ADD_VLAN,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (msg == NULL || msglen <= sizeof(*vlan_filter_list)) {
		PMD_DRV_LOG(ERR, "add_vlan argument too short");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	vid = vlan_filter_list->vlan_id;

	for (i = 0; i < vlan_filter_list->num_elements; i++) {
		ret = i40e_vsi_add_vlan(vf->vsi, vid[i]);
		if(ret != I40E_SUCCESS)
			goto send_msg;
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_ADD_VLAN,
						ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_del_vlan(struct i40e_pf_vf *vf,
				  uint8_t *msg,
				  uint16_t msglen,
				  bool b_op)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_vlan_filter_list *vlan_filter_list =
			(struct virtchnl_vlan_filter_list *)msg;
	int i;
	uint16_t *vid;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_DEL_VLAN,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (msg == NULL || msglen <= sizeof(*vlan_filter_list)) {
		PMD_DRV_LOG(ERR, "delete_vlan argument too short");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	vid = vlan_filter_list->vlan_id;
	for (i = 0; i < vlan_filter_list->num_elements; i++) {
		ret = i40e_vsi_delete_vlan(vf->vsi, vid[i]);
		if(ret != I40E_SUCCESS)
			goto send_msg;
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_DEL_VLAN,
						ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_config_promisc_mode(
					struct i40e_pf_vf *vf,
					uint8_t *msg,
					uint16_t msglen,
					bool b_op)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_promisc_info *promisc =
				(struct virtchnl_promisc_info *)msg;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	bool unicast = FALSE, multicast = FALSE;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (msg == NULL || msglen != sizeof(*promisc)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	if (promisc->flags & FLAG_VF_UNICAST_PROMISC)
		unicast = TRUE;
	ret = i40e_aq_set_vsi_unicast_promiscuous(hw,
			vf->vsi->seid, unicast, NULL, true);
	if (ret != I40E_SUCCESS)
		goto send_msg;

	if (promisc->flags & FLAG_VF_MULTICAST_PROMISC)
		multicast = TRUE;
	ret = i40e_aq_set_vsi_multicast_promiscuous(hw, vf->vsi->seid,
						multicast, NULL);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf,
		VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE, ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_get_stats(struct i40e_pf_vf *vf, bool b_op)
{
	i40e_update_vsi_stats(vf->vsi);

	if (b_op)
		i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_GET_STATS,
					    I40E_SUCCESS,
					    (uint8_t *)&vf->vsi->eth_stats,
					    sizeof(vf->vsi->eth_stats));
	else
		i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_GET_STATS,
					    I40E_NOT_SUPPORTED,
					    (uint8_t *)&vf->vsi->eth_stats,
					    sizeof(vf->vsi->eth_stats));

	return I40E_SUCCESS;
}

static int
i40e_pf_host_process_cmd_enable_vlan_strip(struct i40e_pf_vf *vf, bool b_op)
{
	int ret = I40E_SUCCESS;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_ENABLE_VLAN_STRIPPING,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	ret = i40e_vsi_config_vlan_stripping(vf->vsi, TRUE);
	if (ret != 0)
		PMD_DRV_LOG(ERR, "Failed to enable vlan stripping");

	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_ENABLE_VLAN_STRIPPING,
				    ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_disable_vlan_strip(struct i40e_pf_vf *vf, bool b_op)
{
	int ret = I40E_SUCCESS;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			VIRTCHNL_OP_DISABLE_VLAN_STRIPPING,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	ret = i40e_vsi_config_vlan_stripping(vf->vsi, FALSE);
	if (ret != 0)
		PMD_DRV_LOG(ERR, "Failed to disable vlan stripping");

	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_DISABLE_VLAN_STRIPPING,
				    ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_cfg_pvid(struct i40e_pf_vf *vf,
					uint8_t *msg,
					uint16_t msglen,
					bool b_op)
{
	int ret = I40E_SUCCESS;
	struct virtchnl_pvid_info  *tpid_info =
			(struct virtchnl_pvid_info *)msg;

	if (!b_op) {
		i40e_pf_host_send_msg_to_vf(
			vf,
			I40E_VIRTCHNL_OP_CFG_VLAN_PVID,
			I40E_NOT_SUPPORTED, NULL, 0);
		return ret;
	}

	if (msg == NULL || msglen != sizeof(*tpid_info)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	ret = i40e_vsi_vlan_pvid_set(vf->vsi, &tpid_info->info);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_CFG_VLAN_PVID,
					ret, NULL, 0);

	return ret;
}

void
i40e_notify_vf_link_status(struct rte_eth_dev *dev, struct i40e_pf_vf *vf)
{
	struct virtchnl_pf_event event;

	event.event = VIRTCHNL_EVENT_LINK_CHANGE;
	event.event_data.link_event.link_status =
		dev->data->dev_link.link_status;

	/* need to convert the ETH_SPEED_xxx into VIRTCHNL_LINK_SPEED_xxx */
	switch (dev->data->dev_link.link_speed) {
	case ETH_SPEED_NUM_100M:
		event.event_data.link_event.link_speed = VIRTCHNL_LINK_SPEED_100MB;
		break;
	case ETH_SPEED_NUM_1G:
		event.event_data.link_event.link_speed = VIRTCHNL_LINK_SPEED_1GB;
		break;
	case ETH_SPEED_NUM_10G:
		event.event_data.link_event.link_speed = VIRTCHNL_LINK_SPEED_10GB;
		break;
	case ETH_SPEED_NUM_20G:
		event.event_data.link_event.link_speed = VIRTCHNL_LINK_SPEED_20GB;
		break;
	case ETH_SPEED_NUM_25G:
		event.event_data.link_event.link_speed = VIRTCHNL_LINK_SPEED_25GB;
		break;
	case ETH_SPEED_NUM_40G:
		event.event_data.link_event.link_speed = VIRTCHNL_LINK_SPEED_40GB;
		break;
	default:
		event.event_data.link_event.link_speed =
			VIRTCHNL_LINK_SPEED_UNKNOWN;
		break;
	}

	i40e_pf_host_send_msg_to_vf(vf, VIRTCHNL_OP_EVENT,
		I40E_SUCCESS, (uint8_t *)&event, sizeof(event));
}

void
i40e_pf_host_handle_vf_msg(struct rte_eth_dev *dev,
			   uint16_t abs_vf_id, uint32_t opcode,
			   __rte_unused uint32_t retval,
			   uint8_t *msg,
			   uint16_t msglen)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_pf_vf *vf;
	/* AdminQ will pass absolute VF id, transfer to internal vf id */
	uint16_t vf_id = abs_vf_id - hw->func_caps.vf_base_id;
	struct rte_pmd_i40e_mb_event_param ret_param;
	bool b_op = TRUE;

	if (vf_id > pf->vf_num - 1 || !pf->vfs) {
		PMD_DRV_LOG(ERR, "invalid argument");
		return;
	}

	vf = &pf->vfs[vf_id];
	if (!vf->vsi) {
		PMD_DRV_LOG(ERR, "NO VSI associated with VF found");
		i40e_pf_host_send_msg_to_vf(vf, opcode,
			I40E_ERR_NO_AVAILABLE_VSI, NULL, 0);
		return;
	}

	/**
	 * initialise structure to send to user application
	 * will return response from user in retval field
	 */
	ret_param.retval = RTE_PMD_I40E_MB_EVENT_PROCEED;
	ret_param.vfid = vf_id;
	ret_param.msg_type = opcode;
	ret_param.msg = (void *)msg;
	ret_param.msglen = msglen;

	/**
	 * Ask user application if we're allowed to perform those functions.
	 * If we get ret_param.retval == RTE_PMD_I40E_MB_EVENT_PROCEED,
	 * then business as usual.
	 * If RTE_PMD_I40E_MB_EVENT_NOOP_ACK or RTE_PMD_I40E_MB_EVENT_NOOP_NACK,
	 * do nothing and send not_supported to VF. As PF must send a response
	 * to VF and ACK/NACK is not defined.
	 */
	_rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_VF_MBOX,
				      NULL, &ret_param);
	if (ret_param.retval != RTE_PMD_I40E_MB_EVENT_PROCEED) {
		PMD_DRV_LOG(WARNING, "VF to PF message(%d) is not permitted!",
			    opcode);
		b_op = FALSE;
	}

	switch (opcode) {
	case VIRTCHNL_OP_VERSION:
		PMD_DRV_LOG(INFO, "OP_VERSION received");
		i40e_pf_host_process_cmd_version(vf, b_op);
		break;
	case VIRTCHNL_OP_RESET_VF:
		PMD_DRV_LOG(INFO, "OP_RESET_VF received");
		i40e_pf_host_process_cmd_reset_vf(vf);
		break;
	case VIRTCHNL_OP_GET_VF_RESOURCES:
		PMD_DRV_LOG(INFO, "OP_GET_VF_RESOURCES received");
		i40e_pf_host_process_cmd_get_vf_resource(vf, b_op);
		break;
	case VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		PMD_DRV_LOG(INFO, "OP_CONFIG_VSI_QUEUES received");
		i40e_pf_host_process_cmd_config_vsi_queues(vf, msg,
							   msglen, b_op);
		break;
	case VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT:
		PMD_DRV_LOG(INFO, "OP_CONFIG_VSI_QUEUES_EXT received");
		i40e_pf_host_process_cmd_config_vsi_queues_ext(vf, msg,
							       msglen, b_op);
		break;
	case VIRTCHNL_OP_CONFIG_IRQ_MAP:
		PMD_DRV_LOG(INFO, "OP_CONFIG_IRQ_MAP received");
		i40e_pf_host_process_cmd_config_irq_map(vf, msg, msglen, b_op);
		break;
	case VIRTCHNL_OP_ENABLE_QUEUES:
		PMD_DRV_LOG(INFO, "OP_ENABLE_QUEUES received");
		if (b_op) {
			i40e_pf_host_process_cmd_enable_queues(vf, msg, msglen);
			i40e_notify_vf_link_status(dev, vf);
		} else {
			i40e_pf_host_send_msg_to_vf(
				vf, VIRTCHNL_OP_ENABLE_QUEUES,
				I40E_NOT_SUPPORTED, NULL, 0);
		}
		break;
	case VIRTCHNL_OP_DISABLE_QUEUES:
		PMD_DRV_LOG(INFO, "OP_DISABLE_QUEUE received");
		i40e_pf_host_process_cmd_disable_queues(vf, msg, msglen, b_op);
		break;
	case VIRTCHNL_OP_ADD_ETH_ADDR:
		PMD_DRV_LOG(INFO, "OP_ADD_ETHER_ADDRESS received");
		i40e_pf_host_process_cmd_add_ether_address(vf, msg,
							   msglen, b_op);
		break;
	case VIRTCHNL_OP_DEL_ETH_ADDR:
		PMD_DRV_LOG(INFO, "OP_DEL_ETHER_ADDRESS received");
		i40e_pf_host_process_cmd_del_ether_address(vf, msg,
							   msglen, b_op);
		break;
	case VIRTCHNL_OP_ADD_VLAN:
		PMD_DRV_LOG(INFO, "OP_ADD_VLAN received");
		i40e_pf_host_process_cmd_add_vlan(vf, msg, msglen, b_op);
		break;
	case VIRTCHNL_OP_DEL_VLAN:
		PMD_DRV_LOG(INFO, "OP_DEL_VLAN received");
		i40e_pf_host_process_cmd_del_vlan(vf, msg, msglen, b_op);
		break;
	case VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		PMD_DRV_LOG(INFO, "OP_CONFIG_PROMISCUOUS_MODE received");
		i40e_pf_host_process_cmd_config_promisc_mode(vf, msg,
							     msglen, b_op);
		break;
	case VIRTCHNL_OP_GET_STATS:
		PMD_DRV_LOG(INFO, "OP_GET_STATS received");
		i40e_pf_host_process_cmd_get_stats(vf, b_op);
		break;
	case VIRTCHNL_OP_ENABLE_VLAN_STRIPPING:
		PMD_DRV_LOG(INFO, "OP_ENABLE_VLAN_STRIPPING received");
		i40e_pf_host_process_cmd_enable_vlan_strip(vf, b_op);
		break;
	case VIRTCHNL_OP_DISABLE_VLAN_STRIPPING:
		PMD_DRV_LOG(INFO, "OP_DISABLE_VLAN_STRIPPING received");
		i40e_pf_host_process_cmd_disable_vlan_strip(vf, b_op);
		break;
	case I40E_VIRTCHNL_OP_CFG_VLAN_PVID:
		PMD_DRV_LOG(INFO, "OP_CFG_VLAN_PVID received");
		i40e_pf_host_process_cmd_cfg_pvid(vf, msg, msglen, b_op);
		break;
	/* Don't add command supported below, which will
	 * return an error code.
	 */
	default:
		PMD_DRV_LOG(ERR, "%u received, not supported", opcode);
		i40e_pf_host_send_msg_to_vf(vf, opcode, I40E_ERR_PARAM,
								NULL, 0);
		break;
	}
}

int
i40e_pf_host_init(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	int ret, i;
	uint32_t val;

	PMD_INIT_FUNC_TRACE();

	/**
	 * return if SRIOV not enabled, VF number not configured or
	 * no queue assigned.
	 */
	if(!hw->func_caps.sr_iov_1_1 || pf->vf_num == 0 || pf->vf_nb_qps == 0)
		return I40E_SUCCESS;

	/* Allocate memory to store VF structure */
	pf->vfs = rte_zmalloc("i40e_pf_vf",sizeof(*pf->vfs) * pf->vf_num, 0);
	if(pf->vfs == NULL)
		return -ENOMEM;

	/* Disable irq0 for VFR event */
	i40e_pf_disable_irq0(hw);

	/* Disable VF link status interrupt */
	val = I40E_READ_REG(hw, I40E_PFGEN_PORTMDIO_NUM);
	val &= ~I40E_PFGEN_PORTMDIO_NUM_VFLINK_STAT_ENA_MASK;
	I40E_WRITE_REG(hw, I40E_PFGEN_PORTMDIO_NUM, val);
	I40E_WRITE_FLUSH(hw);

	for (i = 0; i < pf->vf_num; i++) {
		pf->vfs[i].pf = pf;
		pf->vfs[i].state = I40E_VF_INACTIVE;
		pf->vfs[i].vf_idx = i;
		ret = i40e_pf_host_vf_reset(&pf->vfs[i], 0);
		if (ret != I40E_SUCCESS)
			goto fail;
	}

	RTE_ETH_DEV_SRIOV(dev).active = pf->vf_num;
	/* restore irq0 */
	i40e_pf_enable_irq0(hw);

	return I40E_SUCCESS;

fail:
	rte_free(pf->vfs);
	i40e_pf_enable_irq0(hw);

	return ret;
}

int
i40e_pf_host_uninit(struct rte_eth_dev *dev)
{
	struct i40e_pf *pf = I40E_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_PF_TO_HW(pf);
	uint32_t val;

	PMD_INIT_FUNC_TRACE();

	/**
	 * return if SRIOV not enabled, VF number not configured or
	 * no queue assigned.
	 */
	if ((!hw->func_caps.sr_iov_1_1) ||
		(pf->vf_num == 0) ||
		(pf->vf_nb_qps == 0))
		return I40E_SUCCESS;

	/* free memory to store VF structure */
	rte_free(pf->vfs);
	pf->vfs = NULL;

	/* Disable irq0 for VFR event */
	i40e_pf_disable_irq0(hw);

	/* Disable VF link status interrupt */
	val = I40E_READ_REG(hw, I40E_PFGEN_PORTMDIO_NUM);
	val &= ~I40E_PFGEN_PORTMDIO_NUM_VFLINK_STAT_ENA_MASK;
	I40E_WRITE_REG(hw, I40E_PFGEN_PORTMDIO_NUM, val);
	I40E_WRITE_FLUSH(hw);

	return I40E_SUCCESS;
}
