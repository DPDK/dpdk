/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
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
	NULL,		/* eeo_vport_reconfigure */
	NULL,		/* eeo_vport_stats */
};
#endif /* EFSYS_OPT_SIENA */

#if EFX_OPTS_EF10()
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
	ef10_evb_vport_reconfigure,	/* eeo_vport_reconfigure */
	ef10_evb_vport_stats,		/* eeo_vport_stats */
};
#endif /* EFX_OPTS_EF10() */

#if EFSYS_OPT_RIVERHEAD
static const efx_evb_ops_t	__efx_evb_rhead_ops = {
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
	ef10_evb_vport_reconfigure,	/* eeo_vport_reconfigure */
	ef10_evb_vport_stats,		/* eeo_vport_stats */
};
#endif /* EFSYS_OPT_RIVERHEAD */

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

#if EFSYS_OPT_RIVERHEAD
	case EFX_FAMILY_RIVERHEAD:
		eeop = &__efx_evb_rhead_ops;
		break;
#endif /* EFSYS_OPT_RIVERHEAD */

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

/*
 * efx_is_zero_eth_addr returns TRUE if the passed MAC address has all bytes
 * equal to zero. A vport is assigned a MAC address after creation and this
 * function checks if that has happened. It is called in the clean-up function
 * before calling eeo_vport_mac_addr_del to ensure that the vport actually had
 * an allocated MAC address.
 */

__checkReturn				boolean_t
efx_is_zero_eth_addr(
	__in_bcount(EFX_MAC_ADDR_LEN)	const uint8_t *addrp)
{
	return (!(addrp[0] | addrp[1] | addrp[2] |
		addrp[3] | addrp[4] | addrp[5]));
}

static			void
efx_evb_free_vport(
	__in		efx_nic_t *enp,
	__in		efx_vswitch_id_t vswitch_id,
	__inout		efx_vport_config_t *configp)
{
	const efx_evb_ops_t *eeop = enp->en_eeop;

	/* If any callback fails, continue clean-up with others functions */
	if (EFX_VPORT_PCI_FUNCTION_IS_PF(configp)) {
		/* free vadaptor */
		if ((configp->evc_vport_id != EFX_VPORT_ID_INVALID) &&
		    (eeop->eeo_vadaptor_free(enp, vswitch_id,
		    configp->evc_vport_id) != 0)) {
			EFSYS_PROBE2(eeo_vadaptor_free,
			    uint16_t, configp->evc_function,
			    uint32_t, configp->evc_vport_id);
		}
	} else {
		if (configp->evc_vport_assigned == B_TRUE) {
			if (eeop->eeo_vport_assign(enp, vswitch_id,
			    EVB_PORT_ID_NULL,
			    configp->evc_function) != 0) {
				EFSYS_PROBE1(eeo_vport_assign,
				    uint16_t, configp->evc_function);
			}
			configp->evc_vport_assigned = B_FALSE;
		}
	}

	/*
	 * Call eeo_vport_mac_addr_del after checking that this vport is
	 * actually allocated a MAC address in call to efx_evb_configure_vport
	 */
	if (!efx_is_zero_eth_addr(configp->evc_mac_addr)) {
		if (eeop->eeo_vport_mac_addr_del(enp, vswitch_id,
		    configp->evc_vport_id,
		    configp->evc_mac_addr) != 0) {
			EFSYS_PROBE1(eeo_vport_mac_addr_del,
			    uint16_t, configp->evc_function);
		}
		memset(configp->evc_mac_addr, 0x00, EFX_MAC_ADDR_LEN);
	}

	if (configp->evc_vport_id != EFX_VPORT_ID_INVALID) {
		if (eeop->eeo_vport_free(enp, vswitch_id,
		    configp->evc_vport_id) != 0) {
			EFSYS_PROBE1(eeo_vport_free,
			    uint16_t, configp->evc_function);
		}
		configp->evc_vport_id = EFX_VPORT_ID_INVALID;
	}
}

static					void
efx_evb_free_vports(
	__in				efx_nic_t *enp,
	__in				efx_vswitch_id_t vswitch_id,
	__in				uint32_t num_vports,
	__inout_ecount(num_vports)	efx_vport_config_t *vport_configp)
{
	efx_vport_config_t *configp;
	uint32_t i;

	if (vport_configp == NULL) {
		EFSYS_PROBE(null_vport_config);
		return;
	}

	for (i = 0; i < num_vports; i++) {
		configp = vport_configp + i;
		efx_evb_free_vport(enp, vswitch_id, configp);
	}
}

static	__checkReturn	efx_rc_t
efx_evb_configure_vport(
	__in		efx_nic_t *enp,
	__in		efx_vswitch_id_t vswitch_id,
	__in		const efx_evb_ops_t *eeop,
	__inout		efx_vport_config_t *configp)
{
	efx_rc_t rc;
	efx_vport_id_t vport_id;

	if ((rc = eeop->eeo_vport_alloc(enp, vswitch_id,
			EFX_VPORT_TYPE_NORMAL, configp->evc_vid,
			configp->evc_vlan_restrict, &vport_id)) != 0)
		goto fail1;

	configp->evc_vport_id = vport_id;

	if ((rc = eeop->eeo_vport_mac_addr_add(enp, vswitch_id,
			configp->evc_vport_id,
			configp->evc_mac_addr)) != 0)
		goto fail2;

	if (EFX_VPORT_PCI_FUNCTION_IS_PF(configp)) {
		if ((rc = eeop->eeo_vadaptor_alloc(enp, vswitch_id,
				configp->evc_vport_id)) != 0)
			goto fail3;
	} else {
		if ((rc = eeop->eeo_vport_assign(enp, vswitch_id,
				configp->evc_vport_id,
				configp->evc_function)) != 0)
			goto fail4;
		configp->evc_vport_assigned = B_TRUE;
	}

	return (0);

fail4:
	EFSYS_PROBE(fail4);
fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
efx_evb_vswitch_create(
	__in				efx_nic_t *enp,
	__in				uint32_t num_vports,
	__inout_ecount(num_vports)	efx_vport_config_t *vport_configp,
	__deref_out			efx_vswitch_t **evpp)
{
	efx_vswitch_t *evp;
	efx_rc_t rc;
	efx_vswitch_id_t vswitch_id;
	efx_vport_config_t *configp;
	const efx_evb_ops_t *eeop = enp->en_eeop;
	uint32_t i;

	/* vport_configp is a caller allocated array filled in with vports
	 * configuration. Index 0 carries the PF vport configuration and next
	 * num_vports - 1 indices carry VFs configuration.
	 */
	EFSYS_ASSERT((num_vports != 0) && (vport_configp != NULL) &&
		(evpp != NULL));
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_EVB);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_NIC));

	if ((eeop->eeo_vswitch_alloc == NULL) ||
	    (eeop->eeo_vport_alloc == NULL) ||
	    (eeop->eeo_vport_free == NULL) ||
	    (eeop->eeo_vport_mac_addr_add == NULL) ||
	    (eeop->eeo_vport_mac_addr_del == NULL) ||
	    (eeop->eeo_vadaptor_alloc == NULL) ||
	    (eeop->eeo_vadaptor_free == NULL) ||
	    (eeop->eeo_vport_assign == NULL) ||
	    (eeop->eeo_vswitch_free == NULL)) {
		rc = ENOTSUP;
		goto fail1;
	}

	/* Allocate a vSwitch object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_vswitch_t), evp);

	if (evp == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	if ((rc = eeop->eeo_vswitch_alloc(enp, &vswitch_id)) != 0)
		goto fail3;

	evp->ev_enp = enp;
	evp->ev_num_vports = num_vports;
	evp->ev_evcp = vport_configp;
	evp->ev_vswitch_id = vswitch_id;

	for (i = 0; i < num_vports; i++) {
		configp = vport_configp + i;

		if ((rc = efx_evb_configure_vport(enp, vswitch_id, eeop,
				configp)) != 0)
			goto fail4;
	}

	enp->en_vswitchp = evp;
	*evpp = evp;
	return (0);

fail4:
	EFSYS_PROBE(fail4);
	efx_evb_free_vports(enp, vswitch_id, i + 1, vport_configp);
	/* Free the vSwitch */
	eeop->eeo_vswitch_free(enp, vswitch_id);

fail3:
	EFSYS_PROBE(fail3);
	/* Free the vSwitch object */
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_vswitch_t), evp);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn			efx_rc_t
efx_evb_vport_mac_set(
	__in				efx_nic_t *enp,
	__in				efx_vswitch_t *evp,
	__in				efx_vport_id_t vport_id,
	__in_bcount(EFX_MAC_ADDR_LEN)	uint8_t *addrp)
{
	const efx_evb_ops_t *eeop = enp->en_eeop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_EVB);

	if (eeop->eeo_vport_reconfigure == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (addrp == NULL) {
		rc = EINVAL;
		goto fail2;
	}

	rc = eeop->eeo_vport_reconfigure(enp, evp->ev_vswitch_id, vport_id,
		NULL, addrp, NULL);
	if (rc != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_evb_vport_vlan_set(
	__in		efx_nic_t *enp,
	__in		efx_vswitch_t *evp,
	__in		efx_vport_id_t vport_id,
	__in		uint16_t vid)
{
	const efx_evb_ops_t *eeop = enp->en_eeop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_EVB);

	if (eeop->eeo_vport_reconfigure == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	rc = eeop->eeo_vport_reconfigure(enp, evp->ev_vswitch_id, vport_id,
		&vid, NULL, NULL);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_evb_vport_reset(
	__in				efx_nic_t *enp,
	__in				efx_vswitch_t *evp,
	__in				efx_vport_id_t vport_id,
	__in_bcount(EFX_MAC_ADDR_LEN)	uint8_t *addrp,
	__in				uint16_t vid,
	__out				boolean_t *is_fn_resetp)
{
	const efx_evb_ops_t *eeop = enp->en_eeop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_EVB);

	if (eeop->eeo_vport_reconfigure == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (is_fn_resetp == NULL) {
		rc = EINVAL;
		goto fail2;
	}

	rc = eeop->eeo_vport_reconfigure(enp, evp->ev_vswitch_id, vport_id,
		&vid, addrp, is_fn_resetp);
	if (rc != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}
	__checkReturn	efx_rc_t
efx_evb_vswitch_destroy(
	__in		efx_nic_t *enp,
	__in		efx_vswitch_t *evp)
{
	const efx_evb_ops_t *eeop = enp->en_eeop;
	efx_vswitch_id_t vswitch_id;
	efx_rc_t rc;

	EFSYS_ASSERT(evp != NULL);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_EVB);

	if ((eeop->eeo_vport_mac_addr_del == NULL) ||
	    (eeop->eeo_vadaptor_free == NULL) ||
	    (eeop->eeo_vport_assign == NULL) ||
	    (eeop->eeo_vport_free == NULL) ||
	    (eeop->eeo_vswitch_free == NULL)) {
		rc = ENOTSUP;
		goto fail1;
	}

	vswitch_id  = evp->ev_vswitch_id;
	efx_evb_free_vports(enp, vswitch_id,
		evp->ev_num_vports, evp->ev_evcp);

	/* Free the vSwitch object */
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_vswitch_t), evp);
	enp->en_vswitchp = NULL;

	/* Free the vSwitch */
	if ((rc = eeop->eeo_vswitch_free(enp, vswitch_id)) != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_evb_vport_stats(
	__in		efx_nic_t *enp,
	__in		efx_vswitch_t *evp,
	__in		efx_vport_id_t vport_id,
	__out		efsys_mem_t *stats_bufferp)
{
	efx_rc_t rc;
	const efx_evb_ops_t *eeop = enp->en_eeop;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_EVB);

	if (eeop->eeo_vport_stats == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	if (stats_bufferp == NULL) {
		rc = EINVAL;
		goto fail2;
	}

	rc = eeop->eeo_vport_stats(enp, evp->ev_vswitch_id,
		vport_id, stats_bufferp);
	if (rc != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#endif
