/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2021 Xilinx, Inc.
 * Copyright(c) 2016-2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_EFX_MCDI_H
#define _SFC_EFX_MCDI_H

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
	SFC_EFX_MCDI_DEAD,

	SFC_EFX_MCDI_NSTATES
};

typedef int (sfc_efx_mcdi_dma_alloc_cb)(void *cookie, const char *name,
					  size_t len, efsys_mem_t *esmp);

typedef void (sfc_efx_mcdi_dma_free_cb)(void *cookie, efsys_mem_t *esmp);

typedef void (sfc_efx_mcdi_sched_restart_cb)(void *cookie);

typedef void (sfc_efx_mcdi_mgmt_evq_poll_cb)(void *cookie);

struct sfc_efx_mcdi_ops {
	sfc_efx_mcdi_dma_alloc_cb	*dma_alloc;
	sfc_efx_mcdi_dma_free_cb	*dma_free;
	sfc_efx_mcdi_sched_restart_cb	*sched_restart;
	sfc_efx_mcdi_mgmt_evq_poll_cb	*mgmt_evq_poll;
};

struct sfc_efx_mcdi {
	rte_spinlock_t			lock;
	const struct sfc_efx_mcdi_ops	*ops;
	void				*ops_cookie;
	efx_nic_t			*nic;
	efsys_mem_t			mem;
	enum sfc_efx_mcdi_state		state;
	efx_mcdi_transport_t		transport;
	uint32_t			logtype;
	uint32_t			proxy_handle;
	efx_rc_t			proxy_result;
	const char			*log_prefix;
};

__rte_internal
int sfc_efx_mcdi_init(struct sfc_efx_mcdi *mcdi,
		      uint32_t logtype, const char *log_prefix,
		      efx_nic_t *nic,
		      const struct sfc_efx_mcdi_ops *ops, void *ops_cookie);
__rte_internal
void sfc_efx_mcdi_fini(struct sfc_efx_mcdi *mcdi);

#ifdef __cplusplus
}
#endif

#endif  /* _SFC_EFX_MCDI_H */
