/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef RTE_MLDEV_INTERNAL_H
#define RTE_MLDEV_INTERNAL_H

/**
 * @file
 *
 * MLDEV internal header
 *
 * This file contains MLDEV private data structures and macros.
 *
 * @note
 * These APIs are for MLDEV PMDs and library only.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#include <dev_driver.h>
#include <rte_common.h>
#include <rte_log.h>
#include <rte_mldev.h>

/* Device state */
#define ML_DEV_DETACHED (0)
#define ML_DEV_ATTACHED (1)

struct rte_ml_dev;

/**
 * @internal
 *
 * Enqueue a burst of inference requests to a queue on ML device.
 *
 * @param dev
 *	ML device pointer.
 * @param qp_id
 *	Queue-pair ID.
 * @param ops
 *	Array of ML ops to be enqueued.
 * @param nb_ops
 *	Number of ops to enqueue.
 *
 * @return
 *	- Number of ops enqueued.
 */
typedef uint16_t (*mldev_enqueue_t)(struct rte_ml_dev *dev, uint16_t qp_id, struct rte_ml_op **ops,
				    uint16_t nb_ops);

/**
 * @internal
 *
 * Dequeue a burst of inference requests from a queue on ML device.
 *
 * @param dev
 *	ML device pointer.
 * @param qp_id
 *	Queue-pair ID.
 * @param ops
 *	Array of ML ops to dequeued.
 * @param nb_ops
 *	Number of ops to dequeue.
 *
 * @return
 *	- Number of ops dequeued.
 */
typedef uint16_t (*mldev_dequeue_t)(struct rte_ml_dev *dev, uint16_t qp_id, struct rte_ml_op **ops,
				    uint16_t nb_ops);

/**
 * @internal
 *
 * Get error information for an Op.
 *
 * @param dev
 *	ML device pointer.
 * @param op
 *	ML Op handle.
 * @param error
 *	Pointer to error structure.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_op_error_get_t)(struct rte_ml_dev *dev, struct rte_ml_op *op,
				    struct rte_ml_op_error *error);

/**
 * Definitions of all functions exported by a driver through the generic structure of type
 * *ml_dev_ops* supplied in the *rte_ml_dev* structure associated with a device.
 */

/**
 * @internal
 *
 * Function used to get device information.
 *
 * @param dev
 *	ML device pointer.
 * @param dev_info
 *	Pointer to info structure.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error code on failure.
 */
typedef int (*mldev_info_get_t)(struct rte_ml_dev *dev, struct rte_ml_dev_info *dev_info);

/**
 * @internal
 *
 * Function used to configure device.
 *
 * @param dev
 *	ML device pointer.
 * @param config
 *	ML device configurations.
 *
 * @return
 *	- 0 on success
 *	- < 0, error code on failure.
 */
typedef int (*mldev_configure_t)(struct rte_ml_dev *dev, const struct rte_ml_dev_config *config);

/**
 * @internal
 *
 * Function used to close a configured device.
 *
 * @param dev
 *	ML device pointer.
 *
 * @return
 *	- 0 on success.
 *	- -EAGAIN if can't close as device is busy.
 *	- < 0, error code on failure, other than busy.
 */
typedef int (*mldev_close_t)(struct rte_ml_dev *dev);

/**
 * @internal
 *
 * Function used to start a configured device.
 *
 * @param dev
 *	ML device pointer.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error code on failure.
 */
typedef int (*mldev_start_t)(struct rte_ml_dev *dev);

/**
 * @internal
 *
 * Function used to stop a configured device.
 *
 * @param dev
 *	ML device pointer.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error code on failure.
 */
typedef int (*mldev_stop_t)(struct rte_ml_dev *dev);

/**
 * @internal
 *
 * Setup a queue pair for a device.
 *
 * @param dev
 *	ML device pointer.
 * @param queue_pair_id
 *	Queue pair index.
 * @param queue_pair_conf
 *	Queue pair configuration structure.
 * @param socket_id
 *	Socket index.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error on failure.
 */
typedef int (*mldev_queue_pair_setup_t)(struct rte_ml_dev *dev, uint16_t queue_pair_id,
					const struct rte_ml_dev_qp_conf *queue_pair_conf,
					int socket_id);

/**
 * @internal
 *
 * Release memory resources allocated by given queue pair.
 *
 * @param dev
 *	ML device pointer.
 * @param queue_pair_id
 *	Queue pair index.
 *
 * @return
 *	- 0 on success.
 *	- -EAGAIN, if can't close as device is busy.
 */
typedef int (*mldev_queue_pair_release_t)(struct rte_ml_dev *dev, uint16_t queue_pair_id);

/**
 * @internal
 *
 * Function used to get device statistics.
 *
 * @param dev
 *	ML device pointer.
 * @param stats
 *	Pointer to ML device stats structure to update.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error on failure.
 */
typedef int (*mldev_stats_get_t)(struct rte_ml_dev *dev, struct rte_ml_dev_stats *stats);

/**
 * @internal
 *
 * Function used to reset device statistics.
 *
 * @param dev
 *	ML device pointer.
 */
typedef void (*mldev_stats_reset_t)(struct rte_ml_dev *dev);

/**
 * @internal
 *
 * Function used to get names of extended stats.
 *
 * @param dev
 *	ML device pointer.
 * @param mode
 *	Mode of stats to retrieve.
 * @param model_id
 *	Used to specify model id in model mode. Ignored in device mode.
 * @param xstats_map
 *	Array to insert id and names into.
 * @param size
 *	Size of xstats_map array.
 *
 * @return
 *	- >= 0 and <= size on success.
 *	- > size, error. Returns the size of xstats_map array required.
 *	- < 0, error code on failure.
 */
typedef int (*mldev_xstats_names_get_t)(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode,
					int32_t model_id, struct rte_ml_dev_xstats_map *xstats_map,
					uint32_t size);

/**
 * @internal
 *
 * Function used to get a single extended stat by name.
 *
 * @param dev
 *	ML device pointer.
 * @param name
 *	Name of the stat to retrieve.
 * @param stat_id
 *	ID of the stat to be returned.
 * @param value
 *	Value of the stat to be returned.
 *
 * @return
 *	- = 0 success.
 *	- < 0, error code on failure.
 */
typedef int (*mldev_xstats_by_name_get_t)(struct rte_ml_dev *dev, const char *name,
					  uint16_t *stat_id, uint64_t *value);

/**
 * @internal
 *
 * Function used to retrieve extended stats of a device.
 *
 * @param dev
 *	ML device pointer.
 * @param mode
 *	Mode of stats to retrieve.
 * @param model_id
 *	Used to specify model id in model mode. Ignored in device mode.
 * @param stat_ids
 *	Array of ID numbers of the stats to be retrieved.
 * @param values
 *	Values of the stats requested by the ID.
 * @param nb_ids
 *	Number of stats requested.
 *
 * @return
 *	- >= 0, number of entries filled into the values array.
 *	- < 0, error code on failure.
 */
typedef int (*mldev_xstats_get_t)(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode,
				  int32_t model_id, const uint16_t stat_ids[], uint64_t values[],
				  uint16_t nb_ids);

/**
 * @internal
 *
 * Function used to reset extended stats.
 *
 * @param dev
 *	ML device pointer.
 * @param mode
 *	Mode of stats to retrieve.
 * @param model_id
 *	Used to specify model id in model mode. Ignored in device mode.
 * @param stat_ids
 *	Array of stats IDs to be reset.
 * @param nb_ids
 *	Number of IDs in the stat_ids array.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error code on failure.
 */
typedef int (*mldev_xstats_reset_t)(struct rte_ml_dev *dev, enum rte_ml_dev_xstats_mode mode,
				    int32_t model_id, const uint16_t stat_ids[], uint16_t nb_ids);

/**
 * @internal
 *
 * Function used to dump ML device debug info.
 *
 * @param dev
 *	ML device pointer.
 * @param fd
 *	File descriptor to dump the debug info.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error code on failure.
 */

typedef int (*mldev_dump_t)(struct rte_ml_dev *dev, FILE *fd);

/**
 * @internal
 *
 * Function used for selftest of ML device.
 *
 * @param dev
 *	ML device pointer.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error on failure.
 */
typedef int (*mldev_selftest_t)(struct rte_ml_dev *dev);

/**
 * @internal
 *
 * Function used to load an ML model.
 *
 * @param dev
 *	ML device pointer.
 * @param params
 *	Model load params.
 * @param model_id
 *	Model ID returned by the library.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error on failure.
 */
typedef int (*mldev_model_load_t)(struct rte_ml_dev *dev, struct rte_ml_model_params *params,
				  uint16_t *model_id);

/**
 * @internal
 *
 * Function used to unload an ML model.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 *
 * @return
 *	- 0 on success.
 *	- < 0, error on failure.
 */
typedef int (*mldev_model_unload_t)(struct rte_ml_dev *dev, uint16_t model_id);

/**
 * @internal
 *
 * Function used to start an ML model.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_model_start_t)(struct rte_ml_dev *dev, uint16_t model_id);

/**
 * @internal
 *
 * Function used to stop an ML model.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_model_stop_t)(struct rte_ml_dev *dev, uint16_t model_id);

/**
 * @internal
 *
 * Get info about a model.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 * @param model_info
 *	Pointer to model info structure.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_model_info_get_t)(struct rte_ml_dev *dev, uint16_t model_id,
				      struct rte_ml_model_info *model_info);

/**
 * @internal
 *
 * Update model params.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 * @param buffer
 *	Pointer to model params.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_model_params_update_t)(struct rte_ml_dev *dev, uint16_t model_id, void *buffer);

/**
 * @internal
 *
 * Get size of input buffers.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 * @param nb_batches
 *	Number of batches.
 * @param input_qsize
 *	Size of quantized input.
 * @param input_dsize
 *	Size of dequantized input.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_io_input_size_get_t)(struct rte_ml_dev *dev, uint16_t model_id,
					 uint32_t nb_batches, uint64_t *input_qsize,
					 uint64_t *input_dsize);

/**
 * @internal
 *
 * Get size of output buffers.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 * @param nb_batches
 *	Number of batches.
 * @param output_qsize
 *	Size of quantized output.
 * @param output_dsize
 *	Size of dequantized output.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_io_output_size_get_t)(struct rte_ml_dev *dev, uint16_t model_id,
					  uint32_t nb_batches, uint64_t *output_qsize,
					  uint64_t *output_dsize);

/**
 * @internal
 *
 * Quantize model data.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 * @param nb_batches
 *	Number of batches.
 * @param dbuffer
 *	Pointer t de-quantized data buffer.
 * @param qbuffer
 *	Pointer t de-quantized data buffer.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_io_quantize_t)(struct rte_ml_dev *dev, uint16_t model_id, uint16_t nb_batches,
				   void *dbuffer, void *qbuffer);

/**
 * @internal
 *
 * Quantize model data.
 *
 * @param dev
 *	ML device pointer.
 * @param model_id
 *	Model ID to use.
 * @param nb_batches
 *	Number of batches.
 * @param qbuffer
 *	Pointer t de-quantized data buffer.
 * @param dbuffer
 *	Pointer t de-quantized data buffer.
 *
 * @return
 *	- 0 on success.
 *	- <0, error on failure.
 */
typedef int (*mldev_io_dequantize_t)(struct rte_ml_dev *dev, uint16_t model_id, uint16_t nb_batches,
				     void *qbuffer, void *dbuffer);

/**
 * @internal
 *
 * ML device operations function pointer table.
 */
struct rte_ml_dev_ops {
	/** Get device information. */
	mldev_info_get_t dev_info_get;

	/** Configure device. */
	mldev_configure_t dev_configure;

	/** Close device. */
	mldev_close_t dev_close;

	/** Start device. */
	mldev_start_t dev_start;

	/** Stop device. */
	mldev_stop_t dev_stop;

	/** Set up a device queue pair. */
	mldev_queue_pair_setup_t dev_queue_pair_setup;

	/** Release a device queue pair. */
	mldev_queue_pair_release_t dev_queue_pair_release;

	/** Get device statistics. */
	mldev_stats_get_t dev_stats_get;

	/** Reset device statistics. */
	mldev_stats_reset_t dev_stats_reset;

	/** Get names of extended stats. */
	mldev_xstats_names_get_t dev_xstats_names_get;

	/** Get value of a single extended stat. */
	mldev_xstats_by_name_get_t dev_xstats_by_name_get;

	/** Get extended stats of a device. */
	mldev_xstats_get_t dev_xstats_get;

	/** Reset extended stats of the device. */
	mldev_xstats_reset_t dev_xstats_reset;

	/** Dump ML device debug info. */
	mldev_dump_t dev_dump;

	/** Dump ML device debug info. */
	mldev_selftest_t dev_selftest;

	/** Load an ML model. */
	mldev_model_load_t model_load;

	/** Unload an ML model. */
	mldev_model_unload_t model_unload;

	/** Start an ML model. */
	mldev_model_start_t model_start;

	/** Stop an ML model. */
	mldev_model_stop_t model_stop;

	/** Get model information. */
	mldev_model_info_get_t model_info_get;

	/** Update model params. */
	mldev_model_params_update_t model_params_update;

	/** Get input buffer size. */
	mldev_io_input_size_get_t io_input_size_get;

	/** Get output buffer size. */
	mldev_io_output_size_get_t io_output_size_get;

	/** Quantize data */
	mldev_io_quantize_t io_quantize;

	/** De-quantize data */
	mldev_io_dequantize_t io_dequantize;
};

/**
 * @internal
 *
 * The data part, with no function pointers, associated with each device. This structure is safe to
 * place in shared memory to be common among different processes in a multi-process configuration.
 */
struct rte_ml_dev_data {
	/** Device ID for this instance. */
	int16_t dev_id;

	/** Socket ID where memory is allocated. */
	int16_t socket_id;

	/** Device state: STOPPED(0) / STARTED(1) */
	__extension__ uint8_t dev_started : 1;

	/** Number of device queue pairs. */
	uint16_t nb_queue_pairs;

	/** Number of ML models. */
	uint16_t nb_models;

	/** Array of pointers to queue pairs. */
	void **queue_pairs;

	/** Array of pointers to ML models. */
	void **models;

	/** PMD-specific private data. */
	void *dev_private;

	/** Unique identifier name. */
	char name[RTE_ML_STR_MAX];
};

/**
 * @internal
 *
 * The data structure associated with each ML device.
 */
struct rte_ml_dev {
	/** Pointer to PMD enqueue function. */
	mldev_enqueue_t enqueue_burst;

	/** Pointer to PMD dequeue function. */
	mldev_dequeue_t dequeue_burst;

	/** Pointer to PMD Op error get function. */
	mldev_op_error_get_t op_error_get;

	/** Pointer to device data. */
	struct rte_ml_dev_data *data;

	/** Functions exported by PMD. */
	struct rte_ml_dev_ops *dev_ops;

	/** Backing RTE device. */
	struct rte_device *device;

	/** Flag indicating the device is attached. */
	__extension__ uint8_t attached : 1;
} __rte_cache_aligned;

/**
 * @internal
 *
 * Global structure used for maintaining state of allocated ML devices.
 */
struct rte_ml_dev_global {
	/** Device information array. */
	struct rte_ml_dev *devs;

	/** Device private data array. */
	struct rte_ml_dev_data **data;

	/** Number of devices found. */
	uint8_t nb_devs;

	/** Maximum number of devices. */
	uint8_t max_devs;
};

#ifdef __cplusplus
}
#endif

#endif /* RTE_MLDEV_INTERNAL_H */
