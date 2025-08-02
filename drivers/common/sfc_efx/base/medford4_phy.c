/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Advanced Micro Devices, Inc.
 */
#include "efx.h"
#include "efx_impl.h"
#include "medford4_impl.h"

#if EFSYS_OPT_MEDFORD4
	__checkReturn	efx_rc_t
medford4_phy_power(
	__in		efx_nic_t *enp,
	__in		boolean_t power)
{
	if (power)
		enp->en_reset_flags |= EFX_RESET_PHY;

	return (0);
}

	__checkReturn	efx_rc_t
medford4_phy_verify(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
	return (0);
}

	__checkReturn	efx_rc_t
medford4_phy_get_link(
	__in		efx_nic_t *enp,
	__out		ef10_link_state_t *elsp)
{
	efx_np_handle_t nph = enp->en_port.ep_np_handle;
	efx_np_link_state_t ls;
	efx_np_mac_state_t ms;
	efx_rc_t rc;

	rc = efx_np_link_state(enp, nph, &ls);
	if (rc != 0)
		goto fail1;

	elsp->epls.epls_adv_cap_mask = ls.enls_adv_cap_mask;
	elsp->epls.epls_lp_cap_mask = ls.enls_lp_cap_mask;
	elsp->epls.epls_lane_count = ls.enls_lane_count;
	elsp->els_loopback = ls.enls_loopback;

	rc = efx_np_mac_state(enp, nph, &ms);
	if (rc != 0)
		goto fail2;

	elsp->els_mac_up = ms.enms_up;

	mcdi_phy_decode_link_mode(enp, ls.enls_fd, ls.enls_up, ls.enls_speed,
			    ms.enms_fcntl, ls.enls_fec,
			    &elsp->epls.epls_link_mode,
			    &elsp->epls.epls_fcntl,
			    &elsp->epls.epls_fec);

	elsp->epls.epls_ld_cap_mask = 0;
	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
medford4_phy_link_state_get(
	__in			efx_nic_t *enp,
	__out			efx_phy_link_state_t *eplsp)
{
	ef10_link_state_t els;
	efx_rc_t rc;

	rc = medford4_phy_get_link(enp, &els);
	if (rc != 0)
		goto fail1;

	*eplsp = els.epls;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
medford4_phy_reconfigure(
	__in			efx_nic_t *enp)
{
	efx_link_mode_t loopback_link_mode;
	efx_port_t *epp = &(enp->en_port);
	efx_loopback_type_t loopback;
	efx_phy_led_mode_t led;
	boolean_t supported;
	efx_rc_t rc;

	rc = efx_mcdi_link_control_supported(enp, &supported);
	if (rc != 0)
		goto fail1;

	if (supported == B_FALSE)
		goto exit;

#if EFSYS_OPT_LOOPBACK
	loopback_link_mode = epp->ep_loopback_link_mode;
	loopback = epp->ep_loopback_type;
#else /* ! EFSYS_OPT_LOOPBACK */
	loopback_link_mode = EFX_LINK_UNKNOWN;
	loopback = EFX_LOOPBACK_OFF;
#endif /* EFSYS_OPT_LOOPBACK */

	rc = efx_np_link_ctrl(enp, epp->ep_np_handle, epp->ep_np_cap_data_raw,
		    loopback_link_mode, loopback, epp->ep_np_lane_count_req,
		    epp->ep_adv_cap_mask, epp->ep_fcntl_autoneg);
	if (rc != 0)
		goto fail2;

#if EFSYS_OPT_PHY_LED_CONTROL
	led = epp->ep_phy_led_mode;
#else /* ! EFSYS_OPT_PHY_LED_CONTROL */
	led = EFX_PHY_LED_DEFAULT;
#endif /* EFSYS_OPT_PHY_LED_CONTROL */

	rc = efx_mcdi_phy_set_led(enp, led);
	if (rc != 0) {
		/*
		 * If LED control is not supported by firmware, we can
		 * silently ignore default mode set failure
		 * (see FWRIVERHD-198).
		 */
		if (rc == EOPNOTSUPP && led == EFX_PHY_LED_DEFAULT)
			goto exit;

		goto fail3;
	}

exit:
	return (0);

fail3:
	EFSYS_PROBE(fail3);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif	/* EFSYS_OPT_MEDFORD4 */
