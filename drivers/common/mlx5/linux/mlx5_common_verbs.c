/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>

#include <rte_errno.h>
#include <rte_bus_pci.h>
#include <rte_bus_auxiliary.h>

#include "mlx5_common_utils.h"
#include "mlx5_common_log.h"
#include "mlx5_common_private.h"
#include "mlx5_autoconf.h"
#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_common_mr.h>

struct ibv_device *
mlx5_os_get_ibv_dev(const struct rte_device *dev)
{
	struct ibv_device *ibv;

	if (mlx5_dev_is_pci(dev))
		ibv = mlx5_os_get_ibv_device(&RTE_DEV_TO_PCI_CONST(dev)->addr);
	else
		ibv = mlx5_get_aux_ibv_device(RTE_DEV_TO_AUXILIARY_CONST(dev));
	if (ibv == NULL) {
		rte_errno = ENODEV;
		DRV_LOG(ERR, "Verbs device not found: %s", dev->name);
	}
	return ibv;
}

/**
 * Register mr. Given protection domain pointer, pointer to addr and length
 * register the memory region.
 *
 * @param[in] pd
 *   Pointer to protection domain context.
 * @param[in] addr
 *   Pointer to memory start address.
 * @param[in] length
 *   Length of the memory to register.
 * @param[out] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 * @return
 *   0 on successful registration, -1 otherwise
 */
int
mlx5_common_verbs_reg_mr(void *pd, void *addr, size_t length,
			 struct mlx5_pmd_mr *pmd_mr)
{
	struct ibv_mr *ibv_mr;

	ibv_mr = mlx5_glue->reg_mr(pd, addr, length,
				   IBV_ACCESS_LOCAL_WRITE |
				   (haswell_broadwell_cpu ? 0 :
				   IBV_ACCESS_RELAXED_ORDERING));
	if (!ibv_mr)
		return -1;

	*pmd_mr = (struct mlx5_pmd_mr){
		.lkey = ibv_mr->lkey,
		.addr = ibv_mr->addr,
		.len = ibv_mr->length,
		.obj = (void *)ibv_mr,
	};
	return 0;
}

/**
 * Deregister mr. Given the mlx5 pmd MR - deregister the MR
 *
 * @param[in] pmd_mr
 *   pmd_mr struct set with lkey, address, length and pointer to mr object
 *
 */
void
mlx5_common_verbs_dereg_mr(struct mlx5_pmd_mr *pmd_mr)
{
	if (pmd_mr && pmd_mr->obj != NULL) {
		claim_zero(mlx5_glue->dereg_mr(pmd_mr->obj));
		memset(pmd_mr, 0, sizeof(*pmd_mr));
	}
}
