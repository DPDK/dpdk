/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef __NFP_FLOWER_CTRL_H__
#define __NFP_FLOWER_CTRL_H__

#include "nfp_flower.h"

void nfp_flower_ctrl_vnic_poll(struct nfp_app_fw_flower *app_fw_flower);
uint16_t nfp_flower_ctrl_vnic_xmit(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *mbuf);
void nfp_flower_ctrl_vnic_xmit_register(struct nfp_app_fw_flower *app_fw_flower);

#endif /* __NFP_FLOWER_CTRL_H__ */
