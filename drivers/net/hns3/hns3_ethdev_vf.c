/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018-2021 HiSilicon Limited.
 */

#include <rte_alarm.h>
#include <ethdev_pci.h>
#include <rte_io.h>
#include <rte_vfio.h>

#include "hns3_ethdev.h"
#include "hns3_common.h"
#include "hns3_dump.h"
#include "hns3_logs.h"
#include "hns3_rxtx.h"
#include "hns3_regs.h"
#include "hns3_intr.h"
#include "hns3_dcb.h"
#include "hns3_mp.h"
#include "hns3_flow.h"

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
static int hns3vf_dev_link_update(struct rte_eth_dev *eth_dev,
				   __rte_unused int wait_to_complete);

static int
hns3vf_enable_msix(const struct rte_pci_device *device, bool op)
{
	uint16_t control;
	off_t pos;
	int ret;

	if (!rte_pci_has_capability_list(device)) {
		PMD_INIT_LOG(ERR, "Failed to read PCI capability list");
		return 0;
	}

	pos = rte_pci_find_capability(device, RTE_PCI_CAP_ID_MSIX);
	if (pos > 0) {
		ret = rte_pci_read_config(device, &control, sizeof(control),
			pos + RTE_PCI_MSIX_FLAGS);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Failed to read MSIX flags");
			return -ENXIO;
		}

		if (op)
			control |= RTE_PCI_MSIX_FLAGS_ENABLE;
		else
			control &= ~RTE_PCI_MSIX_FLAGS_ENABLE;
		ret = rte_pci_write_config(device, &control, sizeof(control),
			pos + RTE_PCI_MSIX_FLAGS);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "failed to write MSIX flags");
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
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_SET_UNICAST,
			 HNS3_MBX_MAC_VLAN_UC_ADD);
	memcpy(req.data, mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN);
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
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
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_SET_UNICAST,
			 HNS3_MBX_MAC_VLAN_UC_REMOVE);
	memcpy(req.data, mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN);
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				       mac_addr);
		hns3_err(hw, "failed to add uc mac addr(%s), ret = %d",
			 mac_str, ret);
	}
	return ret;
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
	struct hns3_vf_to_pf_msg req;
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

	hns3vf_mbx_setup(&req, HNS3_MBX_SET_UNICAST,
			 HNS3_MBX_MAC_VLAN_UC_MODIFY);
	memcpy(req.data, addr_bytes, HNS3_TWO_ETHER_ADDR_LEN);
	ret = hns3vf_mbx_send(hw, &req, true, NULL, 0);
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
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					       old_addr);
			hns3_warn(hw, "Has permanent mac addr(%s) for vf",
				  mac_str);
		} else {
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					       mac_addr);
			hns3_err(hw, "Failed to set mac addr(%s) for vf: %d",
				 mac_str, ret);
		}
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}

	rte_ether_addr_copy(mac_addr,
			    (struct rte_ether_addr *)hw->mac.mac_addr);
	rte_spinlock_unlock(&hw->lock);

	return ret;
}

static int
hns3vf_add_mc_mac_addr(struct hns3_hw *hw,
		       struct rte_ether_addr *mac_addr)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_SET_MULTICAST,
			 HNS3_MBX_MAC_VLAN_MC_ADD);
	memcpy(req.data, mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN);
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
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
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_SET_MULTICAST,
			 HNS3_MBX_MAC_VLAN_MC_REMOVE);
	memcpy(req.data, mac_addr->addr_bytes, RTE_ETHER_ADDR_LEN);
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				       mac_addr);
		hns3_err(hw, "Failed to remove mc mac addr(%s) for vf: %d",
			 mac_str, ret);
	}

	return ret;
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
	 *    The hns3 VF device can be configured as trusted device by hns3 PF
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
	 *    the packets with vlan tag in promiscuous mode.
	 */
	hns3_cmd_setup_basic_desc(&desc, HNS3_OPC_MBX_VF_TO_PF, false);
	req->msg.code = HNS3_MBX_SET_PROMISC_MODE;
	req->msg.en_bc = en_bc_pmc ? 1 : 0;
	req->msg.en_uc = en_uc_pmc ? 1 : 0;
	req->msg.en_mc = en_mc_pmc ? 1 : 0;
	req->msg.en_limit_promisc =
		hw->promisc_mode == HNS3_LIMIT_PROMISC_MODE ? 1 : 0;

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
hns3vf_bind_ring_with_vector(struct hns3_hw *hw, uint16_t vector_id,
			     bool mmap, enum hns3_ring_type queue_type,
			     uint16_t queue_id)
{
	struct hns3_vf_to_pf_msg req = {0};
	const char *op_str;
	int ret;

	req.code = mmap ? HNS3_MBX_MAP_RING_TO_VECTOR :
		HNS3_MBX_UNMAP_RING_TO_VECTOR;
	req.vector_id = (uint8_t)vector_id;
	req.ring_num = 1;

	if (queue_type == HNS3_RING_TYPE_RX)
		req.ring_param[0].int_gl_index = HNS3_RING_GL_RX;
	else
		req.ring_param[0].int_gl_index = HNS3_RING_GL_TX;
	req.ring_param[0].ring_type = queue_type;
	req.ring_param[0].tqp_index = queue_id;
	op_str = mmap ? "Map" : "Unmap";
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
	if (ret)
		hns3_err(hw, "%s TQP %u fail, vector_id is %u, ret = %d.",
			 op_str, queue_id, req.vector_id, ret);

	return ret;
}

static int
hns3vf_dev_configure(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	enum rte_eth_rx_mq_mode mq_mode = conf->rxmode.mq_mode;
	uint16_t nb_rx_q = dev->data->nb_rx_queues;
	uint16_t nb_tx_q = dev->data->nb_tx_queues;
	struct rte_eth_rss_conf rss_conf;
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
	if (conf->link_speeds & RTE_ETH_LINK_SPEED_FIXED) {
		hns3_err(hw, "setting link speed/duplex not supported");
		ret = -EINVAL;
		goto cfg_err;
	}

	/* When RSS is not configured, redirect the packet queue 0 */
	if ((uint32_t)mq_mode & RTE_ETH_MQ_RX_RSS_FLAG) {
		conf->rxmode.offloads |= RTE_ETH_RX_OFFLOAD_RSS_HASH;
		rss_conf = conf->rx_adv_conf.rss_conf;
		ret = hns3_dev_rss_hash_update(dev, &rss_conf);
		if (ret)
			goto cfg_err;
	}

	ret = hns3vf_dev_mtu_set(dev, conf->rxmode.mtu);
	if (ret != 0)
		goto cfg_err;

	ret = hns3vf_dev_configure_vlan(dev);
	if (ret)
		goto cfg_err;

	/* config hardware GRO */
	gro_en = conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO ? true : false;
	ret = hns3_config_gro(hw, gro_en);
	if (ret)
		goto cfg_err;

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
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_SET_MTU, 0);
	memcpy(req.data, &mtu, sizeof(mtu));
	ret = hns3vf_mbx_send(hw, &req, true, NULL, 0);
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
	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED)) {
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
	rte_spinlock_unlock(&hw->lock);

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

void
hns3vf_clear_reset_event(struct hns3_hw *hw)
{
	uint32_t clearval;
	uint32_t cmdq_stat_reg;

	cmdq_stat_reg = hns3_read_dev(hw, HNS3_VECTOR0_CMDQ_STAT_REG);
	clearval = cmdq_stat_reg & ~BIT(HNS3_VECTOR0_RST_INT_B);
	hns3_write_dev(hw, HNS3_VECTOR0_CMDQ_SRC_REG, clearval);

	hns3vf_enable_irq0(hw);
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
		__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);
		val = hns3_read_dev(hw, HNS3_VF_RST_ING);
		hns3_write_dev(hw, HNS3_VF_RST_ING, val | HNS3_VF_RST_ING_BIT);
		val = cmdq_stat_reg & ~BIT(HNS3_VECTOR0_RST_INT_B);
		hw->reset.stats.global_cnt++;
		hns3_warn(hw, "Global reset detected, clear reset status");

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
		hns3vf_handle_mbx_msg(hw);
		break;
	default:
		break;
	}

	/* Enable interrupt if it is not caused by reset */
	if (event_cause == HNS3VF_VECTOR0_EVENT_MBX ||
	    event_cause == HNS3VF_VECTOR0_EVENT_OTHER)
		hns3vf_enable_irq0(hw);
}

void
hns3vf_update_push_lsc_cap(struct hns3_hw *hw, bool supported)
{
	uint16_t val = supported ? HNS3_PF_PUSH_LSC_CAP_SUPPORTED :
				   HNS3_PF_PUSH_LSC_CAP_NOT_SUPPORTED;
	uint16_t exp = HNS3_PF_PUSH_LSC_CAP_UNKNOWN;
	struct hns3_vf *vf = HNS3_DEV_HW_TO_VF(hw);

	if (vf->pf_push_lsc_cap == HNS3_PF_PUSH_LSC_CAP_UNKNOWN)
		__atomic_compare_exchange(&vf->pf_push_lsc_cap, &exp, &val, 0,
					  __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);
}

static void
hns3vf_get_push_lsc_cap(struct hns3_hw *hw)
{
#define HNS3_CHECK_PUSH_LSC_CAP_TIMEOUT_MS	500

	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	int32_t remain_ms = HNS3_CHECK_PUSH_LSC_CAP_TIMEOUT_MS;
	uint16_t val = HNS3_PF_PUSH_LSC_CAP_NOT_SUPPORTED;
	uint16_t exp = HNS3_PF_PUSH_LSC_CAP_UNKNOWN;
	struct hns3_vf *vf = HNS3_DEV_HW_TO_VF(hw);
	struct hns3_vf_to_pf_msg req;

	__atomic_store_n(&vf->pf_push_lsc_cap, HNS3_PF_PUSH_LSC_CAP_UNKNOWN,
			 __ATOMIC_RELEASE);

	hns3vf_mbx_setup(&req, HNS3_MBX_GET_LINK_STATUS, 0);
	(void)hns3vf_mbx_send(hw, &req, false, NULL, 0);

	while (remain_ms > 0) {
		rte_delay_ms(HNS3_POLL_RESPONE_MS);
		/*
		 * The probe process may perform in interrupt thread context.
		 * For example, users attach a device in the secondary process.
		 * At the moment, the handling mailbox task will be blocked. So
		 * driver has to actively handle the HNS3_MBX_LINK_STAT_CHANGE
		 * mailbox from PF driver to get this capability.
		 */
		hns3vf_handle_mbx_msg(hw);
		if (__atomic_load_n(&vf->pf_push_lsc_cap, __ATOMIC_ACQUIRE) !=
			HNS3_PF_PUSH_LSC_CAP_UNKNOWN)
			break;
		remain_ms--;
	}

	/*
	 * When exit above loop, the pf_push_lsc_cap could be one of the three
	 * state: unknown (means pf not ack), not_supported, supported.
	 * Here config it as 'not_supported' when it's 'unknown' state.
	 */
	__atomic_compare_exchange(&vf->pf_push_lsc_cap, &exp, &val, 0,
				  __ATOMIC_ACQUIRE, __ATOMIC_ACQUIRE);

	if (__atomic_load_n(&vf->pf_push_lsc_cap, __ATOMIC_ACQUIRE) ==
		HNS3_PF_PUSH_LSC_CAP_SUPPORTED) {
		hns3_info(hw, "detect PF support push link status change!");
	} else {
		/*
		 * Framework already set RTE_ETH_DEV_INTR_LSC bit because driver
		 * declared RTE_PCI_DRV_INTR_LSC in drv_flags. So here cleared
		 * the RTE_ETH_DEV_INTR_LSC capability.
		 */
		dev->data->dev_flags &= ~RTE_ETH_DEV_INTR_LSC;
	}
}

static int
hns3vf_get_capability(struct hns3_hw *hw)
{
	int ret;

	if (hw->revision < PCI_REVISION_ID_HIP09_A) {
		hns3_set_default_dev_specifications(hw);
		hw->intr.mapping_mode = HNS3_INTR_MAPPING_VEC_RSV_ONE;
		hw->intr.gl_unit = HNS3_INTR_COALESCE_GL_UINT_2US;
		hw->tso_mode = HNS3_TSO_SW_CAL_PSEUDO_H_CSUM;
		hw->drop_stats_mode = HNS3_PKTS_DROP_STATS_MODE1;
		hw->min_tx_pkt_len = HNS3_HIP08_MIN_TX_PKT_LEN;
		hw->rss_info.ipv6_sctp_offload_supported = false;
		hw->promisc_mode = HNS3_UNLIMIT_PROMISC_MODE;
		return 0;
	}

	ret = hns3_query_dev_specifications(hw);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "failed to query dev specifications, ret = %d",
			     ret);
		return ret;
	}

	hw->intr.mapping_mode = HNS3_INTR_MAPPING_VEC_ALL;
	hw->intr.gl_unit = HNS3_INTR_COALESCE_GL_UINT_1US;
	hw->tso_mode = HNS3_TSO_HW_CAL_PSEUDO_H_CSUM;
	hw->drop_stats_mode = HNS3_PKTS_DROP_STATS_MODE2;
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
	struct hns3_vf_to_pf_msg req;
	uint8_t resp_msg;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_SET_VLAN,
			 HNS3_MBX_GET_PORT_BASE_VLAN_STATE);
	ret = hns3vf_mbx_send(hw, &req, true, &resp_msg, sizeof(resp_msg));
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
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_GET_QINFO, 0);
	ret = hns3vf_mbx_send(hw, &req, true,
			      resp_msg, HNS3VF_TQPS_RSS_INFO_LEN);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to get tqp info from PF: %d", ret);
		return ret;
	}

	memcpy(&hw->tqps_num, &resp_msg[0], sizeof(uint16_t));
	memcpy(&hw->rss_size_max, &resp_msg[2], sizeof(uint16_t));

	return hns3vf_check_tqp_info(hw);
}

static void
hns3vf_update_caps(struct hns3_hw *hw, uint32_t caps)
{
	if (hns3_get_bit(caps, HNS3VF_CAPS_VLAN_FLT_MOD_B))
		hns3_set_bit(hw->capability,
				HNS3_DEV_SUPPORT_VF_VLAN_FLT_MOD_B, 1);
}

static int
hns3vf_get_num_tc(struct hns3_hw *hw)
{
	uint8_t num_tc = 0;
	uint32_t i;

	for (i = 0; i < HNS3_MAX_TC_NUM; i++) {
		if (hw->hw_tc_map & BIT(i))
			num_tc++;
	}
	return num_tc;
}

static int
hns3vf_get_basic_info(struct hns3_hw *hw)
{
	uint8_t resp_msg[HNS3_MBX_MAX_RESP_DATA_SIZE];
	struct hns3_basic_info *basic_info;
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_GET_BASIC_INFO, 0);
	ret = hns3vf_mbx_send(hw, &req, true, resp_msg, sizeof(resp_msg));
	if (ret) {
		hns3_err(hw, "failed to get basic info from PF, ret = %d.",
				ret);
		return ret;
	}

	basic_info = (struct hns3_basic_info *)resp_msg;
	hw->hw_tc_map = basic_info->hw_tc_map;
	hw->num_tc = hns3vf_get_num_tc(hw);
	hw->pf_vf_if_version = basic_info->pf_vf_if_version;
	hns3vf_update_caps(hw, basic_info->caps);

	return 0;
}

static int
hns3vf_get_host_mac_addr(struct hns3_hw *hw)
{
	uint8_t host_mac[RTE_ETHER_ADDR_LEN];
	struct hns3_vf_to_pf_msg req;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_GET_MAC_ADDR, 0);
	ret = hns3vf_mbx_send(hw, &req, true, host_mac, RTE_ETHER_ADDR_LEN);
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

	/* Get device capability */
	ret = hns3vf_get_capability(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to get device capability: %d.", ret);
		return ret;
	}

	hns3vf_get_push_lsc_cap(hw);

	/* Get basic info from PF */
	ret = hns3vf_get_basic_info(hw);
	if (ret)
		return ret;

	/* Get queue configuration from PF */
	ret = hns3vf_get_queue_info(hw);
	if (ret)
		return ret;

	/* Get user defined VF MAC addr from PF */
	ret = hns3vf_get_host_mac_addr(hw);
	if (ret)
		return ret;

	return hns3vf_get_port_base_vlan_filter_state(hw);
}

static void
hns3vf_request_link_info(struct hns3_hw *hw)
{
	struct hns3_vf *vf = HNS3_DEV_HW_TO_VF(hw);
	struct hns3_vf_to_pf_msg req;
	bool send_req;
	int ret;

	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED))
		return;

	send_req = vf->pf_push_lsc_cap == HNS3_PF_PUSH_LSC_CAP_NOT_SUPPORTED ||
		   vf->req_link_info_cnt > 0;
	if (!send_req)
		return;

	hns3vf_mbx_setup(&req, HNS3_MBX_GET_LINK_STATUS, 0);
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
	if (ret) {
		hns3_err(hw, "failed to fetch link status, ret = %d", ret);
		return;
	}

	if (vf->req_link_info_cnt > 0)
		vf->req_link_info_cnt--;
}

void
hns3vf_update_link_status(struct hns3_hw *hw, uint8_t link_status,
			  uint32_t link_speed, uint8_t link_duplex)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	struct hns3_vf *vf = HNS3_DEV_HW_TO_VF(hw);
	struct hns3_mac *mac = &hw->mac;
	int ret;

	/*
	 * PF kernel driver may push link status when VF driver is in resetting,
	 * driver will stop polling job in this case, after resetting done
	 * driver will start polling job again.
	 * When polling job started, driver will get initial link status by
	 * sending request to PF kernel driver, then could update link status by
	 * process PF kernel driver's link status mailbox message.
	 */
	if (!__atomic_load_n(&vf->poll_job_started, __ATOMIC_RELAXED))
		return;

	if (hw->adapter_state != HNS3_NIC_STARTED)
		return;

	mac->link_status = link_status;
	mac->link_speed = link_speed;
	mac->link_duplex = link_duplex;
	ret = hns3vf_dev_link_update(dev, 0);
	if (ret == 0 && dev->data->dev_conf.intr_conf.lsc != 0)
		hns3_start_report_lse(dev);
}

static int
hns3vf_vlan_filter_configure(struct hns3_adapter *hns, uint16_t vlan_id, int on)
{
	struct hns3_mbx_vlan_filter *vlan_filter;
	struct hns3_vf_to_pf_msg req = {0};
	struct hns3_hw *hw = &hns->hw;

	req.code = HNS3_MBX_SET_VLAN;
	req.subcode = HNS3_MBX_VLAN_FILTER;
	vlan_filter = (struct hns3_mbx_vlan_filter *)req.data;
	vlan_filter->is_kill = on ? 0 : 1;
	vlan_filter->proto = rte_cpu_to_le_16(RTE_ETHER_TYPE_VLAN);
	vlan_filter->vlan_id = rte_cpu_to_le_16(vlan_id);

	return hns3vf_mbx_send(hw, &req, true, NULL, 0);
}

static int
hns3vf_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED)) {
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
hns3vf_en_vlan_filter(struct hns3_hw *hw, bool enable)
{
	struct hns3_vf_to_pf_msg req;
	uint8_t msg_data;
	int ret;

	if (!hns3_dev_get_support(hw, VF_VLAN_FLT_MOD))
		return 0;

	msg_data = enable ? 1 : 0;
	hns3vf_mbx_setup(&req, HNS3_MBX_SET_VLAN,
			 HNS3_MBX_ENABLE_VLAN_FILTER);
	memcpy(req.data, &msg_data, sizeof(msg_data));
	ret = hns3vf_mbx_send(hw, &req, true, NULL, 0);
	if (ret)
		hns3_err(hw, "%s vlan filter failed, ret = %d.",
				enable ? "enable" : "disable", ret);

	return ret;
}

static int
hns3vf_en_hw_strip_rxvtag(struct hns3_hw *hw, bool enable)
{
	struct hns3_vf_to_pf_msg req;
	uint8_t msg_data;
	int ret;

	msg_data = enable ? 1 : 0;
	hns3vf_mbx_setup(&req, HNS3_MBX_SET_VLAN,
			 HNS3_MBX_VLAN_RX_OFF_CFG);
	memcpy(req.data, &msg_data, sizeof(msg_data));
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
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

	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED)) {
		hns3_err(hw, "vf set vlan offload failed during resetting, mask = 0x%x",
			 mask);
		return -EIO;
	}

	tmp_mask = (unsigned int)mask;

	if (tmp_mask & RTE_ETH_VLAN_FILTER_MASK) {
		rte_spinlock_lock(&hw->lock);
		/* Enable or disable VLAN filter */
		if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_VLAN_FILTER)
			ret = hns3vf_en_vlan_filter(hw, true);
		else
			ret = hns3vf_en_vlan_filter(hw, false);
		rte_spinlock_unlock(&hw->lock);
		if (ret)
			return ret;
	}

	/* Vlan stripping setting */
	if (tmp_mask & RTE_ETH_VLAN_STRIP_MASK) {
		rte_spinlock_lock(&hw->lock);
		/* Enable or disable VLAN stripping */
		if (dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
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
	en = dev_conf->rxmode.offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP ? true
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
	ret = hns3vf_vlan_offload_set(dev, RTE_ETH_VLAN_STRIP_MASK |
					RTE_ETH_VLAN_FILTER_MASK);
	if (ret)
		hns3_err(hw, "dev config vlan offload failed, ret = %d.", ret);

	return ret;
}

static int
hns3vf_set_alive(struct hns3_hw *hw, bool alive)
{
	struct hns3_vf_to_pf_msg req;
	uint8_t msg_data;

	msg_data = alive ? 1 : 0;
	hns3vf_mbx_setup(&req, HNS3_MBX_SET_ALIVE, 0);
	memcpy(req.data, &msg_data, sizeof(msg_data));
	return hns3vf_mbx_send(hw, &req, false, NULL, 0);
}

static void
hns3vf_keep_alive_handler(void *param)
{
	struct rte_eth_dev *eth_dev = (struct rte_eth_dev *)param;
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_vf_to_pf_msg req;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	hns3vf_mbx_setup(&req, HNS3_MBX_KEEP_ALIVE, 0);
	ret = hns3vf_mbx_send(hw, &req, false, NULL, 0);
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
	 * interrupt thread. When the IMP reset occurs, IMP will not respond,
	 * and the query operation will timeout after 30ms. In the case of
	 * multiple PF/VFs, each query failure timeout causes the IMP reset
	 * interrupt to fail to respond within 100ms.
	 * Before querying the link status, check whether there is a reset
	 * pending, and if so, abandon the query.
	 */
	if (!hns3vf_is_reset_pending(hns)) {
		hns3vf_request_link_info(hw);
		hns3_update_hw_stats(hw);
	} else {
		hns3_warn(hw, "Cancel the query when reset is pending");
	}

	rte_eal_alarm_set(HNS3VF_SERVICE_INTERVAL, hns3vf_service_handler,
			  eth_dev);
}

static void
hns3vf_start_poll_job(struct rte_eth_dev *dev)
{
#define HNS3_REQUEST_LINK_INFO_REMAINS_CNT	3

	struct hns3_vf *vf = HNS3_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	if (vf->pf_push_lsc_cap == HNS3_PF_PUSH_LSC_CAP_SUPPORTED)
		vf->req_link_info_cnt = HNS3_REQUEST_LINK_INFO_REMAINS_CNT;

	__atomic_store_n(&vf->poll_job_started, 1, __ATOMIC_RELAXED);

	hns3vf_service_handler(dev);
}

static void
hns3vf_stop_poll_job(struct rte_eth_dev *dev)
{
	struct hns3_vf *vf = HNS3_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	rte_eal_alarm_cancel(hns3vf_service_handler, dev);

	__atomic_store_n(&vf->poll_job_started, 0, __ATOMIC_RELAXED);
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
	ret = hns3_init_ring_with_vector(hw);
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
	struct hns3_vf_to_pf_msg req;

	hns3vf_mbx_setup(&req, HNS3_MBX_HANDLE_VF_TBL,
			 HNS3_MBX_VPORT_LIST_CLEAR);
	return hns3vf_mbx_send(hw, &req, false, NULL, 0);
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

	ret = hns3_get_pci_revision_id(hw, &hw->revision);
	if (ret)
		return ret;

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

	hns3_tx_push_init(eth_dev);

	/* Get VF resource */
	ret = hns3_query_vf_resource(hw);
	if (ret)
		goto err_cmd_init;

	rte_spinlock_init(&hw->mbx_resp.lock);

	hns3vf_clear_event_cause(hw, 0);

	ret = rte_intr_callback_register(pci_dev->intr_handle,
					 hns3vf_interrupt_handler, eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to register intr: %d", ret);
		goto err_intr_callback_register;
	}

	/* Enable interrupt */
	rte_intr_enable(pci_dev->intr_handle);
	hns3vf_enable_irq0(hw);

	/* Get configuration from PF */
	ret = hns3vf_get_configuration(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to fetch configuration: %d", ret);
		goto err_get_config;
	}

	ret = hns3_stats_init(hw);
	if (ret)
		goto err_get_config;

	ret = hns3_queue_to_tc_mapping(hw, hw->tqps_num, hw->tqps_num);
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

	hns3_rss_set_default_args(hw);

	ret = hns3vf_set_alive(hw, true);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to VF send alive to PF: %d", ret);
		goto err_set_tc_queue;
	}

	return 0;

err_set_tc_queue:
	hns3_stats_uninit(hw);

err_get_config:
	hns3vf_disable_irq0(hw);
	rte_intr_disable(pci_dev->intr_handle);
	hns3_intr_unregister(pci_dev->intr_handle, hns3vf_interrupt_handler,
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
	hns3_stats_uninit(hw);
	hns3vf_disable_irq0(hw);
	rte_intr_disable(pci_dev->intr_handle);
	hns3_intr_unregister(pci_dev->intr_handle, hns3vf_interrupt_handler,
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

	hw->mac.link_status = RTE_ETH_LINK_DOWN;

	/*
	 * The "hns3vf_do_stop" function will also be called by .stop_service to
	 * prepare reset. At the time of global or IMP reset, the command cannot
	 * be sent to stop the tx/rx queues. The mbuf in Tx/Rx queues may be
	 * accessed during the reset process. So the mbuf can not be released
	 * during reset and is required to be released after the reset is
	 * completed.
	 */
	if (__atomic_load_n(&hw->reset.resetting,  __ATOMIC_RELAXED) == 0)
		hns3_dev_release_mbufs(hns);

	if (__atomic_load_n(&hw->reset.disable_cmd, __ATOMIC_RELAXED) == 0) {
		hns3_configure_all_mac_addr(hns, true);
		ret = hns3_reset_all_tqps(hns);
		if (ret) {
			hns3_err(hw, "failed to reset all queues ret = %d",
				 ret);
			return ret;
		}
	}
	return 0;
}

static int
hns3vf_dev_stop(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();
	dev->data->dev_started = 0;

	hw->adapter_state = HNS3_NIC_STOPPING;
	hns3_stop_rxtx_datapath(dev);

	rte_spinlock_lock(&hw->lock);
	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED) == 0) {
		hns3_stop_tqps(hw);
		hns3vf_do_stop(hns);
		hns3_unmap_rx_interrupt(dev);
		hw->adapter_state = HNS3_NIC_CONFIGURED;
	}
	hns3_rx_scattered_reset(dev);
	hns3vf_stop_poll_job(dev);
	hns3_stop_report_lse(dev);
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
		hns3_mp_uninit(eth_dev);
		return 0;
	}

	if (hw->adapter_state == HNS3_NIC_STARTED)
		ret = hns3vf_dev_stop(eth_dev);

	hw->adapter_state = HNS3_NIC_CLOSING;
	hns3_reset_abort(hns);
	hw->adapter_state = HNS3_NIC_CLOSED;
	rte_eal_alarm_cancel(hns3vf_keep_alive_handler, eth_dev);
	hns3_configure_all_mc_mac_addr(hns, true);
	hns3vf_remove_all_vlan_table(hns);
	hns3vf_uninit_vf(eth_dev);
	hns3_free_all_queues(eth_dev);
	rte_free(hw->reset.wait_data);
	hns3_mp_uninit(eth_dev);
	hns3_warn(hw, "Close port %u finished", hw->data->port_id);

	return ret;
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
	case RTE_ETH_SPEED_NUM_10M:
	case RTE_ETH_SPEED_NUM_100M:
	case RTE_ETH_SPEED_NUM_1G:
	case RTE_ETH_SPEED_NUM_10G:
	case RTE_ETH_SPEED_NUM_25G:
	case RTE_ETH_SPEED_NUM_40G:
	case RTE_ETH_SPEED_NUM_50G:
	case RTE_ETH_SPEED_NUM_100G:
	case RTE_ETH_SPEED_NUM_200G:
		if (mac->link_status)
			new_link.link_speed = mac->link_speed;
		break;
	default:
		if (mac->link_status)
			new_link.link_speed = RTE_ETH_SPEED_NUM_UNKNOWN;
		break;
	}

	if (!mac->link_status)
		new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;

	new_link.link_duplex = mac->link_duplex;
	new_link.link_status = mac->link_status ? RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;
	new_link.link_autoneg =
	    !(eth_dev->data->dev_conf.link_speeds & RTE_ETH_LINK_SPEED_FIXED);

	return rte_eth_linkstatus_set(eth_dev, &new_link);
}

static int
hns3vf_do_start(struct hns3_adapter *hns, bool reset_queue)
{
	struct hns3_hw *hw = &hns->hw;
	uint16_t nb_rx_q = hw->data->nb_rx_queues;
	uint16_t nb_tx_q = hw->data->nb_tx_queues;
	int ret;

	ret = hns3_queue_to_tc_mapping(hw, nb_rx_q, nb_tx_q);
	if (ret)
		return ret;

	hns3_enable_rxd_adv_layout(hw);

	ret = hns3_init_queues(hns, reset_queue);
	if (ret) {
		hns3_err(hw, "failed to init queues, ret = %d.", ret);
		return ret;
	}

	return hns3_restore_filter(hns);
}

static int
hns3vf_dev_start(struct rte_eth_dev *dev)
{
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();
	if (__atomic_load_n(&hw->reset.resetting, __ATOMIC_RELAXED))
		return -EBUSY;

	rte_spinlock_lock(&hw->lock);
	hw->adapter_state = HNS3_NIC_STARTING;
	ret = hns3vf_do_start(hns, true);
	if (ret) {
		hw->adapter_state = HNS3_NIC_CONFIGURED;
		rte_spinlock_unlock(&hw->lock);
		return ret;
	}
	ret = hns3_map_rx_interrupt(dev);
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
	hns3_start_rxtx_datapath(dev);

	/* Enable interrupt of all rx queues before enabling queues */
	hns3_dev_all_rx_queue_intr_enable(hw, true);
	hns3_start_tqps(hw);

	if (dev->data->dev_conf.intr_conf.lsc != 0)
		hns3vf_dev_link_update(dev, 0);
	hns3vf_start_poll_job(dev);

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

static enum hns3_reset_level
hns3vf_detect_reset_event(struct hns3_hw *hw)
{
	enum hns3_reset_level reset = HNS3_NONE_RESET;
	uint32_t cmdq_stat_reg;

	cmdq_stat_reg = hns3_read_dev(hw, HNS3_VECTOR0_CMDQ_STAT_REG);
	if (BIT(HNS3_VECTOR0_RST_INT_B) & cmdq_stat_reg)
		reset = HNS3_VF_RESET;

	return reset;
}

bool
hns3vf_is_reset_pending(struct hns3_adapter *hns)
{
	enum hns3_reset_level last_req;
	struct hns3_hw *hw = &hns->hw;
	enum hns3_reset_level new_req;

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

	/*
	 * Only primary can process can process the reset event,
	 * so don't check reset event in secondary.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return false;

	new_req = hns3vf_detect_reset_event(hw);
	if (new_req == HNS3_NONE_RESET)
		return false;

	last_req = hns3vf_get_reset_level(hw, &hw->reset.pending);
	if (last_req == HNS3_NONE_RESET || last_req < new_req) {
		__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);
		hns3_schedule_delayed_reset(hns);
		hns3_warn(hw, "High level reset detected, delay do reset");
		return true;
	}

	return false;
}

static int
hns3vf_wait_hardware_ready(struct hns3_adapter *hns)
{
#define HNS3_WAIT_PF_RESET_READY_TIME 5
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
		wait_data->interval = HNS3_WAIT_PF_RESET_READY_TIME *
			MSEC_PER_SEC * USEC_PER_MSEC;
		wait_data->count = 1;
		wait_data->result = HNS3_WAIT_REQUEST;
		rte_eal_alarm_set(wait_data->interval, hns3_wait_callback,
				  wait_data);
		hns3_warn(hw, "hardware is ready, delay %d sec for PF reset complete",
				HNS3_WAIT_PF_RESET_READY_TIME);
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
	struct hns3_vf_to_pf_msg req;
	struct hns3_hw *hw = &hns->hw;
	int ret;

	if (hw->reset.level == HNS3_VF_FUNC_RESET) {
		hns3vf_mbx_setup(&req, HNS3_MBX_RESET, 0);
		ret = hns3vf_mbx_send(hw, &req, true, NULL, 0);
		if (ret)
			return ret;
	}
	__atomic_store_n(&hw->reset.disable_cmd, 1, __ATOMIC_RELAXED);

	return 0;
}

static int
hns3vf_stop_service(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_dev *eth_dev;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	if (hw->adapter_state == HNS3_NIC_STARTED) {
		/*
		 * Make sure call update link status before hns3vf_stop_poll_job
		 * because update link status depend on polling job exist.
		 */
		hns3vf_update_link_status(hw, RTE_ETH_LINK_DOWN, hw->mac.link_speed,
					  hw->mac.link_duplex);
		hns3vf_stop_poll_job(eth_dev);
	}
	hw->mac.link_status = RTE_ETH_LINK_DOWN;

	hns3_stop_rxtx_datapath(eth_dev);

	rte_spinlock_lock(&hw->lock);
	if (hw->adapter_state == HNS3_NIC_STARTED ||
	    hw->adapter_state == HNS3_NIC_STOPPING) {
		hns3_enable_all_queues(hw, false);
		hns3vf_do_stop(hns);
		hw->reset.mbuf_deferred_free = true;
	} else
		hw->reset.mbuf_deferred_free = false;

	rte_eal_alarm_cancel(hns3vf_keep_alive_handler, eth_dev);

	/*
	 * It is cumbersome for hardware to pick-and-choose entries for deletion
	 * from table space. Hence, for function reset software intervention is
	 * required to delete the entries.
	 */
	if (__atomic_load_n(&hw->reset.disable_cmd, __ATOMIC_RELAXED) == 0)
		hns3_configure_all_mc_mac_addr(hns, true);
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

static int
hns3vf_start_service(struct hns3_adapter *hns)
{
	struct hns3_hw *hw = &hns->hw;
	struct rte_eth_dev *eth_dev;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	hns3_start_rxtx_datapath(eth_dev);

	rte_eal_alarm_set(HNS3VF_KEEP_ALIVE_INTERVAL, hns3vf_keep_alive_handler,
			  eth_dev);

	if (hw->adapter_state == HNS3_NIC_STARTED) {
		hns3vf_start_poll_job(eth_dev);

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
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
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

	ret = hns3_configure_all_mac_addr(hns, false);
	if (ret)
		return ret;

	ret = hns3_configure_all_mc_mac_addr(hns, false);
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

	ret = hns3_restore_rx_interrupt(hw);
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
	hns3_configure_all_mc_mac_addr(hns, true);
err_mc_mac:
	hns3_configure_all_mac_addr(hns, true);
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
	if (__atomic_load_n(&hw->reset.schedule, __ATOMIC_RELAXED) ==
			    SCHEDULE_DEFERRED) {
		__atomic_store_n(&hw->reset.schedule, SCHEDULE_REQUESTED,
				 __ATOMIC_RELAXED);
		hns3_err(hw, "Handling interrupts in delayed tasks");
		hns3vf_interrupt_handler(&rte_eth_devices[hw->data->port_id]);
		reset_level = hns3vf_get_reset_level(hw, &hw->reset.pending);
		if (reset_level == HNS3_NONE_RESET) {
			hns3_err(hw, "No reset level is set, try global reset");
			hns3_atomic_set_bit(HNS3_VF_RESET, &hw->reset.pending);
		}
	}
	__atomic_store_n(&hw->reset.schedule, SCHEDULE_NONE, __ATOMIC_RELAXED);

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
		rte_intr_disable(pci_dev->intr_handle);
		ret = rte_pci_set_bus_master(pci_dev, true);
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
			ret = hns3vf_enable_msix(pci_dev, true);
			if (ret != 0) {
				hns3_err(hw, "Failed to enable msix");
				return ret;
			}
		}

		rte_intr_enable(pci_dev->intr_handle);
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
	.dev_infos_get      = hns3_dev_infos_get,
	.fw_version_get     = hns3_fw_version_get,
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
	.mac_addr_add       = hns3_add_mac_addr,
	.mac_addr_remove    = hns3_remove_mac_addr,
	.mac_addr_set       = hns3vf_set_default_mac_addr,
	.set_mc_addr_list   = hns3_set_mc_mac_addr_list,
	.link_update        = hns3vf_dev_link_update,
	.rss_hash_update    = hns3_dev_rss_hash_update,
	.rss_hash_conf_get  = hns3_dev_rss_hash_conf_get,
	.reta_update        = hns3_dev_rss_reta_update,
	.reta_query         = hns3_dev_rss_reta_query,
	.flow_ops_get       = hns3_dev_flow_ops_get,
	.vlan_filter_set    = hns3vf_vlan_filter_set,
	.vlan_offload_set   = hns3vf_vlan_offload_set,
	.get_reg            = hns3_get_regs,
	.dev_supported_ptypes_get = hns3_dev_supported_ptypes_get,
	.tx_done_cleanup    = hns3_tx_done_cleanup,
	.eth_dev_priv_dump  = hns3_eth_dev_priv_dump,
	.eth_rx_descriptor_dump = hns3_rx_descriptor_dump,
	.eth_tx_descriptor_dump = hns3_tx_descriptor_dump,
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

static void
hns3vf_init_hw_ops(struct hns3_hw *hw)
{
	hw->ops.add_mc_mac_addr = hns3vf_add_mc_mac_addr;
	hw->ops.del_mc_mac_addr = hns3vf_remove_mc_mac_addr;
	hw->ops.add_uc_mac_addr = hns3vf_add_uc_mac_addr;
	hw->ops.del_uc_mac_addr = hns3vf_remove_uc_mac_addr;
	hw->ops.bind_ring_with_vector = hns3vf_bind_ring_with_vector;
}

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
	ret = hns3_mp_init(eth_dev);
	if (ret)
		goto err_mp_init;

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		hns3_tx_push_init(eth_dev);
		return 0;
	}

	hw->adapter_state = HNS3_NIC_UNINITIALIZED;
	hns->is_vf = true;
	hw->data = eth_dev->data;
	hns3_parse_devargs(eth_dev);

	ret = hns3_reset_init(hw);
	if (ret)
		goto err_init_reset;
	hw->reset.ops = &hns3vf_reset_ops;

	hns3vf_init_hw_ops(hw);
	ret = hns3vf_init_vf(eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init vf: %d", ret);
		goto err_init_vf;
	}

	ret = hns3_init_mac_addrs(eth_dev);
	if (ret != 0)
		goto err_init_mac_addrs;

	hw->adapter_state = HNS3_NIC_INITIALIZED;

	if (__atomic_load_n(&hw->reset.schedule, __ATOMIC_RELAXED) ==
			    SCHEDULE_PENDING) {
		hns3_err(hw, "Reschedule reset service after dev_init");
		hns3_schedule_reset(hns);
	} else {
		/* IMP will wait ready flag before reset */
		hns3_notify_reset_ready(hw, false);
	}
	rte_eal_alarm_set(HNS3VF_KEEP_ALIVE_INTERVAL, hns3vf_keep_alive_handler,
			  eth_dev);
	return 0;

err_init_mac_addrs:
	hns3vf_uninit_vf(eth_dev);

err_init_vf:
	rte_free(hw->reset.wait_data);

err_init_reset:
	hns3_mp_uninit(eth_dev);

err_mp_init:
	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->rx_descriptor_status = NULL;
	eth_dev->tx_pkt_burst = NULL;
	eth_dev->tx_pkt_prepare = NULL;
	eth_dev->tx_descriptor_status = NULL;

	return ret;
}

static int
hns3vf_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		hns3_mp_uninit(eth_dev);
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
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC,
	.probe = eth_hns3vf_pci_probe,
	.remove = eth_hns3vf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_hns3_vf, rte_hns3vf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_hns3_vf, pci_id_hns3vf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_hns3_vf, "* igb_uio | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_hns3_vf,
		HNS3_DEVARG_RX_FUNC_HINT "=vec|sve|simple|common "
		HNS3_DEVARG_TX_FUNC_HINT "=vec|sve|simple|common "
		HNS3_DEVARG_DEV_CAPS_MASK "=<1-65535> "
		HNS3_DEVARG_MBX_TIME_LIMIT_MS "=<uint16_t> ");
