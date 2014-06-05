/*******************************************************************************

Copyright (c) 2013 - 2014, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#ifndef _I40E_DCB_H_
#define _I40E_DCB_H_

#include "i40e_type.h"

#define I40E_DCBX_OFFLOAD_DISABLED	0
#define I40E_DCBX_OFFLOAD_ENABLED	1

#define I40E_DCBX_STATUS_NOT_STARTED	0
#define I40E_DCBX_STATUS_IN_PROGRESS	1
#define I40E_DCBX_STATUS_DONE		2
#define I40E_DCBX_STATUS_MULTIPLE_PEERS	3
#define I40E_DCBX_STATUS_DISABLED	7

#define I40E_TLV_TYPE_END		0
#define I40E_TLV_TYPE_ORG		127

#define I40E_IEEE_8021QAZ_OUI		0x0080C2
#define I40E_IEEE_SUBTYPE_ETS_CFG	9
#define I40E_IEEE_SUBTYPE_ETS_REC	10
#define I40E_IEEE_SUBTYPE_PFC_CFG	11
#define I40E_IEEE_SUBTYPE_APP_PRI	12

#define I40E_LLDP_ADMINSTATUS_DISABLED		0
#define I40E_LLDP_ADMINSTATUS_ENABLED_RX	1
#define I40E_LLDP_ADMINSTATUS_ENABLED_TX	2
#define I40E_LLDP_ADMINSTATUS_ENABLED_RXTX	3

/* Defines for LLDP TLV header */
#define I40E_LLDP_MIB_HLEN		14
#define I40E_LLDP_TLV_LEN_SHIFT		0
#define I40E_LLDP_TLV_LEN_MASK		(0x01FF << I40E_LLDP_TLV_LEN_SHIFT)
#define I40E_LLDP_TLV_TYPE_SHIFT	9
#define I40E_LLDP_TLV_TYPE_MASK		(0x7F << I40E_LLDP_TLV_TYPE_SHIFT)
#define I40E_LLDP_TLV_SUBTYPE_SHIFT	0
#define I40E_LLDP_TLV_SUBTYPE_MASK	(0xFF << I40E_LLDP_TLV_SUBTYPE_SHIFT)
#define I40E_LLDP_TLV_OUI_SHIFT		8
#define I40E_LLDP_TLV_OUI_MASK		(0xFFFFFF << I40E_LLDP_TLV_OUI_SHIFT)

/* Defines for IEEE ETS TLV */
#define I40E_IEEE_ETS_MAXTC_SHIFT	0
#define I40E_IEEE_ETS_MAXTC_MASK	(0x7 << I40E_IEEE_ETS_MAXTC_SHIFT)
#define I40E_IEEE_ETS_CBS_SHIFT		6
#define I40E_IEEE_ETS_CBS_MASK		(0x1 << I40E_IEEE_ETS_CBS_SHIFT)
#define I40E_IEEE_ETS_WILLING_SHIFT	7
#define I40E_IEEE_ETS_WILLING_MASK	(0x1 << I40E_IEEE_ETS_WILLING_SHIFT)
#define I40E_IEEE_ETS_PRIO_0_SHIFT	0
#define I40E_IEEE_ETS_PRIO_0_MASK	(0x7 << I40E_IEEE_ETS_PRIO_0_SHIFT)
#define I40E_IEEE_ETS_PRIO_1_SHIFT	4
#define I40E_IEEE_ETS_PRIO_1_MASK	(0x7 << I40E_IEEE_ETS_PRIO_1_SHIFT)

/* Defines for IEEE TSA types */
#define I40E_IEEE_TSA_STRICT		0
#define I40E_IEEE_TSA_CBS		1
#define I40E_IEEE_TSA_ETS		2
#define I40E_IEEE_TSA_VENDOR		255

/* Defines for IEEE PFC TLV */
#define I40E_IEEE_PFC_CAP_SHIFT		0
#define I40E_IEEE_PFC_CAP_MASK		(0xF << I40E_IEEE_PFC_CAP_SHIFT)
#define I40E_IEEE_PFC_MBC_SHIFT		6
#define I40E_IEEE_PFC_MBC_MASK		(0x1 << I40E_IEEE_PFC_MBC_SHIFT)
#define I40E_IEEE_PFC_WILLING_SHIFT	7
#define I40E_IEEE_PFC_WILLING_MASK	(0x1 << I40E_IEEE_PFC_WILLING_SHIFT)

/* Defines for IEEE APP TLV */
#define I40E_IEEE_APP_SEL_SHIFT		0
#define I40E_IEEE_APP_SEL_MASK		(0x7 << I40E_IEEE_APP_SEL_SHIFT)
#define I40E_IEEE_APP_PRIO_SHIFT	5
#define I40E_IEEE_APP_PRIO_MASK		(0x7 << I40E_IEEE_APP_PRIO_SHIFT)


#pragma pack(1)

/* IEEE 802.1AB LLDP TLV structure */
struct i40e_lldp_generic_tlv {
	__be16 typelength;
	u8 tlvinfo[1];
};

/* IEEE 802.1AB LLDP Organization specific TLV */
struct i40e_lldp_org_tlv {
	__be16 typelength;
	__be32 ouisubtype;
	u8 tlvinfo[1];
};
#pragma pack()

/*
 * TODO: The below structures related LLDP/DCBX variables
 * and statistics are defined but need to find how to get
 * the required information from the Firmware to use them
 */

/* IEEE 802.1AB LLDP Agent Statistics */
struct i40e_lldp_stats {
	u64 remtablelastchangetime;
	u64 remtableinserts;
	u64 remtabledeletes;
	u64 remtabledrops;
	u64 remtableageouts;
	u64 txframestotal;
	u64 rxframesdiscarded;
	u64 rxportframeerrors;
	u64 rxportframestotal;
	u64 rxporttlvsdiscardedtotal;
	u64 rxporttlvsunrecognizedtotal;
	u64 remtoomanyneighbors;
};

/* IEEE 802.1Qaz DCBX variables */
struct i40e_dcbx_variables {
	u32 defmaxtrafficclasses;
	u32 defprioritytcmapping;
	u32 deftcbandwidth;
	u32 deftsaassignment;
};

#ifdef I40E_DCB_SW
/* Data structures to pass for SW DCBX */
struct i40e_rx_pb_config {
	u32	shared_pool_size;
	u32	shared_pool_high_wm;
	u32	shared_pool_low_wm;
	u32	shared_pool_high_thresh[I40E_MAX_TRAFFIC_CLASS];
	u32	shared_pool_low_thresh[I40E_MAX_TRAFFIC_CLASS];
	u32	tc_pool_size[I40E_MAX_TRAFFIC_CLASS];
	u32	tc_pool_high_wm[I40E_MAX_TRAFFIC_CLASS];
	u32	tc_pool_low_wm[I40E_MAX_TRAFFIC_CLASS];
};

enum i40e_dcb_arbiter_mode {
	I40E_DCB_ARB_MODE_STRICT_PRIORITY = 0,
	I40E_DCB_ARB_MODE_ROUND_ROBIN = 1
};

#define I40E_DEFAULT_PAUSE_TIME			0xffff
#define I40E_MAX_FRAME_SIZE			4608 /* 4.5 KB */

#define I40E_DEVICE_RPB_SIZE			968000 /* 968 KB */

/* BitTimes (BT) conversion */
#define I40E_BT2KB(BT) ((BT + (8 * 1024 - 1)) / (8 * 1024))
#define I40E_B2BT(BT) (BT * 8)
#define I40E_BT2B(BT) ((BT + (8 - 1)) / (8))

/* Max Frame(TC) = MFS(max) + MFS(TC) */
#define I40E_MAX_FRAME_TC(mfs_max, mfs_tc)	I40E_B2BT(mfs_max + mfs_tc)

/* EEE Tx LPI Exit time in Bit Times */
#define I40E_EEE_TX_LPI_EXIT_TIME		142500

/* PCI Round Trip Time in Bit Times */
#define I40E_PCIRTT_LINK_SPEED_10G		20000
#define I40E_PCIRTT_BYTE_LINK_SPEED_20G		40000
#define I40E_PCIRTT_BYTE_LINK_SPEED_40G		80000

/* PFC Frame Delay Bit Times */
#define I40E_PFC_FRAME_DELAY			672

/* Worst case Cable (10GBase-T) Delay Bit Times */
#define I40E_CABLE_DELAY			5556

/* Higher Layer Delay @10G Bit Times */
#define I40E_HIGHER_LAYER_DELAY_10G		6144

/* Interface Delays in Bit Times */
/* TODO: Add for other link speeds 20G/40G/etc. */
#define I40E_INTERFACE_DELAY_10G_MAC_CONTROL	8192
#define I40E_INTERFACE_DELAY_10G_MAC		8192
#define I40E_INTERFACE_DELAY_10G_RS		8192

#define I40E_INTERFACE_DELAY_XGXS		2048
#define I40E_INTERFACE_DELAY_XAUI		2048

#define I40E_INTERFACE_DELAY_10G_BASEX_PCS	2048
#define I40E_INTERFACE_DELAY_10G_BASER_PCS	3584
#define I40E_INTERFACE_DELAY_LX4_PMD		512
#define I40E_INTERFACE_DELAY_CX4_PMD		512
#define I40E_INTERFACE_DELAY_SERIAL_PMA		512
#define I40E_INTERFACE_DELAY_PMD		512

#define I40E_INTERFACE_DELAY_10G_BASET		25600

/* delay values for with 10G BaseT in Bit Times */
#define I40E_INTERFACE_DELAY_10G_COPPER	\
	(I40E_INTERFACE_DELAY_10G_MAC + (2 * I40E_INTERFACE_DELAY_XAUI) \
	 + I40E_INTERFACE_DELAY_10G_BASET)
#define I40E_DV_TC(mfs_max, mfs_tc) \
		((2 * I40E_MAX_FRAME_TC(mfs_max, mfs_tc)) \
		  + I40E_PFC_FRAME_DELAY \
		  + (2 * I40E_CABLE_DELAY) \
		  + (2 * I40E_INTERFACE_DELAY_10G_COPPER) \
		  + I40E_HIGHER_LAYER_DELAY_10G)
#define I40E_STD_DV_TC(mfs_max, mfs_tc) \
		(I40E_DV_TC(mfs_max, mfs_tc) + I40E_B2BT(mfs_max))

enum i40e_status_code i40e_process_lldp_event(struct i40e_hw *hw,
					      struct i40e_arq_event_info *e);
/* APIs for SW DCBX */
void i40e_dcb_hw_rx_fifo_config(struct i40e_hw *hw,
				enum i40e_dcb_arbiter_mode ets_mode,
				enum i40e_dcb_arbiter_mode non_ets_mode,
				u32 max_exponent, u8 lltc_map);
void i40e_dcb_hw_rx_cmd_monitor_config(struct i40e_hw *hw,
				       u8 num_tc, u8 num_ports);
void i40e_dcb_hw_pfc_config(struct i40e_hw *hw,
			    u8 pfc_en, u8 *prio_tc);
void i40e_dcb_hw_set_num_tc(struct i40e_hw *hw, u8 num_tc);
u8 i40e_dcb_hw_get_num_tc(struct i40e_hw *hw);
void i40e_dcb_hw_rx_ets_bw_config(struct i40e_hw *hw, u8 *bw_share,
				  u8 *mode, u8 *prio_type);
void i40e_dcb_hw_rx_up2tc_config(struct i40e_hw *hw, u8 *prio_tc);
void i40e_dcb_hw_calculate_pool_sizes(struct i40e_hw *hw,
				      u8 num_ports, bool eee_enabled,
				      u8 pfc_en, u32 *mfs_tc,
				      struct i40e_rx_pb_config *pb_cfg);
void i40e_dcb_hw_rx_pb_config(struct i40e_hw *hw,
			      struct i40e_rx_pb_config *old_pb_cfg,
			      struct i40e_rx_pb_config *new_pb_cfg);
#endif /* I40E_DCB_SW */
enum i40e_status_code i40e_get_dcbx_status(struct i40e_hw *hw,
					   u16 *status);
enum i40e_status_code i40e_lldp_to_dcb_config(u8 *lldpmib,
					      struct i40e_dcbx_config *dcbcfg);
enum i40e_status_code i40e_aq_get_dcb_config(struct i40e_hw *hw, u8 mib_type,
					     u8 bridgetype,
					     struct i40e_dcbx_config *dcbcfg);
enum i40e_status_code i40e_get_dcb_config(struct i40e_hw *hw);
enum i40e_status_code i40e_init_dcb(struct i40e_hw *hw);
#endif /* _I40E_DCB_H_ */
