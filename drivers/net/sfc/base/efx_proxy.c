/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018-2019 Solarflare Communications Inc.
 * All rights reserved.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_MCDI_PROXY_AUTH_SERVER

#if EFSYS_OPT_SIENA
static const efx_proxy_ops_t	__efx_proxy_dummy_ops = {
	NULL,			/* epo_init */
	NULL,			/* epo_fini */
	NULL,			/* epo_mc_config */
	NULL,			/* epo_disable */
	NULL,			/* epo_privilege_modify */
};
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2
static const efx_proxy_ops_t			__efx_proxy_ef10_ops = {
	ef10_proxy_auth_init,			/* epo_init */
	ef10_proxy_auth_fini,			/* epo_fini */
	ef10_proxy_auth_mc_config,		/* epo_mc_config */
	ef10_proxy_auth_disable,		/* epo_disable */
	ef10_proxy_auth_privilege_modify,	/* epo_privilege_modify */
};
#endif /* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2 */

	__checkReturn	efx_rc_t
efx_proxy_auth_init(
	__in		efx_nic_t *enp)
{
	const efx_proxy_ops_t *epop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_PROXY));

	switch (enp->en_family) {
#if EFSYS_OPT_SIENA
	case EFX_FAMILY_SIENA:
		epop = &__efx_proxy_dummy_ops;
		break;
#endif /* EFSYS_OPT_SIENA */

#if EFSYS_OPT_HUNTINGTON
	case EFX_FAMILY_HUNTINGTON:
		epop = &__efx_proxy_ef10_ops;
		break;
#endif /* EFSYS_OPT_HUNTINGTON */

#if EFSYS_OPT_MEDFORD
	case EFX_FAMILY_MEDFORD:
		epop = &__efx_proxy_ef10_ops;
		break;
#endif /* EFSYS_OPT_MEDFORD */

#if EFSYS_OPT_MEDFORD2
	case EFX_FAMILY_MEDFORD2:
		epop = &__efx_proxy_ef10_ops;
		break;
#endif /* EFSYS_OPT_MEDFORD2 */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	if (epop->epo_init == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((rc = epop->epo_init(enp)) != 0)
		goto fail3;

	enp->en_epop = epop;
	enp->en_mod_flags |= EFX_MOD_PROXY;
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
efx_proxy_auth_fini(
	__in		efx_nic_t *enp)
{
	const efx_proxy_ops_t *epop = enp->en_epop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROBE);
	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if ((epop != NULL) && (epop->epo_fini != NULL))
		epop->epo_fini(enp);

	enp->en_epop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_PROXY;
}

	__checkReturn	efx_rc_t
efx_proxy_auth_configure(
	__in		efx_nic_t *enp,
	__in		efx_proxy_auth_config_t *configp)
{
	const efx_proxy_ops_t *epop = enp->en_epop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if ((configp == NULL) ||
	    (configp->request_bufferp == NULL) ||
	    (configp->response_bufferp == NULL) ||
	    (configp->status_bufferp == NULL) ||
	    (configp->op_listp == NULL) ||
	    (configp->block_cnt == 0)) {
		rc = EINVAL;
		goto fail1;
	}

	if ((epop->epo_mc_config == NULL) ||
	    (epop->epo_privilege_modify == NULL)) {
		rc = ENOTSUP;
		goto fail2;
	}

	rc = epop->epo_mc_config(enp, configp->request_bufferp,
			configp->response_bufferp, configp->status_bufferp,
			configp->block_cnt, configp->op_listp,
			configp->op_count);
	if (rc != 0)
		goto fail3;

	rc = epop->epo_privilege_modify(enp, MC_CMD_PRIVILEGE_MODIFY_IN_ALL,
			0, 0, 0, configp->handled_privileges);
	if (rc != 0)
		goto fail4;

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

	__checkReturn	efx_rc_t
efx_proxy_auth_destroy(
	__in		efx_nic_t *enp,
	__in		uint32_t handled_privileges)
{
	const efx_proxy_ops_t *epop = enp->en_epop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if ((epop->epo_disable == NULL) ||
	    (epop->epo_privilege_modify == NULL)) {
		rc = ENOTSUP;
		goto fail1;
	}

	rc = epop->epo_privilege_modify(enp, MC_CMD_PRIVILEGE_MODIFY_IN_ALL,
		0, 0, handled_privileges, 0);
	if (rc != 0)
		goto fail2;

	rc = epop->epo_disable(enp);
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

#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */
