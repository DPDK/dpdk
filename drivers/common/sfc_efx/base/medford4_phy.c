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
	uint32_t fcntl;
	efx_rc_t rc;

	rc = efx_np_link_state(enp, nph, &ls);
	if (rc != 0)
		goto fail1;

	elsp->epls.epls_adv_cap_mask = ls.enls_adv_cap_mask;
	elsp->epls.epls_lp_cap_mask = ls.enls_lp_cap_mask;
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
#endif	/* EFSYS_OPT_MEDFORD4 */
