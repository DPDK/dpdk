/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

#include <rte_ethdev_driver.h>

#include "base/ixgbe_api.h"
#include "base/ixgbe_x550.h"
#include "ixgbe_ethdev.h"
#include "rte_pmd_ixgbe.h"

int
rte_pmd_ixgbe_set_vf_mac_addr(uint16_t port, uint16_t vf,
			      struct ether_addr *mac_addr)
{
	struct ixgbe_hw *hw;
	struct ixgbe_vf_info *vfinfo;
	int rar_entry;
	uint8_t *new_mac = (uint8_t *)(mac_addr);
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vfinfo = *(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));
	rar_entry = hw->mac.num_rar_entries - (vf + 1);

	if (is_valid_assigned_ether_addr((struct ether_addr *)new_mac)) {
		rte_memcpy(vfinfo[vf].vf_mac_addresses, new_mac,
			   ETHER_ADDR_LEN);
		return hw->mac.ops.set_rar(hw, rar_entry, new_mac, vf,
					   IXGBE_RAH_AV);
	}
	return -EINVAL;
}

int
rte_pmd_ixgbe_ping_vf(uint16_t port, uint16_t vf)
{
	struct ixgbe_hw *hw;
	struct ixgbe_vf_info *vfinfo;
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;
	uint32_t ctrl;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vfinfo = *(IXGBE_DEV_PRIVATE_TO_P_VFDATA(dev->data->dev_private));

	ctrl = IXGBE_PF_CONTROL_MSG;
	if (vfinfo[vf].clear_to_send)
		ctrl |= IXGBE_VT_MSGTYPE_CTS;

	ixgbe_write_mbx(hw, &ctrl, 1, vf);

	return 0;
}

int
rte_pmd_ixgbe_set_vf_vlan_anti_spoof(uint16_t port, uint16_t vf, uint8_t on)
{
	struct ixgbe_hw *hw;
	struct ixgbe_mac_info *mac;
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	mac = &hw->mac;

	mac->ops.set_vlan_anti_spoofing(hw, on, vf);

	return 0;
}

int
rte_pmd_ixgbe_set_vf_mac_anti_spoof(uint16_t port, uint16_t vf, uint8_t on)
{
	struct ixgbe_hw *hw;
	struct ixgbe_mac_info *mac;
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	mac = &hw->mac;
	mac->ops.set_mac_anti_spoofing(hw, on, vf);

	return 0;
}

int
rte_pmd_ixgbe_set_vf_vlan_insert(uint16_t port, uint16_t vf, uint16_t vlan_id)
{
	struct ixgbe_hw *hw;
	uint32_t ctrl;
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (vlan_id > ETHER_MAX_VLAN_ID)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	ctrl = IXGBE_READ_REG(hw, IXGBE_VMVIR(vf));
	if (vlan_id) {
		ctrl = vlan_id;
		ctrl |= IXGBE_VMVIR_VLANA_DEFAULT;
	} else {
		ctrl = 0;
	}

	IXGBE_WRITE_REG(hw, IXGBE_VMVIR(vf), ctrl);

	return 0;
}

int
rte_pmd_ixgbe_set_tx_loopback(uint16_t port, uint8_t on)
{
	struct ixgbe_hw *hw;
	uint32_t ctrl;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	ctrl = IXGBE_READ_REG(hw, IXGBE_PFDTXGSWC);
	/* enable or disable VMDQ loopback */
	if (on)
		ctrl |= IXGBE_PFDTXGSWC_VT_LBEN;
	else
		ctrl &= ~IXGBE_PFDTXGSWC_VT_LBEN;

	IXGBE_WRITE_REG(hw, IXGBE_PFDTXGSWC, ctrl);

	return 0;
}

int
rte_pmd_ixgbe_set_all_queues_drop_en(uint16_t port, uint8_t on)
{
	struct ixgbe_hw *hw;
	uint32_t reg_value;
	int i;
	int num_queues = (int)(IXGBE_QDE_IDX_MASK >> IXGBE_QDE_IDX_SHIFT);
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	for (i = 0; i <= num_queues; i++) {
		reg_value = IXGBE_QDE_WRITE |
				(i << IXGBE_QDE_IDX_SHIFT) |
				(on & IXGBE_QDE_ENABLE);
		IXGBE_WRITE_REG(hw, IXGBE_QDE, reg_value);
	}

	return 0;
}

int
rte_pmd_ixgbe_set_vf_split_drop_en(uint16_t port, uint16_t vf, uint8_t on)
{
	struct ixgbe_hw *hw;
	uint32_t reg_value;
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	/* only support VF's 0 to 63 */
	if ((vf >= pci_dev->max_vfs) || (vf > 63))
		return -EINVAL;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	reg_value = IXGBE_READ_REG(hw, IXGBE_SRRCTL(vf));
	if (on)
		reg_value |= IXGBE_SRRCTL_DROP_EN;
	else
		reg_value &= ~IXGBE_SRRCTL_DROP_EN;

	IXGBE_WRITE_REG(hw, IXGBE_SRRCTL(vf), reg_value);

	return 0;
}

int
rte_pmd_ixgbe_set_vf_vlan_stripq(uint16_t port, uint16_t vf, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;
	struct ixgbe_hw *hw;
	uint16_t queues_per_pool;
	uint32_t q;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (on > 1)
		return -EINVAL;

	RTE_FUNC_PTR_OR_ERR_RET(*dev->dev_ops->vlan_strip_queue_set, -ENOTSUP);

	/* The PF has 128 queue pairs and in SRIOV configuration
	 * those queues will be assigned to VF's, so RXDCTL
	 * registers will be dealing with queues which will be
	 * assigned to VF's.
	 * Let's say we have SRIOV configured with 31 VF's then the
	 * first 124 queues 0-123 will be allocated to VF's and only
	 * the last 4 queues 123-127 will be assigned to the PF.
	 */
	if (hw->mac.type == ixgbe_mac_82598EB)
		queues_per_pool = (uint16_t)hw->mac.max_rx_queues /
				  ETH_16_POOLS;
	else
		queues_per_pool = (uint16_t)hw->mac.max_rx_queues /
				  ETH_64_POOLS;

	for (q = 0; q < queues_per_pool; q++)
		(*dev->dev_ops->vlan_strip_queue_set)(dev,
				q + vf * queues_per_pool, on);
	return 0;
}

int
rte_pmd_ixgbe_set_vf_rxmode(uint16_t port, uint16_t vf,
			    uint16_t rx_mask, uint8_t on)
{
	int val = 0;
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;
	struct ixgbe_hw *hw;
	uint32_t vmolr;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	vmolr = IXGBE_READ_REG(hw, IXGBE_VMOLR(vf));

	if (hw->mac.type == ixgbe_mac_82598EB) {
		PMD_INIT_LOG(ERR, "setting VF receive mode set should be done"
			     " on 82599 hardware and newer");
		return -ENOTSUP;
	}
	if (ixgbe_vt_check(hw) < 0)
		return -ENOTSUP;

	val = ixgbe_convert_vm_rx_mask_to_val(rx_mask, val);

	if (on)
		vmolr |= val;
	else
		vmolr &= ~val;

	IXGBE_WRITE_REG(hw, IXGBE_VMOLR(vf), vmolr);

	return 0;
}

int
rte_pmd_ixgbe_set_vf_rx(uint16_t port, uint16_t vf, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;
	uint32_t reg, addr;
	uint32_t val;
	const uint8_t bit1 = 0x1;
	struct ixgbe_hw *hw;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (ixgbe_vt_check(hw) < 0)
		return -ENOTSUP;

	/* for vf >= 32, set bit in PFVFRE[1], otherwise PFVFRE[0] */
	if (vf >= 32) {
		addr = IXGBE_VFRE(1);
		val = bit1 << (vf - 32);
	} else {
		addr = IXGBE_VFRE(0);
		val = bit1 << vf;
	}

	reg = IXGBE_READ_REG(hw, addr);

	if (on)
		reg |= val;
	else
		reg &= ~val;

	IXGBE_WRITE_REG(hw, addr, reg);

	return 0;
}

int
rte_pmd_ixgbe_set_vf_tx(uint16_t port, uint16_t vf, uint8_t on)
{
	struct rte_eth_dev *dev;
	struct rte_pci_device *pci_dev;
	uint32_t reg, addr;
	uint32_t val;
	const uint8_t bit1 = 0x1;

	struct ixgbe_hw *hw;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	pci_dev = RTE_ETH_DEV_TO_PCI(dev);

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (vf >= pci_dev->max_vfs)
		return -EINVAL;

	if (on > 1)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (ixgbe_vt_check(hw) < 0)
		return -ENOTSUP;

	/* for vf >= 32, set bit in PFVFTE[1], otherwise PFVFTE[0] */
	if (vf >= 32) {
		addr = IXGBE_VFTE(1);
		val = bit1 << (vf - 32);
	} else {
		addr = IXGBE_VFTE(0);
		val = bit1 << vf;
	}

	reg = IXGBE_READ_REG(hw, addr);

	if (on)
		reg |= val;
	else
		reg &= ~val;

	IXGBE_WRITE_REG(hw, addr, reg);

	return 0;
}

int
rte_pmd_ixgbe_set_vf_vlan_filter(uint16_t port, uint16_t vlan,
				 uint64_t vf_mask, uint8_t vlan_on)
{
	struct rte_eth_dev *dev;
	int ret = 0;
	uint16_t vf_idx;
	struct ixgbe_hw *hw;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if ((vlan > ETHER_MAX_VLAN_ID) || (vf_mask == 0))
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (ixgbe_vt_check(hw) < 0)
		return -ENOTSUP;

	for (vf_idx = 0; vf_idx < 64; vf_idx++) {
		if (vf_mask & ((uint64_t)(1ULL << vf_idx))) {
			ret = hw->mac.ops.set_vfta(hw, vlan, vf_idx,
						   vlan_on, false);
			if (ret < 0)
				return ret;
		}
	}

	return ret;
}

int
rte_pmd_ixgbe_set_vf_rate_limit(uint16_t port, uint16_t vf,
				uint16_t tx_rate, uint64_t q_msk)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_set_vf_rate_limit(dev, vf, tx_rate, q_msk);
}

int
rte_pmd_ixgbe_macsec_enable(uint16_t port, uint8_t en, uint8_t rp)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	uint32_t ctrl;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Stop the data paths */
	if (ixgbe_disable_sec_rx_path(hw) != IXGBE_SUCCESS)
		return -ENOTSUP;
	/**
	 * Workaround:
	 * As no ixgbe_disable_sec_rx_path equivalent is
	 * implemented for tx in the base code, and we are
	 * not allowed to modify the base code in DPDK, so
	 * just call the hand-written one directly for now.
	 * The hardware support has been checked by
	 * ixgbe_disable_sec_rx_path().
	 */
	ixgbe_disable_sec_tx_path_generic(hw);

	/* Enable Ethernet CRC (required by MACsec offload) */
	ctrl = IXGBE_READ_REG(hw, IXGBE_HLREG0);
	ctrl |= IXGBE_HLREG0_TXCRCEN | IXGBE_HLREG0_RXCRCSTRP;
	IXGBE_WRITE_REG(hw, IXGBE_HLREG0, ctrl);

	/* Enable the TX and RX crypto engines */
	ctrl = IXGBE_READ_REG(hw, IXGBE_SECTXCTRL);
	ctrl &= ~IXGBE_SECTXCTRL_SECTX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECTXCTRL, ctrl);

	ctrl = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	ctrl &= ~IXGBE_SECRXCTRL_SECRX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, ctrl);

	ctrl = IXGBE_READ_REG(hw, IXGBE_SECTXMINIFG);
	ctrl &= ~IXGBE_SECTX_MINSECIFG_MASK;
	ctrl |= 0x3;
	IXGBE_WRITE_REG(hw, IXGBE_SECTXMINIFG, ctrl);

	/* Enable SA lookup */
	ctrl = IXGBE_READ_REG(hw, IXGBE_LSECTXCTRL);
	ctrl &= ~IXGBE_LSECTXCTRL_EN_MASK;
	ctrl |= en ? IXGBE_LSECTXCTRL_AUTH_ENCRYPT :
		     IXGBE_LSECTXCTRL_AUTH;
	ctrl |= IXGBE_LSECTXCTRL_AISCI;
	ctrl &= ~IXGBE_LSECTXCTRL_PNTHRSH_MASK;
	ctrl |= IXGBE_MACSEC_PNTHRSH & IXGBE_LSECTXCTRL_PNTHRSH_MASK;
	IXGBE_WRITE_REG(hw, IXGBE_LSECTXCTRL, ctrl);

	ctrl = IXGBE_READ_REG(hw, IXGBE_LSECRXCTRL);
	ctrl &= ~IXGBE_LSECRXCTRL_EN_MASK;
	ctrl |= IXGBE_LSECRXCTRL_STRICT << IXGBE_LSECRXCTRL_EN_SHIFT;
	ctrl &= ~IXGBE_LSECRXCTRL_PLSH;
	if (rp)
		ctrl |= IXGBE_LSECRXCTRL_RP;
	else
		ctrl &= ~IXGBE_LSECRXCTRL_RP;
	IXGBE_WRITE_REG(hw, IXGBE_LSECRXCTRL, ctrl);

	/* Start the data paths */
	ixgbe_enable_sec_rx_path(hw);
	/**
	 * Workaround:
	 * As no ixgbe_enable_sec_rx_path equivalent is
	 * implemented for tx in the base code, and we are
	 * not allowed to modify the base code in DPDK, so
	 * just call the hand-written one directly for now.
	 */
	ixgbe_enable_sec_tx_path_generic(hw);

	return 0;
}

int
rte_pmd_ixgbe_macsec_disable(uint16_t port)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	uint32_t ctrl;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Stop the data paths */
	if (ixgbe_disable_sec_rx_path(hw) != IXGBE_SUCCESS)
		return -ENOTSUP;
	/**
	 * Workaround:
	 * As no ixgbe_disable_sec_rx_path equivalent is
	 * implemented for tx in the base code, and we are
	 * not allowed to modify the base code in DPDK, so
	 * just call the hand-written one directly for now.
	 * The hardware support has been checked by
	 * ixgbe_disable_sec_rx_path().
	 */
	ixgbe_disable_sec_tx_path_generic(hw);

	/* Disable the TX and RX crypto engines */
	ctrl = IXGBE_READ_REG(hw, IXGBE_SECTXCTRL);
	ctrl |= IXGBE_SECTXCTRL_SECTX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECTXCTRL, ctrl);

	ctrl = IXGBE_READ_REG(hw, IXGBE_SECRXCTRL);
	ctrl |= IXGBE_SECRXCTRL_SECRX_DIS;
	IXGBE_WRITE_REG(hw, IXGBE_SECRXCTRL, ctrl);

	/* Disable SA lookup */
	ctrl = IXGBE_READ_REG(hw, IXGBE_LSECTXCTRL);
	ctrl &= ~IXGBE_LSECTXCTRL_EN_MASK;
	ctrl |= IXGBE_LSECTXCTRL_DISABLE;
	IXGBE_WRITE_REG(hw, IXGBE_LSECTXCTRL, ctrl);

	ctrl = IXGBE_READ_REG(hw, IXGBE_LSECRXCTRL);
	ctrl &= ~IXGBE_LSECRXCTRL_EN_MASK;
	ctrl |= IXGBE_LSECRXCTRL_DISABLE << IXGBE_LSECRXCTRL_EN_SHIFT;
	IXGBE_WRITE_REG(hw, IXGBE_LSECRXCTRL, ctrl);

	/* Start the data paths */
	ixgbe_enable_sec_rx_path(hw);
	/**
	 * Workaround:
	 * As no ixgbe_enable_sec_rx_path equivalent is
	 * implemented for tx in the base code, and we are
	 * not allowed to modify the base code in DPDK, so
	 * just call the hand-written one directly for now.
	 */
	ixgbe_enable_sec_tx_path_generic(hw);

	return 0;
}

int
rte_pmd_ixgbe_macsec_config_txsc(uint16_t port, uint8_t *mac)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	uint32_t ctrl;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	ctrl = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
	IXGBE_WRITE_REG(hw, IXGBE_LSECTXSCL, ctrl);

	ctrl = mac[4] | (mac[5] << 8);
	IXGBE_WRITE_REG(hw, IXGBE_LSECTXSCH, ctrl);

	return 0;
}

int
rte_pmd_ixgbe_macsec_config_rxsc(uint16_t port, uint8_t *mac, uint16_t pi)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	uint32_t ctrl;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	ctrl = mac[0] | (mac[1] << 8) | (mac[2] << 16) | (mac[3] << 24);
	IXGBE_WRITE_REG(hw, IXGBE_LSECRXSCL, ctrl);

	pi = rte_cpu_to_be_16(pi);
	ctrl = mac[4] | (mac[5] << 8) | (pi << 16);
	IXGBE_WRITE_REG(hw, IXGBE_LSECRXSCH, ctrl);

	return 0;
}

int
rte_pmd_ixgbe_macsec_select_txsa(uint16_t port, uint8_t idx, uint8_t an,
				 uint32_t pn, uint8_t *key)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	uint32_t ctrl, i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (idx != 0 && idx != 1)
		return -EINVAL;

	if (an >= 4)
		return -EINVAL;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Set the PN and key */
	pn = rte_cpu_to_be_32(pn);
	if (idx == 0) {
		IXGBE_WRITE_REG(hw, IXGBE_LSECTXPN0, pn);

		for (i = 0; i < 4; i++) {
			ctrl = (key[i * 4 + 0] <<  0) |
			       (key[i * 4 + 1] <<  8) |
			       (key[i * 4 + 2] << 16) |
			       (key[i * 4 + 3] << 24);
			IXGBE_WRITE_REG(hw, IXGBE_LSECTXKEY0(i), ctrl);
		}
	} else {
		IXGBE_WRITE_REG(hw, IXGBE_LSECTXPN1, pn);

		for (i = 0; i < 4; i++) {
			ctrl = (key[i * 4 + 0] <<  0) |
			       (key[i * 4 + 1] <<  8) |
			       (key[i * 4 + 2] << 16) |
			       (key[i * 4 + 3] << 24);
			IXGBE_WRITE_REG(hw, IXGBE_LSECTXKEY1(i), ctrl);
		}
	}

	/* Set AN and select the SA */
	ctrl = (an << idx * 2) | (idx << 4);
	IXGBE_WRITE_REG(hw, IXGBE_LSECTXSA, ctrl);

	return 0;
}

int
rte_pmd_ixgbe_macsec_select_rxsa(uint16_t port, uint8_t idx, uint8_t an,
				 uint32_t pn, uint8_t *key)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	uint32_t ctrl, i;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (idx != 0 && idx != 1)
		return -EINVAL;

	if (an >= 4)
		return -EINVAL;

	/* Set the PN */
	pn = rte_cpu_to_be_32(pn);
	IXGBE_WRITE_REG(hw, IXGBE_LSECRXPN(idx), pn);

	/* Set the key */
	for (i = 0; i < 4; i++) {
		ctrl = (key[i * 4 + 0] <<  0) |
		       (key[i * 4 + 1] <<  8) |
		       (key[i * 4 + 2] << 16) |
		       (key[i * 4 + 3] << 24);
		IXGBE_WRITE_REG(hw, IXGBE_LSECRXKEY(idx, i), ctrl);
	}

	/* Set the AN and validate the SA */
	ctrl = an | (1 << 2);
	IXGBE_WRITE_REG(hw, IXGBE_LSECRXSA(idx), ctrl);

	return 0;
}

int
rte_pmd_ixgbe_set_tc_bw_alloc(uint16_t port,
			      uint8_t tc_num,
			      uint8_t *bw_weight)
{
	struct rte_eth_dev *dev;
	struct ixgbe_dcb_config *dcb_config;
	struct ixgbe_dcb_tc_config *tc;
	struct rte_eth_conf *eth_conf;
	struct ixgbe_bw_conf *bw_conf;
	uint8_t i;
	uint8_t nb_tcs;
	uint16_t sum;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];

	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	if (tc_num > IXGBE_DCB_MAX_TRAFFIC_CLASS) {
		PMD_DRV_LOG(ERR, "TCs should be no more than %d.",
			    IXGBE_DCB_MAX_TRAFFIC_CLASS);
		return -EINVAL;
	}

	dcb_config = IXGBE_DEV_PRIVATE_TO_DCB_CFG(dev->data->dev_private);
	bw_conf = IXGBE_DEV_PRIVATE_TO_BW_CONF(dev->data->dev_private);
	eth_conf = &dev->data->dev_conf;

	if (eth_conf->txmode.mq_mode == ETH_MQ_TX_DCB) {
		nb_tcs = eth_conf->tx_adv_conf.dcb_tx_conf.nb_tcs;
	} else if (eth_conf->txmode.mq_mode == ETH_MQ_TX_VMDQ_DCB) {
		if (eth_conf->tx_adv_conf.vmdq_dcb_tx_conf.nb_queue_pools ==
		    ETH_32_POOLS)
			nb_tcs = ETH_4_TCS;
		else
			nb_tcs = ETH_8_TCS;
	} else {
		nb_tcs = 1;
	}

	if (nb_tcs != tc_num) {
		PMD_DRV_LOG(ERR,
			    "Weight should be set for all %d enabled TCs.",
			    nb_tcs);
		return -EINVAL;
	}

	sum = 0;
	for (i = 0; i < nb_tcs; i++)
		sum += bw_weight[i];
	if (sum != 100) {
		PMD_DRV_LOG(ERR,
			    "The summary of the TC weight should be 100.");
		return -EINVAL;
	}

	for (i = 0; i < nb_tcs; i++) {
		tc = &dcb_config->tc_config[i];
		tc->path[IXGBE_DCB_TX_CONFIG].bwg_percent = bw_weight[i];
	}
	for (; i < IXGBE_DCB_MAX_TRAFFIC_CLASS; i++) {
		tc = &dcb_config->tc_config[i];
		tc->path[IXGBE_DCB_TX_CONFIG].bwg_percent = 0;
	}

	bw_conf->tc_num = nb_tcs;

	return 0;
}

int __rte_experimental
rte_pmd_ixgbe_upd_fctrl_sbp(uint16_t port, int enable)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	uint32_t fctrl;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);
	dev = &rte_eth_devices[port];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (!hw)
		return -ENOTSUP;

	fctrl = IXGBE_READ_REG(hw, IXGBE_FCTRL);

	/* If 'enable' set the SBP bit else clear it */
	if (enable)
		fctrl |= IXGBE_FCTRL_SBP;
	else
		fctrl &= ~(IXGBE_FCTRL_SBP);

	IXGBE_WRITE_REG(hw, IXGBE_FCTRL, fctrl);
	return 0;
}

#ifdef RTE_LIBRTE_IXGBE_BYPASS
int
rte_pmd_ixgbe_bypass_init(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	ixgbe_bypass_init(dev);
	return 0;
}

int
rte_pmd_ixgbe_bypass_state_show(uint16_t port_id, uint32_t *state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_state_show(dev, state);
}

int
rte_pmd_ixgbe_bypass_state_set(uint16_t port_id, uint32_t *new_state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_state_store(dev, new_state);
}

int
rte_pmd_ixgbe_bypass_event_show(uint16_t port_id,
				uint32_t event,
				uint32_t *state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_event_show(dev, event, state);
}

int
rte_pmd_ixgbe_bypass_event_store(uint16_t port_id,
				 uint32_t event,
				 uint32_t state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_event_store(dev, event, state);
}

int
rte_pmd_ixgbe_bypass_wd_timeout_store(uint16_t port_id, uint32_t timeout)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_wd_timeout_store(dev, timeout);
}

int
rte_pmd_ixgbe_bypass_ver_show(uint16_t port_id, uint32_t *ver)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_ver_show(dev, ver);
}

int
rte_pmd_ixgbe_bypass_wd_timeout_show(uint16_t port_id, uint32_t *wd_timeout)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_wd_timeout_show(dev, wd_timeout);
}

int
rte_pmd_ixgbe_bypass_wd_reset(uint16_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_wd_reset(dev);
}
#endif

/**
 *  rte_pmd_ixgbe_acquire_swfw - Acquire SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to acquire
 *
 *  Acquires the SWFW semaphore and get the shared phy token as needed
 */
STATIC s32 rte_pmd_ixgbe_acquire_swfw(struct ixgbe_hw *hw, u32 mask)
{
	int retries = FW_PHY_TOKEN_RETRIES;
	s32 status = IXGBE_SUCCESS;

	while (--retries) {
		status = ixgbe_acquire_swfw_semaphore(hw, mask);
		if (status) {
			PMD_DRV_LOG(ERR, "Get SWFW sem failed, Status = %d\n",
				    status);
			return status;
		}
		status = ixgbe_get_phy_token(hw);
		if (status == IXGBE_SUCCESS)
			return IXGBE_SUCCESS;

		if (status == IXGBE_ERR_TOKEN_RETRY)
			PMD_DRV_LOG(ERR, "Get PHY token failed, Status = %d\n",
				    status);

		ixgbe_release_swfw_semaphore(hw, mask);
		if (status != IXGBE_ERR_TOKEN_RETRY) {
			PMD_DRV_LOG(ERR,
				    "Retry get PHY token failed, Status=%d\n",
				    status);
			return status;
		}
	}
	PMD_DRV_LOG(ERR, "swfw acquisition retries failed!: PHY ID = 0x%08X\n",
		    hw->phy.id);
	return status;
}

/**
 *  rte_pmd_ixgbe_release_swfw_sync - Release SWFW semaphore
 *  @hw: pointer to hardware structure
 *  @mask: Mask to specify which semaphore to release
 *
 *  Releases the SWFW semaphore and puts the shared phy token as needed
 */
STATIC void rte_pmd_ixgbe_release_swfw(struct ixgbe_hw *hw, u32 mask)
{
	ixgbe_put_phy_token(hw);
	ixgbe_release_swfw_semaphore(hw, mask);
}

int __rte_experimental
rte_pmd_ixgbe_mdio_lock(uint16_t port)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	u32 swfw_mask;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);
	dev = &rte_eth_devices[port];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (!hw)
		return -ENOTSUP;

	if (hw->bus.lan_id)
		swfw_mask = IXGBE_GSSR_PHY1_SM;
	else
		swfw_mask = IXGBE_GSSR_PHY0_SM;

	if (rte_pmd_ixgbe_acquire_swfw(hw, swfw_mask))
		return IXGBE_ERR_SWFW_SYNC;

	return IXGBE_SUCCESS;
}

int __rte_experimental
rte_pmd_ixgbe_mdio_unlock(uint16_t port)
{
	struct rte_eth_dev *dev;
	struct ixgbe_hw *hw;
	u32 swfw_mask;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);

	dev = &rte_eth_devices[port];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (!hw)
		return -ENOTSUP;

	if (hw->bus.lan_id)
		swfw_mask = IXGBE_GSSR_PHY1_SM;
	else
		swfw_mask = IXGBE_GSSR_PHY0_SM;

	rte_pmd_ixgbe_release_swfw(hw, swfw_mask);

	return IXGBE_SUCCESS;
}

int __rte_experimental
rte_pmd_ixgbe_mdio_unlocked_read(uint16_t port, uint32_t reg_addr,
				 uint32_t dev_type, uint16_t *phy_data)
{
	struct ixgbe_hw *hw;
	struct rte_eth_dev *dev;
	u32 i, data, command;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);
	dev = &rte_eth_devices[port];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (!hw)
		return -ENOTSUP;

	/* Setup and write the read command */
	command = (reg_addr << IXGBE_MSCA_DEV_TYPE_SHIFT) |
		  (dev_type << IXGBE_MSCA_PHY_ADDR_SHIFT) |
		  IXGBE_MSCA_OLD_PROTOCOL | IXGBE_MSCA_READ_AUTOINC |
		  IXGBE_MSCA_MDI_COMMAND;

	IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

	/* Check every 10 usec to see if the access completed.
	 * The MDI Command bit will clear when the operation is
	 * complete
	 */
	for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
		usec_delay(10);

		command = IXGBE_READ_REG(hw, IXGBE_MSCA);
		if (!(command & IXGBE_MSCA_MDI_COMMAND))
			break;
	}
	if (command & IXGBE_MSCA_MDI_COMMAND)
		return IXGBE_ERR_PHY;

	/* Read operation is complete.  Get the data from MSRWD */
	data = IXGBE_READ_REG(hw, IXGBE_MSRWD);
	data >>= IXGBE_MSRWD_READ_DATA_SHIFT;
	*phy_data = (u16)data;

	return 0;
}

int __rte_experimental
rte_pmd_ixgbe_mdio_unlocked_write(uint16_t port, uint32_t reg_addr,
				  uint32_t dev_type, uint16_t phy_data)
{
	struct ixgbe_hw *hw;
	u32 i, command;
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port, -ENODEV);
	dev = &rte_eth_devices[port];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	hw = IXGBE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	if (!hw)
		return -ENOTSUP;

	/* Put the data in the MDI single read and write data register*/
	IXGBE_WRITE_REG(hw, IXGBE_MSRWD, (u32)phy_data);

	/* Setup and write the write command */
	command = (reg_addr << IXGBE_MSCA_DEV_TYPE_SHIFT) |
		  (dev_type << IXGBE_MSCA_PHY_ADDR_SHIFT) |
		  IXGBE_MSCA_OLD_PROTOCOL | IXGBE_MSCA_WRITE |
		  IXGBE_MSCA_MDI_COMMAND;

	IXGBE_WRITE_REG(hw, IXGBE_MSCA, command);

	/* Check every 10 usec to see if the access completed.
	 * The MDI Command bit will clear when the operation is
	 * complete
	 */
	for (i = 0; i < IXGBE_MDIO_COMMAND_TIMEOUT; i++) {
		usec_delay(10);

		command = IXGBE_READ_REG(hw, IXGBE_MSCA);
		if (!(command & IXGBE_MSCA_MDI_COMMAND))
			break;
	}
	if (command & IXGBE_MSCA_MDI_COMMAND) {
		ERROR_REPORT1(IXGBE_ERROR_POLLING,
			      "PHY write cmd didn't complete\n");
		return IXGBE_ERR_PHY;
	}
	return 0;
}
