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
