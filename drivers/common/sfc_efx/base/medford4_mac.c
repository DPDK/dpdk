/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Advanced Micro Devices, Inc.
 */
#include "efx.h"
#include "efx_impl.h"
#include "medford4_impl.h"

#if EFSYS_OPT_MEDFORD4
	__checkReturn	efx_rc_t
medford4_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep)
{
	efx_port_t *epp = &(enp->en_port);
	ef10_link_state_t els;
	efx_rc_t rc;

	rc = medford4_phy_get_link(enp, &els);
	if (rc != 0)
		goto fail1;

	epp->ep_adv_cap_mask = els.epls.epls_adv_cap_mask;
	epp->ep_fcntl = els.epls.epls_fcntl;

	*link_modep = els.epls.epls_link_mode;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	*link_modep = EFX_LINK_UNKNOWN;
	return (rc);
}

	__checkReturn	efx_rc_t
medford4_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp)
{
	ef10_link_state_t els;
	efx_rc_t rc;

	rc = medford4_phy_get_link(enp, &els);
	if (rc != 0)
		goto fail1;

	*mac_upp = els.els_mac_up;
	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif /* EFSYS_OPT_MEDFORD4 */
