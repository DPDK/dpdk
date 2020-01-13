/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */
#include <string.h>
#include <unistd.h>

#include <rte_bus.h>
#include <rte_bus_pci.h>
#include <rte_eal.h>
#include <rte_lcore.h>
#include <rte_mempool.h>
#include <rte_pci.h>

#include <rte_common.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>

#include "otx2_common.h"
#include "otx2_ep_rawdev.h"
#include "otx2_ep_vf.h"

static const struct rte_pci_id pci_sdp_vf_map[] = {
	{
		RTE_PCI_DEVICE(PCI_VENDOR_ID_CAVIUM,
			       PCI_DEVID_OCTEONTX2_EP_VF)
	},
	{
		.vendor_id = 0,
	},
};

/* SDP_VF default configuration */
const struct sdp_config default_sdp_conf = {
	/* IQ attributes */
	.iq                        = {
		.max_iqs           = SDP_VF_CFG_IO_QUEUES,
		.instr_type        = SDP_VF_64BYTE_INSTR,
		.pending_list_size = (SDP_VF_MAX_IQ_DESCRIPTORS *
				      SDP_VF_CFG_IO_QUEUES),
	},

	/* OQ attributes */
	.oq                        = {
		.max_oqs           = SDP_VF_CFG_IO_QUEUES,
		.info_ptr          = SDP_VF_OQ_INFOPTR_MODE,
		.refill_threshold  = SDP_VF_OQ_REFIL_THRESHOLD,
	},

	.num_iqdef_descs           = SDP_VF_MAX_IQ_DESCRIPTORS,
	.num_oqdef_descs           = SDP_VF_MAX_OQ_DESCRIPTORS,
	.oqdef_buf_size            = SDP_VF_OQ_BUF_SIZE,

};

const struct sdp_config*
sdp_get_defconf(struct sdp_device *sdp_dev __rte_unused)
{
	const struct sdp_config *default_conf = NULL;

	default_conf = &default_sdp_conf;

	return default_conf;
}

static int
sdp_vfdev_exit(struct rte_rawdev *rawdev)
{
	struct sdp_device *sdpvf;
	uint32_t rawdev_queues, q;

	otx2_info("%s:", __func__);

	sdpvf = (struct sdp_device *)rawdev->dev_private;

	sdpvf->fn_list.disable_io_queues(sdpvf);

	rawdev_queues = sdpvf->num_oqs;
	for (q = 0; q < rawdev_queues; q++) {
		if (sdp_delete_oqs(sdpvf, q)) {
			otx2_err("Failed to delete OQ:%d", q);
			return -ENOMEM;
		}
	}
	otx2_info("Num OQs:%d freed", sdpvf->num_oqs);

	/* Free the oqbuf_pool */
	rte_mempool_free(sdpvf->enqdeq_mpool);
	sdpvf->enqdeq_mpool = NULL;

	otx2_info("Enqdeq_mpool free done");

	rawdev_queues = sdpvf->num_iqs;
	for (q = 0; q < rawdev_queues; q++) {
		if (sdp_delete_iqs(sdpvf, q)) {
			otx2_err("Failed to delete IQ:%d", q);
			return -ENOMEM;
		}
	}
	otx2_sdp_dbg("Num IQs:%d freed", sdpvf->num_iqs);

	return 0;
}

static int
sdp_chip_specific_setup(struct sdp_device *sdpvf)
{
	struct rte_pci_device *pdev = sdpvf->pci_dev;
	uint32_t dev_id = pdev->id.device_id;
	int ret;

	switch (dev_id) {
	case PCI_DEVID_OCTEONTX2_EP_VF:
		sdpvf->chip_id = PCI_DEVID_OCTEONTX2_EP_VF;
		ret = sdp_vf_setup_device(sdpvf);

		break;
	default:
		otx2_err("Unsupported device");
		ret = -EINVAL;
	}

	if (!ret)
		otx2_info("SDP dev_id[%d]", dev_id);

	return ret;
}

/* SDP VF device initialization */
static int
sdp_vfdev_init(struct sdp_device *sdpvf)
{
	uint32_t rawdev_queues, q;

	if (sdp_chip_specific_setup(sdpvf)) {
		otx2_err("Chip specific setup failed");
		goto setup_fail;
	}

	if (sdpvf->fn_list.setup_device_regs(sdpvf)) {
		otx2_err("Failed to configure device registers");
		goto setup_fail;
	}

	rawdev_queues = (uint32_t)(sdpvf->sriov_info.rings_per_vf);

	/* Rawdev queues setup for enqueue/dequeue */
	for (q = 0; q < rawdev_queues; q++) {
		if (sdp_setup_iqs(sdpvf, q)) {
			otx2_err("Failed to setup IQs");
			goto iq_fail;
		}
	}
	otx2_info("Total[%d] IQs setup", sdpvf->num_iqs);

	for (q = 0; q < rawdev_queues; q++) {
		if (sdp_setup_oqs(sdpvf, q)) {
			otx2_err("Failed to setup OQs");
			goto oq_fail;
		}
	}
	otx2_info("Total [%d] OQs setup", sdpvf->num_oqs);

	/* Enable IQ/OQ for this device */
	sdpvf->fn_list.enable_io_queues(sdpvf);

	/* Send OQ desc credits for OQs, credits are always
	 * sent after the OQs are enabled.
	 */
	for (q = 0; q < rawdev_queues; q++) {
		rte_write32(sdpvf->droq[q]->nb_desc,
			    sdpvf->droq[q]->pkts_credit_reg);

		rte_io_mb();
		otx2_info("OQ[%d] dbells [%d]", q,
		rte_read32(sdpvf->droq[q]->pkts_credit_reg));
	}

	rte_wmb();

	otx2_info("SDP Device is Ready");

	return 0;

/* Error handling  */
oq_fail:
	/* Free the allocated OQs */
	for (q = 0; q < sdpvf->num_oqs; q++)
		sdp_delete_oqs(sdpvf, q);

iq_fail:
	/* Free the allocated IQs */
	for (q = 0; q < sdpvf->num_iqs; q++)
		sdp_delete_iqs(sdpvf, q);

setup_fail:
	return -ENOMEM;
}

static int
sdp_rawdev_start(struct rte_rawdev *dev)
{
	dev->started = 1;

	return 0;
}

static void
sdp_rawdev_stop(struct rte_rawdev *dev)
{
	dev->started = 0;
}

static int
sdp_rawdev_close(struct rte_rawdev *dev)
{
	int ret;
	ret = sdp_vfdev_exit(dev);
	if (ret) {
		otx2_err(" SDP_EP rawdev exit error");
		return ret;
	}

	return 0;
}

static int
sdp_rawdev_configure(const struct rte_rawdev *dev, rte_rawdev_obj_t config)
{
	struct sdp_rawdev_info *app_info = (struct sdp_rawdev_info *)config;
	struct sdp_device *sdpvf;

	if (app_info == NULL) {
		otx2_err("Application config info [NULL]");
		return -EINVAL;
	}

	sdpvf = (struct sdp_device *)dev->dev_private;

	sdpvf->conf = app_info->app_conf;
	sdpvf->enqdeq_mpool = app_info->enqdeq_mpool;

	sdp_vfdev_init(sdpvf);

	return 0;

}

/* SDP VF endpoint rawdev ops */
static const struct rte_rawdev_ops sdp_rawdev_ops = {
	.dev_configure  = sdp_rawdev_configure,
	.dev_start      = sdp_rawdev_start,
	.dev_stop       = sdp_rawdev_stop,
	.dev_close      = sdp_rawdev_close,
	.enqueue_bufs   = sdp_rawdev_enqueue,
	.dequeue_bufs   = sdp_rawdev_dequeue,
	.dev_selftest   = sdp_rawdev_selftest,
};

static int
otx2_sdp_rawdev_probe(struct rte_pci_driver *pci_drv __rte_unused,
		      struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct sdp_device *sdpvf = NULL;
	struct rte_rawdev *sdp_rawdev;
	uint16_t vf_id;

	/* Single process support */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev->mem_resource[0].addr)
		otx2_info("SDP_EP BAR0 is mapped:");
	else {
		otx2_err("SDP_EP: Failed to map device BARs");
		otx2_err("BAR0 %p\n BAR2 %p",
			pci_dev->mem_resource[0].addr,
			pci_dev->mem_resource[2].addr);
		return -ENODEV;
	}

	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "SDPEP:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	/* Allocate rawdev pmd */
	sdp_rawdev = rte_rawdev_pmd_allocate(name,
					     sizeof(struct sdp_device),
					     rte_socket_id());

	if (sdp_rawdev == NULL) {
		otx2_err("SDP_EP VF rawdev allocation failed");
		return -ENOMEM;
	}

	sdp_rawdev->dev_ops = &sdp_rawdev_ops;
	sdp_rawdev->device = &pci_dev->device;
	sdp_rawdev->driver_name = pci_dev->driver->driver.name;

	sdpvf = (struct sdp_device *)sdp_rawdev->dev_private;
	sdpvf->hw_addr = pci_dev->mem_resource[0].addr;
	sdpvf->pci_dev = pci_dev;

	/* Discover the VF number being probed */
	vf_id = ((pci_dev->addr.devid & 0x1F) << 3) |
		 (pci_dev->addr.function & 0x7);

	vf_id -= 1;
	sdpvf->vf_num = vf_id;

	otx2_info("SDP_EP VF[%d] probe done", vf_id);

	return 0;
}

static int
otx2_sdp_rawdev_remove(struct rte_pci_device *pci_dev)
{
	char name[RTE_RAWDEV_NAME_MAX_LEN];
	struct rte_rawdev *rawdev;
	struct sdp_device *sdpvf;

	/* Single process support */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (pci_dev == NULL) {
		otx2_err("SDP_EP:invalid pci_dev!");
		return -EINVAL;
	}


	memset(name, 0, sizeof(name));
	snprintf(name, RTE_RAWDEV_NAME_MAX_LEN, "SDPEP:%x:%02x.%x",
		 pci_dev->addr.bus, pci_dev->addr.devid,
		 pci_dev->addr.function);

	rawdev = rte_rawdev_pmd_get_named_dev(name);
	if (rawdev == NULL) {
		otx2_err("SDP_EP: invalid device name (%s)", name);
		return -EINVAL;
	}

	sdpvf = (struct sdp_device *)rawdev->dev_private;
	otx2_info("Removing SDP_EP VF[%d] ", sdpvf->vf_num);

	/* rte_rawdev_close is called by pmd_release */
	return rte_rawdev_pmd_release(rawdev);
}

static struct rte_pci_driver rte_sdp_rawdev_pmd = {
	.id_table  = pci_sdp_vf_map,
	.drv_flags = (RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_NEED_IOVA_AS_VA),
	.probe     = otx2_sdp_rawdev_probe,
	.remove    = otx2_sdp_rawdev_remove,
};

RTE_PMD_REGISTER_PCI(sdp_rawdev_pci_driver, rte_sdp_rawdev_pmd);
RTE_PMD_REGISTER_PCI_TABLE(sdp_rawdev_pci_driver, pci_sdp_vf_map);
RTE_PMD_REGISTER_KMOD_DEP(sdp_rawdev_pci_driver, "vfio-pci");
