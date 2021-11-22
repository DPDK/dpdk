/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <linux/pci_regs.h>
#include <rte_alarm.h>
#include <rte_ethdev_pci.h>
#include <rte_io.h>
#include <rte_pci.h>
#include <rte_vfio.h>

#include "hns3_ethdev.h"
#include "hns3_logs.h"
#include "hns3_rxtx.h"
#include "hns3_regs.h"
#include "hns3_intr.h"
#include "hns3_dcb.h"
#include "hns3_mp.h"

#define HNS3VF_KEEP_ALIVE_INTERVAL	2000000 /* us */
#define HNS3VF_SERVICE_INTERVAL		1000000 /* us */

#define HNS3VF_RESET_WAIT_MS	20
#define HNS3VF_RESET_WAIT_CNT	2000

/* Reset related Registers */
#define HNS3_GLOBAL_RESET_BIT		0
#define HNS3_CORE_RESET_BIT		1
#define HNS3_IMP_RESET_BIT		2
#define HNS3_FUN_RST_ING_B		0

enum hns3vf_evt_cause {
	HNS3VF_VECTOR0_EVENT_RST,
	HNS3VF_VECTOR0_EVENT_MBX,
	HNS3VF_VECTOR0_EVENT_OTHER,
};

static enum hns3_reset_level hns3vf_get_reset_level(struct hns3_hw *hw,
						    uint64_t *levels);
static int hns3vf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int hns3vf_dev_configure_vlan(struct rte_eth_dev *dev);

static int hns3vf_add_mc_mac_addr(struct hns3_hw *hw,
				  struct rte_ether_addr *mac_addr);
static int hns3vf_remove_mc_mac_addr(struct hns3_hw *hw,
				     struct rte_ether_addr *mac_addr);
/* set PCI bus mastering */
static int
hns3vf_set_bus_master(const struct rte_pci_device *device, bool op)
{
	uint16_t reg;
	int ret;

	ret = rte_pci_read_config(device, &reg, sizeof(reg), PCI_COMMAND);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to read PCI offset 0x%x",
			     PCI_COMMAND);
		return ret;
	}

	if (op)
		/* set the master bit */
		reg |= PCI_COMMAND_MASTER;
	else
		reg &= ~(PCI_COMMAND_MASTER);

	return rte_pci_write_config(device, &reg, sizeof(reg), PCI_COMMAND);
}

/**
 * hns3vf_find_pci_capability - lookup a capability in the PCI capability list
 * @cap: the capability
 *
 * Return the address of the given capability within the PCI capability list.
 */
static int
hns3vf_find_pci_capability(const struct rte_pci_device *device, int cap)
{
#define MAX_PCIE_CAPABILITY 48
	uint16_t status;
	uint8_t pos;
	uint8_t id;
	int ttl;
	int ret;

	ret = rte_pci_read_config(device, &status, sizeof(status), PCI_STATUS);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to read PCI offset 0x%x", PCI_STATUS);
		return 0;
	}

	if (!(status & PCI_STATUS_CAP_LIST))
		return 0;

	ttl = MAX_PCIE_CAPABILITY;
	ret = rte_pci_read_config(device, &pos, sizeof(pos),
				  PCI_CAPABILITY_LIST);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to read PCI offset 0x%x",
			     PCI_CAPABILITY_LIST);
		return 0;
	}

	while (ttl-- && pos >= PCI_STD_HEADER_SIZEOF) {
		ret = rte_pci_read_config(device, &id, sizeof(id),
					  (pos + PCI_CAP_LIST_ID));
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to read PCI offset 0x%x",
				     (pos + PCI_CAP_LIST_ID));
			break;
		}

		if (id == 0xFF)
			break;

		if (id == cap)
			return (int)pos;

		ret = rte_pci_read_config(device, &pos, sizeof(pos),
					  (pos + PCI_CAP_LIST_NEXT));
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to read PCI offset 0x%x",
				     (pos + PCI_CAP_LIST_NEXT));
			break;
		}
	}
	return 0;
}

static int
hns3vf_enable_msix(const struct rte_pci_device *device, bool op)
{
	uint16_t control;
	int pos;
	int ret;

	pos = hns3vf_find_pci_capability(device, PCI_CAP_ID_MSIX);
	if (pos) {
		ret = rte_pci_read_config(device, &control, sizeof(control),
				    (pos + PCI_MSIX_FLAGS));
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to read PCI offset 0x%x",
				     (pos + PCI_MSIX_FLAGS));
			return -ENXIO;
		}

		if (op)
			control |= PCI_MSIX_FLAGS_ENABLE;
		else
			control &= ~PCI_MSIX_FLAGS_ENABLE;
		ret = rte_pci_write_config(device, &control, sizeof(control),
					  (pos + PCI_MSIX_FLAGS));
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "failed to write PCI offset 0x%x",
				    (pos + PCI_MSIX_FLAGS));
			return -ENXIO;
		}

		return 0;
	}

	return -ENXIO;
}

static int
hns3vf_add_uc_mac_addr(struct hns3_hw *hw, struct rte_ether_addr *mac_addr)
{
	/* mac address was checked by upper level interface */
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_UNICAST,
				HNS3_MBX_MAC_VLAN_UC_ADD, mac_addr->addr_bytes,
				RTE_ETHER_ADDR_LEN, false, NULL, 0);
	if (ret) {
		rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to add uc mac addr(%s), ret = %d",
			 mac_str, ret);
	}
	return ret;
}

static int
hns3vf_remove_uc_mac_addr(struct hns3_hw *hw, struct rte_ether_addr *mac_addr)
{
	/* mac address was checked by upper level interface */
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_UNICAST,
				HNS3_MBX_MAC_VLAN_UC_REMOVE,
				mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN,
				false, NULL, 0);
	if (ret) {
		rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to add uc mac addr(%s), ret = %d",
			 mac_str, ret);
	}
	return ret;
}

static int
hns3vf_add_mc_addr_common(struct hns3_hw *hw, struct rte_ether_addr *mac_addr)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *addr;
	int ret;
	int i;

	for (i = 0; i < hw->mc_addrs_num; i++) {
		addr = &hw->mc_addrs[i];
		/* Check if there are duplicate addresses */
		if (rte_is_same_ether_addr(addr, mac_addr)) {
			rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      addr);
			hns3_err(hw, "failed to add mc mac addr, same addrs"
				 "(%s) is added by the set_mc_mac_addr_list "
				 "API", mac_str);
			return -EINVAL;
		}
	}

	ret = hns3vf_add_mc_mac_addr(hw, mac_addr);
	if (ret) {
		rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to add mc mac addr(%s), ret = %d",
			 mac_str, ret);
	}
	return ret;
}

static int
hns3vf_add_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		    __rte_unused uint32_t idx,
		    __rte_unused uint32_t pool)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	rte_spinlock_lock(&hw->lock);

	/*
	 * In hns3 network engine adding UC and MC mac address with different
	 * commands with firmware. We need to determine whether the input
	 * address is a UC or a MC address to call different commands.
	 * By the way, it is recommended calling the API function named
	 * rte_eth_dev_set_mc_addr_list to set the MC mac address, because
	 * using the rte_eth_dev_mac_addr_add API function to set MC mac address
	 * may affect the specifications of UC mac addresses.
	 */
	if (rte_is_multicast_ether_addr(mac_addr))
		ret = hns3vf_add_mc_addr_common(hw, mac_addr);
	else
		ret = hns3vf_add_uc_mac_addr(hw, mac_addr);

	rte_spinlock_unlock(&hw->lock);
	if (ret) {
		rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to add mac addr(%s), ret = %d", mac_str,
			 ret);
	}

	return ret;
}

static void
hns3vf_remove_mac_addr(struct rte_eth_dev *dev, uint32_t idx)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	/* index will be checked by upper level rte interface */
	struct rte_ether_addr *mac_addr = &dev->data->mac_addrs[idx];
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	rte_spinlock_lock(&hw->lock);

	if (rte_is_multicast_ether_addr(mac_addr))
		ret = hns3vf_remove_mc_mac_addr(hw, mac_addr);
	else
		ret = hns3vf_remove_uc_mac_addr(hw, mac_addr);

	rte_spinlock_unlock(&hw->lock);
	if (ret) {
		rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to remove mac addr(%s), ret = %d",
			 mac_str, ret);
	}
}

static int
hns3vf_set_default_mac_addr(struct rte_eth_dev *dev,
			    struct rte_ether_addr *mac_addr)
{
#define HNS3_TWO_ETHER_ADDR_LEN (RTE_ETHER_ADDR_LEN * 2)
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_ether_addr *old_addr;
	uint8_t addr_bytes[HNS3_TWO_ETHER_ADDR_LEN]; /* for 2 MAC addresses */
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	/*
	 * It has been guaranteed that input parameter named mac_addr is valid
	 * address in the rte layer of DPDK framework.
	 */
	old_addr = (struct rte_ether_addr *)hw->mac.mac_addr;
	rte_spinlock_lock(&hw->lock);
	memcpy(addr_bytes, mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN);
	memcpy(&addr_bytes[RTE_ETHER_ADDR_LEN], old_addr->addr_bytes,
	       RTE_ETHER_ADDR_LEN);

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_UNICAST,
				HNS3_MBX_MAC_VLAN_UC_MODIFY, addr_bytes,
				HNS3_TWO_ETHER_ADDR_LEN, true, NULL, 0);
	if (ret) {
		/*
		 * The hns3 VF PMD depends on the hns3 PF kernel ethdev
		 * driver. When user has configured a MAC address for VF device
		 * by "ip link set ..." command based on the PF device, the hns3
		 * PF kernel ethdev driver does not allow VF driver to request
		 * reconfiguring a different default MAC address, and return
		 * -EPREM to VF driver through mailbox.
		 */
		if (ret == -EPERM) {
			rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      old_addr);
			hns3_warn(hw, "Has permanet mac addr(%s) for vf",
				  mac_str);
		} else {
			rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      mac_addr);
			hns3_err(hw, "Failed to set mac addr(%s) for vf: %d",
				 mac_str, ret);
		}
	}

	rte_ether_addr_copy(mac_addr,
			    (struct rte_ether_addr *)hw->mac.mac_addr);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3vf_configure_mac_addr(struct hns3_adapter *hns, bool del)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_ether_addr *addr;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int err = 0;
	int ret;
	int i;

	for (i = 0; i < HNS3_VF_UC_MACADDR_NUM; i++) {
		addr = &hw->data->mac_addrs[i];
		if (rte_is_zero_ether_addr(addr))
			continue;
		if (rte_is_multicast_ether_addr(addr))
			ret = del ? hns3vf_remove_mc_mac_addr(hw, addr) :
			      hns3vf_add_mc_mac_addr(hw, addr);
		else
			ret = del ? hns3vf_remove_uc_mac_addr(hw, addr) :
			      hns3vf_add_uc_mac_addr(hw, addr);

		if (ret) {
			err = ret;
			rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      addr);
			hns3_err(hw, "failed to %s mac addr(%s) index:%d "
				 "ret = %d.", del ? "remove" : "restore",
				 mac_str, i, ret);
		}
	}
	return err;
}

static int
hns3vf_add_mc_mac_addr(struct hns3_hw *hw,
		       struct rte_ether_addr *mac_addr)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_MULTICAST,
				HNS3_MBX_MAC_VLAN_MC_ADD,
				mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN, false,
				NULL, 0);
	if (ret) {
		rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "Failed to add mc mac addr(%s) for vf: %d",
			 mac_str, ret);
	}

	return ret;
}

static int
hns3vf_remove_mc_mac_addr(struct hns3_hw *hw,
			  struct rte_ether_addr *mac_addr)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_MULTICAST,
				HNS3_MBX_MAC_VLAN_MC_REMOVE,
				mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN, false,
				NULL, 0);
	if (ret) {
		rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "Failed to remove mc mac addr(%s) for vf: %d",
			 mac_str, ret);
	}

	return ret;
}

static int
hns3vf_set_mc_addr_chk_param(struct hns3_hw *hw,
			     struct rte_ether_addr *mc_addr_set,
			     uint32_t nb_mc_addr)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *addr;
	uint32_t i;
	uint32_t j;

	if (nb_mc_addr > HNS3_MC_MACADDR_NUM) {
		hns3_err(hw, "failed to set mc mac addr, nb_mc_addr(%u) "
			 "invalid. valid range: 0~%d",
			 nb_mc_addr, HNS3_MC_MACADDR_NUM);
		return -EINVAL;
	}

	/* Check if input mac addresses are valid */
	for (i = 0; i < nb_mc_addr; i++) {
		addr = &mc_addr_set[i];
		if (!rte_is_multicast_ether_addr(addr)) {
			rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      addr);
			hns3_err(hw,
				 "failed to set mc mac addr, addr(%s) invalid.",
				 mac_str);
			return -EINVAL;
		}

		/* Check if there are duplicate addresses */
		for (j = i + 1; j < nb_mc_addr; j++) {
			if (rte_is_same_ether_addr(addr, &mc_addr_set[j])) {
				rte_ether_format_addr(mac_str,
						      RTE_ETHER_ADDR_FMT_SIZE,
						      addr);
				hns3_err(hw, "failed to set mc mac addr, "
					 "addrs invalid. two same addrs(%s).",
					 mac_str);
				return -EINVAL;
			}
		}

		/*
		 * Check if there are duplicate addresses between mac_addrs
		 * and mc_addr_set
		 */
		for (j = 0; j < HNS3_VF_UC_MACADDR_NUM; j++) {
			if (rte_is_same_ether_addr(addr,
						   &hw->data->mac_addrs[j])) {
				rte_ether_format_addr(mac_str,
						      RTE_ETHER_ADDR_FMT_SIZE,
						      addr);
				hns3_err(hw, "failed to set mc mac addr, "
					 "addrs invalid. addrs(%s) has already "
					 "configured in mac_addr add API",
					 mac_str);
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int
hns3vf_set_mc_mac_addr_list(struct rte_eth_dev *dev,
			    struct rte_ether_addr *mc_addr_set,
			    uint32_t nb_mc_addr)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_ether_addr *addr;
	int cur_addr_num;
	int set_addr_num;
	int num;
	int ret;
	int i;

	ret = hns3vf_set_mc_addr_chk_param(hw, mc_addr_set, nb_mc_addr);
	if (ret)
		return ret;

	rte_spinlock_lock(&hw->lock);
	cur_addr_num = hw->mc_addrs_num;
	for (i = 0; i < cur_addr_num; i++) {
		num = cur_addr_num - i - 1;
		addr = &hw->mc_addrs[num];
		ret = hns3vf_remove_mc_mac_addr(hw, addr);
		if (ret) {
			rte_spinlock_unlock(&hw->lock);
			return ret;
		}

		hw->mc_addrs_num--;
	}

	set_addr_num = (int)nb_mc_addr;
	for (i = 0; i < set_addr_num; i++) {
		addr = &mc_addr_set[i];
		ret = hns3vf_add_mc_mac_addr(hw, addr);
		if (ret) {
			rte_spinlock_unlock(&hw->lock);
			return ret;
		}

		rte_ether_addr_copy(addr, &hw->mc_addrs[hw->mc_addrs_num]);
		hw->mc_addrs_num++;
	}
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3vf_configure_all_mc_mac_addr(struct hns3_adapter *hns, bool del)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct hns3_hw *hw = &hns->hw;
	struct rte_ether_addr *addr;
	int err = 0;
	int ret;
	int i;

	for (i = 0; i < hw->mc_addrs_num; i++) {
		addr = &hw->mc_addrs[i];
		if (!rte_is_multicast_ether_addr(addr))
			continue;
		if (del)
			ret = hns3vf_remove_mc_mac_addr(hw, addr);
		else
			ret = hns3vf_add_mc_mac_addr(hw, addr);
		if (ret) {
			err = ret;
			rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      addr);
			hns3_err(hw, "Failed to %s mc mac addr: %s for vf: %d",
				 del ? "Remove" : "Restore", mac_str, ret);
		}
	}
	return err;
}

static int
hns3vf_set_promisc_mode(struct hns3_hw *hw, bool en_bc_pmc,
			bool en_uc_pmc, bool en_mc_pmc)
{
	struct hns3_mbx_vf_to_pf_cmd *req;
	struct hns3_cmd_desc desc;
	int ret;

	req = (struct hns3_mbx_vf_to_pf_cmd *)desc.data;

	/*
	 * The hns3 VF PMD depends on the hns3 PF kernel ethdev driver,
	 * so there are some features for promiscuous/allmulticast mode in hns3
	 * VF PMD as below:
	 * 1. The promiscuous/allmulticast mode can be configured successfully
	 *    only based on the trusted VF device. If based on the non trusted
	 *    VF device, configuring promiscuous/allmulticast mode will fail.
	 *    The hns3 VF device can be confiruged as trusted device by hns3 PF
	 *    kernel ethdev driver on the host by the following command:
	 *      "ip link set <eth num> vf <vf id> turst on"
	 * 2. After the promiscuous mode is configured successfully, hns3 VF PMD
	 *    can receive the ingress and outgoing traffic. This includes
	 *    all the ingress packets, all the packets sent from the PF and
	 *    other VFs on the same physical port.
	 * 3. Note: Because of the hardware constraints, By default vlan filter
	 *    is enabled and couldn't be turned off based on VF device, so vlan
	 *    filter is still effective even in promiscuous mode. If upper
	 *    applications don't call rte_eth_dev_vlan_filter API function to
	 *    set vlan based on VF device, hns3 VF PMD will can't receive
	 *    the packets with vlan tag in promiscuoue mode.
	 */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MBX_VF_TO_PF, false);
	req->msg[0] = HNS3_MBX_SET_PROMISC_MODE;
	req->msg[1] = en_bc_pmc ? 1 : 0;
	req->msg[2] = en_uc_pmc ? 1 : 0;
	req->msg[3] = en_mc_pmc ? 1 : 0;
	req->msg[4] = hw->promisc_mode == HNS3_LIMIT_PROMISC_MODE ? 1 : 0;

	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret)
		hns3_err(hw, "Set promisc mode fail, ret = %d", ret);

	return ret;
}

static int
hns3vf_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3vf_set_promisc_mode(hw, true, true, true);
	if (ret)
		hns3_err(hw, "Failed to enable promiscuous mode, ret = %d",
			ret);
	return ret;
}

static int
hns3vf_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	bool allmulti = dev->data->all_multicast ? true : false;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3vf_set_promisc_mode(hw, true, false, allmulti);
	if (ret)
		hns3_err(hw, "Failed to disable promiscuous mode, ret = %d",
			ret);
	return ret;
}

static int
hns3vf_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (dev->data->promiscuous)
		return 0;

	ret = hns3vf_set_promisc_mode(hw, true, false, true);
	if (ret)
		hns3_err(hw, "Failed to enable allmulticast mode, ret = %d",
			ret);
	return ret;
}

static int
hns3vf_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (dev->data->promiscuous)
		return 0;

	ret = hns3vf_set_promisc_mode(hw, true, false, false);
	if (ret)
		hns3_err(hw, "Failed to disable allmulticast mode, ret = %d",
			ret);
	return ret;
}

static int
hns3vf_restore_promisc(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	bool allmulti = hw->data->all_multicast ? true : false;

	if (hw->data->promiscuous)
		return hns3vf_set_promisc_mode(hw, true, true, true);

	return hns3vf_set_promisc_mode(hw, true, false, allmulti);
}

static int
hns3vf_bind_ring_with_vector(struct hns3_hw *hw, uint8_t vector_id,
			     bool mmap, enum hns3_ring_type queue_type,
			     uint16_t queue_id)
{
	struct hns3_vf_bind_vector_msg bind_msg;
	const char *op_str;
	uint16_t code;
	int ret;

	memset(&bind_msg, 0, sizeof(bind_msg));
	code = mmap ? HNS3_MBX_MAP_RING_TO_VECTOR :
		HNS3_MBX_UNMAP_RING_TO_VECTOR;
	bind_msg.vector_id = vector_id;

	if (queue_type == HNS3_RING_TYPE_RX)
		bind_msg.param[0].int_gl_index = HNS3_RING_GL_RX;
	else
		bind_msg.param[0].int_gl_index = HNS3_RING_GL_TX;

	bind_msg.param[0].ring_type = queue_type;
	bind_msg.ring_num = 1;
	bind_msg.param[0].tqp_index = queue_id;
	op_str = mmap ? "Map" : "Unmap";
	ret = hns3_send_mbx_msg(hw, code, 0, (uint8_t *)&bind_msg,
				sizeof(bind_msg), false, NULL, 0);
	if (ret)
		hns3_err(hw, "%s TQP %u fail, vector_id is %u, ret is %d.",
			 op_str, queue_id, bind_msg.vector_id, ret);

	return ret;
}

static int
hns3vf_init_ring_with_vector(struct hns3_hw *hw)
{
	uint16_t vec;
	int ret;
	int i;

	/*
	 * In hns3 network engine, vector 0 is always the misc interrupt of this
	 * function, vector 1~N can be used respectively for the queues of the
	 * function. Tx and Rx queues with the same number share the interrupt
	 * vector. In the initialization clearing the all hardware mapping
	 * relationship configurations between queues and interrupt vectors is
	 * needed, so some error caused by the residual configurations, such as
	 * the unexpected Tx interrupt, can be avoid.
	 */
	vec = hw->num_msi - 1; /* vector 0 for misc interrupt, not for queue */
	if (hw->intr.mapping_mode == HNS3_INTR_MAPPING_VEC_RSV_ONE)
		vec = vec - 1; /* the last interrupt is reserved */
	hw->intr_tqps_num = RTE_MIN(vec, hw->tqps_num);
	for (i = 0; i < hw->intr_tqps_num; i++) {
		/*
		 * Set gap limiter/rate limiter/quanity limiter algorithm
		 * configuration for interrupt coalesce of queue's interrupt.
		 */
		hns3_set_queue_intr_gl(hw, i, HNS3_RING_GL_RX,
				       HNS3_TQP_INTR_GL_DEFAULT);
		hns3_set_queue_intr_gl(hw, i, HNS3_RING_GL_TX,
				       HNS3_TQP_INTR_GL_DEFAULT);
		hns3_set_queue_intr_rl(hw, i, HNS3_TQP_INTR_RL_DEFAULT);
		/*
		 * QL(quantity limiter) is not used currently, just set 0 to
		 * close it.
		 */
		hns3_set_queue_intr_ql(hw, i, HNS3_TQP_INTR_QL_DEFAULT);

		ret = hns3vf_bind_ring_with_vector(hw, vec, false,
						   HNS3_RING_TYPE_TX, i);
		if (ret) {
			PMD_INIT_LOG(ERR, "VF fail to unbind TX ring(%d) with "
					  "vector: %u, ret=%d", i, vec, ret);
			return ret;
		}

		ret = hns3vf_bind_ring_with_vector(hw, vec, false,
						   HNS3_RING_TYPE_RX, i);
		if (ret) {
			PMD_INIT_LOG(ERR, "VF fail to unbind RX ring(%d) with "
					  "vector: %u, ret=%d", i, vec, ret);
			return ret;
		}
	}

	return 0;
}

static int
hns3vf_dev_configure(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_rss_conf *rss_cfg = &hw->rss_info;
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	enum rte_eth_rx_mq_mode mq_mode = conf->rxmode.mq_mode;
	uint16_t nb_rx_q = dev->data->nb_rx_queues;
	uint16_t nb_tx_q = dev->data->nb_tx_queues;
	struct rte_eth_rss_conf rss_conf;
	uint32_t max_rx_pkt_len;
	uint16_t mtu;
	bool gro_en;
	int ret;

	hw->cfg_max_queues = RTE_MAX(nb_rx_q, nb_tx_q);

	/*
	 * Some versions of hardware network engine does not support
	 * individually enable/disable/reset the Tx or Rx queue. These devices
	 * must enable/disable/reset Tx and Rx queues at the same time. When the
	 * numbers of Tx queues allocated by upper applications are not equal to
	 * the numbers of Rx queues, driver needs to setup fake Tx or Rx queues
	 * to adjust numbers of Tx/Rx queues. otherwise, network engine can not
	 * work as usual. But these fake queues are imperceptible, and can not
	 * be used by upper applications.
	 */
	ret = hns3_set_fake_rx_or_tx_queues(dev, nb_rx_q, nb_tx_q);
	if (ret) {
		hns3_err(hw, "fail to set Rx/Tx fake queues, ret = %d.", ret);
		hw->cfg_max_queues = 0;
		return ret;
	}

	hw->adapter_state = HNS3_NIC_CONFIGURING;
	if (conf->link_speeds & ETH_LINK_SPEED_FIXED) {
		hns3_err(hw, "setting link speed/duplex not supported");
		ret = -EINVAL;
		goto cfg_err;
	}

	/* When RSS is not configured, redirect the packet queue 0 */
	if ((uint32_t)mq_mode & ETH_MQ_RX_RSS_FLAG) {
		conf->rxmode.offloads |= DEV_RX_OFFLOAD_RSS_HASH;
		hw->rss_dis_flag = false;
		rss_conf = conf->rx_adv_conf.rss_conf;
		if (rss_conf.rss_key == NULL) {
			rss_conf.rss_key = rss_cfg->key;
			rss_conf.rss_key_len = HNS3_RSS_KEY_SIZE;
		}

		ret = hns3_dev_rss_hash_update(dev, &rss_conf);
		if (ret)
			goto cfg_err;
	}

	/*
	 * If jumbo frames are enabled, MTU needs to be refreshed
	 * according to the maximum RX packet length.
	 */
	if (conf->rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		max_rx_pkt_len = conf->rxmode.max_rx_pkt_len;
		if (max_rx_pkt_len > HNS3_MAX_FRAME_LEN ||
		    max_rx_pkt_len <= HNS3_DEFAULT_FRAME_LEN) {
			hns3_err(hw, "maximum Rx packet length must be greater "
				 "than %u and less than %u when jumbo frame enabled.",
				 (uint16_t)HNS3_DEFAULT_FRAME_LEN,
				 (uint16_t)HNS3_MAX_FRAME_LEN);
			ret = -EINVAL;
			goto cfg_err;
		}

		mtu = (uint16_t)HNS3_PKTLEN_TO_MTU(max_rx_pkt_len);
		ret = hns3vf_dev_mtu_set(dev, mtu);
		if (ret)
			goto cfg_err;
		dev->data->mtu = mtu;
	}

	ret = hns3vf_dev_configure_vlan(dev);
	if (ret)
		goto cfg_err;

	/* config hardware GRO */
	gro_en = conf->rxmode.offloads & DEV_RX_OFFLOAD_TCP_LRO ? true : false;
	ret = hns3_config_gro(hw, gro_en);
	if (ret)
		goto cfg_err;

	hns->rx_simple_allowed = true;
	hns->rx_vec_allowed = true;
	hns->tx_simple_allowed = true;
	hns->tx_vec_allowed = true;

	hns3_init_rx_ptype_tble(dev);

	hw->adapter_state = HNS3_NIC_CONFIGURED;
	return 0;

cfg_err:
	hw->cfg_max_queues = 0;
	(void)hns3_set_fake_rx_or_tx_queues(dev, 0, 0);
	hw->adapter_state = HNS3_NIC_INITIALIZED;

	return ret;
}

static int
hns3vf_config_mtu(struct hns3_hw *hw, uint16_t mtu)
{
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_MTU, 0, (const uint8_t *)&mtu,
				sizeof(mtu), true, NULL, 0);
	if (ret)
		hns3_err(hw, "Failed to set mtu (%u) for vf: %d", mtu, ret);

	return ret;
}

static int
hns3vf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t frame_size = mtu + HNS3_ETH_OVERHEAD;
	int ret;

	/*
	 * The hns3 PF/VF devices on the same port share the hardware MTU
	 * configuration. Currently, we send mailbox to inform hns3 PF kernel
	 * ethdev driver to finish hardware MTU configuration in hns3 VF PMD,
	 * there is no need to stop the port for hns3 VF device, and the
	 * MTU value issued by hns3 VF PMD must be less than or equal to
	 * PF's MTU.
	 */
	if (rte_atomic16_read(&hw->reset.resetting)) {
		hns3_err(hw, "Failed to set mtu during resetting");
		return -EIO;
	}

	/*
	 * when Rx of scattered packets is off, we have some possibility of
	 * using vector Rx process function or simple Rx functions in hns3 PMD.
	 * If the input MTU is increased and the maximum length of
	 * received packets is greater than the length of a buffer for Rx
	 * packet, the hardware network engine needs to use multiple BDs and
	 * buffers to store these packets. This will cause problems when still
	 * using vector Rx process function or simple Rx function to receiving
	 * packets. So, when Rx of scattered packets is off and device is
	 * started, it is not permitted to increase MTU so that the maximum
	 * length of Rx packets is greater than Rx buffer length.
	 */
	if (dev->data->dev_started && !dev->data->scattered_rx &&
	    frame_size > hw->rx_buf_len) {
		hns3_err(hw, "failed to set mtu because current is "
			"not scattered rx mode");
		return -EOPNOTSUPP;
	}

	rte_spinlock_lock(&hw->lock);
	ret = hns3vf_config_mtu(hw, mtu);
	if (ret) {
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}
	if (mtu > RTE_ETHER_MTU)
		dev->data->dev_conf.rxmode.offloads |=
						DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		dev->data->dev_conf.rxmode.offloads &=
						~DEV_RX_OFFLOAD_JUMBO_FRAME;
	dev->data->dev_conf.rxmode.max_rx_pkt_len = frame_size;
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3vf_dev_infos_get(struct rte_eth_dev *eth_dev, struct rte_eth_dev_info *info)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint16_t q_num = hw->tqps_num;

	/*
	 * In interrupt mode, 'max_rx_queues' is set based on the number of
	 * MSI-X interrupt resources of the hardware.
	 */
	if (hw->data->dev_conf.intr_conf.rxq == 1)
		q_num = hw->intr_tqps_num;

	info->max_rx_queues = q_num;
	info->max_tx_queues = hw->tqps_num;
	info->max_rx_pktlen = HNS3_MAX_FRAME_LEN; /* CRC included */
	info->min_rx_bufsize = HNS3_MIN_BD_BUF_SIZE;
	info->max_mac_addrs = HNS3_VF_UC_MACADDR_NUM;
	info->max_mtu = info->max_rx_pktlen - HNS3_ETH_OVERHEAD;
	info->max_lro_pkt_size = HNS3_MAX_LRO_SIZE;

	info->rx_offload_capa = (DEV_RX_OFFLOAD_IPV4_CKSUM |
				 DEV_RX_OFFLOAD_UDP_CKSUM |
				 DEV_RX_OFFLOAD_TCP_CKSUM |
				 DEV_RX_OFFLOAD_SCTP_CKSUM |
				 DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
				 DEV_RX_OFFLOAD_OUTER_UDP_CKSUM |
				 DEV_RX_OFFLOAD_SCATTER |
				 DEV_RX_OFFLOAD_VLAN_STRIP |
				 DEV_RX_OFFLOAD_VLAN_FILTER |
				 DEV_RX_OFFLOAD_JUMBO_FRAME |
				 DEV_RX_OFFLOAD_RSS_HASH |
				 DEV_RX_OFFLOAD_TCP_LRO);
	info->tx_offload_capa = (DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				 DEV_TX_OFFLOAD_IPV4_CKSUM |
				 DEV_TX_OFFLOAD_TCP_CKSUM |
				 DEV_TX_OFFLOAD_UDP_CKSUM |
				 DEV_TX_OFFLOAD_SCTP_CKSUM |
				 DEV_TX_OFFLOAD_MULTI_SEGS |
				 DEV_TX_OFFLOAD_TCP_TSO |
				 DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
				 DEV_TX_OFFLOAD_GRE_TNL_TSO |
				 DEV_TX_OFFLOAD_GENEVE_TNL_TSO |
				 DEV_TX_OFFLOAD_MBUF_FAST_FREE |
				 hns3_txvlan_cap_get(hw));

	if (hns3_dev_indep_txrx_supported(hw))
		info->dev_capa = RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
				 RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP;

	info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = HNS3_MAX_RING_DESC,
		.nb_min = HNS3_MIN_RING_DESC,
		.nb_align = HNS3_ALIGN_RING_DESC,
	};

	info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = HNS3_MAX_RING_DESC,
		.nb_min = HNS3_MIN_RING_DESC,
		.nb_align = HNS3_ALIGN_RING_DESC,
		.nb_seg_max = HNS3_MAX_TSO_BD_PER_PKT,
		.nb_mtu_seg_max = hw->max_non_tso_bd_num,
	};

	info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = HNS3_DEFAULT_RX_FREE_THRESH,
		/*
		 * If there are no available Rx buffer descriptors, incoming
		 * packets are always dropped by hardware based on hns3 network
		 * engine.
		 */
		.rx_drop_en = 1,
		.offloads = 0,
	};
	info->default_txconf = (struct rte_eth_txconf) {
		.tx_rs_thresh = HNS3_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	info->reta_size = hw->rss_ind_tbl_size;
	info->hash_key_size = HNS3_RSS_KEY_SIZE;
	info->flow_type_rss_offloads = HNS3_ETH_RSS_SUPPORT;
	info->default_rxportconf.ring_size = HNS3_DEFAULT_RING_DESC;
	info->default_txportconf.ring_size = HNS3_DEFAULT_RING_DESC;

	return 0;
}

static void
hns3vf_clear_event_cause(struct hns3_hw *hw, uint32_t regclr)
{
	hns3_write_dev(hw, HNS3_VECTOR0_CMDQ_SRC_REG, regclr);
}

static void
hns3vf_disable_irq0(struct hns3_hw *hw)
{
	hns3_write_dev(hw, HNS3_MISC_VECTOR_REG_BASE, 0);
}

static void
hns3vf_enable_irq0(struct hns3_hw *hw)
{
	hns3_write_dev(hw, HNS3_MISC_VECTOR_REG_BASE, 1);
}

static enum hns3vf_evt_cause
hns3vf_check_event_cause(struct hns3_adapter *hns, uint32_t *clearval)
{
	struct hns3_hw *hw = &hns->hw;
	enum hns3vf_evt_cause ret;
	uint32_t cmdq_stat_reg;
	uint32_t rst_ing_reg;
	uint32_t val;

	/* Fetch the events from their corresponding regs */
	cmdq_stat_reg = hns3_read_dev(hw, HNS3_VECTOR0_CMDQ_STAT_REG);
	if (BIT(HNS3_VECTOR0_RST_INT_B) & cmdq_stat_reg) {
		rst_ing_reg = hns3_read_dev(hw, HNS3_FUN_RST_ING);
		hns3_warn(hw, "resetting reg: 0x%x", rst_ing_reg);
		hns3_atomic_set_bit(HNS3_VF_RESET, &hw->reset.pending);
		rte_atomic16_set(&hw->reset.disable_cmd, 1);
		val = hns3_read_dev(hw, HNS3_VF_RST_ING);
		hns3_write_dev(hw, HNS3_VF_RST_ING, val | HNS3_VF_RST_ING_BIT);
		val = cmdq_stat_reg & ~BIT(HNS3_VECTOR0_RST_INT_B);
		if (clearval) {
			hw->reset.stats.global_cnt++;
			hns3_warn(hw, "Global reset detected, clear reset status");
		} else {
			hns3_schedule_delayed_reset(hns);
			hns3_warn(hw, "Global reset detected, don't clear reset status");
		}

		ret = HNS3VF_VECTOR0_EVENT_RST;
		goto out;
	}

	/* Check for vector0 mailbox(=CMDQ RX) event source */
	if (BIT(HNS3_VECTOR0_RX_CMDQ_INT_B) & cmdq_stat_reg) {
		val = cmdq_stat_reg & ~BIT(HNS3_VECTOR0_RX_CMDQ_INT_B);
		ret = HNS3VF_VECTOR0_EVENT_MBX;
		goto out;
	}

	val = 0;
	ret = HNS3VF_VECTOR0_EVENT_OTHER;
out:
	if (clearval)
		*clearval = val;
	return ret;
}

static void
hns3vf_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	enum hns3vf_evt_cause event_cause;
	uint32_t clearval;

	/* Disable interrupt */
	hns3vf_disable_irq0(hw);

	/* Read out interrupt causes */
	event_cause = hns3vf_check_event_cause(hns, &clearval);
	/* Clear interrupt causes */
	hns3vf_clear_event_cause(hw, clearval);

	switch (event_cause) {
	case HNS3VF_VECTOR0_EVENT_RST:
		hns3_schedule_reset(hns);
		break;
	case HNS3VF_VECTOR0_EVENT_MBX:
		hns3_dev_handle_mbx_msg(hw);
		break;
	default:
		break;
	}

	/* Enable interrupt */
	hns3vf_enable_irq0(hw);
}

static void
hns3vf_set_default_dev_specifications(struct hns3_hw *hw)
{
	hw->max_non_tso_bd_num = HNS3_MAX_NON_TSO_BD_PER_PKT;
	hw->rss_ind_tbl_size = HNS3_RSS_IND_TBL_SIZE;
	hw->rss_key_size = HNS3_RSS_KEY_SIZE;
	hw->intr.int_ql_max = HNS3_INTR_QL_NONE;
}

static void
hns3vf_parse_dev_specifications(struct hns3_hw *hw, struct hns3_cmd_desc *desc)
{
	struct hns3_dev_specs_0_cmd *req0;

	req0 = (struct hns3_dev_specs_0_cmd *)desc[0].data;

	hw->max_non_tso_bd_num = req0->max_non_tso_bd_num;
	hw->rss_ind_tbl_size = rte_le_to_cpu_16(req0->rss_ind_tbl_size);
	hw->rss_key_size = rte_le_to_cpu_16(req0->rss_key_size);
	hw->intr.int_ql_max = rte_le_to_cpu_16(req0->intr_ql_max);
}

static int
hns3vf_check_dev_specifications(struct hns3_hw *hw)
{
	if (hw->rss_ind_tbl_size == 0 ||
	    hw->rss_ind_tbl_size > HNS3_RSS_IND_TBL_SIZE_MAX) {
		hns3_warn(hw, "the size of hash lookup table configured (%u)"
			      " exceeds the maximum(%u)", hw->rss_ind_tbl_size,
			      HNS3_RSS_IND_TBL_SIZE_MAX);
		return -EINVAL;
	}

	return 0;
}

static int
hns3vf_query_dev_specifications(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc[HNS3_QUERY_DEV_SPECS_BD_NUM];
	int ret;
	int i;

	for (i = 0; i < HNS3_QUERY_DEV_SPECS_BD_NUM - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_QUERY_DEV_SPECS,
					  true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_QUERY_DEV_SPECS, true);

	ret = hns3_cmd_send(hw, desc, HNS3_QUERY_DEV_SPECS_BD_NUM);
	if (ret)
		return ret;

	hns3vf_parse_dev_specifications(hw, desc);

	return hns3vf_check_dev_specifications(hw);
}

static int
hns3vf_get_capability(struct hns3_hw *hw)
{
	struct rte_pci_device *pci_dev;
	struct rte_eth_dev *eth_dev;
	uint8_t revision;
	int ret;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	/* Get PCI revision id */
	ret = rte_pci_read_config(pci_dev, &revision, HNS3_PCI_REVISION_ID_LEN,
				  HNS3_PCI_REVISION_ID);
	if (ret != HNS3_PCI_REVISION_ID_LEN) {
		PMD_INIT_LOG(ERR, "failed to read pci revision id, ret = %d",
			     ret);
		return -EIO;
	}
	hw->revision = revision;

	if (revision < PCI_REVISION_ID_HIP09_A) {
		hns3vf_set_default_dev_specifications(hw);
		hw->intr.mapping_mode = HNS3_INTR_MAPPING_VEC_RSV_ONE;
		hw->intr.gl_unit = HNS3_INTR_COALESCE_GL_UINT_2US;
		hw->tso_mode = HNS3_TSO_SW_CAL_PSEUDO_H_CSUM;
		hw->min_tx_pkt_len = HNS3_HIP08_MIN_TX_PKT_LEN;
		hw->rss_info.ipv6_sctp_offload_supported = false;
		hw->promisc_mode = HNS3_UNLIMIT_PROMISC_MODE;
		return 0;
	}

	ret = hns3vf_query_dev_specifications(hw);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "failed to query dev specifications, ret = %d",
			     ret);
		return ret;
	}

	hw->intr.mapping_mode = HNS3_INTR_MAPPING_VEC_ALL;
	hw->intr.gl_unit = HNS3_INTR_COALESCE_GL_UINT_1US;
	hw->tso_mode = HNS3_TSO_HW_CAL_PSEUDO_H_CSUM;
	hw->min_tx_pkt_len = HNS3_HIP09_MIN_TX_PKT_LEN;
	hw->rss_info.ipv6_sctp_offload_supported = true;
	hw->promisc_mode = HNS3_LIMIT_PROMISC_MODE;

	return 0;
}

static int
hns3vf_check_tqp_info(struct hns3_hw *hw)
{
	if (hw->tqps_num == 0) {
		PMD_INIT_LOG(ERR, "Get invalid tqps_num(0) from PF.");
		return -EINVAL;
	}

	if (hw->rss_size_max == 0) {
		PMD_INIT_LOG(ERR, "Get invalid rss_size_max(0) from PF.");
		return -EINVAL;
	}

	hw->tqps_num = RTE_MIN(hw->rss_size_max, hw->tqps_num);

	return 0;
}

static int
hns3vf_get_port_base_vlan_filter_state(struct hns3_hw *hw)
{
	uint8_t resp_msg;
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_VLAN,
				HNS3_MBX_GET_PORT_BASE_VLAN_STATE, NULL, 0,
				true, &resp_msg, sizeof(resp_msg));
	if (ret) {
		if (ret == -ETIME) {
			/*
			 * Getting current port based VLAN state from PF driver
			 * will not affect VF driver's basic function. Because
			 * the VF driver relies on hns3 PF kernel ether driver,
			 * to avoid introducing compatibility issues with older
			 * version of PF driver, no failure will be returned
			 * when the return value is ETIME. This return value has
			 * the following scenarios:
			 * 1) Firmware didn't return the results in time
			 * 2) the result return by firmware is timeout
			 * 3) the older version of kernel side PF driver does
			 *    not support this mailbox message.
			 * For scenarios 1 and 2, it is most likely that a
			 * hardware error has occurred, or a hardware reset has
			 * occurred. In this case, these errors will be caught
			 * by other functions.
			 */
			PMD_INIT_LOG(WARNING,
				"failed to get PVID state for timeout, maybe "
				"kernel side PF driver doesn't support this "
				"mailbox message, or firmware didn't respond.");
			resp_msg = HNS3_PORT_BASE_VLAN_DISABLE;
		} else {
			PMD_INIT_LOG(ERR, "failed to get port based VLAN state,"
				" ret = %d", ret);
			return ret;
		}
	}
	hw->port_base_vlan_cfg.state = resp_msg ?
		HNS3_PORT_BASE_VLAN_ENABLE : HNS3_PORT_BASE_VLAN_DISABLE;
	return 0;
}

static int
hns3vf_get_queue_info(struct hns3_hw *hw)
{
#define HNS3VF_TQPS_RSS_INFO_LEN	6
	uint8_t resp_msg[HNS3VF_TQPS_RSS_INFO_LEN];
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_GET_QINFO, 0, NULL, 0, true,
				resp_msg, HNS3VF_TQPS_RSS_INFO_LEN);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get tqp info from PF: %d", ret);
		return ret;
	}

	memcpy(&hw->tqps_num, &resp_msg[0], sizeof(uint16_t));
	memcpy(&hw->rss_size_max, &resp_msg[2], sizeof(uint16_t));

	return hns3vf_check_tqp_info(hw);
}

static int
hns3vf_get_queue_depth(struct hns3_hw *hw)
{
#define HNS3VF_TQPS_DEPTH_INFO_LEN	4
	uint8_t resp_msg[HNS3VF_TQPS_DEPTH_INFO_LEN];
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_GET_QDEPTH, 0, NULL, 0, true,
				resp_msg, HNS3VF_TQPS_DEPTH_INFO_LEN);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get tqp depth info from PF: %d",
			     ret);
		return ret;
	}

	memcpy(&hw->num_tx_desc, &resp_msg[0], sizeof(uint16_t));
	memcpy(&hw->num_rx_desc, &resp_msg[2], sizeof(uint16_t));

	return 0;
}

static int
hns3vf_get_tc_info(struct hns3_hw *hw)
{
	uint8_t resp_msg;
	int ret;
	uint32_t i;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_GET_TCINFO, 0, NULL, 0,
				true, &resp_msg, sizeof(resp_msg));
	if (ret) {
		hns3_err(hw, "VF request to get TC info from PF failed %d",
			 ret);
		return ret;
	}

	hw->hw_tc_map = resp_msg;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		if (hw->hw_tc_map & BIT(i))
			hw->num_tc++;
	}

	return 0;
}

static int
hns3vf_get_host_mac_addr(struct hns3_hw *hw)
{
	uint8_t host_mac[RTE_ETHER_ADDR_LEN];
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_GET_MAC_ADDR, 0, NULL, 0,
				true, host_mac, RTE_ETHER_ADDR_LEN);
	if (ret) {
		hns3_err(hw, "Failed to get mac addr from PF: %d", ret);
		return ret;
	}

	memcpy(hw->mac.mac_addr, host_mac, RTE_ETHER_ADDR_LEN);

	return 0;
}

static int
hns3vf_get_configuration(struct hns3_hw *hw)
{
	int ret;

	hw->mac.media_type = HNS3_MEDIA_TYPE_NONE;
	hw->rss_dis_flag = false;

	/* Get device capability */
	ret = hns3vf_get_capability(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to get device capability: %d.", ret);
		return ret;
	}

	/* Get queue configuration from PF */
	ret = hns3vf_get_queue_info(hw);
	if (ret)
		return ret;

	/* Get queue depth info from PF */
	ret = hns3vf_get_queue_depth(hw);
	if (ret)
		return ret;

	/* Get user defined VF MAC addr from PF */
	ret = hns3vf_get_host_mac_addr(hw);
	if (ret)
		return ret;

	ret = hns3vf_get_port_base_vlan_filter_state(hw);
	if (ret)
		return ret;

	/* Get tc configuration from PF */
	return hns3vf_get_tc_info(hw);
}

static int
hns3vf_set_tc_queue_mapping(struct hns3_adapter *hns, uint16_t nb_rx_q,
			    uint16_t nb_tx_q)
{
	struct hns3_hw *hw = &hns->hw;

	return hns3_queue_to_tc_mapping(hw, nb_rx_q, nb_tx_q);
}

static void
hns3vf_request_link_info(struct hns3_hw *hw)
{
	int ret;

	if (rte_atomic16_read(&hw->reset.resetting))
		return;
	ret = hns3_send_mbx_msg(hw, HNS3_MBX_GET_LINK_STATUS, 0, NULL, 0, false,
				NULL, 0);
	if (ret)
		hns3_err(hw, "Failed to fetch link status from PF: %d", ret);
}

static int
hns3vf_vlan_filter_configure(struct hns3_adapter *hns, uint16_t vlan_id, int on)
{
#define HNS3VF_VLAN_MBX_MSG_LEN 5
	struct hns3_hw *hw = &hns->hw;
	uint8_t msg_data[HNS3VF_VLAN_MBX_MSG_LEN];
	uint16_t proto = htons(RTE_ETHER_TYPE_VLAN);
	uint8_t is_kill = on ? 0 : 1;

	msg_data[0] = is_kill;
	memcpy(&msg_data[1], &vlan_id, sizeof(vlan_id));
	memcpy(&msg_data[3], &proto, sizeof(proto));

	return hns3_send_mbx_msg(hw, HNS3_MBX_SET_VLAN, HNS3_MBX_VLAN_FILTER,
				 msg_data, HNS3VF_VLAN_MBX_MSG_LEN, true, NULL,
				 0);
}

static int
hns3vf_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (rte_atomic16_read(&hw->reset.resetting)) {
		hns3_err(hw,
			 "vf set vlan id failed during resetting, vlan_id =%u",
			 vlan_id);
		return -EIO;
	}
	rte_spinlock_lock(&hw->lock);
	ret = hns3vf_vlan_filter_configure(hns, vlan_id, on);
	rte_spinlock_unlock(&hw->lock);
	if (ret)
		hns3_err(hw, "vf set vlan id failed, vlan_id =%u, ret =%d",
			 vlan_id, ret);

	return ret;
}

static int
hns3vf_en_hw_strip_rxvtag(struct hns3_hw *hw, bool enable)
{
	uint8_t msg_data;
	int ret;

	msg_data = enable ? 1 : 0;
	ret = hns3_send_mbx_msg(hw, HNS3_MBX_SET_VLAN, HNS3_MBX_VLAN_RX_OFF_CFG,
				&msg_data, sizeof(msg_data), false, NULL, 0);
	if (ret)
		hns3_err(hw, "vf %s strip failed, ret = %d.",
				enable ? "enable" : "disable", ret);

	return ret;
}

static int
hns3vf_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	unsigned int tmp_mask;
	int ret = 0;

	if (rte_atomic16_read(&hw->reset.resetting)) {
		hns3_err(hw, "vf set vlan offload failed during resetting, "
			     "mask = 0x%x", mask);
		return -EIO;
	}

	tmp_mask = (unsigned int)mask;
	/* Vlan stripping setting */
	if (tmp_mask & ETH_VLAN_STRIP_MASK) {
		rte_spinlock_lock(&hw->lock);
		/* Enable or disable VLAN stripping */
		if (dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			ret = hns3vf_en_hw_strip_rxvtag(hw, true);
		else
			ret = hns3vf_en_hw_strip_rxvtag(hw, false);
		rte_spinlock_unlock(&hw->lock);
	}

	return ret;
}

static int
hns3vf_handle_all_vlan_table(struct hns3_adapter *hns, int on)
{
	struct rte_vlan_filter_conf *vfc;
	struct hns3_hw *hw = &hns->hw;
	uint16_t vlan_id;
	uint64_t vbit;
	uint64_t ids;
	int ret = 0;
	uint32_t i;

	vfc = &hw->data->vlan_filter_conf;
	for (i = 0; i < RTE_DIM(vfc->ids); i++) {
		if (vfc->ids[i] == 0)
			continue;
		ids = vfc->ids[i];
		while (ids) {
			/*
			 * 64 means the num bits of ids, one bit corresponds to
			 * one vlan id
			 */
			vlan_id = 64 * i;
			/* count trailing zeroes */
			vbit = ~ids & (ids - 1);
			/* clear least significant bit set */
			ids ^= (ids ^ (ids - 1)) ^ vbit;
			for (; vbit;) {
				vbit >>= 1;
				vlan_id++;
			}
			ret = hns3vf_vlan_filter_configure(hns, vlan_id, on);
			if (ret) {
				hns3_err(hw,
					 "VF handle vlan table failed, ret =%d, on = %d",
					 ret, on);
				return ret;
			}
		}
	}

	return ret;
}

static int
hns3vf_remove_all_vlan_table(struct hns3_adapter *hns)
{
	return hns3vf_handle_all_vlan_table(hns, 0);
}

static int
hns3vf_restore_vlan_conf(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_conf *dev_conf;
	bool en;
	int ret;

	dev_conf = &hw->data->dev_conf;
	en = dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_VLAN_STRIP ? true
								   : false;
	ret = hns3vf_en_hw_strip_rxvtag(hw, en);
	if (ret)
		hns3_err(hw, "VF restore vlan conf fail, en =%d, ret =%d", en,
			 ret);
	return ret;
}

static int
hns3vf_dev_configure_vlan(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct rte_eth_dev_data *data = dev->data;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (data->dev_conf.txmode.hw_vlan_reject_tagged ||
	    data->dev_conf.txmode.hw_vlan_reject_untagged ||
	    data->dev_conf.txmode.hw_vlan_insert_pvid) {
		hns3_warn(hw, "hw_vlan_reject_tagged, hw_vlan_reject_untagged "
			      "or hw_vlan_insert_pvid is not support!");
	}

	/* Apply vlan offload setting */
	ret = hns3vf_vlan_offload_set(dev, ETH_VLAN_STRIP_MASK);
	if (ret)
		hns3_err(hw, "dev config vlan offload failed, ret =%d", ret);

	return ret;
}

static int
hns3vf_set_alive(struct hns3_hw *hw, bool alive)
{
	uint8_t msg_data;

	msg_data = alive ? 1 : 0;
	return hns3_send_mbx_msg(hw, HNS3_MBX_SET_ALIVE, 0, &msg_data,
				 sizeof(msg_data), false, NULL, 0);
}

static void
hns3vf_keep_alive_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3_send_mbx_msg(hw, HNS3_MBX_KEEP_ALIVE, 0, NULL, 0,
				false, NULL, 0);
	if (ret)
		hns3_err(hw, "VF sends keeping alive cmd failed(=%d)",
			 ret);

	rte_eal_alarm_set(HNS3VF_KEEP_ALIVE_INTERVAL, hns3vf_keep_alive_handler,
			  eth_dev);
}

static void
hns3vf_service_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	/*
	 * The query link status and reset processing are executed in the
	 * interrupt thread.When the IMP reset occurs, IMP will not respond,
	 * and the query operation will time out after 30ms. In the case of
	 * multiple PF/VFs, each query failure timeout causes the IMP reset
	 * interrupt to fail to respond within 100ms.
	 * Before querying the link status, check whether there is a reset
	 * pending, and if so, abandon the query.
	 */
	if (!hns3vf_is_reset_pending(hns))
		hns3vf_request_link_info(hw);
	else
		hns3_warn(hw, "Cancel the query when reset is pending");

	rte_eal_alarm_set(HNS3VF_SERVICE_INTERVAL, hns3vf_service_handler,
			  eth_dev);
}

static int
hns3_query_vf_resource(struct hns3_hw *hw)
{
	struct hns3_vf_res_cmd *req;
	struct hns3_cmd_desc desc;
	uint16_t num_msi;
	int ret;

	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_QUERY_VF_RSRC, true);
	ret = hns3_cmd_send(hw, &desc, 1);
	if (ret) {
		hns3_err(hw, "query vf resource failed, ret = %d", ret);
		return ret;
	}

	req = (struct hns3_vf_res_cmd *)desc.data;
	num_msi = hns3_get_field(rte_le_to_cpu_16(req->vf_intr_vector_number),
				 HNS3_VF_VEC_NUM_M, HNS3_VF_VEC_NUM_S);
	if (num_msi < HNS3_MIN_VECTOR_NUM) {
		hns3_err(hw, "Just %u msi resources, not enough for vf(min:%d)",
			 num_msi, HNS3_MIN_VECTOR_NUM);
		return -EINVAL;
	}

	hw->num_msi = num_msi;

	return 0;
}

static int
hns3vf_init_hardware(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	uint16_t mtu = hw->data->mtu;
	int ret;

	ret = hns3vf_set_promisc_mode(hw, true, false, false);
	if (ret)
		return ret;

	ret = hns3vf_config_mtu(hw, mtu);
	if (ret)
		goto err_init_hardware;

	ret = hns3vf_vlan_filter_configure(hns, 0, 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to initialize VLAN config: %d", ret);
		goto err_init_hardware;
	}

	ret = hns3_config_gro(hw, false);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to config gro: %d", ret);
		goto err_init_hardware;
	}

	/*
	 * In the initialization clearing the all hardware mapping relationship
	 * configurations between queues and interrupt vectors is needed, so
	 * some error caused by the residual configurations, such as the
	 * unexpected interrupt, can be avoid.
	 */
	ret = hns3vf_init_ring_with_vector(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init ring intr vector: %d", ret);
		goto err_init_hardware;
	}

	return 0;

err_init_hardware:
	(void)hns3vf_set_promisc_mode(hw, false, false, false);
	return ret;
}

static int
hns3vf_clear_vport_list(struct hns3_hw *hw)
{
	return hns3_send_mbx_msg(hw, HNS3_MBX_HANDLE_VF_TBL,
				 HNS3_MBX_VPORT_LIST_CLEAR, NULL, 0, false,
				 NULL, 0);
}

static int
hns3vf_init_vf(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Get hardware io base address from pcie BAR2 IO space */
	hw->io_base = pci_dev->mem_resource[2].addr;

	/* Firmware command queue initialize */
	ret = hns3_cmd_init_queue(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init cmd queue: %d", ret);
		goto err_cmd_init_queue;
	}

	/* Firmware command initialize */
	ret = hns3_cmd_init(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init cmd: %d", ret);
		goto err_cmd_init;
	}

	/* Get VF resource */
	ret = hns3_query_vf_resource(hw);
	if (ret)
		goto err_cmd_init;

	rte_spinlock_init(&hw->mbx_resp.lock);

	hns3vf_clear_event_cause(hw, 0);

	ret = rte_intr_callback_register(&pci_dev->intr_handle,
					 hns3vf_interrupt_handler, eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to register intr: %d", ret);
		goto err_intr_callback_register;
	}

	/* Enable interrupt */
	rte_intr_enable(&pci_dev->intr_handle);
	hns3vf_enable_irq0(hw);

	/* Get configuration from PF */
	ret = hns3vf_get_configuration(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to fetch configuration: %d", ret);
		goto err_get_config;
	}

	ret = hns3_tqp_stats_init(hw);
	if (ret)
		goto err_get_config;

	ret = hns3vf_set_tc_queue_mapping(hns, hw->tqps_num, hw->tqps_num);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to set tc info, ret = %d.", ret);
		goto err_set_tc_queue;
	}

	ret = hns3vf_clear_vport_list(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to clear tbl list: %d", ret);
		goto err_set_tc_queue;
	}

	ret = hns3vf_init_hardware(hns);
	if (ret)
		goto err_set_tc_queue;

	hns3_set_default_rss_args(hw);

	ret = hns3vf_set_alive(hw, true);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to VF send alive to PF: %d", ret);
		goto err_set_tc_queue;
	}

	return 0;

err_set_tc_queue:
	hns3_tqp_stats_uninit(hw);

err_get_config:
	hns3vf_disable_irq0(hw);
	rte_intr_disable(&pci_dev->intr_handle);
	hns3_intr_unregister(&pci_dev->intr_handle, hns3vf_interrupt_handler,
			     eth_dev);
err_intr_callback_register:
err_cmd_init:
	hns3_cmd_uninit(hw);
	hns3_cmd_destroy_queue(hw);
err_cmd_init_queue:
	hw->io_base = NULL;

	return ret;
}

static void
hns3vf_uninit_vf(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	hns3_rss_uninit(hns);
	(void)hns3_config_gro(hw, false);
	(void)hns3vf_set_alive(hw, false);
	(void)hns3vf_set_promisc_mode(hw, false, false, false);
	hns3_flow_uninit(eth_dev);
	hns3_tqp_stats_uninit(hw);
	hns3vf_disable_irq0(hw);
	rte_intr_disable(&pci_dev->intr_handle);
	hns3_intr_unregister(&pci_dev->intr_handle, hns3vf_interrupt_handler,
			     eth_dev);
	hns3_cmd_uninit(hw);
	hns3_cmd_destroy_queue(hw);
	hw->io_base = NULL;
}

static int
hns3vf_do_stop(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	hw->mac.link_status = ETH_LINK_DOWN;

	/*
	 * The "hns3vf_do_stop" function will also be called by .stop_service to
	 * prepare reset. At the time of global or IMP reset, the command cannot
	 * be sent to stop the tx/rx queues. The mbuf in Tx/Rx queues may be
	 * accessed during the reset process. So the mbuf can not be released
	 * during reset and is required to be released after the reset is
	 * completed.
	 */
	if (rte_atomic16_read(&hw->reset.resetting) == 0)
		hns3_dev_release_mbufs(hns);

	if (rte_atomic16_read(&hw->reset.disable_cmd) == 0) {
		hns3vf_configure_mac_addr(hns, true);
		ret = hns3_reset_all_tqps(hns);
		if (ret) {
			hns3_err(hw, "failed to reset all queues ret = %d",
				 ret);
			return ret;
		}
	}
	return 0;
}

static void
hns3vf_unmap_rx_interrupt(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint8_t base = RTE_INTR_VEC_ZERO_OFFSET;
	uint8_t vec = RTE_INTR_VEC_ZERO_OFFSET;
	uint16_t q_id;

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return;

	/* unmap the ring with vector */
	if (rte_intr_allow_others(intr_handle)) {
		vec = RTE_INTR_VEC_RXTX_OFFSET;
		base = RTE_INTR_VEC_RXTX_OFFSET;
	}
	if (rte_intr_dp_is_en(intr_handle)) {
		for (q_id = 0; q_id < hw->used_rx_queues; q_id++) {
			(void)hns3vf_bind_ring_with_vector(hw, vec, false,
							   HNS3_RING_TYPE_RX,
							   q_id);
			if (vec < base + intr_handle->nb_efd - 1)
				vec++;
		}
	}
	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
}

static int
hns3vf_dev_stop(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();
	dev->data->dev_started = 0;

	hw->adapter_state = HNS3_NIC_STOPPING;
	hns3_set_rxtx_function(dev);
	rte_wmb();
	/* Disable datapath on secondary process. */
	hns3_mp_req_stop_rxtx(dev);
	/* Prevent crashes when queues are still in use. */
	rte_delay_ms(hw->cfg_max_queues);

	rte_spinlock_lock(&hw->lock);
	if (rte_atomic16_read(&hw->reset.resetting) == 0) {
		hns3_stop_tqps(hw);
		hns3vf_do_stop(hns);
		hns3vf_unmap_rx_interrupt(dev);
		hw->adapter_state = HNS3_NIC_CONFIGURED;
	}
	hns3_rx_scattered_reset(dev);
	rte_eal_alarm_cancel(hns3vf_service_handler, dev);
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3vf_dev_close(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret = 0;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		__atomic_fetch_sub(&hw->secondary_cnt, 1, __ATOMIC_RELAXED);
		hns3_mp_uninit();
		return 0;
	}

	if (hw->adapter_state == HNS3_NIC_STARTED)
		ret = hns3vf_dev_stop(eth_dev);

	hw->adapter_state = HNS3_NIC_CLOSING;
	hns3_reset_abort(hns);
	hw->adapter_state = HNS3_NIC_CLOSED;
	rte_eal_alarm_cancel(hns3vf_keep_alive_handler, eth_dev);
	hns3vf_configure_all_mc_mac_addr(hns, true);
	hns3vf_remove_all_vlan_table(hns);
	hns3vf_uninit_vf(eth_dev);
	hns3_free_all_queues(eth_dev);
	rte_free(hw->reset.wait_data);
	hns3_mp_uninit();
	hns3_warn(hw, "Close port %u finished", hw->data->port_id);

	return ret;
}

static int
hns3vf_fw_version_get(struct rte_eth_dev *eth_dev, char *fw_version,
		      size_t fw_size)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t version = hw->fw_version;
	int ret;

	ret = snprintf(fw_version, fw_size, "%lu.%lu.%lu.%lu",
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE3_M,
				      HNS3_FW_VERSION_BYTE3_S),
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE2_M,
				      HNS3_FW_VERSION_BYTE2_S),
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE1_M,
				      HNS3_FW_VERSION_BYTE1_S),
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE0_M,
				      HNS3_FW_VERSION_BYTE0_S));
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

static int
hns3vf_dev_link_update(struct rte_eth_dev *eth_dev,
		       __rte_unused int wait_to_complete)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct hns3_mac *mac = &hw->mac;
	struct rte_eth_link new_link;

	memset(&new_link, 0, sizeof(new_link));
	switch (mac->link_speed) {
	case ETH_SPEED_NUM_10M:
	case ETH_SPEED_NUM_100M:
	case ETH_SPEED_NUM_1G:
	case ETH_SPEED_NUM_10G:
	case ETH_SPEED_NUM_25G:
	case ETH_SPEED_NUM_40G:
	case ETH_SPEED_NUM_50G:
	case ETH_SPEED_NUM_100G:
	case ETH_SPEED_NUM_200G:
		if (mac->link_status)
			new_link.link_speed = mac->link_speed;
		break;
	default:
		if (mac->link_status)
			new_link.link_speed = ETH_SPEED_NUM_UNKNOWN;
		break;
	}

	if (!mac->link_status)
		new_link.link_speed = ETH_SPEED_NUM_NONE;

	new_link.link_duplex = mac->link_duplex;
	new_link.link_status = mac->link_status ? ETH_LINK_UP : ETH_LINK_DOWN;
	new_link.link_autoneg =
	    !(eth_dev->data->dev_conf.link_speeds & ETH_LINK_SPEED_FIXED);

	return rte_eth_linkstatus_set(eth_dev, &new_link);
}

static int
hns3vf_do_start(struct hns3_adapter *hns, bool reset_queue)
{
	struct hns3_hw *hw = &hns->hw;
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	int ret;

	ret = hns3vf_set_tc_queue_mapping(hns, nb_rx_q, nb_tx_q);
	if (ret)
		return ret;

	ret = hns3_init_queues(hns, reset_queue);
	if (ret)
		hns3_err(hw, "failed to init queues, ret = %d.", ret);

	return ret;
}

static int
hns3vf_map_rx_interrupt(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint8_t base = RTE_INTR_VEC_ZERO_OFFSET;
	uint8_t vec = RTE_INTR_VEC_ZERO_OFFSET;
	uint32_t intr_vector;
	uint16_t q_id;
	int ret;

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return 0;

	/* disable uio/vfio intr/eventfd mapping */
	rte_intr_disable(intr_handle);

	/* check and configure queue intr-vector mapping */
	if (rte_intr_cap_multiple(intr_handle) ||
	    !RTE_ETH_DEV_SRIOV(dev).active) {
		intr_vector = hw->used_rx_queues;
		/* It creates event fd for each intr vector when MSIX is used */
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -EINVAL;
	}
	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
				    hw->used_rx_queues * sizeof(int), 0);
		if (intr_handle->intr_vec == NULL) {
			hns3_err(hw, "Failed to allocate %u rx_queues"
				     " intr_vec", hw->used_rx_queues);
			ret = -ENOMEM;
			goto vf_alloc_intr_vec_error;
		}
	}

	if (rte_intr_allow_others(intr_handle)) {
		vec = RTE_INTR_VEC_RXTX_OFFSET;
		base = RTE_INTR_VEC_RXTX_OFFSET;
	}
	if (rte_intr_dp_is_en(intr_handle)) {
		for (q_id = 0; q_id < hw->used_rx_queues; q_id++) {
			ret = hns3vf_bind_ring_with_vector(hw, vec, true,
							   HNS3_RING_TYPE_RX,
							   q_id);
			if (ret)
				goto vf_bind_vector_error;
			intr_handle->intr_vec[q_id] = vec;
			if (vec < base + intr_handle->nb_efd - 1)
				vec++;
		}
	}
	rte_intr_enable(intr_handle);
	return 0;

vf_bind_vector_error:
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}
	return ret;
vf_alloc_intr_vec_error:
	rte_intr_efd_disable(intr_handle);
	return ret;
}

static int
hns3vf_restore_rx_interrupt(struct hns3_hw *hw)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint16_t q_id;
	int ret;

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return 0;

	if (rte_intr_dp_is_en(intr_handle)) {
		for (q_id = 0; q_id < hw->used_rx_queues; q_id++) {
			ret = hns3vf_bind_ring_with_vector(hw,
					intr_handle->intr_vec[q_id], true,
					HNS3_RING_TYPE_RX, q_id);
			if (ret)
				return ret;
		}
	}

	return 0;
}

static void
hns3vf_restore_filter(struct rte_eth_dev *dev)
{
	hns3_restore_rss_filter(dev);
}

static int
hns3vf_dev_start(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (rte_atomic16_read(&hw->reset.resetting))
		return -EBUSY;

	rte_spinlock_lock(&hw->lock);
	hw->adapter_state = HNS3_NIC_STARTING;
	ret = hns3vf_do_start(hns, true);
	if (ret) {
		hw->adapter_state = HNS3_NIC_CONFIGURED;
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}
	ret = hns3vf_map_rx_interrupt(dev);
	if (ret)
		goto map_rx_inter_err;

	/*
	 * There are three register used to control the status of a TQP
	 * (contains a pair of Tx queue and Rx queue) in the new version network
	 * engine. One is used to control the enabling of Tx queue, the other is
	 * used to control the enabling of Rx queue, and the last is the master
	 * switch used to control the enabling of the tqp. The Tx register and
	 * TQP register must be enabled at the same time to enable a Tx queue.
	 * The same applies to the Rx queue. For the older network enginem, this
	 * function only refresh the enabled flag, and it is used to update the
	 * status of queue in the dpdk framework.
	 */
	ret = hns3_start_all_txqs(dev);
	if (ret)
		goto map_rx_inter_err;

	ret = hns3_start_all_rxqs(dev);
	if (ret)
		goto start_all_rxqs_fail;

	hw->adapter_state = HNS3_NIC_STARTED;
	rte_spinlock_unlock(&hw->lock);

	hns3_rx_scattered_calc(dev);
	hns3_set_rxtx_function(dev);
	hns3_mp_req_start_rxtx(dev);
	hns3vf_service_handler(dev);

	hns3vf_restore_filter(dev);

	/* Enable interrupt of all rx queues before enabling queues */
	hns3_dev_all_rx_queue_intr_enable(hw, true);

	/*
	 * After finished the initialization, start all tqps to receive/transmit
	 * packets and refresh all queue status.
	 */
	hns3_start_tqps(hw);

	return ret;

start_all_rxqs_fail:
	hns3_stop_all_txqs(dev);
map_rx_inter_err:
	(void)hns3vf_do_stop(hns);
	hw->adapter_state = HNS3_NIC_CONFIGURED;
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static bool
is_vf_reset_done(struct hns3_hw *hw)
{
#define HNS3_FUN_RST_ING_BITS \
	(BIT(HNS3_VECTOR0_GLOBALRESET_INT_B) | \
	 BIT(HNS3_VECTOR0_CORERESET_INT_B) | \
	 BIT(HNS3_VECTOR0_IMPRESET_INT_B) | \
	 BIT(HNS3_VECTOR0_FUNCRESET_INT_B))

	uint32_t val;

	if (hw->reset.level == HNS3_VF_RESET) {
		val = hns3_read_dev(hw, HNS3_VF_RST_ING);
		if (val & HNS3_VF_RST_ING_BIT)
			return false;
	} else {
		val = hns3_read_dev(hw, HNS3_FUN_RST_ING);
		if (val & HNS3_FUN_RST_ING_BITS)
			return false;
	}
	return true;
}

bool
hns3vf_is_reset_pending(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	enum hns3_reset_level reset;

	/*
	 * According to the protocol of PCIe, FLR to a PF device resets the PF
	 * state as well as the SR-IOV extended capability including VF Enable
	 * which means that VFs no longer exist.
	 *
	 * HNS3_VF_FULL_RESET means PF device is in FLR reset. when PF device
	 * is in FLR stage, the register state of VF device is not reliable,
	 * so register states detection can not be carried out. In this case,
	 * we just ignore the register states and return false to indicate that
	 * there are no other reset states that need to be processed by driver.
	 */
	if (hw->reset.level == HNS3_VF_FULL_RESET)
		return false;

	/* Check the registers to confirm whether there is reset pending */
	hns3vf_check_event_cause(hns, NULL);
	reset = hns3vf_get_reset_level(hw, &hw->reset.pending);
	if (hw->reset.level != HNS3_NONE_RESET && hw->reset.level < reset) {
		hns3_warn(hw, "High level reset %d is pending", reset);
		return true;
	}
	return false;
}

static int
hns3vf_wait_hardware_ready(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct hns3_wait_data *wait_data = hw->reset.wait_data;
	struct timeval tv;

	if (wait_data->result == HNS3_WAIT_SUCCESS) {
		/*
		 * After vf reset is ready, the PF may not have completed
		 * the reset processing. The vf sending mbox to PF may fail
		 * during the pf reset, so it is better to add extra delay.
		 */
		if (hw->reset.level == HNS3_VF_FUNC_RESET ||
		    hw->reset.level == HNS3_FLR_RESET)
			return 0;
		/* Reset retry process, no need to add extra delay. */
		if (hw->reset.attempts)
			return 0;
		if (wait_data->check_completion == NULL)
			return 0;

		wait_data->check_completion = NULL;
		wait_data->interval = 1 * MSEC_PER_SEC * USEC_PER_MSEC;
		wait_data->count = 1;
		wait_data->result = HNS3_WAIT_REQUEST;
		rte_eal_alarm_set(wait_data->interval, hns3_wait_callback,
				  wait_data);
		hns3_warn(hw, "hardware is ready, delay 1 sec for PF reset complete");
		return -EAGAIN;
	} else if (wait_data->result == HNS3_WAIT_TIMEOUT) {
		hns3_clock_gettime(&tv);
		hns3_warn(hw, "Reset step4 hardware not ready after reset time=%ld.%.6ld",
			  tv.tv_sec, tv.tv_usec);
		return -ETIME;
	} else if (wait_data->result == HNS3_WAIT_REQUEST)
		return -EAGAIN;

	wait_data->hns = hns;
	wait_data->check_completion = is_vf_reset_done;
	wait_data->end_ms = (uint64_t)HNS3VF_RESET_WAIT_CNT *
				HNS3VF_RESET_WAIT_MS + hns3_clock_gettime_ms();
	wait_data->interval = HNS3VF_RESET_WAIT_MS * USEC_PER_MSEC;
	wait_data->count = HNS3VF_RESET_WAIT_CNT;
	wait_data->result = HNS3_WAIT_REQUEST;
	rte_eal_alarm_set(wait_data->interval, hns3_wait_callback, wait_data);
	return -EAGAIN;
}

static int
hns3vf_prepare_reset(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (hw->reset.level == HNS3_VF_FUNC_RESET) {
		ret = hns3_send_mbx_msg(hw, HNS3_MBX_RESET, 0, NULL,
					0, true, NULL, 0);
		if (ret)
			return ret;
	}
	rte_atomic16_set(&hw->reset.disable_cmd, 1);

	return 0;
}

static int
hns3vf_stop_service(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_dev *eth_dev;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	if (hw->adapter_state == HNS3_NIC_STARTED)
		rte_eal_alarm_cancel(hns3vf_service_handler, eth_dev);
	hw->mac.link_status = ETH_LINK_DOWN;

	hns3_set_rxtx_function(eth_dev);
	rte_wmb();
	/* Disable datapath on secondary process. */
	hns3_mp_req_stop_rxtx(eth_dev);
	rte_delay_ms(hw->cfg_max_queues);

	rte_spinlock_lock(&hw->lock);
	if (hw->adapter_state == HNS3_NIC_STARTED ||
	    hw->adapter_state == HNS3_NIC_STOPPING) {
		hns3_enable_all_queues(hw, false);
		hns3vf_do_stop(hns);
		hw->reset.mbuf_deferred_free = true;
	} else
		hw->reset.mbuf_deferred_free = false;

	/*
	 * It is cumbersome for hardware to pick-and-choose entries for deletion
	 * from table space. Hence, for function reset software intervention is
	 * required to delete the entries.
	 */
	if (rte_atomic16_read(&hw->reset.disable_cmd) == 0)
		hns3vf_configure_all_mc_mac_addr(hns, true);
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3vf_start_service(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_dev *eth_dev;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	hns3_set_rxtx_function(eth_dev);
	hns3_mp_req_start_rxtx(eth_dev);
	if (hw->adapter_state == HNS3_NIC_STARTED) {
		hns3vf_service_handler(eth_dev);

		/* Enable interrupt of all rx queues before enabling queues */
		hns3_dev_all_rx_queue_intr_enable(hw, true);
		/*
		 * Enable state of each rxq and txq will be recovered after
		 * reset, so we need to restore them before enable all tqps;
		 */
		hns3_restore_tqp_enable_state(hw);
		/*
		 * When finished the initialization, enable queues to receive
		 * and transmit packets.
		 */
		hns3_enable_all_queues(hw, true);
	}

	return 0;
}

static int
hns3vf_check_default_mac_change(struct hns3_hw *hw)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *hw_mac;
	int ret;

	/*
	 * The hns3 PF ethdev driver in kernel support setting VF MAC address
	 * on the host by "ip link set ..." command. If the hns3 PF kernel
	 * ethdev driver sets the MAC address for VF device after the
	 * initialization of the related VF device, the PF driver will notify
	 * VF driver to reset VF device to make the new MAC address effective
	 * immediately. The hns3 VF PMD should check whether the MAC
	 * address has been changed by the PF kernel ethdev driver, if changed
	 * VF driver should configure hardware using the new MAC address in the
	 * recovering hardware configuration stage of the reset process.
	 */
	ret = hns3vf_get_host_mac_addr(hw);
	if (ret)
		return ret;

	hw_mac = (struct rte_ether_addr *)hw->mac.mac_addr;
	ret = rte_is_zero_ether_addr(hw_mac);
	if (ret) {
		rte_ether_addr_copy(&hw->data->mac_addrs[0], hw_mac);
	} else {
		ret = rte_is_same_ether_addr(&hw->data->mac_addrs[0], hw_mac);
		if (!ret) {
			rte_ether_addr_copy(hw_mac, &hw->data->mac_addrs[0]);
			rte_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      &hw->data->mac_addrs[0]);
			hns3_warn(hw, "Default MAC address has been changed to:"
				  " %s by the host PF kernel ethdev driver",
				  mac_str);
		}
	}

	return 0;
}

static int
hns3vf_restore_conf(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	int ret;

	ret = hns3vf_check_default_mac_change(hw);
	if (ret)
		return ret;

	ret = hns3vf_configure_mac_addr(hns, false);
	if (ret)
		return ret;

	ret = hns3vf_configure_all_mc_mac_addr(hns, false);
	if (ret)
		goto err_mc_mac;

	ret = hns3vf_restore_promisc(hns);
	if (ret)
		goto err_vlan_table;

	ret = hns3vf_restore_vlan_conf(hns);
	if (ret)
		goto err_vlan_table;

	ret = hns3vf_get_port_base_vlan_filter_state(hw);
	if (ret)
		goto err_vlan_table;

	ret = hns3vf_restore_rx_interrupt(hw);
	if (ret)
		goto err_vlan_table;

	ret = hns3_restore_gro_conf(hw);
	if (ret)
		goto err_vlan_table;

	if (hw->adapter_state == HNS3_NIC_STARTED) {
		ret = hns3vf_do_start(hns, false);
		if (ret)
			goto err_vlan_table;
		hns3_info(hw, "hns3vf dev restart successful!");
	} else if (hw->adapter_state == HNS3_NIC_STOPPING)
		hw->adapter_state = HNS3_NIC_CONFIGURED;

	ret = hns3vf_set_alive(hw, true);
	if (ret) {
		hns3_err(hw, "failed to VF send alive to PF: %d", ret);
		goto err_vlan_table;
	}

	return 0;

err_vlan_table:
	hns3vf_configure_all_mc_mac_addr(hns, true);
err_mc_mac:
	hns3vf_configure_mac_addr(hns, true);
	return ret;
}

static enum hns3_reset_level
hns3vf_get_reset_level(struct hns3_hw *hw, uint64_t *levels)
{
	enum hns3_reset_level reset_level;

	/* return the highest priority reset level amongst all */
	if (hns3_atomic_test_bit(HNS3_VF_RESET, levels))
		reset_level = HNS3_VF_RESET;
	else if (hns3_atomic_test_bit(HNS3_VF_FULL_RESET, levels))
		reset_level = HNS3_VF_FULL_RESET;
	else if (hns3_atomic_test_bit(HNS3_VF_PF_FUNC_RESET, levels))
		reset_level = HNS3_VF_PF_FUNC_RESET;
	else if (hns3_atomic_test_bit(HNS3_VF_FUNC_RESET, levels))
		reset_level = HNS3_VF_FUNC_RESET;
	else if (hns3_atomic_test_bit(HNS3_FLR_RESET, levels))
		reset_level = HNS3_FLR_RESET;
	else
		reset_level = HNS3_NONE_RESET;

	if (hw->reset.level != HNS3_NONE_RESET && reset_level < hw->reset.level)
		return HNS3_NONE_RESET;

	return reset_level;
}

static void
hns3vf_reset_service(void *param)
{
	struct hns3_adapter *hns = (struct hns3_adapter *)param;
	struct hns3_hw *hw = &hns->hw;
	enum hns3_reset_level reset_level;
	struct timeval tv_delta;
	struct timeval tv_start;
	struct timeval tv;
	uint64_t msec;

	/*
	 * The interrupt is not triggered within the delay time.
	 * The interrupt may have been lost. It is necessary to handle
	 * the interrupt to recover from the error.
	 */
	if (rte_atomic16_read(&hns->hw.reset.schedule) == SCHEDULE_DEFERRED) {
		rte_atomic16_set(&hns->hw.reset.schedule, SCHEDULE_REQUESTED);
		hns3_err(hw, "Handling interrupts in delayed tasks");
		hns3vf_interrupt_handler(&rte_eth_devices[hw->data->port_id]);
		reset_level = hns3vf_get_reset_level(hw, &hw->reset.pending);
		if (reset_level == HNS3_NONE_RESET) {
			hns3_err(hw, "No reset level is set, try global reset");
			hns3_atomic_set_bit(HNS3_VF_RESET, &hw->reset.pending);
		}
	}
	rte_atomic16_set(&hns->hw.reset.schedule, SCHEDULE_NONE);

	/*
	 * Hardware reset has been notified, we now have to poll & check if
	 * hardware has actually completed the reset sequence.
	 */
	reset_level = hns3vf_get_reset_level(hw, &hw->reset.pending);
	if (reset_level != HNS3_NONE_RESET) {
		hns3_clock_gettime(&tv_start);
		hns3_reset_process(hns, reset_level);
		hns3_clock_gettime(&tv);
		timersub(&tv, &tv_start, &tv_delta);
		msec = hns3_clock_calctime_ms(&tv_delta);
		if (msec > HNS3_RESET_PROCESS_MS)
			hns3_err(hw, "%d handle long time delta %" PRIu64
				 " ms time=%ld.%.6ld",
				 hw->reset.level, msec, tv.tv_sec, tv.tv_usec);
	}
}

static int
hns3vf_reinit_dev(struct hns3_adapter *hns)
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[hns->hw.data->port_id];
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (hw->reset.level == HNS3_VF_FULL_RESET) {
		rte_intr_disable(&pci_dev->intr_handle);
		ret = hns3vf_set_bus_master(pci_dev, true);
		if (ret < 0) {
			hns3_err(hw, "failed to set pci bus, ret = %d", ret);
			return ret;
		}
	}

	/* Firmware command initialize */
	ret = hns3_cmd_init(hw);
	if (ret) {
		hns3_err(hw, "Failed to init cmd: %d", ret);
		return ret;
	}

	if (hw->reset.level == HNS3_VF_FULL_RESET) {
		/*
		 * UIO enables msix by writing the pcie configuration space
		 * vfio_pci enables msix in rte_intr_enable.
		 */
		if (pci_dev->kdrv == RTE_PCI_KDRV_IGB_UIO ||
		    pci_dev->kdrv == RTE_PCI_KDRV_UIO_GENERIC) {
			if (hns3vf_enable_msix(pci_dev, true))
				hns3_err(hw, "Failed to enable msix");
		}

		rte_intr_enable(&pci_dev->intr_handle);
	}

	ret = hns3_reset_all_tqps(hns);
	if (ret) {
		hns3_err(hw, "Failed to reset all queues: %d", ret);
		return ret;
	}

	ret = hns3vf_init_hardware(hns);
	if (ret) {
		hns3_err(hw, "Failed to init hardware: %d", ret);
		return ret;
	}

	return 0;
}

static const struct eth_dev_ops hns3vf_eth_dev_ops = {
	.dev_configure      = hns3vf_dev_configure,
	.dev_start          = hns3vf_dev_start,
	.dev_stop           = hns3vf_dev_stop,
	.dev_close          = hns3vf_dev_close,
	.mtu_set            = hns3vf_dev_mtu_set,
	.promiscuous_enable = hns3vf_dev_promiscuous_enable,
	.promiscuous_disable = hns3vf_dev_promiscuous_disable,
	.allmulticast_enable = hns3vf_dev_allmulticast_enable,
	.allmulticast_disable = hns3vf_dev_allmulticast_disable,
	.stats_get          = hns3_stats_get,
	.stats_reset        = hns3_stats_reset,
	.xstats_get         = hns3_dev_xstats_get,
	.xstats_get_names   = hns3_dev_xstats_get_names,
	.xstats_reset       = hns3_dev_xstats_reset,
	.xstats_get_by_id   = hns3_dev_xstats_get_by_id,
	.xstats_get_names_by_id = hns3_dev_xstats_get_names_by_id,
	.dev_infos_get      = hns3vf_dev_infos_get,
	.fw_version_get     = hns3vf_fw_version_get,
	.rx_queue_setup     = hns3_rx_queue_setup,
	.tx_queue_setup     = hns3_tx_queue_setup,
	.rx_queue_release   = hns3_dev_rx_queue_release,
	.tx_queue_release   = hns3_dev_tx_queue_release,
	.rx_queue_start     = hns3_dev_rx_queue_start,
	.rx_queue_stop      = hns3_dev_rx_queue_stop,
	.tx_queue_start     = hns3_dev_tx_queue_start,
	.tx_queue_stop      = hns3_dev_tx_queue_stop,
	.rx_queue_intr_enable   = hns3_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable  = hns3_dev_rx_queue_intr_disable,
	.rxq_info_get       = hns3_rxq_info_get,
	.txq_info_get       = hns3_txq_info_get,
	.rx_burst_mode_get  = hns3_rx_burst_mode_get,
	.tx_burst_mode_get  = hns3_tx_burst_mode_get,
	.mac_addr_add       = hns3vf_add_mac_addr,
	.mac_addr_remove    = hns3vf_remove_mac_addr,
	.mac_addr_set       = hns3vf_set_default_mac_addr,
	.set_mc_addr_list   = hns3vf_set_mc_mac_addr_list,
	.link_update        = hns3vf_dev_link_update,
	.rss_hash_update    = hns3_dev_rss_hash_update,
	.rss_hash_conf_get  = hns3_dev_rss_hash_conf_get,
	.reta_update        = hns3_dev_rss_reta_update,
	.reta_query         = hns3_dev_rss_reta_query,
	.filter_ctrl        = hns3_dev_filter_ctrl,
	.vlan_filter_set    = hns3vf_vlan_filter_set,
	.vlan_offload_set   = hns3vf_vlan_offload_set,
	.get_reg            = hns3_get_regs,
	.dev_supported_ptypes_get = hns3_dev_supported_ptypes_get,
};

static const struct hns3_reset_ops hns3vf_reset_ops = {
	.reset_service       = hns3vf_reset_service,
	.stop_service        = hns3vf_stop_service,
	.prepare_reset       = hns3vf_prepare_reset,
	.wait_hardware_ready = hns3vf_wait_hardware_ready,
	.reinit_dev          = hns3vf_reinit_dev,
	.restore_conf        = hns3vf_restore_conf,
	.start_service       = hns3vf_start_service,
};

static int
hns3vf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	hns3_flow_init(eth_dev);

	hns3_set_rxtx_function(eth_dev);
	eth_dev->dev_ops = &hns3vf_eth_dev_ops;
	eth_dev->rx_queue_count = hns3_rx_queue_count;
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		ret = hns3_mp_init_secondary();
		if (ret) {
			PMD_INIT_LOG(ERR, "Failed to init for secondary "
					  "process, ret = %d", ret);
			goto err_mp_init_secondary;
		}
		__atomic_fetch_add(&hw->secondary_cnt, 1, __ATOMIC_RELAXED);
		process_data.eth_dev_cnt++;
		return 0;
	}

	eth_dev->data->dev_flags |= RTE_ETH_DEV_AUTOFILL_QUEUE_XSTATS;

	ret = hns3_mp_init_primary();
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "Failed to init for primary process, ret = %d",
			     ret);
		goto err_mp_init_primary;
	}
	process_data.eth_dev_cnt++;

	hw->adapter_state = HNS3_NIC_UNINITIALIZED;
	hns->is_vf = true;
	hw->data = eth_dev->data;

	ret = hns3_reset_init(hw);
	if (ret)
		goto err_init_reset;
	hw->reset.ops = &hns3vf_reset_ops;

	ret = hns3vf_init_vf(eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init vf: %d", ret);
		goto err_init_vf;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("hns3vf-mac",
					       sizeof(struct rte_ether_addr) *
					       HNS3_VF_UC_MACADDR_NUM, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %zx bytes needed "
			     "to store MAC addresses",
			     sizeof(struct rte_ether_addr) *
			     HNS3_VF_UC_MACADDR_NUM);
		ret = -ENOMEM;
		goto err_rte_zmalloc;
	}

	/*
	 * The hns3 PF ethdev driver in kernel support setting VF MAC address
	 * on the host by "ip link set ..." command. To avoid some incorrect
	 * scenes, for example, hns3 VF PMD fails to receive and send
	 * packets after user configure the MAC address by using the
	 * "ip link set ..." command, hns3 VF PMD keep the same MAC
	 * address strategy as the hns3 kernel ethdev driver in the
	 * initialization. If user configure a MAC address by the ip command
	 * for VF device, then hns3 VF PMD will start with it, otherwise
	 * start with a random MAC address in the initialization.
	 */
	if (rte_is_zero_ether_addr((struct rte_ether_addr *)hw->mac.mac_addr))
		rte_eth_random_addr(hw->mac.mac_addr);
	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.mac_addr,
			    &eth_dev->data->mac_addrs[0]);

	hw->adapter_state = HNS3_NIC_INITIALIZED;

	if (rte_atomic16_read(&hns->hw.reset.schedule) == SCHEDULE_PENDING) {
		hns3_err(hw, "Reschedule reset service after dev_init");
		hns3_schedule_reset(hns);
	} else {
		/* IMP will wait ready flag before reset */
		hns3_notify_reset_ready(hw, false);
	}
	rte_eal_alarm_set(HNS3VF_KEEP_ALIVE_INTERVAL, hns3vf_keep_alive_handler,
			  eth_dev);
	return 0;

err_rte_zmalloc:
	hns3vf_uninit_vf(eth_dev);

err_init_vf:
	rte_free(hw->reset.wait_data);

err_init_reset:
	hns3_mp_uninit();

err_mp_init_primary:
err_mp_init_secondary:
	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;

	return ret;
}

static int
hns3vf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		__atomic_fetch_sub(&hw->secondary_cnt, 1, __ATOMIC_RELAXED);
		hns3_mp_uninit();
		return 0;
	}

	if (hw->adapter_state < HNS3_NIC_CLOSING)
		hns3vf_dev_close(eth_dev);

	hw->adapter_state = HNS3_NIC_REMOVED;
	return 0;
}

static int
eth_hns3vf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
		     struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct hns3_adapter),
					     hns3vf_dev_init);
}

static int
eth_hns3vf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, hns3vf_dev_uninit);
}

static const struct rte_pci_id pci_id_hns3vf_map[] = {
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_100G_VF) },
	{ RTE_PCI_DEVICE(PCI_VENDOR_ID_HUAWEI, HNS3_DEV_ID_100G_RDMA_PFC_VF) },
	{ .vendor_id = 0, }, /* sentinel */
};

static struct rte_pci_driver rte_hns3vf_pmd = {
	.id_table = pci_id_hns3vf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	.probe = eth_hns3vf_pci_probe,
	.remove = eth_hns3vf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hns3_vf, rte_hns3vf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hns3_vf, pci_id_hns3vf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_hns3_vf, "* igb_uio | vfio-pci");
