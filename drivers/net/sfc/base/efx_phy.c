/*
 * Copyright (c) 2007-2016 Solarflare Communications Inc.
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
static const efx_phy_ops_t	__efx_phy_siena_ops = {
	siena_phy_power,		/* epo_power */
	NULL,				/* epo_reset */
	siena_phy_reconfigure,		/* epo_reconfigure */
	siena_phy_verify,		/* epo_verify */
	siena_phy_oui_get,		/* epo_oui_get */
};
#endif	/* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD
static const efx_phy_ops_t	__efx_phy_ef10_ops = {
	ef10_phy_power,			/* epo_power */
	NULL,				/* epo_reset */
	ef10_phy_reconfigure,		/* epo_reconfigure */
	ef10_phy_verify,		/* epo_verify */
	ef10_phy_oui_get,		/* epo_oui_get */
};
#endif	/* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD */

	__checkReturn	efx_rc_t
efx_phy_probe(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);
	const efx_phy_ops_t *epop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	epp->ep_port = encp->enc_port;
	epp->ep_phy_type = encp->enc_phy_type;

	/* Hook in operations structure */
	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		epop = &__efx_phy_siena_ops;
		break;
#endif	/* EFSYS_OPT_SIENA */
#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		epop = &__efx_phy_ef10_ops;
		break;
#endif	/* EFSYS_OPT_HUNTINGTON */
#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		epop = &__efx_phy_ef10_ops;
		break;
#endif	/* EFSYS_OPT_MEDFORD */
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	epp->ep_epop = epop;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	epp->ep_port = 0;
	epp->ep_phy_type = 0;

	return (rc);
}

	__checkReturn	efx_rc_t
efx_phy_verify(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	const efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	return (epop->epo_verify(enp));
}

			void
efx_phy_adv_cap_get(
	__in		efx_nic_t *enp,
	__in		uint32_t flag,
	__out		uint32_t *maskp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);

	switch (flag) {
	case EFX_PHY_CAP_CURRENT:
		*maskp = epp->ep_adv_cap_mask;
		break;
	case EFX_PHY_CAP_DEFAULT:
		*maskp = epp->ep_default_adv_cap_mask;
		break;
	case EFX_PHY_CAP_PERM:
		*maskp = epp->ep_phy_cap_mask;
		break;
	default:
		EFSYS_ASSERT(B_FALSE);
		break;
	}
}

	__checkReturn	efx_rc_t
efx_phy_adv_cap_set(
	__in		efx_nic_t *enp,
	__in		uint32_t mask)
{
	efx_port_t *epp = &(enp->en_port);
	const efx_phy_ops_t *epop = epp->ep_epop;
	uint32_t old_mask;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if ((mask & ~epp->ep_phy_cap_mask) != 0) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (epp->ep_adv_cap_mask == mask)
		goto done;

	old_mask = epp->ep_adv_cap_mask;
	epp->ep_adv_cap_mask = mask;

	if ((rc = epop->epo_reconfigure(enp)) != 0)
		goto fail2;

done:
	return (0);

fail2:
	EFSYS_PROBE(fail2);

	epp->ep_adv_cap_mask = old_mask;
	/* Reconfigure for robustness */
	if (epop->epo_reconfigure(enp) != 0) {
		/*
		 * We may have an inconsistent view of our advertised speed
		 * capabilities.
		 */
		EFSYS_ASSERT(0);
	}

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	void
efx_phy_lp_cap_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *maskp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	*maskp = epp->ep_lp_cap_mask;
}

	__checkReturn	efx_rc_t
efx_phy_oui_get(
	__in		efx_nic_t *enp,
	__out		uint32_t *ouip)
{
	efx_port_t *epp = &(enp->en_port);
	const efx_phy_ops_t *epop = epp->ep_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	return (epop->epo_oui_get(enp, ouip));
}

			void
efx_phy_media_type_get(
	__in		efx_nic_t *enp,
	__out		efx_phy_media_type_t *typep)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PORT);

	if (epp->ep_module_type != EFX_PHY_MEDIA_INVALID)
		*typep = epp->ep_module_type;
	else
		*typep = epp->ep_fixed_port_type;
}

	__checkReturn	efx_rc_t
efx_phy_module_get_info(
	__in			efx_nic_t *enp,
	__in			uint8_t dev_addr,
	__in			uint8_t offset,
	__in			uint8_t len,
	__out_bcount(len)	uint8_t *data)
{
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(data != NULL);

	if ((uint32_t)offset + len > 0xff) {
		rc = EINVAL;
		goto fail1;
	}

	if ((rc = efx_mcdi_phy_module_get_info(enp, dev_addr,
	    offset, len, data)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}


			void
efx_phy_unprobe(
	__in	efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	epp->ep_epop = NULL;

	epp->ep_adv_cap_mask = 0;

	epp->ep_port = 0;
	epp->ep_phy_type = 0;
}
