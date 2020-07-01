/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdint.h>

#include <rte_bus_pci.h>
#include <rte_ethdev.h>
#include <rte_pci.h>
#include <rte_malloc.h>

#include <rte_mbuf.h>
#include <rte_sched.h>
#include <rte_ethdev_driver.h>

#include <rte_io.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_bus_ifpga.h>
#include <ifpga_common.h>
#include <ifpga_logs.h>
#include <ifpga_rawdev.h>

#include "ipn3ke_rawdev_api.h"
#include "ipn3ke_flow.h"
#include "ipn3ke_logs.h"
#include "ipn3ke_ethdev.h"

static const struct rte_afu_uuid afu_uuid_ipn3ke_map[] = {
	{ MAP_UUID_10G_LOW,  MAP_UUID_10G_HIGH },
	{ IPN3KE_UUID_10G_LOW, IPN3KE_UUID_10G_HIGH },
	{ IPN3KE_UUID_VBNG_LOW, IPN3KE_UUID_VBNG_HIGH},
	{ IPN3KE_UUID_25G_LOW, IPN3KE_UUID_25G_HIGH },
	{ 0, 0 /* sentinel */ },
};

struct ipn3ke_pub_func ipn3ke_bridge_func;

static int
ipn3ke_indirect_read(struct ipn3ke_hw *hw, uint32_t *rd_data,
	uint32_t addr, uint32_t dev_sel, uint32_t eth_group_sel)
{
	uint32_t i, try_cnt;
	uint64_t indirect_value;
	volatile void *indirect_addrs;
	uint64_t target_addr;
	uint64_t read_data = 0;

	if (eth_group_sel != 0 && eth_group_sel != 1)
		return -1;

	target_addr = addr | dev_sel << 17;

	indirect_value = RCMD | target_addr << 32;
	indirect_addrs = hw->eth_group_bar[eth_group_sel] + 0x10;

	rte_delay_us(10);

	rte_write64((rte_cpu_to_le_64(indirect_value)), indirect_addrs);

	i = 0;
	try_cnt = 10;
	indirect_addrs = hw->eth_group_bar[eth_group_sel] +
		0x18;
	do {
		read_data = rte_read64(indirect_addrs);
		if ((read_data >> 32) == 1)
			break;
		i++;
	} while (i <= try_cnt);
	if (i > try_cnt)
		return -1;

	*rd_data = rte_le_to_cpu_32(read_data);
	return 0;
}

static int
ipn3ke_indirect_write(struct ipn3ke_hw *hw, uint32_t wr_data,
	uint32_t addr, uint32_t dev_sel, uint32_t eth_group_sel)
{
	volatile void *indirect_addrs;
	uint64_t indirect_value;
	uint64_t target_addr;

	if (eth_group_sel != 0 && eth_group_sel != 1)
		return -1;

	target_addr = addr | dev_sel << 17;

	indirect_value = WCMD | target_addr << 32 | wr_data;
	indirect_addrs = hw->eth_group_bar[eth_group_sel] + 0x10;

	rte_write64((rte_cpu_to_le_64(indirect_value)), indirect_addrs);
	return 0;
}

static int
ipn3ke_indirect_mac_read(struct ipn3ke_hw *hw, uint32_t *rd_data,
	uint32_t addr, uint32_t mac_num, uint32_t eth_group_sel)
{
	uint32_t dev_sel;

	if (mac_num >= hw->port_num)
		return -1;

	mac_num &= 0x7;
	dev_sel = mac_num * 2 + 3;

	return ipn3ke_indirect_read(hw, rd_data, addr, dev_sel, eth_group_sel);
}

static int
ipn3ke_indirect_mac_write(struct ipn3ke_hw *hw, uint32_t wr_data,
	uint32_t addr, uint32_t mac_num, uint32_t eth_group_sel)
{
	uint32_t dev_sel;

	if (mac_num >= hw->port_num)
		return -1;

	mac_num &= 0x7;
	dev_sel = mac_num * 2 + 3;

	return ipn3ke_indirect_write(hw, wr_data, addr, dev_sel, eth_group_sel);
}

static void
ipn3ke_hw_cap_init(struct ipn3ke_hw *hw)
{
	hw->hw_cap.version_number = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0), 0, 0xFFFF);
	hw->hw_cap.capability_registers_block_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x8), 0, 0xFFFFFFFF);
	hw->hw_cap.status_registers_block_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x10), 0, 0xFFFFFFFF);
	hw->hw_cap.control_registers_block_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x18), 0, 0xFFFFFFFF);
	hw->hw_cap.classify_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x20), 0, 0xFFFFFFFF);
	hw->hw_cap.classy_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x24), 0, 0xFFFF);
	hw->hw_cap.policer_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x28), 0, 0xFFFFFFFF);
	hw->hw_cap.policer_entry_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x2C), 0, 0xFFFF);
	hw->hw_cap.rss_key_array_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x30), 0, 0xFFFFFFFF);
	hw->hw_cap.rss_key_entry_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x34), 0, 0xFFFF);
	hw->hw_cap.rss_indirection_table_array_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x38), 0, 0xFFFFFFFF);
	hw->hw_cap.rss_indirection_table_entry_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x3C), 0, 0xFFFF);
	hw->hw_cap.dmac_map_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x40), 0, 0xFFFFFFFF);
	hw->hw_cap.dmac_map_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x44), 0, 0xFFFF);
	hw->hw_cap.qm_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x48), 0, 0xFFFFFFFF);
	hw->hw_cap.qm_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x4C), 0, 0xFFFF);
	hw->hw_cap.ccb_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x50), 0, 0xFFFFFFFF);
	hw->hw_cap.ccb_entry_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x54), 0, 0xFFFF);
	hw->hw_cap.qos_offset = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x58), 0, 0xFFFFFFFF);
	hw->hw_cap.qos_size = IPN3KE_MASK_READ_REG(hw,
			(IPN3KE_HW_BASE + 0x5C), 0, 0xFFFF);

	hw->hw_cap.num_rx_flow = IPN3KE_MASK_READ_REG(hw,
			IPN3KE_CAPABILITY_REGISTERS_BLOCK_OFFSET,
			0, 0xFFFF);
	hw->hw_cap.num_rss_blocks = IPN3KE_MASK_READ_REG(hw,
			IPN3KE_CAPABILITY_REGISTERS_BLOCK_OFFSET,
			4, 0xFFFF);
	hw->hw_cap.num_dmac_map = IPN3KE_MASK_READ_REG(hw,
			IPN3KE_CAPABILITY_REGISTERS_BLOCK_OFFSET,
			8, 0xFFFF);
	hw->hw_cap.num_tx_flow = IPN3KE_MASK_READ_REG(hw,
			IPN3KE_CAPABILITY_REGISTERS_BLOCK_OFFSET,
			0xC, 0xFFFF);
	hw->hw_cap.num_smac_map = IPN3KE_MASK_READ_REG(hw,
			IPN3KE_CAPABILITY_REGISTERS_BLOCK_OFFSET,
			0x10, 0xFFFF);

	hw->hw_cap.link_speed_mbps = IPN3KE_MASK_READ_REG(hw,
			IPN3KE_STATUS_REGISTERS_BLOCK_OFFSET,
			0, 0xFFFFF);
}

static int
ipn3ke_vbng_init_done(struct ipn3ke_hw *hw)
{
	uint32_t timeout = 10000;
	while (timeout > 0) {
		if (IPN3KE_READ_REG(hw, IPN3KE_VBNG_INIT_STS)
			== IPN3KE_VBNG_INIT_DONE)
			break;
		rte_delay_us(1000);
		timeout--;
	}

	if (!timeout) {
		IPN3KE_AFU_PMD_ERR("IPN3KE vBNG INIT timeout.\n");
		return -1;
	}

	return 0;
}

static uint32_t
ipn3ke_mtu_cal(uint32_t tx, uint32_t rx)
{
	uint32_t tmp;
	tmp = RTE_MIN(tx, rx);
	tmp = RTE_MAX(tmp, (uint32_t)RTE_ETHER_MIN_MTU);
	tmp = RTE_MIN(tmp, (uint32_t)(IPN3KE_MAC_FRAME_SIZE_MAX -
		IPN3KE_ETH_OVERHEAD));
	return tmp;
}

static void
ipn3ke_mtu_set(struct ipn3ke_hw *hw, uint32_t mac_num,
	uint32_t eth_group_sel, uint32_t txaddr, uint32_t rxaddr)
{
	uint32_t tx;
	uint32_t rx;
	uint32_t tmp;

	if (!(*hw->f_mac_read) || !(*hw->f_mac_write))
		return;

	(*hw->f_mac_read)(hw,
			&tx,
			txaddr,
			mac_num,
			eth_group_sel);

	(*hw->f_mac_read)(hw,
			&rx,
			rxaddr,
			mac_num,
			eth_group_sel);

	tmp = ipn3ke_mtu_cal(tx, rx);

	(*hw->f_mac_write)(hw,
			tmp,
			txaddr,
			mac_num,
			eth_group_sel);

	(*hw->f_mac_write)(hw,
			tmp,
			rxaddr,
			mac_num,
			eth_group_sel);
}

static void
ipn3ke_10G_mtu_setup(struct ipn3ke_hw *hw, uint32_t mac_num,
	uint32_t eth_group_sel)
{
	ipn3ke_mtu_set(hw, mac_num, eth_group_sel,
		IPN3KE_10G_TX_FRAME_MAXLENGTH, IPN3KE_10G_RX_FRAME_MAXLENGTH);
}

static void
ipn3ke_25G_mtu_setup(struct ipn3ke_hw *hw, uint32_t mac_num,
	uint32_t eth_group_sel)
{
	ipn3ke_mtu_set(hw, mac_num, eth_group_sel,
		IPN3KE_25G_MAX_TX_SIZE_CONFIG, IPN3KE_25G_MAX_RX_SIZE_CONFIG);
}

static void
ipn3ke_mtu_setup(struct ipn3ke_hw *hw)
{
	int i;
	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		for (i = 0; i < hw->port_num; i++) {
			ipn3ke_10G_mtu_setup(hw, i, 0);
			ipn3ke_10G_mtu_setup(hw, i, 1);
		}
	} else if (hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) {
		for (i = 0; i < hw->port_num; i++) {
			ipn3ke_25G_mtu_setup(hw, i, 0);
			ipn3ke_25G_mtu_setup(hw, i, 1);
		}
	}
}

static int
ipn3ke_hw_init(struct rte_afu_device *afu_dev,
	struct ipn3ke_hw *hw)
{
	struct rte_rawdev *rawdev;
	int ret;
	int i;
	uint64_t port_num, mac_type, index;

	rawdev  = afu_dev->rawdev;

	hw->afu_id.uuid.uuid_low = afu_dev->id.uuid.uuid_low;
	hw->afu_id.uuid.uuid_high = afu_dev->id.uuid.uuid_high;
	hw->afu_id.port = afu_dev->id.port;
	hw->hw_addr = (uint8_t *)(afu_dev->mem_resource[0].addr);
	hw->f_mac_read = ipn3ke_indirect_mac_read;
	hw->f_mac_write = ipn3ke_indirect_mac_write;
	hw->rawdev = rawdev;
	rawdev->dev_ops->attr_get(rawdev,
				"LineSideBARIndex", &index);
	hw->eth_group_bar[0] = (uint8_t *)(afu_dev->mem_resource[index].addr);
	rawdev->dev_ops->attr_get(rawdev,
				"NICSideBARIndex", &index);
	hw->eth_group_bar[1] = (uint8_t *)(afu_dev->mem_resource[index].addr);
	rawdev->dev_ops->attr_get(rawdev,
				"LineSideLinkPortNum", &port_num);
	hw->retimer.port_num = (int)port_num;
	hw->port_num = hw->retimer.port_num;
	rawdev->dev_ops->attr_get(rawdev,
				"LineSideMACType", &mac_type);
	hw->retimer.mac_type = (int)mac_type;

	hw->acc_tm = 0;
	hw->acc_flow = 0;

	if (afu_dev->id.uuid.uuid_low == IPN3KE_UUID_VBNG_LOW &&
		afu_dev->id.uuid.uuid_high == IPN3KE_UUID_VBNG_HIGH) {
		/* After power on, wait until init done */
		if (ipn3ke_vbng_init_done(hw))
			return -1;

		ipn3ke_hw_cap_init(hw);

		/* Reset vBNG IP */
		IPN3KE_WRITE_REG(hw, IPN3KE_CTRL_RESET, 1);
		rte_delay_us(10);
		IPN3KE_WRITE_REG(hw, IPN3KE_CTRL_RESET, 0);

		/* After reset, wait until init done */
		if (ipn3ke_vbng_init_done(hw))
			return -1;

		hw->acc_tm = 1;
		hw->acc_flow = 1;

		IPN3KE_AFU_PMD_DEBUG("UPL_version is 0x%x\n",
			IPN3KE_READ_REG(hw, 0));
	}

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Enable inter connect channel */
		for (i = 0; i < hw->port_num; i++) {
			/* Enable the TX path */
			ipn3ke_xmac_tx_enable(hw, i, 1);

			/* Disables source address override */
			ipn3ke_xmac_smac_ovd_dis(hw, i, 1);

			/* Enable the RX path */
			ipn3ke_xmac_rx_enable(hw, i, 1);

			/* Clear NIC side TX statistics counters */
			ipn3ke_xmac_tx_clr_10G_stcs(hw, i, 1);

			/* Clear NIC side RX statistics counters */
			ipn3ke_xmac_rx_clr_10G_stcs(hw, i, 1);

			/* Clear line side TX statistics counters */
			ipn3ke_xmac_tx_clr_10G_stcs(hw, i, 0);

			/* Clear line RX statistics counters */
			ipn3ke_xmac_rx_clr_10G_stcs(hw, i, 0);
		}
	} else if (hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) {
		/* Enable inter connect channel */
		for (i = 0; i < hw->port_num; i++) {
			/* Clear NIC side TX statistics counters */
			ipn3ke_xmac_tx_clr_25G_stcs(hw, i, 1);

			/* Clear NIC side RX statistics counters */
			ipn3ke_xmac_rx_clr_25G_stcs(hw, i, 1);

			/* Clear line side TX statistics counters */
			ipn3ke_xmac_tx_clr_25G_stcs(hw, i, 0);

			/* Clear line side RX statistics counters */
			ipn3ke_xmac_rx_clr_25G_stcs(hw, i, 0);
		}
	}

	/* init mtu */
	ipn3ke_mtu_setup(hw);

	ret = rte_eth_switch_domain_alloc(&hw->switch_domain_id);
	if (ret)
		IPN3KE_AFU_PMD_WARN("failed to allocate switch domain for device %d",
		ret);

	hw->tm_hw_enable = 0;
	hw->flow_hw_enable = 0;
	if (afu_dev->id.uuid.uuid_low == IPN3KE_UUID_VBNG_LOW &&
		afu_dev->id.uuid.uuid_high == IPN3KE_UUID_VBNG_HIGH) {
		ret = ipn3ke_hw_tm_init(hw);
		if (ret)
			return ret;
		hw->tm_hw_enable = 1;

		ret = ipn3ke_flow_init(hw);
		if (ret)
			return ret;
		hw->flow_hw_enable = 1;
	}

	return 0;
}

static void
ipn3ke_hw_uninit(struct ipn3ke_hw *hw)
{
	int i;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		for (i = 0; i < hw->port_num; i++) {
			/* Disable the TX path */
			ipn3ke_xmac_tx_disable(hw, i, 1);

			/* Disable the RX path */
			ipn3ke_xmac_rx_disable(hw, i, 1);

			/* Clear NIC side TX statistics counters */
			ipn3ke_xmac_tx_clr_10G_stcs(hw, i, 1);

			/* Clear NIC side RX statistics counters */
			ipn3ke_xmac_rx_clr_10G_stcs(hw, i, 1);

			/* Clear line side TX statistics counters */
			ipn3ke_xmac_tx_clr_10G_stcs(hw, i, 0);

			/* Clear line side RX statistics counters */
			ipn3ke_xmac_rx_clr_10G_stcs(hw, i, 0);
		}
	} else if (hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) {
		for (i = 0; i < hw->port_num; i++) {
			/* Clear NIC side TX statistics counters */
			ipn3ke_xmac_tx_clr_25G_stcs(hw, i, 1);

			/* Clear NIC side RX statistics counters */
			ipn3ke_xmac_rx_clr_25G_stcs(hw, i, 1);

			/* Clear line side TX statistics counters */
			ipn3ke_xmac_tx_clr_25G_stcs(hw, i, 0);

			/* Clear line side RX statistics counters */
			ipn3ke_xmac_rx_clr_25G_stcs(hw, i, 0);
		}
	}
}

static int ipn3ke_vswitch_probe(struct rte_afu_device *afu_dev)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct ipn3ke_hw *hw;
	struct rte_eth_dev *i40e_eth;
	struct ifpga_rawdev *ifpga_dev;
	uint16_t port_id;
	int i, j, retval;
	char *fvl_bdf;

	/* check if the AFU device has been probed already */
	/* allocate shared mcp_vswitch structure */
	if (!afu_dev->shared.data) {
		snprintf(name, sizeof(name), "net_%s_hw",
			afu_dev->device.name);
		hw = rte_zmalloc_socket(name,
					sizeof(struct ipn3ke_hw),
					RTE_CACHE_LINE_SIZE,
					afu_dev->device.numa_node);
		if (!hw) {
			IPN3KE_AFU_PMD_ERR("failed to allocate hardwart data");
				retval = -ENOMEM;
				return -ENOMEM;
		}
		afu_dev->shared.data = hw;

		rte_spinlock_init(&afu_dev->shared.lock);
	} else {
		hw = afu_dev->shared.data;
	}

	retval = ipn3ke_hw_init(afu_dev, hw);
	if (retval)
		return retval;

	if (ipn3ke_bridge_func.get_ifpga_rawdev == NULL)
		return -ENOMEM;
	ifpga_dev = ipn3ke_bridge_func.get_ifpga_rawdev(hw->rawdev);
		if (!ifpga_dev)
			IPN3KE_AFU_PMD_ERR("failed to find ifpga_device.");

	/* probe representor ports */
	j = 0;
	for (i = 0; i < hw->port_num; i++) {
		struct ipn3ke_rpst rpst = {
			.port_id = i,
			.switch_domain_id = hw->switch_domain_id,
			.hw = hw
		};

		/* representor port net_bdf_port */
		snprintf(name, sizeof(name), "net_%s_representor_%d",
			afu_dev->device.name, i);

		for (; j < 8; j++) {
			fvl_bdf = ifpga_dev->fvl_bdf[j];
			retval = rte_eth_dev_get_port_by_name(fvl_bdf,
				&port_id);
			if (retval) {
				continue;
			} else {
				i40e_eth = &rte_eth_devices[port_id];
				rpst.i40e_pf_eth = i40e_eth;
				rpst.i40e_pf_eth_port_id = port_id;

				j++;
				break;
			}
		}

		retval = rte_eth_dev_create(&afu_dev->device, name,
			sizeof(struct ipn3ke_rpst), NULL, NULL,
			ipn3ke_rpst_init, &rpst);

		if (retval)
			IPN3KE_AFU_PMD_ERR("failed to create ipn3ke representor %s.",
								name);

	}

	return 0;
}

static int ipn3ke_vswitch_remove(struct rte_afu_device *afu_dev)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct ipn3ke_hw *hw;
	struct rte_eth_dev *ethdev;
	int i, ret;

	hw = afu_dev->shared.data;

	/* remove representor ports */
	for (i = 0; i < hw->port_num; i++) {
		/* representor port net_bdf_port */
		snprintf(name, sizeof(name), "net_%s_representor_%d",
			afu_dev->device.name, i);

		ethdev = rte_eth_dev_allocated(afu_dev->device.name);
		if (!ethdev)
			return -ENODEV;

		rte_eth_dev_destroy(ethdev, ipn3ke_rpst_uninit);
	}

	ret = rte_eth_switch_domain_free(hw->switch_domain_id);
	if (ret)
		IPN3KE_AFU_PMD_WARN("failed to free switch domain: %d", ret);

	/* hw uninit*/
	ipn3ke_hw_uninit(hw);

	return 0;
}

static struct rte_afu_driver afu_ipn3ke_driver = {
	.id_table = afu_uuid_ipn3ke_map,
	.probe = ipn3ke_vswitch_probe,
	.remove = ipn3ke_vswitch_remove,
};

RTE_PMD_REGISTER_AFU(net_ipn3ke_afu, afu_ipn3ke_driver);
RTE_LOG_REGISTER(ipn3ke_afu_logtype, pmd.afu.ipn3ke, NOTICE);
