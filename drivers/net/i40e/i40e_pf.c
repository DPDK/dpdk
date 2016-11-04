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

#define I40E_CFG_CRCSTRIP_DEFAULT 1

static int
i40e_pf_host_switch_queues(struct i40e_pf_vf *vf,
			   struct i40e_virtchnl_queue_select *qsel,
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
	struct i40e_virtchnl_queue_select qsel;

	if (vf == NULL)
		return -EINVAL;

	pf = vf->pf;
	hw = I40E_PF_TO_HW(vf->pf);
	vf_id = vf->vf_idx;
	abs_vf_id = vf_id + hw->func_caps.vf_base_id;

	/* Notify VF that we are in VFR progress */
	I40E_WRITE_REG(hw, I40E_VFGEN_RSTAT1(vf_id), I40E_PF_VFR_INPROGRESS);

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
	I40E_WRITE_REG(hw, I40E_VFGEN_RSTAT1(vf_id), I40E_PF_VFR_COMPLETED);
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
i40e_pf_host_process_cmd_version(struct i40e_pf_vf *vf)
{
	struct i40e_virtchnl_version_info info;

	info.major = I40E_DPDK_VERSION_MAJOR;
	info.minor = I40E_DPDK_VERSION_MINOR;
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_VERSION,
		I40E_SUCCESS, (uint8_t *)&info, sizeof(info));
}

static int
i40e_pf_host_process_cmd_reset_vf(struct i40e_pf_vf *vf)
{
	i40e_pf_host_vf_reset(vf, 1);

	/* No feedback will be sent to VF for VFLR */
	return I40E_SUCCESS;
}

static int
i40e_pf_host_process_cmd_get_vf_resource(struct i40e_pf_vf *vf)
{
	struct i40e_virtchnl_vf_resource *vf_res = NULL;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	uint32_t len = 0;
	int ret = I40E_SUCCESS;

	/* only have 1 VSI by default */
	len =  sizeof(struct i40e_virtchnl_vf_resource) +
				I40E_DEFAULT_VF_VSI_NUM *
		sizeof(struct i40e_virtchnl_vsi_resource);

	vf_res = rte_zmalloc("i40e_vf_res", len, 0);
	if (vf_res == NULL) {
		PMD_DRV_LOG(ERR, "failed to allocate mem");
		ret = I40E_ERR_NO_MEMORY;
		vf_res = NULL;
		len = 0;
		goto send_msg;
	}

	vf_res->vf_offload_flags = I40E_VIRTCHNL_VF_OFFLOAD_L2 |
				I40E_VIRTCHNL_VF_OFFLOAD_VLAN;
	vf_res->max_vectors = hw->func_caps.num_msix_vectors_vf;
	vf_res->num_queue_pairs = vf->vsi->nb_qps;
	vf_res->num_vsis = I40E_DEFAULT_VF_VSI_NUM;

	/* Change below setting if PF host can support more VSIs for VF */
	vf_res->vsi_res[0].vsi_type = I40E_VSI_SRIOV;
	/* As assume Vf only has single VSI now, always return 0 */
	vf_res->vsi_res[0].vsi_id = 0;
	vf_res->vsi_res[0].num_queue_pairs = vf->vsi->nb_qps;
	ether_addr_copy(&vf->mac_addr,
		(struct ether_addr *)vf_res->vsi_res[0].default_mac_addr);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_GET_VF_RESOURCES,
					ret, (uint8_t *)vf_res, len);
	rte_free(vf_res);

	return ret;
}

static int
i40e_pf_host_hmc_config_rxq(struct i40e_hw *hw,
			    struct i40e_pf_vf *vf,
			    struct i40e_virtchnl_rxq_info *rxq,
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

static int
i40e_pf_host_hmc_config_txq(struct i40e_hw *hw,
			    struct i40e_pf_vf *vf,
			    struct i40e_virtchnl_txq_info *txq)
{
	int err = I40E_SUCCESS;
	struct i40e_hmc_obj_txq tx_ctx;
	uint32_t qtx_ctl;
	uint16_t abs_queue_id = vf->vsi->base_queue + txq->queue_id;


	/* clear the context structure first */
	memset(&tx_ctx, 0, sizeof(tx_ctx));
	tx_ctx.new_context = 1;
	tx_ctx.base = txq->dma_ring_addr / I40E_QUEUE_BASE_ADDR_UNIT;
	tx_ctx.qlen = txq->ring_len;
	tx_ctx.rdylist = rte_le_to_cpu_16(vf->vsi->info.qs_handle[0]);
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
					   uint16_t msglen)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	struct i40e_vsi *vsi = vf->vsi;
	struct i40e_virtchnl_vsi_queue_config_info *vc_vqci =
		(struct i40e_virtchnl_vsi_queue_config_info *)msg;
	struct i40e_virtchnl_queue_pair_info *vc_qpi;
	int i, ret = I40E_SUCCESS;

	if (!msg || vc_vqci->num_queue_pairs > vsi->nb_qps ||
		vc_vqci->num_queue_pairs > I40E_MAX_VSI_QP ||
		msglen < I40E_VIRTCHNL_CONFIG_VSI_QUEUES_SIZE(vc_vqci,
					vc_vqci->num_queue_pairs)) {
		PMD_DRV_LOG(ERR, "vsi_queue_config_info argument wrong\n");
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
		 * If the opcode is I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
		 * then the extra information of
		 * 'struct i40e_virtchnl_queue_pair_extra_info' is needed,
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
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_config_vsi_queues_ext(struct i40e_pf_vf *vf,
					       uint8_t *msg,
					       uint16_t msglen)
{
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	struct i40e_vsi *vsi = vf->vsi;
	struct i40e_virtchnl_vsi_queue_config_ext_info *vc_vqcei =
		(struct i40e_virtchnl_vsi_queue_config_ext_info *)msg;
	struct i40e_virtchnl_queue_pair_ext_info *vc_qpei;
	int i, ret = I40E_SUCCESS;

	if (!msg || vc_vqcei->num_queue_pairs > vsi->nb_qps ||
		vc_vqcei->num_queue_pairs > I40E_MAX_VSI_QP ||
		msglen < I40E_VIRTCHNL_CONFIG_VSI_QUEUES_SIZE(vc_vqcei,
					vc_vqcei->num_queue_pairs)) {
		PMD_DRV_LOG(ERR, "vsi_queue_config_ext_info argument wrong\n");
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
		 * If the opcode is I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
		 * then the extra information of
		 * 'struct i40e_virtchnl_queue_pair_ext_info' is needed,
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
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT,
								ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_config_irq_map(struct i40e_pf_vf *vf,
					uint8_t *msg, uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_irq_map_info *irqmap =
	    (struct i40e_virtchnl_irq_map_info *)msg;

	if (msg == NULL || msglen < sizeof(struct i40e_virtchnl_irq_map_info)) {
		PMD_DRV_LOG(ERR, "buffer too short");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	/* Assume VF only have 1 vector to bind all queues */
	if (irqmap->num_vectors != 1) {
		PMD_DRV_LOG(ERR, "DKDK host only support 1 vector");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	/* This MSIX intr store the intr in VF range */
	vf->vsi->msix_intr = irqmap->vecmap[0].vector_id;
	vf->vsi->nb_msix = irqmap->num_vectors;
	vf->vsi->nb_used_qps = vf->vsi->nb_qps;

	/* Don't care how the TX/RX queue mapping with this vector.
	 * Link all VF RX queues together. Only did mapping work.
	 * VF can disable/enable the intr by itself.
	 */
	i40e_vsi_queues_bind_intr(vf->vsi);
send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_switch_queues(struct i40e_pf_vf *vf,
			   struct i40e_virtchnl_queue_select *qsel,
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
	struct i40e_virtchnl_queue_select *q_sel =
		(struct i40e_virtchnl_queue_select *)msg;

	if (msg == NULL || msglen != sizeof(*q_sel)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}
	ret = i40e_pf_host_switch_queues(vf, q_sel, true);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_ENABLE_QUEUES,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_disable_queues(struct i40e_pf_vf *vf,
					uint8_t *msg,
					uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_queue_select *q_sel =
		(struct i40e_virtchnl_queue_select *)msg;

	if (msg == NULL || msglen != sizeof(*q_sel)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}
	ret = i40e_pf_host_switch_queues(vf, q_sel, false);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_DISABLE_QUEUES,
							ret, NULL, 0);

	return ret;
}


static int
i40e_pf_host_process_cmd_add_ether_address(struct i40e_pf_vf *vf,
					   uint8_t *msg,
					   uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_ether_addr_list *addr_list =
			(struct i40e_virtchnl_ether_addr_list *)msg;
	struct i40e_mac_filter_info filter;
	int i;
	struct ether_addr *mac;

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
		if(!is_valid_assigned_ether_addr(mac) ||
			i40e_vsi_add_mac(vf->vsi, &filter)) {
			ret = I40E_ERR_INVALID_MAC_ADDR;
			goto send_msg;
		}
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_del_ether_address(struct i40e_pf_vf *vf,
					   uint8_t *msg,
					   uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_ether_addr_list *addr_list =
		(struct i40e_virtchnl_ether_addr_list *)msg;
	int i;
	struct ether_addr *mac;

	if (msg == NULL || msglen <= sizeof(*addr_list)) {
		PMD_DRV_LOG(ERR, "delete_ether_address argument too short");
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	for (i = 0; i < addr_list->num_elements; i++) {
		mac = (struct ether_addr *)(addr_list->list[i].addr);
		if(!is_valid_assigned_ether_addr(mac) ||
			i40e_vsi_delete_mac(vf->vsi, mac)) {
			ret = I40E_ERR_INVALID_MAC_ADDR;
			goto send_msg;
		}
	}

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS,
							ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_add_vlan(struct i40e_pf_vf *vf,
				uint8_t *msg, uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_vlan_filter_list *vlan_filter_list =
		(struct i40e_virtchnl_vlan_filter_list *)msg;
	int i;
	uint16_t *vid;

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
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_ADD_VLAN,
						ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_del_vlan(struct i40e_pf_vf *vf,
				  uint8_t *msg,
				  uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_vlan_filter_list *vlan_filter_list =
			(struct i40e_virtchnl_vlan_filter_list *)msg;
	int i;
	uint16_t *vid;

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
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_DEL_VLAN,
						ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_config_promisc_mode(
					struct i40e_pf_vf *vf,
					uint8_t *msg,
					uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_promisc_info *promisc =
				(struct i40e_virtchnl_promisc_info *)msg;
	struct i40e_hw *hw = I40E_PF_TO_HW(vf->pf);
	bool unicast = FALSE, multicast = FALSE;

	if (msg == NULL || msglen != sizeof(*promisc)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	if (promisc->flags & I40E_FLAG_VF_UNICAST_PROMISC)
		unicast = TRUE;
	ret = i40e_aq_set_vsi_unicast_promiscuous(hw,
			vf->vsi->seid, unicast, NULL, true);
	if (ret != I40E_SUCCESS)
		goto send_msg;

	if (promisc->flags & I40E_FLAG_VF_MULTICAST_PROMISC)
		multicast = TRUE;
	ret = i40e_aq_set_vsi_multicast_promiscuous(hw, vf->vsi->seid,
						multicast, NULL);

send_msg:
	i40e_pf_host_send_msg_to_vf(vf,
		I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE, ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_get_stats(struct i40e_pf_vf *vf)
{
	i40e_update_vsi_stats(vf->vsi);

	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_GET_STATS,
		I40E_SUCCESS, (uint8_t *)&vf->vsi->eth_stats,
				sizeof(vf->vsi->eth_stats));

	return I40E_SUCCESS;
}

static int
i40e_pf_host_process_cmd_cfg_vlan_offload(
					struct i40e_pf_vf *vf,
					uint8_t *msg,
					uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_vlan_offload_info *offload =
			(struct i40e_virtchnl_vlan_offload_info *)msg;

	if (msg == NULL || msglen != sizeof(*offload)) {
		ret = I40E_ERR_PARAM;
		goto send_msg;
	}

	ret = i40e_vsi_config_vlan_stripping(vf->vsi,
						!!offload->enable_vlan_strip);
	if (ret != 0)
		PMD_DRV_LOG(ERR, "Failed to configure vlan stripping");

send_msg:
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_CFG_VLAN_OFFLOAD,
					ret, NULL, 0);

	return ret;
}

static int
i40e_pf_host_process_cmd_cfg_pvid(struct i40e_pf_vf *vf,
					uint8_t *msg,
					uint16_t msglen)
{
	int ret = I40E_SUCCESS;
	struct i40e_virtchnl_pvid_info  *tpid_info =
			(struct i40e_virtchnl_pvid_info *)msg;

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

static void
i40e_notify_vf_link_status(struct rte_eth_dev *dev, struct i40e_pf_vf *vf)
{
	struct i40e_virtchnl_pf_event event;

	event.event = I40E_VIRTCHNL_EVENT_LINK_CHANGE;
	event.event_data.link_event.link_status =
		dev->data->dev_link.link_status;
	event.event_data.link_event.link_speed =
		(enum i40e_aq_link_speed)dev->data->dev_link.link_speed;
	i40e_pf_host_send_msg_to_vf(vf, I40E_VIRTCHNL_OP_EVENT,
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

	switch (opcode) {
	case I40E_VIRTCHNL_OP_VERSION :
		PMD_DRV_LOG(INFO, "OP_VERSION received");
		i40e_pf_host_process_cmd_version(vf);
		break;
	case I40E_VIRTCHNL_OP_RESET_VF :
		PMD_DRV_LOG(INFO, "OP_RESET_VF received");
		i40e_pf_host_process_cmd_reset_vf(vf);
		break;
	case I40E_VIRTCHNL_OP_GET_VF_RESOURCES:
		PMD_DRV_LOG(INFO, "OP_GET_VF_RESOURCES received");
		i40e_pf_host_process_cmd_get_vf_resource(vf);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES:
		PMD_DRV_LOG(INFO, "OP_CONFIG_VSI_QUEUES received");
		i40e_pf_host_process_cmd_config_vsi_queues(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES_EXT:
		PMD_DRV_LOG(INFO, "OP_CONFIG_VSI_QUEUES_EXT received");
		i40e_pf_host_process_cmd_config_vsi_queues_ext(vf, msg,
								msglen);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP:
		PMD_DRV_LOG(INFO, "OP_CONFIG_IRQ_MAP received");
		i40e_pf_host_process_cmd_config_irq_map(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_ENABLE_QUEUES:
		PMD_DRV_LOG(INFO, "OP_ENABLE_QUEUES received");
		i40e_pf_host_process_cmd_enable_queues(vf, msg, msglen);
		i40e_notify_vf_link_status(dev, vf);
		break;
	case I40E_VIRTCHNL_OP_DISABLE_QUEUES:
		PMD_DRV_LOG(INFO, "OP_DISABLE_QUEUE received");
		i40e_pf_host_process_cmd_disable_queues(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS:
		PMD_DRV_LOG(INFO, "OP_ADD_ETHER_ADDRESS received");
		i40e_pf_host_process_cmd_add_ether_address(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS:
		PMD_DRV_LOG(INFO, "OP_DEL_ETHER_ADDRESS received");
		i40e_pf_host_process_cmd_del_ether_address(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_ADD_VLAN:
		PMD_DRV_LOG(INFO, "OP_ADD_VLAN received");
		i40e_pf_host_process_cmd_add_vlan(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_DEL_VLAN:
		PMD_DRV_LOG(INFO, "OP_DEL_VLAN received");
		i40e_pf_host_process_cmd_del_vlan(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE:
		PMD_DRV_LOG(INFO, "OP_CONFIG_PROMISCUOUS_MODE received");
		i40e_pf_host_process_cmd_config_promisc_mode(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_GET_STATS:
		PMD_DRV_LOG(INFO, "OP_GET_STATS received");
		i40e_pf_host_process_cmd_get_stats(vf);
		break;
	case I40E_VIRTCHNL_OP_CFG_VLAN_OFFLOAD:
		PMD_DRV_LOG(INFO, "OP_CFG_VLAN_OFFLOAD received");
		i40e_pf_host_process_cmd_cfg_vlan_offload(vf, msg, msglen);
		break;
	case I40E_VIRTCHNL_OP_CFG_VLAN_PVID:
		PMD_DRV_LOG(INFO, "OP_CFG_VLAN_PVID received");
		i40e_pf_host_process_cmd_cfg_pvid(vf, msg, msglen);
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
		eth_random_addr(pf->vfs[i].mac_addr.addr_bytes);
	}

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
