/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#include <stdbool.h>

#include <rte_common.h>

#include "efx.h"

#include "sfc.h"
#include "sfc_log.h"

int
sfc_mae_attach(struct sfc_adapter *sa)
{
	const efx_nic_cfg_t *encp = efx_nic_cfg_get(sa->nic);
	struct sfc_mae *mae = &sa->mae;

	sfc_log_init(sa, "entry");

	if (!encp->enc_mae_supported) {
		mae->status = SFC_MAE_STATUS_UNSUPPORTED;
		return 0;
	}

	mae->status = SFC_MAE_STATUS_SUPPORTED;

	sfc_log_init(sa, "done");

	return 0;
}

void
sfc_mae_detach(struct sfc_adapter *sa)
{
	struct sfc_mae *mae = &sa->mae;

	sfc_log_init(sa, "entry");

	mae->status = SFC_MAE_STATUS_UNKNOWN;

	sfc_log_init(sa, "done");
}
