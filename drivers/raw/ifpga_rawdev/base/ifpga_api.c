/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_api.h"
#include "ifpga_enumerate.h"
#include "ifpga_feature_dev.h"

#include "opae_hw_api.h"

/* Accelerator APIs */
static int ifpga_acc_get_uuid(struct opae_accelerator *acc,
			      struct uuid *uuid)
{
	struct opae_bridge *br = acc->br;
	struct ifpga_port_hw *port;

	if (!br || !br->data)
		return -EINVAL;

	port = br->data;

	return fpga_get_afu_uuid(port, uuid);
}

static int ifpga_acc_set_irq(struct opae_accelerator *acc,
			     u32 start, u32 count, s32 evtfds[])
{
	struct ifpga_afu_info *afu_info = acc->data;
	struct opae_bridge *br = acc->br;
	struct ifpga_port_hw *port;
	struct fpga_uafu_irq_set irq_set;

	if (!br || !br->data)
		return -EINVAL;

	if (start >= afu_info->num_irqs || start + count > afu_info->num_irqs)
		return -EINVAL;

	port = br->data;

	irq_set.start = start;
	irq_set.count = count;
	irq_set.evtfds = evtfds;

	return ifpga_set_irq(port->parent, FEATURE_FIU_ID_PORT, port->port_id,
			     IFPGA_PORT_FEATURE_ID_UINT, &irq_set);
}

static int ifpga_acc_get_info(struct opae_accelerator *acc,
			      struct opae_acc_info *info)
{
	struct ifpga_afu_info *afu_info = acc->data;

	if (!afu_info)
		return -ENODEV;

	info->num_regions = afu_info->num_regions;
	info->num_irqs = afu_info->num_irqs;

	return 0;
}

static int ifpga_acc_get_region_info(struct opae_accelerator *acc,
				     struct opae_acc_region_info *info)
{
	struct ifpga_afu_info *afu_info = acc->data;

	if (!afu_info)
		return -EINVAL;

	if (info->index >= afu_info->num_regions)
		return -EINVAL;

	/* always one RW region only for AFU now */
	info->flags = ACC_REGION_READ | ACC_REGION_WRITE | ACC_REGION_MMIO;
	info->len = afu_info->region[info->index].len;
	info->addr = afu_info->region[info->index].addr;

	return 0;
}

static int ifpga_acc_read(struct opae_accelerator *acc, unsigned int region_idx,
			  u64 offset, unsigned int byte, void *data)
{
	struct ifpga_afu_info *afu_info = acc->data;
	struct opae_reg_region *region;

	if (!afu_info)
		return -EINVAL;

	if (offset + byte <= offset)
		return -EINVAL;

	if (region_idx >= afu_info->num_regions)
		return -EINVAL;

	region = &afu_info->region[region_idx];
	if (offset + byte > region->len)
		return -EINVAL;

	switch (byte) {
	case 8:
		*(u64  *)data = opae_readq(region->addr + offset);
		break;
	case 4:
		*(u32 *)data = opae_readl(region->addr + offset);
		break;
	case 2:
		*(u16 *)data = opae_readw(region->addr + offset);
		break;
	case 1:
		*(u8 *)data = opae_readb(region->addr + offset);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

static int ifpga_acc_write(struct opae_accelerator *acc,
			   unsigned int region_idx, u64 offset,
			   unsigned int byte, void *data)
{
	struct ifpga_afu_info *afu_info = acc->data;
	struct opae_reg_region *region;

	if (!afu_info)
		return -EINVAL;

	if (offset + byte <= offset)
		return -EINVAL;

	if (region_idx >= afu_info->num_regions)
		return -EINVAL;

	region = &afu_info->region[region_idx];
	if (offset + byte > region->len)
		return -EINVAL;

	/* normal mmio case */
	switch (byte) {
	case 8:
		opae_writeq(*(u64 *)data, region->addr + offset);
		break;
	case 4:
		opae_writel(*(u32 *)data, region->addr + offset);
		break;
	case 2:
		opae_writew(*(u16 *)data, region->addr + offset);
		break;
	case 1:
		opae_writeb(*(u8 *)data, region->addr + offset);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

struct opae_accelerator_ops ifpga_acc_ops = {
	.read = ifpga_acc_read,
	.write = ifpga_acc_write,
	.set_irq = ifpga_acc_set_irq,
	.get_info = ifpga_acc_get_info,
	.get_region_info = ifpga_acc_get_region_info,
	.get_uuid = ifpga_acc_get_uuid,
};

/* Bridge APIs */
static int ifpga_br_reset(struct opae_bridge *br)
{
	struct ifpga_port_hw *port = br->data;

	return fpga_port_reset(port);
}

struct opae_bridge_ops ifpga_br_ops = {
	.reset = ifpga_br_reset,
};

/* Manager APIs */
static int ifpga_mgr_flash(struct opae_manager *mgr, int id, void *buf,
			   u32 size, u64 *status)
{
	struct ifpga_fme_hw *fme = mgr->data;
	struct ifpga_hw *hw = fme->parent;

	return ifpga_pr(hw, id, buf, size, status);
}

static int ifpga_mgr_get_eth_group_region_info(struct opae_manager *mgr,
		struct opae_eth_group_region_info *info)
{
	struct ifpga_fme_hw *fme = mgr->data;

	if (info->group_id >= MAX_ETH_GROUP_DEVICES)
		return -EINVAL;

	info->phys_addr = fme->eth_group_region[info->group_id].phys_addr;
	info->addr = fme->eth_group_region[info->group_id].addr;
	info->len = fme->eth_group_region[info->group_id].len;

	info->mem_idx = fme->nums_acc_region + info->group_id;

	return 0;
}

struct opae_manager_ops ifpga_mgr_ops = {
	.flash = ifpga_mgr_flash,
	.get_eth_group_region_info = ifpga_mgr_get_eth_group_region_info,
};

static int ifpga_mgr_read_mac_rom(struct opae_manager *mgr, int offset,
		void *buf, int size)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_read_mac_rom(fme, offset, buf, size);
}

static int ifpga_mgr_write_mac_rom(struct opae_manager *mgr, int offset,
		void *buf, int size)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_write_mac_rom(fme, offset, buf, size);
}

static int ifpga_mgr_get_eth_group_nums(struct opae_manager *mgr)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_get_eth_group_nums(fme);
}

static int ifpga_mgr_get_eth_group_info(struct opae_manager *mgr,
		u8 group_id, struct opae_eth_group_info *info)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_get_eth_group_info(fme, group_id, info);
}

static int ifpga_mgr_eth_group_reg_read(struct opae_manager *mgr, u8 group_id,
		u8 type, u8 index, u16 addr, u32 *data)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_eth_group_read_reg(fme, group_id,
			type, index, addr, data);
}

static int ifpga_mgr_eth_group_reg_write(struct opae_manager *mgr, u8 group_id,
		u8 type, u8 index, u16 addr, u32 data)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_eth_group_write_reg(fme, group_id,
			type, index, addr, data);
}

static int ifpga_mgr_get_retimer_info(struct opae_manager *mgr,
		struct opae_retimer_info *info)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_get_retimer_info(fme, info);
}

static int ifpga_mgr_get_retimer_status(struct opae_manager *mgr,
		struct opae_retimer_status *status)
{
	struct ifpga_fme_hw *fme = mgr->data;

	return fme_mgr_get_retimer_status(fme, status);
}

/* Network APIs in FME */
struct opae_manager_networking_ops ifpga_mgr_network_ops = {
	.read_mac_rom = ifpga_mgr_read_mac_rom,
	.write_mac_rom = ifpga_mgr_write_mac_rom,
	.get_eth_group_nums = ifpga_mgr_get_eth_group_nums,
	.get_eth_group_info = ifpga_mgr_get_eth_group_info,
	.eth_group_reg_read = ifpga_mgr_eth_group_reg_read,
	.eth_group_reg_write = ifpga_mgr_eth_group_reg_write,
	.get_retimer_info = ifpga_mgr_get_retimer_info,
	.get_retimer_status = ifpga_mgr_get_retimer_status,
};

/* Adapter APIs */
static int ifpga_adapter_enumerate(struct opae_adapter *adapter)
{
	struct ifpga_hw *hw = malloc(sizeof(*hw));

	if (hw) {
		opae_memset(hw, 0, sizeof(*hw));
		hw->pci_data = adapter->data;
		hw->adapter = adapter;
		if (ifpga_bus_enumerate(hw))
			goto error;
		return ifpga_bus_init(hw);
	}

error:
	return -ENOMEM;
}

struct opae_adapter_ops ifpga_adapter_ops = {
	.enumerate = ifpga_adapter_enumerate,
};

/**
 *  ifpga_pr - do the partial reconfiguration for a given port device
 *  @hw: pointer to the HW structure
 *  @port_id: the port device id
 *  @buffer: the buffer of the bitstream
 *  @size: the size of the bitstream
 *  @status: hardware status including PR error code if return -EIO.
 *
 *  @return
 *   - 0: Success, partial reconfiguration finished.
 *   - <0: Error code returned in partial reconfiguration.
 **/
int ifpga_pr(struct ifpga_hw *hw, u32 port_id, void *buffer, u32 size,
	     u64 *status)
{
	if (!is_valid_port_id(hw, port_id))
		return -ENODEV;

	return do_pr(hw, port_id, buffer, size, status);
}

int ifpga_get_prop(struct ifpga_hw *hw, u32 fiu_id, u32 port_id,
		   struct feature_prop *prop)
{
	if (!hw || !prop)
		return -EINVAL;

	switch (fiu_id) {
	case FEATURE_FIU_ID_FME:
		return fme_get_prop(&hw->fme, prop);
	case FEATURE_FIU_ID_PORT:
		if (!is_valid_port_id(hw, port_id))
			return -ENODEV;
		return port_get_prop(&hw->port[port_id], prop);
	}

	return -ENOENT;
}

int ifpga_set_prop(struct ifpga_hw *hw, u32 fiu_id, u32 port_id,
		   struct feature_prop *prop)
{
	if (!hw || !prop)
		return -EINVAL;

	switch (fiu_id) {
	case FEATURE_FIU_ID_FME:
		return fme_set_prop(&hw->fme, prop);
	case FEATURE_FIU_ID_PORT:
		if (!is_valid_port_id(hw, port_id))
			return -ENODEV;
		return port_set_prop(&hw->port[port_id], prop);
	}

	return -ENOENT;
}

int ifpga_set_irq(struct ifpga_hw *hw, u32 fiu_id, u32 port_id,
		  u32 feature_id, void *irq_set)
{
	if (!hw || !irq_set)
		return -EINVAL;

	switch (fiu_id) {
	case FEATURE_FIU_ID_FME:
		return fme_set_irq(&hw->fme, feature_id, irq_set);
	case FEATURE_FIU_ID_PORT:
		if (!is_valid_port_id(hw, port_id))
			return -ENODEV;
		return port_set_irq(&hw->port[port_id], feature_id, irq_set);
	}

	return -ENOENT;
}
