/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <string.h>

#include <rte_memory.h>
#include <rte_memcpy.h>
#include <rte_memzone.h>
#include <rte_string_fns.h>

#include "rte_regexdev.h"
#include "rte_regexdev_core.h"
#include "rte_regexdev_driver.h"

static const char *MZ_RTE_REGEXDEV_DATA = "rte_regexdev_data";
static struct rte_regexdev regex_devices[RTE_MAX_REGEXDEV_DEVS];
/* Shared memory between primary and secondary processes. */
static struct {
	struct rte_regexdev_data data[RTE_MAX_REGEXDEV_DEVS];
} *rte_regexdev_shared_data;

int rte_regexdev_logtype;

static uint16_t
regexdev_find_free_dev(void)
{
	uint16_t i;

	for (i = 0; i < RTE_MAX_REGEXDEV_DEVS; i++) {
		if (regex_devices[i].state == RTE_REGEXDEV_UNUSED)
			return i;
	}
	return RTE_MAX_REGEXDEV_DEVS;
}

static struct rte_regexdev*
regexdev_allocated(const char *name)
{
	uint16_t i;

	for (i = 0; i < RTE_MAX_REGEXDEV_DEVS; i++) {
		if (regex_devices[i].state != RTE_REGEXDEV_UNUSED)
			if (!strcmp(name, regex_devices[i].data->dev_name))
				return &regex_devices[i];
	}
	return NULL;
}

static int
regexdev_shared_data_prepare(void)
{
	const unsigned int flags = 0;
	const struct rte_memzone *mz;

	if (rte_regexdev_shared_data == NULL) {
		/* Allocate port data and ownership shared memory. */
		mz = rte_memzone_reserve(MZ_RTE_REGEXDEV_DATA,
					 sizeof(*rte_regexdev_shared_data),
					 rte_socket_id(), flags);
		if (mz == NULL)
			return -ENOMEM;

		rte_regexdev_shared_data = mz->addr;
		memset(rte_regexdev_shared_data->data, 0,
		       sizeof(rte_regexdev_shared_data->data));
	}
	return 0;
}

static int
regexdev_check_name(const char *name)
{
	size_t name_len;

	if (name == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Name can't be NULL\n");
		return -EINVAL;
	}
	name_len = strnlen(name, RTE_REGEXDEV_NAME_MAX_LEN);
	if (name_len == 0) {
		RTE_REGEXDEV_LOG(ERR, "Zero length RegEx device name\n");
		return -EINVAL;
	}
	if (name_len >= RTE_REGEXDEV_NAME_MAX_LEN) {
		RTE_REGEXDEV_LOG(ERR, "RegEx device name is too long\n");
		return -EINVAL;
	}
	return (int)name_len;

}

struct rte_regexdev *
rte_regexdev_register(const char *name)
{
	uint16_t dev_id;
	int name_len;
	struct rte_regexdev *dev;

	name_len = regexdev_check_name(name);
	if (name_len < 0)
		return NULL;
	dev = regexdev_allocated(name);
	if (dev != NULL) {
		RTE_REGEXDEV_LOG(ERR, "RegEx device already allocated\n");
		return NULL;
	}
	dev_id = regexdev_find_free_dev();
	if (dev_id == RTE_MAX_REGEXDEV_DEVS) {
		RTE_REGEXDEV_LOG
			(ERR, "Reached maximum number of RegEx devices\n");
		return NULL;
	}
	if (regexdev_shared_data_prepare() < 0) {
		RTE_REGEXDEV_LOG(ERR, "Cannot allocate RegEx shared data\n");
		return NULL;
	}

	dev = &regex_devices[dev_id];
	dev->state = RTE_REGEXDEV_REGISTERED;
	if (dev->data == NULL)
		dev->data = &rte_regexdev_shared_data->data[dev_id];
	else
		memset(dev->data, 1, sizeof(*dev->data));
	dev->data->dev_id = dev_id;
	strlcpy(dev->data->dev_name, name, sizeof(dev->data->dev_name));
	return dev;
}

void
rte_regexdev_unregister(struct rte_regexdev *dev)
{
	dev->state = RTE_REGEXDEV_UNUSED;
}

struct rte_regexdev *
rte_regexdev_get_device_by_name(const char *name)
{
	if (regexdev_check_name(name) < 0)
		return NULL;
	return regexdev_allocated(name);
}
