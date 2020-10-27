/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_PRIVATE_MLX5_H_
#define RTE_PMD_PRIVATE_MLX5_H_

/**
 * @file
 * MLX5 public header.
 *
 * This interface provides the ability to support private PMD
 * dynamic flags.
 */

#define RTE_PMD_MLX5_FINE_GRANULARITY_INLINE "mlx5_fine_granularity_inline"

/**
 * Returns the dynamic flags name, that are supported.
 *
 * @param[out] names
 *   Array that is used to return the supported dynamic flags names.
 * @param[in] n
 *   The number of elements in the names array.
 *
 * @return
 *   The number of dynamic flags that were copied if not negative.
 *   Otherwise:
 *   - ENOMEM - not enough entries in the array
 *   - EINVAL - invalid array entry
 */
__rte_experimental
int rte_pmd_mlx5_get_dyn_flag_names(char *names[], unsigned int n);

#define MLX5_DOMAIN_BIT_NIC_RX	(1 << 0) /**< NIC RX domain bit mask. */
#define MLX5_DOMAIN_BIT_NIC_TX	(1 << 1) /**< NIC TX domain bit mask. */
#define MLX5_DOMAIN_BIT_FDB	(1 << 2) /**< FDB (TX + RX) domain bit mask. */

/**
 * Synchronize the flows to make them take effort on hardware.
 * It only supports DR flows now. For DV and Verbs flows, there is no need to
 * call this function, and a success will return directly in case of Verbs.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] domains
 *   Refer to "/usr/include/infiniband/mlx5dv.h".
 *   Bitmask of domains in which the synchronization will be done.
 *   MLX5_DOMAIN_BIT* macros are used to specify the domains.
 *   An ADD or OR operation could be used to synchronize flows in more than
 *   one domain per call.
 *
 * @return
 *   - (0) if successful.
 *   - Negative value if an error.
 */
__rte_experimental
int rte_pmd_mlx5_sync_flow(uint16_t port_id, uint32_t domains);

#endif
