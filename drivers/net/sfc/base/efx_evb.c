/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018-2019 Solarflare Communications Inc.
 * All rights reserved.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_EVB

#if EFSYS_OPT_SIENA
static const efx_evb_ops_t	__efx_evb_dummy_ops = {
	NULL,		/* eeo_init */
	NULL,		/* eeo_fini */
	NULL,		/* eeo_vswitch_alloc */
	NULL,		/* eeo_vswitch_free */
	NULL,		/* eeo_vport_alloc */
	NULL,		/* eeo_vport_free */
	NULL,		/* eeo_vport_mac_addr_add */
	NULL,		/* eeo_vport_mac_addr_del */
	NULL,		/* eeo_vadaptor_alloc */
	NULL,		/* eeo_vadaptor_free */
	NULL,		/* eeo_vport_assign */
};
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2
static const efx_evb_ops_t	__efx_evb_ef10_ops = {
	ef10_evb_init,			/* eeo_init */
	ef10_evb_fini,			/* eeo_fini */
	ef10_evb_vswitch_alloc,		/* eeo_vswitch_alloc */
	ef10_evb_vswitch_free,		/* eeo_vswitch_free */
	ef10_evb_vport_alloc,		/* eeo_vport_alloc */
	ef10_evb_vport_free,		/* eeo_vport_free */
	ef10_evb_vport_mac_addr_add,	/* eeo_vport_mac_addr_add */
	ef10_evb_vport_mac_addr_del,	/* eeo_vport_mac_addr_del */
	ef10_evb_vadaptor_alloc,	/* eeo_vadaptor_alloc */
	ef10_evb_vadaptor_free,		/* eeo_vadaptor_free */
	ef10_evb_vport_assign,		/* eeo_vport_assign */
};
#endif /* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2 */

	__checkReturn	efx_rc_t
efx_evb_init(
	__in		efx_nic_t *enp)
{
	const efx_evb_ops_t *eeop;
	efx_rc_t rc;
	efx_nic_cfg_t *encp = &(enp->en_nic_cfg);

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_EVB));

	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		eeop = &__efx_evb_dummy_ops;
		break;
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		eeop = &__efx_evb_ef10_ops;
		break;
#endif /* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		eeop = &__efx_evb_ef10_ops;
		break;
#endif /* EFSYS_OPT_MEDFORD */

#if EFSYS_OPT_MEDFORD2
	case EFX_FAMILY_MEDFORD2:
		eeop = &__efx_evb_ef10_ops;
		break;
#endif /* EFSYS_OPT_MEDFORD2 */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	if (!encp->enc_datapath_cap_evb || !eeop->eeo_init) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((rc = eeop->eeo_init(enp)) != 0)
		goto fail3;

	enp->en_eeop = eeop;
	enp->en_mod_flags |= EFX_MOD_EVB;
	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	void
efx_evb_fini(
	__in		efx_nic_t *enp)
{
	const efx_evb_ops_t *eeop = enp->en_eeop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_RX));
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_TX));

	if (eeop && eeop->eeo_fini)
		eeop->eeo_fini(enp);

	enp->en_eeop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_EVB;
}

#endif
