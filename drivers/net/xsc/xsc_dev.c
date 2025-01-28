/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Yunsilicon Technology Co., Ltd.
 */

#include <stdlib.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <bus_pci_driver.h>
#include <rte_kvargs.h>
#include <rte_eal_paging.h>
#include <rte_bitops.h>

#include "xsc_log.h"
#include "xsc_defs.h"
#include "xsc_dev.h"

#define XSC_DEV_DEF_FLOW_MODE	7

TAILQ_HEAD(xsc_dev_ops_list, xsc_dev_ops);
static struct xsc_dev_ops_list dev_ops_list = TAILQ_HEAD_INITIALIZER(dev_ops_list);

static const struct xsc_dev_ops *
xsc_dev_ops_get(enum rte_pci_kernel_driver kdrv)
{
	const struct xsc_dev_ops *ops;

	TAILQ_FOREACH(ops, &dev_ops_list, entry) {
		if (ops->kdrv == kdrv)
			return ops;
	}

	return NULL;
}

void
xsc_dev_ops_register(struct xsc_dev_ops *new_ops)
{
	struct xsc_dev_ops *ops;

	TAILQ_FOREACH(ops, &dev_ops_list, entry) {
		if (ops->kdrv == new_ops->kdrv) {
			PMD_DRV_LOG(ERR, "xsc dev ops exists, kdrv=%d", new_ops->kdrv);
			return;
		}
	}

	TAILQ_INSERT_TAIL(&dev_ops_list, new_ops, entry);
}

int
xsc_dev_mailbox_exec(struct xsc_dev *xdev, void *data_in,
		     int in_len, void *data_out, int out_len)
{
	return xdev->dev_ops->mailbox_exec(xdev, data_in, in_len,
					   data_out, out_len);
}

int
xsc_dev_close(struct xsc_dev *xdev, int repr_id)
{
	xsc_dev_clear_pct(xdev, repr_id);
	return xdev->dev_ops->dev_close(xdev);
}

static int
xsc_dev_alloc_vfos_info(struct xsc_dev *xdev)
{
	struct xsc_hwinfo *hwinfo;
	int base_lp = 0;

	if (xsc_dev_is_vf(xdev))
		return 0;

	hwinfo = &xdev->hwinfo;
	if (hwinfo->pcie_no == 1) {
		xdev->vfrep_offset = hwinfo->func_id -
			hwinfo->pcie1_pf_funcid_base +
			hwinfo->pcie0_pf_funcid_top -
			hwinfo->pcie0_pf_funcid_base  + 1;
	} else {
		xdev->vfrep_offset = hwinfo->func_id - hwinfo->pcie0_pf_funcid_base;
	}

	base_lp = XSC_VFREP_BASE_LOGICAL_PORT;
	if (xdev->devargs.nic_mode == XSC_NIC_MODE_LEGACY)
		base_lp += xdev->vfrep_offset;
	xdev->vfos_logical_in_port = base_lp;
	return 0;
}

static void
xsc_dev_args_parse(struct xsc_dev *xdev, struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	struct xsc_devargs *xdevargs = &xdev->devargs;
	const char *tmp;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (kvlist == NULL)
		return;

	tmp = rte_kvargs_get(kvlist, XSC_PPH_MODE_ARG);
	if (tmp != NULL)
		xdevargs->pph_mode = atoi(tmp);
	else
		xdevargs->pph_mode = XSC_PPH_NONE;

	tmp = rte_kvargs_get(kvlist, XSC_NIC_MODE_ARG);
	if (tmp != NULL)
		xdevargs->nic_mode = atoi(tmp);
	else
		xdevargs->nic_mode = XSC_NIC_MODE_LEGACY;

	tmp = rte_kvargs_get(kvlist, XSC_FLOW_MODE_ARG);
	if (tmp != NULL)
		xdevargs->flow_mode = atoi(tmp);
	else
		xdevargs->flow_mode = XSC_DEV_DEF_FLOW_MODE;

	rte_kvargs_free(kvlist);
}

void
xsc_dev_uninit(struct xsc_dev *xdev)
{
	PMD_INIT_FUNC_TRACE();
	xsc_dev_pct_uninit();
	xsc_dev_close(xdev, XSC_DEV_REPR_ID_INVALID);
	rte_free(xdev);
}

int
xsc_dev_init(struct rte_pci_device *pci_dev, struct xsc_dev **xdev)
{
	struct xsc_dev *d;
	int ret;

	PMD_INIT_FUNC_TRACE();

	d = rte_zmalloc(NULL, sizeof(*d), RTE_CACHE_LINE_SIZE);
	if (d == NULL) {
		PMD_DRV_LOG(ERR, "Failed to alloc memory for xsc_dev");
		return -ENOMEM;
	}

	d->dev_ops = xsc_dev_ops_get(pci_dev->kdrv);
	if (d->dev_ops == NULL) {
		PMD_DRV_LOG(ERR, "Could not get dev_ops, kdrv=%d", pci_dev->kdrv);
		return -ENODEV;
	}

	d->pci_dev = pci_dev;

	if (d->dev_ops->dev_init)
		d->dev_ops->dev_init(d);

	xsc_dev_args_parse(d, pci_dev->device.devargs);

	ret = xsc_dev_alloc_vfos_info(d);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to alloc vfos info");
		ret = -EINVAL;
		goto hwinfo_init_fail;
	}

	ret = xsc_dev_pct_init();
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to init xsc pct");
		ret = -EINVAL;
		goto hwinfo_init_fail;
	}

	*xdev = d;

	return 0;

hwinfo_init_fail:
	xsc_dev_uninit(d);
	return ret;
}

bool
xsc_dev_is_vf(struct xsc_dev *xdev)
{
	uint16_t device_id = xdev->pci_dev->id.device_id;

	if (device_id == XSC_PCI_DEV_ID_MSVF ||
	    device_id == XSC_PCI_DEV_ID_MVHVF)
		return true;

	return false;
}
