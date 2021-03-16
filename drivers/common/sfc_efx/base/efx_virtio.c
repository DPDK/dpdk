/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_VIRTIO

#if EFSYS_OPT_RIVERHEAD
static const efx_virtio_ops_t	__efx_virtio_rhead_ops = {
	rhead_virtio_qstart,			/* evo_virtio_qstart */
	rhead_virtio_qstop,			/* evo_virtio_qstop */
	rhead_virtio_get_doorbell_offset,	/* evo_get_doorbell_offset */
	rhead_virtio_get_features,		/* evo_get_features */
	rhead_virtio_verify_features,		/* evo_verify_features */
};
#endif /* EFSYS_OPT_RIVERHEAD */

	__checkReturn	efx_rc_t
efx_virtio_init(
	__in		efx_nic_t *enp)
{
	const efx_virtio_ops_t *evop;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT(!(enp->en_mod_flags & EFX_MOD_VIRTIO));

	switch (enp->en_family) {
#if EFSYS_OPT_RIVERHEAD
	case EFX_FAMILY_RIVERHEAD:
		evop = &__efx_virtio_rhead_ops;
		break;
#endif /* EFSYS_OPT_RIVERHEAD */

	default:
		EFSYS_ASSERT(0);
		rc = ENOTSUP;
		goto fail1;
	}

	enp->en_evop = evop;
	enp->en_mod_flags |= EFX_MOD_VIRTIO;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	enp->en_evop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_VIRTIO;

	return (rc);
}

	void
efx_virtio_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_PROBE);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_VIRTIO);

	enp->en_evop = NULL;
	enp->en_mod_flags &= ~EFX_MOD_VIRTIO;
}

	__checkReturn   efx_rc_t
efx_virtio_qcreate(
	__in		efx_nic_t *enp,
	__deref_out	efx_virtio_vq_t **evvpp)
{
	const efx_virtio_ops_t *evop = enp->en_evop;
	efx_virtio_vq_t *evvp;
	efx_rc_t rc;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_VIRTIO);

	/* Allocate a virtqueue object */
	EFSYS_KMEM_ALLOC(enp->en_esip, sizeof (efx_virtio_vq_t), evvp);
	if (evvp == NULL) {
		rc = ENOMEM;
		goto fail1;
	}

	evvp->evv_magic = EFX_VQ_MAGIC;
	evvp->evv_enp = enp;
	evvp->evv_state = EFX_VIRTIO_VQ_STATE_INITIALIZED;

	*evvpp = evvp;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);

	return (rc);
}

	__checkReturn   efx_rc_t
efx_virtio_qstart(
	__in		efx_virtio_vq_t *evvp,
	__in		efx_virtio_vq_cfg_t *evvcp,
	__in_opt	efx_virtio_vq_dyncfg_t *evvdp)
{
	const efx_virtio_ops_t *evop;
	efx_rc_t rc;

	if ((evvcp == NULL) || (evvp == NULL)) {
		rc = EINVAL;
		goto fail1;
	}

	if (evvp->evv_state != EFX_VIRTIO_VQ_STATE_INITIALIZED) {
		rc = EINVAL;
		goto fail2;
	}

	evop = evvp->evv_enp->en_evop;
	if (evop == NULL) {
		rc = ENOTSUP;
		goto fail3;
	}

	if ((rc = evop->evo_virtio_qstart(evvp, evvcp, evvdp)) != 0)
		goto fail4;

	evvp->evv_type = evvcp->evvc_type;
	evvp->evv_target_vf = evvcp->evvc_target_vf;
	evvp->evv_state = EFX_VIRTIO_VQ_STATE_STARTED;

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

	__checkReturn   efx_rc_t
efx_virtio_qstop(
	__in		efx_virtio_vq_t *evvp,
	__out_opt	efx_virtio_vq_dyncfg_t *evvdp)
{
	efx_nic_t *enp;
	const efx_virtio_ops_t *evop;
	efx_rc_t rc;

	if (evvp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	enp = evvp->evv_enp;
	evop = enp->en_evop;

	EFSYS_ASSERT3U(evvp->evv_magic, ==, EFX_VQ_MAGIC);
	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_VIRTIO);

	if (evop == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}

	if (evvp->evv_state != EFX_VIRTIO_VQ_STATE_STARTED) {
		rc = EINVAL;
		goto fail3;
	}

	if ((rc = evop->evo_virtio_qstop(evvp, evvdp)) != 0)
		goto fail4;

	evvp->evv_state = EFX_VIRTIO_VQ_STATE_INITIALIZED;

	return 0;

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

	void
efx_virtio_qdestroy(
	__in		efx_virtio_vq_t *evvp)
{
	efx_nic_t *enp;

	if (evvp == NULL)
		return;

	enp = evvp->evv_enp;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);

	if (evvp->evv_state == EFX_VIRTIO_VQ_STATE_INITIALIZED) {
		/* Free the virtqueue object */
		EFSYS_KMEM_FREE(enp->en_esip, sizeof (efx_virtio_vq_t), evvp);
	}
}

	__checkReturn	efx_rc_t
efx_virtio_get_doorbell_offset(
	__in		efx_virtio_vq_t *evvp,
	__out		uint32_t *offsetp)
{
	efx_nic_t *enp;
	const efx_virtio_ops_t *evop;
	efx_rc_t rc;

	if ((evvp == NULL) || (offsetp == NULL)) {
		rc = EINVAL;
		goto fail1;
	}

	enp = evvp->evv_enp;
	evop = enp->en_evop;

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_VIRTIO);

	if (evop == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((rc = evop->evo_get_doorbell_offset(evvp, offsetp)) != 0)
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
efx_virtio_get_features(
	__in		efx_nic_t *enp,
	__in		efx_virtio_device_type_t type,
	__out		uint64_t *featuresp)
{
	const efx_virtio_ops_t *evop = enp->en_evop;
	efx_rc_t rc;

	if (featuresp == NULL) {
		rc = EINVAL;
		goto fail1;
	}

	if (type >= EFX_VIRTIO_DEVICE_NTYPES) {
		rc = EINVAL;
		goto fail2;
	}

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_VIRTIO);

	if (evop == NULL) {
		rc = ENOTSUP;
		goto fail3;
	}

	if ((rc = evop->evo_get_features(enp, type, featuresp)) != 0)
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
efx_virtio_verify_features(
	__in		efx_nic_t *enp,
	__in		efx_virtio_device_type_t type,
	__in		uint64_t features)
{
	const efx_virtio_ops_t *evop = enp->en_evop;
	efx_rc_t rc;

	if (type >= EFX_VIRTIO_DEVICE_NTYPES) {
		rc = EINVAL;
		goto fail1;
	}

	EFSYS_ASSERT3U(enp->en_magic, ==, EFX_NIC_MAGIC);
	EFSYS_ASSERT3U(enp->en_mod_flags, &, EFX_MOD_VIRTIO);

	if (evop == NULL) {
		rc = ENOTSUP;
		goto fail2;
	}

	if ((rc = evop->evo_verify_features(enp, type, features)) != 0)
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

#endif /* EFSYS_OPT_VIRTIO */
