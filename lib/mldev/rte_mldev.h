/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef RTE_MLDEV_H
#define RTE_MLDEV_H

/**
 * @file rte_mldev.h
 *
 * @warning
 * @b EXPERIMENTAL:
 * All functions in this file may be changed or removed without prior notice.
 *
 * ML (Machine Learning) device API.
 *
 * The ML framework is built on the following model:
 *
 *
 *     +-----------------+               rte_ml_[en|de]queue_burst()
 *     |                 |                          |
 *     |     Machine     o------+     +--------+    |
 *     |     Learning    |      |     | queue  |    |    +------+
 *     |     Inference   o------+-----o        |<===o===>|Core 0|
 *     |     Engine      |      |     | pair 0 |         +------+
 *     |                 o----+ |     +--------+
 *     |                 |    | |
 *     +-----------------+    | |     +--------+
 *              ^             | |     | queue  |         +------+
 *              |             | +-----o        |<=======>|Core 1|
 *              |             |       | pair 1 |         +------+
 *              |             |       +--------+
 *     +--------+--------+    |
 *     | +-------------+ |    |       +--------+
 *     | |   Model 0   | |    |       | queue  |         +------+
 *     | +-------------+ |    +-------o        |<=======>|Core N|
 *     | +-------------+ |            | pair N |         +------+
 *     | |   Model 1   | |            +--------+
 *     | +-------------+ |
 *     | +-------------+ |<------> rte_ml_model_load()
 *     | |   Model ..  | |-------> rte_ml_model_info_get()
 *     | +-------------+ |<------- rte_ml_model_start()
 *     | +-------------+ |<------- rte_ml_model_stop()
 *     | |   Model N   | |<------- rte_ml_model_params_update()
 *     | +-------------+ |<------- rte_ml_model_unload()
 *     +-----------------+
 *
 * ML Device: A hardware or software-based implementation of ML device API for
 * running inferences using a pre-trained ML model.
 *
 * ML Model: An ML model is an algorithm trained over a dataset. A model consists of
 * procedure/algorithm and data/pattern required to make predictions on live data.
 * Once the model is created and trained outside of the DPDK scope, the model can be loaded
 * via rte_ml_model_load() and then start it using rte_ml_model_start() API.
 * The rte_ml_model_params_update() can be used to update the model parameters such as weight
 * and bias without unloading the model using rte_ml_model_unload().
 *
 * ML Inference: ML inference is the process of feeding data to the model via
 * rte_ml_enqueue_burst() API and use rte_ml_dequeue_burst() API to get the calculated
 * outputs/predictions from the started model.
 *
 * In all functions of the ML device API, the ML device is designated by an
 * integer >= 0 named as device identifier *dev_id*.
 *
 * The functions exported by the ML device API to setup a device designated by
 * its device identifier must be invoked in the following order:
 *
 *      - rte_ml_dev_configure()
 *      - rte_ml_dev_queue_pair_setup()
 *      - rte_ml_dev_start()
 *
 * A model is required to run the inference operations with the user specified inputs.
 * Application needs to invoke the ML model API in the following order before queueing
 * inference jobs.
 *
 *      - rte_ml_model_load()
 *      - rte_ml_model_start()
 *
 * A model can be loaded on a device only after the device has been configured and can be
 * started or stopped only after a device has been started.
 *
 * The rte_ml_model_info_get() API is provided to retrieve the information related to the model.
 * The information would include the shape and type of input and output required for the inference.
 *
 * Data quantization and dequantization is one of the main aspects in ML domain. This involves
 * conversion of input data from a higher precision to a lower precision data type and vice-versa
 * for the output. APIs are provided to enable quantization through rte_ml_io_quantize() and
 * dequantization through rte_ml_io_dequantize(). These APIs have the capability to handle input
 * and output buffers holding data for multiple batches.
 *
 * Two utility APIs rte_ml_io_input_size_get() and rte_ml_io_output_size_get() can used to get the
 * size of quantized and de-quantized multi-batch input and output buffers.
 *
 * User can optionally update the model parameters with rte_ml_model_params_update() after
 * invoking rte_ml_model_stop() API on a given model ID.
 *
 * The application can invoke, in any order, the functions exported by the ML API to enqueue
 * inference jobs and dequeue inference response.
 *
 * If the application wants to change the device configuration (i.e., call
 * rte_ml_dev_configure() or rte_ml_dev_queue_pair_setup()), then application must stop the
 * device using rte_ml_dev_stop() API. Likewise, if model parameters need to be updated then
 * the application must call rte_ml_model_stop() followed by rte_ml_model_params_update() API
 * for the given model. The application does not need to call rte_ml_dev_stop() API for
 * any model re-configuration such as rte_ml_model_params_update(), rte_ml_model_unload() etc.
 *
 * Once the device is in the start state after invoking rte_ml_dev_start() API and the model is in
 * start state after invoking rte_ml_model_start() API, then the application can call
 * rte_ml_enqueue_burst() and rte_ml_dequeue_burst() API on the destined device and model ID.
 *
 * Finally, an application can close an ML device by invoking the rte_ml_dev_close() function.
 *
 * Typical application utilisation of the ML API will follow the following
 * programming flow.
 *
 * - rte_ml_dev_configure()
 * - rte_ml_dev_queue_pair_setup()
 * - rte_ml_model_load()
 * - rte_ml_dev_start()
 * - rte_ml_model_start()
 * - rte_ml_model_info_get()
 * - rte_ml_enqueue_burst()
 * - rte_ml_dequeue_burst()
 * - rte_ml_model_stop()
 * - rte_ml_model_unload()
 * - rte_ml_dev_stop()
 * - rte_ml_dev_close()
 *
 * Regarding multi-threading, by default, all the functions of the ML Device API exported by a PMD
 * are lock-free functions which assume to not be invoked in parallel on different logical cores
 * on the same target object. For instance, the dequeue function of a poll mode driver cannot be
 * invoked in parallel on two logical cores to operate on same queue pair. Of course, this function
 * can be invoked in parallel by different logical core on different queue pair.
 * It is the responsibility of the user application to enforce this rule.
 */

#include <rte_common.h>
#include <rte_log.h>
#include <rte_mempool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Logging Macro */
extern int rte_ml_dev_logtype;
#define RTE_LOGTYPE_MLDEV rte_ml_dev_logtype

#define RTE_MLDEV_LOG(level, fmt, args...) \
	RTE_LOG_LINE(level, MLDEV, "%s(): " fmt, __func__, ##args)

#define RTE_ML_STR_MAX 128
/**< Maximum length of name string */

#define RTE_MLDEV_DEFAULT_MAX 32
/** Maximum number of devices if rte_ml_dev_init() is not called. */

/* Device operations */

/**
 * Initialize the device array before probing devices. If not called, the first device probed would
 * initialize the array to a size of RTE_MLDEV_DEFAULT_MAX.
 *
 * @param dev_max
 *   Maximum number of devices.
 *
 * @return
 *   0 on success, -rte_errno otherwise:
 *   - ENOMEM if out of memory
 *   - EINVAL if 0 size
 *   - EBUSY if already initialized
 */
__rte_experimental
int
rte_ml_dev_init(size_t dev_max);

/**
 * Get the total number of ML devices that have been successfully initialised.
 *
 * @return
 *   - The total number of usable ML devices.
 */
__rte_experimental
uint16_t
rte_ml_dev_count(void);

/**
 * Check if the device is in ready state.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - 0 if device state is not in ready state.
 *   - 1 if device state is ready state.
 */
__rte_experimental
int
rte_ml_dev_is_valid_dev(int16_t dev_id);

/**
 * Return the NUMA socket to which a device is connected.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - The NUMA socket id to which the device is connected
 *   - 0 If the socket could not be determined.
 *   - -EINVAL: if the dev_id value is not valid.
 */
__rte_experimental
int
rte_ml_dev_socket_id(int16_t dev_id);

/**  ML device information */
struct rte_ml_dev_info {
	const char *driver_name;
	/**< Driver name */
	uint16_t max_models;
	/**< Maximum number of models supported by the device.
	 * @see struct rte_ml_dev_config::nb_models
	 */
	uint16_t max_queue_pairs;
	/**< Maximum number of queues pairs supported by the device.
	 * @see struct rte_ml_dev_config::nb_queue_pairs
	 */
	uint16_t max_desc;
	/**< Maximum allowed number of descriptors for queue pair by the device.
	 * @see struct rte_ml_dev_qp_conf::nb_desc
	 */
	uint16_t max_io;
	/**< Maximum number of inputs/outputs supported per model. */
	uint16_t max_segments;
	/**< Maximum number of scatter-gather entries supported by the device.
	 * @see struct rte_ml_buff_seg  struct rte_ml_buff_seg::next
	 */
	uint16_t align_size;
	/**< Alignment size of IO buffers used by the device. */
};

/**
 * Retrieve the information of the device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param dev_info
 *   A pointer to a structure of type *rte_ml_dev_info* to be filled with the info of the device.
 *
 * @return
 *   - 0: Success, driver updates the information of the ML device
 *   - < 0: Error code returned by the driver info get function.
 */
__rte_experimental
int
rte_ml_dev_info_get(int16_t dev_id, struct rte_ml_dev_info *dev_info);

/** ML device configuration structure */
struct rte_ml_dev_config {
	int socket_id;
	/**< Socket to allocate resources on. */
	uint16_t nb_models;
	/**< Number of models to be loaded on the device.
	 * This value cannot exceed the max_models which is previously provided in
	 * struct rte_ml_dev_info::max_models
	 */
	uint16_t nb_queue_pairs;
	/**< Number of queue pairs to configure on this device.
	 * This value cannot exceed the max_models which is previously provided in
	 * struct rte_ml_dev_info::max_queue_pairs
	 */
};

/**
 * Configure an ML device.
 *
 * This function must be invoked first before any other function in the API.
 *
 * ML Device can be re-configured, when in a stopped state. Device cannot be re-configured after
 * rte_ml_dev_close() is called.
 *
 * The caller may use rte_ml_dev_info_get() to get the capability of each resources available for
 * this ML device.
 *
 * @param dev_id
 *   The identifier of the device to configure.
 * @param config
 *   The ML device configuration structure.
 *
 * @return
 *   - 0: Success, device configured.
 *   - < 0: Error code returned by the driver configuration function.
 */
__rte_experimental
int
rte_ml_dev_configure(int16_t dev_id, const struct rte_ml_dev_config *config);

/* Forward declaration */
struct rte_ml_op;

/**< Callback function called during rte_ml_dev_stop(), invoked once per flushed ML op */
typedef void (*rte_ml_dev_stop_flush_t)(int16_t dev_id, uint16_t qp_id, struct rte_ml_op *op);

/** ML device queue pair configuration structure. */
struct rte_ml_dev_qp_conf {
	uint32_t nb_desc;
	/**< Number of descriptors per queue pair.
	 * This value cannot exceed the max_desc which previously provided in
	 * struct rte_ml_dev_info:max_desc
	 */
	rte_ml_dev_stop_flush_t cb;
	/**< Callback function called during rte_ml_dev_stop(), invoked once per active ML op.
	 * Value NULL is allowed, in which case callback will not be invoked.
	 * This function can be used to properly dispose of outstanding ML ops from all
	 * queue pairs, for example ops containing  memory pointers.
	 * @see rte_ml_dev_stop()
	 */
};

/**
 * Set up a queue pair for a device. This should only be called when the device is stopped.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param queue_pair_id
 *   The index of the queue pairs to set up. The value must be in the range [0, nb_queue_pairs - 1]
 * previously supplied to rte_ml_dev_configure().
 * @param qp_conf
 *   The pointer to the configuration data to be used for the queue pair.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of NUMA.
 * The value can be *SOCKET_ID_ANY* if there is no NUMA constraint for the memory allocated
 * for the queue pair.
 *
 * @return
 *   - 0: Success, queue pair correctly set up.
 *   - < 0: Queue pair configuration failed.
 */
__rte_experimental
int
rte_ml_dev_queue_pair_setup(int16_t dev_id, uint16_t queue_pair_id,
			    const struct rte_ml_dev_qp_conf *qp_conf, int socket_id);

/**
 * Start an ML device.
 *
 * The device start step consists of setting the configured features and enabling the ML device
 * to accept inference jobs.
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - 0: Success, device started.
 *   - <0: Error code of the driver device start function.
 */
__rte_experimental
int
rte_ml_dev_start(int16_t dev_id);

/**
 * Stop an ML device. A stopped device cannot accept inference jobs.
 * The device can be restarted with a call to rte_ml_dev_start().
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *   - 0: Success, device stopped.
 *   - <0: Error code of the driver device stop function.
 */
__rte_experimental
int
rte_ml_dev_stop(int16_t dev_id);

/**
 * Close an ML device. The device cannot be restarted!
 *
 * @param dev_id
 *   The identifier of the device.
 *
 * @return
 *  - 0 on successfully closing device.
 *  - <0 on failure to close device.
 */
__rte_experimental
int
rte_ml_dev_close(int16_t dev_id);

/** Status of ML operation */
enum rte_ml_op_status {
	RTE_ML_OP_STATUS_SUCCESS = 0,
	/**< Operation completed successfully */
	RTE_ML_OP_STATUS_NOT_PROCESSED,
	/**< Operation has not yet been processed by the device. */
	RTE_ML_OP_STATUS_ERROR,
	/**< Operation completed with error.
	 * Application can invoke rte_ml_op_error_get() to get PMD specific
	 * error code if needed.
	 */
};

/** ML operation's input and output buffer representation as scatter gather list
 */
struct rte_ml_buff_seg {
	rte_iova_t iova_addr;
	/**< IOVA address of segment buffer. */
	void *addr;
	/**< Virtual address of segment buffer. */
	uint32_t length;
	/**< Segment length. */
	uint32_t reserved;
	/**< Reserved for future use. */
	struct rte_ml_buff_seg *next;
	/**< Points to next segment. Value NULL represents the last segment. */
};

/**
 * ML Operation.
 *
 * This structure contains data related to performing an ML operation on the buffers using
 * the model specified through model_id.
 */
struct rte_ml_op {
	uint16_t model_id;
	/**< Model ID to be used for the operation. */
	uint16_t nb_batches;
	/**< Number of batches. Minimum value must be one.
	 * Input buffer must hold inference data for each batch as contiguous.
	 */
	uint32_t reserved;
	/**< Reserved for future use. */
	struct rte_mempool *mempool;
	/**< Pool from which operation is allocated. */
	struct rte_ml_buff_seg **input;
	/**< Array of buffer segments to hold the inference input data.
	 *
	 * When the model supports IO layout RTE_ML_IO_LAYOUT_PACKED, size of
	 * the array is 1.
	 *
	 * When the model supports IO layout RTE_ML_IO_LAYOUT_SPLIT, size of
	 * the array is rte_ml_model_info::nb_inputs.
	 *
	 * @see struct rte_ml_dev_info::io_layout
	 */
	struct rte_ml_buff_seg **output;
	/**< Array of buffer segments to hold the inference output data.
	 *
	 * When the model supports IO layout RTE_ML_IO_LAYOUT_PACKED, size of
	 * the array is 1.
	 *
	 * When the model supports IO layout RTE_ML_IO_LAYOUT_SPLIT, size of
	 * the array is rte_ml_model_info::nb_outputs.
	 *
	 * @see struct rte_ml_dev_info::io_layout
	 */
	union {
		uint64_t user_u64;
		/**< User data as uint64_t.*/
		void *user_ptr;
		/**< User data as void*.*/
	};
	enum rte_ml_op_status status;
	/**< Operation status. */
	uint64_t impl_opaque;
	/**< Implementation specific opaque value.
	 * An implementation may use this field to hold
	 * implementation specific value to share between
	 * dequeue and enqueue operation.
	 * The application should not modify this field.
	 */
} __rte_cache_aligned;

/* Enqueue/Dequeue operations */

/**
 * Enqueue a burst of ML inferences for processing on an ML device.
 *
 * The rte_ml_enqueue_burst() function is invoked to place ML inference
 * operations on the queue *qp_id* of the device designated by its *dev_id*.
 *
 * The *nb_ops* parameter is the number of inferences to process which are
 * supplied in the *ops* array of *rte_ml_op* structures.
 *
 * The rte_ml_enqueue_burst() function returns the number of inferences it
 * actually enqueued for processing. A return value equal to *nb_ops* means that
 * all packets have been enqueued.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param qp_id
 *   The index of the queue pair which inferences are to be enqueued for processing.
 * The value must be in the range [0, nb_queue_pairs - 1] previously supplied to
 * *rte_ml_dev_configure*.
 * @param ops
 *   The address of an array of *nb_ops* pointers to *rte_ml_op* structures which contain the
 * ML inferences to be processed.
 * @param nb_ops
 *   The number of operations to process.
 *
 * @return
 *   The number of inference operations actually enqueued to the ML device.
 * The return value can be less than the value of the *nb_ops* parameter when the ML device queue
 * is full or if invalid parameters are specified in a *rte_ml_op*.
 */
__rte_experimental
uint16_t
rte_ml_enqueue_burst(int16_t dev_id, uint16_t qp_id, struct rte_ml_op **ops, uint16_t nb_ops);

/**
 * Dequeue a burst of processed ML inferences operations from a queue on the ML device.
 * The dequeued operations are stored in *rte_ml_op* structures whose pointers are supplied
 * in the *ops* array.
 *
 * The rte_ml_dequeue_burst() function returns the number of inferences actually dequeued,
 * which is the number of *rte_ml_op* data structures effectively supplied into the *ops* array.
 *
 * A return value equal to *nb_ops* indicates that the queue contained at least nb_ops* operations,
 * and this is likely to signify that other processed operations remain in the devices output queue.
 * Application implementing a "retrieve as many processed operations as possible" policy can check
 * this specific case and keep invoking the rte_ml_dequeue_burst() function until a value less than
 * *nb_ops* is returned.
 *
 * The rte_ml_dequeue_burst() function does not provide any error notification to avoid
 * the corresponding overhead.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param qp_id
 *   The index of the queue pair from which to retrieve processed packets.
 * The value must be in the range [0, nb_queue_pairs - 1] previously supplied to
 * rte_ml_dev_configure().
 * @param ops
 *   The address of an array of pointers to *rte_ml_op* structures that must be large enough to
 * store *nb_ops* pointers in it.
 * @param nb_ops
 *   The maximum number of inferences to dequeue.
 *
 * @return
 *   The number of operations actually dequeued, which is the number of pointers
 * to *rte_ml_op* structures effectively supplied to the *ops* array.
 */
__rte_experimental
uint16_t
rte_ml_dequeue_burst(int16_t dev_id, uint16_t qp_id, struct rte_ml_op **ops, uint16_t nb_ops);

/**
 * Verbose error structure definition.
 */
struct rte_ml_op_error {
	char message[RTE_ML_STR_MAX]; /**< Human-readable error message. */
	uint64_t errcode;	      /**< Vendor specific error code. */
};

/**
 * Get PMD specific error information for an ML op.
 *
 * When an ML operation completed with RTE_ML_OP_STATUS_ERROR as status,
 * This API allows to get PMD specific error details.
 *
 * @param[in] dev_id
 *   Device identifier
 * @param[in] op
 *   Handle of ML operation
 * @param[in] error
 *   Address of structure rte_ml_op_error to be filled
 *
 * @return
 *   - Returns 0 on success
 *   - Returns negative value on failure
 */
__rte_experimental
int
rte_ml_op_error_get(int16_t dev_id, struct rte_ml_op *op, struct rte_ml_op_error *error);

/* Statistics operations */

/** Device statistics. */
struct rte_ml_dev_stats {
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
 * Retrieve the general I/O statistics of a device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param stats
 *   Pointer to structure to where statistics will be copied.
 * On error, this location may or may not have been modified.
 * @return
 *   - 0 on success
 *   - -EINVAL: If invalid parameter pointer is provided.
 */
__rte_experimental
int
rte_ml_dev_stats_get(int16_t dev_id, struct rte_ml_dev_stats *stats);

/**
 * Reset the statistics of a device.
 *
 * @param dev_id
 *   The identifier of the device.
 */
__rte_experimental
void
rte_ml_dev_stats_reset(int16_t dev_id);

/**
 * Selects the component of the mldev to retrieve statistics from.
 */
enum rte_ml_dev_xstats_mode {
	RTE_ML_DEV_XSTATS_DEVICE,
	/**< Device xstats */
	RTE_ML_DEV_XSTATS_MODEL,
	/**< Model xstats */
};

/**
 * A name-key lookup element for extended statistics.
 *
 * This structure is used to map between names and ID numbers for extended ML device statistics.
 */
struct rte_ml_dev_xstats_map {
	uint16_t id;
	/**< xstat identifier */
	char name[RTE_ML_STR_MAX];
	/**< xstat name */
};

/**
 * Retrieve names of extended statistics of an ML device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param mode
 *   Mode of statistics to retrieve. Choices include the device statistics and model statistics.
 * @param model_id
 *   Used to specify the model number in model mode, and is ignored in device mode.
 * @param[out] xstats_map
 *   Block of memory to insert names and ids into. Must be at least size in capacity. If set to
 * NULL, function returns required capacity. The id values returned can be passed to
 * *rte_ml_dev_xstats_get* to select statistics.
 * @param size
 *   Capacity of xstats_names (number of xstats_map).
 * @return
 *   - Positive value lower or equal to size: success. The return value is the number of entries
 * filled in the stats table.
 *   - Positive value higher than size: error, the given statistics table is too small. The return
 * value corresponds to the size that should be given to succeed. The entries in the table are not
 * valid and shall not be used by the caller.
 *   - Negative value on error:
 *        -ENODEV for invalid *dev_id*.
 *        -EINVAL for invalid mode, model parameters.
 *        -ENOTSUP if the device doesn't support this function.
 */
__rte_experimental
int
rte_ml_dev_xstats_names_get(int16_t dev_id, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
			    struct rte_ml_dev_xstats_map *xstats_map, uint32_t size);

/**
 * Retrieve the value of a single stat by requesting it by name.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param name
 *   Name of stat name to retrieve.
 * @param[out] stat_id
 *   If non-NULL, the numerical id of the stat will be returned, so that further requests for the
 * stat can be got using rte_ml_dev_xstats_get, which will be faster as it doesn't need to scan a
 * list of names for the stat. If the stat cannot be found, the id returned will be (unsigned)-1.
 * @param[out] value
 *   Value of the stat to be returned.
 * @return
 *   - Zero: No error.
 *   - Negative value: -EINVAL if stat not found, -ENOTSUP if not supported.
 */
__rte_experimental
int
rte_ml_dev_xstats_by_name_get(int16_t dev_id, const char *name, uint16_t *stat_id, uint64_t *value);

/**
 * Retrieve extended statistics of an ML device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param mode
 *  Mode of statistics to retrieve. Choices include the device statistics and model statistics.
 * @param model_id
 *   Used to specify the model id in model mode, and is ignored in device mode.
 * @param stat_ids
 *   ID numbers of the stats to get. The ids can be got from the stat position in the stat list from
 * rte_ml_dev_xstats_names_get(), or by using rte_ml_dev_xstats_by_name_get().
 * @param[out] values
 *   Values for each stats request by ID.
 * @param nb_ids
 *   Number of stats requested.
 * @return
 *   - Positive value: number of stat entries filled into the values array
 *   - Negative value on error:
 *        -ENODEV for invalid *dev_id*.
 *        -EINVAL for invalid mode, model id or stat id parameters.
 *        -ENOTSUP if the device doesn't support this function.
 */
__rte_experimental
int
rte_ml_dev_xstats_get(int16_t dev_id, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
		      const uint16_t stat_ids[], uint64_t values[], uint16_t nb_ids);

/**
 * Reset the values of the xstats of the selected component in the device.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param mode
 *   Mode of the statistics to reset. Choose from device or model.
 * @param model_id
 *   Model stats to reset. 0 and positive values select models, while -1 indicates all models.
 * @param stat_ids
 *   Selects specific statistics to be reset. When NULL, all statistics selected by *mode* will be
 * reset. If non-NULL, must point to array of at least *nb_ids* size.
 * @param nb_ids
 *   The number of ids available from the *ids* array. Ignored when ids is NULL.
 * @return
 *   - Zero: successfully reset the statistics.
 *   - Negative value: -EINVAL invalid parameters, -ENOTSUP if not supported.
 */
__rte_experimental
int
rte_ml_dev_xstats_reset(int16_t dev_id, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
			const uint16_t stat_ids[], uint16_t nb_ids);

/**
 * Dump internal information about *dev_id* to the FILE* provided in *fd*.
 *
 * @param dev_id
 *   The identifier of the device.
 * @param fd
 *   A pointer to a file for output.
 * @return
 *   - 0: on success.
 *   - <0: on failure.
 */
__rte_experimental
int
rte_ml_dev_dump(int16_t dev_id, FILE *fd);

/**
 * Trigger the ML device self test.
 *
 * @param dev_id
 *   The identifier of the device.
 * @return
 *   - 0: Selftest successful.
 *   - -ENOTSUP: if the device doesn't support selftest.
 *   - other values < 0 on failure.
 */
__rte_experimental
int
rte_ml_dev_selftest(int16_t dev_id);

/* Model operations */

/** ML model load parameters
 *
 * Parameters required to load an ML model.
 */
struct rte_ml_model_params {
	void *addr;
	/**< Address of model buffer */
	size_t size;
	/**< Size of model buffer */
};

/**
 * Load an ML model to the device.
 *
 * Load an ML model to the device with parameters requested in the structure rte_ml_model_params.
 *
 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] params
 *   Parameters for the model to be loaded.
 * @param[out] model_id
 *   Identifier of the model loaded.
 *
 * @return
 *   - 0: Success, Model loaded.
 *   - < 0: Failure, Error code of the model load driver function.
 */
__rte_experimental
int
rte_ml_model_load(int16_t dev_id, struct rte_ml_model_params *params, uint16_t *model_id);

/**
 * Unload an ML model from the device.
 *
 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] model_id
 *   Identifier of the model to be unloaded.
 *
 * @return
 *   - 0: Success, Model unloaded.
 *   - < 0: Failure, Error code of the model unload driver function.
 */
__rte_experimental
int
rte_ml_model_unload(int16_t dev_id, uint16_t model_id);

/**
 * Start an ML model for the given device ID.
 *
 * Start an ML model to accept inference requests.
 *
 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] model_id
 *   Identifier of the model to be started.
 *
 * @return
 *   - 0: Success, Model loaded.
 *   - < 0: Failure, Error code of the model start driver function.
 */
__rte_experimental
int
rte_ml_model_start(int16_t dev_id, uint16_t model_id);

/**
 * Stop an ML model for the given device ID.
 *
 * Model stop would disable the ML model to be used for inference jobs.
 * All inference jobs must have been completed before model stop is attempted.

 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] model_id
 *   Identifier of the model to be stopped.
 *
 * @return
 *   - 0: Success, Model unloaded.
 *   - < 0: Failure, Error code of the model stop driver function.
 */
__rte_experimental
int
rte_ml_model_stop(int16_t dev_id, uint16_t model_id);

/**
 * Input and output data types. ML models can operate on reduced precision
 * datatypes to achieve better power efficiency, lower network latency and lower memory footprint.
 * This enum is used to represent the lower precision integer and floating point types used
 * by ML models.
 */
enum rte_ml_io_type {
	RTE_ML_IO_TYPE_UNKNOWN = 0,
	/**< Invalid or unknown type */
	RTE_ML_IO_TYPE_INT8,
	/**< 8-bit integer */
	RTE_ML_IO_TYPE_UINT8,
	/**< 8-bit unsigned integer */
	RTE_ML_IO_TYPE_INT16,
	/**< 16-bit integer */
	RTE_ML_IO_TYPE_UINT16,
	/**< 16-bit unsigned integer */
	RTE_ML_IO_TYPE_INT32,
	/**< 32-bit integer */
	RTE_ML_IO_TYPE_UINT32,
	/**< 32-bit unsigned integer */
	RTE_ML_IO_TYPE_FP8,
	/**< 8-bit floating point number */
	RTE_ML_IO_TYPE_FP16,
	/**< IEEE 754 16-bit floating point number */
	RTE_ML_IO_TYPE_FP32,
	/**< IEEE 754 32-bit floating point number */
	RTE_ML_IO_TYPE_BFLOAT16
	/**< 16-bit brain floating point number. */
};

/** ML I/O buffer layout */
enum rte_ml_io_layout {
	RTE_ML_IO_LAYOUT_PACKED,
	/**< All inputs for the model should packed in a single buffer with
	 * no padding between individual inputs. The buffer is expected to
	 * be aligned to rte_ml_dev_info::align_size.
	 *
	 * When I/O segmentation is supported by the device, the packed
	 * data can be split into multiple segments. In this case, each
	 * segment is expected to be aligned to rte_ml_dev_info::align_size
	 *
	 * Same applies to output.
	 *
	 * @see struct rte_ml_dev_info::max_segments
	 */
	RTE_ML_IO_LAYOUT_SPLIT
	/**< Each input for the model should be stored as separate buffers
	 * and each input should be aligned to rte_ml_dev_info::align_size.
	 *
	 * When I/O segmentation is supported, each input can be split into
	 * multiple segments. In this case, each segment is expected to be
	 * aligned to rte_ml_dev_info::align_size
	 *
	 * Same applies to output.
	 *
	 * @see struct rte_ml_dev_info::max_segments
	 */
};

/**
 * Input and output data information structure
 *
 * Specifies the type and shape of input and output data.
 */
struct rte_ml_io_info {
	char name[RTE_ML_STR_MAX];
	/**< Name of data */
	uint32_t nb_dims;
	/**< Number of dimensions in shape */
	uint32_t *shape;
	/**< Shape of the tensor for rte_ml_model_info::min_batches of the model. */
	enum rte_ml_io_type type;
	/**< Type of data
	 * @see enum rte_ml_io_type
	 */
	uint64_t nb_elements;
	/** Number of elements in tensor */
	uint64_t size;
	/** Size of tensor in bytes */
};

/** Model information structure */
struct rte_ml_model_info {
	char name[RTE_ML_STR_MAX];
	/**< Model name. */
	char version[RTE_ML_STR_MAX];
	/**< Model version */
	uint16_t model_id;
	/**< Model ID */
	uint16_t device_id;
	/**< Device ID */
	enum rte_ml_io_layout io_layout;
	/**< I/O buffer layout for the model */
	uint16_t min_batches;
	/**< Minimum number of batches that the model can process
	 * in one inference request
	 */
	uint16_t max_batches;
	/**< Maximum number of batches that the model can process
	 * in one inference request
	 */
	uint32_t nb_inputs;
	/**< Number of inputs */
	const struct rte_ml_io_info *input_info;
	/**< Input info array. Array size is equal to nb_inputs */
	uint32_t nb_outputs;
	/**< Number of outputs */
	const struct rte_ml_io_info *output_info;
	/**< Output info array. Array size is equal to nb_output */
	uint64_t wb_size;
	/**< Size of model weights and bias */
};

/**
 * Get ML model information.
 *
 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] model_id
 *   Identifier for the model created
 * @param[out] model_info
 *   Pointer to a model info structure
 *
 * @return
 *   - Returns 0 on success
 *   - Returns negative value on failure
 */
__rte_experimental
int
rte_ml_model_info_get(int16_t dev_id, uint16_t model_id, struct rte_ml_model_info *model_info);

/**
 * Update the model parameters without unloading model.
 *
 * Update model parameters such as weights and bias without unloading the model.
 * rte_ml_model_stop() must be called before invoking this API.
 *
 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] model_id
 *   Identifier for the model created
 * @param[in] buffer
 *   Pointer to the model weights and bias buffer.
 * Size of the buffer is equal to wb_size returned in *rte_ml_model_info*.
 *
 * @return
 *   - Returns 0 on success
 *   - Returns negative value on failure
 */
__rte_experimental
int
rte_ml_model_params_update(int16_t dev_id, uint16_t model_id, void *buffer);

/* IO operations */

/**
 * Quantize input data.
 *
 * Quantization converts data from a higher precision types to a lower precision types to improve
 * the throughput and efficiency of the model execution with minimal loss of accuracy.
 * Types of dequantized data and quantized data are specified by the model.
 *
 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] model_id
 *   Identifier for the model
 * @param[in] dbuffer
 *   Address of dequantized input data
 * @param[in] qbuffer
 *   Address of quantized input data
 *
 * @return
 *   - Returns 0 on success
 *   - Returns negative value on failure
 */
__rte_experimental
int
rte_ml_io_quantize(int16_t dev_id, uint16_t model_id, struct rte_ml_buff_seg **dbuffer,
		   struct rte_ml_buff_seg **qbuffer);

/**
 * Dequantize output data.
 *
 * Dequantization converts data from a lower precision type to a higher precision type.
 * Types of quantized data and dequantized are specified by the model.
 *
 * @param[in] dev_id
 *   The identifier of the device.
 * @param[in] model_id
 *   Identifier for the model
 * @param[in] qbuffer
 *   Address of quantized output data
 * @param[in] dbuffer
 *   Address of dequantized output data
 *
 * @return
 *   - Returns 0 on success
 *   - Returns negative value on failure
 */
__rte_experimental
int
rte_ml_io_dequantize(int16_t dev_id, uint16_t model_id, struct rte_ml_buff_seg **qbuffer,
		     struct rte_ml_buff_seg **dbuffer);

/* ML op pool operations */

/**
 * Create an ML operation pool
 *
 * @param name
 *   ML operations pool name
 * @param nb_elts
 *   Number of elements in pool
 * @param cache_size
 *   Number of elements to cache on lcore, see
 *   *rte_mempool_create* for further details about cache size
 * @param user_size
 *   Size of private data to allocate for user with each operation
 * @param socket_id
 *   Socket to identifier allocate memory on
 * @return
 *  - On success pointer to mempool
 *  - On failure NULL
 */
__rte_experimental
struct rte_mempool *
rte_ml_op_pool_create(const char *name, unsigned int nb_elts, unsigned int cache_size,
		      uint16_t user_size, int socket_id);

/**
 * Free an ML operation pool
 *
 * @param mempool
 *   A pointer to the mempool structure.
 *   If NULL then, the function does nothing.
 */
__rte_experimental
void
rte_ml_op_pool_free(struct rte_mempool *mempool);

#ifdef __cplusplus
}
#endif

#endif /* RTE_MLDEV_H */
