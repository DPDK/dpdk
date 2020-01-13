/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#include <rte_common.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "otx2_common.h"
#include "otx2_ep_rawdev.h"
#include "otx2_ep_vf.h"

static int
sdp_vf_reset_iq(struct sdp_device *sdpvf, int q_no)
{
	uint64_t loop = SDP_VF_BUSY_LOOP_COUNT;
	volatile uint64_t d64 = 0ull;

	/* There is no RST for a ring.
	 * Clear all registers one by one after disabling the ring
	 */

	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_ENABLE(q_no));
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_INSTR_BADDR(q_no));
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_INSTR_RSIZE(q_no));

	d64 = 0xFFFFFFFF; /* ~0ull */
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_INSTR_DBELL(q_no));
	d64 = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_INSTR_DBELL(q_no));

	while ((d64 != 0) && loop--) {
		otx2_write64(d64, sdpvf->hw_addr +
			     SDP_VF_R_IN_INSTR_DBELL(q_no));

		rte_delay_ms(1);

		d64 = otx2_read64(sdpvf->hw_addr +
				  SDP_VF_R_IN_INSTR_DBELL(q_no));
	}

	loop = SDP_VF_BUSY_LOOP_COUNT;
	d64 = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_CNTS(q_no));
	while ((d64 != 0) && loop--) {
		otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_CNTS(q_no));

		rte_delay_ms(1);

		d64 = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_CNTS(q_no));
	}

	d64 = 0ull;
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_INT_LEVELS(q_no));
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_PKT_CNT(q_no));
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_IN_BYTE_CNT(q_no));

	return 0;
}

static int
sdp_vf_reset_oq(struct sdp_device *sdpvf, int q_no)
{
	uint64_t loop = SDP_VF_BUSY_LOOP_COUNT;
	volatile uint64_t d64 = 0ull;

	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_ENABLE(q_no));

	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_SLIST_BADDR(q_no));

	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_SLIST_RSIZE(q_no));

	d64 = 0xFFFFFFFF;
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_SLIST_DBELL(q_no));
	d64 = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_SLIST_DBELL(q_no));

	while ((d64 != 0) && loop--) {
		otx2_write64(d64, sdpvf->hw_addr +
			     SDP_VF_R_OUT_SLIST_DBELL(q_no));

		rte_delay_ms(1);

		d64 = otx2_read64(sdpvf->hw_addr +
				  SDP_VF_R_OUT_SLIST_DBELL(q_no));
	}

	loop = SDP_VF_BUSY_LOOP_COUNT;
	d64 = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_CNTS(q_no));
	while ((d64 != 0) && (loop--)) {
		otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_CNTS(q_no));

		rte_delay_ms(1);

		d64 = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_CNTS(q_no));
	}

	d64 = 0ull;
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_INT_LEVELS(q_no));
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_PKT_CNT(q_no));
	otx2_write64(d64, sdpvf->hw_addr + SDP_VF_R_OUT_BYTE_CNT(q_no));

	return 0;
}

static void
sdp_vf_setup_global_iq_reg(struct sdp_device *sdpvf, int q_no)
{
	volatile uint64_t reg_val = 0ull;

	/* Select ES, RO, NS, RDSIZE,DPTR Fomat#0 for IQs
	 * IS_64B is by default enabled.
	 */
	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_CONTROL(q_no));

	reg_val |= SDP_VF_R_IN_CTL_RDSIZE;
	reg_val |= SDP_VF_R_IN_CTL_IS_64B;
	reg_val |= SDP_VF_R_IN_CTL_ESR;

	otx2_write64(reg_val, sdpvf->hw_addr + SDP_VF_R_IN_CONTROL(q_no));

}

static void
sdp_vf_setup_global_oq_reg(struct sdp_device *sdpvf, int q_no)
{
	volatile uint64_t reg_val = 0ull;

	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_CONTROL(q_no));

	reg_val |= (SDP_VF_R_OUT_CTL_IMODE);

	reg_val &= ~(SDP_VF_R_OUT_CTL_ROR_P);
	reg_val &= ~(SDP_VF_R_OUT_CTL_NSR_P);
	reg_val &= ~(SDP_VF_R_OUT_CTL_ROR_I);
	reg_val &= ~(SDP_VF_R_OUT_CTL_NSR_I);
	reg_val &= ~(SDP_VF_R_OUT_CTL_ES_I);
	reg_val &= ~(SDP_VF_R_OUT_CTL_ROR_D);
	reg_val &= ~(SDP_VF_R_OUT_CTL_NSR_D);
	reg_val &= ~(SDP_VF_R_OUT_CTL_ES_D);

	/* INFO/DATA ptr swap is required  */
	reg_val |= (SDP_VF_R_OUT_CTL_ES_P);

	otx2_write64(reg_val, sdpvf->hw_addr + SDP_VF_R_OUT_CONTROL(q_no));

}

static int
sdp_vf_reset_input_queues(struct sdp_device *sdpvf)
{
	uint32_t q_no = 0;

	otx2_sdp_dbg("%s :", __func__);

	for (q_no = 0; q_no < sdpvf->sriov_info.rings_per_vf; q_no++)
		sdp_vf_reset_iq(sdpvf, q_no);

	return 0;
}

static int
sdp_vf_reset_output_queues(struct sdp_device *sdpvf)
{
	uint64_t q_no = 0ull;

	otx2_sdp_dbg(" %s :", __func__);

	for (q_no = 0; q_no < sdpvf->sriov_info.rings_per_vf; q_no++)
		sdp_vf_reset_oq(sdpvf, q_no);

	return 0;
}

static void
sdp_vf_setup_global_input_regs(struct sdp_device *sdpvf)
{
	uint64_t q_no = 0ull;

	sdp_vf_reset_input_queues(sdpvf);

	for (q_no = 0; q_no < (sdpvf->sriov_info.rings_per_vf); q_no++)
		sdp_vf_setup_global_iq_reg(sdpvf, q_no);
}

static void
sdp_vf_setup_global_output_regs(struct sdp_device *sdpvf)
{
	uint32_t q_no;

	sdp_vf_reset_output_queues(sdpvf);

	for (q_no = 0; q_no < (sdpvf->sriov_info.rings_per_vf); q_no++)
		sdp_vf_setup_global_oq_reg(sdpvf, q_no);

}

static int
sdp_vf_setup_device_regs(struct sdp_device *sdpvf)
{
	sdp_vf_setup_global_input_regs(sdpvf);
	sdp_vf_setup_global_output_regs(sdpvf);

	return 0;
}

static void
sdp_vf_setup_iq_regs(struct sdp_device *sdpvf, uint32_t iq_no)
{
	struct sdp_instr_queue *iq = sdpvf->instr_queue[iq_no];
	volatile uint64_t reg_val = 0ull;

	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_CONTROL(iq_no));

	/* Wait till IDLE to set to 1, not supposed to configure BADDR
	 * as long as IDLE is 0
	 */
	if (!(reg_val & SDP_VF_R_IN_CTL_IDLE)) {
		do {
			reg_val = otx2_read64(sdpvf->hw_addr +
					      SDP_VF_R_IN_CONTROL(iq_no));
		} while (!(reg_val & SDP_VF_R_IN_CTL_IDLE));
	}

	/* Write the start of the input queue's ring and its size  */
	otx2_write64(iq->base_addr_dma, sdpvf->hw_addr +
		     SDP_VF_R_IN_INSTR_BADDR(iq_no));
	otx2_write64(iq->nb_desc, sdpvf->hw_addr +
		     SDP_VF_R_IN_INSTR_RSIZE(iq_no));

	/* Remember the doorbell & instruction count register addr
	 * for this queue
	 */
	iq->doorbell_reg = (uint8_t *) sdpvf->hw_addr +
			   SDP_VF_R_IN_INSTR_DBELL(iq_no);
	iq->inst_cnt_reg = (uint8_t *) sdpvf->hw_addr +
			   SDP_VF_R_IN_CNTS(iq_no);

	otx2_sdp_dbg("InstQ[%d]:dbell reg @ 0x%p instcnt_reg @ 0x%p",
		     iq_no, iq->doorbell_reg, iq->inst_cnt_reg);

	/* Store the current instrn counter(used in flush_iq calculation) */
	iq->reset_instr_cnt = rte_read32(iq->inst_cnt_reg);

	/* IN INTR_THRESHOLD is set to max(FFFFFFFF) which disable the IN INTR
	 * to raise
	 */
	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_INT_LEVELS(iq_no));
	reg_val = 0xffffffff;

	otx2_write64(reg_val, sdpvf->hw_addr + SDP_VF_R_IN_INT_LEVELS(iq_no));

}

static void
sdp_vf_setup_oq_regs(struct sdp_device *sdpvf, uint32_t oq_no)
{
	volatile uint64_t reg_val = 0ull;
	uint64_t oq_ctl = 0ull;

	struct sdp_droq *droq = sdpvf->droq[oq_no];

	/* Wait on IDLE to set to 1, supposed to configure BADDR
	 * as log as IDLE is 0
	 */
	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_CONTROL(oq_no));

	while (!(reg_val & SDP_VF_R_OUT_CTL_IDLE)) {
		reg_val = otx2_read64(sdpvf->hw_addr +
				      SDP_VF_R_OUT_CONTROL(oq_no));
	}

	otx2_write64(droq->desc_ring_dma, sdpvf->hw_addr +
		     SDP_VF_R_OUT_SLIST_BADDR(oq_no));
	otx2_write64(droq->nb_desc, sdpvf->hw_addr +
		     SDP_VF_R_OUT_SLIST_RSIZE(oq_no));

	oq_ctl = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_CONTROL(oq_no));

	/* Clear the ISIZE and BSIZE (22-0) */
	oq_ctl &= ~(0x7fffffull);

	/* Populate the BSIZE (15-0) */
	oq_ctl |= (droq->buffer_size & 0xffff);

	/* Populate ISIZE(22-16) */
	oq_ctl |= ((SDP_RH_SIZE << 16) & 0x7fffff);
	otx2_write64(oq_ctl, sdpvf->hw_addr + SDP_VF_R_OUT_CONTROL(oq_no));

	/* Mapped address of the pkt_sent and pkts_credit regs */
	droq->pkts_sent_reg = (uint8_t *) sdpvf->hw_addr +
			      SDP_VF_R_OUT_CNTS(oq_no);
	droq->pkts_credit_reg = (uint8_t *) sdpvf->hw_addr +
				SDP_VF_R_OUT_SLIST_DBELL(oq_no);

	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_INT_LEVELS(oq_no));

	/* Clear PKT_CNT register */
	rte_write64(0xFFFFFFFFF, (uint8_t *)sdpvf->hw_addr +
		    SDP_VF_R_OUT_PKT_CNT(oq_no));

	/* Clear the OQ doorbell  */
	rte_write32(0xFFFFFFFF, droq->pkts_credit_reg);
	while ((rte_read32(droq->pkts_credit_reg) != 0ull)) {
		rte_write32(0xFFFFFFFF, droq->pkts_credit_reg);
		rte_delay_ms(1);
	}
	otx2_sdp_dbg("SDP_R[%d]_credit:%x", oq_no,
		     rte_read32(droq->pkts_credit_reg));

	/* Clear the OQ_OUT_CNTS doorbell  */
	reg_val = rte_read32(droq->pkts_sent_reg);
	rte_write32((uint32_t)reg_val, droq->pkts_sent_reg);

	otx2_sdp_dbg("SDP_R[%d]_sent: %x", oq_no,
		     rte_read32(droq->pkts_sent_reg));

	while (((rte_read32(droq->pkts_sent_reg)) != 0ull)) {
		reg_val = rte_read32(droq->pkts_sent_reg);
		rte_write32((uint32_t)reg_val, droq->pkts_sent_reg);
		rte_delay_ms(1);
	}

}

static void
sdp_vf_enable_iq(struct sdp_device *sdpvf, uint32_t q_no)
{
	volatile uint64_t reg_val = 0ull;
	uint64_t loop = SDP_VF_BUSY_LOOP_COUNT;

	/* Resetting doorbells during IQ enabling also to handle abrupt
	 * guest reboot. IQ reset does not clear the doorbells.
	 */
	otx2_write64(0xFFFFFFFF, sdpvf->hw_addr +
		     SDP_VF_R_IN_INSTR_DBELL(q_no));

	while (((otx2_read64(sdpvf->hw_addr +
		 SDP_VF_R_IN_INSTR_DBELL(q_no))) != 0ull) && loop--) {

		rte_delay_ms(1);
	}

	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_ENABLE(q_no));
	reg_val |= 0x1ull;

	otx2_write64(reg_val, sdpvf->hw_addr + SDP_VF_R_IN_ENABLE(q_no));

	otx2_info("IQ[%d] enable done", q_no);

}

static void
sdp_vf_enable_oq(struct sdp_device *sdpvf, uint32_t q_no)
{
	volatile uint64_t reg_val = 0ull;

	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_ENABLE(q_no));
	reg_val |= 0x1ull;
	otx2_write64(reg_val, sdpvf->hw_addr + SDP_VF_R_OUT_ENABLE(q_no));

	otx2_info("OQ[%d] enable done", q_no);
}

static void
sdp_vf_enable_io_queues(struct sdp_device *sdpvf)
{
	uint32_t q_no = 0;

	for (q_no = 0; q_no < sdpvf->num_iqs; q_no++)
		sdp_vf_enable_iq(sdpvf, q_no);

	for (q_no = 0; q_no < sdpvf->num_oqs; q_no++)
		sdp_vf_enable_oq(sdpvf, q_no);
}

static void
sdp_vf_disable_iq(struct sdp_device *sdpvf, uint32_t q_no)
{
	volatile uint64_t reg_val = 0ull;

	/* Reset the doorbell register for this Input Queue. */
	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_ENABLE(q_no));
	reg_val &= ~0x1ull;

	otx2_write64(reg_val, sdpvf->hw_addr + SDP_VF_R_IN_ENABLE(q_no));
}

static void
sdp_vf_disable_oq(struct sdp_device *sdpvf, uint32_t q_no)
{
	volatile uint64_t reg_val = 0ull;

	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_OUT_ENABLE(q_no));
	reg_val &= ~0x1ull;

	otx2_write64(reg_val, sdpvf->hw_addr + SDP_VF_R_OUT_ENABLE(q_no));

}

static void
sdp_vf_disable_io_queues(struct sdp_device *sdpvf)
{
	uint32_t q_no = 0;

	/* Disable Input Queues. */
	for (q_no = 0; q_no < sdpvf->num_iqs; q_no++)
		sdp_vf_disable_iq(sdpvf, q_no);

	/* Disable Output Queues. */
	for (q_no = 0; q_no < sdpvf->num_oqs; q_no++)
		sdp_vf_disable_oq(sdpvf, q_no);
}

static uint32_t
sdp_vf_update_read_index(struct sdp_instr_queue *iq)
{
	uint32_t new_idx = rte_read32(iq->inst_cnt_reg);

	/* The new instr cnt reg is a 32-bit counter that can roll over.
	 * We have noted the counter's initial value at init time into
	 * reset_instr_cnt
	 */
	if (iq->reset_instr_cnt < new_idx)
		new_idx -= iq->reset_instr_cnt;
	else
		new_idx += (0xffffffff - iq->reset_instr_cnt) + 1;

	/* Modulo of the new index with the IQ size will give us
	 * the new index.
	 */
	new_idx %= iq->nb_desc;

	return new_idx;
}

int
sdp_vf_setup_device(struct sdp_device *sdpvf)
{
	uint64_t reg_val = 0ull;

	/* If application doesn't provide its conf, use driver default conf */
	if (sdpvf->conf == NULL) {
		sdpvf->conf = sdp_get_defconf(sdpvf);
		if (sdpvf->conf == NULL) {
			otx2_err("SDP VF default config not found");
			return -ENOMEM;
		}
		otx2_info("Default config is used");
	}

	/* Get IOQs (RPVF] count */
	reg_val = otx2_read64(sdpvf->hw_addr + SDP_VF_R_IN_CONTROL(0));

	sdpvf->sriov_info.rings_per_vf = ((reg_val >> SDP_VF_R_IN_CTL_RPVF_POS)
					  & SDP_VF_R_IN_CTL_RPVF_MASK);

	otx2_info("SDP RPVF: %d", sdpvf->sriov_info.rings_per_vf);

	sdpvf->fn_list.setup_iq_regs       = sdp_vf_setup_iq_regs;
	sdpvf->fn_list.setup_oq_regs       = sdp_vf_setup_oq_regs;

	sdpvf->fn_list.setup_device_regs   = sdp_vf_setup_device_regs;
	sdpvf->fn_list.update_iq_read_idx  = sdp_vf_update_read_index;

	sdpvf->fn_list.enable_io_queues    = sdp_vf_enable_io_queues;
	sdpvf->fn_list.disable_io_queues   = sdp_vf_disable_io_queues;

	sdpvf->fn_list.enable_iq           = sdp_vf_enable_iq;
	sdpvf->fn_list.disable_iq          = sdp_vf_disable_iq;

	sdpvf->fn_list.enable_oq           = sdp_vf_enable_oq;
	sdpvf->fn_list.disable_oq          = sdp_vf_disable_oq;


	return 0;

}
