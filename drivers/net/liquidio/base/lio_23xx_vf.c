/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Cavium, Inc
 */

#include <string.h>

#include <rte_ethdev_driver.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include "lio_logs.h"
#include "lio_23xx_vf.h"
#include "lio_23xx_reg.h"
#include "lio_mbox.h"

static int
cn23xx_vf_reset_io_queues(struct lio_device *lio_dev, uint32_t num_queues)
{
	uint32_t loop = CN23XX_VF_BUSY_READING_REG_LOOP_COUNT;
	uint64_t d64, q_no;
	int ret_val = 0;

	PMD_INIT_FUNC_TRACE();

	for (q_no = 0; q_no < num_queues; q_no++) {
		/* set RST bit to 1. This bit applies to both IQ and OQ */
		d64 = lio_read_csr64(lio_dev,
				     CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
		d64 = d64 | CN23XX_PKT_INPUT_CTL_RST;
		lio_write_csr64(lio_dev, CN23XX_SLI_IQ_PKT_CONTROL64(q_no),
				d64);
	}

	/* wait until the RST bit is clear or the RST and QUIET bits are set */
	for (q_no = 0; q_no < num_queues; q_no++) {
		volatile uint64_t reg_val;

		reg_val	= lio_read_csr64(lio_dev,
					 CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
		while ((reg_val & CN23XX_PKT_INPUT_CTL_RST) &&
				!(reg_val & CN23XX_PKT_INPUT_CTL_QUIET) &&
				loop) {
			reg_val = lio_read_csr64(
					lio_dev,
					CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
			loop = loop - 1;
		}

		if (loop == 0) {
			lio_dev_err(lio_dev,
				    "clearing the reset reg failed or setting the quiet reg failed for qno: %lu\n",
				    (unsigned long)q_no);
			return -1;
		}

		reg_val = reg_val & ~CN23XX_PKT_INPUT_CTL_RST;
		lio_write_csr64(lio_dev, CN23XX_SLI_IQ_PKT_CONTROL64(q_no),
				reg_val);

		reg_val = lio_read_csr64(
		    lio_dev, CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
		if (reg_val & CN23XX_PKT_INPUT_CTL_RST) {
			lio_dev_err(lio_dev,
				    "clearing the reset failed for qno: %lu\n",
				    (unsigned long)q_no);
			ret_val = -1;
		}
	}

	return ret_val;
}

static int
cn23xx_vf_setup_global_input_regs(struct lio_device *lio_dev)
{
	uint64_t q_no;
	uint64_t d64;

	PMD_INIT_FUNC_TRACE();

	if (cn23xx_vf_reset_io_queues(lio_dev,
				      lio_dev->sriov_info.rings_per_vf))
		return -1;

	for (q_no = 0; q_no < (lio_dev->sriov_info.rings_per_vf); q_no++) {
		lio_write_csr64(lio_dev, CN23XX_SLI_IQ_DOORBELL(q_no),
				0xFFFFFFFF);

		d64 = lio_read_csr64(lio_dev,
				     CN23XX_SLI_IQ_INSTR_COUNT64(q_no));

		d64 &= 0xEFFFFFFFFFFFFFFFL;

		lio_write_csr64(lio_dev, CN23XX_SLI_IQ_INSTR_COUNT64(q_no),
				d64);

		/* Select ES, RO, NS, RDSIZE,DPTR Fomat#0 for
		 * the Input Queues
		 */
		lio_write_csr64(lio_dev, CN23XX_SLI_IQ_PKT_CONTROL64(q_no),
				CN23XX_PKT_INPUT_CTL_MASK);
	}

	return 0;
}

static void
cn23xx_vf_setup_global_output_regs(struct lio_device *lio_dev)
{
	uint32_t reg_val;
	uint32_t q_no;

	PMD_INIT_FUNC_TRACE();

	for (q_no = 0; q_no < lio_dev->sriov_info.rings_per_vf; q_no++) {
		lio_write_csr(lio_dev, CN23XX_SLI_OQ_PKTS_CREDIT(q_no),
			      0xFFFFFFFF);

		reg_val =
		    lio_read_csr(lio_dev, CN23XX_SLI_OQ_PKTS_SENT(q_no));

		reg_val &= 0xEFFFFFFFFFFFFFFFL;

		lio_write_csr(lio_dev, CN23XX_SLI_OQ_PKTS_SENT(q_no), reg_val);

		reg_val =
		    lio_read_csr(lio_dev, CN23XX_SLI_OQ_PKT_CONTROL(q_no));

		/* set IPTR & DPTR */
		reg_val |=
		    (CN23XX_PKT_OUTPUT_CTL_IPTR | CN23XX_PKT_OUTPUT_CTL_DPTR);

		/* reset BMODE */
		reg_val &= ~(CN23XX_PKT_OUTPUT_CTL_BMODE);

		/* No Relaxed Ordering, No Snoop, 64-bit Byte swap
		 * for Output Queue Scatter List
		 * reset ROR_P, NSR_P
		 */
		reg_val &= ~(CN23XX_PKT_OUTPUT_CTL_ROR_P);
		reg_val &= ~(CN23XX_PKT_OUTPUT_CTL_NSR_P);

#if RTE_BYTE_ORDER == RTE_LITTLE_ENDIAN
		reg_val &= ~(CN23XX_PKT_OUTPUT_CTL_ES_P);
#elif RTE_BYTE_ORDER == RTE_BIG_ENDIAN
		reg_val |= (CN23XX_PKT_OUTPUT_CTL_ES_P);
#endif
		/* No Relaxed Ordering, No Snoop, 64-bit Byte swap
		 * for Output Queue Data
		 * reset ROR, NSR
		 */
		reg_val &= ~(CN23XX_PKT_OUTPUT_CTL_ROR);
		reg_val &= ~(CN23XX_PKT_OUTPUT_CTL_NSR);
		/* set the ES bit */
		reg_val |= (CN23XX_PKT_OUTPUT_CTL_ES);

		/* write all the selected settings */
		lio_write_csr(lio_dev, CN23XX_SLI_OQ_PKT_CONTROL(q_no),
			      reg_val);
	}
}

static int
cn23xx_vf_setup_device_regs(struct lio_device *lio_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (cn23xx_vf_setup_global_input_regs(lio_dev))
		return -1;

	cn23xx_vf_setup_global_output_regs(lio_dev);

	return 0;
}

static void
cn23xx_vf_setup_iq_regs(struct lio_device *lio_dev, uint32_t iq_no)
{
	struct lio_instr_queue *iq = lio_dev->instr_queue[iq_no];
	uint64_t pkt_in_done = 0;

	PMD_INIT_FUNC_TRACE();

	/* Write the start of the input queue's ring and its size */
	lio_write_csr64(lio_dev, CN23XX_SLI_IQ_BASE_ADDR64(iq_no),
			iq->base_addr_dma);
	lio_write_csr(lio_dev, CN23XX_SLI_IQ_SIZE(iq_no), iq->nb_desc);

	/* Remember the doorbell & instruction count register addr
	 * for this queue
	 */
	iq->doorbell_reg = (uint8_t *)lio_dev->hw_addr +
				CN23XX_SLI_IQ_DOORBELL(iq_no);
	iq->inst_cnt_reg = (uint8_t *)lio_dev->hw_addr +
				CN23XX_SLI_IQ_INSTR_COUNT64(iq_no);
	lio_dev_dbg(lio_dev, "InstQ[%d]:dbell reg @ 0x%p instcnt_reg @ 0x%p\n",
		    iq_no, iq->doorbell_reg, iq->inst_cnt_reg);

	/* Store the current instruction counter (used in flush_iq
	 * calculation)
	 */
	pkt_in_done = rte_read64(iq->inst_cnt_reg);

	/* Clear the count by writing back what we read, but don't
	 * enable data traffic here
	 */
	rte_write64(pkt_in_done, iq->inst_cnt_reg);
}

static void
cn23xx_vf_setup_oq_regs(struct lio_device *lio_dev, uint32_t oq_no)
{
	struct lio_droq *droq = lio_dev->droq[oq_no];

	PMD_INIT_FUNC_TRACE();

	lio_write_csr64(lio_dev, CN23XX_SLI_OQ_BASE_ADDR64(oq_no),
			droq->desc_ring_dma);
	lio_write_csr(lio_dev, CN23XX_SLI_OQ_SIZE(oq_no), droq->nb_desc);

	lio_write_csr(lio_dev, CN23XX_SLI_OQ_BUFF_INFO_SIZE(oq_no),
		      (droq->buffer_size | (OCTEON_RH_SIZE << 16)));

	/* Get the mapped address of the pkt_sent and pkts_credit regs */
	droq->pkts_sent_reg = (uint8_t *)lio_dev->hw_addr +
					CN23XX_SLI_OQ_PKTS_SENT(oq_no);
	droq->pkts_credit_reg = (uint8_t *)lio_dev->hw_addr +
					CN23XX_SLI_OQ_PKTS_CREDIT(oq_no);
}

static void
cn23xx_vf_free_mbox(struct lio_device *lio_dev)
{
	PMD_INIT_FUNC_TRACE();

	rte_free(lio_dev->mbox[0]);
	lio_dev->mbox[0] = NULL;

	rte_free(lio_dev->mbox);
	lio_dev->mbox = NULL;
}

static int
cn23xx_vf_setup_mbox(struct lio_device *lio_dev)
{
	struct lio_mbox *mbox;

	PMD_INIT_FUNC_TRACE();

	if (lio_dev->mbox == NULL) {
		lio_dev->mbox = rte_zmalloc(NULL, sizeof(void *), 0);
		if (lio_dev->mbox == NULL)
			return -ENOMEM;
	}

	mbox = rte_zmalloc(NULL, sizeof(struct lio_mbox), 0);
	if (mbox == NULL) {
		rte_free(lio_dev->mbox);
		lio_dev->mbox = NULL;
		return -ENOMEM;
	}

	rte_spinlock_init(&mbox->lock);

	mbox->lio_dev = lio_dev;

	mbox->q_no = 0;

	mbox->state = LIO_MBOX_STATE_IDLE;

	/* VF mbox interrupt reg */
	mbox->mbox_int_reg = (uint8_t *)lio_dev->hw_addr +
				CN23XX_VF_SLI_PKT_MBOX_INT(0);
	/* VF reads from SIG0 reg */
	mbox->mbox_read_reg = (uint8_t *)lio_dev->hw_addr +
				CN23XX_SLI_PKT_PF_VF_MBOX_SIG(0, 0);
	/* VF writes into SIG1 reg */
	mbox->mbox_write_reg = (uint8_t *)lio_dev->hw_addr +
				CN23XX_SLI_PKT_PF_VF_MBOX_SIG(0, 1);

	lio_dev->mbox[0] = mbox;

	rte_write64(LIO_PFVFSIG, mbox->mbox_read_reg);

	return 0;
}

static int
cn23xx_vf_enable_io_queues(struct lio_device *lio_dev)
{
	uint32_t q_no;

	PMD_INIT_FUNC_TRACE();

	for (q_no = 0; q_no < lio_dev->num_iqs; q_no++) {
		uint64_t reg_val;

		/* set the corresponding IQ IS_64B bit */
		if (lio_dev->io_qmask.iq64B & (1ULL << q_no)) {
			reg_val = lio_read_csr64(
					lio_dev,
					CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
			reg_val = reg_val | CN23XX_PKT_INPUT_CTL_IS_64B;
			lio_write_csr64(lio_dev,
					CN23XX_SLI_IQ_PKT_CONTROL64(q_no),
					reg_val);
		}

		/* set the corresponding IQ ENB bit */
		if (lio_dev->io_qmask.iq & (1ULL << q_no)) {
			reg_val = lio_read_csr64(
					lio_dev,
					CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
			reg_val = reg_val | CN23XX_PKT_INPUT_CTL_RING_ENB;
			lio_write_csr64(lio_dev,
					CN23XX_SLI_IQ_PKT_CONTROL64(q_no),
					reg_val);
		}
	}
	for (q_no = 0; q_no < lio_dev->num_oqs; q_no++) {
		uint32_t reg_val;

		/* set the corresponding OQ ENB bit */
		if (lio_dev->io_qmask.oq & (1ULL << q_no)) {
			reg_val = lio_read_csr(
					lio_dev,
					CN23XX_SLI_OQ_PKT_CONTROL(q_no));
			reg_val = reg_val | CN23XX_PKT_OUTPUT_CTL_RING_ENB;
			lio_write_csr(lio_dev,
				      CN23XX_SLI_OQ_PKT_CONTROL(q_no),
				      reg_val);
		}
	}

	return 0;
}

static void
cn23xx_vf_disable_io_queues(struct lio_device *lio_dev)
{
	uint32_t num_queues;

	PMD_INIT_FUNC_TRACE();

	/* per HRM, rings can only be disabled via reset operation,
	 * NOT via SLI_PKT()_INPUT/OUTPUT_CONTROL[ENB]
	 */
	num_queues = lio_dev->num_iqs;
	if (num_queues < lio_dev->num_oqs)
		num_queues = lio_dev->num_oqs;

	cn23xx_vf_reset_io_queues(lio_dev, num_queues);
}

void
cn23xx_vf_ask_pf_to_do_flr(struct lio_device *lio_dev)
{
	struct lio_mbox_cmd mbox_cmd;

	memset(&mbox_cmd, 0, sizeof(struct lio_mbox_cmd));
	mbox_cmd.msg.s.type = LIO_MBOX_REQUEST;
	mbox_cmd.msg.s.resp_needed = 0;
	mbox_cmd.msg.s.cmd = LIO_VF_FLR_REQUEST;
	mbox_cmd.msg.s.len = 1;
	mbox_cmd.q_no = 0;
	mbox_cmd.recv_len = 0;
	mbox_cmd.recv_status = 0;
	mbox_cmd.fn = NULL;
	mbox_cmd.fn_arg = 0;

	lio_mbox_write(lio_dev, &mbox_cmd);
}

static void
cn23xx_pfvf_hs_callback(struct lio_device *lio_dev,
			struct lio_mbox_cmd *cmd, void *arg)
{
	uint32_t major = 0;

	PMD_INIT_FUNC_TRACE();

	rte_memcpy((uint8_t *)&lio_dev->pfvf_hsword, cmd->msg.s.params, 6);
	if (cmd->recv_len > 1) {
		struct lio_version *lio_ver = (struct lio_version *)cmd->data;

		major = lio_ver->major;
		major = major << 16;
	}

	rte_atomic64_set((rte_atomic64_t *)arg, major | 1);
}

int
cn23xx_pfvf_handshake(struct lio_device *lio_dev)
{
	struct lio_mbox_cmd mbox_cmd;
	struct lio_version *lio_ver = (struct lio_version *)&mbox_cmd.data[0];
	uint32_t q_no, count = 0;
	rte_atomic64_t status;
	uint32_t pfmajor;
	uint32_t vfmajor;
	uint32_t ret;

	PMD_INIT_FUNC_TRACE();

	/* Sending VF_ACTIVE indication to the PF driver */
	lio_dev_dbg(lio_dev, "requesting info from PF\n");

	mbox_cmd.msg.mbox_msg64 = 0;
	mbox_cmd.msg.s.type = LIO_MBOX_REQUEST;
	mbox_cmd.msg.s.resp_needed = 1;
	mbox_cmd.msg.s.cmd = LIO_VF_ACTIVE;
	mbox_cmd.msg.s.len = 2;
	mbox_cmd.data[0] = 0;
	lio_ver->major = LIO_BASE_MAJOR_VERSION;
	lio_ver->minor = LIO_BASE_MINOR_VERSION;
	lio_ver->micro = LIO_BASE_MICRO_VERSION;
	mbox_cmd.q_no = 0;
	mbox_cmd.recv_len = 0;
	mbox_cmd.recv_status = 0;
	mbox_cmd.fn = (lio_mbox_callback)cn23xx_pfvf_hs_callback;
	mbox_cmd.fn_arg = (void *)&status;

	if (lio_mbox_write(lio_dev, &mbox_cmd)) {
		lio_dev_err(lio_dev, "Write to mailbox failed\n");
		return -1;
	}

	rte_atomic64_set(&status, 0);

	do {
		rte_delay_ms(1);
	} while ((rte_atomic64_read(&status) == 0) && (count++ < 10000));

	ret = rte_atomic64_read(&status);
	if (ret == 0) {
		lio_dev_err(lio_dev, "cn23xx_pfvf_handshake timeout\n");
		return -1;
	}

	for (q_no = 0; q_no < lio_dev->num_iqs; q_no++)
		lio_dev->instr_queue[q_no]->txpciq.s.pkind =
						lio_dev->pfvf_hsword.pkind;

	vfmajor = LIO_BASE_MAJOR_VERSION;
	pfmajor = ret >> 16;
	if (pfmajor != vfmajor) {
		lio_dev_err(lio_dev,
			    "VF LiquidIO driver (major version %d) is not compatible with LiquidIO PF driver (major version %d)\n",
			    vfmajor, pfmajor);
		ret = -EPERM;
	} else {
		lio_dev_dbg(lio_dev,
			    "VF LiquidIO driver (major version %d), LiquidIO PF driver (major version %d)\n",
			    vfmajor, pfmajor);
		ret = 0;
	}

	lio_dev_dbg(lio_dev, "got data from PF pkind is %d\n",
		    lio_dev->pfvf_hsword.pkind);

	return ret;
}

void
cn23xx_vf_handle_mbox(struct lio_device *lio_dev)
{
	uint64_t mbox_int_val;

	/* read and clear by writing 1 */
	mbox_int_val = rte_read64(lio_dev->mbox[0]->mbox_int_reg);
	rte_write64(mbox_int_val, lio_dev->mbox[0]->mbox_int_reg);
	if (lio_mbox_read(lio_dev->mbox[0]))
		lio_mbox_process_message(lio_dev->mbox[0]);
}

int
cn23xx_vf_setup_device(struct lio_device *lio_dev)
{
	uint64_t reg_val;

	PMD_INIT_FUNC_TRACE();

	/* INPUT_CONTROL[RPVF] gives the VF IOq count */
	reg_val = lio_read_csr64(lio_dev, CN23XX_SLI_IQ_PKT_CONTROL64(0));

	lio_dev->pf_num = (reg_val >> CN23XX_PKT_INPUT_CTL_PF_NUM_POS) &
				CN23XX_PKT_INPUT_CTL_PF_NUM_MASK;
	lio_dev->vf_num = (reg_val >> CN23XX_PKT_INPUT_CTL_VF_NUM_POS) &
				CN23XX_PKT_INPUT_CTL_VF_NUM_MASK;

	reg_val = reg_val >> CN23XX_PKT_INPUT_CTL_RPVF_POS;

	lio_dev->sriov_info.rings_per_vf =
				reg_val & CN23XX_PKT_INPUT_CTL_RPVF_MASK;

	lio_dev->default_config = lio_get_conf(lio_dev);
	if (lio_dev->default_config == NULL)
		return -1;

	lio_dev->fn_list.setup_iq_regs		= cn23xx_vf_setup_iq_regs;
	lio_dev->fn_list.setup_oq_regs		= cn23xx_vf_setup_oq_regs;
	lio_dev->fn_list.setup_mbox		= cn23xx_vf_setup_mbox;
	lio_dev->fn_list.free_mbox		= cn23xx_vf_free_mbox;

	lio_dev->fn_list.setup_device_regs	= cn23xx_vf_setup_device_regs;

	lio_dev->fn_list.enable_io_queues	= cn23xx_vf_enable_io_queues;
	lio_dev->fn_list.disable_io_queues	= cn23xx_vf_disable_io_queues;

	return 0;
}

