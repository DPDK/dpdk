/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2017 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <rte_ethdev.h>

#include "base/ixgbe_api.h"
#include "ixgbe_ethdev.h"
#include "rte_pmd_ixgbe.h"

int
rte_pmd_ixgbe_set_vf_mac_addr(uint8_t port, uint16_t vf,
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
rte_pmd_ixgbe_ping_vf(uint8_t port, uint16_t vf)
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
rte_pmd_ixgbe_set_vf_vlan_anti_spoof(uint8_t port, uint16_t vf, uint8_t on)
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
rte_pmd_ixgbe_set_vf_mac_anti_spoof(uint8_t port, uint16_t vf, uint8_t on)
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
rte_pmd_ixgbe_set_vf_vlan_insert(uint8_t port, uint16_t vf, uint16_t vlan_id)
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
rte_pmd_ixgbe_set_tx_loopback(uint8_t port, uint8_t on)
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
rte_pmd_ixgbe_set_all_queues_drop_en(uint8_t port, uint8_t on)
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
rte_pmd_ixgbe_set_vf_split_drop_en(uint8_t port, uint16_t vf, uint8_t on)
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
rte_pmd_ixgbe_set_vf_vlan_stripq(uint8_t port, uint16_t vf, uint8_t on)
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
rte_pmd_ixgbe_set_vf_rxmode(uint8_t port, uint16_t vf,
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
rte_pmd_ixgbe_set_vf_rx(uint8_t port, uint16_t vf, uint8_t on)
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
rte_pmd_ixgbe_set_vf_tx(uint8_t port, uint16_t vf, uint8_t on)
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
rte_pmd_ixgbe_set_vf_vlan_filter(uint8_t port, uint16_t vlan,
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
rte_pmd_ixgbe_set_vf_rate_limit(uint8_t port, uint16_t vf,
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
rte_pmd_ixgbe_macsec_enable(uint8_t port, uint8_t en, uint8_t rp)
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
rte_pmd_ixgbe_macsec_disable(uint8_t port)
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
rte_pmd_ixgbe_macsec_config_txsc(uint8_t port, uint8_t *mac)
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
rte_pmd_ixgbe_macsec_config_rxsc(uint8_t port, uint8_t *mac, uint16_t pi)
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
rte_pmd_ixgbe_macsec_select_txsa(uint8_t port, uint8_t idx, uint8_t an,
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
rte_pmd_ixgbe_macsec_select_rxsa(uint8_t port, uint8_t idx, uint8_t an,
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
rte_pmd_ixgbe_set_tc_bw_alloc(uint8_t port,
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

#ifdef RTE_LIBRTE_IXGBE_BYPASS
int
rte_pmd_ixgbe_bypass_init(uint8_t port_id)
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
rte_pmd_ixgbe_bypass_state_show(uint8_t port_id, uint32_t *state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_state_show(dev, state);
}

int
rte_pmd_ixgbe_bypass_state_set(uint8_t port_id, uint32_t *new_state)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_state_store(dev, new_state);
}

int
rte_pmd_ixgbe_bypass_event_show(uint8_t port_id,
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
rte_pmd_ixgbe_bypass_event_store(uint8_t port_id,
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
rte_pmd_ixgbe_bypass_wd_timeout_store(uint8_t port_id, uint32_t timeout)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_wd_timeout_store(dev, timeout);
}

int
rte_pmd_ixgbe_bypass_ver_show(uint8_t port_id, uint32_t *ver)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_ver_show(dev, ver);
}

int
rte_pmd_ixgbe_bypass_wd_timeout_show(uint8_t port_id, uint32_t *wd_timeout)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_wd_timeout_show(dev, wd_timeout);
}

int
rte_pmd_ixgbe_bypass_wd_reset(uint8_t port_id)
{
	struct rte_eth_dev *dev;

	RTE_ETH_VALID_PORTID_OR_ERR_RET(port_id, -ENODEV);

	dev = &rte_eth_devices[port_id];
	if (!is_ixgbe_supported(dev))
		return -ENOTSUP;

	return ixgbe_bypass_wd_reset(dev);
}
#endif
