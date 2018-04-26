/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */
#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <rte_dev.h>
#include <rte_cryptodev.h>
#include <rte_cryptodev_pmd.h>
#include <rte_reorder.h>

#include "scheduler_pmd_private.h"

/** attaching the slaves predefined by scheduler's EAL options */
static int
scheduler_attach_init_slave(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint8_t scheduler_id = dev->data->dev_id;
	int i;

	for (i = sched_ctx->nb_init_slaves - 1; i >= 0; i--) {
		const char *dev_name = sched_ctx->init_slave_names[i];
		struct rte_cryptodev *slave_dev =
				rte_cryptodev_pmd_get_named_dev(dev_name);
		int status;

		if (!slave_dev) {
			CS_LOG_ERR("Failed to locate slave dev %s",
					dev_name);
			return -EINVAL;
		}

		status = rte_cryptodev_scheduler_slave_attach(
				scheduler_id, slave_dev->data->dev_id);

		if (status < 0) {
			CS_LOG_ERR("Failed to attach slave cryptodev %u",
					slave_dev->data->dev_id);
			return status;
		}

		CS_LOG_INFO("Scheduler %s attached slave %s\n",
				dev->data->name,
				sched_ctx->init_slave_names[i]);

		rte_free(sched_ctx->init_slave_names[i]);
		sched_ctx->init_slave_names[i] = NULL;

		sched_ctx->nb_init_slaves -= 1;
	}

	return 0;
}
/** Configure device */
static int
scheduler_pmd_config(struct rte_cryptodev *dev,
		struct rte_cryptodev_config *config)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;
	int ret;

	/* although scheduler_attach_init_slave presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	ret = scheduler_attach_init_slave(dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;

		ret = rte_cryptodev_configure(slave_dev_id, config);
		if (ret < 0)
			break;
	}

	return ret;
}

static int
update_order_ring(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[qp_id];

	if (sched_ctx->reordering_enabled) {
		char order_ring_name[RTE_CRYPTODEV_NAME_MAX_LEN];
		uint32_t buff_size = rte_align32pow2(
			sched_ctx->nb_slaves * PER_SLAVE_BUFF_SIZE);

		if (qp_ctx->order_ring) {
			rte_ring_free(qp_ctx->order_ring);
			qp_ctx->order_ring = NULL;
		}

		if (!buff_size)
			return 0;

		if (snprintf(order_ring_name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"%s_rb_%u_%u", RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),
			dev->data->dev_id, qp_id) < 0) {
			CS_LOG_ERR("failed to create unique reorder buffer "
					"name");
			return -ENOMEM;
		}

		qp_ctx->order_ring = rte_ring_create(order_ring_name,
				buff_size, rte_socket_id(),
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (!qp_ctx->order_ring) {
			CS_LOG_ERR("failed to create order ring");
			return -ENOMEM;
		}
	} else {
		if (qp_ctx->order_ring) {
			rte_ring_free(qp_ctx->order_ring);
			qp_ctx->order_ring = NULL;
		}
	}

	return 0;
}

/** Start device */
static int
scheduler_pmd_start(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;
	int ret;

	if (dev->data->dev_started)
		return 0;

	/* although scheduler_attach_init_slave presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	ret = scheduler_attach_init_slave(dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = update_order_ring(dev, i);
		if (ret < 0) {
			CS_LOG_ERR("Failed to update reorder buffer");
			return ret;
		}
	}

	if (sched_ctx->mode == CDEV_SCHED_MODE_NOT_SET) {
		CS_LOG_ERR("Scheduler mode is not set");
		return -1;
	}

	if (!sched_ctx->nb_slaves) {
		CS_LOG_ERR("No slave in the scheduler");
		return -1;
	}

	RTE_FUNC_PTR_OR_ERR_RET(*sched_ctx->ops.slave_attach, -ENOTSUP);

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;

		if ((*sched_ctx->ops.slave_attach)(dev, slave_dev_id) < 0) {
			CS_LOG_ERR("Failed to attach slave");
			return -ENOTSUP;
		}
	}

	RTE_FUNC_PTR_OR_ERR_RET(*sched_ctx->ops.scheduler_start, -ENOTSUP);

	if ((*sched_ctx->ops.scheduler_start)(dev) < 0) {
		CS_LOG_ERR("Scheduler start failed");
		return -1;
	}

	/* start all slaves */
	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;
		struct rte_cryptodev *slave_dev =
				rte_cryptodev_pmd_get_dev(slave_dev_id);

		ret = (*slave_dev->dev_ops->dev_start)(slave_dev);
		if (ret < 0) {
			CS_LOG_ERR("Failed to start slave dev %u",
					slave_dev_id);
			return ret;
		}
	}

	return 0;
}

/** Stop device */
static void
scheduler_pmd_stop(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;

	if (!dev->data->dev_started)
		return;

	/* stop all slaves first */
	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;
		struct rte_cryptodev *slave_dev =
				rte_cryptodev_pmd_get_dev(slave_dev_id);

		(*slave_dev->dev_ops->dev_stop)(slave_dev);
	}

	if (*sched_ctx->ops.scheduler_stop)
		(*sched_ctx->ops.scheduler_stop)(dev);

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;

		if (*sched_ctx->ops.slave_detach)
			(*sched_ctx->ops.slave_detach)(dev, slave_dev_id);
	}
}

/** Close device */
static int
scheduler_pmd_close(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;
	int ret;

	/* the dev should be stopped before being closed */
	if (dev->data->dev_started)
		return -EBUSY;

	/* close all slaves first */
	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;
		struct rte_cryptodev *slave_dev =
				rte_cryptodev_pmd_get_dev(slave_dev_id);

		ret = (*slave_dev->dev_ops->dev_close)(slave_dev);
		if (ret < 0)
			return ret;
	}

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[i];

		if (qp_ctx->order_ring) {
			rte_ring_free(qp_ctx->order_ring);
			qp_ctx->order_ring = NULL;
		}

		if (qp_ctx->private_qp_ctx) {
			rte_free(qp_ctx->private_qp_ctx);
			qp_ctx->private_qp_ctx = NULL;
		}
	}

	if (sched_ctx->private_ctx) {
		rte_free(sched_ctx->private_ctx);
		sched_ctx->private_ctx = NULL;
	}

	if (sched_ctx->capabilities) {
		rte_free(sched_ctx->capabilities);
		sched_ctx->capabilities = NULL;
	}

	return 0;
}

/** Get device statistics */
static void
scheduler_pmd_stats_get(struct rte_cryptodev *dev,
	struct rte_cryptodev_stats *stats)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;
		struct rte_cryptodev *slave_dev =
				rte_cryptodev_pmd_get_dev(slave_dev_id);
		struct rte_cryptodev_stats slave_stats = {0};

		(*slave_dev->dev_ops->stats_get)(slave_dev, &slave_stats);

		stats->enqueued_count += slave_stats.enqueued_count;
		stats->dequeued_count += slave_stats.dequeued_count;

		stats->enqueue_err_count += slave_stats.enqueue_err_count;
		stats->dequeue_err_count += slave_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
scheduler_pmd_stats_reset(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;
		struct rte_cryptodev *slave_dev =
				rte_cryptodev_pmd_get_dev(slave_dev_id);

		(*slave_dev->dev_ops->stats_reset)(slave_dev);
	}
}

/** Get device info */
static void
scheduler_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t max_nb_sessions = sched_ctx->nb_slaves ?
			UINT32_MAX : RTE_CRYPTODEV_PMD_DEFAULT_MAX_NB_SESSIONS;
	uint32_t i;

	if (!dev_info)
		return;

	/* although scheduler_attach_init_slave presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	scheduler_attach_init_slave(dev);

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;
		struct rte_cryptodev_info slave_info;

		rte_cryptodev_info_get(slave_dev_id, &slave_info);
		max_nb_sessions = slave_info.sym.max_nb_sessions <
				max_nb_sessions ?
				slave_info.sym.max_nb_sessions :
				max_nb_sessions;
	}

	dev_info->driver_id = dev->driver_id;
	dev_info->feature_flags = dev->feature_flags;
	dev_info->capabilities = sched_ctx->capabilities;
	dev_info->max_nb_queue_pairs = sched_ctx->max_nb_queue_pairs;
	dev_info->sym.max_nb_sessions = max_nb_sessions;
}

/** Release queue pair */
static int
scheduler_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[qp_id];

	if (!qp_ctx)
		return 0;

	if (qp_ctx->order_ring)
		rte_ring_free(qp_ctx->order_ring);
	if (qp_ctx->private_qp_ctx)
		rte_free(qp_ctx->private_qp_ctx);

	rte_free(qp_ctx);
	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

/** Setup a queue pair */
static int
scheduler_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
	const struct rte_cryptodev_qp_conf *qp_conf, int socket_id,
	struct rte_mempool *session_pool)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct scheduler_qp_ctx *qp_ctx;
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	uint32_t i;
	int ret;

	if (snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"CRYTO_SCHE PMD %u QP %u",
			dev->data->dev_id, qp_id) < 0) {
		CS_LOG_ERR("Failed to create unique queue pair name");
		return -EFAULT;
	}

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		scheduler_pmd_qp_release(dev, qp_id);

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_id = sched_ctx->slaves[i].dev_id;

		/*
		 * All slaves will share the same session mempool
		 * for session-less operations, so the objects
		 * must be big enough for all the drivers used.
		 */
		ret = rte_cryptodev_queue_pair_setup(slave_id, qp_id,
				qp_conf, socket_id, session_pool);
		if (ret < 0)
			return ret;
	}

	/* Allocate the queue pair data structure. */
	qp_ctx = rte_zmalloc_socket(name, sizeof(*qp_ctx), RTE_CACHE_LINE_SIZE,
			socket_id);
	if (qp_ctx == NULL)
		return -ENOMEM;

	/* The actual available object number = nb_descriptors - 1 */
	qp_ctx->max_nb_objs = qp_conf->nb_descriptors - 1;

	dev->data->queue_pairs[qp_id] = qp_ctx;

	/* although scheduler_attach_init_slave presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	ret = scheduler_attach_init_slave(dev);
	if (ret < 0) {
		CS_LOG_ERR("Failed to attach slave");
		scheduler_pmd_qp_release(dev, qp_id);
		return ret;
	}

	if (*sched_ctx->ops.config_queue_pair) {
		if ((*sched_ctx->ops.config_queue_pair)(dev, qp_id) < 0) {
			CS_LOG_ERR("Unable to configure queue pair");
			return -1;
		}
	}

	return 0;
}

/** Start queue pair */
static int
scheduler_pmd_qp_start(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Stop queue pair */
static int
scheduler_pmd_qp_stop(__rte_unused struct rte_cryptodev *dev,
		__rte_unused uint16_t queue_pair_id)
{
	return -ENOTSUP;
}

/** Return the number of allocated queue pairs */
static uint32_t
scheduler_pmd_qp_count(struct rte_cryptodev *dev)
{
	return dev->data->nb_queue_pairs;
}

static uint32_t
scheduler_pmd_session_get_size(struct rte_cryptodev *dev __rte_unused)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint8_t i = 0;
	uint32_t max_priv_sess_size = 0;

	/* Check what is the maximum private session size for all slaves */
	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		uint8_t slave_dev_id = sched_ctx->slaves[i].dev_id;
		struct rte_cryptodev *dev = &rte_cryptodevs[slave_dev_id];
		uint32_t priv_sess_size = (*dev->dev_ops->session_get_size)(dev);

		if (max_priv_sess_size < priv_sess_size)
			max_priv_sess_size = priv_sess_size;
	}

	return max_priv_sess_size;
}

static int
scheduler_pmd_session_configure(struct rte_cryptodev *dev,
	struct rte_crypto_sym_xform *xform,
	struct rte_cryptodev_sym_session *sess,
	struct rte_mempool *mempool)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;
	int ret;

	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		struct scheduler_slave *slave = &sched_ctx->slaves[i];

		ret = rte_cryptodev_sym_session_init(slave->dev_id, sess,
					xform, mempool);
		if (ret < 0) {
			CS_LOG_ERR("unabled to config sym session");
			return ret;
		}
	}

	return 0;
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
scheduler_pmd_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;

	/* Clear private data of slaves */
	for (i = 0; i < sched_ctx->nb_slaves; i++) {
		struct scheduler_slave *slave = &sched_ctx->slaves[i];

		rte_cryptodev_sym_session_clear(slave->dev_id, sess);
	}
}

struct rte_cryptodev_ops scheduler_pmd_ops = {
		.dev_configure		= scheduler_pmd_config,
		.dev_start		= scheduler_pmd_start,
		.dev_stop		= scheduler_pmd_stop,
		.dev_close		= scheduler_pmd_close,

		.stats_get		= scheduler_pmd_stats_get,
		.stats_reset		= scheduler_pmd_stats_reset,

		.dev_infos_get		= scheduler_pmd_info_get,

		.queue_pair_setup	= scheduler_pmd_qp_setup,
		.queue_pair_release	= scheduler_pmd_qp_release,
		.queue_pair_start	= scheduler_pmd_qp_start,
		.queue_pair_stop	= scheduler_pmd_qp_stop,
		.queue_pair_count	= scheduler_pmd_qp_count,

		.session_get_size	= scheduler_pmd_session_get_size,
		.session_configure	= scheduler_pmd_session_configure,
		.session_clear		= scheduler_pmd_session_clear,
};

struct rte_cryptodev_ops *rte_crypto_scheduler_pmd_ops = &scheduler_pmd_ops;
