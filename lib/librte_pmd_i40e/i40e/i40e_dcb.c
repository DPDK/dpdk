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

#include "i40e_adminq.h"
#include "i40e_prototype.h"
#include "i40e_dcb.h"

/**
 * i40e_get_dcbx_status
 * @hw: pointer to the hw struct
 * @status: Embedded DCBX Engine Status
 *
 * Get the DCBX status from the Firmware
 **/
enum i40e_status_code i40e_get_dcbx_status(struct i40e_hw *hw, u16 *status)
{
	u32 reg;

	if (!status)
		return I40E_ERR_PARAM;

	reg = rd32(hw, I40E_PRTDCB_GENS);
	*status = (u16)((reg & I40E_PRTDCB_GENS_DCBX_STATUS_MASK) >>
			I40E_PRTDCB_GENS_DCBX_STATUS_SHIFT);

	return I40E_SUCCESS;
}

/**
 * i40e_parse_ieee_etscfg_tlv
 * @tlv: IEEE 802.1Qaz ETS CFG TLV
 * @dcbcfg: Local store to update ETS CFG data
 *
 * Parses IEEE 802.1Qaz ETS CFG TLV
 **/
static void i40e_parse_ieee_etscfg_tlv(struct i40e_lldp_org_tlv *tlv,
				       struct i40e_dcbx_config *dcbcfg)
{
	struct i40e_ieee_ets_config *etscfg;
	u8 *buf = tlv->tlvinfo;
	u16 offset = 0;
	u8 priority;
	int i;

	/* First Octet post subtype
	 * --------------------------
	 * |will-|CBS  | Re-  | Max |
	 * |ing  |     |served| TCs |
	 * --------------------------
	 * |1bit | 1bit|3 bits|3bits|
	 */
	etscfg = &dcbcfg->etscfg;
	etscfg->willing = (u8)((buf[offset] & I40E_IEEE_ETS_WILLING_MASK) >>
			       I40E_IEEE_ETS_WILLING_SHIFT);
	etscfg->cbs = (u8)((buf[offset] & I40E_IEEE_ETS_CBS_MASK) >>
			   I40E_IEEE_ETS_CBS_SHIFT);
	etscfg->maxtcs = (u8)((buf[offset] & I40E_IEEE_ETS_MAXTC_MASK) >>
			      I40E_IEEE_ETS_MAXTC_SHIFT);

	/* Move offset to Priority Assignment Table */
	offset++;

	/* Priority Assignment Table (4 octets)
	 * Octets:|    1    |    2    |    3    |    4    |
	 *        -----------------------------------------
	 *        |pri0|pri1|pri2|pri3|pri4|pri5|pri6|pri7|
	 *        -----------------------------------------
	 *   Bits:|7  4|3  0|7  4|3  0|7  4|3  0|7  4|3  0|
	 *        -----------------------------------------
	 */
	for (i = 0; i < 4; i++) {
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_1_MASK) >>
				I40E_IEEE_ETS_PRIO_1_SHIFT);
		etscfg->prioritytable[i * 2] =  priority;
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_0_MASK) >>
				I40E_IEEE_ETS_PRIO_0_SHIFT);
		etscfg->prioritytable[i * 2 + 1] = priority;
		offset++;
	}

	/* TC Bandwidth Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		etscfg->tcbwtable[i] = buf[offset++];

	/* TSA Assignment Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		etscfg->tsatable[i] = buf[offset++];
}

/**
 * i40e_parse_ieee_etsrec_tlv
 * @tlv: IEEE 802.1Qaz ETS REC TLV
 * @dcbcfg: Local store to update ETS REC data
 *
 * Parses IEEE 802.1Qaz ETS REC TLV
 **/
static void i40e_parse_ieee_etsrec_tlv(struct i40e_lldp_org_tlv *tlv,
				       struct i40e_dcbx_config *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;
	u16 offset = 0;
	u8 priority;
	int i;

	/* Move offset to priority table */
	offset++;

	/* Priority Assignment Table (4 octets)
	 * Octets:|    1    |    2    |    3    |    4    |
	 *        -----------------------------------------
	 *        |pri0|pri1|pri2|pri3|pri4|pri5|pri6|pri7|
	 *        -----------------------------------------
	 *   Bits:|7  4|3  0|7  4|3  0|7  4|3  0|7  4|3  0|
	 *        -----------------------------------------
	 */
	for (i = 0; i < 4; i++) {
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_1_MASK) >>
				I40E_IEEE_ETS_PRIO_1_SHIFT);
		dcbcfg->etsrec.prioritytable[i*2] =  priority;
		priority = (u8)((buf[offset] & I40E_IEEE_ETS_PRIO_0_MASK) >>
				I40E_IEEE_ETS_PRIO_0_SHIFT);
		dcbcfg->etsrec.prioritytable[i*2 + 1] = priority;
		offset++;
	}

	/* TC Bandwidth Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		dcbcfg->etsrec.tcbwtable[i] = buf[offset++];

	/* TSA Assignment Table (8 octets)
	 * Octets:| 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 |
	 *        ---------------------------------
	 *        |tc0|tc1|tc2|tc3|tc4|tc5|tc6|tc7|
	 *        ---------------------------------
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++)
		dcbcfg->etsrec.tsatable[i] = buf[offset++];
}

/**
 * i40e_parse_ieee_pfccfg_tlv
 * @tlv: IEEE 802.1Qaz PFC CFG TLV
 * @dcbcfg: Local store to update PFC CFG data
 *
 * Parses IEEE 802.1Qaz PFC CFG TLV
 **/
static void i40e_parse_ieee_pfccfg_tlv(struct i40e_lldp_org_tlv *tlv,
				       struct i40e_dcbx_config *dcbcfg)
{
	u8 *buf = tlv->tlvinfo;

	/* ----------------------------------------
	 * |will-|MBC  | Re-  | PFC |  PFC Enable  |
	 * |ing  |     |served| cap |              |
	 * -----------------------------------------
	 * |1bit | 1bit|2 bits|4bits| 1 octet      |
	 */
	dcbcfg->pfc.willing = (u8)((buf[0] & I40E_IEEE_PFC_WILLING_MASK) >>
				   I40E_IEEE_PFC_WILLING_SHIFT);
	dcbcfg->pfc.mbc = (u8)((buf[0] & I40E_IEEE_PFC_MBC_MASK) >>
			       I40E_IEEE_PFC_MBC_SHIFT);
	dcbcfg->pfc.pfccap = (u8)((buf[0] & I40E_IEEE_PFC_CAP_MASK) >>
				  I40E_IEEE_PFC_CAP_SHIFT);
	dcbcfg->pfc.pfcenable = buf[1];
}

/**
 * i40e_parse_ieee_app_tlv
 * @tlv: IEEE 802.1Qaz APP TLV
 * @dcbcfg: Local store to update APP PRIO data
 *
 * Parses IEEE 802.1Qaz APP PRIO TLV
 **/
static void i40e_parse_ieee_app_tlv(struct i40e_lldp_org_tlv *tlv,
				    struct i40e_dcbx_config *dcbcfg)
{
	u16 typelength;
	u16 offset = 0;
	u16 length;
	int i = 0;
	u8 *buf;

	typelength = I40E_NTOHS(tlv->typelength);
	length = (u16)((typelength & I40E_LLDP_TLV_LEN_MASK) >>
		       I40E_LLDP_TLV_LEN_SHIFT);
	buf = tlv->tlvinfo;

	/* The App priority table starts 5 octets after TLV header */
	length -= (sizeof(tlv->ouisubtype) + 1);

	/* Move offset to App Priority Table */
	offset++;

	/* Application Priority Table (3 octets)
	 * Octets:|         1          |    2    |    3    |
	 *        -----------------------------------------
	 *        |Priority|Rsrvd| Sel |    Protocol ID    |
	 *        -----------------------------------------
	 *   Bits:|23    21|20 19|18 16|15                0|
	 *        -----------------------------------------
	 */
	while (offset < length) {
		dcbcfg->app[i].priority = (u8)((buf[offset] &
						I40E_IEEE_APP_PRIO_MASK) >>
					       I40E_IEEE_APP_PRIO_SHIFT);
		dcbcfg->app[i].selector = (u8)((buf[offset] &
						I40E_IEEE_APP_SEL_MASK) >>
					       I40E_IEEE_APP_SEL_SHIFT);
		dcbcfg->app[i].protocolid = (buf[offset + 1] << 0x8) |
					     buf[offset + 2];
		/* Move to next app */
		offset += 3;
		i++;
		if (i >= I40E_DCBX_MAX_APPS)
			break;
	}

	dcbcfg->numapps = i;
}

/**
 * i40e_parse_ieee_etsrec_tlv
 * @tlv: IEEE 802.1Qaz TLV
 * @dcbcfg: Local store to update ETS REC data
 *
 * Get the TLV subtype and send it to parsing function
 * based on the subtype value
 **/
static void i40e_parse_ieee_tlv(struct i40e_lldp_org_tlv *tlv,
				struct i40e_dcbx_config *dcbcfg)
{
	u32 ouisubtype;
	u8 subtype;

	ouisubtype = I40E_NTOHL(tlv->ouisubtype);
	subtype = (u8)((ouisubtype & I40E_LLDP_TLV_SUBTYPE_MASK) >>
		       I40E_LLDP_TLV_SUBTYPE_SHIFT);
	switch (subtype) {
	case I40E_IEEE_SUBTYPE_ETS_CFG:
		i40e_parse_ieee_etscfg_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_SUBTYPE_ETS_REC:
		i40e_parse_ieee_etsrec_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_SUBTYPE_PFC_CFG:
		i40e_parse_ieee_pfccfg_tlv(tlv, dcbcfg);
		break;
	case I40E_IEEE_SUBTYPE_APP_PRI:
		i40e_parse_ieee_app_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

/**
 * i40e_parse_org_tlv
 * @tlv: Organization specific TLV
 * @dcbcfg: Local store to update ETS REC data
 *
 * Currently only IEEE 802.1Qaz TLV is supported, all others
 * will be returned
 **/
static void i40e_parse_org_tlv(struct i40e_lldp_org_tlv *tlv,
			       struct i40e_dcbx_config *dcbcfg)
{
	u32 ouisubtype;
	u32 oui;

	ouisubtype = I40E_NTOHL(tlv->ouisubtype);
	oui = (u32)((ouisubtype & I40E_LLDP_TLV_OUI_MASK) >>
		    I40E_LLDP_TLV_OUI_SHIFT);
	switch (oui) {
	case I40E_IEEE_8021QAZ_OUI:
		i40e_parse_ieee_tlv(tlv, dcbcfg);
		break;
	default:
		break;
	}
}

/**
 * i40e_lldp_to_dcb_config
 * @lldpmib: LLDPDU to be parsed
 * @dcbcfg: store for LLDPDU data
 *
 * Parse DCB configuration from the LLDPDU
 **/
enum i40e_status_code i40e_lldp_to_dcb_config(u8 *lldpmib,
				    struct i40e_dcbx_config *dcbcfg)
{
	enum i40e_status_code ret = I40E_SUCCESS;
	struct i40e_lldp_org_tlv *tlv;
	u16 type;
	u16 length;
	u16 typelength;
	u16 offset = 0;

	if (!lldpmib || !dcbcfg)
		return I40E_ERR_PARAM;

	/* set to the start of LLDPDU */
	lldpmib += I40E_LLDP_MIB_HLEN;
	tlv = (struct i40e_lldp_org_tlv *)lldpmib;
	while (1) {
		typelength = I40E_NTOHS(tlv->typelength);
		type = (u16)((typelength & I40E_LLDP_TLV_TYPE_MASK) >>
			     I40E_LLDP_TLV_TYPE_SHIFT);
		length = (u16)((typelength & I40E_LLDP_TLV_LEN_MASK) >>
			       I40E_LLDP_TLV_LEN_SHIFT);
		offset += sizeof(typelength) + length;

		/* END TLV or beyond LLDPDU size */
		if ((type == I40E_TLV_TYPE_END) || (offset > I40E_LLDPDU_SIZE))
			break;

		switch (type) {
		case I40E_TLV_TYPE_ORG:
			i40e_parse_org_tlv(tlv, dcbcfg);
			break;
		default:
			break;
		}

		/* Move to next TLV */
		tlv = (struct i40e_lldp_org_tlv *)((char *)tlv +
						    sizeof(tlv->typelength) +
						    length);
	}

	return ret;
}

/**
 * i40e_aq_get_dcb_config
 * @hw: pointer to the hw struct
 * @mib_type: mib type for the query
 * @bridgetype: bridge type for the query (remote)
 * @dcbcfg: store for LLDPDU data
 *
 * Query DCB configuration from the Firmware
 **/
enum i40e_status_code i40e_aq_get_dcb_config(struct i40e_hw *hw, u8 mib_type,
				   u8 bridgetype,
				   struct i40e_dcbx_config *dcbcfg)
{
	enum i40e_status_code ret = I40E_SUCCESS;
	struct i40e_virt_mem mem;
	u8 *lldpmib;

	/* Allocate the LLDPDU */
	ret = i40e_allocate_virt_mem(hw, &mem, I40E_LLDPDU_SIZE);
	if (ret)
		return ret;

	lldpmib = (u8 *)mem.va;
	ret = i40e_aq_get_lldp_mib(hw, bridgetype, mib_type,
				   (void *)lldpmib, I40E_LLDPDU_SIZE,
				   NULL, NULL, NULL);
	if (ret)
		goto free_mem;

	/* Parse LLDP MIB to get dcb configuration */
	ret = i40e_lldp_to_dcb_config(lldpmib, dcbcfg);

free_mem:
	i40e_free_virt_mem(hw, &mem);
	return ret;
}

/**
 * i40e_get_dcb_config
 * @hw: pointer to the hw struct
 *
 * Get DCB configuration from the Firmware
 **/
enum i40e_status_code i40e_get_dcb_config(struct i40e_hw *hw)
{
	enum i40e_status_code ret = I40E_SUCCESS;

	/* Get Local DCB Config */
	ret = i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_LOCAL, 0,
				     &hw->local_dcbx_config);
	if (ret)
		goto out;

	/* Get Remote DCB Config */
	ret = i40e_aq_get_dcb_config(hw, I40E_AQ_LLDP_MIB_REMOTE,
				     I40E_AQ_LLDP_BRIDGE_TYPE_NEAREST_BRIDGE,
				     &hw->remote_dcbx_config);
out:
	return ret;
}

/**
 * i40e_init_dcb
 * @hw: pointer to the hw struct
 *
 * Update DCB configuration from the Firmware
 **/
enum i40e_status_code i40e_init_dcb(struct i40e_hw *hw)
{
	enum i40e_status_code ret = I40E_SUCCESS;

	if (!hw->func_caps.dcb)
		return ret;

	/* Get DCBX status */
	ret = i40e_get_dcbx_status(hw, &hw->dcbx_status);
	if (ret)
		return ret;

	/* Check the DCBX Status */
	switch (hw->dcbx_status) {
	case I40E_DCBX_STATUS_DONE:
	case I40E_DCBX_STATUS_IN_PROGRESS:
		/* Get current DCBX configuration */
		ret = i40e_get_dcb_config(hw);
		break;
	case I40E_DCBX_STATUS_DISABLED:
		return ret;
	case I40E_DCBX_STATUS_NOT_STARTED:
	case I40E_DCBX_STATUS_MULTIPLE_PEERS:
	default:
		break;
	}

	/* Configure the LLDP MIB change event */
	ret = i40e_aq_cfg_lldp_mib_change_event(hw, true, NULL);
	if (ret)
		return ret;

	return ret;
}
#ifdef I40E_DCB_SW

/**
 * i40e_dcbx_event_handler
 * @hw: pointer to the hw struct
 * @e: event data to be processed (LLDPDU)
 *
 * Process LLDP MIB Change event from the Firmware
 **/
enum i40e_status_code i40e_process_lldp_event(struct i40e_hw *hw,
					      struct i40e_arq_event_info *e)
{
	enum i40e_status_code ret = I40E_SUCCESS;
	UNREFERENCED_2PARAMETER(hw, e);

	return ret;
}

/**
 * i40e_dcb_hw_rx_fifo_config
 * @hw: pointer to the hw struct
 * @ets_mode: Strict Priority or Round Robin mode
 * @non_ets_mode: Strict Priority or Round Robin
 * @max_exponent: Exponent to calculate max refill credits
 * @lltc_map: Low latency TC bitmap
 *
 * Configure HW Rx FIFO as part of DCB configuration.
 **/
void i40e_dcb_hw_rx_fifo_config(struct i40e_hw *hw,
				enum i40e_dcb_arbiter_mode ets_mode,
				enum i40e_dcb_arbiter_mode non_ets_mode,
				u32 max_exponent,
				u8 lltc_map)
{
	u32 reg = 0;

	reg = rd32(hw, I40E_PRTDCB_RETSC);

	reg &= ~I40E_PRTDCB_RETSC_ETS_MODE_MASK;
	reg |= ((u32)ets_mode << I40E_PRTDCB_RETSC_ETS_MODE_SHIFT) &
		I40E_PRTDCB_RETSC_ETS_MODE_MASK;

	reg &= ~I40E_PRTDCB_RETSC_NON_ETS_MODE_MASK;
	reg |= ((u32)non_ets_mode << I40E_PRTDCB_RETSC_NON_ETS_MODE_SHIFT) &
		I40E_PRTDCB_RETSC_NON_ETS_MODE_MASK;

	reg &= ~I40E_PRTDCB_RETSC_ETS_MAX_EXP_MASK;
	reg |= (max_exponent << I40E_PRTDCB_RETSC_ETS_MAX_EXP_SHIFT) &
		I40E_PRTDCB_RETSC_ETS_MAX_EXP_MASK;

	reg &= ~I40E_PRTDCB_RETSC_LLTC_MASK;
	reg |= (lltc_map << I40E_PRTDCB_RETSC_LLTC_SHIFT) &
		I40E_PRTDCB_RETSC_LLTC_MASK;
	wr32(hw, I40E_PRTDCB_RETSC, reg);
}

/**
 * i40e_dcb_hw_rx_cmd_monitor_config
 * @hw: pointer to the hw struct
 * @num_tc: Total number of traffic class
 * @num_ports: Total number of ports on device
 *
 * Configure HW Rx command monitor as part of DCB configuration.
 **/
void i40e_dcb_hw_rx_cmd_monitor_config(struct i40e_hw *hw,
				       u8 num_tc, u8 num_ports)
{
	u32 threshold = 0;
	u32 fifo_size = 0;
	u32 reg = 0;

	/* Set the threshold and fifo_size based on number of ports */
	switch (num_ports) {
	case 1:
		threshold = 0xF;
		fifo_size = 0x10;
		break;
	case 2:
		if (num_tc > 4) {
			threshold = 0xC;
			fifo_size = 0x8;
		} else {
			threshold = 0xF;
			fifo_size = 0x10;
		}
		break;
	case 4:
		if (num_tc > 4) {
			threshold = 0x6;
			fifo_size = 0x4;
		} else {
			threshold = 0x9;
			fifo_size = 0x8;
		}
		break;
	}


	reg = rd32(hw, I40E_PRTDCB_RPPMC);
	reg &= ~I40E_PRTDCB_RPPMC_RX_FIFO_SIZE_MASK;
	reg |= (fifo_size << I40E_PRTDCB_RPPMC_RX_FIFO_SIZE_SHIFT) &
		I40E_PRTDCB_RPPMC_RX_FIFO_SIZE_MASK;
	wr32(hw, I40E_PRTDCB_RPPMC, reg);
}

/**
 * i40e_dcb_hw_pfc_config
 * @hw: pointer to the hw struct
 * @pfc_en: Bitmap of PFC enabled priorities
 * @prio_tc: priority to tc assignment indexed by priority
 *
 * Configure HW Priority Flow Controller as part of DCB configuration.
 **/
void i40e_dcb_hw_pfc_config(struct i40e_hw *hw,
			    u8 pfc_en, u8 *prio_tc)
{
	u16 pause_time = I40E_DEFAULT_PAUSE_TIME;
	u16 refresh_time = pause_time/2;
	u8 first_pfc_prio = 0;
	u32 link_speed = 0;
	u8 num_pfc_tc = 0;
	u8 tc2pfc = 0;
	u32 reg = 0;
	u8 i;

	/* Get Number of PFC TCs and TC2PFC map */
	for (i = 0; i < I40E_MAX_USER_PRIORITY; i++) {
		if (pfc_en & (1 << i)) {
			if (!first_pfc_prio)
				first_pfc_prio = i;
			/* Set bit for the PFC TC */
			tc2pfc |= 1 << prio_tc[i];
			num_pfc_tc++;
		}
	}

	link_speed = hw->phy.link_info.link_speed;
	switch (link_speed) {
	case I40E_LINK_SPEED_10GB:
		reg = rd32(hw, I40E_PRTDCB_MFLCN);
		reg |= (1 << I40E_PRTDCB_MFLCN_DPF_SHIFT) &
			I40E_PRTDCB_MFLCN_DPF_MASK;
		reg &= ~I40E_PRTDCB_MFLCN_RFCE_MASK;
		reg &= ~I40E_PRTDCB_MFLCN_RPFCE_MASK;
		if (pfc_en) {
			reg |= (1 << I40E_PRTDCB_MFLCN_RPFCM_SHIFT) &
				I40E_PRTDCB_MFLCN_RPFCM_MASK;
			reg |= ((u32)pfc_en << I40E_PRTDCB_MFLCN_RPFCE_SHIFT) &
				I40E_PRTDCB_MFLCN_RPFCE_MASK;
		}
		wr32(hw, I40E_PRTDCB_MFLCN, reg);

		reg = rd32(hw, I40E_PRTDCB_FCCFG);
		reg &= ~I40E_PRTDCB_FCCFG_TFCE_MASK;
		if (pfc_en)
			reg |= (2 << I40E_PRTDCB_FCCFG_TFCE_SHIFT) &
				I40E_PRTDCB_FCCFG_TFCE_MASK;
		wr32(hw, I40E_PRTDCB_FCCFG, reg);

		/* FCTTV and FCRTV to be set by default */
		break;
	case I40E_LINK_SPEED_40GB:
		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP);
		reg &= ~I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP_HSEC_CTL_RX_ENABLE_GPP_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP, reg);

		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP);
		reg &= ~I40E_PRTMAC_HSEC_CTL_RX_ENABLE_GPP_HSEC_CTL_RX_ENABLE_GPP_MASK;
		reg |= (1 <<
			   I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP_HSEC_CTL_RX_ENABLE_PPP_SHIFT) &
			I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP_HSEC_CTL_RX_ENABLE_PPP_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_RX_ENABLE_PPP, reg);

		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE);
		reg &= ~I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_HSEC_CTL_RX_PAUSE_ENABLE_MASK;
		reg |= ((u32)pfc_en <<
			   I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_HSEC_CTL_RX_PAUSE_ENABLE_SHIFT) &
			I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE_HSEC_CTL_RX_PAUSE_ENABLE_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_RX_PAUSE_ENABLE, reg);

		reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE);
		reg &= ~I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_HSEC_CTL_TX_PAUSE_ENABLE_MASK;
		reg |= ((u32)pfc_en <<
			   I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_HSEC_CTL_TX_PAUSE_ENABLE_SHIFT) &
			I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE_HSEC_CTL_TX_PAUSE_ENABLE_MASK;
		wr32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_ENABLE, reg);

		for (i = 0; i < I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_MAX_INDEX; i++) {
			reg = rd32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER(i));
			reg &= ~I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_MASK;
			if (pfc_en) {
				reg |= ((u32)refresh_time <<
					I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_SHIFT) &
					I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_HSEC_CTL_TX_PAUSE_REFRESH_TIMER_MASK;
			}
			wr32(hw, I40E_PRTMAC_HSEC_CTL_TX_PAUSE_REFRESH_TIMER(i), reg);
		}
		/*
		 * PRTMAC_HSEC_CTL_TX_PAUSE_QUANTA default value is 0xFFFF
		 * for all user priorities
		 */
		break;
	}

	reg = rd32(hw, I40E_PRTDCB_TC2PFC);
	reg &= ~I40E_PRTDCB_TC2PFC_TC2PFC_MASK;
	reg |= ((u32)tc2pfc << I40E_PRTDCB_TC2PFC_TC2PFC_SHIFT) &
		I40E_PRTDCB_TC2PFC_TC2PFC_MASK;
	wr32(hw, I40E_PRTDCB_TC2PFC, reg);

	reg = rd32(hw, I40E_PRTDCB_RUP);
	reg &= ~I40E_PRTDCB_RUP_NOVLANUP_MASK;
	reg |= ((u32)first_pfc_prio << I40E_PRTDCB_RUP_NOVLANUP_SHIFT) &
		 I40E_PRTDCB_RUP_NOVLANUP_MASK;
	wr32(hw, I40E_PRTDCB_RUP, reg);

	reg = rd32(hw, I40E_PRTDCB_TDPMC);
	reg &= ~I40E_PRTDCB_TDPMC_TCPM_MODE_MASK;
	if (num_pfc_tc > 2) {
		reg |= (1 << I40E_PRTDCB_TDPMC_TCPM_MODE_SHIFT) &
			I40E_PRTDCB_TDPMC_TCPM_MODE_MASK;
	}
	wr32(hw, I40E_PRTDCB_TDPMC, reg);

	reg = rd32(hw, I40E_PRTDCB_TCPMC);
	reg &= ~I40E_PRTDCB_TCPMC_TCPM_MODE_MASK;
	if (num_pfc_tc > 2) {
		reg |= (1 << I40E_PRTDCB_TCPMC_TCPM_MODE_SHIFT) &
			I40E_PRTDCB_TCPMC_TCPM_MODE_MASK;
	}
	wr32(hw, I40E_PRTDCB_TCPMC, reg);
}

/**
 * i40e_dcb_hw_set_num_tc
 * @hw: pointer to the hw struct
 * @num_tc: number of traffic classes
 *
 * Configure number of traffic classes in HW
 **/
void i40e_dcb_hw_set_num_tc(struct i40e_hw *hw, u8 num_tc)
{
	u32 reg = rd32(hw, I40E_PRTDCB_GENC);

	reg &= ~I40E_PRTDCB_GENC_NUMTC_MASK;
	reg |= ((u32)num_tc << I40E_PRTDCB_GENC_NUMTC_SHIFT) &
		I40E_PRTDCB_GENC_NUMTC_MASK;
	wr32(hw, I40E_PRTDCB_GENC, reg);
}

/**
 * i40e_dcb_hw_get_num_tc
 * @hw: pointer to the hw struct
 *
 * Returns number of traffic classes configured in HW
 **/
u8 i40e_dcb_hw_get_num_tc(struct i40e_hw *hw)
{
	u32 reg = rd32(hw, I40E_PRTDCB_GENC);

	return (reg >> I40E_PRTDCB_GENC_NUMTC_SHIFT) &
		I40E_PRTDCB_GENC_NUMTC_MASK;
}

/**
 * i40e_dcb_hw_rx_ets_bw_config
 * @hw: pointer to the hw struct
 * @bw_share: Bandwidth share indexed per traffic class
 * @mode: Strict Priority or Round Robin mode between UP sharing same
 * traffic class
 * @prio_type: TC is ETS enabled or strict priority
 *
 * Configure HW Rx ETS bandwidth as part of DCB configuration.
 **/
void i40e_dcb_hw_rx_ets_bw_config(struct i40e_hw *hw, u8 *bw_share,
				  u8 *mode, u8 *prio_type)
{
	u32 reg = 0;
	u8 i = 0;

	for (i = 0; i <= I40E_PRTDCB_RETSTCC_MAX_INDEX; i++) {
		reg = rd32(hw, I40E_PRTDCB_RETSTCC(i));
		reg &= ~(I40E_PRTDCB_RETSTCC_BWSHARE_MASK     |
			 I40E_PRTDCB_RETSTCC_UPINTC_MODE_MASK |
			 I40E_PRTDCB_RETSTCC_ETSTC_SHIFT);
		reg |= ((u32)bw_share[i] << I40E_PRTDCB_RETSTCC_BWSHARE_SHIFT) &
			 I40E_PRTDCB_RETSTCC_BWSHARE_MASK;
		reg |= ((u32)mode[i] << I40E_PRTDCB_RETSTCC_UPINTC_MODE_SHIFT) &
			 I40E_PRTDCB_RETSTCC_UPINTC_MODE_MASK;
		reg |= ((u32)prio_type[i] << I40E_PRTDCB_RETSTCC_ETSTC_SHIFT) &
			 I40E_PRTDCB_RETSTCC_ETSTC_MASK;
		wr32(hw, I40E_PRTDCB_RETSTCC(i), reg);
	}
}

/**
 * i40e_dcb_hw_rx_ets_bw_config
 * @hw: pointer to the hw struct
 * @prio_tc: priority to tc assignment indexed by priority
 *
 * Configure HW Rx UP2TC map as part of DCB configuration.
 **/
void i40e_dcb_hw_rx_up2tc_config(struct i40e_hw *hw, u8 *prio_tc)
{
	u32 reg = 0;

#define I40E_UP2TC_REG(val, i) \
		((val << I40E_PRTDCB_RUP2TC_UP##i##TC_SHIFT) & \
		  I40E_PRTDCB_RUP2TC_UP##i##TC_MASK)

	reg = rd32(hw, I40E_PRTDCB_RUP2TC);
	reg |= I40E_UP2TC_REG(prio_tc[0], 0);
	reg |= I40E_UP2TC_REG(prio_tc[1], 1);
	reg |= I40E_UP2TC_REG(prio_tc[2], 2);
	reg |= I40E_UP2TC_REG(prio_tc[3], 3);
	reg |= I40E_UP2TC_REG(prio_tc[4], 4);
	reg |= I40E_UP2TC_REG(prio_tc[5], 5);
	reg |= I40E_UP2TC_REG(prio_tc[6], 6);
	reg |= I40E_UP2TC_REG(prio_tc[7], 7);
	wr32(hw, I40E_PRTDCB_RUP2TC, reg);
}

/**
 * i40e_dcb_hw_calculate_pool_sizes
 * @hw: pointer to the hw struct
 * @num_ports: Number of available ports on the device
 * @eee_enabled: EEE enabled for the given port
 * @pfc_en: Bit map of PFC enabled traffic classes
 * @mfs_tc: Array of max frame size for each traffic class
 *
 * Calculate the shared and dedicated per TC pool sizes,
 * watermarks and threshold values.
 **/
void i40e_dcb_hw_calculate_pool_sizes(struct i40e_hw *hw,
				      u8 num_ports, bool eee_enabled,
				      u8 pfc_en, u32 *mfs_tc,
				      struct i40e_rx_pb_config *pb_cfg)
{
	u32 pool_size[I40E_MAX_TRAFFIC_CLASS];
	u32 high_wm[I40E_MAX_TRAFFIC_CLASS];
	u32 low_wm[I40E_MAX_TRAFFIC_CLASS];
	int shared_pool_size = 0; /* Need signed variable */
	u32 total_pool_size = 0;
	u32 port_pb_size = 0;
	u32 mfs_max = 0;
	u32 pcirtt = 0;
	u8 i = 0;

	/* Get the MFS(max) for the port */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (mfs_tc[i] > mfs_max)
			mfs_max = mfs_tc[i];
	}

	pcirtt = I40E_BT2B(I40E_PCIRTT_LINK_SPEED_10G);

	/* Calculate effective Rx PB size per port */
	port_pb_size = (I40E_DEVICE_RPB_SIZE/num_ports);
	if (eee_enabled)
		port_pb_size -= I40E_BT2B(I40E_EEE_TX_LPI_EXIT_TIME);
	port_pb_size -= mfs_max;

	/* Step 1 Calculating tc pool/shared pool sizes and watermarks */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		if (pfc_en & (1 << i)) {
			low_wm[i] = (2 * mfs_tc[i]) + pcirtt;
			high_wm[i] = low_wm[i];
			high_wm[i] += ((mfs_max > I40E_MAX_FRAME_SIZE)
					? mfs_max : I40E_MAX_FRAME_SIZE);
			pool_size[i] = high_wm[i];
			pool_size[i] += I40E_BT2B(I40E_STD_DV_TC(mfs_max,
								mfs_tc[i]));
		} else {
			low_wm[i] = 0;
			pool_size[i] = (2 * mfs_tc[i]) + pcirtt;
			high_wm[i] = pool_size[i];
		}
		total_pool_size += pool_size[i];
	}

	shared_pool_size = port_pb_size - total_pool_size;
	if (shared_pool_size > 0) {
		pb_cfg->shared_pool_size = shared_pool_size;
		pb_cfg->shared_pool_high_wm = shared_pool_size;
		pb_cfg->shared_pool_low_wm = 0;
		for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
			pb_cfg->shared_pool_low_thresh[i] = 0;
			pb_cfg->shared_pool_high_thresh[i] = shared_pool_size;
			pb_cfg->tc_pool_size[i] = pool_size[i];
			pb_cfg->tc_pool_high_wm[i] = high_wm[i];
			pb_cfg->tc_pool_low_wm[i] = low_wm[i];
		}

	} else {
		i40e_debug(hw, I40E_DEBUG_DCB,
			   "The shared pool size for the port is negative %d.\n",
			   shared_pool_size);
	}
}

/**
 * i40e_dcb_hw_rx_pb_config
 * @hw: pointer to the hw struct
 * @old_pb_cfg: Existing Rx Packet buffer configuration
 * @new_pb_cfg: New Rx Packet buffer configuration
 *
 * Program the Rx Packet Buffer registers.
 **/
void i40e_dcb_hw_rx_pb_config(struct i40e_hw *hw,
			      struct i40e_rx_pb_config *old_pb_cfg,
			      struct i40e_rx_pb_config *new_pb_cfg)
{
	u32 old_val = 0;
	u32 new_val = 0;
	u32 reg = 0;
	u8 i = 0;

	/* Program the shared pool low water mark per port if decreasing */
	old_val = old_pb_cfg->shared_pool_low_wm;
	new_val = new_pb_cfg->shared_pool_low_wm;
	if (new_val < old_val) {
		reg = rd32(hw, I40E_PRTRPB_SLW);
		reg &= ~I40E_PRTRPB_SLW_SLW_MASK;
		reg |= (new_val << I40E_PRTRPB_SLW_SLW_SHIFT) &
			I40E_PRTRPB_SLW_SLW_MASK;
		wr32(hw, I40E_PRTRPB_SLW, reg);
	}

	/* Program the shared pool low threshold and tc pool
	 * low water mark per TC that are decreasing.
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_low_thresh[i];
		new_val = new_pb_cfg->shared_pool_low_thresh[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_SLT(i));
			reg &= ~I40E_PRTRPB_SLT_SLT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SLT_SLT_TCN_SHIFT) &
				I40E_PRTRPB_SLT_SLT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SLT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_low_wm[i];
		new_val = new_pb_cfg->tc_pool_low_wm[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_DLW(i));
			reg &= ~I40E_PRTRPB_DLW_DLW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DLW_DLW_TCN_SHIFT) &
				I40E_PRTRPB_DLW_DLW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DLW(i), reg);
		}
	}

	/* Program the shared pool high water mark per port if decreasing */
	old_val = old_pb_cfg->shared_pool_high_wm;
	new_val = new_pb_cfg->shared_pool_high_wm;
	if (new_val < old_val) {
		reg = rd32(hw, I40E_PRTRPB_SHW);
		reg &= ~I40E_PRTRPB_SHW_SHW_MASK;
		reg |= (new_val << I40E_PRTRPB_SHW_SHW_SHIFT) &
			I40E_PRTRPB_SHW_SHW_MASK;
		wr32(hw, I40E_PRTRPB_SHW, reg);
	}

	/* Program the shared pool high threshold and tc pool
	 * high water mark per TC that are decreasing.
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_high_thresh[i];
		new_val = new_pb_cfg->shared_pool_high_thresh[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_SHT(i));
			reg &= ~I40E_PRTRPB_SHT_SHT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SHT_SHT_TCN_SHIFT) &
				I40E_PRTRPB_SHT_SHT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SHT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_high_wm[i];
		new_val = new_pb_cfg->tc_pool_high_wm[i];
		if (new_val < old_val) {
			reg = rd32(hw, I40E_PRTRPB_DHW(i));
			reg &= ~I40E_PRTRPB_DHW_DHW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DHW_DHW_TCN_SHIFT) &
				I40E_PRTRPB_DHW_DHW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DHW(i), reg);
		}
	}

	/* Write Dedicated Pool Sizes per TC */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		new_val = new_pb_cfg->tc_pool_size[i];
		reg = rd32(hw, I40E_PRTRPB_DPS(i));
		reg &= ~I40E_PRTRPB_DPS_DPS_TCN_MASK;
		reg |= (new_val << I40E_PRTRPB_DPS_DPS_TCN_SHIFT) &
			I40E_PRTRPB_DPS_DPS_TCN_MASK;
		wr32(hw, I40E_PRTRPB_DPS(i), reg);
	}

	/* Write Shared Pool Size per port */
	new_val = new_pb_cfg->shared_pool_size;
	reg = rd32(hw, I40E_PRTRPB_SPS);
	reg &= ~I40E_PRTRPB_SPS_SPS_MASK;
	reg |= (new_val << I40E_PRTRPB_SPS_SPS_SHIFT) &
		I40E_PRTRPB_SPS_SPS_MASK;
	wr32(hw, I40E_PRTRPB_SPS, reg);

	/* Program the shared pool low water mark per port if increasing */
	old_val = old_pb_cfg->shared_pool_low_wm;
	new_val = new_pb_cfg->shared_pool_low_wm;
	if (new_val > old_val) {
		reg = rd32(hw, I40E_PRTRPB_SLW);
		reg &= ~I40E_PRTRPB_SLW_SLW_MASK;
		reg |= (new_val << I40E_PRTRPB_SLW_SLW_SHIFT) &
			I40E_PRTRPB_SLW_SLW_MASK;
		wr32(hw, I40E_PRTRPB_SLW, reg);
	}

	/* Program the shared pool low threshold and tc pool
	 * low water mark per TC that are increasing.
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_low_thresh[i];
		new_val = new_pb_cfg->shared_pool_low_thresh[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_SLT(i));
			reg &= ~I40E_PRTRPB_SLT_SLT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SLT_SLT_TCN_SHIFT) &
				I40E_PRTRPB_SLT_SLT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SLT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_low_wm[i];
		new_val = new_pb_cfg->tc_pool_low_wm[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_DLW(i));
			reg &= ~I40E_PRTRPB_DLW_DLW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DLW_DLW_TCN_SHIFT) &
				I40E_PRTRPB_DLW_DLW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DLW(i), reg);
		}
	}

	/* Program the shared pool high water mark per port if increasing */
	old_val = old_pb_cfg->shared_pool_high_wm;
	new_val = new_pb_cfg->shared_pool_high_wm;
	if (new_val > old_val) {
		reg = rd32(hw, I40E_PRTRPB_SHW);
		reg &= ~I40E_PRTRPB_SHW_SHW_MASK;
		reg |= (new_val << I40E_PRTRPB_SHW_SHW_SHIFT) &
			I40E_PRTRPB_SHW_SHW_MASK;
		wr32(hw, I40E_PRTRPB_SHW, reg);
	}

	/* Program the shared pool high threshold and tc pool
	 * high water mark per TC that are increasing.
	 */
	for (i = 0; i < I40E_MAX_TRAFFIC_CLASS; i++) {
		old_val = old_pb_cfg->shared_pool_high_thresh[i];
		new_val = new_pb_cfg->shared_pool_high_thresh[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_SHT(i));
			reg &= ~I40E_PRTRPB_SHT_SHT_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_SHT_SHT_TCN_SHIFT) &
				I40E_PRTRPB_SHT_SHT_TCN_MASK;
			wr32(hw, I40E_PRTRPB_SHT(i), reg);
		}

		old_val = old_pb_cfg->tc_pool_high_wm[i];
		new_val = new_pb_cfg->tc_pool_high_wm[i];
		if (new_val > old_val) {
			reg = rd32(hw, I40E_PRTRPB_DHW(i));
			reg &= ~I40E_PRTRPB_DHW_DHW_TCN_MASK;
			reg |= (new_val << I40E_PRTRPB_DHW_DHW_TCN_SHIFT) &
				I40E_PRTRPB_DHW_DHW_TCN_MASK;
			wr32(hw, I40E_PRTRPB_DHW(i), reg);
		}
	}
}

/**
 * i40e_read_lldp_cfg - read LLDP Configuration data from NVM
 * @hw: pointer to the HW structure
 * @lldp_cfg: pointer to hold lldp configuration variables
 *
 * Reads the LLDP configuration data from NVM
 **/
enum i40e_status_code i40e_read_lldp_cfg(struct i40e_hw *hw,
					 struct i40e_lldp_variables *lldp_cfg)
{
	enum i40e_status_code ret = I40E_SUCCESS;
	struct i40e_emp_settings_module emp_ptr;
	u32 offset = 0;

	if (!lldp_cfg)
		return I40E_ERR_PARAM;

	ret = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
	if (ret != I40E_SUCCESS)
		goto err_lldp_cfg;

	ret = i40e_aq_read_nvm(hw, I40E_SR_EMP_MODULE_PTR, 0,
			       sizeof(emp_ptr), (u8 *)&emp_ptr,
			       true, NULL);
	i40e_release_nvm(hw);
	if (ret != I40E_SUCCESS)
		goto err_lldp_cfg;

	/* Calculate the byte offset for LLDP config pointer */
	offset = (2 * emp_ptr.lldp_cfg_ptr);
	offset += (2 * I40E_NVM_LLDP_CFG_PTR);
	ret = i40e_acquire_nvm(hw, I40E_RESOURCE_READ);
	if (ret != I40E_SUCCESS)
		goto err_lldp_cfg;

	ret = i40e_aq_read_nvm(hw, I40E_SR_EMP_MODULE_PTR, offset,
			       sizeof(struct i40e_lldp_variables),
			       (u8 *)lldp_cfg,
			       true, NULL);
	i40e_release_nvm(hw);

err_lldp_cfg:
	return ret;
}
#endif /* I40E_DCB_SW */
