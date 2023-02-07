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
	ml_dev_globals.devs = ml_dev_globals.devs;

	return 0;
}

RTE_LOG_REGISTER_DEFAULT(rte_ml_dev_logtype, INFO);
