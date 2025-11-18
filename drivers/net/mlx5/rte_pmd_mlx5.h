/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_PRIVATE_MLX5_H_
#define RTE_PMD_PRIVATE_MLX5_H_

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_compat.h>

/**
 * @file
 * MLX5 public header.
 *
 * This interface provides the ability to support private PMD
 * dynamic flags.
 */

#ifdef __cplusplus
extern "C" {
#endif

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

/**
 * External Rx queue rte_flow index minimal value.
 */
#define MLX5_EXTERNAL_RX_QUEUE_ID_MIN (UINT16_MAX - 1000 + 1)

/**
 * Update mapping between rte_flow queue index (16 bits) and HW queue index (32
 * bits) for RxQs which is created outside the PMD.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] dpdk_idx
 *   Queue index in rte_flow.
 * @param[in] hw_idx
 *   Queue index in hardware.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *   Possible values for rte_errno:
 *   - EEXIST - a mapping with the same rte_flow index already exists.
 *   - EINVAL - invalid rte_flow index, out of range.
 *   - ENODEV - there is no Ethernet device for this port id.
 *   - ENOTSUP - the port doesn't support external RxQ.
 */
__rte_experimental
int rte_pmd_mlx5_external_rx_queue_id_map(uint16_t port_id, uint16_t dpdk_idx,
					  uint32_t hw_idx);

/**
 * Remove mapping between rte_flow queue index (16 bits) and HW queue index (32
 * bits) for RxQs which is created outside the PMD.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] dpdk_idx
 *   Queue index in rte_flow.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *   Possible values for rte_errno:
 *   - EINVAL - invalid index, out of range, still referenced or doesn't exist.
 *   - ENODEV - there is no Ethernet device for this port id.
 *   - ENOTSUP - the port doesn't support external RxQ.
 */
__rte_experimental
int rte_pmd_mlx5_external_rx_queue_id_unmap(uint16_t port_id,
					    uint16_t dpdk_idx);

/**
 * The rate of the host port shaper will be updated directly at the next
 * available descriptor threshold event to the rate that comes with this flag set;
 * set rate 0 to disable this rate update.
 * Unset this flag to update the rate of the host port shaper directly in
 * the API call; use rate 0 to disable the current shaper.
 */
#define MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED 0

/**
 * Configure a HW shaper to limit Tx rate for a host port.
 * The configuration will affect all the ethdev ports belonging to
 * the same rte_device.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] rate
 *   Unit is 100Mbps, setting the rate to 0 disables the shaper.
 * @param[in] flags
 *   Host shaper flags.
 * @return
 *   0 : operation success.
 *   Otherwise:
 *   - ENOENT - no ibdev interface.
 *   - EBUSY  - the register access unit is busy.
 *   - EIO    - the register access command meets IO error.
 */
__rte_experimental
int rte_pmd_mlx5_host_shaper_config(int port_id, uint8_t rate, uint32_t flags);

/**
 * Enable traffic for external SQ.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] sq_num
 *   SQ HW number.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *   Possible values for rte_errno:
 *   - EINVAL - invalid sq_number or port type.
 *   - ENODEV - there is no Ethernet device for this port id.
 */
__rte_experimental
int rte_pmd_mlx5_external_sq_enable(uint16_t port_id, uint32_t sq_num);

/**
 * Disable traffic for external SQ. Should be invoked by application
 * before destroying the external SQ.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] sq_num
 *   SQ HW number.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *   Possible values for rte_errno:
 *   - EINVAL - invalid sq_number or port type.
 *   - ENODEV - there is no Ethernet device for this port id.
 */
__rte_experimental
int rte_pmd_mlx5_external_sq_disable(uint16_t port_id, uint32_t sq_num);

#ifdef __cplusplus
}
#endif

#endif /* RTE_PMD_PRIVATE_MLX5_H_ */
