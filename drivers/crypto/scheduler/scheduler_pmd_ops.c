/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */
#include <string.h>

#include <rte_common.h>
#include <rte_malloc.h>
#include <dev_driver.h>
#include <rte_cryptodev.h>
#include <cryptodev_pmd.h>
#include <rte_security_driver.h>
#include <rte_reorder.h>
#include <rte_errno.h>

#include "scheduler_pmd_private.h"

struct scheduler_configured_sess_info {
	uint8_t dev_id;
	uint8_t driver_id;
	union {
		struct rte_cryptodev_sym_session *sess;
		struct {
			struct rte_security_session *sec_sess;
			struct rte_security_ctx *sec_ctx;
		};
	};
};

static int
scheduler_session_create(void *sess, void *sess_params,
		struct scheduler_ctx *sched_ctx,
		enum rte_crypto_op_sess_type session_type)
{
	struct rte_mempool *mp = rte_mempool_from_obj(sess);
	struct scheduler_session_ctx *sess_ctx;
	struct scheduler_configured_sess_info configured_sess[
			RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS] = {{0}};
	uint32_t i, j, n_configured_sess = 0;
	int ret = 0;

	if (session_type == RTE_CRYPTO_OP_WITH_SESSION)
		sess_ctx = CRYPTODEV_GET_SYM_SESS_PRIV(sess);
	else
		sess_ctx = SECURITY_GET_SESS_PRIV(sess);

	if (mp == NULL)
		return -EINVAL;

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		struct scheduler_worker *worker = &sched_ctx->workers[i];
		struct rte_cryptodev *dev = &rte_cryptodevs[worker->dev_id];
		uint8_t next_worker = 0;

		for (j = 0; j < n_configured_sess; j++) {
			if (configured_sess[j].driver_id == worker->driver_id) {
				if (session_type == RTE_CRYPTO_OP_WITH_SESSION)
					sess_ctx->worker_sess[i] =
						configured_sess[j].sess;
				else
					sess_ctx->worker_sec_sess[i] =
						configured_sess[j].sec_sess;

				next_worker = 1;
				break;
			}
		}
		if (next_worker)
			continue;

		if (rte_mempool_avail_count(mp) == 0) {
			ret = -ENOMEM;
			goto error_exit;
		}

		if (session_type == RTE_CRYPTO_OP_WITH_SESSION) {
			struct rte_cryptodev_sym_session *worker_sess =
				rte_cryptodev_sym_session_create(worker->dev_id,
						sess_params, mp);

			if (worker_sess == NULL) {
				ret = -rte_errno;
				goto error_exit;
			}

			worker_sess->opaque_data = (uint64_t)sess;
			sess_ctx->worker_sess[i] = worker_sess;
			configured_sess[n_configured_sess].sess = worker_sess;
		} else {
			struct rte_security_session *worker_sess =
				rte_security_session_create(dev->security_ctx,
						sess_params, mp);

			if (worker_sess == NULL) {
				ret = -rte_errno;
				goto error_exit;
			}

			worker_sess->opaque_data = (uint64_t)sess;
			sess_ctx->worker_sec_sess[i] = worker_sess;
			configured_sess[n_configured_sess].sec_sess =
							worker_sess;
			configured_sess[n_configured_sess].sec_ctx =
							dev->security_ctx;
		}

		configured_sess[n_configured_sess].driver_id =
							worker->driver_id;
		configured_sess[n_configured_sess].dev_id = worker->dev_id;
		n_configured_sess++;
	}

	return 0;

error_exit:
	sess_ctx->ref_cnt = sched_ctx->ref_cnt;
	for (i = 0; i < n_configured_sess; i++) {
		if (session_type == RTE_CRYPTO_OP_WITH_SESSION)
			rte_cryptodev_sym_session_free(
						configured_sess[i].dev_id,
						configured_sess[i].sess);
		else
			rte_security_session_destroy(
						configured_sess[i].sec_ctx,
						configured_sess[i].sec_sess);
	}

	return ret;
}

static void
scheduler_session_destroy(void *sess, struct scheduler_ctx *sched_ctx,
		uint8_t session_type)
{
	struct scheduler_session_ctx *sess_ctx;
	struct scheduler_configured_sess_info deleted_sess[
			RTE_CRYPTODEV_SCHEDULER_MAX_NB_WORKERS] = {{0}};
	uint32_t i, j, n_deleted_sess = 0;

	if (session_type == RTE_CRYPTO_OP_WITH_SESSION)
		sess_ctx = CRYPTODEV_GET_SYM_SESS_PRIV(sess);
	else
		sess_ctx = SECURITY_GET_SESS_PRIV(sess);

	if (sched_ctx->ref_cnt != sess_ctx->ref_cnt) {
		CR_SCHED_LOG(WARNING,
			"Worker updated between session creation/deletion. "
			"The session may not be freed fully.");
	}

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		struct scheduler_worker *worker = &sched_ctx->workers[i];
		struct rte_cryptodev *dev = &rte_cryptodevs[worker->dev_id];
		uint8_t next_worker = 0;

		for (j = 0; j < n_deleted_sess; j++) {
			if (deleted_sess[j].driver_id == worker->driver_id) {
				if (session_type == RTE_CRYPTO_OP_WITH_SESSION)
					sess_ctx->worker_sess[i] = NULL;
				else
					sess_ctx->worker_sec_sess[i] = NULL;

				next_worker = 1;
				break;
			}
		}
		if (next_worker)
			continue;

		if (session_type == RTE_CRYPTO_OP_WITH_SESSION) {
			rte_cryptodev_sym_session_free(worker->dev_id,
						sess_ctx->worker_sess[i]);
			sess_ctx->worker_sess[i] = NULL;
		} else {
			rte_security_session_destroy(dev->security_ctx,
						sess_ctx->worker_sec_sess[i]);
			sess_ctx->worker_sec_sess[i] = NULL;
		}

		deleted_sess[n_deleted_sess++].driver_id = worker->driver_id;
	}
}

static unsigned int
scheduler_session_size_get(struct scheduler_ctx *sched_ctx,
		uint8_t session_type)
{
	uint8_t i = 0;
	uint32_t max_priv_sess_size = 0;

	/* Check what is the maximum private session size for all workers */
	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;
		struct rte_cryptodev *dev = &rte_cryptodevs[worker_dev_id];
		struct rte_security_ctx *sec_ctx = dev->security_ctx;
		uint32_t priv_sess_size = 0;

		if (session_type == RTE_CRYPTO_OP_WITH_SESSION) {
			priv_sess_size =
				(*dev->dev_ops->sym_session_get_size)(dev);
		} else {
			priv_sess_size = (*sec_ctx->ops->session_get_size)(dev);
		}

		max_priv_sess_size = RTE_MAX(max_priv_sess_size, priv_sess_size);
	}

	return max_priv_sess_size;
}

/** attaching the workers predefined by scheduler's EAL options */
static int
scheduler_attach_init_worker(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint8_t scheduler_id = dev->data->dev_id;
	int i;

	for (i = sched_ctx->nb_init_workers - 1; i >= 0; i--) {
		const char *dev_name = sched_ctx->init_worker_names[i];
		struct rte_cryptodev *worker_dev =
				rte_cryptodev_pmd_get_named_dev(dev_name);
		int status;

		if (!worker_dev) {
			CR_SCHED_LOG(ERR, "Failed to locate worker dev %s",
					dev_name);
			return -EINVAL;
		}

		status = rte_cryptodev_scheduler_worker_attach(
				scheduler_id, worker_dev->data->dev_id);

		if (status < 0) {
			CR_SCHED_LOG(ERR, "Failed to attach worker cryptodev %u",
					worker_dev->data->dev_id);
			return status;
		}

		CR_SCHED_LOG(INFO, "Scheduler %s attached worker %s",
				dev->data->name,
				sched_ctx->init_worker_names[i]);

		rte_free(sched_ctx->init_worker_names[i]);
		sched_ctx->init_worker_names[i] = NULL;

		sched_ctx->nb_init_workers -= 1;
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

	/* although scheduler_attach_init_worker presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	ret = scheduler_attach_init_worker(dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;

		ret = rte_cryptodev_configure(worker_dev_id, config);
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
			sched_ctx->nb_workers * PER_WORKER_BUFF_SIZE);

		if (qp_ctx->order_ring) {
			rte_ring_free(qp_ctx->order_ring);
			qp_ctx->order_ring = NULL;
		}

		if (!buff_size)
			return 0;

		if (snprintf(order_ring_name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"%s_rb_%u_%u", RTE_STR(CRYPTODEV_NAME_SCHEDULER_PMD),
			dev->data->dev_id, qp_id) < 0) {
			CR_SCHED_LOG(ERR, "failed to create unique reorder buffer"
					"name");
			return -ENOMEM;
		}

		qp_ctx->order_ring = rte_ring_create(order_ring_name,
				buff_size, rte_socket_id(),
				RING_F_SP_ENQ | RING_F_SC_DEQ);
		if (!qp_ctx->order_ring) {
			CR_SCHED_LOG(ERR, "failed to create order ring");
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

	/* although scheduler_attach_init_worker presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	ret = scheduler_attach_init_worker(dev);
	if (ret < 0)
		return ret;

	for (i = 0; i < dev->data->nb_queue_pairs; i++) {
		ret = update_order_ring(dev, i);
		if (ret < 0) {
			CR_SCHED_LOG(ERR, "Failed to update reorder buffer");
			return ret;
		}
	}

	if (sched_ctx->mode == CDEV_SCHED_MODE_NOT_SET) {
		CR_SCHED_LOG(ERR, "Scheduler mode is not set");
		return -1;
	}

	if (!sched_ctx->nb_workers) {
		CR_SCHED_LOG(ERR, "No worker in the scheduler");
		return -1;
	}

	if (*sched_ctx->ops.worker_attach == NULL)
		return -ENOTSUP;

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;

		if ((*sched_ctx->ops.worker_attach)(dev, worker_dev_id) < 0) {
			CR_SCHED_LOG(ERR, "Failed to attach worker");
			return -ENOTSUP;
		}
	}

	if (*sched_ctx->ops.scheduler_start == NULL)
		return -ENOTSUP;

	if ((*sched_ctx->ops.scheduler_start)(dev) < 0) {
		CR_SCHED_LOG(ERR, "Scheduler start failed");
		return -1;
	}

	/* start all workers */
	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;
		ret = rte_cryptodev_start(worker_dev_id);
		if (ret < 0) {
			CR_SCHED_LOG(ERR, "Failed to start worker dev %u",
					worker_dev_id);
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

	/* stop all workers first */
	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;

		rte_cryptodev_stop(worker_dev_id);
	}

	if (*sched_ctx->ops.scheduler_stop)
		(*sched_ctx->ops.scheduler_stop)(dev);

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;

		if (*sched_ctx->ops.worker_detach)
			(*sched_ctx->ops.worker_detach)(dev, worker_dev_id);
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

	/* close all workers first */
	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;
		struct rte_cryptodev *worker_dev =
				rte_cryptodev_pmd_get_dev(worker_dev_id);

		ret = (*worker_dev->dev_ops->dev_close)(worker_dev);
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

	scheduler_free_capabilities(sched_ctx);

	return 0;
}

/** Get device statistics */
static void
scheduler_pmd_stats_get(struct rte_cryptodev *dev,
	struct rte_cryptodev_stats *stats)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;
		struct rte_cryptodev *worker_dev =
				rte_cryptodev_pmd_get_dev(worker_dev_id);
		struct rte_cryptodev_stats worker_stats = {0};

		(*worker_dev->dev_ops->stats_get)(worker_dev, &worker_stats);

		stats->enqueued_count += worker_stats.enqueued_count;
		stats->dequeued_count += worker_stats.dequeued_count;

		stats->enqueue_err_count += worker_stats.enqueue_err_count;
		stats->dequeue_err_count += worker_stats.dequeue_err_count;
	}
}

/** Reset device statistics */
static void
scheduler_pmd_stats_reset(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t i;

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;
		struct rte_cryptodev *worker_dev =
				rte_cryptodev_pmd_get_dev(worker_dev_id);

		(*worker_dev->dev_ops->stats_reset)(worker_dev);
	}
}

/** Get device info */
static void
scheduler_pmd_info_get(struct rte_cryptodev *dev,
		struct rte_cryptodev_info *dev_info)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	uint32_t max_nb_sess = 0;
	uint16_t headroom_sz = 0;
	uint16_t tailroom_sz = 0;
	uint32_t i;

	if (!dev_info)
		return;

	/* although scheduler_attach_init_worker presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	scheduler_attach_init_worker(dev);

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_dev_id = sched_ctx->workers[i].dev_id;
		struct rte_cryptodev_info worker_info;

		rte_cryptodev_info_get(worker_dev_id, &worker_info);
		uint32_t dev_max_sess = worker_info.sym.max_nb_sessions;
		if (dev_max_sess != 0) {
			if (max_nb_sess == 0 ||	dev_max_sess < max_nb_sess)
				max_nb_sess = worker_info.sym.max_nb_sessions;
		}

		/* Get the max headroom requirement among worker PMDs */
		headroom_sz = worker_info.min_mbuf_headroom_req >
				headroom_sz ?
				worker_info.min_mbuf_headroom_req :
				headroom_sz;

		/* Get the max tailroom requirement among worker PMDs */
		tailroom_sz = worker_info.min_mbuf_tailroom_req >
				tailroom_sz ?
				worker_info.min_mbuf_tailroom_req :
				tailroom_sz;
	}

	dev_info->driver_id = dev->driver_id;
	dev_info->feature_flags = dev->feature_flags;
	dev_info->capabilities = sched_ctx->capabilities;
	dev_info->max_nb_queue_pairs = sched_ctx->max_nb_queue_pairs;
	dev_info->min_mbuf_headroom_req = headroom_sz;
	dev_info->min_mbuf_tailroom_req = tailroom_sz;
	dev_info->sym.max_nb_sessions = max_nb_sess;
}

/** Release queue pair */
static int
scheduler_pmd_qp_release(struct rte_cryptodev *dev, uint16_t qp_id)
{
	struct scheduler_qp_ctx *qp_ctx = dev->data->queue_pairs[qp_id];

	if (!qp_ctx)
		return 0;

	rte_ring_free(qp_ctx->order_ring);
	rte_free(qp_ctx->private_qp_ctx);

	rte_free(qp_ctx);
	dev->data->queue_pairs[qp_id] = NULL;

	return 0;
}

/** Setup a queue pair */
static int
scheduler_pmd_qp_setup(struct rte_cryptodev *dev, uint16_t qp_id,
	const struct rte_cryptodev_qp_conf *qp_conf, int socket_id)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;
	struct scheduler_qp_ctx *qp_ctx;
	char name[RTE_CRYPTODEV_NAME_MAX_LEN];
	uint32_t i;
	int ret;

	if (snprintf(name, RTE_CRYPTODEV_NAME_MAX_LEN,
			"CRYTO_SCHE PMD %u QP %u",
			dev->data->dev_id, qp_id) < 0) {
		CR_SCHED_LOG(ERR, "Failed to create unique queue pair name");
		return -EFAULT;
	}

	/* Free memory prior to re-allocation if needed. */
	if (dev->data->queue_pairs[qp_id] != NULL)
		scheduler_pmd_qp_release(dev, qp_id);

	for (i = 0; i < sched_ctx->nb_workers; i++) {
		uint8_t worker_id = sched_ctx->workers[i].dev_id;

		/*
		 * All workers will share the same session mempool
		 * for session-less operations, so the objects
		 * must be big enough for all the drivers used.
		 */
		ret = rte_cryptodev_queue_pair_setup(worker_id, qp_id,
				qp_conf, socket_id);
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

	/* although scheduler_attach_init_worker presents multiple times,
	 * there will be only 1 meaningful execution.
	 */
	ret = scheduler_attach_init_worker(dev);
	if (ret < 0) {
		CR_SCHED_LOG(ERR, "Failed to attach worker");
		scheduler_pmd_qp_release(dev, qp_id);
		return ret;
	}

	if (*sched_ctx->ops.config_queue_pair) {
		if ((*sched_ctx->ops.config_queue_pair)(dev, qp_id) < 0) {
			CR_SCHED_LOG(ERR, "Unable to configure queue pair");
			return -1;
		}
	}

	return 0;
}

static uint32_t
scheduler_pmd_sym_session_get_size(struct rte_cryptodev *dev)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;

	return scheduler_session_size_get(sched_ctx, RTE_CRYPTO_OP_WITH_SESSION);
}

static int
scheduler_pmd_sym_session_configure(struct rte_cryptodev *dev,
	struct rte_crypto_sym_xform *xform,
	struct rte_cryptodev_sym_session *sess)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;

	return scheduler_session_create(sess, xform, sched_ctx, RTE_CRYPTO_OP_WITH_SESSION);
}

/** Clear the memory of session so it doesn't leave key material behind */
static void
scheduler_pmd_sym_session_clear(struct rte_cryptodev *dev,
		struct rte_cryptodev_sym_session *sess)
{
	struct scheduler_ctx *sched_ctx = dev->data->dev_private;

	scheduler_session_destroy(sess, sched_ctx, RTE_CRYPTO_OP_WITH_SESSION);
}

static struct rte_cryptodev_ops scheduler_pmd_ops = {
		.dev_configure		= scheduler_pmd_config,
		.dev_start		= scheduler_pmd_start,
		.dev_stop		= scheduler_pmd_stop,
		.dev_close		= scheduler_pmd_close,

		.stats_get		= scheduler_pmd_stats_get,
		.stats_reset		= scheduler_pmd_stats_reset,

		.dev_infos_get		= scheduler_pmd_info_get,

		.queue_pair_setup	= scheduler_pmd_qp_setup,
		.queue_pair_release	= scheduler_pmd_qp_release,

		.sym_session_get_size	= scheduler_pmd_sym_session_get_size,
		.sym_session_configure	= scheduler_pmd_sym_session_configure,
		.sym_session_clear	= scheduler_pmd_sym_session_clear,
};

struct rte_cryptodev_ops *rte_crypto_scheduler_pmd_ops = &scheduler_pmd_ops;

/** Configure a scheduler session from a security session configuration */
static int
scheduler_pmd_sec_sess_create(void *dev, struct rte_security_session_conf *conf,
			struct rte_security_session *sess)
{
	struct rte_cryptodev *cdev = dev;
	struct scheduler_ctx *sched_ctx = cdev->data->dev_private;

	/* Check for supported security protocols */
	if (!scheduler_check_sec_proto_supp(conf->action_type, conf->protocol)) {
		CR_SCHED_LOG(ERR, "Unsupported security protocol");
		return -ENOTSUP;
	}

	return scheduler_session_create(sess, conf, sched_ctx, RTE_CRYPTO_OP_SECURITY_SESSION);
}

/** Clear the memory of session so it doesn't leave key material behind */
static int
scheduler_pmd_sec_sess_destroy(void *dev,
			       struct rte_security_session *sess)
{
	struct rte_cryptodev *cdev = dev;
	struct scheduler_ctx *sched_ctx = cdev->data->dev_private;

	scheduler_session_destroy(sess, sched_ctx, RTE_CRYPTO_OP_SECURITY_SESSION);

	return 0;
}

/** Get sync security capabilities for scheduler pmds */
static const struct rte_security_capability *
scheduler_pmd_sec_capa_get(void *dev)
{
	struct rte_cryptodev *cdev = dev;
	struct scheduler_ctx *sched_ctx = cdev->data->dev_private;

	return sched_ctx->sec_capabilities;
}

static unsigned int
scheduler_pmd_sec_sess_size_get(void *dev)
{
	struct rte_cryptodev *cdev = dev;
	struct scheduler_ctx *sched_ctx = cdev->data->dev_private;

	return scheduler_session_size_get(sched_ctx,
				RTE_CRYPTO_OP_SECURITY_SESSION);
}

static struct rte_security_ops scheduler_pmd_sec_ops = {
		.session_create = scheduler_pmd_sec_sess_create,
		.session_update = NULL,
		.session_get_size = scheduler_pmd_sec_sess_size_get,
		.session_stats_get = NULL,
		.session_destroy = scheduler_pmd_sec_sess_destroy,
		.set_pkt_metadata = NULL,
		.capabilities_get = scheduler_pmd_sec_capa_get
};

struct rte_security_ops *rte_crypto_scheduler_pmd_sec_ops =
							&scheduler_pmd_sec_ops;
