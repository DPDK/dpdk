/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright (c) 2018-2019 Solarflare Communications Inc.
 * All rights reserved.
 */

#include "efx.h"
#include "efx_impl.h"

#if EFSYS_OPT_EVB

#if EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2

	__checkReturn	efx_rc_t
ef10_evb_init(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		enp->en_family == EFX_FAMILY_MEDFORD ||
		enp->en_family == EFX_FAMILY_MEDFORD2);

	return (0);
}

	void
ef10_evb_fini(
	__in		efx_nic_t *enp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_HUNTINGTON ||
		enp->en_family == EFX_FAMILY_MEDFORD ||
		enp->en_family == EFX_FAMILY_MEDFORD2);
}

#endif /* EFSYS_OPT_HUNTINGTON || EFSYS_OPT_MEDFORD || EFSYS_OPT_MEDFORD2 */
#endif /* EFSYS_OPT_EVB */
