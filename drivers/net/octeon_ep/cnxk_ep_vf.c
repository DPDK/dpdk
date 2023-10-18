/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2022 Marvell.
 */

#include <inttypes.h>
#include <errno.h>

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_memzone.h>
#include "otx_ep_common.h"
#include "cnxk_ep_vf.h"

static void
cnxk_ep_vf_setup_global_iq_reg(struct otx_ep_device *otx_ep, int q_no)
{
	volatile uint64_t reg_val = 0ull;

	/* Select ES, RO, NS, RDSIZE,DPTR Format#0 for IQs
	 * IS_64B is by default enabled.
	 */
	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_IN_CONTROL(q_no));

	reg_val |= CNXK_EP_R_IN_CTL_RDSIZE;
	reg_val |= CNXK_EP_R_IN_CTL_IS_64B;
	reg_val |= CNXK_EP_R_IN_CTL_ESR;

	oct_ep_write64(reg_val, otx_ep->hw_addr + CNXK_EP_R_IN_CONTROL(q_no));
}

static void
cnxk_ep_vf_setup_global_oq_reg(struct otx_ep_device *otx_ep, int q_no)
{
	volatile uint64_t reg_val = 0ull;

	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_OUT_CONTROL(q_no));

	reg_val &= ~(CNXK_EP_R_OUT_CTL_IMODE);
	reg_val &= ~(CNXK_EP_R_OUT_CTL_ROR_P);
	reg_val &= ~(CNXK_EP_R_OUT_CTL_NSR_P);
	reg_val &= ~(CNXK_EP_R_OUT_CTL_ROR_I);
	reg_val &= ~(CNXK_EP_R_OUT_CTL_NSR_I);
	reg_val &= ~(CNXK_EP_R_OUT_CTL_ROR_D);
	reg_val &= ~(CNXK_EP_R_OUT_CTL_NSR_D);
	reg_val &= ~(CNXK_EP_R_OUT_CTL_ES_I | CNXK_EP_R_OUT_CTL_ES_D);

	/* INFO/DATA ptr swap is required  */
	reg_val |= (CNXK_EP_R_OUT_CTL_ES_P);
	oct_ep_write64(reg_val, otx_ep->hw_addr + CNXK_EP_R_OUT_CONTROL(q_no));
}

static int
cnxk_ep_vf_setup_global_input_regs(struct otx_ep_device *otx_ep)
{
	uint64_t q_no = 0ull;

	for (q_no = 0; q_no < (otx_ep->sriov_info.rings_per_vf); q_no++)
		cnxk_ep_vf_setup_global_iq_reg(otx_ep, q_no);
	return 0;
}

static int
cnxk_ep_vf_setup_global_output_regs(struct otx_ep_device *otx_ep)
{
	uint32_t q_no;

	for (q_no = 0; q_no < (otx_ep->sriov_info.rings_per_vf); q_no++)
		cnxk_ep_vf_setup_global_oq_reg(otx_ep, q_no);
	return 0;
}

static int
cnxk_ep_vf_setup_device_regs(struct otx_ep_device *otx_ep)
{
	int ret;

	ret = cnxk_ep_vf_setup_global_input_regs(otx_ep);
	if (ret)
		return ret;
	ret = cnxk_ep_vf_setup_global_output_regs(otx_ep);
	return ret;
}

static int
cnxk_ep_vf_setup_iq_regs(struct otx_ep_device *otx_ep, uint32_t iq_no)
{
	struct otx_ep_instr_queue *iq = otx_ep->instr_queue[iq_no];
	int loop = OTX_EP_BUSY_LOOP_COUNT;
	volatile uint64_t reg_val = 0ull;
	uint64_t ism_addr;

	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_IN_CONTROL(iq_no));

	/* Wait till IDLE to set to 1, not supposed to configure BADDR
	 * as long as IDLE is 0
	 */
	if (!(reg_val & CNXK_EP_R_IN_CTL_IDLE)) {
		do {
			reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_IN_CONTROL(iq_no));
			rte_delay_ms(1);
		} while ((!(reg_val & CNXK_EP_R_IN_CTL_IDLE)) && loop--);
	}

	if (loop < 0) {
		otx_ep_err("IDLE bit is not set\n");
		return -EIO;
	}

	/* Configure input queue instruction size. */
	if (otx_ep->conf->iq.instr_type == OTX_EP_32BYTE_INSTR)
		reg_val &= ~(CNXK_EP_R_IN_CTL_IS_64B);
	else
		reg_val |= CNXK_EP_R_IN_CTL_IS_64B;
	oct_ep_write64(reg_val, otx_ep->hw_addr + CNXK_EP_R_IN_CONTROL(iq_no));
	iq->desc_size = otx_ep->conf->iq.instr_type;

	/* Write the start of the input queue's ring and its size  */
	oct_ep_write64(iq->base_addr_dma, otx_ep->hw_addr + CNXK_EP_R_IN_INSTR_BADDR(iq_no));
	oct_ep_write64(iq->nb_desc, otx_ep->hw_addr + CNXK_EP_R_IN_INSTR_RSIZE(iq_no));

	/* Remember the doorbell & instruction count register addr
	 * for this queue
	 */
	iq->doorbell_reg = (uint8_t *)otx_ep->hw_addr + CNXK_EP_R_IN_INSTR_DBELL(iq_no);
	iq->inst_cnt_reg = (uint8_t *)otx_ep->hw_addr + CNXK_EP_R_IN_CNTS(iq_no);

	otx_ep_dbg("InstQ[%d]:dbell reg @ 0x%p instcnt_reg @ 0x%p",
		   iq_no, iq->doorbell_reg, iq->inst_cnt_reg);
	loop = OTX_EP_BUSY_LOOP_COUNT;
	do {
		reg_val = rte_read32(iq->inst_cnt_reg);
		rte_write32(reg_val, iq->inst_cnt_reg);
		rte_delay_ms(1);
	} while (reg_val != 0 && loop--);

	if (loop < 0) {
		otx_ep_err("INST CNT REGISTER is not zero\n");
		return -EIO;
	}

	/* IN INTR_THRESHOLD is set to max(FFFFFFFF) which disable the IN INTR
	 * to raise
	 */
	oct_ep_write64(OTX_EP_CLEAR_SDP_IN_INT_LVLS,
		       otx_ep->hw_addr + CNXK_EP_R_IN_INT_LEVELS(iq_no));
	/* Set up IQ ISM registers and structures */
	ism_addr = (otx_ep->ism_buffer_mz->iova | CNXK_EP_ISM_EN
		    | CNXK_EP_ISM_MSIX_DIS)
		    + CNXK_EP_IQ_ISM_OFFSET(iq_no);
	rte_write64(ism_addr, (uint8_t *)otx_ep->hw_addr +
		    CNXK_EP_R_IN_CNTS_ISM(iq_no));
	iq->inst_cnt_ism =
		(uint32_t *)((uint8_t *)otx_ep->ism_buffer_mz->addr
			     + CNXK_EP_IQ_ISM_OFFSET(iq_no));
	otx_ep_err("SDP_R[%d] INST Q ISM virt: %p, dma: 0x%" PRIX64, iq_no,
		   (void *)iq->inst_cnt_ism, ism_addr);
	*iq->inst_cnt_ism = 0;
	iq->inst_cnt_ism_prev = 0;
	iq->partial_ih = ((uint64_t)otx_ep->pkind) << 36;

	return 0;
}

static int
cnxk_ep_vf_setup_oq_regs(struct otx_ep_device *otx_ep, uint32_t oq_no)
{
	volatile uint64_t reg_val = 0ull;
	uint64_t oq_ctl = 0ull;
	int loop = OTX_EP_BUSY_LOOP_COUNT;
	struct otx_ep_droq *droq = otx_ep->droq[oq_no];
	uint64_t ism_addr;

	/* Wait on IDLE to set to 1, supposed to configure BADDR
	 * as long as IDLE is 0
	 */
	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_OUT_CONTROL(oq_no));

	while ((!(reg_val & CNXK_EP_R_OUT_CTL_IDLE)) && loop--) {
		reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_OUT_CONTROL(oq_no));
		rte_delay_ms(1);
	}

	if (loop < 0) {
		otx_ep_err("OUT CNT REGISTER value is zero\n");
		return -EIO;
	}

	oct_ep_write64(droq->desc_ring_dma, otx_ep->hw_addr + CNXK_EP_R_OUT_SLIST_BADDR(oq_no));
	oct_ep_write64(droq->nb_desc, otx_ep->hw_addr + CNXK_EP_R_OUT_SLIST_RSIZE(oq_no));

	oq_ctl = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_OUT_CONTROL(oq_no));

	/* Clear the ISIZE and BSIZE (22-0) */
	oq_ctl &= ~(OTX_EP_CLEAR_ISIZE_BSIZE);

	/* Populate the BSIZE (15-0) */
	oq_ctl |= (droq->buffer_size & OTX_EP_DROQ_BUFSZ_MASK);

	oct_ep_write64(oq_ctl, otx_ep->hw_addr + CNXK_EP_R_OUT_CONTROL(oq_no));

	/* Mapped address of the pkt_sent and pkts_credit regs */
	droq->pkts_sent_reg = (uint8_t *)otx_ep->hw_addr + CNXK_EP_R_OUT_CNTS(oq_no);
	droq->pkts_credit_reg = (uint8_t *)otx_ep->hw_addr + CNXK_EP_R_OUT_SLIST_DBELL(oq_no);

	rte_write64(OTX_EP_CLEAR_OUT_INT_LVLS, otx_ep->hw_addr + CNXK_EP_R_OUT_INT_LEVELS(oq_no));

	/* Clear PKT_CNT register */
	rte_write64(OTX_EP_CLEAR_SDP_OUT_PKT_CNT, (uint8_t *)otx_ep->hw_addr +
		    CNXK_EP_R_OUT_PKT_CNT(oq_no));

	/* Clear the OQ doorbell  */
	rte_write32(OTX_EP_CLEAR_SLIST_DBELL, droq->pkts_credit_reg);
	loop = OTX_EP_BUSY_LOOP_COUNT;
	while ((rte_read32(droq->pkts_credit_reg) != 0ull) && loop--) {
		rte_write32(OTX_EP_CLEAR_SLIST_DBELL, droq->pkts_credit_reg);
		rte_delay_ms(1);
	}

	if (loop < 0) {
		otx_ep_err("Packets credit register value is not cleared\n");
		return -EIO;
	}

	otx_ep_dbg("SDP_R[%d]_credit:%x", oq_no, rte_read32(droq->pkts_credit_reg));

	/* Clear the OQ_OUT_CNTS doorbell  */
	reg_val = rte_read32(droq->pkts_sent_reg);
	rte_write32((uint32_t)reg_val, droq->pkts_sent_reg);

	otx_ep_dbg("SDP_R[%d]_sent: %x", oq_no, rte_read32(droq->pkts_sent_reg));
	/* Set up ISM registers and structures */
	ism_addr = (otx_ep->ism_buffer_mz->iova | CNXK_EP_ISM_EN
		    | CNXK_EP_ISM_MSIX_DIS)
		    + CNXK_EP_OQ_ISM_OFFSET(oq_no);
	rte_write64(ism_addr, (uint8_t *)otx_ep->hw_addr +
		    CNXK_EP_R_OUT_CNTS_ISM(oq_no));
	droq->pkts_sent_ism =
		(uint32_t *)((uint8_t *)otx_ep->ism_buffer_mz->addr
			     + CNXK_EP_OQ_ISM_OFFSET(oq_no));
	otx_ep_err("SDP_R[%d] OQ ISM virt: %p dma: 0x%" PRIX64,
		    oq_no, (void *)droq->pkts_sent_ism, ism_addr);
	*droq->pkts_sent_ism = 0;
	droq->pkts_sent_ism_prev = 0;

	loop = OTX_EP_BUSY_LOOP_COUNT;
	while (((rte_read32(droq->pkts_sent_reg)) != 0ull) && loop--) {
		reg_val = rte_read32(droq->pkts_sent_reg);
		rte_write32((uint32_t)reg_val, droq->pkts_sent_reg);
		rte_delay_ms(1);
	}

	if (loop < 0) {
		otx_ep_err("Packets sent register value is not cleared\n");
		return -EIO;
	}

	otx_ep_dbg("SDP_R[%d]_sent: %x", oq_no, rte_read32(droq->pkts_sent_reg));

	/* Set Watermark for backpressure */
	oct_ep_write64(OTX_EP_OQ_WMARK_MIN,
		       otx_ep->hw_addr + CNXK_EP_R_OUT_WMARK(oq_no));

	return 0;
}

static int
cnxk_ep_vf_enable_iq(struct otx_ep_device *otx_ep, uint32_t q_no)
{
	int loop = OTX_EP_BUSY_LOOP_COUNT;
	uint64_t reg_val = 0ull;

	/* Resetting doorbells during IQ enabling also to handle abrupt
	 * guest reboot. IQ reset does not clear the doorbells.
	 */
	oct_ep_write64(0xFFFFFFFF, otx_ep->hw_addr + CNXK_EP_R_IN_INSTR_DBELL(q_no));

	while (((oct_ep_read64(otx_ep->hw_addr +
		 CNXK_EP_R_IN_INSTR_DBELL(q_no))) != 0ull) && loop--) {
		rte_delay_ms(1);
	}

	if (loop < 0) {
		otx_ep_err("INSTR DBELL not coming back to 0\n");
		return -EIO;
	}

	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_IN_ENABLE(q_no));
	reg_val |= 0x1ull;

	oct_ep_write64(reg_val, otx_ep->hw_addr + CNXK_EP_R_IN_ENABLE(q_no));

	otx_ep_info("IQ[%d] enable done", q_no);

	return 0;
}

static int
cnxk_ep_vf_enable_oq(struct otx_ep_device *otx_ep, uint32_t q_no)
{
	uint64_t reg_val = 0ull;

	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_OUT_ENABLE(q_no));
	reg_val |= 0x1ull;
	oct_ep_write64(reg_val, otx_ep->hw_addr + CNXK_EP_R_OUT_ENABLE(q_no));

	otx_ep_info("OQ[%d] enable done", q_no);

	return 0;
}

static int
cnxk_ep_vf_enable_io_queues(struct otx_ep_device *otx_ep)
{
	uint32_t q_no = 0;
	int ret;

	for (q_no = 0; q_no < otx_ep->nb_tx_queues; q_no++) {
		ret = cnxk_ep_vf_enable_iq(otx_ep, q_no);
		if (ret)
			return ret;
	}

	for (q_no = 0; q_no < otx_ep->nb_rx_queues; q_no++)
		cnxk_ep_vf_enable_oq(otx_ep, q_no);

	return 0;
}

static void
cnxk_ep_vf_disable_iq(struct otx_ep_device *otx_ep, uint32_t q_no)
{
	uint64_t reg_val = 0ull;

	/* Reset the doorbell register for this Input Queue. */
	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_IN_ENABLE(q_no));
	reg_val &= ~0x1ull;

	oct_ep_write64(reg_val, otx_ep->hw_addr + CNXK_EP_R_IN_ENABLE(q_no));
}

static void
cnxk_ep_vf_disable_oq(struct otx_ep_device *otx_ep, uint32_t q_no)
{
	volatile uint64_t reg_val = 0ull;

	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_OUT_ENABLE(q_no));
	reg_val &= ~0x1ull;

	oct_ep_write64(reg_val, otx_ep->hw_addr + CNXK_EP_R_OUT_ENABLE(q_no));
}

static void
cnxk_ep_vf_disable_io_queues(struct otx_ep_device *otx_ep)
{
	uint32_t q_no = 0;

	for (q_no = 0; q_no < otx_ep->sriov_info.rings_per_vf; q_no++) {
		cnxk_ep_vf_disable_iq(otx_ep, q_no);
		cnxk_ep_vf_disable_oq(otx_ep, q_no);
	}
}

static const struct otx_ep_config default_cnxk_ep_conf = {
	/* IQ attributes */
	.iq                        = {
		.max_iqs           = OTX_EP_CFG_IO_QUEUES,
		.instr_type        = OTX_EP_32BYTE_INSTR,
		.pending_list_size = (OTX_EP_MAX_IQ_DESCRIPTORS *
				      OTX_EP_CFG_IO_QUEUES),
	},

	/* OQ attributes */
	.oq                        = {
		.max_oqs           = OTX_EP_CFG_IO_QUEUES,
		.info_ptr          = OTX_EP_OQ_INFOPTR_MODE,
		.refill_threshold  = OTX_EP_OQ_REFIL_THRESHOLD,
	},

	.num_iqdef_descs           = OTX_EP_MAX_IQ_DESCRIPTORS,
	.num_oqdef_descs           = OTX_EP_MAX_OQ_DESCRIPTORS,
	.oqdef_buf_size            = OTX_EP_OQ_BUF_SIZE,
};

static const struct otx_ep_config*
cnxk_ep_get_defconf(struct otx_ep_device *otx_ep_dev __rte_unused)
{
	const struct otx_ep_config *default_conf = NULL;

	default_conf = &default_cnxk_ep_conf;

	return default_conf;
}

int
cnxk_ep_vf_setup_device(struct otx_ep_device *otx_ep)
{
	uint64_t reg_val = 0ull;

	/* If application does not provide its conf, use driver default conf */
	if (otx_ep->conf == NULL) {
		otx_ep->conf = cnxk_ep_get_defconf(otx_ep);
		if (otx_ep->conf == NULL) {
			otx_ep_err("SDP VF default config not found");
			return -ENOENT;
		}
		otx_ep_info("Default config is used");
	}

	/* Get IOQs (RPVF] count */
	reg_val = oct_ep_read64(otx_ep->hw_addr + CNXK_EP_R_IN_CONTROL(0));

	otx_ep->sriov_info.rings_per_vf =
		((reg_val >> CNXK_EP_R_IN_CTL_RPVF_POS) & CNXK_EP_R_IN_CTL_RPVF_MASK);

	otx_ep_info("SDP RPVF: %d", otx_ep->sriov_info.rings_per_vf);

	otx_ep->fn_list.setup_iq_regs       = cnxk_ep_vf_setup_iq_regs;
	otx_ep->fn_list.setup_oq_regs       = cnxk_ep_vf_setup_oq_regs;

	otx_ep->fn_list.setup_device_regs   = cnxk_ep_vf_setup_device_regs;

	otx_ep->fn_list.enable_io_queues    = cnxk_ep_vf_enable_io_queues;
	otx_ep->fn_list.disable_io_queues   = cnxk_ep_vf_disable_io_queues;

	otx_ep->fn_list.enable_iq           = cnxk_ep_vf_enable_iq;
	otx_ep->fn_list.disable_iq          = cnxk_ep_vf_disable_iq;

	otx_ep->fn_list.enable_oq           = cnxk_ep_vf_enable_oq;
	otx_ep->fn_list.disable_oq          = cnxk_ep_vf_disable_oq;

	return 0;
}
