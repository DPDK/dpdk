/*
 * Copyright (c) 2012-2016 Solarflare Communications Inc.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice,
 *    this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright notice,
 *    this list of conditions and the following disclaimer in the documentation
 *    and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
 * EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 * The views and conclusions contained in the software and documentation are
 * those of the authors and should not be interpreted as representing official
 * policies, either expressed or implied, of the FreeBSD Project.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD

	__checkReturn	efx_rc_t
ef10_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep)
{
	efx_port_t *epp = &(enp->en_port);
	ef10_link_state_t els;
	efx_rc_t rc;

	if ((rc = ef10_phy_get_link(enp, &els)) != 0)
		goto fail1;

	epp->ep_adv_cap_mask = els.els_adv_cap_mask;
	epp->ep_fcntl = els.els_fcntl;

	*link_modep = els.els_link_mode;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	*link_modep = EFX_LINK_UNKNOWN;

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp)
{
	ef10_link_state_t els;
	efx_rc_t rc;

	/*
	 * Because EF10 doesn't *require* polling, we can't rely on
	 * ef10_mac_poll() being executed to populate epp->ep_mac_up.
	 */
	if ((rc = ef10_phy_get_link(enp, &els)) != 0)
		goto fail1;

	*mac_upp = els.els_mac_up;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

/*
 * EF10 adapters use MC_CMD_VADAPTOR_SET_MAC to set the
 * MAC address; the address field in MC_CMD_SET_MAC has no
 * effect.
 * MC_CMD_VADAPTOR_SET_MAC requires mac-spoofing privilege and
 * the port to have no filters or queues active.
 */
static	__checkReturn	efx_rc_t
efx_mcdi_vadapter_set_mac(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_VADAPTOR_SET_MAC_IN_LEN,
			    MC_CMD_VADAPTOR_SET_MAC_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_VADAPTOR_SET_MAC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_VADAPTOR_SET_MAC_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_VADAPTOR_SET_MAC_OUT_LEN;

	MCDI_IN_SET_DWORD(req, VADAPTOR_SET_MAC_IN_UPSTREAM_PORT_ID,
	    enp->en_vport_id);
	EFX_MAC_ADDR_COPY(MCDI_IN2(req, uint8_t, VADAPTOR_SET_MAC_IN_MACADDR),
	    epp->ep_mac_addr);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_mac_addr_set(
	__in		efx_nic_t *enp)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_vadapter_set_mac(enp)) != 0) {
		if (rc != ENOTSUP)
			goto fail1;

		/*
		 * Fallback for older Huntington firmware without Vadapter
		 * support.
		 */
		if ((rc = ef10_mac_reconfigure(enp)) != 0)
			goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn	efx_rc_t
efx_mcdi_mtu_set(
	__in		efx_nic_t *enp,
	__in		uint32_t mtu)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_SET_MAC_EXT_IN_LEN,
			    MC_CMD_SET_MAC_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_SET_MAC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_SET_MAC_EXT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_SET_MAC_OUT_LEN;

	/* Only configure the MTU in this call to MC_CMD_SET_MAC */
	MCDI_IN_SET_DWORD(req, SET_MAC_EXT_IN_MTU, mtu);
	MCDI_IN_POPULATE_DWORD_1(req, SET_MAC_EXT_IN_CONTROL,
			    SET_MAC_EXT_IN_CFG_MTU, 1);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

static	__checkReturn		efx_rc_t
efx_mcdi_mtu_get(
	__in		efx_nic_t *enp,
	__out		size_t *mtu)
{
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_SET_MAC_EXT_IN_LEN,
			    MC_CMD_SET_MAC_V2_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_SET_MAC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_SET_MAC_EXT_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_SET_MAC_V2_OUT_LEN;

	/*
	 * With MC_CMD_SET_MAC_EXT_IN_CONTROL set to 0, this just queries the
	 * MTU.  This should always be supported on Medford, but it is not
	 * supported on older Huntington firmware.
	 */
	MCDI_IN_SET_DWORD(req, SET_MAC_EXT_IN_CONTROL, 0);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}
	if (req.emr_out_length_used < MC_CMD_SET_MAC_V2_OUT_MTU_OFST + 4) {
		rc = EMSGSIZE;
		goto fail2;
	}

	*mtu = MCDI_OUT_DWORD(req, SET_MAC_V2_OUT_MTU);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_mac_pdu_set(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_rc_t rc;

	if (encp->enc_enhanced_set_mac_supported) {
		if ((rc = efx_mcdi_mtu_set(enp, epp->ep_mac_pdu)) != 0)
			goto fail1;
	} else {
		/*
		 * Fallback for older Huntington firmware, which always
		 * configure all of the parameters to MC_CMD_SET_MAC. This isn't
		 * suitable for setting the MTU on unpriviliged functions.
		 */
		if ((rc = ef10_mac_reconfigure(enp)) != 0)
			goto fail2;
	}

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn		efx_rc_t
ef10_mac_pdu_get(
	__in		efx_nic_t *enp,
	__out		size_t *pdu)
{
	efx_rc_t rc;

	if ((rc = efx_mcdi_mtu_get(enp, pdu)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

__checkReturn	efx_rc_t
ef10_mac_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MC_CMD_SET_MAC_IN_LEN,
			    MC_CMD_SET_MAC_OUT_LEN)];
	efx_rc_t rc;

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_SET_MAC;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_SET_MAC_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_SET_MAC_OUT_LEN;

	MCDI_IN_SET_DWORD(req, SET_MAC_IN_MTU, epp->ep_mac_pdu);
	MCDI_IN_SET_DWORD(req, SET_MAC_IN_DRAIN, epp->ep_mac_drain ? 1 : 0);
	EFX_MAC_ADDR_COPY(MCDI_IN2(req, uint8_t, SET_MAC_IN_ADDR),
			    epp->ep_mac_addr);

	/*
	 * Note: The Huntington MAC does not support REJECT_BRDCST.
	 * The REJECT_UNCST flag will also prevent multicast traffic
	 * from reaching the filters. As Huntington filters drop any
	 * traffic that does not match a filter it is ok to leave the
	 * MAC running in promiscuous mode. See bug41141.
	 *
	 * FIXME: Does REJECT_UNCST behave the same way on Medford?
	 */
	MCDI_IN_POPULATE_DWORD_2(req, SET_MAC_IN_REJECT,
				    SET_MAC_IN_REJECT_UNCST, 0,
				    SET_MAC_IN_REJECT_BRDCST, 0);

	/*
	 * Flow control, whether it is auto-negotiated or not,
	 * is set via the PHY advertised capabilities.  When set to
	 * automatic the MAC will use the PHY settings to determine
	 * the flow control settings.
	 */
	MCDI_IN_SET_DWORD(req, SET_MAC_IN_FCNTL, MC_CMD_FCNTL_AUTO);

	/* Do not include the Ethernet frame checksum in RX packets */
	MCDI_IN_POPULATE_DWORD_1(req, SET_MAC_IN_FLAGS,
				    SET_MAC_IN_FLAG_INCLUDE_FCS, 0);

	efx_mcdi_execute_quiet(enp, &req);

	if (req.emr_rc != 0) {
		/*
		 * Unprivileged functions cannot control link state,
		 * but still need to configure filters.
		 */
		if (req.emr_rc != EACCES) {
			rc = req.emr_rc;
			goto fail1;
		}
	}

	/*
	 * Apply the filters for the MAC configuration.
	 * If the NIC isn't ready to accept filters this may
	 * return success without setting anything.
	 */
	rc = efx_filter_reconfigure(enp, epp->ep_mac_addr,
				    epp->ep_all_unicst, epp->ep_mulcst,
				    epp->ep_all_mulcst, epp->ep_brdcst,
				    epp->ep_mulcst_addr_list,
				    epp->ep_mulcst_addr_count);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
ef10_mac_multicast_list_set(
	__in				efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	const efx_mac_ops_t *emop = epp->ep_emop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		    enp->en_family == EFX_FAMILY_MEDFORD);

	if ((rc = emop->emo_reconfigure(enp)) != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
ef10_mac_filter_default_rxq_set(
	__in		efx_nic_t *enp,
	__in		efx_rxq_t *erp,
	__in		boolean_t using_rss)
{
	efx_port_t *epp = &(enp->en_port);
	efx_rxq_t *old_rxq;
	boolean_t old_using_rss;
	efx_rc_t rc;

	ef10_filter_get_default_rxq(enp, &old_rxq, &old_using_rss);

	ef10_filter_default_rxq_set(enp, erp, using_rss);

	rc = efx_filter_reconfigure(enp, epp->ep_mac_addr,
				    epp->ep_all_unicst, epp->ep_mulcst,
				    epp->ep_all_mulcst, epp->ep_brdcst,
				    epp->ep_mulcst_addr_list,
				    epp->ep_mulcst_addr_count);

	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	ef10_filter_default_rxq_set(enp, old_rxq, old_using_rss);

	return (rc);
}

			void
ef10_mac_filter_default_rxq_clear(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);

	ef10_filter_default_rxq_clear(enp);

	efx_filter_reconfigure(enp, epp->ep_mac_addr,
				    epp->ep_all_unicst, epp->ep_mulcst,
				    epp->ep_all_mulcst, epp->ep_brdcst,
				    epp->ep_mulcst_addr_list,
				    epp->ep_mulcst_addr_count);
}


#endif	/* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */
