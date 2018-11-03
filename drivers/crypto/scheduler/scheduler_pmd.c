/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */
#include <rte_common.h>
#include <rte_hexdump.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_cpuflags.h>
#include <rte_reorder.h>
#include <rte_string_fns.h>

#include "rte_cryptodev_scheduler.h"
#include "scheduler_pmd_private.h"

uint8_t cryptodev_scheduler_driver_id;

struct scheduler_init_params {
	struct rte_cryptodev_pmd_init_params def_p;
	uint32_t nb_slaves;
	enum rte_cryptodev_scheduler_mode mode;
	char mode_param_str[RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN];
	uint32_t enable_ordering;
	uint16_t wc_pool[RTE_MAX_LCORE];
	uint16_t nb_wc;
	char slave_names[RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES]
			[RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN];
};

#define RTE_CRYPTODEV_VDEV_NAME			("name")
#define RTE_CRYPTODEV_VDEV_SLAVE		("slave")
#define RTE_CRYPTODEV_VDEV_MODE			("mode")
#define RTE_CRYPTODEV_VDEV_MODE_PARAM		("mode_param")
#define RTE_CRYPTODEV_VDEV_ORDERING		("ordering")
#define RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG	("max_nb_queue_pairs")
#define RTE_CRYPTODEV_VDEV_SOCKET_ID		("socket_id")
#define RTE_CRYPTODEV_VDEV_COREMASK		("coremask")
#define RTE_CRYPTODEV_VDEV_CORELIST		("corelist")

static const char * const scheduler_valid_params[] = {
	RTE_CRYPTODEV_VDEV_NAME,
	RTE_CRYPTODEV_VDEV_SLAVE,
	RTE_CRYPTODEV_VDEV_MODE,
	RTE_CRYPTODEV_VDEV_MODE_PARAM,
	RTE_CRYPTODEV_VDEV_ORDERING,
	RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
	RTE_CRYPTODEV_VDEV_SOCKET_ID,
	RTE_CRYPTODEV_VDEV_COREMASK,
	RTE_CRYPTODEV_VDEV_CORELIST
};

struct scheduler_parse_map {
	const char *name;
	uint32_t val;
};

const struct scheduler_parse_map scheduler_mode_map[] = {
	{RTE_STR(SCHEDULER_MODE_NAME_ROUND_ROBIN),
			CDEV_SCHED_MODE_ROUNDROBIN},
	{RTE_STR(SCHEDULER_MODE_NAME_PKT_SIZE_DISTR),
			CDEV_SCHED_MODE_PKT_SIZE_DISTR},
	{RTE_STR(SCHEDULER_MODE_NAME_FAIL_OVER),
			CDEV_SCHED_MODE_FAILOVER},
	{RTE_STR(SCHEDULER_MODE_NAME_MULTI_CORE),
			CDEV_SCHED_MODE_MULTICORE}
};

const struct scheduler_parse_map scheduler_ordering_map[] = {
		{"enable", 1},
		{"disable", 0}
};

#define CDEV_SCHED_MODE_PARAM_SEP_CHAR		':'

static int
cryptodev_scheduler_create(const char *name,
		struct rte_vdev_device *vdev,
		struct scheduler_init_params *init_params)
{
	struct rte_cryptodev *dev;
	struct scheduler_ctx *sched_ctx;
	uint32_t i;
	int ret;

	dev = rte_cryptodev_pmd_create(name, &vdev->device,
			&init_params->def_p);
	if (dev == NULL) {
		CR_SCHED_LOG(ERR, "driver %s: failed to create cryptodev vdev",
			name);
		return -EFAULT;
	}

	dev->driver_id = cryptodev_scheduler_driver_id;
	dev->dev_ops = rte_crypto_scheduler_pmd_ops;

	sched_ctx = dev->data->dev_private;
	sched_ctx->max_nb_queue_pairs =
			init_params->def_p.max_nb_queue_pairs;

	if (init_params->mode == CDEV_SCHED_MODE_MULTICORE) {
		uint16_t i;

		sched_ctx->nb_wc = init_params->nb_wc;

		for (i = 0; i < sched_ctx->nb_wc; i++) {
			sched_ctx->wc_pool[i] = init_params->wc_pool[i];
			CR_SCHED_LOG(INFO, "  Worker core[%u]=%u added",
				i, sched_ctx->wc_pool[i]);
		}
	}

	if (init_params->mode > CDEV_SCHED_MODE_USERDEFINED &&
			init_params->mode < CDEV_SCHED_MODE_COUNT) {
		union {
			struct rte_cryptodev_scheduler_threshold_option
					threshold_option;
		} option;
		enum rte_cryptodev_schedule_option_type option_type;
		char param_name[RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN] = {0};
		char param_val[RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN] = {0};
		char *s, *end;

		ret = rte_cryptodev_scheduler_mode_set(dev->data->dev_id,
			init_params->mode);
		if (ret < 0) {
			rte_cryptodev_pmd_release_device(dev);
			return ret;
		}

		for (i = 0; i < RTE_DIM(scheduler_mode_map); i++) {
			if (scheduler_mode_map[i].val != sched_ctx->mode)
				continue;

			CR_SCHED_LOG(INFO, "  Scheduling mode = %s",
					scheduler_mode_map[i].name);
			break;
		}

		if (strlen(init_params->mode_param_str) > 0) {
			s = strchr(init_params->mode_param_str,
					CDEV_SCHED_MODE_PARAM_SEP_CHAR);
			if (s == NULL) {
				CR_SCHED_LOG(ERR, "Invalid mode param");
				return -EINVAL;
			}

			strlcpy(param_name, init_params->mode_param_str,
					s - init_params->mode_param_str + 1);
			s++;
			strlcpy(param_val, s,
					RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN);

			switch (init_params->mode) {
			case CDEV_SCHED_MODE_PKT_SIZE_DISTR:
				if (strcmp(param_name,
					RTE_CRYPTODEV_SCHEDULER_PARAM_THRES)
						!= 0) {
					CR_SCHED_LOG(ERR, "Invalid mode param");
					return -EINVAL;
				}
				option_type = CDEV_SCHED_OPTION_THRESHOLD;

				option.threshold_option.threshold =
						strtoul(param_val, &end, 0);
				break;
			default:
				CR_SCHED_LOG(ERR, "Invalid mode param");
				return -EINVAL;
			}

			if (sched_ctx->ops.option_set(dev, option_type,
					(void *)&option) < 0) {
				CR_SCHED_LOG(ERR, "Invalid mode param");
				return -EINVAL;
			}

			RTE_LOG(INFO, PMD, "  Sched mode param (%s = %s)\n",
					param_name, param_val);
		}
	}

	sched_ctx->reordering_enabled = init_params->enable_ordering;

	for (i = 0; i < RTE_DIM(scheduler_ordering_map); i++) {
		if (scheduler_ordering_map[i].val !=
				sched_ctx->reordering_enabled)
			continue;

		CR_SCHED_LOG(INFO, "  Packet ordering = %s",
				scheduler_ordering_map[i].name);

		break;
	}

	for (i = 0; i < init_params->nb_slaves; i++) {
		sched_ctx->init_slave_names[sched_ctx->nb_init_slaves] =
			rte_zmalloc_socket(
				NULL,
				RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN, 0,
				SOCKET_ID_ANY);

		if (!sched_ctx->init_slave_names[
				sched_ctx->nb_init_slaves]) {
			CR_SCHED_LOG(ERR, "driver %s: Insufficient memory",
					name);
			return -ENOMEM;
		}

		strncpy(sched_ctx->init_slave_names[
					sched_ctx->nb_init_slaves],
				init_params->slave_names[i],
				RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN - 1);

		sched_ctx->nb_init_slaves++;
	}

	/*
	 * Initialize capabilities structure as an empty structure,
	 * in case device information is requested when no slaves are attached
	 */
	sched_ctx->capabilities = rte_zmalloc_socket(NULL,
			sizeof(struct rte_cryptodev_capabilities),
			0, SOCKET_ID_ANY);

	if (!sched_ctx->capabilities) {
		CR_SCHED_LOG(ERR, "Not enough memory for capability "
				"information");
		return -ENOMEM;
	}

	return 0;
}

static int
cryptodev_scheduler_remove(struct rte_vdev_device *vdev)
{
	const char *name;
	struct rte_cryptodev *dev;
	struct scheduler_ctx *sched_ctx;

	if (vdev == NULL)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	dev = rte_cryptodev_pmd_get_named_dev(name);
	if (dev == NULL)
		return -EINVAL;

	sched_ctx = dev->data->dev_private;

	if (sched_ctx->nb_slaves) {
		uint32_t i;

		for (i = 0; i < sched_ctx->nb_slaves; i++)
			rte_cryptodev_scheduler_slave_detach(dev->data->dev_id,
					sched_ctx->slaves[i].dev_id);
	}

	return rte_cryptodev_pmd_destroy(dev);
}

/** Parse integer from integer argument */
static int
parse_integer_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int *i = (int *) extra_args;

	*i = atoi(value);
	if (*i < 0) {
		CR_SCHED_LOG(ERR, "Argument has to be positive.");
		return -EINVAL;
	}

	return 0;
}

/** Parse integer from hexadecimal integer argument */
static int
parse_coremask_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	int i, j, val;
	uint16_t idx = 0;
	char c;
	struct scheduler_init_params *params = extra_args;

	params->nb_wc = 0;

	if (value == NULL)
		return -1;
	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*value))
		value++;
	if (value[0] == '0' && ((value[1] == 'x') || (value[1] == 'X')))
		value += 2;
	i = strlen(value);
	while ((i > 0) && isblank(value[i - 1]))
		i--;

	if (i == 0)
		return -1;

	for (i = i - 1; i >= 0 && idx < RTE_MAX_LCORE; i--) {
		c = value[i];
		if (isxdigit(c) == 0) {
			/* invalid characters */
			return -1;
		}
		if (isdigit(c))
			val = c - '0';
		else if (isupper(c))
			val = c - 'A' + 10;
		else
			val = c - 'a' + 10;

		for (j = 0; j < 4 && idx < RTE_MAX_LCORE; j++, idx++) {
			if ((1 << j) & val)
				params->wc_pool[params->nb_wc++] = idx;
		}
	}

	return 0;
}

/** Parse integer from list of integers argument */
static int
parse_corelist_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct scheduler_init_params *params = extra_args;

	params->nb_wc = 0;

	const char *token = value;

	while (isdigit(token[0])) {
		char *rval;
		unsigned int core = strtoul(token, &rval, 10);

		if (core >= RTE_MAX_LCORE) {
			CR_SCHED_LOG(ERR, "Invalid worker core %u, should be smaller "
				   "than %u.", core, RTE_MAX_LCORE);
		}
		params->wc_pool[params->nb_wc++] = (uint16_t)core;
		token = (const char *)rval;
		if (token[0] == '\0')
			break;
		token++;
	}

	return 0;
}

/** Parse name */
static int
parse_name_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct rte_cryptodev_pmd_init_params *params = extra_args;

	if (strlen(value) >= RTE_CRYPTODEV_NAME_MAX_LEN - 1) {
		CR_SCHED_LOG(ERR, "Invalid name %s, should be less than "
				"%u bytes.", value,
				RTE_CRYPTODEV_NAME_MAX_LEN - 1);
		return -EINVAL;
	}

	strlcpy(params->name, value, RTE_CRYPTODEV_NAME_MAX_LEN);

	return 0;
}

/** Parse slave */
static int
parse_slave_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct scheduler_init_params *param = extra_args;

	if (param->nb_slaves >= RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES) {
		CR_SCHED_LOG(ERR, "Too many slaves.");
		return -ENOMEM;
	}

	strncpy(param->slave_names[param->nb_slaves++], value,
			RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN - 1);

	return 0;
}

static int
parse_mode_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct scheduler_init_params *param = extra_args;
	uint32_t i;

	for (i = 0; i < RTE_DIM(scheduler_mode_map); i++) {
		if (strcmp(value, scheduler_mode_map[i].name) == 0) {
			param->mode = (enum rte_cryptodev_scheduler_mode)
					scheduler_mode_map[i].val;

			break;
		}
	}

	if (i == RTE_DIM(scheduler_mode_map)) {
		CR_SCHED_LOG(ERR, "Unrecognized input.");
		return -EINVAL;
	}

	return 0;
}

static int
parse_mode_param_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct scheduler_init_params *param = extra_args;

	strlcpy(param->mode_param_str, value,
			RTE_CRYPTODEV_SCHEDULER_NAME_MAX_LEN);

	return 0;
}

static int
parse_ordering_arg(const char *key __rte_unused,
		const char *value, void *extra_args)
{
	struct scheduler_init_params *param = extra_args;
	uint32_t i;

	for (i = 0; i < RTE_DIM(scheduler_ordering_map); i++) {
		if (strcmp(value, scheduler_ordering_map[i].name) == 0) {
			param->enable_ordering =
					scheduler_ordering_map[i].val;
			break;
		}
	}

	if (i == RTE_DIM(scheduler_ordering_map)) {
		CR_SCHED_LOG(ERR, "Unrecognized input.");
		return -EINVAL;
	}

	return 0;
}

static int
scheduler_parse_init_params(struct scheduler_init_params *params,
		const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;

	if (input_args) {
		kvlist = rte_kvargs_parse(input_args,
				scheduler_valid_params);
		if (kvlist == NULL)
			return -1;

		ret = rte_kvargs_process(kvlist,
				RTE_CRYPTODEV_VDEV_MAX_NB_QP_ARG,
				&parse_integer_arg,
				&params->def_p.max_nb_queue_pairs);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_SOCKET_ID,
				&parse_integer_arg,
				&params->def_p.socket_id);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_COREMASK,
				&parse_coremask_arg,
				params);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_CORELIST,
				&parse_corelist_arg,
				params);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_NAME,
				&parse_name_arg,
				&params->def_p);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_SLAVE,
				&parse_slave_arg, params);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_MODE,
				&parse_mode_arg, params);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_MODE_PARAM,
				&parse_mode_param_arg, params);
		if (ret < 0)
			goto free_kvlist;

		ret = rte_kvargs_process(kvlist, RTE_CRYPTODEV_VDEV_ORDERING,
				&parse_ordering_arg, params);
		if (ret < 0)
			goto free_kvlist;
	}

free_kvlist:
	rte_kvargs_free(kvlist);
	return ret;
}

static int
cryptodev_scheduler_probe(struct rte_vdev_device *vdev)
{
	struct scheduler_init_params init_params = {
		.def_p = {
			"",
			sizeof(struct scheduler_ctx),
			rte_socket_id(),
			RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_QUEUE_PAIRS
		},
		.nb_slaves = 0,
		.mode = CDEV_SCHED_MODE_NOT_SET,
		.enable_ordering = 0,
		.slave_names = { {0} }
	};
	const char *name;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	scheduler_parse_init_params(&init_params,
				    rte_vdev_device_args(vdev));


	return cryptodev_scheduler_create(name,
					vdev,
					&init_params);
}

static struct rte_vdev_driver cryptodev_scheduler_pmd_drv = {
	.probe = cryptodev_scheduler_probe,
	.remove = cryptodev_scheduler_remove
};

static struct cryptodev_driver scheduler_crypto_drv;

RTE_PMD_REGISTER_VDEV(CRYPTODEV_NAME_SCHEDULER_PMD,
	cryptodev_scheduler_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(CRYPTODEV_NAME_SCHEDULER_PMD,
	"max_nb_queue_pairs=<int> "
	"socket_id=<int> "
	"slave=<name>");
RTE_PMD_REGISTER_CRYPTO_DRIVER(scheduler_crypto_drv,
		cryptodev_scheduler_pmd_drv.driver,
		cryptodev_scheduler_driver_id);
