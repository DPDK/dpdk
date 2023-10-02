/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <rte_errno.h>
#include <rte_log.h>
#include <rte_mldev.h>
#include <rte_mldev_pmd.h>

#include <stdlib.h>

static struct rte_ml_dev_global ml_dev_globals = {
	.devs = NULL, .data = NULL, .nb_devs = 0, .max_devs = RTE_MLDEV_DEFAULT_MAX};

/*
 * Private data structure of an operation pool.
 *
 * A structure that contains ml op_pool specific data that is
 * appended after the mempool structure (in private data).
 */
struct rte_ml_op_pool_private {
	uint16_t user_size;
	/*< Size of private user data with each operation. */
};

struct rte_ml_dev *
rte_ml_dev_pmd_get_dev(int16_t dev_id)
{
	return &ml_dev_globals.devs[dev_id];
}

struct rte_ml_dev *
rte_ml_dev_pmd_get_named_dev(const char *name)
{
	struct rte_ml_dev *dev;
	int16_t dev_id;

	if (name == NULL)
		return NULL;

	for (dev_id = 0; dev_id < ml_dev_globals.max_devs; dev_id++) {
		dev = rte_ml_dev_pmd_get_dev(dev_id);
		if ((dev->attached == ML_DEV_ATTACHED) && (strcmp(dev->data->name, name) == 0))
			return dev;
	}

	return NULL;
}

struct rte_ml_dev *
rte_ml_dev_pmd_allocate(const char *name, uint8_t socket_id)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	struct rte_ml_dev *dev;
	int16_t dev_id;

	/* implicit initialization of library before adding first device */
	if (ml_dev_globals.devs == NULL) {
		if (rte_ml_dev_init(RTE_MLDEV_DEFAULT_MAX) != 0)
			return NULL;
	}

	if (rte_ml_dev_pmd_get_named_dev(name) != NULL) {
		RTE_MLDEV_LOG(ERR, "ML device with name %s already allocated!", name);
		return NULL;
	}

	/* Get a free device ID */
	for (dev_id = 0; dev_id < ml_dev_globals.max_devs; dev_id++) {
		dev = rte_ml_dev_pmd_get_dev(dev_id);
		if (dev->attached == ML_DEV_DETACHED)
			break;
	}

	if (dev_id == ml_dev_globals.max_devs) {
		RTE_MLDEV_LOG(ERR, "Reached maximum number of ML devices");
		return NULL;
	}

	if (dev->data == NULL) {
		/* Reserve memzone name */
		sprintf(mz_name, "rte_ml_dev_data_%d", dev_id);
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			mz = rte_memzone_reserve(mz_name, sizeof(struct rte_ml_dev_data), socket_id,
						 0);
			RTE_MLDEV_LOG(DEBUG, "PRIMARY: reserved memzone for %s (%p)", mz_name, mz);
		} else {
			mz = rte_memzone_lookup(mz_name);
			RTE_MLDEV_LOG(DEBUG, "SECONDARY: looked up memzone for %s (%p)", mz_name,
				      mz);
		}

		if (mz == NULL)
			return NULL;

		ml_dev_globals.data[dev_id] = mz->addr;
		if (rte_eal_process_type() == RTE_PROC_PRIMARY)
			memset(ml_dev_globals.data[dev_id], 0, sizeof(struct rte_ml_dev_data));

		dev->data = ml_dev_globals.data[dev_id];
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			strlcpy(dev->data->name, name, RTE_ML_STR_MAX);
			dev->data->dev_id = dev_id;
			dev->data->socket_id = socket_id;
			dev->data->dev_started = 0;
			RTE_MLDEV_LOG(DEBUG, "PRIMARY: init mldev data");
		}

		RTE_MLDEV_LOG(DEBUG, "Data for %s: dev_id %d, socket %u", dev->data->name,
			      dev->data->dev_id, dev->data->socket_id);

		dev->attached = ML_DEV_ATTACHED;
		ml_dev_globals.nb_devs++;
	}

	dev->enqueue_burst = NULL;
	dev->dequeue_burst = NULL;

	return dev;
}

int
rte_ml_dev_pmd_release(struct rte_ml_dev *dev)
{
	char mz_name[RTE_MEMZONE_NAMESIZE];
	const struct rte_memzone *mz;
	int16_t dev_id;
	int ret = 0;

	if (dev == NULL)
		return -EINVAL;

	dev_id = dev->data->dev_id;

	/* Memzone lookup */
	sprintf(mz_name, "rte_ml_dev_data_%d", dev_id);
	mz = rte_memzone_lookup(mz_name);
	if (mz == NULL)
		return -ENOMEM;

	RTE_ASSERT(ml_dev_globals.data[dev_id] == mz->addr);
	ml_dev_globals.data[dev_id] = NULL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		RTE_MLDEV_LOG(DEBUG, "PRIMARY: free memzone of %s (%p)", mz_name, mz);
		ret = rte_memzone_free(mz);
	} else {
		RTE_MLDEV_LOG(DEBUG, "SECONDARY: don't free memzone of %s (%p)", mz_name, mz);
	}

	dev->attached = ML_DEV_DETACHED;
	ml_dev_globals.nb_devs--;

	return ret;
}

int
rte_ml_dev_init(size_t dev_max)
{
	if (dev_max == 0 || dev_max > INT16_MAX) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_max = %zu (> %d)\n", dev_max, INT16_MAX);
		rte_errno = EINVAL;
		return -rte_errno;
	}

	/* No lock, it must be called before or during first probing. */
	if (ml_dev_globals.devs != NULL) {
		RTE_MLDEV_LOG(ERR, "Device array already initialized");
		rte_errno = EBUSY;
		return -rte_errno;
	}

	ml_dev_globals.devs = calloc(dev_max, sizeof(struct rte_ml_dev));
	if (ml_dev_globals.devs == NULL) {
		RTE_MLDEV_LOG(ERR, "Cannot initialize MLDEV library");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	ml_dev_globals.data = calloc(dev_max, sizeof(struct rte_ml_dev_data *));
	if (ml_dev_globals.data == NULL) {
		RTE_MLDEV_LOG(ERR, "Cannot initialize MLDEV library");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	ml_dev_globals.max_devs = dev_max;

	return 0;
}

uint16_t
rte_ml_dev_count(void)
{
	return ml_dev_globals.nb_devs;
}

int
rte_ml_dev_is_valid_dev(int16_t dev_id)
{
	struct rte_ml_dev *dev = NULL;

	if (dev_id >= ml_dev_globals.max_devs || ml_dev_globals.devs[dev_id].data == NULL)
		return 0;

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (dev->attached != ML_DEV_ATTACHED)
		return 0;
	else
		return 1;
}

int
rte_ml_dev_socket_id(int16_t dev_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);

	return dev->data->socket_id;
}

int
rte_ml_dev_info_get(int16_t dev_id, struct rte_ml_dev_info *dev_info)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_info_get == NULL)
		return -ENOTSUP;

	if (dev_info == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, dev_info cannot be NULL\n", dev_id);
		return -EINVAL;
	}
	memset(dev_info, 0, sizeof(struct rte_ml_dev_info));

	return (*dev->dev_ops->dev_info_get)(dev, dev_info);
}

int
rte_ml_dev_configure(int16_t dev_id, const struct rte_ml_dev_config *config)
{
	struct rte_ml_dev_info dev_info;
	struct rte_ml_dev *dev;
	int ret;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_configure == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started) {
		RTE_MLDEV_LOG(ERR, "Device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	if (config == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, config cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	ret = rte_ml_dev_info_get(dev_id, &dev_info);
	if (ret < 0)
		return ret;

	if (config->nb_queue_pairs > dev_info.max_queue_pairs) {
		RTE_MLDEV_LOG(ERR, "Device %d num of queues %u > %u\n", dev_id,
			      config->nb_queue_pairs, dev_info.max_queue_pairs);
		return -EINVAL;
	}

	return (*dev->dev_ops->dev_configure)(dev, config);
}

int
rte_ml_dev_close(int16_t dev_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_close == NULL)
		return -ENOTSUP;

	/* Device must be stopped before it can be closed */
	if (dev->data->dev_started == 1) {
		RTE_MLDEV_LOG(ERR, "Device %d must be stopped before closing", dev_id);
		return -EBUSY;
	}

	return (*dev->dev_ops->dev_close)(dev);
}

int
rte_ml_dev_start(int16_t dev_id)
{
	struct rte_ml_dev *dev;
	int ret;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_start == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started != 0) {
		RTE_MLDEV_LOG(ERR, "Device %d is already started", dev_id);
		return -EBUSY;
	}

	ret = (*dev->dev_ops->dev_start)(dev);
	if (ret == 0)
		dev->data->dev_started = 1;

	return ret;
}

int
rte_ml_dev_stop(int16_t dev_id)
{
	struct rte_ml_dev *dev;
	int ret;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_stop == NULL)
		return -ENOTSUP;

	if (dev->data->dev_started == 0) {
		RTE_MLDEV_LOG(ERR, "Device %d is not started", dev_id);
		return -EBUSY;
	}

	ret = (*dev->dev_ops->dev_stop)(dev);
	if (ret == 0)
		dev->data->dev_started = 0;

	return ret;
}

int
rte_ml_dev_queue_pair_setup(int16_t dev_id, uint16_t queue_pair_id,
			    const struct rte_ml_dev_qp_conf *qp_conf, int socket_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_queue_pair_setup == NULL)
		return -ENOTSUP;

	if (queue_pair_id >= dev->data->nb_queue_pairs) {
		RTE_MLDEV_LOG(ERR, "Invalid queue_pair_id = %d", queue_pair_id);
		return -EINVAL;
	}

	if (qp_conf == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, qp_conf cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		RTE_MLDEV_LOG(ERR, "Device %d must be stopped to allow configuration", dev_id);
		return -EBUSY;
	}

	return (*dev->dev_ops->dev_queue_pair_setup)(dev, queue_pair_id, qp_conf, socket_id);
}

int
rte_ml_dev_stats_get(int16_t dev_id, struct rte_ml_dev_stats *stats)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_stats_get == NULL)
		return -ENOTSUP;

	if (stats == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, stats cannot be NULL\n", dev_id);
		return -EINVAL;
	}
	memset(stats, 0, sizeof(struct rte_ml_dev_stats));

	return (*dev->dev_ops->dev_stats_get)(dev, stats);
}

void
rte_ml_dev_stats_reset(int16_t dev_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_stats_reset == NULL)
		return;

	(*dev->dev_ops->dev_stats_reset)(dev);
}

int
rte_ml_dev_xstats_names_get(int16_t dev_id, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
			    struct rte_ml_dev_xstats_map *xstats_map, uint32_t size)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_xstats_names_get == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->dev_xstats_names_get)(dev, mode, model_id, xstats_map, size);
}

int
rte_ml_dev_xstats_by_name_get(int16_t dev_id, const char *name, uint16_t *stat_id, uint64_t *value)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_xstats_by_name_get == NULL)
		return -ENOTSUP;

	if (name == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, name cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	if (value == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, value cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->dev_xstats_by_name_get)(dev, name, stat_id, value);
}

int
rte_ml_dev_xstats_get(int16_t dev_id, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
		      const uint16_t stat_ids[], uint64_t values[], uint16_t nb_ids)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_xstats_get == NULL)
		return -ENOTSUP;

	if (stat_ids == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, stat_ids cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	if (values == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, values cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->dev_xstats_get)(dev, mode, model_id, stat_ids, values, nb_ids);
}

int
rte_ml_dev_xstats_reset(int16_t dev_id, enum rte_ml_dev_xstats_mode mode, int32_t model_id,
			const uint16_t stat_ids[], uint16_t nb_ids)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_xstats_reset == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->dev_xstats_reset)(dev, mode, model_id, stat_ids, nb_ids);
}

int
rte_ml_dev_dump(int16_t dev_id, FILE *fd)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_dump == NULL)
		return -ENOTSUP;

	if (fd == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, file descriptor cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->dev_dump)(dev, fd);
}

int
rte_ml_dev_selftest(int16_t dev_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->dev_selftest == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->dev_selftest)(dev);
}

int
rte_ml_model_load(int16_t dev_id, struct rte_ml_model_params *params, uint16_t *model_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->model_load == NULL)
		return -ENOTSUP;

	if (params == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, params cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	if (model_id == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, model_id cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->model_load)(dev, params, model_id);
}

int
rte_ml_model_unload(int16_t dev_id, uint16_t model_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->model_unload == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->model_unload)(dev, model_id);
}

int
rte_ml_model_start(int16_t dev_id, uint16_t model_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->model_start == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->model_start)(dev, model_id);
}

int
rte_ml_model_stop(int16_t dev_id, uint16_t model_id)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->model_stop == NULL)
		return -ENOTSUP;

	return (*dev->dev_ops->model_stop)(dev, model_id);
}

int
rte_ml_model_info_get(int16_t dev_id, uint16_t model_id, struct rte_ml_model_info *model_info)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->model_info_get == NULL)
		return -ENOTSUP;

	if (model_info == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, model_id %u, model_info cannot be NULL\n", dev_id,
			      model_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->model_info_get)(dev, model_id, model_info);
}

int
rte_ml_model_params_update(int16_t dev_id, uint16_t model_id, void *buffer)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->model_params_update == NULL)
		return -ENOTSUP;

	if (buffer == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, buffer cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->model_params_update)(dev, model_id, buffer);
}

int
rte_ml_io_quantize(int16_t dev_id, uint16_t model_id, struct rte_ml_buff_seg **dbuffer,
		   struct rte_ml_buff_seg **qbuffer)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->io_quantize == NULL)
		return -ENOTSUP;

	if (dbuffer == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, dbuffer cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	if (qbuffer == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, qbuffer cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->io_quantize)(dev, model_id, dbuffer, qbuffer);
}

int
rte_ml_io_dequantize(int16_t dev_id, uint16_t model_id, struct rte_ml_buff_seg **qbuffer,
		     struct rte_ml_buff_seg **dbuffer)
{
	struct rte_ml_dev *dev;

	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dev_ops->io_dequantize == NULL)
		return -ENOTSUP;

	if (qbuffer == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, qbuffer cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	if (dbuffer == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, dbuffer cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	return (*dev->dev_ops->io_dequantize)(dev, model_id, qbuffer, dbuffer);
}

/** Initialise rte_ml_op mempool element */
static void
ml_op_init(struct rte_mempool *mempool, __rte_unused void *opaque_arg, void *_op_data,
	   __rte_unused unsigned int i)
{
	struct rte_ml_op *op = _op_data;

	memset(_op_data, 0, mempool->elt_size);
	op->status = RTE_ML_OP_STATUS_NOT_PROCESSED;
	op->mempool = mempool;
}

struct rte_mempool *
rte_ml_op_pool_create(const char *name, unsigned int nb_elts, unsigned int cache_size,
		      uint16_t user_size, int socket_id)
{
	struct rte_ml_op_pool_private *priv;
	struct rte_mempool *mp;
	unsigned int elt_size;

	/* lookup mempool in case already allocated */
	mp = rte_mempool_lookup(name);
	elt_size = sizeof(struct rte_ml_op) + user_size;

	if (mp != NULL) {
		priv = (struct rte_ml_op_pool_private *)rte_mempool_get_priv(mp);
		if (mp->elt_size != elt_size || mp->cache_size < cache_size || mp->size < nb_elts ||
		    priv->user_size < user_size) {
			mp = NULL;
			RTE_MLDEV_LOG(ERR,
				      "Mempool %s already exists but with incompatible parameters",
				      name);
			return NULL;
		}
		return mp;
	}

	mp = rte_mempool_create(name, nb_elts, elt_size, cache_size,
				sizeof(struct rte_ml_op_pool_private), NULL, NULL, ml_op_init, NULL,
				socket_id, 0);
	if (mp == NULL) {
		RTE_MLDEV_LOG(ERR, "Failed to create mempool %s", name);
		return NULL;
	}

	priv = (struct rte_ml_op_pool_private *)rte_mempool_get_priv(mp);
	priv->user_size = user_size;

	return mp;
}

void
rte_ml_op_pool_free(struct rte_mempool *mempool)
{
	rte_mempool_free(mempool);
}

uint16_t
rte_ml_enqueue_burst(int16_t dev_id, uint16_t qp_id, struct rte_ml_op **ops, uint16_t nb_ops)
{
	struct rte_ml_dev *dev;

#ifdef RTE_LIBRTE_ML_DEV_DEBUG
	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		rte_errno = -EINVAL;
		return 0;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->enqueue_burst == NULL) {
		rte_errno = -ENOTSUP;
		return 0;
	}

	if (ops == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, ops cannot be NULL\n", dev_id);
		rte_errno = -EINVAL;
		return 0;
	}

	if (qp_id >= dev->data->nb_queue_pairs) {
		RTE_MLDEV_LOG(ERR, "Invalid qp_id %u\n", qp_id);
		rte_errno = -EINVAL;
		return 0;
	}
#else
	dev = rte_ml_dev_pmd_get_dev(dev_id);
#endif

	return (*dev->enqueue_burst)(dev, qp_id, ops, nb_ops);
}

uint16_t
rte_ml_dequeue_burst(int16_t dev_id, uint16_t qp_id, struct rte_ml_op **ops, uint16_t nb_ops)
{
	struct rte_ml_dev *dev;

#ifdef RTE_LIBRTE_ML_DEV_DEBUG
	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		rte_errno = -EINVAL;
		return 0;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->dequeue_burst == NULL) {
		rte_errno = -ENOTSUP;
		return 0;
	}

	if (ops == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, ops cannot be NULL\n", dev_id);
		rte_errno = -EINVAL;
		return 0;
	}

	if (qp_id >= dev->data->nb_queue_pairs) {
		RTE_MLDEV_LOG(ERR, "Invalid qp_id %u\n", qp_id);
		rte_errno = -EINVAL;
		return 0;
	}
#else
	dev = rte_ml_dev_pmd_get_dev(dev_id);
#endif

	return (*dev->dequeue_burst)(dev, qp_id, ops, nb_ops);
}

int
rte_ml_op_error_get(int16_t dev_id, struct rte_ml_op *op, struct rte_ml_op_error *error)
{
	struct rte_ml_dev *dev;

#ifdef RTE_LIBRTE_ML_DEV_DEBUG
	if (!rte_ml_dev_is_valid_dev(dev_id)) {
		RTE_MLDEV_LOG(ERR, "Invalid dev_id = %d\n", dev_id);
		return -EINVAL;
	}

	dev = rte_ml_dev_pmd_get_dev(dev_id);
	if (*dev->op_error_get == NULL)
		return -ENOTSUP;

	if (op == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, op cannot be NULL\n", dev_id);
		return -EINVAL;
	}

	if (error == NULL) {
		RTE_MLDEV_LOG(ERR, "Dev %d, error cannot be NULL\n", dev_id);
		return -EINVAL;
	}
#else
	dev = rte_ml_dev_pmd_get_dev(dev_id);
#endif

	return (*dev->op_error_get)(dev, op, error);
}

RTE_LOG_REGISTER_DEFAULT(rte_ml_dev_logtype, INFO);
