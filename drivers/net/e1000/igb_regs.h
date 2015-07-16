/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2015 Intel Corporation. All rights reserved.
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
#ifndef _IGB_REGS_H_
#define _IGB_REGS_H_

#include "e1000_ethdev.h"

struct reg_info {
	uint32_t base_addr;
	uint32_t count;
	uint32_t stride;
	const char *name;
};

static const struct reg_info igb_regs_general[] = {
	{E1000_CTRL, 1, 1, "E1000_CTRL"},
	{E1000_STATUS, 1, 1, "E1000_STATUS"},
	{E1000_CTRL_EXT, 1, 1, "E1000_CTRL_EXT"},
	{E1000_MDIC, 1, 1, "E1000_MDIC"},
	{E1000_SCTL, 1, 1, "E1000_SCTL"},
	{E1000_CONNSW, 1, 1, "E1000_CONNSW"},
	{E1000_VET, 1, 1, "E1000_VET"},
	{E1000_LEDCTL, 1, 1, "E1000_LEDCTL"},
	{E1000_PBA, 1, 1, "E1000_PBA"},
	{E1000_PBS, 1, 1, "E1000_PBS"},
	{E1000_FRTIMER, 1, 1, "E1000_FRTIMER"},
	{E1000_TCPTIMER, 1, 1, "E1000_TCPTIMER"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_nvm[] = {
	{E1000_EECD, 1, 1, "E1000_EECD"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_interrupt[] = {
	{E1000_EICS, 1, 1, "E1000_EICS"},
	{E1000_EIMS, 1, 1, "E1000_EIMS"},
	{E1000_EIMC, 1, 1, "E1000_EIMC"},
	{E1000_EIAC, 1, 1, "E1000_EIAC"},
	{E1000_EIAM, 1, 1, "E1000_EIAM"},
	{E1000_ICS, 1, 1, "E1000_ICS"},
	{E1000_IMS, 1, 1, "E1000_IMS"},
	{E1000_IMC, 1, 1, "E1000_IMC"},
	{E1000_IAC, 1, 1, "E1000_IAC"},
	{E1000_IAM,  1, 1, "E1000_IAM"},
	{E1000_IMIRVP, 1, 1, "E1000_IMIRVP"},
	{E1000_EITR(0), 10, 4, "E1000_EITR"},
	{E1000_IMIR(0), 8, 4, "E1000_IMIR"},
	{E1000_IMIREXT(0), 8, 4, "E1000_IMIREXT"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_fctl[] = {
	{E1000_FCAL, 1, 1, "E1000_FCAL"},
	{E1000_FCAH, 1, 1, "E1000_FCAH"},
	{E1000_FCTTV, 1, 1, "E1000_FCTTV"},
	{E1000_FCRTL, 1, 1, "E1000_FCRTL"},
	{E1000_FCRTH, 1, 1, "E1000_FCRTH"},
	{E1000_FCRTV, 1, 1, "E1000_FCRTV"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_rxdma[] = {
	{E1000_RDBAL(0), 4, 0x100, "E1000_RDBAL"},
	{E1000_RDBAH(0), 4, 0x100, "E1000_RDBAH"},
	{E1000_RDLEN(0), 4, 0x100, "E1000_RDLEN"},
	{E1000_RDH(0), 4, 0x100, "E1000_RDH"},
	{E1000_RDT(0), 4, 0x100, "E1000_RDT"},
	{E1000_RXCTL(0), 4, 0x100, "E1000_RXCTL"},
	{E1000_SRRCTL(0), 4, 0x100, "E1000_SRRCTL"},
	{E1000_DCA_RXCTRL(0), 4, 0x100, "E1000_DCA_RXCTRL"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_rx[] = {
	{E1000_RCTL, 1, 1, "E1000_RCTL"},
	{E1000_RXCSUM, 1, 1, "E1000_RXCSUM"},
	{E1000_RLPML, 1, 1, "E1000_RLPML"},
	{E1000_RFCTL, 1, 1, "E1000_RFCTL"},
	{E1000_MRQC, 1, 1, "E1000_MRQC"},
	{E1000_VT_CTL, 1, 1, "E1000_VT_CTL"},
	{E1000_RAL(0), 16, 8, "E1000_RAL"},
	{E1000_RAH(0), 16, 8, "E1000_RAH"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_tx[] = {
	{E1000_TCTL, 1, 1, "E1000_TCTL"},
	{E1000_TCTL_EXT, 1, 1, "E1000_TCTL_EXT"},
	{E1000_TIPG, 1, 1, "E1000_TIPG"},
	{E1000_DTXCTL, 1, 1, "E1000_DTXCTL"},
	{E1000_TDBAL(0), 4, 0x100, "E1000_TDBAL"},
	{E1000_TDBAH(0), 4, 0x100, "E1000_TDBAH"},
	{E1000_TDLEN(0), 4, 0x100, "E1000_TDLEN"},
	{E1000_TDH(0), 4, 0x100, "E1000_TDLEN"},
	{E1000_TDT(0), 4, 0x100, "E1000_TDT"},
	{E1000_TXDCTL(0), 4, 0x100, "E1000_TXDCTL"},
	{E1000_TDWBAL(0), 4, 0x100, "E1000_TDWBAL"},
	{E1000_TDWBAH(0), 4, 0x100, "E1000_TDWBAH"},
	{E1000_DCA_TXCTRL(0), 4, 0x100, "E1000_DCA_TXCTRL"},
	{E1000_TDFH, 1, 1, "E1000_TDFH"},
	{E1000_TDFT, 1, 1, "E1000_TDFT"},
	{E1000_TDFHS, 1, 1, "E1000_TDFHS"},
	{E1000_TDFPC, 1, 1, "E1000_TDFPC"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_wakeup[] = {
	{E1000_WUC, 1, 1, "E1000_WUC"},
	{E1000_WUFC, 1, 1, "E1000_WUFC"},
	{E1000_WUS, 1, 1, "E1000_WUS"},
	{E1000_IPAV, 1, 1, "E1000_IPAV"},
	{E1000_WUPL, 1, 1, "E1000_WUPL"},
	{E1000_IP4AT_REG(0), 4, 8, "E1000_IP4AT_REG"},
	{E1000_IP6AT_REG(0), 4, 4, "E1000_IP6AT_REG"},
	{E1000_WUPM_REG(0), 4, 4, "E1000_WUPM_REG"},
	{E1000_FFMT_REG(0), 4, 8, "E1000_FFMT_REG"},
	{E1000_FFVT_REG(0), 4, 8, "E1000_FFVT_REG"},
	{E1000_FFLT_REG(0), 4, 8, "E1000_FFLT_REG"},
	{0, 0, 0, ""}
};

static const struct reg_info igb_regs_mac[] = {
	{E1000_PCS_CFG0, 1, 1, "E1000_PCS_CFG0"},
	{E1000_PCS_LCTL, 1, 1, "E1000_PCS_LCTL"},
	{E1000_PCS_LSTAT, 1, 1, "E1000_PCS_LSTAT"},
	{E1000_PCS_ANADV, 1, 1, "E1000_PCS_ANADV"},
	{E1000_PCS_LPAB, 1, 1, "E1000_PCS_LPAB"},
	{E1000_PCS_NPTX, 1, 1, "E1000_PCS_NPTX"},
	{E1000_PCS_LPABNP, 1, 1, "E1000_PCS_LPABNP"},
	{0, 0, 0, ""}
};

static const struct reg_info *igb_regs[] = {
				igb_regs_general,
				igb_regs_nvm,
				igb_regs_interrupt,
				igb_regs_fctl,
				igb_regs_rxdma,
				igb_regs_rx,
				igb_regs_tx,
				igb_regs_wakeup,
				igb_regs_mac,
				NULL};

/* FIXME: reading igb_regs_interrupt results side-effect which doesn't
 * work with VFIO; re-install igb_regs_interrupt once issue is resolved.
 */
static const struct reg_info *igbvf_regs[] = {
				igb_regs_general,
				igb_regs_rxdma,
				igb_regs_tx,
				NULL};

static inline int
igb_read_regs(struct e1000_hw *hw, const struct reg_info *reg,
	uint32_t *reg_buf)
{
	unsigned int i;

	for (i = 0; i < reg->count; i++) {
		reg_buf[i] = E1000_READ_REG(hw,
				reg->base_addr + i * reg->stride);
	}
	return reg->count;
};

static inline int
igb_reg_group_count(const struct reg_info *regs)
{
	int count = 0;
	int i = 0;

	while (regs[i].count)
		count += regs[i++].count;
	return count;
};

static inline int
igb_read_regs_group(struct rte_eth_dev *dev, uint32_t *reg_buf,
		const struct reg_info *regs)
{
	int count = 0;
	int i = 0;
	struct e1000_hw *hw = E1000_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	while (regs[i].count)
		count += igb_read_regs(hw, &regs[i++], &reg_buf[count]);
	return count;
};

#endif /* _IGB_REGS_H_ */
