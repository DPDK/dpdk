/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#ifndef _NBL_DISPATCH_H_
#define _NBL_DISPATCH_H_

#include "nbl_ethdev.h"

#define NBL_DISP_MGT_TO_RES_OPS_TBL(disp_mgt)	((disp_mgt)->res_ops_tbl)
#define NBL_DISP_MGT_TO_RES_OPS(disp_mgt)	(NBL_DISP_MGT_TO_RES_OPS_TBL(disp_mgt)->ops)
#define NBL_DISP_MGT_TO_RES_PRIV(disp_mgt)	(NBL_DISP_MGT_TO_RES_OPS_TBL(disp_mgt)->priv)
#define NBL_DISP_MGT_TO_CHAN_OPS_TBL(disp_mgt)	((disp_mgt)->chan_ops_tbl)
#define NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt)	(NBL_DISP_MGT_TO_CHAN_OPS_TBL(disp_mgt)->ops)
#define NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt)	(NBL_DISP_MGT_TO_CHAN_OPS_TBL(disp_mgt)->priv)
#define NBL_DISP_MGT_TO_DISP_OPS_TBL(disp_mgt)	((disp_mgt)->disp_ops_tbl)
#define NBL_DISP_MGT_TO_DISP_OPS(disp_mgt)	(NBL_DISP_MGT_TO_DISP_OPS_TBL(disp_mgt)->ops)
#define NBL_DISP_MGT_TO_DISP_PRIV(disp_mgt)	(NBL_DISP_MGT_TO_DISP_OPS_TBL(disp_mgt)->priv)

struct nbl_dispatch_mgt {
	struct nbl_resource_ops_tbl *res_ops_tbl;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_dispatch_ops_tbl *disp_ops_tbl;
	uint32_t ctrl_lvl;
};

struct nbl_product_dispatch_ops *nbl_dispatch_get_product_ops(enum nbl_product_type product_type);

#endif
