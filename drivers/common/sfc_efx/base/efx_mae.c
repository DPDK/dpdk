/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019 Xilinx, Inc. All rights reserved.
 * All rights reserved.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_MAE

static	__checkReturn			efx_rc_t
efx_mae_get_capabilities(
	__in				efx_nic_t *enp)
{
	efx_mcdi_req_t req;
	EFX_MCDI_DECLARE_BUF(payload,
	    MC_CMD_MAE_GET_CAPS_IN_LEN,
	    MC_CMD_MAE_GET_CAPS_OUT_LEN);
	struct efx_mae_s *maep = enp->en_maep;
	efx_rc_t rc;

	req.emr_cmd = MC_CMD_MAE_GET_CAPS;
	req.emr_in_buf = payload;
	req.emr_in_length = MC_CMD_MAE_GET_CAPS_IN_LEN;
	req.emr_out_buf = payload;
	req.emr_out_length = MC_CMD_MAE_GET_CAPS_OUT_LEN;

	efx_mcdi_execute(enp, &req);

	if (req.emr_rc != 0) {
		rc = req.emr_rc;
		goto fail1;
	}

	if (req.emr_out_length_used < MC_CMD_MAE_GET_CAPS_OUT_LEN) {
		rc = EMSGSIZE;
		goto fail2;
	}

	maep->em_max_n_action_prios =
	    MCDI_OUT_DWORD(req, MAE_GET_CAPS_OUT_ACTION_PRIOS);

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_init(
	__in				efx_nic_t *enp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mae_t *maep;
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (*maep), maep);
	if (maep == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	enp->en_maep = maep;

	rc = efx_mae_get_capabilities(enp);
	if (rc != 0)
		goto fail3;

	return (0);

fail3:
	EFSYS_PROBE(fail3);
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (struct efx_mae_s), enp->en_maep);
	enp->en_maep = NULL;
fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

					void
efx_mae_fini(
	__in				efx_nic_t *enp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	efx_mae_t *maep = enp->en_maep;

	if (encp->enc_mae_supported == B_FALSE)
		return;

	EFSYS_KMEM_FREE(enp->en_esip, sizeof (*maep), maep);
	enp->en_maep = NULL;
}

	__checkReturn			efx_rc_t
efx_mae_get_limits(
	__in				efx_nic_t *enp,
	__out				efx_mae_limits_t *emlp)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(enp);
	struct efx_mae_s *maep = enp->en_maep;
	efx_rc_t rc;

	if (encp->enc_mae_supported == B_FALSE) {
		rc = ENOTSUP;
		goto fail1;
	}

	emlp->eml_max_n_action_prios = maep->em_max_n_action_prios;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn			efx_rc_t
efx_mae_match_spec_init(
	__in				efx_nic_t *enp,
	__in				efx_mae_rule_type_t type,
	__in				uint32_t prio,
	__out				efx_mae_match_spec_t **specp)
{
	efx_mae_match_spec_t *spec;
	efx_rc_t rc;

	switch (type) {
	case EFX_MAE_RULE_ACTION:
		break;
	default:
		rc = ENOTSUP;
		goto fail1;
	}

	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (*spec), spec);
	if (spec == NULL) {
		rc = ENOMEM;
		goto fail2;
	}

	spec->emms_type = type;
	spec->emms_prio = prio;

	*specp = spec;

	return (0);

fail2:
	EFSYS_PROBE(fail2);
fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

					void
efx_mae_match_spec_fini(
	__in				efx_nic_t *enp,
	__in				efx_mae_match_spec_t *spec)
{
	EFSYS_KMEM_FREE(enp->en_esip, sizeof (*spec), spec);
}

#endif /* EFSYS_OPT_MAE */
