/*
 * Copyright (c) 2009-2016 Solarflare Communications Inc.
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

#if EFSYS_OPT_SIENA

	__checkReturn	efx_rc_t
siena_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep)
{
	efx_port_t *epp = &(enp->en_port);
	siena_link_state_t sls;
	efx_rc_t rc;

	if ((rc = siena_phy_get_link(enp, &sls)) != 0)
		goto fail1;

	epp->ep_adv_cap_mask = sls.sls_adv_cap_mask;
	epp->ep_fcntl = sls.sls_fcntl;

	*link_modep = sls.sls_link_mode;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	*link_modep = EFX_LINK_UNKNOWN;

	return (rc);
}

	__checkReturn	efx_rc_t
siena_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp)
{
	siena_link_state_t sls;
	efx_rc_t rc;

	/*
	 * Because Siena doesn't *require* polling, we can't rely on
	 * siena_mac_poll() being executed to populate epp->ep_mac_up.
	 */
	if ((rc = siena_phy_get_link(enp, &sls)) != 0)
		goto fail1;

	*mac_upp = sls.sls_mac_up;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn	efx_rc_t
siena_mac_reconfigure(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_oword_t multicast_hash[2];
	efx_mcdi_req_t req;
	uint8_t payload[MAX(MAX(MC_CMD_SET_MAC_IN_LEN,
				MC_CMD_SET_MAC_OUT_LEN),
			    MAX(MC_CMD_SET_MCAST_HASH_IN_LEN,
				MC_CMD_SET_MCAST_HASH_OUT_LEN))];
	unsigned int fcntl;
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
	MCDI_IN_POPULATE_DWORD_2(req, SET_MAC_IN_REJECT,
			    SET_MAC_IN_REJECT_UNCST, !epp->ep_all_unicst,
			    SET_MAC_IN_REJECT_BRDCST, !epp->ep_brdcst);

	if (epp->ep_fcntl_autoneg)
		/* efx_fcntl_set() has already set the phy capabilities */
		fcntl = MC_CMD_FCNTL_AUTO;
	else if (epp->ep_fcntl & EFX_FCNTL_RESPOND)
		fcntl = (epp->ep_fcntl & EFX_FCNTL_GENERATE)
			? MC_CMD_FCNTL_BIDIR
			: MC_CMD_FCNTL_RESPOND;
	else
		fcntl = MC_CMD_FCNTL_OFF;

	MCDI_IN_SET_DWORD(req, SET_MAC_IN_FCNTL, fcntl);

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	/* Push multicast hash */

	if (epp->ep_all_mulcst) {
		/* A hash matching all multicast is all 1s */
		EFX_SET_OWORD(multicast_hash[0]);
		EFX_SET_OWORD(multicast_hash[1]);
	} else if (epp->ep_mulcst) {
		/* Use the hash set by the multicast list */
		multicast_hash[0] = epp->ep_multicst_hash[0];
		multicast_hash[1] = epp->ep_multicst_hash[1];
	} else {
		/* A hash matching no traffic is simply 0 */
		EFX_ZERO_OWORD(multicast_hash[0]);
		EFX_ZERO_OWORD(multicast_hash[1]);
	}

	/*
	 * Broadcast packets go through the multicast hash filter.
	 * The IEEE 802.3 CRC32 of the broadcast address is 0xbe2612ff
	 * so we always add bit 0xff to the mask (bit 0x7f in the
	 * second octword).
	 */
	if (epp->ep_brdcst) {
		/*
		 * NOTE: due to constant folding, some of this evaluates
		 * to null expressions, giving E_EXPR_NULL_EFFECT during
		 * lint on Illumos.  No good way to fix this without
		 * explicit coding the individual word/bit setting.
		 * So just suppress lint for this one line.
		 */
		/* LINTED */
		EFX_SET_OWORD_BIT(multicast_hash[1], 0x7f);
	}

	(void) memset(payload, 0, sizeof (payload));
	req.emr_cmd = MC_CMD_SET_MCAST_HASH;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_SET_MCAST_HASH_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_SET_MCAST_HASH_OUT_LEN;

	memcpy(MCDI_IN2(req, uint8_t, SET_MCAST_HASH_IN_HASH0),
	    multicast_hash, sizeof (multicast_hash));

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
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
siena_mac_pdu_get(
	__in		efx_nic_t *enp,
	__out		size_t *pdu)
{
	return (ENOTSUP);
}

#endif	/* EFSYS_OPT_SIENA */
