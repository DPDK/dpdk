/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019-2020 Intel Corporation
 */

#include <stdint.h>

#include <rte_pci.h>
#include <rte_bus_pci.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>

#include "igc_logs.h"
#include "igc_ethdev.h"

#define IGC_INTEL_VENDOR_ID		0x8086

#define IGC_FC_PAUSE_TIME		0x0680

static const struct rte_pci_id pci_id_igc_map[] = {
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_LM) },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_V)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_I)  },
	{ RTE_PCI_DEVICE(IGC_INTEL_VENDOR_ID, IGC_DEV_ID_I225_K)  },
	{ .vendor_id = 0, /* sentinel */ },
};

static int eth_igc_configure(struct rte_eth_dev *dev);
static int eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete);
static void eth_igc_stop(struct rte_eth_dev *dev);
static int eth_igc_start(struct rte_eth_dev *dev);
static void eth_igc_close(struct rte_eth_dev *dev);
static int eth_igc_reset(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_enable(struct rte_eth_dev *dev);
static int eth_igc_promiscuous_disable(struct rte_eth_dev *dev);
static int eth_igc_infos_get(struct rte_eth_dev *dev,
			struct rte_eth_dev_info *dev_info);
static int
eth_igc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool);
static int
eth_igc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf);

static const struct eth_dev_ops eth_igc_ops = {
	.dev_configure		= eth_igc_configure,
	.link_update		= eth_igc_link_update,
	.dev_stop		= eth_igc_stop,
	.dev_start		= eth_igc_start,
	.dev_close		= eth_igc_close,
	.dev_reset		= eth_igc_reset,
	.promiscuous_enable	= eth_igc_promiscuous_enable,
	.promiscuous_disable	= eth_igc_promiscuous_disable,
	.dev_infos_get		= eth_igc_infos_get,
	.rx_queue_setup		= eth_igc_rx_queue_setup,
	.tx_queue_setup		= eth_igc_tx_queue_setup,
};

static int
eth_igc_configure(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static int
eth_igc_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	RTE_SET_USED(wait_to_complete);
	return 0;
}

static void
eth_igc_stop(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
}

/*
 *  Get hardware rx-buffer size.
 */
static inline int
igc_get_rx_buffer_size(struct igc_hw *hw)
{
	return (IGC_READ_REG(hw, IGC_RXPBS) & 0x3f) << 10;
}

/*
 * igc_hw_control_acquire sets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means
 * that the driver is loaded.
 */
static void
igc_hw_control_acquire(struct igc_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware know the driver has taken over */
	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(hw, IGC_CTRL_EXT, ctrl_ext | IGC_CTRL_EXT_DRV_LOAD);
}

/*
 * igc_hw_control_release resets CTRL_EXT:DRV_LOAD bit.
 * For ASF and Pass Through versions of f/w this means that the
 * driver is no longer loaded.
 */
static void
igc_hw_control_release(struct igc_hw *hw)
{
	uint32_t ctrl_ext;

	/* Let firmware taken over control of h/w */
	ctrl_ext = IGC_READ_REG(hw, IGC_CTRL_EXT);
	IGC_WRITE_REG(hw, IGC_CTRL_EXT,
			ctrl_ext & ~IGC_CTRL_EXT_DRV_LOAD);
}

static int
igc_hardware_init(struct igc_hw *hw)
{
	uint32_t rx_buf_size;
	int diag;

	/* Let the firmware know the OS is in control */
	igc_hw_control_acquire(hw);

	/* Issue a global reset */
	igc_reset_hw(hw);

	/* disable all wake up */
	IGC_WRITE_REG(hw, IGC_WUC, 0);

	/*
	 * Hardware flow control
	 * - High water mark should allow for at least two standard size (1518)
	 *   frames to be received after sending an XOFF.
	 * - Low water mark works best when it is very near the high water mark.
	 *   This allows the receiver to restart by sending XON when it has
	 *   drained a bit. Here we use an arbitrary value of 1500 which will
	 *   restart after one full frame is pulled from the buffer. There
	 *   could be several smaller frames in the buffer and if so they will
	 *   not trigger the XON until their total number reduces the buffer
	 *   by 1500.
	 */
	rx_buf_size = igc_get_rx_buffer_size(hw);
	hw->fc.high_water = rx_buf_size - (RTE_ETHER_MAX_LEN * 2);
	hw->fc.low_water = hw->fc.high_water - 1500;
	hw->fc.pause_time = IGC_FC_PAUSE_TIME;
	hw->fc.send_xon = 1;
	hw->fc.requested_mode = igc_fc_full;

	diag = igc_init_hw(hw);
	if (diag < 0)
		return diag;

	igc_get_phy_info(hw);
	igc_check_for_link(hw);

	return 0;
}

static int
eth_igc_start(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static int
igc_reset_swfw_lock(struct igc_hw *hw)
{
	int ret_val;

	/*
	 * Do mac ops initialization manually here, since we will need
	 * some function pointers set by this call.
	 */
	ret_val = igc_init_mac_params(hw);
	if (ret_val)
		return ret_val;

	/*
	 * SMBI lock should not fail in this early stage. If this is the case,
	 * it is due to an improper exit of the application.
	 * So force the release of the faulty lock.
	 */
	if (igc_get_hw_semaphore_generic(hw) < 0)
		PMD_DRV_LOG(DEBUG, "SMBI lock released");

	igc_put_hw_semaphore_generic(hw);

	if (hw->mac.ops.acquire_swfw_sync != NULL) {
		uint16_t mask;

		/*
		 * Phy lock should not fail in this early stage.
		 * If this is the case, it is due to an improper exit of the
		 * application. So force the release of the faulty lock.
		 */
		mask = IGC_SWFW_PHY0_SM;
		if (hw->mac.ops.acquire_swfw_sync(hw, mask) < 0) {
			PMD_DRV_LOG(DEBUG, "SWFW phy%d lock released",
				    hw->bus.func);
		}
		hw->mac.ops.release_swfw_sync(hw, mask);

		/*
		 * This one is more tricky since it is common to all ports; but
		 * swfw_sync retries last long enough (1s) to be almost sure
		 * that if lock can not be taken it is due to an improper lock
		 * of the semaphore.
		 */
		mask = IGC_SWFW_EEP_SM;
		if (hw->mac.ops.acquire_swfw_sync(hw, mask) < 0)
			PMD_DRV_LOG(DEBUG, "SWFW common locks released");

		hw->mac.ops.release_swfw_sync(hw, mask);
	}

	return IGC_SUCCESS;
}

static void
eth_igc_close(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	PMD_INIT_FUNC_TRACE();

	igc_phy_hw_reset(hw);
	igc_hw_control_release(hw);

	/* Reset any pending lock */
	igc_reset_swfw_lock(hw);
}

static void
igc_identify_hardware(struct rte_eth_dev *dev, struct rte_pci_device *pci_dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
}

static int
eth_igc_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	int error = 0;

	PMD_INIT_FUNC_TRACE();
	dev->dev_ops = &eth_igc_ops;

	/*
	 * for secondary processes, we don't initialize any further as primary
	 * has already done this work. Only check we don't need a different
	 * RX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	rte_eth_copy_pci_info(dev, pci_dev);

	hw->back = pci_dev;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;

	igc_identify_hardware(dev, pci_dev);
	if (igc_setup_init_funcs(hw, false) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	igc_get_bus_info(hw);

	/* Reset any pending lock */
	if (igc_reset_swfw_lock(hw) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	/* Finish initialization */
	if (igc_setup_init_funcs(hw, true) != IGC_SUCCESS) {
		error = -EIO;
		goto err_late;
	}

	hw->mac.autoneg = 1;
	hw->phy.autoneg_wait_to_complete = 0;
	hw->phy.autoneg_advertised = IGC_ALL_SPEED_DUPLEX_2500;

	/* Copper options */
	if (hw->phy.media_type == igc_media_type_copper) {
		hw->phy.mdix = 0; /* AUTO_ALL_MODES */
		hw->phy.disable_polarity_correction = 0;
		hw->phy.ms_type = igc_ms_hw_default;
	}

	/*
	 * Start from a known state, this is important in reading the nvm
	 * and mac from that.
	 */
	igc_reset_hw(hw);

	/* Make sure we have a good EEPROM before we read from it */
	if (igc_validate_nvm_checksum(hw) < 0) {
		/*
		 * Some PCI-E parts fail the first check due to
		 * the link being in sleep state, call it again,
		 * if it fails a second time its a real issue.
		 */
		if (igc_validate_nvm_checksum(hw) < 0) {
			PMD_INIT_LOG(ERR, "EEPROM checksum invalid");
			error = -EIO;
			goto err_late;
		}
	}

	/* Read the permanent MAC address out of the EEPROM */
	if (igc_read_mac_addr(hw) != 0) {
		PMD_INIT_LOG(ERR, "EEPROM error while reading MAC address");
		error = -EIO;
		goto err_late;
	}

	/* Allocate memory for storing MAC addresses */
	dev->data->mac_addrs = rte_zmalloc("igc",
		RTE_ETHER_ADDR_LEN * hw->mac.rar_entry_count, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes for storing MAC",
				RTE_ETHER_ADDR_LEN * hw->mac.rar_entry_count);
		error = -ENOMEM;
		goto err_late;
	}

	/* Copy the permanent MAC address */
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.addr,
			&dev->data->mac_addrs[0]);

	/* Now initialize the hardware */
	if (igc_hardware_init(hw) != 0) {
		PMD_INIT_LOG(ERR, "Hardware initialization failed");
		rte_free(dev->data->mac_addrs);
		dev->data->mac_addrs = NULL;
		error = -ENODEV;
		goto err_late;
	}

	/* Pass the information to the rte_eth_dev_close() that it should also
	 * release the private port resources.
	 */
	dev->data->dev_flags |= RTE_ETH_DEV_CLOSE_REMOVE;

	hw->mac.get_link_status = 1;

	/* Indicate SOL/IDER usage */
	if (igc_check_reset_block(hw) < 0)
		PMD_INIT_LOG(ERR,
			"PHY reset is blocked due to SOL/IDER session.");

	PMD_INIT_LOG(DEBUG, "port_id %d vendorID=0x%x deviceID=0x%x",
			dev->data->port_id, pci_dev->id.vendor_id,
			pci_dev->id.device_id);

	return 0;

err_late:
	igc_hw_control_release(hw);
	return error;
}

static int
eth_igc_dev_uninit(__rte_unused struct rte_eth_dev *eth_dev)
{
	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	eth_igc_close(eth_dev);
	return 0;
}

static int
eth_igc_reset(struct rte_eth_dev *dev)
{
	int ret;

	PMD_INIT_FUNC_TRACE();

	ret = eth_igc_dev_uninit(dev);
	if (ret)
		return ret;

	return eth_igc_dev_init(dev);
}

static int
eth_igc_promiscuous_enable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static int
eth_igc_promiscuous_disable(struct rte_eth_dev *dev)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	return 0;
}

static int
eth_igc_infos_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	dev_info->max_rx_queues = IGC_QUEUE_PAIRS_NUM;
	dev_info->max_tx_queues = IGC_QUEUE_PAIRS_NUM;
	return 0;
}

static int
eth_igc_rx_queue_setup(struct rte_eth_dev *dev, uint16_t rx_queue_id,
		uint16_t nb_rx_desc, unsigned int socket_id,
		const struct rte_eth_rxconf *rx_conf,
		struct rte_mempool *mb_pool)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	RTE_SET_USED(rx_queue_id);
	RTE_SET_USED(nb_rx_desc);
	RTE_SET_USED(socket_id);
	RTE_SET_USED(rx_conf);
	RTE_SET_USED(mb_pool);
	return 0;
}

static int
eth_igc_tx_queue_setup(struct rte_eth_dev *dev, uint16_t queue_idx,
		uint16_t nb_desc, unsigned int socket_id,
		const struct rte_eth_txconf *tx_conf)
{
	PMD_INIT_FUNC_TRACE();
	RTE_SET_USED(dev);
	RTE_SET_USED(queue_idx);
	RTE_SET_USED(nb_desc);
	RTE_SET_USED(socket_id);
	RTE_SET_USED(tx_conf);
	return 0;
}

static int
eth_igc_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct igc_adapter), eth_igc_dev_init);
}

static int
eth_igc_pci_remove(struct rte_pci_device *pci_dev)
{
	PMD_INIT_FUNC_TRACE();
	return rte_eth_dev_pci_generic_remove(pci_dev, eth_igc_dev_uninit);
}

static struct rte_pci_driver rte_igc_pmd = {
	.id_table = pci_id_igc_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_igc_pci_probe,
	.remove = eth_igc_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_igc, rte_igc_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_igc, pci_id_igc_map);
RTE_PMD_REGISTER_KMOD_DEP(net_igc, "* igb_uio | uio_pci_generic | vfio-pci");
