/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2025 Advanced Micro Devices, Inc.
 */
#include "efx.h"
#include "efx_impl.h"

			boolean_t
efx_np_supported(
	__in		efx_nic_t *enp)
{
	return (enp->en_family >= EFX_FAMILY_MEDFORD4) ? B_TRUE : B_FALSE;
}

	__checkReturn	efx_rc_t
efx_np_attach(
	__in		efx_nic_t *enp)
{
	if (efx_np_supported(enp) == B_FALSE)
		return (0);

	return (0);
}

		void
efx_np_detach(
	__in	efx_nic_t *enp)
{
	if (efx_np_supported(enp) == B_FALSE)
		return;
}
