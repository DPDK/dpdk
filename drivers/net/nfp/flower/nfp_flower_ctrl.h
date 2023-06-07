/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Corigine, Inc.
 * All rights reserved.
 */

#ifndef _NFP_FLOWER_CTRL_H_
#define _NFP_FLOWER_CTRL_H_

void nfp_flower_ctrl_vnic_poll(struct nfp_app_fw_flower *app_fw_flower);
uint16_t nfp_flower_ctrl_vnic_xmit(struct nfp_app_fw_flower *app_fw_flower,
		struct rte_mbuf *mbuf);
void nfp_flower_ctrl_vnic_xmit_register(struct nfp_app_fw_flower *app_fw_flower);

#endif /* _NFP_FLOWER_CTRL_H_ */
