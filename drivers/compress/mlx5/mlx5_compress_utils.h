/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_MLX5_COMPRESS_UTILS_H_
#define RTE_PMD_MLX5_COMPRESS_UTILS_H_

#include <mlx5_common.h>


extern int mlx5_compress_logtype;

#define MLX5_COMPRESS_LOG_PREFIX "mlx5_compress"
/* Generic printf()-like logging macro with automatic line feed. */
#define DRV_LOG(level, ...) \
	PMD_DRV_LOG_(level, mlx5_compress_logtype, MLX5_COMPRESS_LOG_PREFIX, \
		__VA_ARGS__ PMD_DRV_LOG_STRIP PMD_DRV_LOG_OPAREN, \
		PMD_DRV_LOG_CPAREN)

#endif /* RTE_PMD_MLX5_COMPRESS_UTILS_H_ */
