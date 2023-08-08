/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <bus_pci_driver.h>
#include <rte_eal_paging.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_common_defs.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>
#include <mlx5_nl.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"

#define MLX5_VDPA_DRIVER_NAME vdpa_mlx5

#define MLX5_VDPA_DEFAULT_FEATURES ((1ULL << VHOST_USER_F_PROTOCOL_FEATURES) | \
			    (1ULL << VIRTIO_F_ANY_LAYOUT) | \
			    (1ULL << VIRTIO_NET_F_MQ) | \
			    (1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE) | \
			    (1ULL << VIRTIO_F_ORDER_PLATFORM) | \
			    (1ULL << VHOST_F_LOG_ALL) | \
			    (1ULL << VIRTIO_NET_F_MTU))

#define MLX5_VDPA_PROTOCOL_FEATURES \
			    ((1ULL << VHOST_USER_PROTOCOL_F_BACKEND_REQ) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_BACKEND_SEND_FD) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_NET_MTU) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_STATUS))

#define MLX5_VDPA_DEFAULT_NO_TRAFFIC_MAX 16LLU

TAILQ_HEAD(mlx5_vdpa_privs, mlx5_vdpa_priv) priv_list =
					      TAILQ_HEAD_INITIALIZER(priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;

struct mlx5_vdpa_conf_thread_mng conf_thread_mng;

static void mlx5_vdpa_dev_release(struct mlx5_vdpa_priv *priv);

static struct mlx5_vdpa_priv *
mlx5_vdpa_find_priv_resource_by_vdev(struct rte_vdpa_device *vdev)
{
	struct mlx5_vdpa_priv *priv;
	int found = 0;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &priv_list, next) {
		if (vdev == priv->vdev) {
			found = 1;
			break;
		}
	}
	pthread_mutex_unlock(&priv_list_lock);
	if (!found) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		rte_errno = EINVAL;
		return NULL;
	}
	return priv;
}

static int
mlx5_vdpa_get_queue_num(struct rte_vdpa_device *vdev, uint32_t *queue_num)
{
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -1;
	}
	*queue_num = priv->caps.max_num_virtio_queues / 2;
	return 0;
}

static int
mlx5_vdpa_get_vdpa_features(struct rte_vdpa_device *vdev, uint64_t *features)
{
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -1;
	}
	*features = MLX5_VDPA_DEFAULT_FEATURES;
	if (priv->caps.virtio_queue_type & (1 << MLX5_VIRTQ_TYPE_PACKED))
		*features |= (1ULL << VIRTIO_F_RING_PACKED);
	if (priv->caps.tso_ipv4)
		*features |= (1ULL << VIRTIO_NET_F_HOST_TSO4);
	if (priv->caps.tso_ipv6)
		*features |= (1ULL << VIRTIO_NET_F_HOST_TSO6);
	if (priv->caps.tx_csum)
		*features |= (1ULL << VIRTIO_NET_F_CSUM);
	if (priv->caps.rx_csum)
		*features |= (1ULL << VIRTIO_NET_F_GUEST_CSUM);
	if (priv->caps.virtio_version_1_0)
		*features |= (1ULL << VIRTIO_F_VERSION_1);
	return 0;
}

static int
mlx5_vdpa_get_protocol_features(struct rte_vdpa_device *vdev,
		uint64_t *features)
{
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -1;
	}
	*features = MLX5_VDPA_PROTOCOL_FEATURES;
	return 0;
}

static int
mlx5_vdpa_set_vring_state(int vid, int vring, int state)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);
	struct mlx5_vdpa_virtq *virtq;
	int ret;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -EINVAL;
	}
	if (vring >= (int)priv->caps.max_num_virtio_queues) {
		DRV_LOG(ERR, "Too big vring id: %d.", vring);
		return -E2BIG;
	}
	virtq = &priv->virtqs[vring];
	pthread_mutex_lock(&virtq->virtq_lock);
	ret = mlx5_vdpa_virtq_enable(priv, vring, state);
	pthread_mutex_unlock(&virtq->virtq_lock);
	return ret;
}

static int
mlx5_vdpa_features_set(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);
	uint64_t log_base, log_size;
	uint64_t features;
	int ret;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -EINVAL;
	}
	ret = rte_vhost_get_negotiated_features(vid, &features);
	if (ret) {
		DRV_LOG(ERR, "Failed to get negotiated features.");
		return ret;
	}
	if (RTE_VHOST_NEED_LOG(features)) {
		ret = rte_vhost_get_log_base(vid, &log_base, &log_size);
		if (ret) {
			DRV_LOG(ERR, "Failed to get log base.");
			return ret;
		}
		ret = mlx5_vdpa_dirty_bitmap_set(priv, log_base, log_size);
		if (ret) {
			DRV_LOG(ERR, "Failed to set dirty bitmap.");
			return ret;
		}
		DRV_LOG(INFO, "mlx5 vdpa: enabling dirty logging...");
		ret = mlx5_vdpa_logging_enable(priv, 1);
		if (ret) {
			DRV_LOG(ERR, "Failed t enable dirty logging.");
			return ret;
		}
	}
	return 0;
}

static int
mlx5_vdpa_mtu_set(struct mlx5_vdpa_priv *priv)
{
	struct ifreq request;
	uint16_t vhost_mtu = 0;
	uint16_t kern_mtu = 0;
	int ret = rte_vhost_get_mtu(priv->vid, &vhost_mtu);
	int sock;
	int retries = MLX5_VDPA_MAX_RETRIES;

	if (ret) {
		DRV_LOG(DEBUG, "Cannot get vhost MTU - %d.", ret);
		return ret;
	}
	if (!vhost_mtu) {
		DRV_LOG(DEBUG, "Vhost MTU is 0.");
		return ret;
	}
	ret = mlx5_get_ifname_sysfs
				(mlx5_os_get_ctx_device_name(priv->cdev->ctx),
				 request.ifr_name);
	if (ret) {
		DRV_LOG(DEBUG, "Cannot get kernel IF name - %d.", ret);
		return ret;
	}
	sock = socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
	if (sock == -1) {
		DRV_LOG(DEBUG, "Cannot open IF socket.");
		return sock;
	}
	while (retries--) {
		ret = ioctl(sock, SIOCGIFMTU, &request);
		if (ret == -1)
			break;
		kern_mtu = request.ifr_mtu;
		DRV_LOG(DEBUG, "MTU: current %d requested %d.", (int)kern_mtu,
			(int)vhost_mtu);
		if (kern_mtu == vhost_mtu)
			break;
		request.ifr_mtu = vhost_mtu;
		ret = ioctl(sock, SIOCSIFMTU, &request);
		if (ret == -1)
			break;
		request.ifr_mtu = 0;
		usleep(MLX5_VDPA_USEC);
	}
	close(sock);
	return kern_mtu == vhost_mtu ? 0 : -1;
}

void
mlx5_vdpa_dev_cache_clean(struct mlx5_vdpa_priv *priv)
{
	/* Clean pre-created resource in dev removal only. */
	if (!priv->queues)
		mlx5_vdpa_virtqs_cleanup(priv);
	mlx5_vdpa_mem_dereg(priv);
}

static bool
mlx5_vdpa_wait_dev_close_tasks_done(struct mlx5_vdpa_priv *priv)
{
	uint32_t timeout = 0;

	/* Check and wait all close tasks done. */
	while (__atomic_load_n(&priv->dev_close_progress,
		__ATOMIC_RELAXED) != 0 && timeout < 1000) {
		rte_delay_us_sleep(10000);
		timeout++;
	}
	if (priv->dev_close_progress) {
		DRV_LOG(ERR,
		"Failed to wait close device tasks done vid %d.",
		priv->vid);
		return true;
	}
	return false;
}

static int
_internal_mlx5_vdpa_dev_close(struct mlx5_vdpa_priv *priv,
		bool release_resource)
{
	int ret = 0;
	int vid = priv->vid;

	mlx5_vdpa_virtq_unreg_intr_handle_all(priv);
	mlx5_vdpa_cqe_event_unset(priv);
	if (priv->state == MLX5_VDPA_STATE_CONFIGURED) {
		ret |= mlx5_vdpa_lm_log(priv);
		priv->state = MLX5_VDPA_STATE_IN_PROGRESS;
	}
	if (priv->use_c_thread && !release_resource) {
		if (priv->last_c_thrd_idx >=
			(conf_thread_mng.max_thrds - 1))
			priv->last_c_thrd_idx = 0;
		else
			priv->last_c_thrd_idx++;
		__atomic_store_n(&priv->dev_close_progress,
			1, __ATOMIC_RELAXED);
		if (mlx5_vdpa_task_add(priv,
			priv->last_c_thrd_idx,
			MLX5_VDPA_TASK_DEV_CLOSE_NOWAIT,
			NULL, NULL, NULL, 1)) {
			DRV_LOG(ERR,
			"Fail to add dev close task. ");
			goto single_thrd;
		}
		priv->state = MLX5_VDPA_STATE_PROBED;
		DRV_LOG(INFO, "vDPA device %d was closed.", vid);
		return ret;
	}
single_thrd:
	pthread_mutex_lock(&priv->steer_update_lock);
	mlx5_vdpa_steer_unset(priv);
	pthread_mutex_unlock(&priv->steer_update_lock);
	mlx5_vdpa_virtqs_release(priv, release_resource);
	mlx5_vdpa_drain_cq(priv);
	if (priv->lm_mr.addr)
		mlx5_os_wrapped_mkey_destroy(&priv->lm_mr);
	if (!priv->connected)
		mlx5_vdpa_dev_cache_clean(priv);
	priv->vid = 0;
	__atomic_store_n(&priv->dev_close_progress, 0,
		__ATOMIC_RELAXED);
	priv->state = MLX5_VDPA_STATE_PROBED;
	DRV_LOG(INFO, "vDPA device %d was closed.", vid);
	return ret;
}

static int
mlx5_vdpa_dev_close(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv;

	if (!vdev) {
		DRV_LOG(ERR, "Invalid vDPA device.");
		return -1;
	}
	priv = mlx5_vdpa_find_priv_resource_by_vdev(vdev);
	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -1;
	}
	return _internal_mlx5_vdpa_dev_close(priv, false);
}

static int
mlx5_vdpa_dev_config(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -EINVAL;
	}
	if (priv->state == MLX5_VDPA_STATE_CONFIGURED &&
	    mlx5_vdpa_dev_close(vid)) {
		DRV_LOG(ERR, "Failed to reconfigure vid %d.", vid);
		return -1;
	}
	if (mlx5_vdpa_wait_dev_close_tasks_done(priv))
		return -1;
	priv->vid = vid;
	priv->connected = true;
	if (mlx5_vdpa_mtu_set(priv))
		DRV_LOG(WARNING, "MTU cannot be set on device %s.",
				vdev->device->name);
	if (mlx5_vdpa_mem_register(priv) ||
	    mlx5_vdpa_virtqs_prepare(priv) || mlx5_vdpa_steer_setup(priv) ||
	    mlx5_vdpa_cqe_event_setup(priv)) {
		mlx5_vdpa_dev_close(vid);
		return -1;
	}
	priv->state = MLX5_VDPA_STATE_CONFIGURED;
	DRV_LOG(INFO, "vDPA device %d was configured.", vid);
	return 0;
}

static int
mlx5_vdpa_get_device_fd(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -EINVAL;
	}
	return ((struct ibv_context *)priv->cdev->ctx)->cmd_fd;
}

static int
mlx5_vdpa_get_notify_area(int vid, int qid, uint64_t *offset, uint64_t *size)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	RTE_SET_USED(qid);
	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -EINVAL;
	}
	if (!priv->var) {
		DRV_LOG(ERR, "VAR was not created for device %s, is the device"
			" configured?.", vdev->device->name);
		return -EINVAL;
	}
	*offset = priv->var->mmap_off;
	*size = priv->var->length;
	return 0;
}

static int
mlx5_vdpa_get_stats_names(struct rte_vdpa_device *vdev,
		struct rte_vdpa_stat_name *stats_names,
		unsigned int size)
{
	static const char *mlx5_vdpa_stats_names[MLX5_VDPA_STATS_MAX] = {
		"received_descriptors",
		"completed_descriptors",
		"bad descriptor errors",
		"exceed max chain",
		"invalid buffer",
		"completion errors",
	};
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);
	unsigned int i;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid device: %s.", vdev->device->name);
		return -ENODEV;
	}
	if (!stats_names)
		return MLX5_VDPA_STATS_MAX;
	size = RTE_MIN(size, (unsigned int)MLX5_VDPA_STATS_MAX);
	for (i = 0; i < size; ++i)
		strlcpy(stats_names[i].name, mlx5_vdpa_stats_names[i],
			RTE_VDPA_STATS_NAME_SIZE);
	return size;
}

static int
mlx5_vdpa_get_stats(struct rte_vdpa_device *vdev, int qid,
		struct rte_vdpa_stat *stats, unsigned int n)
{
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid device: %s.", vdev->device->name);
		return -ENODEV;
	}
	if (qid >= (int)priv->caps.max_num_virtio_queues) {
		DRV_LOG(ERR, "Too big vring id: %d for device %s.", qid,
				vdev->device->name);
		return -E2BIG;
	}
	if (!priv->caps.queue_counters_valid) {
		DRV_LOG(ERR, "Virtq statistics is not supported for device %s.",
			vdev->device->name);
		return -ENOTSUP;
	}
	return mlx5_vdpa_virtq_stats_get(priv, qid, stats, n);
}

static int
mlx5_vdpa_reset_stats(struct rte_vdpa_device *vdev, int qid)
{
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid device: %s.", vdev->device->name);
		return -ENODEV;
	}
	if (qid >= (int)priv->caps.max_num_virtio_queues) {
		DRV_LOG(ERR, "Too big vring id: %d for device %s.", qid,
				vdev->device->name);
		return -E2BIG;
	}
	if (!priv->caps.queue_counters_valid) {
		DRV_LOG(ERR, "Virtq statistics is not supported for device %s.",
			vdev->device->name);
		return -ENOTSUP;
	}
	return mlx5_vdpa_virtq_stats_reset(priv, qid);
}

static int
mlx5_vdpa_dev_cleanup(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv;

	if (vdev == NULL)
		return -1;
	priv = mlx5_vdpa_find_priv_resource_by_vdev(vdev);
	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -1;
	}
	if (priv->state == MLX5_VDPA_STATE_PROBED) {
		if (priv->use_c_thread)
			mlx5_vdpa_wait_dev_close_tasks_done(priv);
		mlx5_vdpa_dev_cache_clean(priv);
	}
	priv->connected = false;
	return 0;
}

static struct rte_vdpa_dev_ops mlx5_vdpa_ops = {
	.get_queue_num = mlx5_vdpa_get_queue_num,
	.get_features = mlx5_vdpa_get_vdpa_features,
	.get_protocol_features = mlx5_vdpa_get_protocol_features,
	.dev_conf = mlx5_vdpa_dev_config,
	.dev_close = mlx5_vdpa_dev_close,
	.dev_cleanup = mlx5_vdpa_dev_cleanup,
	.set_vring_state = mlx5_vdpa_set_vring_state,
	.set_features = mlx5_vdpa_features_set,
	.migration_done = NULL,
	.get_vfio_group_fd = NULL,
	.get_vfio_device_fd = mlx5_vdpa_get_device_fd,
	.get_notify_area = mlx5_vdpa_get_notify_area,
	.get_stats_names = mlx5_vdpa_get_stats_names,
	.get_stats = mlx5_vdpa_get_stats,
	.reset_stats = mlx5_vdpa_reset_stats,
};

static int
mlx5_vdpa_args_check_handler(const char *key, const char *val, void *opaque)
{
	struct mlx5_vdpa_priv *priv = opaque;
	unsigned long tmp;
	int n_cores = sysconf(_SC_NPROCESSORS_ONLN);

	errno = 0;
	tmp = strtoul(val, NULL, 0);
	if (errno) {
		DRV_LOG(WARNING, "%s: \"%s\" is an invalid integer.", key, val);
		return -errno;
	}
	if (strcmp(key, "event_mode") == 0) {
		if (tmp <= MLX5_VDPA_EVENT_MODE_ONLY_INTERRUPT)
			priv->event_mode = (int)tmp;
		else
			DRV_LOG(WARNING, "Invalid event_mode %s.", val);
	} else if (strcmp(key, "event_us") == 0) {
		priv->event_us = (uint32_t)tmp;
	} else if (strcmp(key, "no_traffic_time") == 0) {
		priv->no_traffic_max = (uint32_t)tmp;
	} else if (strcmp(key, "event_core") == 0) {
		if (tmp >= (unsigned long)n_cores)
			DRV_LOG(WARNING, "Invalid event_core %s.", val);
		else
			priv->event_core = tmp;
	} else if (strcmp(key, "max_conf_threads") == 0) {
		if (tmp) {
			priv->use_c_thread = true;
			if (!conf_thread_mng.initializer_priv) {
				conf_thread_mng.initializer_priv = priv;
				if (tmp > MLX5_VDPA_MAX_C_THRD) {
					DRV_LOG(WARNING,
				"Invalid max_conf_threads %s "
				"and set max_conf_threads to %d",
				val, MLX5_VDPA_MAX_C_THRD);
					tmp = MLX5_VDPA_MAX_C_THRD;
				}
				conf_thread_mng.max_thrds = tmp;
			} else if (tmp != conf_thread_mng.max_thrds) {
				DRV_LOG(WARNING,
	"max_conf_threads is PMD argument and not per device, "
	"only the first device configuration set it, current value is %d "
	"and will not be changed to %d.",
				conf_thread_mng.max_thrds, (int)tmp);
			}
		} else {
			priv->use_c_thread = false;
		}
	} else if (strcmp(key, "hw_latency_mode") == 0) {
		priv->hw_latency_mode = (uint32_t)tmp;
	} else if (strcmp(key, "hw_max_latency_us") == 0) {
		priv->hw_max_latency_us = (uint32_t)tmp;
	} else if (strcmp(key, "hw_max_pending_comp") == 0) {
		priv->hw_max_pending_comp = (uint32_t)tmp;
	} else if (strcmp(key, "queue_size") == 0) {
		priv->queue_size = (uint16_t)tmp;
	} else if (strcmp(key, "queues") == 0) {
		priv->queues = (uint16_t)tmp;
	} else {
		DRV_LOG(WARNING, "Invalid key %s.", key);
	}
	return 0;
}

static void
mlx5_vdpa_config_get(struct mlx5_kvargs_ctrl *mkvlist,
		     struct mlx5_vdpa_priv *priv)
{
	const char **params = (const char *[]){
		"event_core",
		"event_mode",
		"event_us",
		"hw_latency_mode",
		"hw_max_latency_us",
		"hw_max_pending_comp",
		"no_traffic_time",
		"queue_size",
		"queues",
		"max_conf_threads",
		NULL,
	};

	priv->event_mode = MLX5_VDPA_EVENT_MODE_FIXED_TIMER;
	priv->event_us = 0;
	priv->event_core = -1;
	priv->no_traffic_max = MLX5_VDPA_DEFAULT_NO_TRAFFIC_MAX;
	if (mkvlist == NULL)
		return;
	mlx5_kvargs_process(mkvlist, params, mlx5_vdpa_args_check_handler,
			    priv);
	if (!priv->event_us &&
	    priv->event_mode == MLX5_VDPA_EVENT_MODE_DYNAMIC_TIMER)
		priv->event_us = MLX5_VDPA_DEFAULT_TIMER_STEP_US;
	if ((priv->queue_size && !priv->queues) ||
		(!priv->queue_size && priv->queues)) {
		priv->queue_size = 0;
		priv->queues = 0;
		DRV_LOG(WARNING, "Please provide both queue_size and queues.");
	}
	DRV_LOG(DEBUG, "event mode is %d.", priv->event_mode);
	DRV_LOG(DEBUG, "event_us is %u us.", priv->event_us);
	DRV_LOG(DEBUG, "no traffic max is %u.", priv->no_traffic_max);
	DRV_LOG(DEBUG, "queues is %u, queue_size is %u.", priv->queues,
		priv->queue_size);
}

void
mlx5_vdpa_prepare_virtq_destroy(struct mlx5_vdpa_priv *priv)
{
	uint32_t max_queues, index;
	struct mlx5_vdpa_virtq *virtq;

	if (!priv->queues || !priv->queue_size)
		return;
	max_queues = ((priv->queues * 2) < priv->caps.max_num_virtio_queues) ?
		(priv->queues * 2) : (priv->caps.max_num_virtio_queues);
	if (mlx5_vdpa_is_modify_virtq_supported(priv))
		mlx5_vdpa_steer_unset(priv);
	for (index = 0; index < max_queues; ++index) {
		virtq = &priv->virtqs[index];
		if (virtq->virtq) {
			pthread_mutex_lock(&virtq->virtq_lock);
			mlx5_vdpa_virtq_unset(virtq);
			pthread_mutex_unlock(&virtq->virtq_lock);
		}
	}
}

static int
mlx5_vdpa_virtq_resource_prepare(struct mlx5_vdpa_priv *priv)
{
	uint32_t remaining_cnt = 0, err_cnt = 0, task_num = 0;
	uint32_t max_queues, index, thrd_idx, data[1];
	struct mlx5_vdpa_virtq *virtq;

	for (index = 0; index < priv->caps.max_num_virtio_queues;
		index++) {
		virtq = &priv->virtqs[index];
		pthread_mutex_init(&virtq->virtq_lock, NULL);
	}
	if (!priv->queues || !priv->queue_size)
		return 0;
	max_queues = (priv->queues < priv->caps.max_num_virtio_queues) ?
		(priv->queues * 2) : (priv->caps.max_num_virtio_queues);
	if (priv->use_c_thread) {
		uint32_t main_task_idx[max_queues];

		for (index = 0; index < max_queues; ++index) {
			thrd_idx = index % (conf_thread_mng.max_thrds + 1);
			if (!thrd_idx) {
				main_task_idx[task_num] = index;
				task_num++;
				continue;
			}
			thrd_idx = priv->last_c_thrd_idx + 1;
			if (thrd_idx >= conf_thread_mng.max_thrds)
				thrd_idx = 0;
			priv->last_c_thrd_idx = thrd_idx;
			data[0] = index;
			if (mlx5_vdpa_task_add(priv, thrd_idx,
				MLX5_VDPA_TASK_PREPARE_VIRTQ,
				&remaining_cnt, &err_cnt,
				(void **)&data, 1)) {
				DRV_LOG(ERR, "Fail to add "
				"task prepare virtq (%d).", index);
				main_task_idx[task_num] = index;
				task_num++;
			}
		}
		for (index = 0; index < task_num; ++index)
			if (mlx5_vdpa_virtq_single_resource_prepare(priv,
				main_task_idx[index]))
				goto error;
		if (mlx5_vdpa_c_thread_wait_bulk_tasks_done(&remaining_cnt,
			&err_cnt, 2000)) {
			DRV_LOG(ERR,
			"Failed to wait virt-queue prepare tasks ready.");
			goto error;
		}
	} else {
		for (index = 0; index < max_queues; ++index)
			if (mlx5_vdpa_virtq_single_resource_prepare(priv,
				index))
				goto error;
	}
	if (mlx5_vdpa_is_modify_virtq_supported(priv))
		if (mlx5_vdpa_steer_update(priv, true))
			goto error;
	return 0;
error:
	mlx5_vdpa_prepare_virtq_destroy(priv);
	return -1;
}

static int
mlx5_vdpa_create_dev_resources(struct mlx5_vdpa_priv *priv)
{
	struct mlx5_devx_tis_attr tis_attr = {0};
	struct ibv_context *ctx = priv->cdev->ctx;
	uint32_t i;
	int retry;

	for (retry = 0; retry < 7; retry++) {
		priv->var = mlx5_glue->dv_alloc_var(ctx, 0);
		if (priv->var != NULL)
			break;
		DRV_LOG(WARNING, "Failed to allocate VAR, retry %d.", retry);
		/* Wait Qemu release VAR during vdpa restart, 0.1 sec based. */
		usleep(100000U << retry);
	}
	if (!priv->var) {
		DRV_LOG(ERR, "Failed to allocate VAR %u.", errno);
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	/* Always map the entire page. */
	priv->virtq_db_addr = mmap(NULL, priv->var->length, PROT_READ |
				   PROT_WRITE, MAP_SHARED, ctx->cmd_fd,
				   priv->var->mmap_off);
	if (priv->virtq_db_addr == MAP_FAILED) {
		DRV_LOG(ERR, "Failed to map doorbell page %u.", errno);
		priv->virtq_db_addr = NULL;
		rte_errno = errno;
		return -rte_errno;
	}
	/* Add within page offset for 64K page system. */
	priv->virtq_db_addr = (char *)priv->virtq_db_addr +
		((rte_mem_page_size() - 1) & priv->caps.doorbell_bar_offset);
	DRV_LOG(DEBUG, "VAR address of doorbell mapping is %p.",
		priv->virtq_db_addr);
	priv->td = mlx5_devx_cmd_create_td(ctx);
	if (!priv->td) {
		DRV_LOG(ERR, "Failed to create transport domain.");
		rte_errno = errno;
		return -rte_errno;
	}
	tis_attr.transport_domain = priv->td->id;
	for (i = 0; i < priv->num_lag_ports; i++) {
		/* 0 is auto affinity, non-zero value to propose port. */
		tis_attr.lag_tx_port_affinity = i + 1;
		priv->tiss[i] = mlx5_devx_cmd_create_tis(ctx, &tis_attr);
		if (!priv->tiss[i]) {
			DRV_LOG(ERR, "Failed to create TIS %u.", i);
			return -rte_errno;
		}
	}
	priv->null_mr = mlx5_glue->alloc_null_mr(priv->cdev->pd);
	if (!priv->null_mr) {
		DRV_LOG(ERR, "Failed to allocate null MR.");
		rte_errno = errno;
		return -rte_errno;
	}
	DRV_LOG(DEBUG, "Dump fill Mkey = %u.", priv->null_mr->lkey);
#ifdef HAVE_MLX5DV_DR
	priv->steer.domain = mlx5_glue->dr_create_domain(ctx,
					MLX5DV_DR_DOMAIN_TYPE_NIC_RX);
	if (!priv->steer.domain) {
		DRV_LOG(ERR, "Failed to create Rx domain.");
		rte_errno = errno;
		return -rte_errno;
	}
#endif
	priv->steer.tbl = mlx5_glue->dr_create_flow_tbl(priv->steer.domain, 0);
	if (!priv->steer.tbl) {
		DRV_LOG(ERR, "Failed to create table 0 with Rx domain.");
		rte_errno = errno;
		return -rte_errno;
	}
	if (mlx5_vdpa_err_event_setup(priv) != 0)
		return -rte_errno;
	if (mlx5_vdpa_event_qp_global_prepare(priv))
		return -rte_errno;
	if (mlx5_vdpa_virtq_resource_prepare(priv))
		return -rte_errno;
	return 0;
}

static int
mlx5_vdpa_dev_probe(struct mlx5_common_device *cdev,
		    struct mlx5_kvargs_ctrl *mkvlist)
{
	struct mlx5_vdpa_priv *priv = NULL;
	struct mlx5_hca_attr *attr = &cdev->config.hca_attr;

	if (!attr->vdpa.valid || !attr->vdpa.max_num_virtio_queues) {
		DRV_LOG(ERR, "Not enough capabilities to support vdpa, maybe "
			"old FW/OFED version?");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	if (!attr->vdpa.queue_counters_valid)
		DRV_LOG(DEBUG, "No capability to support virtq statistics.");
	priv = rte_zmalloc("mlx5 vDPA device private", sizeof(*priv) +
			   sizeof(struct mlx5_vdpa_virtq) *
			   attr->vdpa.max_num_virtio_queues,
			   RTE_CACHE_LINE_SIZE);
	if (!priv) {
		DRV_LOG(ERR, "Failed to allocate private memory.");
		rte_errno = ENOMEM;
		return -rte_errno;
	}
	priv->caps = attr->vdpa;
	priv->log_max_rqt_size = attr->log_max_rqt_size;
	priv->num_lag_ports = attr->num_lag_ports;
	if (attr->num_lag_ports == 0)
		priv->num_lag_ports = 1;
	rte_spinlock_init(&priv->db_lock);
	pthread_mutex_init(&priv->steer_update_lock, NULL);
	priv->cdev = cdev;
	mlx5_vdpa_config_get(mkvlist, priv);
	if (priv->use_c_thread) {
		if (conf_thread_mng.initializer_priv == priv)
			if (mlx5_vdpa_mult_threads_create())
				goto error;
		__atomic_fetch_add(&conf_thread_mng.refcnt, 1,
			__ATOMIC_RELAXED);
	}
	if (mlx5_vdpa_create_dev_resources(priv))
		goto error;
	priv->vdev = rte_vdpa_register_device(cdev->dev, &mlx5_vdpa_ops);
	if (priv->vdev == NULL) {
		DRV_LOG(ERR, "Failed to register vDPA device.");
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	return 0;
error:
	if (conf_thread_mng.initializer_priv == priv)
		mlx5_vdpa_mult_threads_destroy(false);
	if (priv)
		mlx5_vdpa_dev_release(priv);
	return -rte_errno;
}

static int
mlx5_vdpa_dev_remove(struct mlx5_common_device *cdev)
{
	struct mlx5_vdpa_priv *priv = NULL;
	int found = 0;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &priv_list, next) {
		if (priv->vdev->device == cdev->dev) {
			found = 1;
			break;
		}
	}
	if (found)
		TAILQ_REMOVE(&priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	if (found)
		mlx5_vdpa_dev_release(priv);
	return 0;
}

static void
mlx5_vdpa_release_dev_resources(struct mlx5_vdpa_priv *priv)
{
	uint32_t i;

	if (priv->queues)
		mlx5_vdpa_virtqs_cleanup(priv);
	mlx5_vdpa_dev_cache_clean(priv);
	for (i = 0; i < priv->caps.max_num_virtio_queues; i++) {
		if (!priv->virtqs[i].counters)
			continue;
		claim_zero(mlx5_devx_cmd_destroy(priv->virtqs[i].counters));
	}
	mlx5_vdpa_event_qp_global_release(priv);
	mlx5_vdpa_err_event_unset(priv);
	if (priv->steer.tbl)
		claim_zero(mlx5_glue->dr_destroy_flow_tbl(priv->steer.tbl));
	if (priv->steer.domain)
		claim_zero(mlx5_glue->dr_destroy_domain(priv->steer.domain));
	if (priv->null_mr)
		claim_zero(mlx5_glue->dereg_mr(priv->null_mr));
	for (i = 0; i < priv->num_lag_ports; i++) {
		if (priv->tiss[i])
			claim_zero(mlx5_devx_cmd_destroy(priv->tiss[i]));
	}
	if (priv->td)
		claim_zero(mlx5_devx_cmd_destroy(priv->td));
	if (priv->virtq_db_addr)
		/* Mask out the within page offset for munmap. */
		claim_zero(munmap((void *)((uintptr_t)priv->virtq_db_addr &
			~(rte_mem_page_size() - 1)), priv->var->length));
	if (priv->var)
		mlx5_glue->dv_free_var(priv->var);
}

static void
mlx5_vdpa_dev_release(struct mlx5_vdpa_priv *priv)
{
	if (priv->state == MLX5_VDPA_STATE_CONFIGURED)
		_internal_mlx5_vdpa_dev_close(priv, true);
	if (priv->use_c_thread)
		mlx5_vdpa_wait_dev_close_tasks_done(priv);
	mlx5_vdpa_release_dev_resources(priv);
	if (priv->vdev)
		rte_vdpa_unregister_device(priv->vdev);
	if (priv->use_c_thread)
		if (__atomic_fetch_sub(&conf_thread_mng.refcnt,
			1, __ATOMIC_RELAXED) == 1)
			mlx5_vdpa_mult_threads_destroy(true);
	rte_free(priv);
}

static const struct rte_pci_id mlx5_vdpa_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6VF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTXVF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_BLUEFIELD2)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX6LX)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX7)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_BLUEFIELD3)
	},
	{
		.vendor_id = 0
	}
};

static struct mlx5_class_driver mlx5_vdpa_driver = {
	.drv_class = MLX5_CLASS_VDPA,
	.name = RTE_STR(MLX5_VDPA_DRIVER_NAME),
	.id_table = mlx5_vdpa_pci_id_map,
	.probe = mlx5_vdpa_dev_probe,
	.remove = mlx5_vdpa_dev_remove,
};

RTE_LOG_REGISTER_DEFAULT(mlx5_vdpa_logtype, NOTICE)

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx5_vdpa_init)
{
	mlx5_common_init();
	if (mlx5_glue)
		mlx5_class_driver_register(&mlx5_vdpa_driver);
}

RTE_PMD_EXPORT_NAME(MLX5_VDPA_DRIVER_NAME, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(MLX5_VDPA_DRIVER_NAME, mlx5_vdpa_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(MLX5_VDPA_DRIVER_NAME, "* ib_uverbs & mlx5_core & mlx5_ib");
