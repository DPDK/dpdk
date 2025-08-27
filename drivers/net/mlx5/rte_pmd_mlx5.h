/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#ifndef RTE_PMD_PRIVATE_MLX5_H_
#define RTE_PMD_PRIVATE_MLX5_H_

#include <stdint.h>

#include <rte_byteorder.h>
#include <rte_compat.h>
#include <rte_per_lcore.h>

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
 * External Tx queue rte_flow index minimal value.
 */
#define MLX5_EXTERNAL_TX_QUEUE_ID_MIN (UINT16_MAX - 1000 + 1)

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
 * Update mapping between rte_flow Tx queue index (16 bits) and HW queue index (32
 * bits) for TxQs which is created outside the PMD.
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
 *   - ENOTSUP - the port doesn't support external TxQ.
 */
__rte_experimental
int rte_pmd_mlx5_external_tx_queue_id_map(uint16_t port_id, uint16_t dpdk_idx,
					  uint32_t hw_idx);

/**
 * Remove mapping between rte_flow Tx queue index (16 bits) and HW queue index (32
 * bits) for TxQs which is created outside the PMD.
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
 *   - ENOTSUP - the port doesn't support external TxQ.
 */
__rte_experimental
int rte_pmd_mlx5_external_tx_queue_id_unmap(uint16_t port_id,
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

/**
 * User configuration structure using to create parser for single GENEVE TLV option.
 */
struct rte_pmd_mlx5_geneve_tlv {
	/**
	 * The class of the GENEVE TLV option.
	 * Relevant only when 'match_on_class_mode' is 1.
	 */
	rte_be16_t option_class;
	/**
	 * The type of the GENEVE TLV option.
	 * This field is the identifier of the option.
	 */
	uint8_t option_type;
	/**
	 * The length of the GENEVE TLV option data excluding the option header
	 * in DW granularity.
	 */
	uint8_t option_len;
	/**
	 * Indicator about class field role in this option:
	 *  0 - class is ignored.
	 *  1 - class is fixed (the class defines the option along with the type).
	 *  2 - class matching per flow.
	 */
	uint8_t match_on_class_mode;
	/**
	 * The offset of the first sample in DW granularity.
	 * This offset is relative to first of option data.
	 * The 'match_data_mask' corresponds to option data since this offset.
	 */
	uint8_t offset;
	/**
	 * The number of DW to sample.
	 * This field describes the length of 'match_data_mask' in DW
	 * granularity.
	 */
	uint8_t sample_len;
	/**
	 * Array of DWs which each bit marks if this bit should be sampled.
	 * Each nonzero DW consumes one DW from maximum 7 DW in total.
	 */
	rte_be32_t *match_data_mask;
};

/**
 * Creates GENEVE TLV parser for the selected port.
 * This function must be called before first use of GENEVE option.
 *
 * This API is port oriented, but the configuration is done once for all ports
 * under the same physical device. Each port should call this API before using
 * GENEVE OPT item, but it must use the same options in the same order inside
 * the list.
 *
 * Each physical device has 7 DWs for GENEVE TLV options. Each nonzero element
 * in 'match_data_mask' array consumes one DW, and choosing matchable mode for
 * class consumes additional one.
 * Calling this API for second port under same physical device doesn't consume
 * more DW, it uses same configuration.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] tlv_list
 *   A list of GENEVE TLV options to create parser for them.
 * @param[in] nb_options
 *   The number of options in TLV list.
 *
 * @return
 *   A pointer to TLV handle on success, NULL otherwise and rte_errno is set.
 *   Possible values for rte_errno:
 *   - ENOMEM - not enough memory to create GENEVE TLV parser.
 *   - EEXIST - this port already has GENEVE TLV parser or another port under
 *              same physical device has already prepared a different parser.
 *   - EINVAL - invalid GENEVE TLV requested.
 *   - ENODEV - there is no Ethernet device for this port id.
 *   - ENOTSUP - the port doesn't support GENEVE TLV parsing.
 */
__rte_experimental
void *
rte_pmd_mlx5_create_geneve_tlv_parser(uint16_t port_id,
				      const struct rte_pmd_mlx5_geneve_tlv tlv_list[],
				      uint8_t nb_options);

/**
 * Destroy GENEVE TLV parser for the selected port.
 * This function must be called after last use of GENEVE option and before port
 * closing.
 *
 * @param[in] handle
 *   Handle for the GENEVE TLV parser object to be destroyed.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 *   Possible values for rte_errno:
 *   - EINVAL - invalid handle.
 *   - ENOENT - there is no valid GENEVE TLV parser in this handle.
 *   - EBUSY - one of options is in used by template table.
 */
__rte_experimental
int
rte_pmd_mlx5_destroy_geneve_tlv_parser(void *handle);

/**
 * Dump Rx Queue Context for a given port/queue
 *
 * @param[in] port_id
 *   Port ID
 * @param[in] queue_id
 *   Queue ID
 * @param[in] filename
 *   Name of file to dump the Rx Queue Context
 *
 * @return
 *   0 for success, non-zero value depending on failure type
 */

__rte_experimental
int
rte_pmd_mlx5_rxq_dump_contexts(uint16_t port_id, uint16_t queue_id, const char *filename);

/**
 * Dump Tx Queue Context for a given port/queue
 *
 * @param[in] port_id
 *   Port ID
 * @param[in] queue_id
 *   Queue ID
 * @param[in] filename
 *   Name of file to dump the Tx Queue Context
 *
 * @return
 *   0 for success, non-zero value depending on failure type
 */

__rte_experimental
int
rte_pmd_mlx5_txq_dump_contexts(uint16_t port_id, uint16_t queue_id, const char *filename);

/** Type of mlx5 driver event for which custom callback is called. */
enum rte_pmd_mlx5_driver_event_cb_type {
	/** Called after HW Rx queue is created. */
	RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_CREATE,
	/** Called before HW Rx queue will be destroyed. */
	RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_DESTROY,
	/** Called after HW Tx queue is created. */
	RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_CREATE,
	/** Called before HW Tx queue will be destroyed. */
	RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_DESTROY,
};

/** Information about the queue for which driver event is being called. */
struct rte_pmd_mlx5_driver_event_cb_queue_info {
	/** DPDK queue index. */
	uint16_t dpdk_queue_id;
	/** HW queue identifier (DevX object ID). */
	uint32_t hw_queue_id;
	/**
	 * Low-level HW configuration of the port related to the queue.
	 * This configuration is presented as a string
	 * with "key=value" pairs, separated by commas.
	 * This string is owned by mlx5 PMD and should not be freed by the user,
	 * and should be copied to the memory owned by the user.
	 *
	 * For RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_CREATE this will contain:
	 *
	 * - lro_timeout - Configured timeout of LRO session in microseconds.
	 *   Set to 0 if LRO is not configured.
	 * - max_lro_msg_size - Maximum size of a single LRO message.
	 *   Provided in granularity of 256 bytes.
	 *   Set to 0 if LRO is not configured.
	 * - td - Identifier of transport domain allocated from HW (DevX object ID).
	 * - lbpk - Set to 1 if loopback is enabled on the given queue
	 *
	 * For all other events, this field will be set to NULL.
	 */
	const char *queue_info;
};

/** Information related to a driver event. */
struct rte_pmd_mlx5_driver_event_cb_info {
	/** Type of the driver event for which the callback is called. */
	enum rte_pmd_mlx5_driver_event_cb_type event;
	union {
		/**
		 * Information about the queue for which driver event is being called.
		 *
		 * This union variant is valid for the following events:
		 *
		 * - RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_CREATE
		 * - RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_RXQ_DESTROY
		 * - RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_CREATE
		 * - RTE_PMD_MLX5_DRIVER_EVENT_CB_TYPE_TXQ_DESTROY
		 */
		struct rte_pmd_mlx5_driver_event_cb_queue_info queue;
	};
};

/** Prototype of the callback called on mlx5 driver events. */
typedef void (*rte_pmd_mlx5_driver_event_callback_t)(uint16_t port_id,
		const struct rte_pmd_mlx5_driver_event_cb_info *info,
		const void *opaque);


/**
 * Register mlx5 driver event callback.
 *
 * mlx5 PMD configures HW through interfaces exposed by rdma-core and mlx5 kernel driver.
 * Any HW object created this way may be used by other libraries or applications.
 * This function allows application to register a custom callback which will be called
 * whenever mlx5 PMD performs some operation (driver event) on a managed HW objects.
 * #rte_pmd_mlx5_driver_event_cb_type defines exposed driver events.
 *
 * This function can be called multiple times with different callbacks.
 * mlx5 PMD will register all of them and all of them will be called for triggered driver events.
 *
 * This function can be called:
 *
 * - before or after #rte_eal_init (potentially in a constructor function as well),
 * - before or after any mlx5 port is probed.
 *
 * If this function is called when mlx5 ports (at least one) exist,
 * then provided callback will be immediately called for all applicable driver events,
 * for all existing mlx5 ports.
 *
 * This function is lock-free and it is assumed that it won't be called concurrently
 * with other functions from ethdev API used to configure any of the mlx5 ports.
 * It is the responsibility of the application to enforce this.
 *
 * Registered callbacks might be called during control path configuration triggered
 * by DPDK API. It is the user's responsibility to prevent
 * calling more configurations by the DPDK API from the callback itself.
 *
 * mlx5 PMD registers a destructor (through #RTE_FINI)
 * which will unregister all known callbacks.
 *
 * @param[in] cb
 *   Pointer to callback.
 * @param[in] opaque
 *   Opaque pointer which will be passed as an argument to @p cb on each event.
 *
 * @return
 *   - 0 if callback was successfully registered.
 *   - (-EINVAL) if @p cb is NULL.
 *   - (-EEXIST) if @p cb was already registered.
 *   - (-ENOMEM) if failed to allocate memory for callback entry.
 */
__rte_experimental
int
rte_pmd_mlx5_driver_event_cb_register(rte_pmd_mlx5_driver_event_callback_t cb, void *opaque);

/**
 * Unregister driver event callback.
 *
 * Unregisters a mlx5 driver event callback which was previously registered
 * through #rte_pmd_mlx5_driver_event_cb_unregister.
 *
 * This function is lock-free and it is assumed that it won't be called concurrently
 * with other functions from ethdev API used to configure any of the mlx5 ports.
 * It is the responsibility of the application to enforce this.
 *
 * @param[in] cb
 *   Pointer to callback.
 *
 * @return
 *   - 0 if callback was successfully unregistered or if no such callback was registered.
 *   - (-EINVAL) if @p cb is NULL.
 */
__rte_experimental
int
rte_pmd_mlx5_driver_event_cb_unregister(rte_pmd_mlx5_driver_event_callback_t cb);

#ifdef __cplusplus
}
#endif

#endif /* RTE_PMD_PRIVATE_MLX5_H_ */
