/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Advanced Micro Devices, Inc.
 */
#ifndef	_SYS_MEDFORD4_IMPL_H
#define	_SYS_MEDFORD4_IMPL_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
medford4_phy_power(
	__in		efx_nic_t *enp,
	__in		boolean_t power);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
medford4_phy_verify(
	__in		efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
medford4_phy_get_link(
	__in		efx_nic_t *enp,
	__out		ef10_link_state_t *elsp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
medford4_phy_link_state_get(
	__in			efx_nic_t *enp,
	__out			efx_phy_link_state_t *eplsp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
medford4_phy_reconfigure(
	__in			efx_nic_t *enp);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
medford4_mac_poll(
	__in		efx_nic_t *enp,
	__out		efx_link_mode_t *link_modep);

LIBEFX_INTERNAL
extern	__checkReturn	efx_rc_t
medford4_mac_up(
	__in		efx_nic_t *enp,
	__out		boolean_t *mac_upp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
medford4_mac_reconfigure(
	__in			efx_nic_t *enp);

#if EFSYS_OPT_MAC_STATS
LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
medford4_mac_stats_get_mask(
	__in			efx_nic_t *enp,
	__inout_bcount(sz)	uint32_t *maskp,
	__in			size_t sz);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
medford4_mac_stats_upload(
	__in			efx_nic_t *enp,
	__in			efsys_mem_t *esmp);

LIBEFX_INTERNAL
extern	__checkReturn		efx_rc_t
medford4_mac_stats_periodic(
	__in			efx_nic_t *enp,
	__in			efsys_mem_t *esmp,
	__in			uint16_t period_ms,
	__in			boolean_t events);

LIBEFX_INTERNAL
extern	__checkReturn			efx_rc_t
medford4_mac_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MAC_NSTATS)	efsys_stat_t *stats,
	__inout_opt			uint32_t *generationp);
#endif /* EFSYS_OPT_MAC_STATS */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEDFORD4_IMPL_H */
