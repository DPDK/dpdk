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

#include "ipn3ke_rawdev_api.h"
#include "ipn3ke_flow.h"
#include "ipn3ke_logs.h"
#include "ipn3ke_ethdev.h"

int ipn3ke_afu_logtype;

static const struct rte_afu_uuid afu_uuid_ipn3ke_map[] = {
	{ MAP_UUID_10G_LOW,  MAP_UUID_10G_HIGH },
	{ IPN3KE_UUID_10G_LOW, IPN3KE_UUID_10G_HIGH },
	{ IPN3KE_UUID_VBNG_LOW, IPN3KE_UUID_VBNG_HIGH},
	{ IPN3KE_UUID_25G_LOW, IPN3KE_UUID_25G_HIGH },
	{ 0, 0 /* sentinel */ },
};

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

	addr &= 0x3FF;
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

	addr &= 0x3FF;
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

	if (afu_dev->id.uuid.uuid_low == IPN3KE_UUID_VBNG_LOW &&
		afu_dev->id.uuid.uuid_high == IPN3KE_UUID_VBNG_HIGH) {
		ipn3ke_hw_cap_init(hw);
		IPN3KE_AFU_PMD_DEBUG("UPL_version is 0x%x\n",
			IPN3KE_READ_REG(hw, 0));

		/* Reset FPGA IP */
		IPN3KE_WRITE_REG(hw, IPN3KE_CTRL_RESET, 1);
		IPN3KE_WRITE_REG(hw, IPN3KE_CTRL_RESET, 0);
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

			/* Clear all TX statistics counters */
			ipn3ke_xmac_tx_clr_stcs(hw, i, 1);

			/* Clear all RX statistics counters */
			ipn3ke_xmac_rx_clr_stcs(hw, i, 1);
		}
	}

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

	hw->acc_tm = 0;
	hw->acc_flow = 0;

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

			/* Clear all TX statistics counters */
			ipn3ke_xmac_tx_clr_stcs(hw, i, 1);

			/* Clear all RX statistics counters */
			ipn3ke_xmac_rx_clr_stcs(hw, i, 1);
		}
	}
}

static int ipn3ke_vswitch_probe(struct rte_afu_device *afu_dev)
{
	char name[RTE_ETH_NAME_MAX_LEN];
	struct ipn3ke_hw *hw;
	int i, retval;

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

	/* probe representor ports */
	for (i = 0; i < hw->port_num; i++) {
		struct ipn3ke_rpst rpst = {
			.port_id = i,
			.switch_domain_id = hw->switch_domain_id,
			.hw = hw
		};

		/* representor port net_bdf_port */
		snprintf(name, sizeof(name), "net_%s_representor_%d",
			afu_dev->device.name, i);

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

static const char * const valid_args[] = {
#define IPN3KE_AFU_NAME         "afu"
		IPN3KE_AFU_NAME,
#define IPN3KE_FPGA_ACCELERATION_LIST     "fpga_acc"
		IPN3KE_FPGA_ACCELERATION_LIST,
#define IPN3KE_I40E_PF_LIST     "i40e_pf"
		IPN3KE_I40E_PF_LIST,
		NULL
};

static int
ipn3ke_cfg_parse_acc_list(const char *afu_name,
	const char *acc_list_name)
{
	struct rte_afu_device *afu_dev;
	struct ipn3ke_hw *hw;
	const char *p_source;
	char *p_start;
	char name[RTE_ETH_NAME_MAX_LEN];

	afu_dev = rte_ifpga_find_afu_by_name(afu_name);
	if (!afu_dev)
		return -1;
	hw = afu_dev->shared.data;
	if (!hw)
		return -1;

	p_source = acc_list_name;
	while (*p_source) {
		while ((*p_source == '{') || (*p_source == '|'))
			p_source++;
		p_start = name;
		while ((*p_source != '|') && (*p_source != '}'))
			*p_start++ = *p_source++;
		*p_start = 0;
		if (!strcmp(name, "tm") && hw->tm_hw_enable)
			hw->acc_tm = 1;

		if (!strcmp(name, "flow") && hw->flow_hw_enable)
			hw->acc_flow = 1;

		if (*p_source == '}')
			return 0;
	}

	return 0;
}

static int
ipn3ke_cfg_parse_i40e_pf_ethdev(const char *afu_name,
	const char *pf_name)
{
	struct rte_eth_dev *i40e_eth, *rpst_eth;
	struct rte_afu_device *afu_dev;
	struct ipn3ke_rpst *rpst;
	struct ipn3ke_hw *hw;
	const char *p_source;
	char *p_start;
	char name[RTE_ETH_NAME_MAX_LEN];
	uint16_t port_id;
	int i;
	int ret = -1;

	afu_dev = rte_ifpga_find_afu_by_name(afu_name);
	if (!afu_dev)
		return -1;
	hw = afu_dev->shared.data;
	if (!hw)
		return -1;

	p_source = pf_name;
	for (i = 0; i < hw->port_num; i++) {
		snprintf(name, sizeof(name), "net_%s_representor_%d",
			afu_name, i);
		ret = rte_eth_dev_get_port_by_name(name, &port_id);
		if (ret)
			return -1;
		rpst_eth = &rte_eth_devices[port_id];
		rpst = IPN3KE_DEV_PRIVATE_TO_RPST(rpst_eth);

		while ((*p_source == '{') || (*p_source == '|'))
			p_source++;
		p_start = name;
		while ((*p_source != '|') && (*p_source != '}'))
			*p_start++ = *p_source++;
		*p_start = 0;

		ret = rte_eth_dev_get_port_by_name(name, &port_id);
		if (ret)
			return -1;
		i40e_eth = &rte_eth_devices[port_id];

		rpst->i40e_pf_eth = i40e_eth;
		rpst->i40e_pf_eth_port_id = port_id;

		if ((*p_source == '}') || !(*p_source))
			break;
	}

	return 0;
}

static int
ipn3ke_cfg_probe(struct rte_vdev_device *dev)
{
	struct rte_devargs *devargs;
	struct rte_kvargs *kvlist = NULL;
	char *afu_name = NULL;
	char *acc_name = NULL;
	char *pf_name = NULL;
	int afu_name_en = 0;
	int acc_list_en = 0;
	int pf_list_en = 0;
	int ret = -1;

	devargs = dev->device.devargs;

	kvlist = rte_kvargs_parse(devargs->args, valid_args);
	if (!kvlist) {
		IPN3KE_AFU_PMD_ERR("error when parsing param");
		goto end;
	}

	if (rte_kvargs_count(kvlist, IPN3KE_AFU_NAME) == 1) {
		if (rte_kvargs_process(kvlist, IPN3KE_AFU_NAME,
				       &rte_ifpga_get_string_arg,
				       &afu_name) < 0) {
			IPN3KE_AFU_PMD_ERR("error to parse %s",
				     IPN3KE_AFU_NAME);
			goto end;
		} else {
			afu_name_en = 1;
		}
	}

	if (rte_kvargs_count(kvlist, IPN3KE_FPGA_ACCELERATION_LIST) == 1) {
		if (rte_kvargs_process(kvlist, IPN3KE_FPGA_ACCELERATION_LIST,
				       &rte_ifpga_get_string_arg,
				       &acc_name) < 0) {
			IPN3KE_AFU_PMD_ERR("error to parse %s",
				     IPN3KE_FPGA_ACCELERATION_LIST);
			goto end;
		} else {
			acc_list_en = 1;
		}
	}

	if (rte_kvargs_count(kvlist, IPN3KE_I40E_PF_LIST) == 1) {
		if (rte_kvargs_process(kvlist, IPN3KE_I40E_PF_LIST,
				       &rte_ifpga_get_string_arg,
				       &pf_name) < 0) {
			IPN3KE_AFU_PMD_ERR("error to parse %s",
				     IPN3KE_I40E_PF_LIST);
			goto end;
		} else {
			pf_list_en = 1;
		}
	}

	if (!afu_name_en) {
		IPN3KE_AFU_PMD_ERR("arg %s is mandatory for ipn3ke",
			  IPN3KE_AFU_NAME);
		goto end;
	}

	if (!pf_list_en) {
		IPN3KE_AFU_PMD_ERR("arg %s is mandatory for ipn3ke",
			  IPN3KE_I40E_PF_LIST);
		goto end;
	}

	if (acc_list_en) {
		ret = ipn3ke_cfg_parse_acc_list(afu_name, acc_name);
		if (ret) {
			IPN3KE_AFU_PMD_ERR("arg %s parse error for ipn3ke",
			  IPN3KE_FPGA_ACCELERATION_LIST);
			goto end;
		}
	} else {
		IPN3KE_AFU_PMD_INFO("arg %s is optional for ipn3ke, using i40e acc",
			  IPN3KE_FPGA_ACCELERATION_LIST);
	}

	ret = ipn3ke_cfg_parse_i40e_pf_ethdev(afu_name, pf_name);
	if (ret)
		goto end;
end:
	if (kvlist)
		rte_kvargs_free(kvlist);
	if (afu_name)
		free(afu_name);
	if (acc_name)
		free(acc_name);

	return ret;
}

static int
ipn3ke_cfg_remove(struct rte_vdev_device *dev)
{
	struct rte_devargs *devargs;
	struct rte_kvargs *kvlist = NULL;
	char *afu_name = NULL;
	struct rte_afu_device *afu_dev;
	int ret = -1;

	devargs = dev->device.devargs;

	kvlist = rte_kvargs_parse(devargs->args, valid_args);
	if (!kvlist) {
		IPN3KE_AFU_PMD_ERR("error when parsing param");
		goto end;
	}

	if (rte_kvargs_count(kvlist, IPN3KE_AFU_NAME) == 1) {
		if (rte_kvargs_process(kvlist, IPN3KE_AFU_NAME,
				       &rte_ifpga_get_string_arg,
				       &afu_name) < 0) {
			IPN3KE_AFU_PMD_ERR("error to parse %s",
				     IPN3KE_AFU_NAME);
		} else {
			afu_dev = rte_ifpga_find_afu_by_name(afu_name);
			if (!afu_dev)
				goto end;
			ret = ipn3ke_vswitch_remove(afu_dev);
		}
	} else {
		IPN3KE_AFU_PMD_ERR("Remove ipn3ke_cfg %p error", dev);
	}

end:
	if (kvlist)
		rte_kvargs_free(kvlist);

	return ret;
}

static struct rte_vdev_driver ipn3ke_cfg_driver = {
	.probe = ipn3ke_cfg_probe,
	.remove = ipn3ke_cfg_remove,
};

RTE_PMD_REGISTER_VDEV(ipn3ke_cfg, ipn3ke_cfg_driver);
RTE_PMD_REGISTER_PARAM_STRING(ipn3ke_cfg,
	"afu=<string> "
	"fpga_acc=<string>"
	"i40e_pf=<string>");

RTE_INIT(ipn3ke_afu_init_log)
{
	ipn3ke_afu_logtype = rte_log_register("pmd.afu.ipn3ke");
	if (ipn3ke_afu_logtype >= 0)
		rte_log_set_level(ipn3ke_afu_logtype, RTE_LOG_NOTICE);
}
