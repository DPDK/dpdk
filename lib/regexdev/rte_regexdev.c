/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 * Copyright 2020 Mellanox Technologies, Ltd
 */

#include <string.h>

#include <rte_memzone.h>
#include <rte_string_fns.h>

#include "rte_regexdev.h"
#include "rte_regexdev_core.h"
#include "rte_regexdev_driver.h"

static const char *MZ_RTE_REGEXDEV_DATA = "rte_regexdev_data";
struct rte_regexdev rte_regex_devices[RTE_MAX_REGEXDEV_DEVS];
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
		if (rte_regex_devices[i].state == RTE_REGEXDEV_UNUSED)
			return i;
	}
	return RTE_MAX_REGEXDEV_DEVS;
}

static struct rte_regexdev*
regexdev_allocated(const char *name)
{
	uint16_t i;

	for (i = 0; i < RTE_MAX_REGEXDEV_DEVS; i++) {
		if (rte_regex_devices[i].state != RTE_REGEXDEV_UNUSED)
			if (!strcmp(name, rte_regex_devices[i].data->dev_name))
				return &rte_regex_devices[i];
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

	dev = &rte_regex_devices[dev_id];
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

uint8_t
rte_regexdev_count(void)
{
	int i;
	int count = 0;

	for (i = 0; i < RTE_MAX_REGEXDEV_DEVS; i++) {
		if (rte_regex_devices[i].state != RTE_REGEXDEV_UNUSED)
			count++;
	}
	return count;
}

int
rte_regexdev_get_dev_id(const char *name)
{
	int i;
	int id = -EINVAL;

	if (name == NULL)
		return -EINVAL;
	for (i = 0; i < RTE_MAX_REGEXDEV_DEVS; i++) {
		if (rte_regex_devices[i].state != RTE_REGEXDEV_UNUSED)
			if (strcmp(name, rte_regex_devices[i].data->dev_name)) {
				id = rte_regex_devices[i].data->dev_id;
				break;
			}
	}
	return id;
}

int
rte_regexdev_is_valid_dev(uint16_t dev_id)
{
	if (dev_id >= RTE_MAX_REGEXDEV_DEVS ||
	    rte_regex_devices[dev_id].state != RTE_REGEXDEV_READY)
		return 0;
	return 1;
}

static int
regexdev_info_get(uint8_t dev_id, struct rte_regexdev_info *dev_info)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	if (dev_info == NULL)
		return -EINVAL;
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_info_get, -ENOTSUP);
	return (*dev->dev_ops->dev_info_get)(dev, dev_info);

}

int
rte_regexdev_info_get(uint8_t dev_id, struct rte_regexdev_info *dev_info)
{
	return regexdev_info_get(dev_id, dev_info);
}

int
rte_regexdev_configure(uint8_t dev_id, const struct rte_regexdev_config *cfg)
{
	struct rte_regexdev *dev;
	struct rte_regexdev_info dev_info;
	int ret;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	if (cfg == NULL)
		return -EINVAL;
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_configure, -ENOTSUP);
	if (dev->data->dev_started) {
		RTE_REGEXDEV_LOG
			(ERR, "Dev %u must be stopped to allow configuration\n",
			 dev_id);
		return -EBUSY;
	}
	ret = regexdev_info_get(dev_id, &dev_info);
	if (ret < 0)
		return ret;
	if ((cfg->dev_cfg_flags & RTE_REGEXDEV_CFG_CROSS_BUFFER_SCAN_F) &&
	    !(dev_info.regexdev_capa & RTE_REGEXDEV_SUPP_CROSS_BUFFER_F)) {
		RTE_REGEXDEV_LOG(ERR,
				 "Dev %u doesn't support cross buffer scan\n",
				 dev_id);
		return -EINVAL;
	}
	if ((cfg->dev_cfg_flags & RTE_REGEXDEV_CFG_MATCH_AS_END_F) &&
	    !(dev_info.regexdev_capa & RTE_REGEXDEV_SUPP_MATCH_AS_END_F)) {
		RTE_REGEXDEV_LOG(ERR,
				 "Dev %u doesn't support match as end\n",
				 dev_id);
		return -EINVAL;
	}
	if ((cfg->dev_cfg_flags & RTE_REGEXDEV_CFG_MATCH_ALL_F) &&
	    !(dev_info.regexdev_capa & RTE_REGEXDEV_SUPP_MATCH_ALL_F)) {
		RTE_REGEXDEV_LOG(ERR,
				 "Dev %u doesn't support match all\n",
				 dev_id);
		return -EINVAL;
	}
	if (cfg->nb_groups == 0) {
		RTE_REGEXDEV_LOG(ERR, "Dev %u num of groups must be > 0\n",
				 dev_id);
		return -EINVAL;
	}
	if (cfg->nb_groups > dev_info.max_groups) {
		RTE_REGEXDEV_LOG(ERR, "Dev %u num of groups %d > %d\n",
				 dev_id, cfg->nb_groups, dev_info.max_groups);
		return -EINVAL;
	}
	if (cfg->nb_max_matches == 0) {
		RTE_REGEXDEV_LOG(ERR, "Dev %u num of matches must be > 0\n",
				 dev_id);
		return -EINVAL;
	}
	if (cfg->nb_max_matches > dev_info.max_matches) {
		RTE_REGEXDEV_LOG(ERR, "Dev %u num of matches %d > %d\n",
				 dev_id, cfg->nb_max_matches,
				 dev_info.max_matches);
		return -EINVAL;
	}
	if (cfg->nb_queue_pairs == 0) {
		RTE_REGEXDEV_LOG(ERR, "Dev %u num of queues must be > 0\n",
				 dev_id);
		return -EINVAL;
	}
	if (cfg->nb_queue_pairs > dev_info.max_queue_pairs) {
		RTE_REGEXDEV_LOG(ERR, "Dev %u num of queues %d > %d\n",
				 dev_id, cfg->nb_queue_pairs,
				 dev_info.max_queue_pairs);
		return -EINVAL;
	}
	if (cfg->nb_rules_per_group == 0) {
		RTE_REGEXDEV_LOG(ERR,
				 "Dev %u num of rules per group must be > 0\n",
				 dev_id);
		return -EINVAL;
	}
	if (cfg->nb_rules_per_group > dev_info.max_rules_per_group) {
		RTE_REGEXDEV_LOG(ERR,
				 "Dev %u num of rules per group %d > %d\n",
				 dev_id, cfg->nb_rules_per_group,
				 dev_info.max_rules_per_group);
		return -EINVAL;
	}
	ret = (*dev->dev_ops->dev_configure)(dev, cfg);
	if (ret == 0)
		dev->data->dev_conf = *cfg;
	return ret;
}

int
rte_regexdev_queue_pair_setup(uint8_t dev_id, uint16_t queue_pair_id,
			   const struct rte_regexdev_qp_conf *qp_conf)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_qp_setup, -ENOTSUP);
	if (dev->data->dev_started) {
		RTE_REGEXDEV_LOG
			(ERR, "Dev %u must be stopped to allow configuration\n",
			 dev_id);
		return -EBUSY;
	}
	if (queue_pair_id >= dev->data->dev_conf.nb_queue_pairs) {
		RTE_REGEXDEV_LOG(ERR,
				 "Dev %u invalid queue %d > %d\n",
				 dev_id, queue_pair_id,
				 dev->data->dev_conf.nb_queue_pairs);
		return -EINVAL;
	}
	if (dev->data->dev_started) {
		RTE_REGEXDEV_LOG
			(ERR, "Dev %u must be stopped to allow configuration\n",
			 dev_id);
		return -EBUSY;
	}
	return (*dev->dev_ops->dev_qp_setup)(dev, queue_pair_id, qp_conf);
}

int
rte_regexdev_start(uint8_t dev_id)
{
	struct rte_regexdev *dev;
	int ret;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_start, -ENOTSUP);
	ret = (*dev->dev_ops->dev_start)(dev);
	if (ret == 0)
		dev->data->dev_started = 1;
	return ret;
}

int
rte_regexdev_stop(uint8_t dev_id)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_stop, -ENOTSUP);
	(*dev->dev_ops->dev_stop)(dev);
	dev->data->dev_started = 0;
	return 0;
}

int
rte_regexdev_close(uint8_t dev_id)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_close, -ENOTSUP);
	(*dev->dev_ops->dev_close)(dev);
	dev->data->dev_started = 0;
	dev->state = RTE_REGEXDEV_UNUSED;
	return 0;
}

int
rte_regexdev_attr_get(uint8_t dev_id, enum rte_regexdev_attr_id attr_id,
		      void *attr_value)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_attr_get, -ENOTSUP);
	if (attr_value == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d attribute value can't be NULL\n",
				 dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_attr_get)(dev, attr_id, attr_value);
}

int
rte_regexdev_attr_set(uint8_t dev_id, enum rte_regexdev_attr_id attr_id,
		      const void *attr_value)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_attr_set, -ENOTSUP);
	if (attr_value == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d attribute value can't be NULL\n",
				 dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_attr_set)(dev, attr_id, attr_value);
}

int
rte_regexdev_rule_db_update(uint8_t dev_id,
			    const struct rte_regexdev_rule *rules,
			    uint32_t nb_rules)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_rule_db_update, -ENOTSUP);
	if (rules == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d rules can't be NULL\n",
				 dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_rule_db_update)(dev, rules, nb_rules);
}

int
rte_regexdev_rule_db_compile_activate(uint8_t dev_id)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_rule_db_compile_activate,
				-ENOTSUP);
	return (*dev->dev_ops->dev_rule_db_compile_activate)(dev);
}

int
rte_regexdev_rule_db_import(uint8_t dev_id, const char *rule_db,
			    uint32_t rule_db_len)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_db_import,
				-ENOTSUP);
	if (rule_db == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d rules can't be NULL\n",
				 dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_db_import)(dev, rule_db, rule_db_len);
}

int
rte_regexdev_rule_db_export(uint8_t dev_id, char *rule_db)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_db_export,
				-ENOTSUP);
	return (*dev->dev_ops->dev_db_export)(dev, rule_db);
}

int
rte_regexdev_xstats_names_get(uint8_t dev_id,
			      struct rte_regexdev_xstats_map *xstats_map)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_xstats_names_get,
				-ENOTSUP);
	if (xstats_map == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d xstats map can't be NULL\n",
				 dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_xstats_names_get)(dev, xstats_map);
}

int
rte_regexdev_xstats_get(uint8_t dev_id, const uint16_t *ids,
			uint64_t *values, uint16_t n)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_xstats_get, -ENOTSUP);
	if (ids == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d ids can't be NULL\n", dev_id);
		return -EINVAL;
	}
	if (values == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d values can't be NULL\n", dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_xstats_get)(dev, ids, values, n);
}

int
rte_regexdev_xstats_by_name_get(uint8_t dev_id, const char *name,
				uint16_t *id, uint64_t *value)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_xstats_by_name_get,
				-ENOTSUP);
	if (name == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d name can't be NULL\n", dev_id);
		return -EINVAL;
	}
	if (id == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d id can't be NULL\n", dev_id);
		return -EINVAL;
	}
	if (value == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d value can't be NULL\n", dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_xstats_by_name_get)(dev, name, id, value);
}

int
rte_regexdev_xstats_reset(uint8_t dev_id, const uint16_t *ids,
			  uint16_t nb_ids)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_xstats_reset, -ENOTSUP);
	if (ids == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d ids can't be NULL\n", dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_xstats_reset)(dev, ids, nb_ids);
}

int
rte_regexdev_selftest(uint8_t dev_id)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_selftest, -ENOTSUP);
	return (*dev->dev_ops->dev_selftest)(dev);
}

int
rte_regexdev_dump(uint8_t dev_id, FILE *f)
{
	struct rte_regexdev *dev;

	RTE_REGEXDEV_VALID_DEV_ID_OR_ERR_RET(dev_id, -EINVAL);
	dev = &rte_regex_devices[dev_id];
	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->dev_dump, -ENOTSUP);
	if (f == NULL) {
		RTE_REGEXDEV_LOG(ERR, "Dev %d file can't be NULL\n", dev_id);
		return -EINVAL;
	}
	return (*dev->dev_ops->dev_dump)(dev, f);
}
