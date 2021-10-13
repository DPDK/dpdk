/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright(c) 2021 Xilinx, Inc.
 */

#include <stdbool.h>
#include <stdint.h>

#include "sfc.h"
#include "sfc_dp_rx.h"
#include "sfc_flow_tunnel.h"
#include "sfc_mae.h"

bool
sfc_flow_tunnel_is_supported(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	return ((sa->priv.dp_rx->features & SFC_DP_RX_FEAT_FLOW_MARK) != 0 &&
		sa->mae.status == SFC_MAE_STATUS_SUPPORTED);
}

bool
sfc_flow_tunnel_is_active(struct sfc_adapter *sa)
{
	SFC_ASSERT(sfc_adapter_is_locked(sa));

	return ((sa->negotiated_rx_metadata &
		 RTE_ETH_RX_METADATA_TUNNEL_ID) != 0);
}
