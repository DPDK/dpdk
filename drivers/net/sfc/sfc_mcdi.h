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

enum sfc_mcdi_state {
	SFC_MCDI_UNINITIALIZED = 0,
	SFC_MCDI_INITIALIZED,
	SFC_MCDI_BUSY,
	SFC_MCDI_COMPLETED,

	SFC_MCDI_NSTATES
};

struct sfc_mcdi {
	rte_spinlock_t			lock;
	efsys_mem_t			mem;
	enum sfc_mcdi_state		state;
	efx_mcdi_transport_t		transport;
	uint32_t			logtype;
	uint32_t			proxy_handle;
	efx_rc_t			proxy_result;
};


struct sfc_adapter;

int sfc_mcdi_init(struct sfc_adapter *sa);
void sfc_mcdi_fini(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_MCDI_H */
