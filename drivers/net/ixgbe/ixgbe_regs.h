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
#ifndef _IXGBE_REGS_H_
#define _IXGBE_REGS_H_

#include "ixgbe_ethdev.h"

struct ixgbe_hw;
struct reg_info {
	uint32_t base_addr;
	uint32_t count;
	uint32_t stride;
	const char *name;
} reg_info;

static const struct reg_info ixgbe_regs_general[] = {
	{IXGBE_CTRL, 1, 1, "IXGBE_CTRL"},
	{IXGBE_STATUS, 1, 1, "IXGBE_STATUS"},
	{IXGBE_CTRL_EXT, 1, 1, "IXGBE_CTRL_EXT"},
	{IXGBE_ESDP, 1, 1, "IXGBE_ESDP"},
	{IXGBE_EODSDP, 1, 1, "IXGBE_EODSDP"},
	{IXGBE_LEDCTL, 1, 1, "IXGBE_LEDCTL"},
	{IXGBE_FRTIMER, 1, 1, "IXGBE_FRTIMER"},
	{IXGBE_TCPTIMER, 1, 1, "IXGBE_TCPTIMER"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbevf_regs_general[] = {
	{IXGBE_CTRL, 1, 1, "IXGBE_CTRL"},
	{IXGBE_STATUS, 1, 1, "IXGBE_STATUS"},
	{IXGBE_VFLINKS, 1, 1, "IXGBE_VFLINKS"},
	{IXGBE_FRTIMER, 1, 1, "IXGBE_FRTIMER"},
	{IXGBE_VFMAILBOX, 1, 1, "IXGBE_VFMAILBOX"},
	{IXGBE_VFMBMEM, 16, 4, "IXGBE_VFMBMEM"},
	{IXGBE_VFRXMEMWRAP, 1, 1, "IXGBE_VFRXMEMWRAP"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_nvm[] = {
	{IXGBE_EEC, 1, 1, "IXGBE_EEC"},
	{IXGBE_EERD, 1, 1, "IXGBE_EERD"},
	{IXGBE_FLA, 1, 1, "IXGBE_FLA"},
	{IXGBE_EEMNGCTL, 1, 1, "IXGBE_EEMNGCTL"},
	{IXGBE_EEMNGDATA, 1, 1, "IXGBE_EEMNGDATA"},
	{IXGBE_FLMNGCTL, 1, 1, "IXGBE_FLMNGCTL"},
	{IXGBE_FLMNGDATA, 1, 1, "IXGBE_FLMNGDATA"},
	{IXGBE_FLMNGCNT, 1, 1, "IXGBE_FLMNGCNT"},
	{IXGBE_FLOP, 1, 1, "IXGBE_FLOP"},
	{IXGBE_GRC,  1, 1, "IXGBE_GRC"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_interrupt[] = {
	{IXGBE_EICS, 1, 1, "IXGBE_EICS"},
	{IXGBE_EIMS, 1, 1, "IXGBE_EIMS"},
	{IXGBE_EIMC, 1, 1, "IXGBE_EIMC"},
	{IXGBE_EIAC, 1, 1, "IXGBE_EIAC"},
	{IXGBE_EIAM, 1, 1, "IXGBE_EIAM"},
	{IXGBE_EITR(0), 24, 4, "IXGBE_EITR"},
	{IXGBE_IVAR(0), 24, 4, "IXGBE_IVAR"},
	{IXGBE_MSIXT, 1, 1, "IXGBE_MSIXT"},
	{IXGBE_MSIXPBA, 1, 1, "IXGBE_MSIXPBA"},
	{IXGBE_PBACL(0),  1, 4, "IXGBE_PBACL"},
	{IXGBE_GPIE, 1, 1, ""},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbevf_regs_interrupt[] = {
	{IXGBE_VTEICR, 1, 1, "IXGBE_VTEICR"},
	{IXGBE_VTEICS, 1, 1, "IXGBE_VTEICS"},
	{IXGBE_VTEIMS, 1, 1, "IXGBE_VTEIMS"},
	{IXGBE_VTEIMC, 1, 1, "IXGBE_VTEIMC"},
	{IXGBE_VTEIAM, 1, 1, "IXGBE_VTEIAM"},
	{IXGBE_VTEITR(0), 2, 4, "IXGBE_VTEITR"},
	{IXGBE_VTIVAR(0), 4, 4, "IXGBE_VTIVAR"},
	{IXGBE_VTIVAR_MISC, 1, 1, "IXGBE_VTIVAR_MISC"},
	{IXGBE_VTRSCINT(0), 2, 4, "IXGBE_VTRSCINT"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_fctl_mac_82598EB[] = {
	{IXGBE_PFCTOP, 1, 1, ""},
	{IXGBE_FCTTV(0), 4, 4, ""},
	{IXGBE_FCRTV, 1, 1, ""},
	{IXGBE_TFCS, 1, 1, ""},
	{IXGBE_FCRTL(0), 8, 8, "IXGBE_FCRTL"},
	{IXGBE_FCRTH(0), 8, 8, "IXGBE_FCRTH"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_fctl_others[] = {
	{IXGBE_PFCTOP, 1, 1, ""},
	{IXGBE_FCTTV(0), 4, 4, ""},
	{IXGBE_FCRTV, 1, 1, ""},
	{IXGBE_TFCS, 1, 1, ""},
	{IXGBE_FCRTL_82599(0), 8, 4, "IXGBE_FCRTL"},
	{IXGBE_FCRTH_82599(0), 8, 4, "IXGBE_FCRTH"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_rxdma[] = {
	{IXGBE_RDBAL(0), 64, 0x40, "IXGBE_RDBAL"},
	{IXGBE_RDBAH(0), 64, 0x40, "IXGBE_RDBAH"},
	{IXGBE_RDLEN(0), 64, 0x40, "IXGBE_RDLEN"},
	{IXGBE_RDH(0), 64, 0x40, "IXGBE_RDH"},
	{IXGBE_RDT(0), 64, 0x40, "IXGBE_RDT"},
	{IXGBE_RXDCTL(0), 64, 0x40, "IXGBE_RXDCTL"},
	{IXGBE_SRRCTL(0), 16, 0x4, "IXGBE_SRRCTL"},
	{IXGBE_DCA_RXCTRL(0), 16, 4, "IXGBE_DCA_RXCTRL"},
	{IXGBE_RDRXCTL, 1, 1, "IXGBE_RDRXCTL"},
	{IXGBE_RXPBSIZE(0), 8, 4, "IXGBE_RXPBSIZE"},
	{IXGBE_RXCTRL, 1, 1, "IXGBE_RXCTRL"},
	{IXGBE_DROPEN, 1, 1, "IXGBE_DROPEN"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbevf_regs_rxdma[] = {
	{IXGBE_RDBAL(0), 8, 0x40, "IXGBE_RDBAL"},
	{IXGBE_RDBAH(0), 8, 0x40, "IXGBE_RDBAH"},
	{IXGBE_RDLEN(0), 8, 0x40, "IXGBE_RDLEN"},
	{IXGBE_RDH(0), 8, 0x40, "IXGBE_RDH"},
	{IXGBE_RDT(0), 8, 0x40, "IXGBE_RDT"},
	{IXGBE_RXDCTL(0), 8, 0x40, "IXGBE_RXDCTL"},
	{IXGBE_SRRCTL(0), 8, 0x40, "IXGBE_SRRCTL"},
	{IXGBE_VFPSRTYPE, 1, 1,	"IXGBE_VFPSRTYPE"},
	{IXGBE_VFRSCCTL(0), 8, 0x40, "IXGBE_VFRSCCTL"},
	{IXGBE_PVFDCA_RXCTRL(0), 8, 0x40, "IXGBE_PVFDCA_RXCTRL"},
	{IXGBE_PVFDCA_TXCTRL(0), 8, 0x40, "IXGBE_PVFDCA_TXCTRL"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_rx[] = {
	{IXGBE_RXCSUM, 1, 1, "IXGBE_RXCSUM"},
	{IXGBE_RFCTL, 1, 1, "IXGBE_RFCTL"},
	{IXGBE_RAL(0), 16, 8, "IXGBE_RAL"},
	{IXGBE_RAH(0), 16, 8, "IXGBE_RAH"},
	{IXGBE_PSRTYPE(0), 1, 4, "IXGBE_PSRTYPE"},
	{IXGBE_FCTRL, 1, 1, "IXGBE_FCTRL"},
	{IXGBE_VLNCTRL, 1, 1, "IXGBE_VLNCTRL"},
	{IXGBE_MCSTCTRL, 1, 1, "IXGBE_MCSTCTRL"},
	{IXGBE_MRQC, 1, 1, "IXGBE_MRQC"},
	{IXGBE_VMD_CTL, 1, 1, "IXGBE_VMD_CTL"},
	{IXGBE_IMIR(0), 8, 4, "IXGBE_IMIR"},
	{IXGBE_IMIREXT(0), 8, 4, "IXGBE_IMIREXT"},
	{IXGBE_IMIRVP, 1, 1, "IXGBE_IMIRVP"},
	{0, 0, 0, ""}
};

static struct reg_info ixgbe_regs_tx[] = {
	{IXGBE_TDBAL(0), 32, 0x40, "IXGBE_TDBAL"},
	{IXGBE_TDBAH(0), 32, 0x40, "IXGBE_TDBAH"},
	{IXGBE_TDLEN(0), 32, 0x40, "IXGBE_TDLEN"},
	{IXGBE_TDH(0), 32, 0x40, "IXGBE_TDH"},
	{IXGBE_TDT(0), 32, 0x40, "IXGBE_TDT"},
	{IXGBE_TXDCTL(0), 32, 0x40, "IXGBE_TXDCTL"},
	{IXGBE_TDWBAL(0), 32, 0x40, "IXGBE_TDWBAL"},
	{IXGBE_TDWBAH(0), 32, 0x40, "IXGBE_TDWBAH"},
	{IXGBE_DTXCTL, 1, 1, "IXGBE_DTXCTL"},
	{IXGBE_DCA_TXCTRL(0), 16, 4, "IXGBE_DCA_TXCTRL"},
	{IXGBE_TXPBSIZE(0), 8, 4, "IXGBE_TXPBSIZE"},
	{IXGBE_MNGTXMAP, 1, 1, "IXGBE_MNGTXMAP"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbevf_regs_tx[] = {
	{IXGBE_TDBAL(0), 4, 0x40, "IXGBE_TDBAL"},
	{IXGBE_TDBAH(0), 4, 0x40, "IXGBE_TDBAH"},
	{IXGBE_TDLEN(0), 4, 0x40, "IXGBE_TDLEN"},
	{IXGBE_TDH(0), 4, 0x40, "IXGBE_TDH"},
	{IXGBE_TDT(0), 4, 0x40, "IXGBE_TDT"},
	{IXGBE_TXDCTL(0), 4, 0x40, "IXGBE_TXDCTL"},
	{IXGBE_TDWBAL(0), 4, 0x40, "IXGBE_TDWBAL"},
	{IXGBE_TDWBAH(0), 4, 0x40, "IXGBE_TDWBAH"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_wakeup[] = {
	{IXGBE_WUC, 1, 1, "IXGBE_WUC"},
	{IXGBE_WUFC, 1, 1, "IXGBE_WUFC"},
	{IXGBE_WUS, 1, 1, "IXGBE_WUS"},
	{IXGBE_IPAV, 1, 1, "IXGBE_IPAV"},
	{IXGBE_IP4AT, 1, 1, "IXGBE_IP4AT"},
	{IXGBE_IP6AT, 1, 1, "IXGBE_IP6AT"},
	{IXGBE_WUPL, 1, 1, "IXGBE_WUPL"},
	{IXGBE_WUPM, 1, 1, "IXGBE_WUPM"},
	{IXGBE_FHFT(0), 1, 1, "IXGBE_FHFT"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_dcb[] = {
	{IXGBE_RMCS, 1, 1, "IXGBE_RMCS"},
	{IXGBE_DPMCS, 1, 1, "IXGBE_DPMCS"},
	{IXGBE_PDPMCS, 1, 1, "IXGBE_PDPMCS"},
	{IXGBE_RUPPBMR, 1, 1, "IXGBE_RUPPBMR"},
	{IXGBE_RT2CR(0), 8, 4, "IXGBE_RT2CR"},
	{IXGBE_RT2SR(0), 8, 4, "IXGBE_RT2SR"},
	{IXGBE_TDTQ2TCCR(0), 8, 0x40, "IXGBE_TDTQ2TCCR"},
	{IXGBE_TDTQ2TCSR(0), 8, 0x40, "IXGBE_TDTQ2TCSR"},
	{IXGBE_TDPT2TCCR(0), 8, 4, "IXGBE_TDPT2TCCR"},
	{IXGBE_TDPT2TCSR(0), 8, 4, "IXGBE_TDPT2TCSR"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_mac[] = {
	{IXGBE_PCS1GCFIG, 1, 1, "IXGBE_PCS1GCFIG"},
	{IXGBE_PCS1GLCTL, 1, 1, "IXGBE_PCS1GLCTL"},
	{IXGBE_PCS1GLSTA, 1, 1, "IXGBE_PCS1GLSTA"},
	{IXGBE_PCS1GDBG0, 1, 1, "IXGBE_PCS1GDBG0"},
	{IXGBE_PCS1GDBG1, 1, 1, "IXGBE_PCS1GDBG1"},
	{IXGBE_PCS1GANA, 1, 1, "IXGBE_PCS1GANA"},
	{IXGBE_PCS1GANLP, 1, 1, "IXGBE_PCS1GANLP"},
	{IXGBE_PCS1GANNP, 1, 1, "IXGBE_PCS1GANNP"},
	{IXGBE_PCS1GANLPNP, 1, 1, "IXGBE_PCS1GANLPNP"},
	{IXGBE_HLREG0, 1, 1, "IXGBE_HLREG0"},
	{IXGBE_HLREG1, 1, 1, "IXGBE_HLREG1"},
	{IXGBE_PAP, 1, 1, "IXGBE_PAP"},
	{IXGBE_MACA, 1, 1, "IXGBE_MACA"},
	{IXGBE_APAE, 1, 1, "IXGBE_APAE"},
	{IXGBE_ARD, 1, 1, "IXGBE_ARD"},
	{IXGBE_AIS, 1, 1, "IXGBE_AIS"},
	{IXGBE_MSCA, 1, 1, "IXGBE_MSCA"},
	{IXGBE_MSRWD, 1, 1, "IXGBE_MSRWD"},
	{IXGBE_MLADD, 1, 1, "IXGBE_MLADD"},
	{IXGBE_MHADD, 1, 1, "IXGBE_MHADD"},
	{IXGBE_TREG, 1, 1, "IXGBE_TREG"},
	{IXGBE_PCSS1, 1, 1, "IXGBE_PCSS1"},
	{IXGBE_PCSS2, 1, 1, "IXGBE_PCSS2"},
	{IXGBE_XPCSS, 1, 1, "IXGBE_XPCSS"},
	{IXGBE_SERDESC, 1, 1, "IXGBE_SERDESC"},
	{IXGBE_MACS, 1, 1, "IXGBE_MACS"},
	{IXGBE_AUTOC, 1, 1, "IXGBE_AUTOC"},
	{IXGBE_LINKS, 1, 1, "IXGBE_LINKS"},
	{IXGBE_AUTOC2, 1, 1, "IXGBE_AUTOC2"},
	{IXGBE_AUTOC3, 1, 1, "IXGBE_AUTOC3"},
	{IXGBE_ANLP1, 1, 1, "IXGBE_ANLP1"},
	{IXGBE_ANLP2, 1, 1, "IXGBE_ANLP2"},
	{IXGBE_ATLASCTL, 1, 1, "IXGBE_ATLASCTL"},
	{0, 0, 0, ""}
};

static const struct reg_info ixgbe_regs_diagnostic[] = {
	{IXGBE_RDSTATCTL, 1, 1, "IXGBE_RDSTATCTL"},
	{IXGBE_RDSTAT(0), 8, 4, "IXGBE_RDSTAT"},
	{IXGBE_RDHMPN, 1, 1, "IXGBE_RDHMPN"},
	{IXGBE_RIC_DW(0), 4, 4, "IXGBE_RIC_DW"},
	{IXGBE_RDPROBE, 1, 1, "IXGBE_RDPROBE"},
	{IXGBE_TDHMPN, 1, 1, "IXGBE_TDHMPN"},
	{IXGBE_TIC_DW(0), 4, 4, "IXGBE_TIC_DW"},
	{IXGBE_TDPROBE, 1, 1, "IXGBE_TDPROBE"},
	{IXGBE_TXBUFCTRL, 1, 1, "IXGBE_TXBUFCTRL"},
	{IXGBE_TXBUFDATA0, 1, 1, "IXGBE_TXBUFDATA0"},
	{IXGBE_TXBUFDATA1, 1, 1, "IXGBE_TXBUFDATA1"},
	{IXGBE_TXBUFDATA2, 1, 1, "IXGBE_TXBUFDATA2"},
	{IXGBE_TXBUFDATA3, 1, 1, "IXGBE_TXBUFDATA3"},
	{IXGBE_RXBUFCTRL, 1, 1, "IXGBE_RXBUFCTRL"},
	{IXGBE_RXBUFDATA0, 1, 1, "IXGBE_RXBUFDATA0"},
	{IXGBE_RXBUFDATA1, 1, 1, "IXGBE_RXBUFDATA1"},
	{IXGBE_RXBUFDATA2, 1, 1, "IXGBE_RXBUFDATA2"},
	{IXGBE_RXBUFDATA3, 1, 1, "IXGBE_RXBUFDATA3"},
	{IXGBE_PCIE_DIAG(0), 8, 4, ""},
	{IXGBE_RFVAL, 1, 1, "IXGBE_RFVAL"},
	{IXGBE_MDFTC1, 1, 1, "IXGBE_MDFTC1"},
	{IXGBE_MDFTC2, 1, 1, "IXGBE_MDFTC2"},
	{IXGBE_MDFTFIFO1, 1, 1, "IXGBE_MDFTFIFO1"},
	{IXGBE_MDFTFIFO2, 1, 1, "IXGBE_MDFTFIFO2"},
	{IXGBE_MDFTS, 1, 1, "IXGBE_MDFTS"},
	{IXGBE_PCIEECCCTL, 1, 1, "IXGBE_PCIEECCCTL"},
	{IXGBE_PBTXECC, 1, 1, "IXGBE_PBTXECC"},
	{IXGBE_PBRXECC, 1, 1, "IXGBE_PBRXECC"},
	{IXGBE_MFLCN, 1, 1, "IXGBE_MFLCN"},
	{0, 0, 0, ""},
};

/* PF registers */
static const struct reg_info *ixgbe_regs_others[] = {
				ixgbe_regs_general,
				ixgbe_regs_nvm, ixgbe_regs_interrupt,
				ixgbe_regs_fctl_others,
				ixgbe_regs_rxdma,
				ixgbe_regs_rx,
				ixgbe_regs_tx,
				ixgbe_regs_wakeup,
				ixgbe_regs_dcb,
				ixgbe_regs_mac,
				ixgbe_regs_diagnostic,
				NULL};

static const struct reg_info *ixgbe_regs_mac_82598EB[] = {
				ixgbe_regs_general,
				ixgbe_regs_nvm,
				ixgbe_regs_interrupt,
				ixgbe_regs_fctl_mac_82598EB,
				ixgbe_regs_rxdma,
				ixgbe_regs_rx,
				ixgbe_regs_tx,
				ixgbe_regs_wakeup,
				ixgbe_regs_dcb,
				ixgbe_regs_mac,
				ixgbe_regs_diagnostic,
				NULL};

/* VF registers */
static const struct reg_info *ixgbevf_regs[] = {
				ixgbevf_regs_general,
				ixgbevf_regs_interrupt,
				ixgbevf_regs_rxdma,
				ixgbevf_regs_tx,
				NULL};

static inline int
ixgbe_read_regs(struct ixgbe_hw *hw, const struct reg_info *reg,
	uint32_t *reg_buf)
{
	unsigned int i;

	for (i = 0; i < reg->count; i++)
		reg_buf[i] = IXGBE_READ_REG(hw,
					reg->base_addr + i * reg->stride);
	return reg->count;
};

static inline int
ixgbe_regs_group_count(const struct reg_info *regs)
{
	int count = 0;
	int i = 0;

	while (regs[i].count)
		count += regs[i++].count;
	return count;
};

static inline int
ixgbe_read_regs_group(struct rte_eth_dev *dev, uint32_t *reg_buf,
					  const struct reg_info *regs)
{
	int count = 0;
	int i = 0;
	struct ixgbe_hw *hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	while (regs[i].count)
		count += ixgbe_read_regs(hw, &regs[i++], &reg_buf[count]);
	return count;
};

#endif /* _IXGBE_REGS_H_ */
