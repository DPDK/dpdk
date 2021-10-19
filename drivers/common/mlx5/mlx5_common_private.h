/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2021 Mellanox Technologies, Ltd
 */

#ifndef MLX5_COMMON_PRIVATE_H
#define MLX5_COMMON_PRIVATE_H

#include <rte_pci.h>
#include <rte_bus_auxiliary.h>

#include "mlx5_common.h"

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Common bus driver: */

int mlx5_common_dev_probe(struct rte_device *eal_dev);
int mlx5_common_dev_remove(struct rte_device *eal_dev);
int mlx5_common_dev_dma_map(struct rte_device *dev, void *addr, uint64_t iova,
			    size_t len);
int mlx5_common_dev_dma_unmap(struct rte_device *dev, void *addr, uint64_t iova,
			      size_t len);

/* Common PCI bus driver: */

void mlx5_common_pci_init(void);
void mlx5_common_driver_on_register_pci(struct mlx5_class_driver *driver);
bool mlx5_dev_pci_match(const struct mlx5_class_driver *drv,
			const struct rte_device *dev);

/* Common auxiliary bus driver: */
void mlx5_common_auxiliary_init(void);
struct ibv_device *mlx5_get_aux_ibv_device(
		const struct rte_auxiliary_device *dev);
int mlx5_auxiliary_get_pci_str(const struct rte_auxiliary_device *dev,
			       char *addr, size_t size);

#ifdef __cplusplus
}
#endif /* __cplusplus */

#endif /* MLX5_COMMON_PRIVATE_H */
