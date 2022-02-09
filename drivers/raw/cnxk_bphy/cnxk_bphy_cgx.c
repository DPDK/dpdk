/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */
#include <string.h>

#include <rte_bus_pci.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include <roc_api.h>

#include "cnxk_bphy_cgx.h"
#include "rte_pmd_bphy.h"

struct cnxk_bphy_cgx_queue {
	unsigned int lmac;
	/* queue holds up to one response */
	void *rsp;
};

struct cnxk_bphy_cgx {
	struct roc_bphy_cgx *rcgx;
	struct cnxk_bphy_cgx_queue queues[MAX_LMACS_PER_CGX];
	unsigned int num_queues;
};

static void
cnxk_bphy_cgx_format_name(char *name, unsigned int len,
			  struct rte_pci_device *pci_dev)
{
	snprintf(name, len, "BPHY_CGX:%02x:%02x.%x", pci_dev->addr.bus,
		 pci_dev->addr.devid, pci_dev->addr.function);
}

static int
cnxk_bphy_cgx_queue_def_conf(struct rte_rawdev *dev, uint16_t queue_id,
			     rte_rawdev_obj_t queue_conf,
			     size_t queue_conf_size)
{
	unsigned int *conf;

	RTE_SET_USED(dev);
	RTE_SET_USED(queue_id);

	if (queue_conf_size != sizeof(*conf))
		return -EINVAL;

	conf = (unsigned int *)queue_conf;
	*conf = 1;

	return 0;
}

static int
cnxk_bphy_cgx_process_buf(struct cnxk_bphy_cgx *cgx, unsigned int queue,
			  struct rte_rawdev_buf *buf)
{
	struct cnxk_bphy_cgx_queue *qp = &cgx->queues[queue];
	struct cnxk_bphy_cgx_msg_set_link_state *link_state;
	struct cnxk_bphy_cgx_msg *msg = buf->buf_addr;
	struct cnxk_bphy_cgx_msg_link_mode *link_mode;
	struct cnxk_bphy_cgx_msg_link_info *link_info;
	struct roc_bphy_cgx_link_info rlink_info;
	struct roc_bphy_cgx_link_mode rlink_mode;
	enum roc_bphy_cgx_eth_link_fec *fec;
	unsigned int lmac = qp->lmac;
	void *rsp = NULL;
	int ret;

	switch (msg->type) {
	case CNXK_BPHY_CGX_MSG_TYPE_GET_LINKINFO:
		memset(&rlink_info, 0, sizeof(rlink_info));
		ret = roc_bphy_cgx_get_linkinfo(cgx->rcgx, lmac, &rlink_info);
		if (ret)
			break;

		link_info = rte_zmalloc(NULL, sizeof(*link_info), 0);
		if (!link_info)
			return -ENOMEM;

		link_info->link_up = rlink_info.link_up;
		link_info->full_duplex = rlink_info.full_duplex;
		link_info->speed =
			(enum cnxk_bphy_cgx_eth_link_speed)rlink_info.speed;
		link_info->autoneg = rlink_info.an;
		link_info->fec =
			(enum cnxk_bphy_cgx_eth_link_fec)rlink_info.fec;
		link_info->mode =
			(enum cnxk_bphy_cgx_eth_link_mode)rlink_info.mode;
		rsp = link_info;
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_INTLBK_DISABLE:
		ret = roc_bphy_cgx_intlbk_disable(cgx->rcgx, lmac);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_INTLBK_ENABLE:
		ret = roc_bphy_cgx_intlbk_enable(cgx->rcgx, lmac);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_DISABLE:
		ret = roc_bphy_cgx_ptp_rx_disable(cgx->rcgx, lmac);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_PTP_RX_ENABLE:
		ret = roc_bphy_cgx_ptp_rx_enable(cgx->rcgx, lmac);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_MODE:
		link_mode = msg->data;
		memset(&rlink_mode, 0, sizeof(rlink_mode));
		rlink_mode.full_duplex = link_mode->full_duplex;
		rlink_mode.an = link_mode->autoneg;
		rlink_mode.speed =
			(enum roc_bphy_cgx_eth_link_speed)link_mode->speed;
		rlink_mode.mode =
			(enum roc_bphy_cgx_eth_link_mode)link_mode->mode;
		ret = roc_bphy_cgx_set_link_mode(cgx->rcgx, lmac, &rlink_mode);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_SET_LINK_STATE:
		link_state = msg->data;
		ret = roc_bphy_cgx_set_link_state(cgx->rcgx, lmac,
						  link_state->state);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_START_RXTX:
		ret = roc_bphy_cgx_start_rxtx(cgx->rcgx, lmac);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_STOP_RXTX:
		ret = roc_bphy_cgx_stop_rxtx(cgx->rcgx, lmac);
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_GET_SUPPORTED_FEC:
		fec = rte_zmalloc(NULL, sizeof(*fec), 0);
		if (!fec)
			return -ENOMEM;

		ret = roc_bphy_cgx_fec_supported_get(cgx->rcgx, lmac, fec);
		rsp = fec;
		break;
	case CNXK_BPHY_CGX_MSG_TYPE_SET_FEC:
		fec = msg->data;
		ret = roc_bphy_cgx_fec_set(cgx->rcgx, lmac, *fec);
		break;
	default:
		return -EINVAL;
	}

	/* get rid of last response if any */
	if (qp->rsp) {
		RTE_LOG(WARNING, PMD, "Previous response got overwritten\n");
		rte_free(qp->rsp);
	}
	qp->rsp = rsp;

	return ret;
}

static int
cnxk_bphy_cgx_enqueue_bufs(struct rte_rawdev *dev,
			   struct rte_rawdev_buf **buffers, unsigned int count,
			   rte_rawdev_obj_t context)
{
	struct cnxk_bphy_cgx *cgx = dev->dev_private;
	unsigned int queue = (size_t)context;
	int ret;

	if (queue >= cgx->num_queues)
		return -EINVAL;

	if (count == 0)
		return 0;

	ret = cnxk_bphy_cgx_process_buf(cgx, queue, buffers[0]);
	if (ret)
		return ret;

	return 1;
}

static int
cnxk_bphy_cgx_dequeue_bufs(struct rte_rawdev *dev,
			   struct rte_rawdev_buf **buffers, unsigned int count,
			   rte_rawdev_obj_t context)
{
	struct cnxk_bphy_cgx *cgx = dev->dev_private;
	unsigned int queue = (size_t)context;
	struct cnxk_bphy_cgx_queue *qp;

	if (queue >= cgx->num_queues)
		return -EINVAL;

	if (count == 0)
		return 0;

	qp = &cgx->queues[queue];
	if (qp->rsp) {
		buffers[0]->buf_addr = qp->rsp;
		qp->rsp = NULL;

		return 1;
	}

	return 0;
}

static uint16_t
cnxk_bphy_cgx_queue_count(struct rte_rawdev *dev)
{
	struct cnxk_bphy_cgx *cgx = dev->dev_private;

	return cgx->num_queues;
}

static const struct rte_rawdev_ops cnxk_bphy_cgx_rawdev_ops = {
	.queue_def_conf = cnxk_bphy_cgx_queue_def_conf,
	.enqueue_bufs = cnxk_bphy_cgx_enqueue_bufs,
	.dequeue_bufs = cnxk_bphy_cgx_dequeue_bufs,
	.queue_count = cnxk_bphy_cgx_queue_count,
	.dev_selftest = cnxk_bphy_cgx_dev_selftest,
};

static void
cnxk_bphy_cgx_init_queues(struct cnxk_bphy_cgx *cgx)
{
	struct roc_bphy_cgx *rcgx = cgx->rcgx;
	unsigned int i;

	for (i = 0; i < RTE_DIM(cgx->queues); i++) {
		if (!(rcgx->lmac_bmap & BIT_ULL(i)))
			continue;

		cgx->queues[cgx->num_queues++].lmac = i;
	}
}

static void
cnxk_bphy_cgx_fini_queues(struct cnxk_bphy_cgx *cgx)
{
	unsigned int i;

	for (i = 0; i < cgx->num_queues; i++) {
		rte_free(cgx->queues[i].rsp);
	}

	cgx->num_queues = 0;
}

static int
cnxk_bphy_cgx_rawdev_probe(struct rte_pci_driver *pci_drv,
			   struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	struct cnxk_bphy_cgx *cgx;
	struct roc_bphy_cgx *rcgx;
	int ret;

	RTE_SET_USED(pci_drv);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!pci_dev->mem_resource[0].addr)
		return -ENODEV;

	ret = roc_plt_init();
	if (ret)
		return ret;

	cnxk_bphy_cgx_format_name(name, sizeof(name), pci_dev);
	rawdev = rte_rawdev_pmd_allocate(name, sizeof(*cgx), rte_socket_id());
	if (!rawdev)
		return -ENOMEM;

	rawdev->dev_ops = &cnxk_bphy_cgx_rawdev_ops;
	rawdev->device = &pci_dev->device;
	rawdev->driver_name = pci_dev->driver->driver.name;

	cgx = rawdev->dev_private;
	cgx->rcgx = rte_zmalloc(NULL, sizeof(*rcgx), 0);
	if (!cgx->rcgx) {
		ret = -ENOMEM;
		goto out_pmd_release;
	}

	rcgx = cgx->rcgx;
	rcgx->bar0_pa = pci_dev->mem_resource[0].phys_addr;
	rcgx->bar0_va = pci_dev->mem_resource[0].addr;
	ret = roc_bphy_cgx_dev_init(rcgx);
	if (ret)
		goto out_free;

	cnxk_bphy_cgx_init_queues(cgx);

	return 0;
out_free:
	rte_free(rcgx);
out_pmd_release:
	rte_rawdev_pmd_release(rawdev);

	return ret;
}

static int
cnxk_bphy_cgx_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	struct cnxk_bphy_cgx *cgx;

	cnxk_bphy_cgx_format_name(name, sizeof(name), pci_dev);
	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (!rawdev)
		return -ENODEV;

	cgx = rawdev->dev_private;
	cnxk_bphy_cgx_fini_queues(cgx);
	roc_bphy_cgx_dev_fini(cgx->rcgx);
	rte_free(cgx->rcgx);

	return rte_rawdev_pmd_release(rawdev);
}

static const struct rte_pci_id cnxk_bphy_cgx_map[] = {
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN9K_CGX)},
	{RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM, PCI_DEVID_CN10K_RPM)},
	{} /* sentinel */
};

static struct rte_pci_driver bphy_cgx_rawdev_pmd = {
	.id_table = cnxk_bphy_cgx_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = cnxk_bphy_cgx_rawdev_probe,
	.remove = cnxk_bphy_cgx_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(cnxk_bphy_cgx_rawdev_pci_driver, bphy_cgx_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(cnxk_bphy_cgx_rawdev_pci_driver, cnxk_bphy_cgx_map);
RTE_PMD_REGISTER_KMOD_DEP(cnxk_bphy_cgx_rawdev_pci_driver, "vfio-pci");
