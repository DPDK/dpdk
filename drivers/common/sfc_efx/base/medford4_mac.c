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

	__checkReturn		efx_rc_t
medford4_mac_reconfigure(
	__in			efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_np_mac_ctrl_t mc = {0};
	efx_rc_t rc;

	mc.enmc_fcntl_autoneg = epp->ep_fcntl_autoneg;
	mc.enmc_include_fcs = epp->ep_include_fcs;
	mc.enmc_fcntl = epp->ep_fcntl;

	rc = efx_np_mac_ctrl(enp, epp->ep_np_handle, &mc);
	if (rc != 0)
		goto fail1;

	/*
	 * Apply the filters for the MAC configuration. If the NIC isn't ready
	 * to accept filters, this may return success without setting anything.
	 */
	rc = efx_filter_reconfigure(enp, epp->ep_mac_addr,
				    epp->ep_all_unicst, epp->ep_mulcst,
				    epp->ep_all_mulcst, epp->ep_brdcst,
				    epp->ep_mulcst_addr_list,
				    epp->ep_mulcst_addr_count);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
#endif /* EFSYS_OPT_MEDFORD4 */
