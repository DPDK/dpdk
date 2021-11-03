/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <rte_errno.h>
#include <rte_malloc.h>
#include <rte_vdpa.h>
#include <rte_vhost.h>

#include <vdpa_driver.h>

#include "efx.h"
#include "sfc_vdpa_ops.h"
#include "sfc_vdpa.h"

/* These protocol features are needed to enable notifier ctrl */
#define SFC_VDPA_PROTOCOL_FEATURES \
		((1ULL << VHOST_USER_PROTOCOL_F_REPLY_ACK) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_REQ) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_SLAVE_SEND_FD) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_HOST_NOTIFIER) | \
		 (1ULL << VHOST_USER_PROTOCOL_F_LOG_SHMFD))

/*
 * Set of features which are enabled by default.
 * Protocol feature bit is needed to enable notification notifier ctrl.
 */
#define SFC_VDPA_DEFAULT_FEATURES \
		(1ULL << VHOST_USER_F_PROTOCOL_FEATURES)

static int
sfc_vdpa_get_queue_num(struct rte_vdpa_device *vdpa_dev, uint32_t *queue_num)
{
	RTE_SET_USED(vdpa_dev);
	RTE_SET_USED(queue_num);

	return -1;
}

static int
sfc_vdpa_get_device_features(struct sfc_vdpa_ops_data *ops_data)
{
	int rc;
	uint64_t dev_features;
	efx_nic_t *nic;

	nic = sfc_vdpa_adapter_by_dev_handle(ops_data->dev_handle)->nic;

	rc = efx_virtio_get_features(nic, EFX_VIRTIO_DEVICE_TYPE_NET,
				     &dev_features);
	if (rc != 0) {
		sfc_vdpa_err(ops_data->dev_handle,
			     "could not read device feature: %s",
			     rte_strerror(rc));
		return rc;
	}

	ops_data->dev_features = dev_features;

	sfc_vdpa_info(ops_data->dev_handle,
		      "device supported virtio features : 0x%" PRIx64,
		      ops_data->dev_features);

	return 0;
}

static int
sfc_vdpa_get_features(struct rte_vdpa_device *vdpa_dev, uint64_t *features)
{
	struct sfc_vdpa_ops_data *ops_data;

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	*features = ops_data->drv_features;

	sfc_vdpa_info(ops_data->dev_handle,
		      "vDPA ops get_feature :: features : 0x%" PRIx64,
		      *features);

	return 0;
}

static int
sfc_vdpa_get_protocol_features(struct rte_vdpa_device *vdpa_dev,
			       uint64_t *features)
{
	struct sfc_vdpa_ops_data *ops_data;

	ops_data = sfc_vdpa_get_data_by_dev(vdpa_dev);
	if (ops_data == NULL)
		return -1;

	*features = SFC_VDPA_PROTOCOL_FEATURES;

	sfc_vdpa_info(ops_data->dev_handle,
		      "vDPA ops get_protocol_feature :: features : 0x%" PRIx64,
		      *features);

	return 0;
}

static int
sfc_vdpa_dev_config(int vid)
{
	RTE_SET_USED(vid);

	return -1;
}

static int
sfc_vdpa_dev_close(int vid)
{
	RTE_SET_USED(vid);

	return -1;
}

static int
sfc_vdpa_set_vring_state(int vid, int vring, int state)
{
	RTE_SET_USED(vid);
	RTE_SET_USED(vring);
	RTE_SET_USED(state);

	return -1;
}

static int
sfc_vdpa_set_features(int vid)
{
	RTE_SET_USED(vid);

	return -1;
}

static struct rte_vdpa_dev_ops sfc_vdpa_ops = {
	.get_queue_num = sfc_vdpa_get_queue_num,
	.get_features = sfc_vdpa_get_features,
	.get_protocol_features = sfc_vdpa_get_protocol_features,
	.dev_conf = sfc_vdpa_dev_config,
	.dev_close = sfc_vdpa_dev_close,
	.set_vring_state = sfc_vdpa_set_vring_state,
	.set_features = sfc_vdpa_set_features,
};

struct sfc_vdpa_ops_data *
sfc_vdpa_device_init(void *dev_handle, enum sfc_vdpa_context context)
{
	struct sfc_vdpa_ops_data *ops_data;
	struct rte_pci_device *pci_dev;
	int rc;

	/* Create vDPA ops context */
	ops_data = rte_zmalloc("vdpa", sizeof(struct sfc_vdpa_ops_data), 0);
	if (ops_data == NULL)
		return NULL;

	ops_data->vdpa_context = context;
	ops_data->dev_handle = dev_handle;

	pci_dev = sfc_vdpa_adapter_by_dev_handle(dev_handle)->pdev;

	/* Register vDPA Device */
	sfc_vdpa_log_init(dev_handle, "register vDPA device");
	ops_data->vdpa_dev =
		rte_vdpa_register_device(&pci_dev->device, &sfc_vdpa_ops);
	if (ops_data->vdpa_dev == NULL) {
		sfc_vdpa_err(dev_handle, "vDPA device registration failed");
		goto fail_register_device;
	}

	/* Read supported device features */
	sfc_vdpa_log_init(dev_handle, "get device feature");
	rc = sfc_vdpa_get_device_features(ops_data);
	if (rc != 0)
		goto fail_get_dev_feature;

	/* Driver features are superset of device supported feature
	 * and any additional features supported by the driver.
	 */
	ops_data->drv_features =
		ops_data->dev_features | SFC_VDPA_DEFAULT_FEATURES;

	ops_data->state = SFC_VDPA_STATE_INITIALIZED;

	return ops_data;

fail_get_dev_feature:
	rte_vdpa_unregister_device(ops_data->vdpa_dev);

fail_register_device:
	rte_free(ops_data);
	return NULL;
}

void
sfc_vdpa_device_fini(struct sfc_vdpa_ops_data *ops_data)
{
	rte_vdpa_unregister_device(ops_data->vdpa_dev);

	rte_free(ops_data);
}
