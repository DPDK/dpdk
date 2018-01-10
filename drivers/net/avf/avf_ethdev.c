/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>

#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>

#include "avf_log.h"
#include "base/avf_prototype.h"
#include "base/avf_adminq_cmd.h"
#include "base/avf_type.h"

#include "avf.h"

int avf_logtype_init;
int avf_logtype_driver;
static const struct rte_pci_id pci_id_avf_map[] = {
	{ RTE_PCI_DEVICE(AVF_INTEL_VENDOR_ID, AVF_DEV_ID_ADAPTIVE_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops avf_eth_dev_ops = {
};

static int
avf_check_vf_reset_done(struct avf_hw *hw)
{
	int i, reset;

	for (i = 0; i < AVF_RESET_WAIT_CNT; i++) {
		reset = AVF_READ_REG(hw, AVFGEN_RSTAT) &
			AVFGEN_RSTAT_VFR_STATE_MASK;
		reset = reset >> AVFGEN_RSTAT_VFR_STATE_SHIFT;
		if (reset == VIRTCHNL_VFR_VFACTIVE ||
		    reset == VIRTCHNL_VFR_COMPLETED)
			break;
		rte_delay_ms(20);
	}

	if (i >= AVF_RESET_WAIT_CNT)
		return -1;

	return 0;
}

static int
avf_init_vf(struct rte_eth_dev *dev)
{
	int i, err, bufsz;
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	err = avf_set_mac_type(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "set_mac_type failed: %d", err);
		goto err;
	}

	err = avf_check_vf_reset_done(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "VF is still resetting");
		goto err;
	}

	avf_init_adminq_parameter(hw);
	err = avf_init_adminq(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "init_adminq failed: %d", err);
		goto err;
	}

	vf->aq_resp = rte_zmalloc("vf_aq_resp", AVF_AQ_BUF_SZ, 0);
	if (!vf->aq_resp) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_aq_resp memory");
		goto err_aq;
	}
	if (avf_check_api_version(adapter) != 0) {
		PMD_INIT_LOG(ERR, "check_api version failed");
		goto err_api;
	}

	bufsz = sizeof(struct virtchnl_vf_resource) +
		(AVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource));
	vf->vf_res = rte_zmalloc("vf_res", bufsz, 0);
	if (!vf->vf_res) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_res memory");
		goto err_api;
	}
	if (avf_get_vf_resource(adapter) != 0) {
		PMD_INIT_LOG(ERR, "avf_get_vf_config failed");
		goto err_alloc;
	}
	/* Allocate memort for RSS info */
	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		vf->rss_key = rte_zmalloc("rss_key",
					  vf->vf_res->rss_key_size, 0);
		if (!vf->rss_key) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_key memory");
			goto err_rss;
		}
		vf->rss_lut = rte_zmalloc("rss_lut",
					  vf->vf_res->rss_lut_size, 0);
		if (!vf->rss_lut) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_lut memory");
			goto err_rss;
		}
	}
	return 0;
err_rss:
	rte_free(vf->rss_key);
	rte_free(vf->rss_lut);
err_alloc:
	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
err_api:
	rte_free(vf->aq_resp);
err_aq:
	avf_shutdown_adminq(hw);
err:
	return -1;
}

/* Enable default admin queue interrupt setting */
static inline void
avf_enable_irq0(struct avf_hw *hw)
{
	/* Enable admin queue interrupt trigger */
	AVF_WRITE_REG(hw, AVFINT_ICR0_ENA1, AVFINT_ICR0_ENA1_ADMINQ_MASK);

	AVF_WRITE_REG(hw, AVFINT_DYN_CTL01, AVFINT_DYN_CTL01_INTENA_MASK |
					    AVFINT_DYN_CTL01_ITR_INDX_MASK);

	AVF_WRITE_FLUSH(hw);
}

static inline void
avf_disable_irq0(struct avf_hw *hw)
{
	/* Disable all interrupt types */
	AVF_WRITE_REG(hw, AVFINT_ICR0_ENA1, 0);
	AVF_WRITE_REG(hw, AVFINT_DYN_CTL01,
		      AVFINT_DYN_CTL01_ITR_INDX_MASK);
	AVF_WRITE_FLUSH(hw);
}

static void
avf_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	avf_disable_irq0(hw);

	avf_handle_virtchnl_msg(dev);

done:
	avf_enable_irq0(hw);
}

static int
avf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_INIT_FUNC_TRACE();

	/* assign ops func pointer */
	eth_dev->dev_ops = &avf_eth_dev_ops;

	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.bus_id = pci_dev->addr.bus;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->back = AVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	adapter->eth_dev = eth_dev;

	if (avf_init_vf(eth_dev) != 0) {
		PMD_INIT_LOG(ERR, "Init vf failed");
		return -1;
	}

	/* copy mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc(
					"avf_mac",
					ETHER_ADDR_LEN * AVF_NUM_MACADDR_MAX,
					0);
	if (!eth_dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes needed to"
			     " store MAC addresses",
			     ETHER_ADDR_LEN * AVF_NUM_MACADDR_MAX);
		return -ENOMEM;
	}
	/* If the MAC address is not configured by host,
	 * generate a random one.
	 */
	if (!is_valid_assigned_ether_addr((struct ether_addr *)hw->mac.addr))
		eth_random_addr(hw->mac.addr);
	ether_addr_copy((struct ether_addr *)hw->mac.addr,
			&eth_dev->data->mac_addrs[0]);

	/* register callback func to eal lib */
	rte_intr_callback_register(&pci_dev->intr_handle,
				   avf_dev_interrupt_handler,
				   (void *)eth_dev);

	/* enable uio intr after callback register */
	rte_intr_enable(&pci_dev->intr_handle);

	/* configure and enable device interrupt */
	avf_enable_irq0(hw);

	return 0;
}

static void
avf_dev_close(struct rte_eth_dev *dev)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	avf_shutdown_adminq(hw);
	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* unregister callback func from eal lib */
	rte_intr_callback_unregister(intr_handle,
				     avf_dev_interrupt_handler, dev);
	avf_disable_irq0(hw);
}

static int
avf_dev_uninit(struct rte_eth_dev *dev)
{
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;
	if (hw->adapter_stopped == 0)
		avf_dev_close(dev);

	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
	vf->vf_res = NULL;

	rte_free(vf->aq_resp);
	vf->aq_resp = NULL;

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	if (vf->rss_lut) {
		rte_free(vf->rss_lut);
		vf->rss_lut = NULL;
	}
	if (vf->rss_key) {
		rte_free(vf->rss_key);
		vf->rss_key = NULL;
	}

	return 0;
}

static int eth_avf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			     struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct avf_adapter), avf_dev_init);
}

static int eth_avf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, avf_dev_uninit);
}

/* Adaptive virtual function driver struct */
static struct rte_pci_driver rte_avf_pmd = {
	.id_table = pci_id_avf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_IOVA_AS_VA,
	.probe = eth_avf_pci_probe,
	.remove = eth_avf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_avf, rte_avf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_avf, pci_id_avf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_avf, "* igb_uio | vfio-pci");
RTE_INIT(avf_init_log);
static void
avf_init_log(void)
{
	avf_logtype_init = rte_log_register("pmd.avf.init");
	if (avf_logtype_init >= 0)
		rte_log_set_level(avf_logtype_init, RTE_LOG_NOTICE);
	avf_logtype_driver = rte_log_register("pmd.avf.driver");
	if (avf_logtype_driver >= 0)
		rte_log_set_level(avf_logtype_driver, RTE_LOG_NOTICE);
}

/* memory func for base code */
enum avf_status_code
avf_allocate_dma_mem_d(__rte_unused struct avf_hw *hw,
		       struct avf_dma_mem *mem,
		       u64 size,
		       u32 alignment)
{
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return AVF_ERR_PARAM;

	snprintf(z_name, sizeof(z_name), "avf_dma_%"PRIu64, rte_rand());
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY, 0,
					 alignment, RTE_PGSIZE_2M);
	if (!mz)
		return AVF_ERR_NO_MEMORY;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->phys_addr;
	mem->zone = (const void *)mz;
	PMD_DRV_LOG(DEBUG,
		    "memzone %s allocated with physical address: %"PRIu64,
		    mz->name, mem->pa);

	return AVF_SUCCESS;
}

enum avf_status_code
avf_free_dma_mem_d(__rte_unused struct avf_hw *hw,
		   struct avf_dma_mem *mem)
{
	if (!mem)
		return AVF_ERR_PARAM;

	PMD_DRV_LOG(DEBUG,
		    "memzone %s to be freed with physical address: %"PRIu64,
		    ((const struct rte_memzone *)mem->zone)->name, mem->pa);
	rte_memzone_free((const struct rte_memzone *)mem->zone);
	mem->zone = NULL;
	mem->va = NULL;
	mem->pa = (u64)0;

	return AVF_SUCCESS;
}

enum avf_status_code
avf_allocate_virt_mem_d(__rte_unused struct avf_hw *hw,
			struct avf_virt_mem *mem,
			u32 size)
{
	if (!mem)
		return AVF_ERR_PARAM;

	mem->size = size;
	mem->va = rte_zmalloc("avf", size, 0);

	if (mem->va)
		return AVF_SUCCESS;
	else
		return AVF_ERR_NO_MEMORY;
}

enum avf_status_code
avf_free_virt_mem_d(__rte_unused struct avf_hw *hw,
		    struct avf_virt_mem *mem)
{
	if (!mem)
		return AVF_ERR_PARAM;

	rte_free(mem->va);
	mem->va = NULL;

	return AVF_SUCCESS;
}
