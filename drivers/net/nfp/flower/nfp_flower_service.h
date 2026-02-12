/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2024 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_FLOWER_SERVICE_H__
#define __NFP_FLOWER_SERVICE_H__

#include "../nfp_net_common.h"

int nfp_flower_service_start(struct nfp_net_hw_priv *hw_priv);
void nfp_flower_service_stop(struct nfp_net_hw_priv *hw_priv);

int nfp_flower_service_sync_alloc(struct nfp_net_hw_priv *hw_priv);
void nfp_flower_service_sync_free(struct nfp_net_hw_priv *hw_priv);

#endif /* __NFP_FLOWER_SERVICE_H__ */
