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

#ifdef	__cplusplus
}
#endif

#endif	/* _SYS_MEDFORD4_IMPL_H */
