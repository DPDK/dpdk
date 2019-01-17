/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017-2018 Intel Corporation
 */

#ifndef _RTE_COMPRESSDEV_H_
#define _RTE_COMPRESSDEV_H_

/**
 * @file rte_compressdev.h
 *
 * RTE Compression Device APIs
 *
 * Defines comp device APIs for the provisioning of compression operations.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <rte_common.h>

#include "rte_comp.h"

/**
 * Parameter log base 2 range description.
 * Final value will be 2^value.
 */
struct rte_param_log2_range {
	uint8_t min;	/**< Minimum log2 value */
	uint8_t max;	/**< Maximum log2 value */
	uint8_t increment;
	/**< If a range of sizes are supported,
	 * this parameter is used to indicate
	 * increments in base 2 log byte value
	 * that are supported between the minimum and maximum
	 */
};

/** Structure used to capture a capability of a comp device */
struct rte_compressdev_capabilities {
	enum rte_comp_algorithm algo;
	/* Compression algorithm */
	uint64_t comp_feature_flags;
	/**< Bitmask of flags for compression service features */
	struct rte_param_log2_range window_size;
	/**< Window size range in base two log byte values */
};

/** Macro used at end of comp PMD list */
#define RTE_COMP_END_OF_CAPABILITIES_LIST() \
	{ RTE_COMP_ALGO_UNSPECIFIED }

const struct rte_compressdev_capabilities * __rte_experimental
rte_compressdev_capability_get(uint8_t dev_id,
			enum rte_comp_algorithm algo);

/**
 * compression device supported feature flags
 *
 * @note New features flags should be added to the end of the list
 *
 * Keep these flags synchronised with rte_compressdev_get_feature_name()
 */
#define	RTE_COMPDEV_FF_HW_ACCELERATED		(1ULL << 0)
/**< Operations are off-loaded to an external hardware accelerator */
#define	RTE_COMPDEV_FF_CPU_SSE			(1ULL << 1)
/**< Utilises CPU SIMD SSE instructions */
#define	RTE_COMPDEV_FF_CPU_AVX			(1ULL << 2)
/**< Utilises CPU SIMD AVX instructions */
#define	RTE_COMPDEV_FF_CPU_AVX2			(1ULL << 3)
/**< Utilises CPU SIMD AVX2 instructions */
#define	RTE_COMPDEV_FF_CPU_AVX512		(1ULL << 4)
/**< Utilises CPU SIMD AVX512 instructions */
#define	RTE_COMPDEV_FF_CPU_NEON			(1ULL << 5)
/**< Utilises CPU NEON instructions */
#define RTE_COMPDEV_FF_OP_DONE_IN_DEQUEUE	(1ULL << 6)
/**< A PMD should set this if the bulk of the
 * processing is done during the dequeue. It should leave it
 * cleared if the processing is done during the enqueue (default).
 * Applications can use this as a hint for tuning.
 */

/**
 * Get the name of a compress device feature flag.
 *
 * @param flag
 *   The mask describing the flag
 *
 * @return
 *   The name of this flag, or NULL if it's not a valid feature flag.
 */
const char * __rte_experimental
rte_compressdev_get_feature_name(uint64_t flag);

/**  comp device information */
struct rte_compressdev_info {
	const char *driver_name;		/**< Driver name. */
	uint64_t feature_flags;			/**< Feature flags */
	const struct rte_compressdev_capabilities *capabilities;
	/**< Array of devices supported capabilities */
	uint16_t max_nb_queue_pairs;
	/**< Maximum number of queues pairs supported by device.
	 * (If 0, there is no limit in maximum number of queue pairs)
	 */
};

/** comp device statistics */
struct rte_compressdev_stats {
	uint64_t enqueued_count;
	/**< Count of all operations enqueued */
	uint64_t dequeued_count;
	/**< Count of all operations dequeued */

	uint64_t enqueue_err_count;
	/**< Total error count on operations enqueued */
	uint64_t dequeue_err_count;
	/**< Total error count on operations dequeued */
};


/**
 * Get the device identifier for the named compress device.
 *
 * @param name
 *   Device name to select the device structure
 * @return
 *   - Returns compress device identifier on success.
 *   - Return -1 on failure to find named compress device.
 */
int __rte_experimental
rte_compressdev_get_dev_id(const char *name);

/**
 * Get the compress device name given a device identifier.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - Returns compress device name.
 *   - Returns NULL if compress device is not present.
 */
const char * __rte_experimental
rte_compressdev_name_get(uint8_t dev_id);

/**
 * Get the total number of compress devices that have been successfully
 * initialised.
 *
 * @return
 *   - The total number of usable compress devices.
 */
uint8_t __rte_experimental
rte_compressdev_count(void);

/**
 * Get number and identifiers of attached comp devices that
 * use the same compress driver.
 *
 * @param driver_name
 *   Driver name
 * @param devices
 *   Output devices identifiers
 * @param nb_devices
 *   Maximal number of devices
 *
 * @return
 *   Returns number of attached compress devices.
 */
uint8_t __rte_experimental
rte_compressdev_devices_get(const char *driver_name, uint8_t *devices,
		uint8_t nb_devices);

/*
 * Return the NUMA socket to which a device is connected.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   The NUMA socket id to which the device is connected or
 *   a default of zero if the socket could not be determined.
 *   -1 if returned is the dev_id value is out of range.
 */
int __rte_experimental
rte_compressdev_socket_id(uint8_t dev_id);

/** Compress device configuration structure */
struct rte_compressdev_config {
	int socket_id;
	/**< Socket on which to allocate resources */
	uint16_t nb_queue_pairs;
	/**< Total number of queue pairs to configure on a device */
	uint16_t max_nb_priv_xforms;
	/**< Max number of private_xforms which will be created on the device */
	uint16_t max_nb_streams;
	/**< Max number of streams which will be created on the device */
};

/**
 * Configure a device.
 *
 * This function must be invoked first before any other function in the
 * API. This function can also be re-invoked when a device is in the
 * stopped state.
 *
 * @param dev_id
 *   Compress device identifier
 * @param config
 *   The compress device configuration
 * @return
 *   - 0: Success, device configured.
 *   - <0: Error code returned by the driver configuration function.
 */
int __rte_experimental
rte_compressdev_configure(uint8_t dev_id,
			struct rte_compressdev_config *config);

/**
 * Start a device.
 *
 * The device start step is called after configuring the device and setting up
 * its queue pairs.
 * On success, data-path functions exported by the API (enqueue/dequeue, etc)
 * can be invoked.
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - 0: Success, device started.
 *   - <0: Error code of the driver device start function.
 */
int __rte_experimental
rte_compressdev_start(uint8_t dev_id);

/**
 * Stop a device. The device can be restarted with a call to
 * rte_compressdev_start()
 *
 * @param dev_id
 *   Compress device identifier
 */
void __rte_experimental
rte_compressdev_stop(uint8_t dev_id);

/**
 * Close an device.
 * The memory allocated in the device gets freed.
 * After calling this function, in order to use
 * the device again, it is required to
 * configure the device again.
 *
 * @param dev_id
 *   Compress device identifier
 *
 * @return
 *  - 0 on successfully closing device
 *  - <0 on failure to close device
 */
int __rte_experimental
rte_compressdev_close(uint8_t dev_id);

/**
 * Allocate and set up a receive queue pair for a device.
 * This should only be called when the device is stopped.
 *
 *
 * @param dev_id
 *   Compress device identifier
 * @param queue_pair_id
 *   The index of the queue pairs to set up. The
 *   value must be in the range [0, nb_queue_pair - 1]
 *   previously supplied to rte_compressdev_configure()
 * @param max_inflight_ops
 *   Max number of ops which the qp will have to
 *   accommodate simultaneously
 * @param socket_id
 *   The *socket_id* argument is the socket identifier
 *   in case of NUMA. The value can be *SOCKET_ID_ANY*
 *   if there is no NUMA constraint for the DMA memory
 *   allocated for the receive queue pair
 * @return
 *   - 0: Success, queue pair correctly set up.
 *   - <0: Queue pair configuration failed
 */
int __rte_experimental
rte_compressdev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
		uint32_t max_inflight_ops, int socket_id);

/**
 * Get the number of queue pairs on a specific comp device
 *
 * @param dev_id
 *   Compress device identifier
 * @return
 *   - The number of configured queue pairs.
 */
uint16_t __rte_experimental
rte_compressdev_queue_pair_count(uint8_t dev_id);


/**
 * Retrieve the general I/O statistics of a device.
 *
 * @param dev_id
 *   The identifier of the device
 * @param stats
 *   A pointer to a structure of type
 *   *rte_compressdev_stats* to be filled with the
 *   values of device counters
 * @return
 *   - Zero if successful.
 *   - Non-zero otherwise.
 */
int __rte_experimental
rte_compressdev_stats_get(uint8_t dev_id, struct rte_compressdev_stats *stats);

/**
 * Reset the general I/O statistics of a device.
 *
 * @param dev_id
 *   The identifier of the device.
 */
void __rte_experimental
rte_compressdev_stats_reset(uint8_t dev_id);

/**
 * Retrieve the contextual information of a device.
 *
 * @param dev_id
 *   Compress device identifier
 * @param dev_info
 *   A pointer to a structure of type *rte_compressdev_info*
 *   to be filled with the contextual information of the device
 *
 * @note The capabilities field of dev_info is set to point to the first
 * element of an array of struct rte_compressdev_capabilities.
 * The element after the last valid element has it's op field set to
 * RTE_COMP_ALGO_LIST_END.
 */
void __rte_experimental
rte_compressdev_info_get(uint8_t dev_id, struct rte_compressdev_info *dev_info);

/**
 *
 * Dequeue a burst of processed compression operations from a queue on the comp
 * device. The dequeued operation are stored in *rte_comp_op* structures
 * whose pointers are supplied in the *ops* array.
 *
 * The rte_compressdev_dequeue_burst() function returns the number of ops
 * actually dequeued, which is the number of *rte_comp_op* data structures
 * effectively supplied into the *ops* array.
 *
 * A return value equal to *nb_ops* indicates that the queue contained
 * at least *nb_ops* operations, and this is likely to signify that other
 * processed operations remain in the devices output queue. Applications
 * implementing a "retrieve as many processed operations as possible" policy
 * can check this specific case and keep invoking the
 * rte_compressdev_dequeue_burst() function until a value less than
 * *nb_ops* is returned.
 *
 * The rte_compressdev_dequeue_burst() function does not provide any error
 * notification to avoid the corresponding overhead.
 *
 * @note: operation ordering is not maintained within the queue pair.
 *
 * @note: In case op status = OUT_OF_SPACE_TERMINATED, op.consumed=0 and the
 * op must be resubmitted with the same input data and a larger output buffer.
 * op.produced is usually 0, but in decompression cases a PMD may return > 0
 * and the application may find it useful to inspect that data.
 * This status is only returned on STATELESS ops.
 *
 * @note: In case op status = OUT_OF_SPACE_RECOVERABLE, op.produced can be used
 * and next op in stream should continue on from op.consumed+1 with a fresh
 * output buffer.
 * Consumed=0, produced=0 is an unusual but allowed case. There may be useful
 * state/history stored in the PMD, even though no output was produced yet.
 *
 *
 * @param dev_id
 *   Compress device identifier
 * @param qp_id
 *   The index of the queue pair from which to retrieve
 *   processed operations. The value must be in the range
 *   [0, nb_queue_pair - 1] previously supplied to
 *   rte_compressdev_configure()
 * @param ops
 *   The address of an array of pointers to
 *   *rte_comp_op* structures that must be
 *   large enough to store *nb_ops* pointers in it
 * @param nb_ops
 *   The maximum number of operations to dequeue
 * @return
 *   - The number of operations actually dequeued, which is the number
 *   of pointers to *rte_comp_op* structures effectively supplied to the
 *   *ops* array.
 */
uint16_t __rte_experimental
rte_compressdev_dequeue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_comp_op **ops, uint16_t nb_ops);

/**
 * Enqueue a burst of operations for processing on a compression device.
 *
 * The rte_compressdev_enqueue_burst() function is invoked to place
 * comp operations on the queue *qp_id* of the device designated by
 * its *dev_id*.
 *
 * The *nb_ops* parameter is the number of operations to process which are
 * supplied in the *ops* array of *rte_comp_op* structures.
 *
 * The rte_compressdev_enqueue_burst() function returns the number of
 * operations it actually enqueued for processing. A return value equal to
 * *nb_ops* means that all packets have been enqueued.
 *
 * @note All compression operations are Out-of-place (OOP) operations,
 * as the size of the output data is different to the size of the input data.
 *
 * @note The rte_comp_op contains both input and output parameters and is the
 * vehicle for the application to pass data into and out of the PMD. While an
 * op is inflight, i.e. once it has been enqueued, the private_xform or stream
 * attached to it and any mbufs or memory referenced by it should not be altered
 * or freed by the application. The PMD may use or change some of this data at
 * any time until it has been returned in a dequeue operation.
 *
 * @note The flush flag only applies to operations which return SUCCESS.
 * In OUT_OF_SPACE cases whether STATEFUL or STATELESS, data in dest buffer
 * is as if flush flag was FLUSH_NONE.
 * @note flush flag only applies in compression direction. It has no meaning
 * for decompression.
 * @note: operation ordering is not maintained within the queue pair.
 *
 * @param dev_id
 *   Compress device identifier
 * @param qp_id
 *   The index of the queue pair on which operations
 *   are to be enqueued for processing. The value
 *   must be in the range [0, nb_queue_pairs - 1]
 *   previously supplied to *rte_compressdev_configure*
 * @param ops
 *   The address of an array of *nb_ops* pointers
 *   to *rte_comp_op* structures which contain
 *   the operations to be processed
 * @param nb_ops
 *   The number of operations to process
 * @return
 *   The number of operations actually enqueued on the device. The return
 *   value can be less than the value of the *nb_ops* parameter when the
 *   comp devices queue is full or if invalid parameters are specified in
 *   a *rte_comp_op*.
 */
uint16_t __rte_experimental
rte_compressdev_enqueue_burst(uint8_t dev_id, uint16_t qp_id,
		struct rte_comp_op **ops, uint16_t nb_ops);

/**
 * This should alloc a stream from the device's mempool and initialise it.
 * The application should call this API when setting up for the stateful
 * processing of a set of data on a device. The API can be called multiple
 * times to set up a stream for each data set. The handle returned is only for
 * use with ops of op_type STATEFUL and must be passed to the PMD
 * with every op in the data stream
 *
 * @param dev_id
 *   Compress device identifier
 * @param xform
 *   xform data
 * @param stream
 *   Pointer to where PMD's private stream handle should be stored
 *
 * @return
 *  - 0 if successful and valid stream handle
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if comp device does not support STATEFUL operations.
 *  - Returns -ENOTSUP if comp device does not support the comp transform.
 *  - Returns -ENOMEM if the private stream could not be allocated.
 *
 */
int __rte_experimental
rte_compressdev_stream_create(uint8_t dev_id,
		const struct rte_comp_xform *xform,
		void **stream);

/**
 * This should clear the stream and return it to the device's mempool.
 *
 * @param dev_id
 *   Compress device identifier
 *
 * @param stream
 *   PMD's private stream data
 *
 * @return
 *  - 0 if successful
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if comp device does not support STATEFUL operations.
 *  - Returns -EBUSY if can't free stream as there are inflight operations
 */
int __rte_experimental
rte_compressdev_stream_free(uint8_t dev_id, void *stream);

/**
 * This should alloc a private_xform from the device's mempool and initialise
 * it. The application should call this API when setting up for stateless
 * processing on a device. If it returns non-shareable, then the appl cannot
 * share this handle with multiple in-flight ops and should call this API again
 * to get a separate handle for every in-flight op.
 * The handle returned is only valid for use with ops of op_type STATELESS.
 *
 * @param dev_id
 *   Compress device identifier
 * @param xform
 *   xform data
 * @param private_xform
 *   Pointer to where PMD's private_xform handle should be stored
 *
 * @return
 *  - if successful returns 0
 *    and valid private_xform handle
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if comp device does not support the comp transform.
 *  - Returns -ENOMEM if the private_xform could not be allocated.
 */
int __rte_experimental
rte_compressdev_private_xform_create(uint8_t dev_id,
		const struct rte_comp_xform *xform,
		void **private_xform);

/**
 * This should clear the private_xform and return it to the device's mempool.
 * It is the application's responsibility to ensure that private_xform data
 * is not cleared while there are still in-flight operations using it.
 *
 * @param dev_id
 *   Compress device identifier
 *
 * @param private_xform
 *   PMD's private_xform data
 *
 * @return
 *  - 0 if successful
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 */
int __rte_experimental
rte_compressdev_private_xform_free(uint8_t dev_id, void *private_xform);

#ifdef __cplusplus
}
#endif

#endif /* _RTE_COMPRESSDEV_H_ */
