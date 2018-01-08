/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2009-2018 Solarflare Communications Inc.
 * All rights reserved.
 */

#ifndef _SYS_MCDI_MON_H
#define	_SYS_MCDI_MON_H

#include "efx.h"

#ifdef	__cplusplus
extern "C" {
#endif

#if EFSYS_OPT_MON_MCDI

#if EFSYS_OPT_MON_STATS

	__checkReturn	efx_rc_t
mcdi_mon_cfg_build(
    __in		efx_nic_t *enp);

			void
mcdi_mon_cfg_free(
	__in		efx_nic_t *enp);


extern	__checkReturn			efx_rc_t
mcdi_mon_ev(
	__in				efx_nic_t *enp,
	__in				efx_qword_t *eqp,
	__out				efx_mon_stat_t *idp,
	__out				efx_mon_stat_value_t *valuep);

extern	__checkReturn			efx_rc_t
mcdi_mon_stats_update(
	__in				efx_nic_t *enp,
	__in				efsys_mem_t *esmp,
	__inout_ecount(EFX_MON_NSTATS)	efx_mon_stat_value_t *values);

#endif	/* EFSYS_OPT_MON_STATS */

#endif /* EFSYS_OPT_MON_MCDI */

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MCDI_MON_H */
