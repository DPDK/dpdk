/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_PRIVATE_MLX5_H_
#define RTE_PMD_PRIVATE_MLX5_H_

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

#define RTE_PMD_MLX5_DOMAIN_BIT_NIC_RX	(1 << 0) /**< NIC RX domain bit mask. */
#define RTE_PMD_MLX5_DOMAIN_BIT_NIC_TX	(1 << 1) /**< NIC TX domain bit mask. */
#define RTE_PMD_MLX5_DOMAIN_BIT_FDB	(1 << 2) /**< FDB (TX + RX) domain bit mask. */

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
 *   RTE_PMD_MLX5_DOMAIN_BIT_* macros are used to specify the domains.
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
#define RTE_PMD_MLX5_EXTERNAL_RX_QUEUE_ID_MIN (UINT16_MAX - 1000 + 1)

/**
 * Tag level to set the linear hash index.
 */
#define RTE_PMD_MLX5_LINEAR_HASH_TAG_INDEX 255

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
#define RTE_PMD_MLX5_HOST_SHAPER_FLAG_AVAIL_THRESH_TRIGGERED 0

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
 *   Host shaper flags (see RTE_PMD_MLX5_HOST_SHAPER_FLAG_*).
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

/* MLX5 flow engine mode definition for live migration. */
enum rte_pmd_mlx5_flow_engine_mode {
	RTE_PMD_MLX5_FLOW_ENGINE_MODE_ACTIVE, /* active means high priority, effective in HW. */
	RTE_PMD_MLX5_FLOW_ENGINE_MODE_STANDBY, /* standby mode with lower priority flow rules. */
};

/**
 * When set on the flow engine of a standby process, ingress flow rules will be effective
 * in active and standby processes, so the ingress traffic may be duplicated.
 */
#define RTE_PMD_MLX5_FLOW_ENGINE_FLAG_STANDBY_DUP_INGRESS      RTE_BIT32(0)

/**
 * @warning
 * @b EXPERIMENTAL: this API may change without prior notice
 *
 * Set the flow engine mode of the process to active or standby,
 * affecting network traffic handling.
 *
 * If one device does not support this operation or fails,
 * the whole operation is failed and rolled back.
 *
 * It is forbidden to have multiple flow engines with the same mode
 * unless only one of them is configured to handle the traffic.
 *
 * The application's flow engine is active by default.
 * The configuration from the active flow engine is effective immediately
 * while the configuration from the standby flow engine is queued by hardware.
 * When configuring the device from a standby flow engine,
 * it has no effect except for below situations:
 *   - traffic not handled by the active flow engine configuration
 *   - no active flow engine
 *
 * When flow engine of a process is changed from a standby to an active mode,
 * all preceding configurations that are queued by hardware
 * should become effective immediately.
 * Before mode transition, all the traffic handling configurations
 * set by the active flow engine should be flushed first.
 *
 * In summary, the operations are expected to happen in this order
 * in "old" and "new" applications:
 *   device: already configured by the old application
 *   new:    start as active
 *   new:    probe the same device
 *   new:    set as standby
 *   new:    configure the device
 *   device: has configurations from old and new applications
 *   old:    clear its device configuration
 *   device: has only 1 configuration from new application
 *   new:    set as active
 *   device: downtime for connecting all to the new application
 *   old:    shutdown
 *
 * @param mode
 *   The desired mode (see rte_pmd_mlx5_flow_engine_mode).
 * @param flags
 *   Mode specific flags (see RTE_PMD_MLX5_FLOW_ENGINE_FLAG_*).
 * @return
 *   Positive value on success, -rte_errno value on error:
 *   - (> 0) Number of switched devices.
 *   - (-EINVAL) if error happen and rollback internally.
 *   - (-EPERM) if operation failed and can't recover.
 */
__rte_experimental
int rte_pmd_mlx5_flow_engine_set_mode(enum rte_pmd_mlx5_flow_engine_mode mode, uint32_t flags);

#ifdef __cplusplus
}
#endif

#endif /* RTE_PMD_PRIVATE_MLX5_H_ */
