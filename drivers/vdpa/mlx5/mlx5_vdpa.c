/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <unistd.h>
#include <net/if.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <netinet/in.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_string_fns.h>
#include <rte_bus_pci.h>

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
			    ((1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_MQ) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_NET_MTU) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_STATUS))

#define MLX5_VDPA_DEFAULT_NO_TRAFFIC_MAX 16LLU

TAILQ_HEAD(mlx5_vdpa_privs, mlx5_vdpa_priv) priv_list =
					      TAILQ_HEAD_INITIALIZER(priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;

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
	int ret;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -EINVAL;
	}
	if (vring >= (int)priv->caps.max_num_virtio_queues) {
		DRV_LOG(ERR, "Too big vring id: %d.", vring);
		return -E2BIG;
	}
	pthread_mutex_lock(&priv->vq_config_lock);
	ret = mlx5_vdpa_virtq_enable(priv, vring, state);
	pthread_mutex_unlock(&priv->vq_config_lock);
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

static int
mlx5_vdpa_dev_close(int vid)
{
	struct rte_vdpa_device *vdev = rte_vhost_get_vdpa_device(vid);
	struct mlx5_vdpa_priv *priv =
		mlx5_vdpa_find_priv_resource_by_vdev(vdev);
	int ret = 0;

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid vDPA device: %s.", vdev->device->name);
		return -1;
	}
	mlx5_vdpa_err_event_unset(priv);
	mlx5_vdpa_cqe_event_unset(priv);
	if (priv->configured)
		ret |= mlx5_vdpa_lm_log(priv);
	mlx5_vdpa_steer_unset(priv);
	mlx5_vdpa_virtqs_release(priv);
	mlx5_vdpa_event_qp_global_release(priv);
	mlx5_vdpa_mem_dereg(priv);
	priv->configured = 0;
	priv->vid = 0;
	/* The mutex may stay locked after event thread cancel - initiate it. */
	pthread_mutex_init(&priv->vq_config_lock, NULL);
	DRV_LOG(INFO, "vDPA device %d was closed.", vid);
	return ret;
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
	if (priv->configured && mlx5_vdpa_dev_close(vid)) {
		DRV_LOG(ERR, "Failed to reconfigure vid %d.", vid);
		return -1;
	}
	priv->vid = vid;
	if (mlx5_vdpa_mtu_set(priv))
		DRV_LOG(WARNING, "MTU cannot be set on device %s.",
				vdev->device->name);
	if (mlx5_vdpa_mem_register(priv) || mlx5_vdpa_err_event_setup(priv) ||
	    mlx5_vdpa_virtqs_prepare(priv) || mlx5_vdpa_steer_setup(priv) ||
	    mlx5_vdpa_cqe_event_setup(priv)) {
		mlx5_vdpa_dev_close(vid);
		return -1;
	}
	priv->configured = 1;
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
	if (!priv->configured) {
		DRV_LOG(ERR, "Device %s was not configured.",
				vdev->device->name);
		return -ENODATA;
	}
	if (qid >= (int)priv->nr_virtqs) {
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
	if (!priv->configured) {
		DRV_LOG(ERR, "Device %s was not configured.",
				vdev->device->name);
		return -ENODATA;
	}
	if (qid >= (int)priv->nr_virtqs) {
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

static struct rte_vdpa_dev_ops mlx5_vdpa_ops = {
	.get_queue_num = mlx5_vdpa_get_queue_num,
	.get_features = mlx5_vdpa_get_vdpa_features,
	.get_protocol_features = mlx5_vdpa_get_protocol_features,
	.dev_conf = mlx5_vdpa_dev_config,
	.dev_close = mlx5_vdpa_dev_close,
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

	if (strcmp(key, RTE_DEVARGS_KEY_CLASS) == 0)
		return 0;
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
	} else if (strcmp(key, "hw_latency_mode") == 0) {
		priv->hw_latency_mode = (uint32_t)tmp;
	} else if (strcmp(key, "hw_max_latency_us") == 0) {
		priv->hw_max_latency_us = (uint32_t)tmp;
	} else if (strcmp(key, "hw_max_pending_comp") == 0) {
		priv->hw_max_pending_comp = (uint32_t)tmp;
	} else {
		DRV_LOG(WARNING, "Invalid key %s.", key);
	}
	return 0;
}

static void
mlx5_vdpa_config_get(struct rte_devargs *devargs, struct mlx5_vdpa_priv *priv)
{
	struct rte_kvargs *kvlist;

	priv->event_mode = MLX5_VDPA_EVENT_MODE_FIXED_TIMER;
	priv->event_us = 0;
	priv->event_core = -1;
	priv->no_traffic_max = MLX5_VDPA_DEFAULT_NO_TRAFFIC_MAX;
	if (devargs == NULL)
		return;
	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;
	rte_kvargs_process(kvlist, NULL, mlx5_vdpa_args_check_handler, priv);
	rte_kvargs_free(kvlist);
	if (!priv->event_us &&
	    priv->event_mode == MLX5_VDPA_EVENT_MODE_DYNAMIC_TIMER)
		priv->event_us = MLX5_VDPA_DEFAULT_TIMER_STEP_US;
	DRV_LOG(DEBUG, "event mode is %d.", priv->event_mode);
	DRV_LOG(DEBUG, "event_us is %u us.", priv->event_us);
	DRV_LOG(DEBUG, "no traffic max is %u.", priv->no_traffic_max);
}

static int
mlx5_vdpa_dev_probe(struct mlx5_common_device *cdev)
{
	struct mlx5_vdpa_priv *priv = NULL;
	struct mlx5_hca_attr *attr = &cdev->config.hca_attr;
	int retry;

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
	priv->cdev = cdev;
	for (retry = 0; retry < 7; retry++) {
		priv->var = mlx5_glue->dv_alloc_var(priv->cdev->ctx, 0);
		if (priv->var != NULL)
			break;
		DRV_LOG(WARNING, "Failed to allocate VAR, retry %d.\n", retry);
		/* Wait Qemu release VAR during vdpa restart, 0.1 sec based. */
		usleep(100000U << retry);
	}
	if (!priv->var) {
		DRV_LOG(ERR, "Failed to allocate VAR %u.", errno);
		goto error;
	}
	priv->err_intr_handle =
		rte_intr_instance_alloc(RTE_INTR_INSTANCE_F_SHARED);
	if (priv->err_intr_handle == NULL) {
		DRV_LOG(ERR, "Fail to allocate intr_handle");
		goto error;
	}
	priv->vdev = rte_vdpa_register_device(cdev->dev, &mlx5_vdpa_ops);
	if (priv->vdev == NULL) {
		DRV_LOG(ERR, "Failed to register vDPA device.");
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}
	mlx5_vdpa_config_get(cdev->dev->devargs, priv);
	SLIST_INIT(&priv->mr_list);
	pthread_mutex_init(&priv->vq_config_lock, NULL);
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	return 0;

error:
	if (priv) {
		if (priv->var)
			mlx5_glue->dv_free_var(priv->var);
		rte_intr_instance_free(priv->err_intr_handle);
		rte_free(priv);
	}
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
	if (found) {
		if (priv->configured)
			mlx5_vdpa_dev_close(priv->vid);
		if (priv->var) {
			mlx5_glue->dv_free_var(priv->var);
			priv->var = NULL;
		}
		if (priv->vdev)
			rte_vdpa_unregister_device(priv->vdev);
		pthread_mutex_destroy(&priv->vq_config_lock);
		rte_intr_instance_free(priv->err_intr_handle);
		rte_free(priv);
	}
	return 0;
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
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DXBF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX7)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
				PCI_DEVICE_ID_MELLANOX_CONNECTX7BF)
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
