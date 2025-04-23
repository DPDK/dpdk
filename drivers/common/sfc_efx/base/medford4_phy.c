/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Advanced Micro Devices, Inc.
 */
#include "efx.h"
#include "efx_impl.h"
#include "medford4_impl.h"

#if EFSYS_OPT_MEDFORD4
	__checkReturn	efx_rc_t
medford4_phy_power(
	__in		efx_nic_t *enp,
	__in		boolean_t power)
{
	if (power)
		enp->en_reset_flags |= EFX_RESET_PHY;

	return (0);
}

	__checkReturn	efx_rc_t
medford4_phy_verify(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
	return (0);
}
#endif	/* EFSYS_OPT_MEDFORD4 */
