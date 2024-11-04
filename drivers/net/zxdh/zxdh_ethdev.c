/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <ethdev_pci.h>
#include <bus_pci_driver.h>
#include <rte_ethdev.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_msg.h"
#include "zxdh_common.h"
#include "zxdh_queue.h"

struct zxdh_hw_internal zxdh_hw_internal[RTE_MAX_ETHPORTS];

uint16_t
zxdh_vport_to_vfid(union zxdh_virport_num v)
{
	/* epid > 4 is local soft queue. return 1192 */
	if (v.epid > 4)
		return 1192;
	if (v.vf_flag)
		return v.epid * 256 + v.vfid;
	else
		return (v.epid * 8 + v.pfid) + 1152;
}

static void
zxdh_queues_unbind_intr(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int32_t i;

	for (i = 0; i < dev->data->nb_rx_queues; ++i) {
		ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2], ZXDH_MSI_NO_VECTOR);
		ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2 + 1], ZXDH_MSI_NO_VECTOR);
	}
}


static int32_t
zxdh_intr_unmask(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (rte_intr_ack(dev->intr_handle) < 0)
		return -1;

	hw->use_msix = zxdh_pci_msix_detect(RTE_ETH_DEV_TO_PCI(dev));

	return 0;
}

static void
zxdh_devconf_intr_handler(void *param)
{
	struct rte_eth_dev *dev = param;

	if (zxdh_intr_unmask(dev) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");
}


/* Interrupt handler triggered by NIC for handling specific interrupt. */
static void
zxdh_fromriscv_intr_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct zxdh_hw *hw = dev->data->dev_private;
	uint64_t virt_addr = (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX] + ZXDH_CTRLCH_OFFSET);

	if (hw->is_pf) {
		PMD_DRV_LOG(DEBUG, "zxdh_risc2pf_intr_handler");
		zxdh_bar_irq_recv(ZXDH_MSG_CHAN_END_RISC, ZXDH_MSG_CHAN_END_PF, virt_addr, dev);
	} else {
		PMD_DRV_LOG(DEBUG, "zxdh_riscvf_intr_handler");
		zxdh_bar_irq_recv(ZXDH_MSG_CHAN_END_RISC, ZXDH_MSG_CHAN_END_VF, virt_addr, dev);
	}
}

/* Interrupt handler triggered by NIC for handling specific interrupt. */
static void
zxdh_frompfvf_intr_handler(void *param)
{
	struct rte_eth_dev *dev = param;
	struct zxdh_hw *hw = dev->data->dev_private;
	uint64_t virt_addr = (uint64_t)(hw->bar_addr[ZXDH_BAR0_INDEX] +
				ZXDH_MSG_CHAN_PFVFSHARE_OFFSET);

	if (hw->is_pf) {
		PMD_DRV_LOG(DEBUG, "zxdh_vf2pf_intr_handler");
		zxdh_bar_irq_recv(ZXDH_MSG_CHAN_END_VF, ZXDH_MSG_CHAN_END_PF, virt_addr, dev);
	} else {
		PMD_DRV_LOG(DEBUG, "zxdh_pf2vf_intr_handler");
		zxdh_bar_irq_recv(ZXDH_MSG_CHAN_END_PF, ZXDH_MSG_CHAN_END_VF, virt_addr, dev);
	}
}

static void
zxdh_intr_cb_reg(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		rte_intr_callback_unregister(dev->intr_handle, zxdh_devconf_intr_handler, dev);

	/* register callback to update dev config intr */
	rte_intr_callback_register(dev->intr_handle, zxdh_devconf_intr_handler, dev);
	/* Register rsic_v to pf interrupt callback */
	struct rte_intr_handle *tmp = hw->risc_intr +
			(ZXDH_MSIX_FROM_PFVF - ZXDH_MSIX_INTR_MSG_VEC_BASE);

	rte_intr_callback_register(tmp, zxdh_frompfvf_intr_handler, dev);

	tmp = hw->risc_intr + (ZXDH_MSIX_FROM_RISCV - ZXDH_MSIX_INTR_MSG_VEC_BASE);
	rte_intr_callback_register(tmp, zxdh_fromriscv_intr_handler, dev);
}

static void
zxdh_intr_cb_unreg(struct rte_eth_dev *dev)
{
	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		rte_intr_callback_unregister(dev->intr_handle, zxdh_devconf_intr_handler, dev);

	struct zxdh_hw *hw = dev->data->dev_private;

	/* register callback to update dev config intr */
	rte_intr_callback_unregister(dev->intr_handle, zxdh_devconf_intr_handler, dev);
	/* Register rsic_v to pf interrupt callback */
	struct rte_intr_handle *tmp = hw->risc_intr +
			(ZXDH_MSIX_FROM_PFVF - ZXDH_MSIX_INTR_MSG_VEC_BASE);

	rte_intr_callback_unregister(tmp, zxdh_frompfvf_intr_handler, dev);
	tmp = hw->risc_intr + (ZXDH_MSIX_FROM_RISCV - ZXDH_MSIX_INTR_MSG_VEC_BASE);
	rte_intr_callback_unregister(tmp, zxdh_fromriscv_intr_handler, dev);
}

static int32_t
zxdh_intr_disable(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (!hw->intr_enabled)
		return 0;

	zxdh_intr_cb_unreg(dev);
	if (rte_intr_disable(dev->intr_handle) < 0)
		return -1;

	hw->intr_enabled = 0;
	return 0;
}

static int32_t
zxdh_intr_enable(struct rte_eth_dev *dev)
{
	int ret = 0;
	struct zxdh_hw *hw = dev->data->dev_private;

	if (!hw->intr_enabled) {
		zxdh_intr_cb_reg(dev);
		ret = rte_intr_enable(dev->intr_handle);
		if (unlikely(ret))
			PMD_DRV_LOG(ERR, "Failed to enable %s intr", dev->data->name);

		hw->intr_enabled = 1;
	}
	return ret;
}

static int32_t
zxdh_intr_release(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)
		ZXDH_VTPCI_OPS(hw)->set_config_irq(hw, ZXDH_MSI_NO_VECTOR);

	zxdh_queues_unbind_intr(dev);
	zxdh_intr_disable(dev);

	rte_intr_efd_disable(dev->intr_handle);
	rte_intr_vec_list_free(dev->intr_handle);
	rte_free(hw->risc_intr);
	hw->risc_intr = NULL;
	rte_free(hw->dtb_intr);
	hw->dtb_intr = NULL;
	return 0;
}

static int32_t
zxdh_setup_risc_interrupts(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint8_t i;

	if (!hw->risc_intr) {
		PMD_DRV_LOG(ERR, " to allocate risc_intr");
		hw->risc_intr = rte_zmalloc("risc_intr",
			ZXDH_MSIX_INTR_MSG_VEC_NUM * sizeof(struct rte_intr_handle), 0);
		if (hw->risc_intr == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate risc_intr");
			return -ENOMEM;
		}
	}

	for (i = 0; i < ZXDH_MSIX_INTR_MSG_VEC_NUM; i++) {
		if (dev->intr_handle->efds[i] < 0) {
			PMD_DRV_LOG(ERR, "[%u]risc interrupt fd is invalid", i);
			rte_free(hw->risc_intr);
			hw->risc_intr = NULL;
			return -1;
		}

		struct rte_intr_handle *intr_handle = hw->risc_intr + i;

		intr_handle->fd = dev->intr_handle->efds[i];
		intr_handle->type = dev->intr_handle->type;
	}

	return 0;
}

static int32_t
zxdh_setup_dtb_interrupts(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (!hw->dtb_intr) {
		hw->dtb_intr = rte_zmalloc("dtb_intr", sizeof(struct rte_intr_handle), 0);
		if (hw->dtb_intr == NULL) {
			PMD_DRV_LOG(ERR, "Failed to allocate dtb_intr");
			return -ENOMEM;
		}
	}

	if (dev->intr_handle->efds[ZXDH_MSIX_INTR_DTB_VEC - 1] < 0) {
		PMD_DRV_LOG(ERR, "[%d]dtb interrupt fd is invalid", ZXDH_MSIX_INTR_DTB_VEC - 1);
		rte_free(hw->dtb_intr);
		hw->dtb_intr = NULL;
		return -1;
	}
	hw->dtb_intr->fd = dev->intr_handle->efds[ZXDH_MSIX_INTR_DTB_VEC - 1];
	hw->dtb_intr->type = dev->intr_handle->type;
	return 0;
}

static int32_t
zxdh_queues_bind_intr(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int32_t i;
	uint16_t vec;

	if (!dev->data->dev_conf.intr_conf.rxq) {
		for (i = 0; i < dev->data->nb_rx_queues; ++i) {
			vec = ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw,
					hw->vqs[i * 2], ZXDH_MSI_NO_VECTOR);
			PMD_DRV_LOG(DEBUG, "vq%d irq set 0x%x, get 0x%x",
					i * 2, ZXDH_MSI_NO_VECTOR, vec);
		}
	} else {
		for (i = 0; i < dev->data->nb_rx_queues; ++i) {
			vec = ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw,
					hw->vqs[i * 2], i + ZXDH_QUEUE_INTR_VEC_BASE);
			PMD_DRV_LOG(DEBUG, "vq%d irq set %d, get %d",
					i * 2, i + ZXDH_QUEUE_INTR_VEC_BASE, vec);
		}
	}
	/* mask all txq intr */
	for (i = 0; i < dev->data->nb_tx_queues; ++i) {
		vec = ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw,
				hw->vqs[(i * 2) + 1], ZXDH_MSI_NO_VECTOR);
		PMD_DRV_LOG(DEBUG, "vq%d irq set 0x%x, get 0x%x",
				(i * 2) + 1, ZXDH_MSI_NO_VECTOR, vec);
	}
	return 0;
}

static int32_t
zxdh_configure_intr(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int32_t ret = 0;

	if (!rte_intr_cap_multiple(dev->intr_handle)) {
		PMD_DRV_LOG(ERR, "Multiple intr vector not supported");
		return -ENOTSUP;
	}
	zxdh_intr_release(dev);
	uint8_t nb_efd = ZXDH_MSIX_INTR_DTB_VEC_NUM + ZXDH_MSIX_INTR_MSG_VEC_NUM;

	if (dev->data->dev_conf.intr_conf.rxq)
		nb_efd += dev->data->nb_rx_queues;

	if (rte_intr_efd_enable(dev->intr_handle, nb_efd)) {
		PMD_DRV_LOG(ERR, "Fail to create eventfd");
		return -1;
	}

	if (rte_intr_vec_list_alloc(dev->intr_handle, "intr_vec",
					hw->max_queue_pairs + ZXDH_INTR_NONQUE_NUM)) {
		PMD_DRV_LOG(ERR, "Failed to allocate %u rxq vectors",
					hw->max_queue_pairs + ZXDH_INTR_NONQUE_NUM);
		return -ENOMEM;
	}
	PMD_DRV_LOG(DEBUG, "allocate %u rxq vectors", dev->intr_handle->vec_list_size);
	if (zxdh_setup_risc_interrupts(dev) != 0) {
		PMD_DRV_LOG(ERR, "Error setting up rsic_v interrupts!");
		ret = -1;
		goto free_intr_vec;
	}
	if (zxdh_setup_dtb_interrupts(dev) != 0) {
		PMD_DRV_LOG(ERR, "Error setting up dtb interrupts!");
		ret = -1;
		goto free_intr_vec;
	}

	if (zxdh_queues_bind_intr(dev) < 0) {
		PMD_DRV_LOG(ERR, "Failed to bind queue/interrupt");
		ret = -1;
		goto free_intr_vec;
	}

	if (zxdh_intr_enable(dev) < 0) {
		PMD_DRV_LOG(ERR, "interrupt enable failed");
		ret = -1;
		goto free_intr_vec;
	}
	return 0;

free_intr_vec:
	zxdh_intr_release(dev);
	return ret;
}

static int
zxdh_dev_close(struct rte_eth_dev *dev __rte_unused)
{
	int ret = 0;

	return ret;
}

static int32_t
zxdh_init_device(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret = 0;

	ret = zxdh_read_pci_caps(pci_dev, hw);
	if (ret) {
		PMD_DRV_LOG(ERR, "port 0x%x pci caps read failed .", hw->port_id);
		goto err;
	}

	zxdh_hw_internal[hw->port_id].zxdh_vtpci_ops = &zxdh_dev_pci_ops;
	zxdh_pci_reset(hw);
	zxdh_get_pci_dev_config(hw);

	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr, &eth_dev->data->mac_addrs[0]);

	/* If host does not support both status and MSI-X then disable LSC */
	if (vtpci_with_feature(hw, ZXDH_NET_F_STATUS) && hw->use_msix != ZXDH_MSIX_NONE)
		eth_dev->data->dev_flags |= RTE_ETH_DEV_INTR_LSC;
	else
		eth_dev->data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;

	return 0;

err:
	PMD_DRV_LOG(ERR, "port %d init device failed", eth_dev->data->port_id);
	return ret;
}

static int
zxdh_agent_comm(struct rte_eth_dev *eth_dev, struct zxdh_hw *hw)
{
	if (zxdh_phyport_get(eth_dev, &hw->phyport) != 0) {
		PMD_DRV_LOG(ERR, "Failed to get phyport");
		return -1;
	}
	PMD_DRV_LOG(INFO, "Get phyport success: 0x%x", hw->phyport);

	hw->vfid = zxdh_vport_to_vfid(hw->vport);

	if (zxdh_panelid_get(eth_dev, &hw->panel_id) != 0) {
		PMD_DRV_LOG(ERR, "Failed to get panel_id");
		return -1;
	}
	PMD_DRV_LOG(INFO, "Get panel id success: 0x%x", hw->panel_id);

	return 0;
}

static int
zxdh_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	int ret = 0;

	eth_dev->dev_ops = NULL;

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("zxdh_mac",
			ZXDH_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d bytes store MAC addresses",
				ZXDH_MAX_MAC_ADDRS * RTE_ETHER_ADDR_LEN);
		return -ENOMEM;
	}

	memset(hw, 0, sizeof(*hw));
	hw->bar_addr[0] = (uint64_t)pci_dev->mem_resource[0].addr;
	if (hw->bar_addr[0] == 0) {
		PMD_DRV_LOG(ERR, "Bad mem resource.");
		return -EIO;
	}

	hw->device_id = pci_dev->id.device_id;
	hw->port_id = eth_dev->data->port_id;
	hw->eth_dev = eth_dev;
	hw->speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	hw->duplex = RTE_ETH_LINK_FULL_DUPLEX;
	hw->is_pf = 0;

	if (pci_dev->id.device_id == ZXDH_E310_PF_DEVICEID ||
		pci_dev->id.device_id == ZXDH_E312_PF_DEVICEID) {
		hw->is_pf = 1;
	}

	ret = zxdh_init_device(eth_dev);
	if (ret < 0)
		goto err_zxdh_init;

	ret = zxdh_msg_chan_init();
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to init bar msg chan");
		goto err_zxdh_init;
	}
	hw->msg_chan_init = 1;

	ret = zxdh_msg_chan_hwlock_init(eth_dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "zxdh_msg_chan_hwlock_init failed ret %d", ret);
		goto err_zxdh_init;
	}

	ret = zxdh_msg_chan_enable(eth_dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "zxdh_msg_bar_chan_enable failed ret %d", ret);
		goto err_zxdh_init;
	}

	ret = zxdh_agent_comm(eth_dev, hw);
	if (ret != 0)
		goto err_zxdh_init;

	ret = zxdh_configure_intr(eth_dev);
	if (ret != 0)
		goto err_zxdh_init;

	return ret;

err_zxdh_init:
	zxdh_intr_release(eth_dev);
	zxdh_bar_msg_chan_exit();
	rte_free(eth_dev->data->mac_addrs);
	eth_dev->data->mac_addrs = NULL;
	return ret;
}

static int
zxdh_eth_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
						sizeof(struct zxdh_hw),
						zxdh_eth_dev_init);
}

static int
zxdh_eth_dev_uninit(struct rte_eth_dev *eth_dev)
{
	int ret = 0;

	ret = zxdh_dev_close(eth_dev);

	return ret;
}

static int
zxdh_eth_pci_remove(struct rte_pci_device *pci_dev)
{
	int ret = rte_eth_dev_pci_generic_remove(pci_dev, zxdh_eth_dev_uninit);

	return ret;
}

static const struct rte_pci_id pci_id_zxdh_map[] = {
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E310_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E310_VF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312_VF_DEVICEID)},
	{.vendor_id = 0, /* sentinel */ },
};
static struct rte_pci_driver zxdh_pmd = {
	.id_table = pci_id_zxdh_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = zxdh_eth_pci_probe,
	.remove = zxdh_eth_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_zxdh, zxdh_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_zxdh, pci_id_zxdh_map);
RTE_PMD_REGISTER_KMOD_DEP(net_zxdh, "* vfio-pci");
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_driver, driver, NOTICE);
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_rx, rx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_tx, tx, NOTICE);
RTE_LOG_REGISTER_SUFFIX(zxdh_logtype_msg, msg, NOTICE);
