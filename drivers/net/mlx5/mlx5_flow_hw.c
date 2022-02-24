/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 NVIDIA Corporation & Affiliates
 */

#include <rte_flow.h>

#include "mlx5_flow.h"

#if defined(HAVE_IBV_FLOW_DV_SUPPORT) || !defined(HAVE_INFINIBAND_VERBS_H)

const struct mlx5_flow_driver_ops mlx5_flow_hw_drv_ops;

#endif
