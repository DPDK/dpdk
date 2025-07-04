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

	__checkReturn	efx_rc_t
medford4_mac_pdu_set(
	__in		efx_nic_t *enp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_np_mac_ctrl_t mc = {0};
	efx_rc_t rc;

	mc.enmc_set_pdu_only = B_TRUE;
	mc.enmc_pdu = epp->ep_mac_pdu;

	rc = efx_np_mac_ctrl(enp, epp->ep_np_handle, &mc);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn	efx_rc_t
medford4_mac_pdu_get(
	__in		efx_nic_t *enp,
	__out		size_t *pdup)
{
	efx_port_t *epp = &(enp->en_port);
	efx_np_mac_state_t ms;
	efx_rc_t rc;

	rc = efx_np_mac_state(enp, epp->ep_np_handle, &ms);
	if (rc != 0)
		goto fail1;

	*pdup = ms.enms_pdu;
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
	mc.enmc_pdu = epp->ep_mac_pdu;

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

#if EFSYS_OPT_MAC_STATS
	__checkReturn		efx_rc_t
medford4_mac_stats_get_mask(
	__in			efx_nic_t *enp,
	__inout_bcount(sz)	uint32_t *maskp,
	__in			size_t sz)
{
	efx_port_t *epp = &(enp->en_port);
	unsigned int i;
	efx_rc_t rc;

	for (i = 0; i < EFX_ARRAY_SIZE(epp->ep_np_mac_stat_lut); ++i) {
		const struct efx_mac_stats_range rng[] = { { i, i } };

		if (epp->ep_np_mac_stat_lut[i].ens_valid == B_FALSE)
			continue;

		rc = efx_mac_stats_mask_add_ranges(maskp, sz, rng, 1);
		if (rc != 0)
			goto fail1;
	}

	/* TODO: care about VADAPTOR statistics when VF support arrives */
	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
medford4_mac_stats_upload(
	__in			efx_nic_t *enp,
	__in			efsys_mem_t *esmp)
{
	efx_port_t *epp = &(enp->en_port);
	efx_rc_t rc;

	rc = efx_np_mac_stats(enp,
		    epp->ep_np_handle, EFX_STATS_UPLOAD, esmp, 0);
	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

	__checkReturn		efx_rc_t
medford4_mac_stats_periodic(
	__in			efx_nic_t *enp,
	__in			efsys_mem_t *esmp,
	__in			uint16_t period_ms,
	__in			boolean_t events)
{
	efx_port_t *epp = &(enp->en_port);
	efx_rc_t rc;

	if (period_ms == 0) {
		rc = efx_np_mac_stats(enp, epp->ep_np_handle,
			    EFX_STATS_DISABLE, NULL, 0);
	} else if (events != B_FALSE) {
		rc = efx_np_mac_stats(enp, epp->ep_np_handle,
			    EFX_STATS_ENABLE_EVENTS, esmp, period_ms);
	} else {
		rc = efx_np_mac_stats(enp, epp->ep_np_handle,
			    EFX_STATS_ENABLE_NOEVENTS, esmp, period_ms);
	}

	if (rc != 0)
		goto fail1;

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#define	MEDFORD4_MAC_STAT_READ(_esmp, _field, _eqp)			\
	EFSYS_MEM_READQ((_esmp), (_field) * sizeof (efx_qword_t), _eqp)

	__checkReturn			efx_rc_t
medford4_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stats,
	__inout_opt			uint32_t *generationp)
{
	const efx_nic_cfg_t *encp = &enp->en_nic_cfg;
	efx_port_t *epp = &(enp->en_port);
	efx_qword_t generation_start;
	efx_qword_t generation_end;
	unsigned int i;
	efx_rc_t rc;

	if (EFSYS_MEM_SIZE(esmp) <
	    (encp->enc_mac_stats_nstats * sizeof (efx_qword_t))) {
		/* DMA buffer too small */
		rc = ENOSPC;
		goto fail1;
	}

	/* Read END first so we don't race with the MC */
	EFSYS_DMA_SYNC_FOR_KERNEL(esmp, 0, EFSYS_MEM_SIZE(esmp));
	MEDFORD4_MAC_STAT_READ(esmp, (encp->enc_mac_stats_nstats - 1),
	    &generation_end);
	EFSYS_MEM_READ_BARRIER();

	for (i = 0; i < EFX_ARRAY_SIZE(epp->ep_np_mac_stat_lut); ++i) {
		efx_qword_t value;

		if (epp->ep_np_mac_stat_lut[i].ens_valid == B_FALSE)
			continue;

		MEDFORD4_MAC_STAT_READ(esmp,
		    epp->ep_np_mac_stat_lut[i].ens_dma_fld, &value);

		EFSYS_STAT_SET_QWORD(&(stats[i]), &value);
	}

	/* TODO: care about VADAPTOR statistics when VF support arrives */

	/* Read START generation counter */
	EFSYS_DMA_SYNC_FOR_KERNEL(esmp, 0, EFSYS_MEM_SIZE(esmp));
	EFSYS_MEM_READ_BARRIER();

	/* We never parse marker descriptors; assume start is 0 offset */
	MEDFORD4_MAC_STAT_READ(esmp, 0, &generation_start);

	/* Check that we didn't read the stats in the middle of a DMA */
	if (memcmp(&generation_start, &generation_end,
		    sizeof (generation_start)) != 0)
		return (EAGAIN);

	if (generationp != NULL)
		*generationp = EFX_QWORD_FIELD(generation_start, EFX_DWORD_0);

	return (0);

fail1:
	EFSYS_PROBE1(fail1, efx_rc_t, rc);
	return (rc);
}

#undef MEDFORD4_MAC_STAT_READ
#endif /* EFSYS_OPT_MAC_STATS */
#endif /* EFSYS_OPT_MEDFORD4 */
