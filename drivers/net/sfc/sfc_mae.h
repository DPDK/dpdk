/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2019-2020 Xilinx, Inc.
 * Copyright(c) 2019 Solarflare Communications Inc.
 *
 * This software was jointly developed between OKTET Labs (under contract
 * for Solarflare) and Solarflare Communications, Inc.
 */

#ifndef _SFC_MAE_H
#define _SFC_MAE_H

#include <stdbool.h>

#include "efx.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Options for MAE support status */
enum sfc_mae_status {
	SFC_MAE_STATUS_UNKNOWN = 0,
	SFC_MAE_STATUS_UNSUPPORTED,
	SFC_MAE_STATUS_SUPPORTED
};

struct sfc_mae {
	/** NIC support for MAE status */
	enum sfc_mae_status		status;
	/** Priority level limit for MAE action rules */
	unsigned int			nb_action_rule_prios_max;
};

struct sfc_adapter;

int sfc_mae_attach(struct sfc_adapter *sa);
void sfc_mae_detach(struct sfc_adapter *sa);

#ifdef __cplusplus
}
#endif
#endif /* _SFC_MAE_H */
