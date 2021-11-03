/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020-2021 Xilinx, Inc.
 */

#include <rte_malloc.h>
#include <rte_vdpa.h>
#include <rte_vhost.h>

#include <vdpa_driver.h>

#include "sfc_vdpa_ops.h"
#include "sfc_vdpa.h"

/* Dummy functions for mandatory vDPA ops to pass vDPA device registration.
 * In subsequent patches these ops would be implemented.
 */
static int
sfc_vdpa_get_queue_num(struct rte_vdpa_device *vdpa_dev, uint32_t *queue_num)
{
	RTE_SET_USED(vdpa_dev);
	RTE_SET_USED(queue_num);

	return -1;
}

static int
sfc_vdpa_get_features(struct rte_vdpa_device *vdpa_dev, uint64_t *features)
{
	RTE_SET_USED(vdpa_dev);
	RTE_SET_USED(features);

	return -1;
}

static int
sfc_vdpa_get_protocol_features(struct rte_vdpa_device *vdpa_dev,
			       uint64_t *features)
{
	RTE_SET_USED(vdpa_dev);
	RTE_SET_USED(features);

	return -1;
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

	ops_data->state = SFC_VDPA_STATE_INITIALIZED;

	return ops_data;

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
