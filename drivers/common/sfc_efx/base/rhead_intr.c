/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2018-2019 Solarflare Communications Inc.
 */

#include "efx.h"
#include "efx_impl.h"


#if EFSYS_OPT_RIVERHEAD

	__checkReturn	efx_rc_t
rhead_intr_init(
	__in		efx_nic_t *enp,
	__in		efx_intr_type_t type,
	__in		efsys_mem_t *esmp)
{
	_NOTE(ARGUNUSED(enp, type, esmp))

	return (0);
}


			void
rhead_intr_enable(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}


			void
rhead_intr_disable(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}


			void
rhead_intr_disable_unlocked(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

	__checkReturn	efx_rc_t
rhead_intr_trigger(
	__in		efx_nic_t *enp,
	__in		unsigned int level)
{
	_NOTE(ARGUNUSED(enp, level))

	return (ENOTSUP);
}

			void
rhead_intr_status_line(
	__in		efx_nic_t *enp,
	__out		boolean_t *fatalp,
	__out		uint32_t *qmaskp)
{
	_NOTE(ARGUNUSED(enp, qmaskp))

	/*
	 * Riverhead does not support line interrupts,
	 * so this function should never be called.
	 */

	/* Fatal errors are reported via events */
	*fatalp = B_FALSE;
}

			void
rhead_intr_status_message(
	__in		efx_nic_t *enp,
	__in		unsigned int message,
	__out		boolean_t *fatalp)
{
	EFSYS_ASSERT(enp->en_family == EFX_FAMILY_RIVERHEAD);

	_NOTE(ARGUNUSED(enp, message))

	/* Fatal errors are reported via events */
	*fatalp = B_FALSE;
}

			void
rhead_intr_fatal(
	__in		efx_nic_t *enp)
{
	/* Fatal errors are reported via events */
	_NOTE(ARGUNUSED(enp))
}

			void
rhead_intr_fini(
	__in		efx_nic_t *enp)
{
	_NOTE(ARGUNUSED(enp))
}

#endif	/* EFSYS_OPT_RIVERHEAD */
