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
