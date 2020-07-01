/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <string.h>

#include <rte_common.h>
#include <rte_bus_vdev.h>
#include <rte_malloc.h>
#include <rte_ring.h>
#include <rte_kvargs.h>

#include <rte_bbdev.h>
#include <rte_bbdev_pmd.h>

#define DRIVER_NAME baseband_null

RTE_LOG_REGISTER(bbdev_null_logtype, pmd.bb.null, NOTICE);

/* Helper macro for logging */
#define rte_bbdev_log(level, fmt, ...) \
	rte_log(RTE_LOG_ ## level, bbdev_null_logtype, fmt "\n", ##__VA_ARGS__)

#define rte_bbdev_log_debug(fmt, ...) \
	rte_bbdev_log(DEBUG, RTE_STR(__LINE__) ":%s() " fmt, __func__, \
		##__VA_ARGS__)

/*  Initialisation params structure that can be used by null BBDEV driver */
struct bbdev_null_params {
	int socket_id;  /*< Null BBDEV socket */
	uint16_t queues_num;  /*< Null BBDEV queues number */
};

/* Accecptable params for null BBDEV devices */
#define BBDEV_NULL_MAX_NB_QUEUES_ARG  "max_nb_queues"
#define BBDEV_NULL_SOCKET_ID_ARG      "socket_id"

static const char * const bbdev_null_valid_params[] = {
	BBDEV_NULL_MAX_NB_QUEUES_ARG,
	BBDEV_NULL_SOCKET_ID_ARG
};

/* private data structure */
struct bbdev_private {
	unsigned int max_nb_queues;  /**< Max number of queues */
};

/* queue */
struct bbdev_queue {
	struct rte_ring *processed_pkts;  /* Ring for processed packets */
} __rte_cache_aligned;

/* Get device info */
static void
info_get(struct rte_bbdev *dev, struct rte_bbdev_driver_info *dev_info)
{
	struct bbdev_private *internals = dev->data->dev_private;

	static const struct rte_bbdev_op_cap bbdev_capabilities[] = {
		RTE_BBDEV_END_OF_CAPABILITIES_LIST(),
	};

	static struct rte_bbdev_queue_conf default_queue_conf = {
		.queue_size = RTE_BBDEV_QUEUE_SIZE_LIMIT,
	};

	default_queue_conf.socket = dev->data->socket_id;

	dev_info->driver_name = RTE_STR(DRIVER_NAME);
	dev_info->max_num_queues = internals->max_nb_queues;
	dev_info->queue_size_lim = RTE_BBDEV_QUEUE_SIZE_LIMIT;
	dev_info->hardware_accelerated = false;
	dev_info->max_dl_queue_priority = 0;
	dev_info->max_ul_queue_priority = 0;
	dev_info->default_queue_conf = default_queue_conf;
	dev_info->capabilities = bbdev_capabilities;
	dev_info->cpu_flag_reqs = NULL;
	dev_info->min_alignment = 0;

	rte_bbdev_log_debug("got device info from %u", dev->data->dev_id);
}

/* Release queue */
static int
q_release(struct rte_bbdev *dev, uint16_t q_id)
{
	struct bbdev_queue *q = dev->data->queues[q_id].queue_private;

	if (q != NULL) {
		rte_ring_free(q->processed_pkts);
		rte_free(q);
		dev->data->queues[q_id].queue_private = NULL;
	}

	rte_bbdev_log_debug("released device queue %u:%u",
			dev->data->dev_id, q_id);
	return 0;
}

/* Setup a queue */
static int
q_setup(struct rte_bbdev *dev, uint16_t q_id,
		const struct rte_bbdev_queue_conf *queue_conf)
{
	struct bbdev_queue *q;
	char ring_name[RTE_RING_NAMESIZE];
	snprintf(ring_name, RTE_RING_NAMESIZE, RTE_STR(DRIVER_NAME) "%u:%u",
				dev->data->dev_id, q_id);

	/* Allocate the queue data structure. */
	q = rte_zmalloc_socket(RTE_STR(DRIVER_NAME), sizeof(*q),
			RTE_CACHE_LINE_SIZE, queue_conf->socket);
	if (q == NULL) {
		rte_bbdev_log(ERR, "Failed to allocate queue memory");
		return -ENOMEM;
	}

	q->processed_pkts = rte_ring_create(ring_name, queue_conf->queue_size,
			queue_conf->socket, RING_F_SP_ENQ | RING_F_SC_DEQ);
	if (q->processed_pkts == NULL) {
		rte_bbdev_log(ERR, "Failed to create ring");
		goto free_q;
	}

	dev->data->queues[q_id].queue_private = q;
	rte_bbdev_log_debug("setup device queue %s", ring_name);
	return 0;

free_q:
	rte_free(q);
	return -EFAULT;
}

static const struct rte_bbdev_ops pmd_ops = {
	.info_get = info_get,
	.queue_setup = q_setup,
	.queue_release = q_release
};

/* Enqueue decode burst */
static uint16_t
enqueue_dec_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
	struct bbdev_queue *q = q_data->queue_private;
	uint16_t nb_enqueued = rte_ring_enqueue_burst(q->processed_pkts,
			(void **)ops, nb_ops, NULL);

	q_data->queue_stats.enqueue_err_count += nb_ops - nb_enqueued;
	q_data->queue_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/* Enqueue encode burst */
static uint16_t
enqueue_enc_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
	struct bbdev_queue *q = q_data->queue_private;
	uint16_t nb_enqueued = rte_ring_enqueue_burst(q->processed_pkts,
			(void **)ops, nb_ops, NULL);

	q_data->queue_stats.enqueue_err_count += nb_ops - nb_enqueued;
	q_data->queue_stats.enqueued_count += nb_enqueued;

	return nb_enqueued;
}

/* Dequeue decode burst */
static uint16_t
dequeue_dec_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_dec_op **ops, uint16_t nb_ops)
{
	struct bbdev_queue *q = q_data->queue_private;
	uint16_t nb_dequeued = rte_ring_dequeue_burst(q->processed_pkts,
			(void **)ops, nb_ops, NULL);
	q_data->queue_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/* Dequeue encode burst */
static uint16_t
dequeue_enc_ops(struct rte_bbdev_queue_data *q_data,
		struct rte_bbdev_enc_op **ops, uint16_t nb_ops)
{
	struct bbdev_queue *q = q_data->queue_private;
	uint16_t nb_dequeued = rte_ring_dequeue_burst(q->processed_pkts,
			(void **)ops, nb_ops, NULL);
	q_data->queue_stats.dequeued_count += nb_dequeued;

	return nb_dequeued;
}

/* Parse 16bit integer from string argument */
static inline int
parse_u16_arg(const char *key, const char *value, void *extra_args)
{
	uint16_t *u16 = extra_args;
	unsigned int long result;

	if ((value == NULL) || (extra_args == NULL))
		return -EINVAL;
	errno = 0;
	result = strtoul(value, NULL, 0);
	if ((result >= (1 << 16)) || (errno != 0)) {
		rte_bbdev_log(ERR, "Invalid value %lu for %s", result, key);
		return -ERANGE;
	}
	*u16 = (uint16_t)result;
	return 0;
}

/* Parse parameters used to create device */
static int
parse_bbdev_null_params(struct bbdev_null_params *params,
		const char *input_args)
{
	struct rte_kvargs *kvlist = NULL;
	int ret = 0;

	if (params == NULL)
		return -EINVAL;
	if (input_args) {
		kvlist = rte_kvargs_parse(input_args, bbdev_null_valid_params);
		if (kvlist == NULL)
			return -EFAULT;

		ret = rte_kvargs_process(kvlist, bbdev_null_valid_params[0],
					&parse_u16_arg, &params->queues_num);
		if (ret < 0)
			goto exit;

		ret = rte_kvargs_process(kvlist, bbdev_null_valid_params[1],
					&parse_u16_arg, &params->socket_id);
		if (ret < 0)
			goto exit;

		if (params->socket_id >= RTE_MAX_NUMA_NODES) {
			rte_bbdev_log(ERR, "Invalid socket, must be < %u",
					RTE_MAX_NUMA_NODES);
			goto exit;
		}
	}

exit:
	if (kvlist)
		rte_kvargs_free(kvlist);
	return ret;
}

/* Create device */
static int
null_bbdev_create(struct rte_vdev_device *vdev,
		struct bbdev_null_params *init_params)
{
	struct rte_bbdev *bbdev;
	const char *name = rte_vdev_device_name(vdev);

	bbdev = rte_bbdev_allocate(name);
	if (bbdev == NULL)
		return -ENODEV;

	bbdev->data->dev_private = rte_zmalloc_socket(name,
			sizeof(struct bbdev_private), RTE_CACHE_LINE_SIZE,
			init_params->socket_id);
	if (bbdev->data->dev_private == NULL) {
		rte_bbdev_release(bbdev);
		return -ENOMEM;
	}

	bbdev->dev_ops = &pmd_ops;
	bbdev->device = &vdev->device;
	bbdev->data->socket_id = init_params->socket_id;
	bbdev->intr_handle = NULL;

	/* register rx/tx burst functions for data path */
	bbdev->dequeue_enc_ops = dequeue_enc_ops;
	bbdev->dequeue_dec_ops = dequeue_dec_ops;
	bbdev->enqueue_enc_ops = enqueue_enc_ops;
	bbdev->enqueue_dec_ops = enqueue_dec_ops;
	((struct bbdev_private *) bbdev->data->dev_private)->max_nb_queues =
			init_params->queues_num;

	return 0;
}

/* Initialise device */
static int
null_bbdev_probe(struct rte_vdev_device *vdev)
{
	struct bbdev_null_params init_params = {
		rte_socket_id(),
		RTE_BBDEV_DEFAULT_MAX_NB_QUEUES
	};
	const char *name;
	const char *input_args;

	if (vdev == NULL)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	input_args = rte_vdev_device_args(vdev);
	parse_bbdev_null_params(&init_params, input_args);

	rte_bbdev_log_debug("Init %s on NUMA node %d with max queues: %d",
			name, init_params.socket_id, init_params.queues_num);

	return null_bbdev_create(vdev, &init_params);
}

/* Uninitialise device */
static int
null_bbdev_remove(struct rte_vdev_device *vdev)
{
	struct rte_bbdev *bbdev;
	const char *name;

	if (vdev == NULL)
		return -EINVAL;

	name = rte_vdev_device_name(vdev);
	if (name == NULL)
		return -EINVAL;

	bbdev = rte_bbdev_get_named_dev(name);
	if (bbdev == NULL)
		return -EINVAL;

	rte_free(bbdev->data->dev_private);

	return rte_bbdev_release(bbdev);
}

static struct rte_vdev_driver bbdev_null_pmd_drv = {
	.probe = null_bbdev_probe,
	.remove = null_bbdev_remove
};

RTE_PMD_REGISTER_VDEV(DRIVER_NAME, bbdev_null_pmd_drv);
RTE_PMD_REGISTER_PARAM_STRING(DRIVER_NAME,
	BBDEV_NULL_MAX_NB_QUEUES_ARG"=<int> "
	BBDEV_NULL_SOCKET_ID_ARG"=<int>");
RTE_PMD_REGISTER_ALIAS(DRIVER_NAME, bbdev_null);
