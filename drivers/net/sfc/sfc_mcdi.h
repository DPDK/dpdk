/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_MCDI_H
#define _SFC_MCDI_H

#include <stdint.h>

#include <rte_spinlock.h>

#include "efsys.h"
#include "efx.h"


#ifdef __cplusplus
extern "C" {
#endif

enum sfc_efx_mcdi_state {
	SFC_EFX_MCDI_UNINITIALIZED = 0,
	SFC_EFX_MCDI_INITIALIZED,
	SFC_EFX_MCDI_BUSY,
	SFC_EFX_MCDI_COMPLETED,

	SFC_EFX_MCDI_NSTATES
};

struct sfc_efx_mcdi {
	rte_spinlock_t			lock;
	efsys_mem_t			mem;
	enum sfc_efx_mcdi_state		state;
	efx_mcdi_transport_t		transport;
	uint32_t			logtype;
	uint32_t			proxy_handle;
	efx_rc_t			proxy_result;
};

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_MCDI_H */
