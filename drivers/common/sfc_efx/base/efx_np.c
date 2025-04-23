/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Advanced Micro Devices, Inc.
 */
#include "efx.h"
#include "efx_impl.h"

			boolean_t
efx_np_supported(
	__in		efx_nic_t *enp)
{
	return (enp->en_family >= EFX_FAMILY_MEDFORD4) ? B_TRUE : B_FALSE;
}

static				void
efx_np_assign_legacy_props(
	__in			efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

	memset(encp->enc_phy_revision, 0, sizeof (encp->enc_phy_revision));
	encp->enc_phy_type = 0;

#if EFSYS_OPT_NAMES
	memset(encp->enc_phy_name, 0, sizeof (encp->enc_phy_name));
#endif /* EFSYS_OPT_NAMES */

#if EFSYS_OPT_PHY_STATS
	encp->enc_mcdi_phy_stat_mask = 0;
#endif /* EFSYS_OPT_PHY_STATS */

#if EFSYS_OPT_PHY_FLAGS
	encp->enc_phy_flags_mask = 0;
#endif /* EFSYS_OPT_PHY_FLAGS */

#if EFSYS_OPT_BIST
	encp->enc_bist_mask = 0;
#endif /* EFSYS_OPT_BIST */

	encp->enc_mcdi_mdio_channel = 0;
	encp->enc_port = 0;
}

	__checkReturn	efx_rc_t
efx_np_attach(
	__in		efx_nic_t *enp)
{
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	efx_port_t *epp = &(enp->en_port);

	if (efx_np_supported(enp) == B_FALSE)
		return (0);

	/*
	 * Some EFX properties are mostly leftover from Siena era
	 * and we prefer to initialise those to harmless defaults.
	 */
	efx_np_assign_legacy_props(enp);

#if EFSYS_OPT_PHY_LED_CONTROL
	encp->enc_led_mask = 1U << EFX_PHY_LED_DEFAULT;
#endif /* EFSYS_OPT_PHY_LED_CONTROL */

	epp->ep_fixed_port_type = EFX_PHY_MEDIA_INVALID;
	epp->ep_module_type = EFX_PHY_MEDIA_INVALID;
	return (0);
}

		void
efx_np_detach(
	__in	efx_nic_t *enp)
{
	if (efx_np_supported(enp) == B_FALSE)
		return;
}
