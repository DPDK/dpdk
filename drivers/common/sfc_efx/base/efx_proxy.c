/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
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
	NULL,			/* epo_set_privilege_mask */
	NULL,			/* epo_complete_request */
	NULL,			/* epo_exec_cmd */
	NULL,			/* epo_get_privilege_mask */
};
#endif /* EFSYS_OPT_SIENA */

#if EFX_OPTS_EF10()
static const efx_proxy_ops_t			__efx_proxy_ef10_ops = {
	ef10_proxy_auth_init,			/* epo_init */
	ef10_proxy_auth_fini,			/* epo_fini */
	ef10_proxy_auth_mc_config,		/* epo_mc_config */
	ef10_proxy_auth_disable,		/* epo_disable */
	ef10_proxy_auth_privilege_modify,	/* epo_privilege_modify */
	ef10_proxy_auth_set_privilege_mask,	/* epo_set_privilege_mask */
	ef10_proxy_auth_complete_request,	/* epo_complete_request */
	ef10_proxy_auth_exec_cmd,		/* epo_exec_cmd */
	ef10_proxy_auth_get_privilege_mask,	/* epo_get_privilege_mask */
};
#endif /* EFX_OPTS_EF10() */

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

	__checkReturn	efx_rc_t
efx_proxy_auth_complete_request(
	__in		efx_nic_t *enp,
	__in		uint32_t fn_index,
	__in		uint32_t proxy_result,
	__in		uint32_t handle)
{
	const efx_proxy_ops_t *epop = enp->en_epop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if (epop->epo_complete_request == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	rc = epop->epo_complete_request(enp, fn_index, proxy_result, handle);
	if (rc != 0)
		goto fail2;

	return (0);
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_proxy_auth_exec_cmd(
	__in		efx_nic_t *enp,
	__inout		efx_proxy_cmd_params_t *paramsp)
{
	const efx_proxy_ops_t *epop = enp->en_epop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if (paramsp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	if (epop->epo_exec_cmd == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}

	rc = epop->epo_exec_cmd(enp, paramsp);
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
efx_proxy_auth_set_privilege_mask(
	__in		efx_nic_t *enp,
	__in		uint32_t vf_index,
	__in		uint32_t mask,
	__in		uint32_t value)
{
	const efx_proxy_ops_t *epop = enp->en_epop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if (epop->epo_set_privilege_mask == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	rc = epop->epo_set_privilege_mask(enp, vf_index, mask, value);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_proxy_auth_privilege_mask_get(
	__in		efx_nic_t *enp,
	__in		uint32_t pf_index,
	__in		uint32_t vf_index,
	__out		uint32_t *maskp)
{
	const efx_proxy_ops_t *epop = enp->en_epop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if (epop->epo_get_privilege_mask == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	rc = epop->epo_get_privilege_mask(enp, pf_index, vf_index, maskp);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
efx_proxy_auth_privilege_modify(
	__in		efx_nic_t *enp,
	__in		uint32_t pf_index,
	__in		uint32_t vf_index,
	__in		uint32_t add_privileges_mask,
	__in		uint32_t remove_privileges_mask)
{
	const efx_proxy_ops_t *epop = enp->en_epop;
	efx_rc_t rc;

	EFSYS_ASSERT(enp->en_mod_flags & EFX_MOD_PROXY);

	if (epop->epo_privilege_modify == NULL) {
		rc = ENOTSUP;
		goto fail1;
	}

	rc = epop->epo_privilege_modify(enp, MC_CMD_PRIVILEGE_MODIFY_IN_ONE,
		    pf_index, vf_index, add_privileges_mask,
		    remove_privileges_mask);
	if (rc != 0)
		goto fail2;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#endif /* EFSYS_OPT_MCDI_PROXY_AUTH_SERVER */
