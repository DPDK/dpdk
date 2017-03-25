/*
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Cavium, Inc.. All rights reserved.
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
 *     * Neither the name of Cavium, Inc. nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER(S) OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_ethdev.h>
#include <rte_cycles.h>
#include <rte_malloc.h>

#include "lio_logs.h"
#include "lio_23xx_vf.h"
#include "lio_23xx_reg.h"

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

	lio_dev->fn_list.setup_device_regs	= cn23xx_vf_setup_device_regs;

	return 0;
}

int
cn23xx_vf_set_io_queues_off(struct lio_device *lio_dev)
{
	uint32_t loop = CN23XX_VF_BUSY_READING_REG_LOOP_COUNT;
	uint64_t q_no;

	/* Disable the i/p and o/p queues for this Octeon.
	 * IOQs will already be in reset.
	 * If RST bit is set, wait for Quiet bit to be set
	 * Once Quiet bit is set, clear the RST bit
	 */
	PMD_INIT_FUNC_TRACE();

	for (q_no = 0; q_no < lio_dev->sriov_info.rings_per_vf; q_no++) {
		volatile uint64_t reg_val;

		reg_val = lio_read_csr64(lio_dev,
					 CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
		while ((reg_val & CN23XX_PKT_INPUT_CTL_RST) && !(reg_val &
					 CN23XX_PKT_INPUT_CTL_QUIET) && loop) {
			reg_val = lio_read_csr64(
					lio_dev,
					CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
			loop = loop - 1;
		}

		if (loop == 0) {
			lio_dev_err(lio_dev,
				    "clearing the reset reg failed or setting the quiet reg failed for qno %lu\n",
				    (unsigned long)q_no);
			return -1;
		}

		reg_val = reg_val & ~CN23XX_PKT_INPUT_CTL_RST;
		lio_write_csr64(lio_dev, CN23XX_SLI_IQ_PKT_CONTROL64(q_no),
				reg_val);

		reg_val = lio_read_csr64(lio_dev,
					 CN23XX_SLI_IQ_PKT_CONTROL64(q_no));
		if (reg_val & CN23XX_PKT_INPUT_CTL_RST) {
			lio_dev_err(lio_dev, "unable to reset qno %lu\n",
				    (unsigned long)q_no);
			return -1;
		}
	}

	return 0;
}
