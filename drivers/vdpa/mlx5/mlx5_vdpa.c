/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2019 Mellanox Technologies, Ltd
 */
#include <linux/virtio_net.h>

#include <rte_malloc.h>
#include <rte_log.h>
#include <rte_errno.h>
#include <rte_bus_pci.h>

#include <mlx5_glue.h>
#include <mlx5_common.h>
#include <mlx5_devx_cmds.h>
#include <mlx5_prm.h>

#include "mlx5_vdpa_utils.h"
#include "mlx5_vdpa.h"


#ifndef VIRTIO_F_ORDER_PLATFORM
#define VIRTIO_F_ORDER_PLATFORM 36
#endif

#ifndef VIRTIO_F_RING_PACKED
#define VIRTIO_F_RING_PACKED 34
#endif

#define MLX5_VDPA_DEFAULT_FEATURES ((1ULL << VHOST_USER_F_PROTOCOL_FEATURES) | \
			    (1ULL << VIRTIO_F_ANY_LAYOUT) | \
			    (1ULL << VIRTIO_NET_F_MQ) | \
			    (1ULL << VIRTIO_NET_F_GUEST_ANNOUNCE) | \
			    (1ULL << VIRTIO_F_ORDER_PLATFORM))

#define MLX5_VDPA_PROTOCOL_FEATURES \
			    ((1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD) | \
			     (1ULL << VHOST_USER_PROTOCOL_F_MQ))

TAILQ_HEAD(mlx5_vdpa_privs, mlx5_vdpa_priv) priv_list =
					      TAILQ_HEAD_INITIALIZER(priv_list);
static pthread_mutex_t priv_list_lock = PTHREAD_MUTEX_INITIALIZER;
int mlx5_vdpa_logtype;

static struct mlx5_vdpa_priv *
mlx5_vdpa_find_priv_resource_by_did(int did)
{
	struct mlx5_vdpa_priv *priv;
	int found = 0;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &priv_list, next) {
		if (did == priv->id) {
			found = 1;
			break;
		}
	}
	pthread_mutex_unlock(&priv_list_lock);
	if (!found) {
		DRV_LOG(ERR, "Invalid device id: %d.", did);
		rte_errno = EINVAL;
		return NULL;
	}
	return priv;
}

static int
mlx5_vdpa_get_queue_num(int did, uint32_t *queue_num)
{
	struct mlx5_vdpa_priv *priv = mlx5_vdpa_find_priv_resource_by_did(did);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d.", did);
		return -1;
	}
	*queue_num = priv->caps.max_num_virtio_queues;
	return 0;
}

static int
mlx5_vdpa_get_vdpa_features(int did, uint64_t *features)
{
	struct mlx5_vdpa_priv *priv = mlx5_vdpa_find_priv_resource_by_did(did);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d.", did);
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
mlx5_vdpa_get_protocol_features(int did, uint64_t *features)
{
	struct mlx5_vdpa_priv *priv = mlx5_vdpa_find_priv_resource_by_did(did);

	if (priv == NULL) {
		DRV_LOG(ERR, "Invalid device id: %d.", did);
		return -1;
	}
	*features = MLX5_VDPA_PROTOCOL_FEATURES;
	return 0;
}

static struct rte_vdpa_dev_ops mlx5_vdpa_ops = {
	.get_queue_num = mlx5_vdpa_get_queue_num,
	.get_features = mlx5_vdpa_get_vdpa_features,
	.get_protocol_features = mlx5_vdpa_get_protocol_features,
	.dev_conf = NULL,
	.dev_close = NULL,
	.set_vring_state = NULL,
	.set_features = NULL,
	.migration_done = NULL,
	.get_vfio_group_fd = NULL,
	.get_vfio_device_fd = NULL,
	.get_notify_area = NULL,
};

/**
 * DPDK callback to register a PCI device.
 *
 * This function spawns vdpa device out of a given PCI device.
 *
 * @param[in] pci_drv
 *   PCI driver structure (mlx5_vpda_driver).
 * @param[in] pci_dev
 *   PCI device information.
 *
 * @return
 *   0 on success, 1 to skip this driver, a negative errno value otherwise
 *   and rte_errno is set.
 */
static int
mlx5_vdpa_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		    struct rte_pci_device *pci_dev __rte_unused)
{
	struct ibv_device **ibv_list;
	struct ibv_device *ibv_match = NULL;
	struct mlx5_vdpa_priv *priv = NULL;
	struct ibv_context *ctx = NULL;
	struct mlx5_hca_attr attr;
	int ret;

	if (mlx5_class_get(pci_dev->device.devargs) != MLX5_CLASS_VDPA) {
		DRV_LOG(DEBUG, "Skip probing - should be probed by other mlx5"
			" driver.");
		return 1;
	}
	errno = 0;
	ibv_list = mlx5_glue->get_device_list(&ret);
	if (!ibv_list) {
		rte_errno = ENOSYS;
		DRV_LOG(ERR, "Failed to get device list, is ib_uverbs loaded?");
		return -rte_errno;
	}
	while (ret-- > 0) {
		struct rte_pci_addr pci_addr;

		DRV_LOG(DEBUG, "Checking device \"%s\"..", ibv_list[ret]->name);
		if (mlx5_dev_to_pci_addr(ibv_list[ret]->ibdev_path, &pci_addr))
			continue;
		if (pci_dev->addr.domain != pci_addr.domain ||
		    pci_dev->addr.bus != pci_addr.bus ||
		    pci_dev->addr.devid != pci_addr.devid ||
		    pci_dev->addr.function != pci_addr.function)
			continue;
		DRV_LOG(INFO, "PCI information matches for device \"%s\".",
			ibv_list[ret]->name);
		ibv_match = ibv_list[ret];
		break;
	}
	mlx5_glue->free_device_list(ibv_list);
	if (!ibv_match) {
		DRV_LOG(ERR, "No matching IB device for PCI slot "
			"%" SCNx32 ":%" SCNx8 ":%" SCNx8 ".%" SCNx8 ".",
			pci_dev->addr.domain, pci_dev->addr.bus,
			pci_dev->addr.devid, pci_dev->addr.function);
		rte_errno = ENOENT;
		return -rte_errno;
	}
	ctx = mlx5_glue->dv_open_device(ibv_match);
	if (!ctx) {
		DRV_LOG(ERR, "Failed to open IB device \"%s\".",
			ibv_match->name);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	priv = rte_zmalloc("mlx5 vDPA device private", sizeof(*priv),
			   RTE_CACHE_LINE_SIZE);
	if (!priv) {
		DRV_LOG(ERR, "Failed to allocate private memory.");
		rte_errno = ENOMEM;
		goto error;
	}
	ret = mlx5_devx_cmd_query_hca_attr(ctx, &attr);
	if (ret) {
		DRV_LOG(ERR, "Unable to read HCA capabilities.");
		rte_errno = ENOTSUP;
		goto error;
	} else {
		if (!attr.vdpa.valid || !attr.vdpa.max_num_virtio_queues) {
			DRV_LOG(ERR, "Not enough capabilities to support vdpa,"
				" maybe old FW/OFED version?");
			rte_errno = ENOTSUP;
			goto error;
		}
		priv->caps = attr.vdpa;
	}
	priv->ctx = ctx;
	priv->dev_addr.pci_addr = pci_dev->addr;
	priv->dev_addr.type = PCI_ADDR;
	priv->id = rte_vdpa_register_device(&priv->dev_addr, &mlx5_vdpa_ops);
	if (priv->id < 0) {
		DRV_LOG(ERR, "Failed to register vDPA device.");
		rte_errno = rte_errno ? rte_errno : EINVAL;
		goto error;
	}
	SLIST_INIT(&priv->mr_list);
	SLIST_INIT(&priv->virtq_list);
	pthread_mutex_lock(&priv_list_lock);
	TAILQ_INSERT_TAIL(&priv_list, priv, next);
	pthread_mutex_unlock(&priv_list_lock);
	return 0;

error:
	if (priv)
		rte_free(priv);
	if (ctx)
		mlx5_glue->close_device(ctx);
	return -rte_errno;
}

/**
 * DPDK callback to remove a PCI device.
 *
 * This function removes all vDPA devices belong to a given PCI device.
 *
 * @param[in] pci_dev
 *   Pointer to the PCI device.
 *
 * @return
 *   0 on success, the function cannot fail.
 */
static int
mlx5_vdpa_pci_remove(struct rte_pci_device *pci_dev)
{
	struct mlx5_vdpa_priv *priv = NULL;
	int found = 0;

	pthread_mutex_lock(&priv_list_lock);
	TAILQ_FOREACH(priv, &priv_list, next) {
		if (memcmp(&priv->dev_addr.pci_addr, &pci_dev->addr,
			   sizeof(pci_dev->addr)) == 0) {
			found = 1;
			break;
		}
	}
	if (found) {
		TAILQ_REMOVE(&priv_list, priv, next);
		mlx5_glue->close_device(priv->ctx);
		rte_free(priv);
	}
	pthread_mutex_unlock(&priv_list_lock);
	return 0;
}

static const struct rte_pci_id mlx5_vdpa_pci_id_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5BF)
	},
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_MELLANOX,
			       PCI_DEVICE_ID_MELLANOX_CONNECTX5BFVF)
	},
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
				PCI_DEVICE_ID_MELLANOX_CONNECTX6DXVF)
	},
	{
		.vendor_id = 0
	}
};

static struct rte_pci_driver mlx5_vdpa_driver = {
	.driver = {
		.name = "mlx5_vdpa",
	},
	.id_table = mlx5_vdpa_pci_id_map,
	.probe = mlx5_vdpa_pci_probe,
	.remove = mlx5_vdpa_pci_remove,
	.drv_flags = 0,
};

/**
 * Driver initialization routine.
 */
RTE_INIT(rte_mlx5_vdpa_init)
{
	/* Initialize common log type. */
	mlx5_vdpa_logtype = rte_log_register("pmd.vdpa.mlx5");
	if (mlx5_vdpa_logtype >= 0)
		rte_log_set_level(mlx5_vdpa_logtype, RTE_LOG_NOTICE);
	if (mlx5_glue)
		rte_pci_register(&mlx5_vdpa_driver);
}

RTE_PMD_EXPORT_NAME(net_mlx5_vdpa, __COUNTER__);
RTE_PMD_REGISTER_PCI_TABLE(net_mlx5_vdpa, mlx5_vdpa_pci_id_map);
RTE_PMD_REGISTER_KMOD_DEP(net_mlx5_vdpa, "* ib_uverbs & mlx5_core & mlx5_ib");
