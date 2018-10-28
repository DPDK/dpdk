/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */
#include <rte_reorder.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_malloc.h>

#include "rte_cryptodev_scheduler.h"
#include "scheduler_pmd_private.h"

int scheduler_logtype_driver;

/** update the scheduler pmd's capability with attaching device's
 *  capability.
 *  For each device to be attached, the scheduler's capability should be
 *  the common capability set of all slaves
 **/
static uint32_t
sync_caps(struct rte_cryptodev_capabilities *caps,
		uint32_t nb_caps,
		const struct rte_cryptodev_capabilities *slave_caps)
{
	uint32_t sync_nb_caps = nb_caps, nb_slave_caps = 0;
	uint32_t i;

	while (slave_caps[nb_slave_caps].op != RTE_CRYPTO_OP_TYPE_UNDEFINED)
		nb_slave_caps++;

	if (nb_caps == 0) {
		rte_memcpy(caps, slave_caps, sizeof(*caps) * nb_slave_caps);
		return nb_slave_caps;
	}

	for (i = 0; i < sync_nb_caps; i++) {
		struct rte_cryptodev_capabilities *cap = &caps[i];
		uint32_t j;

		for (j = 0; j < nb_slave_caps; j++) {
			const struct rte_cryptodev_capabilities *s_cap =
					&slave_caps[j];

			if (s_cap->op != cap->op || s_cap->sym.xform_type !=
					cap->sym.xform_type)
				continue;

			if (s_cap->sym.xform_type ==
					RTE_CRYPTO_SYM_XFORM_AUTH) {
				if (s_cap->sym.auth.algo !=
						cap->sym.auth.algo)
					continue;

				cap->sym.auth.digest_size.min =
					s_cap->sym.auth.digest_size.min <
					cap->sym.auth.digest_size.min ?
					s_cap->sym.auth.digest_size.min :
					cap->sym.auth.digest_size.min;
				cap->sym.auth.digest_size.max =
					s_cap->sym.auth.digest_size.max <
					cap->sym.auth.digest_size.max ?
					s_cap->sym.auth.digest_size.max :
					cap->sym.auth.digest_size.max;

			}

			if (s_cap->sym.xform_type ==
					RTE_CRYPTO_SYM_XFORM_CIPHER)
				if (s_cap->sym.cipher.algo !=
						cap->sym.cipher.algo)
					continue;

			/* no common cap found */
			break;
		}

		if (j < nb_slave_caps)
			continue;

		/* remove a uncommon cap from the array */
		for (j = i; j < sync_nb_caps - 1; j++)
			rte_memcpy(&caps[j], &caps[j+1], sizeof(*cap));

		memset(&caps[sync_nb_caps - 1], 0, sizeof(*cap));
		sync_nb_caps--;
	}

	return sync_nb_caps;
}

static int
update_scheduler_capability(struct scheduler_ctx *sched_ctx)
{
	struct rte_cryptodev_capabilities tmp_caps[256] = { {0} };
	uint32_t nb_caps = 0, i;

	if (sched_ctx->capabilities) {
		rte_free(sched_ctx->capabilities);
		sched_ctx->capabilities = NULL;
	}

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		struct rte_cryptodev_info dev_info;

		rte_cryptodev_info_get(sched_ctx->slaves[i].dev_id, &dev_info);

		nb_caps = sync_caps(tmp_caps, nb_caps, dev_info.capabilities);
		if (nb_caps == 0)
			return -1;
	}

	sched_ctx->capabilities = rte_zmalloc_socket(NULL,
			sizeof(struct rte_cryptodev_capabilities) *
			(nb_caps + 1), 0, SOCKET_ID_ANY);
	if (!sched_ctx->capabilities)
		return -ENOMEM;

	rte_memcpy(sched_ctx->capabilities, tmp_caps,
			sizeof(struct rte_cryptodev_capabilities) * nb_caps);

	return 0;
}

static void
update_scheduler_feature_flag(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;

	dev->feature_flags = 0;

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		struct rte_cryptodev_info dev_info;

		rte_cryptodev_info_get(sched_ctx->slaves[i].dev_id, &dev_info);

		dev->feature_flags |= dev_info.feature_flags;
	}
}

static void
update_max_nb_qp(struct scheduler_ctx *sched_ctx)
{
	uint32_t i;
	uint32_t max_nb_qp;

	if (!sched_ctx->nb_slaves)
		return;

	max_nb_qp = sched_ctx->nb_slaves ? UINT32_MAX : 0;

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		struct rte_cryptodev_info dev_info;

		rte_cryptodev_info_get(sched_ctx->slaves[i].dev_id, &dev_info);
		max_nb_qp = dev_info.max_nb_queue_pairs < max_nb_qp ?
				dev_info.max_nb_queue_pairs : max_nb_qp;
	}

	sched_ctx->max_nb_queue_pairs = max_nb_qp;
}

/** Attach a device to the scheduler. */
int
rte_cryptodev_scheduler_slave_attach(uint8_t scheduler_id, uint8_t slave_id)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;
	struct scheduler_slave *slave;
	struct rte_cryptodev_info dev_info;
	uint32_t i;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->data->dev_started) {
		CR_SCHED_LOG(ERR, "Illegal operation");
		return -EBUSY;
	}

	sched_ctx = dev->data->dev_private;
	if (sched_ctx->nb_slaves >=
			RTE_CRYPTODEV_SCHEDULER_MAX_NB_SLAVES) {
		CR_SCHED_LOG(ERR, "Too many slaves attached");
		return -ENOMEM;
	}

	for (i = 0; i < sched_ctx->nb_slaves; i++)
		if (sched_ctx->slaves[i].dev_id == slave_id) {
			CR_SCHED_LOG(ERR, "Slave already added");
			return -ENOTSUP;
		}

	slave = &sched_ctx->slaves[sched_ctx->nb_slaves];

	rte_cryptodev_info_get(slave_id, &dev_info);

	slave->dev_id = slave_id;
	slave->driver_id = dev_info.driver_id;
	sched_ctx->nb_slaves++;

	if (update_scheduler_capability(sched_ctx) < 0) {
		slave->dev_id = 0;
		slave->driver_id = 0;
		sched_ctx->nb_slaves--;

		CR_SCHED_LOG(ERR, "capabilities update failed");
		return -ENOTSUP;
	}

	update_scheduler_feature_flag(dev);

	update_max_nb_qp(sched_ctx);

	return 0;
}

int
rte_cryptodev_scheduler_slave_detach(uint8_t scheduler_id, uint8_t slave_id)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;
	uint32_t i, slave_pos;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->data->dev_started) {
		CR_SCHED_LOG(ERR, "Illegal operation");
		return -EBUSY;
	}

	sched_ctx = dev->data->dev_private;

	for (slave_pos = 0; slave_pos < sched_ctx->nb_slaves; slave_pos++)
		if (sched_ctx->slaves[slave_pos].dev_id == slave_id)
			break;
	if (slave_pos == sched_ctx->nb_slaves) {
		CR_SCHED_LOG(ERR, "Cannot find slave");
		return -ENOTSUP;
	}

	if (sched_ctx->ops.slave_detach(dev, slave_id) < 0) {
		CR_SCHED_LOG(ERR, "Failed to detach slave");
		return -ENOTSUP;
	}

	for (i = slave_pos; i < sched_ctx->nb_slaves - 1; i++) {
		memcpy(&sched_ctx->slaves[i], &sched_ctx->slaves[i+1],
				sizeof(struct scheduler_slave));
	}
	memset(&sched_ctx->slaves[sched_ctx->nb_slaves - 1], 0,
			sizeof(struct scheduler_slave));
	sched_ctx->nb_slaves--;

	if (update_scheduler_capability(sched_ctx) < 0) {
		CR_SCHED_LOG(ERR, "capabilities update failed");
		return -ENOTSUP;
	}

	update_scheduler_feature_flag(dev);

	update_max_nb_qp(sched_ctx);

	return 0;
}

int
rte_cryptodev_scheduler_mode_set(uint8_t scheduler_id,
		enum rte_cryptodev_scheduler_mode mode)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->data->dev_started) {
		CR_SCHED_LOG(ERR, "Illegal operation");
		return -EBUSY;
	}

	sched_ctx = dev->data->dev_private;

	if (mode == sched_ctx->mode)
		return 0;

	switch (mode) {
	case CDEV_SCHED_MODE_ROUNDROBIN:
		if (rte_cryptodev_scheduler_load_user_scheduler(scheduler_id,
				crypto_scheduler_roundrobin) < 0) {
			CR_SCHED_LOG(ERR, "Failed to load scheduler");
			return -1;
		}
		break;
	case CDEV_SCHED_MODE_PKT_SIZE_DISTR:
		if (rte_cryptodev_scheduler_load_user_scheduler(scheduler_id,
				crypto_scheduler_pkt_size_based_distr) < 0) {
			CR_SCHED_LOG(ERR, "Failed to load scheduler");
			return -1;
		}
		break;
	case CDEV_SCHED_MODE_FAILOVER:
		if (rte_cryptodev_scheduler_load_user_scheduler(scheduler_id,
				crypto_scheduler_failover) < 0) {
			CR_SCHED_LOG(ERR, "Failed to load scheduler");
			return -1;
		}
		break;
	case CDEV_SCHED_MODE_MULTICORE:
		if (rte_cryptodev_scheduler_load_user_scheduler(scheduler_id,
				crypto_scheduler_multicore) < 0) {
			CR_SCHED_LOG(ERR, "Failed to load scheduler");
			return -1;
		}
		break;
	default:
		CR_SCHED_LOG(ERR, "Not yet supported");
		return -ENOTSUP;
	}

	return 0;
}

enum rte_cryptodev_scheduler_mode
rte_cryptodev_scheduler_mode_get(uint8_t scheduler_id)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	sched_ctx = dev->data->dev_private;

	return sched_ctx->mode;
}

int
rte_cryptodev_scheduler_ordering_set(uint8_t scheduler_id,
		uint32_t enable_reorder)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->data->dev_started) {
		CR_SCHED_LOG(ERR, "Illegal operation");
		return -EBUSY;
	}

	sched_ctx = dev->data->dev_private;

	sched_ctx->reordering_enabled = enable_reorder;

	return 0;
}

int
rte_cryptodev_scheduler_ordering_get(uint8_t scheduler_id)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	sched_ctx = dev->data->dev_private;

	return (int)sched_ctx->reordering_enabled;
}

int
rte_cryptodev_scheduler_load_user_scheduler(uint8_t scheduler_id,
		struct rte_cryptodev_scheduler *scheduler) {

	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->data->dev_started) {
		CR_SCHED_LOG(ERR, "Illegal operation");
		return -EBUSY;
	}

	sched_ctx = dev->data->dev_private;

	if (strlen(scheduler->name) > RTE_CRYPTODEV_NAME_MAX_LEN - 1) {
		CR_SCHED_LOG(ERR, "Invalid name %s, should be less than "
				"%u bytes.", scheduler->name,
				RTE_CRYPTODEV_NAME_MAX_LEN);
		return -EINVAL;
	}
	snprintf(sched_ctx->name, sizeof(sched_ctx->name), "%s",
			scheduler->name);

	if (strlen(scheduler->description) >
			RTE_CRYPTODEV_SCHEDULER_DESC_MAX_LEN - 1) {
		CR_SCHED_LOG(ERR, "Invalid description %s, should be less than "
				"%u bytes.", scheduler->description,
				RTE_CRYPTODEV_SCHEDULER_DESC_MAX_LEN - 1);
		return -EINVAL;
	}
	snprintf(sched_ctx->description, sizeof(sched_ctx->description), "%s",
			scheduler->description);

	/* load scheduler instance operations functions */
	sched_ctx->ops.config_queue_pair = scheduler->ops->config_queue_pair;
	sched_ctx->ops.create_private_ctx = scheduler->ops->create_private_ctx;
	sched_ctx->ops.scheduler_start = scheduler->ops->scheduler_start;
	sched_ctx->ops.scheduler_stop = scheduler->ops->scheduler_stop;
	sched_ctx->ops.slave_attach = scheduler->ops->slave_attach;
	sched_ctx->ops.slave_detach = scheduler->ops->slave_detach;
	sched_ctx->ops.option_set = scheduler->ops->option_set;
	sched_ctx->ops.option_get = scheduler->ops->option_get;

	if (sched_ctx->private_ctx) {
		rte_free(sched_ctx->private_ctx);
		sched_ctx->private_ctx = NULL;
	}

	if (sched_ctx->ops.create_private_ctx) {
		int ret = (*sched_ctx->ops.create_private_ctx)(dev);

		if (ret < 0) {
			CR_SCHED_LOG(ERR, "Unable to create scheduler private "
					"context");
			return ret;
		}
	}

	sched_ctx->mode = scheduler->mode;

	return 0;
}

int
rte_cryptodev_scheduler_slaves_get(uint8_t scheduler_id, uint8_t *slaves)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;
	uint32_t nb_slaves = 0;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	sched_ctx = dev->data->dev_private;

	nb_slaves = sched_ctx->nb_slaves;

	if (slaves && nb_slaves) {
		uint32_t i;

		for (i = 0; i < nb_slaves; i++)
			slaves[i] = sched_ctx->slaves[i].dev_id;
	}

	return (int)nb_slaves;
}

int
rte_cryptodev_scheduler_option_set(uint8_t scheduler_id,
		enum rte_cryptodev_schedule_option_type option_type,
		void *option)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;

	if (option_type == CDEV_SCHED_OPTION_NOT_SET ||
			option_type >= CDEV_SCHED_OPTION_COUNT) {
		CR_SCHED_LOG(ERR, "Invalid option parameter");
		return -EINVAL;
	}

	if (!option) {
		CR_SCHED_LOG(ERR, "Invalid option parameter");
		return -EINVAL;
	}

	if (dev->data->dev_started) {
		CR_SCHED_LOG(ERR, "Illegal operation");
		return -EBUSY;
	}

	sched_ctx = dev->data->dev_private;

	RTE_FUNC_PTR_OR_ERR_RET(*sched_ctx->ops.option_set, -ENOTSUP);

	return (*sched_ctx->ops.option_set)(dev, option_type, option);
}

int
rte_cryptodev_scheduler_option_get(uint8_t scheduler_id,
		enum rte_cryptodev_schedule_option_type option_type,
		void *option)
{
	struct rte_cryptodev *dev = rte_cryptodev_pmd_get_dev(scheduler_id);
	struct scheduler_ctx *sched_ctx;

	if (!dev) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	if (!option) {
		CR_SCHED_LOG(ERR, "Invalid option parameter");
		return -EINVAL;
	}

	if (dev->driver_id != cryptodev_scheduler_driver_id) {
		CR_SCHED_LOG(ERR, "Operation not supported");
		return -ENOTSUP;
	}

	sched_ctx = dev->data->dev_private;

	RTE_FUNC_PTR_OR_ERR_RET(*sched_ctx->ops.option_get, -ENOTSUP);

	return (*sched_ctx->ops.option_get)(dev, option_type, option);
}

RTE_INIT(scheduler_init_log)
{
	scheduler_logtype_driver = rte_log_register("pmd.crypto.scheduler");
}
