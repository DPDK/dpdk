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
#include <rte_spinlock.h>

#include <rte_io.h>
#include <rte_rawdev.h>
#include <rte_rawdev_pmd.h>
#include <rte_bus_ifpga.h>
#include <ifpga_logs.h>

#include "ipn3ke_rawdev_api.h"
#include "ipn3ke_flow.h"
#include "ipn3ke_logs.h"
#include "ipn3ke_ethdev.h"

static int ipn3ke_rpst_scan_num;
static pthread_t ipn3ke_rpst_scan_thread;

/** Double linked list of representor port. */
TAILQ_HEAD(ipn3ke_rpst_list, ipn3ke_rpst);

static struct ipn3ke_rpst_list ipn3ke_rpst_list =
	TAILQ_HEAD_INITIALIZER(ipn3ke_rpst_list);

static rte_spinlock_t ipn3ke_link_notify_list_lk = RTE_SPINLOCK_INITIALIZER;

static int
ipn3ke_rpst_link_check(struct ipn3ke_rpst *rpst);

static void
ipn3ke_rpst_dev_infos_get(struct rte_eth_dev *ethdev,
	struct rte_eth_dev_info *dev_info)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);

	dev_info->speed_capa =
		(hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) ?
		ETH_LINK_SPEED_10G :
		((hw->retimer.mac_type ==
			IFPGA_RAWDEV_RETIMER_MAC_TYPE_25GE_25GAUI) ?
		ETH_LINK_SPEED_25G :
		ETH_LINK_SPEED_AUTONEG);

	dev_info->max_rx_queues  = 1;
	dev_info->max_tx_queues  = 1;
	dev_info->min_rx_bufsize = IPN3KE_AFU_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen  = IPN3KE_AFU_FRAME_SIZE_MAX;
	dev_info->max_mac_addrs  = hw->port_num;
	dev_info->max_vfs = 0;
	dev_info->default_txconf = (struct rte_eth_txconf) {
		.offloads = 0,
	};
	dev_info->rx_queue_offload_capa = 0;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_RX_OFFLOAD_VLAN_EXTEND |
		DEV_RX_OFFLOAD_VLAN_FILTER |
		DEV_RX_OFFLOAD_JUMBO_FRAME;

	dev_info->tx_queue_offload_capa = DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_QINQ_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO |
		DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
		DEV_TX_OFFLOAD_GRE_TNL_TSO |
		DEV_TX_OFFLOAD_IPIP_TNL_TSO |
		DEV_TX_OFFLOAD_GENEVE_TNL_TSO |
		DEV_TX_OFFLOAD_MULTI_SEGS |
		dev_info->tx_queue_offload_capa;

	dev_info->dev_capa =
		RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
		RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP;

	dev_info->switch_info.name = ethdev->device->name;
	dev_info->switch_info.domain_id = rpst->switch_domain_id;
	dev_info->switch_info.port_id = rpst->port_id;
}

static int
ipn3ke_rpst_dev_configure(__rte_unused struct rte_eth_dev *dev)
{
	return 0;
}

static int
ipn3ke_rpst_dev_start(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	struct rte_rawdev *rawdev;
	uint64_t base_mac;
	uint32_t val;
	char attr_name[IPN3KE_RAWDEV_ATTR_LEN_MAX];

	rawdev = hw->rawdev;

	memset(attr_name, 0, sizeof(attr_name));
	snprintf(attr_name, IPN3KE_RAWDEV_ATTR_LEN_MAX, "%s",
			"LineSideBaseMAC");
	rawdev->dev_ops->attr_get(rawdev, attr_name, &base_mac);
	ether_addr_copy((struct ether_addr *)&base_mac, &rpst->mac_addr);

	ether_addr_copy(&rpst->mac_addr, &dev->data->mac_addrs[0]);
	dev->data->mac_addrs->addr_bytes[ETHER_ADDR_LEN - 1] =
		(uint8_t)rpst->port_id + 1;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Set mac address */
		rte_memcpy(((char *)(&val)),
			(char *)&dev->data->mac_addrs->addr_bytes[0],
			sizeof(uint32_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR0,
				rpst->port_id,
				0);
		rte_memcpy(((char *)(&val)),
			(char *)&dev->data->mac_addrs->addr_bytes[4],
			sizeof(uint16_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR1,
				rpst->port_id,
				0);

		/* Enable the TX path */
		ipn3ke_xmac_tx_enable(hw, rpst->port_id, 0);

		/* Disables source address override */
		ipn3ke_xmac_smac_ovd_dis(hw, rpst->port_id, 0);

		/* Enable the RX path */
		ipn3ke_xmac_rx_enable(hw, rpst->port_id, 0);

		/* Clear all TX statistics counters */
		ipn3ke_xmac_tx_clr_stcs(hw, rpst->port_id, 0);

		/* Clear all RX statistics counters */
		ipn3ke_xmac_rx_clr_stcs(hw, rpst->port_id, 0);
	}

	ipn3ke_rpst_link_update(dev, 0);

	return 0;
}

static void
ipn3ke_rpst_dev_stop(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable the TX path */
		ipn3ke_xmac_tx_disable(hw, rpst->port_id, 0);

		/* Disable the RX path */
		ipn3ke_xmac_rx_disable(hw, rpst->port_id, 0);
	}
}

static void
ipn3ke_rpst_dev_close(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable the TX path */
		ipn3ke_xmac_tx_disable(hw, rpst->port_id, 0);

		/* Disable the RX path */
		ipn3ke_xmac_rx_disable(hw, rpst->port_id, 0);
	}
}

/*
 * Reset PF device only to re-initialize resources in PMD layer
 */
static int
ipn3ke_rpst_dev_reset(struct rte_eth_dev *dev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(dev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable the TX path */
		ipn3ke_xmac_tx_disable(hw, rpst->port_id, 0);

		/* Disable the RX path */
		ipn3ke_xmac_rx_disable(hw, rpst->port_id, 0);
	}

	return 0;
}

static int
ipn3ke_rpst_rx_queue_start(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_rx_queue_stop(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t rx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_tx_queue_start(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t tx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_tx_queue_stop(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t tx_queue_id)
{
	return 0;
}

static int
ipn3ke_rpst_rx_queue_setup(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t queue_idx, __rte_unused uint16_t nb_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_rxconf *rx_conf,
	__rte_unused struct rte_mempool *mp)
{
	return 0;
}

static void
ipn3ke_rpst_rx_queue_release(__rte_unused void *rxq)
{
}

static int
ipn3ke_rpst_tx_queue_setup(__rte_unused struct rte_eth_dev *dev,
	__rte_unused uint16_t queue_idx, __rte_unused uint16_t nb_desc,
	__rte_unused unsigned int socket_id,
	__rte_unused const struct rte_eth_txconf *tx_conf)
{
	return 0;
}

static void
ipn3ke_rpst_tx_queue_release(__rte_unused void *txq)
{
}

static int
ipn3ke_rpst_stats_get(__rte_unused struct rte_eth_dev *ethdev,
	__rte_unused struct rte_eth_stats *stats)
{
	return 0;
}

static int
ipn3ke_rpst_xstats_get(__rte_unused struct rte_eth_dev *dev,
	__rte_unused struct rte_eth_xstat *xstats, __rte_unused unsigned int n)
{
	return 0;
}

static int
ipn3ke_rpst_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
		__rte_unused struct rte_eth_xstat_name *xstats_names,
		__rte_unused unsigned int limit)
{
	return 0;
}

static void
ipn3ke_rpst_stats_reset(__rte_unused struct rte_eth_dev *ethdev)
{
}

static void
ipn3ke_update_link(struct rte_rawdev *rawdev,
	uint16_t port, struct rte_eth_link *link)
{
	uint64_t line_link_bitmap = 0;
	enum ifpga_rawdev_link_speed link_speed;

	rawdev->dev_ops->attr_get(rawdev,
				"LineSideLinkStatus",
				(uint64_t *)&line_link_bitmap);

	/* Parse the link status */
	if ((1 << port) & line_link_bitmap)
		link->link_status = 1;
	else
		link->link_status = 0;

	IPN3KE_AFU_PMD_DEBUG("port is %d\n", port);
	IPN3KE_AFU_PMD_DEBUG("link->link_status is %d\n", link->link_status);

	rawdev->dev_ops->attr_get(rawdev,
				"LineSideLinkSpeed",
				(uint64_t *)&link_speed);
	switch (link_speed) {
	case IFPGA_RAWDEV_LINK_SPEED_10GB:
		link->link_speed = ETH_SPEED_NUM_10G;
		break;
	case IFPGA_RAWDEV_LINK_SPEED_25GB:
		link->link_speed = ETH_SPEED_NUM_25G;
		break;
	default:
		IPN3KE_AFU_PMD_ERR("Unknown link speed info %u", link_speed);
		break;
	}
}

/*
 * Set device link up.
 */
int
ipn3ke_rpst_dev_set_link_up(struct rte_eth_dev *dev)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	struct rte_eth_dev *pf;
	int ret = 0;

	if (rpst->i40e_pf_eth) {
		ret = rte_eth_dev_set_link_up(rpst->i40e_pf_eth_port_id);
		pf = rpst->i40e_pf_eth;
		(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
	}

	return ret;
}

/*
 * Set device link down.
 */
int
ipn3ke_rpst_dev_set_link_down(struct rte_eth_dev *dev)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(dev);
	struct rte_eth_dev *pf;
	int ret = 0;

	if (rpst->i40e_pf_eth) {
		ret = rte_eth_dev_set_link_down(rpst->i40e_pf_eth_port_id);
		pf = rpst->i40e_pf_eth;
		(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
	}

	return ret;
}

int
ipn3ke_rpst_link_update(struct rte_eth_dev *ethdev,
	__rte_unused int wait_to_complete)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct rte_rawdev *rawdev;
	struct rte_eth_link link;
	struct rte_eth_dev *pf;

	memset(&link, 0, sizeof(link));

	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = !(ethdev->data->dev_conf.link_speeds &
				ETH_LINK_SPEED_FIXED);

	rawdev = hw->rawdev;
	ipn3ke_update_link(rawdev, rpst->port_id, &link);

	if (!rpst->ori_linfo.link_status &&
		link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Update Rpst %d Up\n", rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Update FVL PF %d Up\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_up(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	} else if (rpst->ori_linfo.link_status &&
				!link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Update Rpst %d Down\n",
			rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Update FVL PF %d Down\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_down(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	}

	return 0;
}

static int
ipn3ke_rpst_link_check(struct ipn3ke_rpst *rpst)
{
	struct ipn3ke_hw *hw;
	struct rte_rawdev *rawdev;
	struct rte_eth_link link;
	struct rte_eth_dev *pf;

	if (rpst == NULL)
		return -1;

	hw = rpst->hw;

	memset(&link, 0, sizeof(link));

	link.link_duplex = ETH_LINK_FULL_DUPLEX;
	link.link_autoneg = !(rpst->ethdev->data->dev_conf.link_speeds &
				ETH_LINK_SPEED_FIXED);

	rawdev = hw->rawdev;
	ipn3ke_update_link(rawdev, rpst->port_id, &link);

	if (!rpst->ori_linfo.link_status &&
				link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Check Rpst %d Up\n", rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(rpst->ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Check FVL PF %d Up\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_up(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	} else if (rpst->ori_linfo.link_status &&
		!link.link_status) {
		IPN3KE_AFU_PMD_DEBUG("Check Rpst %d Down\n", rpst->port_id);
		rpst->ori_linfo.link_status = link.link_status;
		rpst->ori_linfo.link_speed = link.link_speed;

		rte_eth_linkstatus_set(rpst->ethdev, &link);

		if (rpst->i40e_pf_eth) {
			IPN3KE_AFU_PMD_DEBUG("Check FVL PF %d Down\n",
				rpst->i40e_pf_eth_port_id);
			rte_eth_dev_set_link_down(rpst->i40e_pf_eth_port_id);
			pf = rpst->i40e_pf_eth;
			(*rpst->i40e_pf_eth->dev_ops->link_update)(pf, 1);
		}
	}

	return 0;
}

static void *
ipn3ke_rpst_scan_handle_request(__rte_unused void *param)
{
	struct ipn3ke_rpst *rpst;
	int num = 0;
#define MS 1000
#define SCAN_NUM 32

	for (;;) {
		num = 0;
		TAILQ_FOREACH(rpst, &ipn3ke_rpst_list, next) {
			if (rpst->i40e_pf_eth &&
				rpst->ethdev->data->dev_started &&
				rpst->i40e_pf_eth->data->dev_started)
				ipn3ke_rpst_link_check(rpst);

			if (++num > SCAN_NUM)
				rte_delay_us(1 * MS);
		}
		rte_delay_us(50 * MS);

		if (num == 0xffffff)
			return NULL;
	}

	return NULL;
}

static int
ipn3ke_rpst_scan_check(void)
{
	int ret;

	if (ipn3ke_rpst_scan_num == 1) {
		ret = pthread_create(&ipn3ke_rpst_scan_thread,
			NULL,
			ipn3ke_rpst_scan_handle_request, NULL);
		if (ret) {
			IPN3KE_AFU_PMD_ERR("Fail to create ipn3ke rpst scan thread");
			return -1;
		}
	} else if (ipn3ke_rpst_scan_num == 0) {
		ret = pthread_cancel(ipn3ke_rpst_scan_thread);
		if (ret)
			IPN3KE_AFU_PMD_ERR("Can't cancel the thread");

		ret = pthread_join(ipn3ke_rpst_scan_thread, NULL);
		if (ret)
			IPN3KE_AFU_PMD_ERR("Can't join the thread");

		return ret;
	}

	return 0;
}

void
ipn3ke_rpst_promiscuous_enable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Enable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 1;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLUCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}
}

void
ipn3ke_rpst_promiscuous_disable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 0;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLUCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}
}

void
ipn3ke_rpst_allmulticast_enable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Enable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 1;
		val <<= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_SHIFT;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}
}

void
ipn3ke_rpst_allmulticast_disable(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t rddata, val;

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		/* Disable all unicast */
		(*hw->f_mac_read)(hw,
				&rddata,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
		val = 0;
		val <<= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_SHIFT;
		val &= IPN3KE_MAC_RX_FRAME_CONTROL_EN_ALLMCAST_MASK;
		val |= rddata;
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_RX_FRAME_CONTROL,
				rpst->port_id,
				0);
	}
}

int
ipn3ke_rpst_mac_addr_set(struct rte_eth_dev *ethdev,
				struct ether_addr *mac_addr)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	uint32_t val;

	if (!is_valid_assigned_ether_addr(mac_addr)) {
		IPN3KE_AFU_PMD_ERR("Tried to set invalid MAC address.");
		return -EINVAL;
	}

	if (hw->retimer.mac_type == IFPGA_RAWDEV_RETIMER_MAC_TYPE_10GE_XFI) {
		ether_addr_copy(&mac_addr[0], &rpst->mac_addr);

		/* Set mac address */
		rte_memcpy(((char *)(&val)), &mac_addr[0], sizeof(uint32_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR0,
				rpst->port_id,
				0);
		rte_memcpy(((char *)(&val)), &mac_addr[4], sizeof(uint16_t));
		(*hw->f_mac_write)(hw,
				val,
				IPN3KE_MAC_PRIMARY_MAC_ADDR0,
				rpst->port_id,
				0);
	}

	return 0;
}

int
ipn3ke_rpst_mtu_set(struct rte_eth_dev *ethdev, uint16_t mtu)
{
	int ret = 0;
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct rte_eth_dev_data *dev_data = ethdev->data;
	uint32_t frame_size = mtu  + IPN3KE_ETH_OVERHEAD;

	/* check if mtu is within the allowed range */
	if (mtu < ETHER_MIN_MTU ||
		frame_size > IPN3KE_MAC_FRAME_SIZE_MAX)
		return -EINVAL;

	/* mtu setting is forbidden if port is start */
	/* make sure NIC port is stopped */
	if (rpst->i40e_pf_eth && rpst->i40e_pf_eth->data->dev_started) {
		IPN3KE_AFU_PMD_ERR("NIC port %d must "
			"be stopped before configuration",
			rpst->i40e_pf_eth->data->port_id);
		return -EBUSY;
	}
	/* mtu setting is forbidden if port is start */
	if (dev_data->dev_started) {
		IPN3KE_AFU_PMD_ERR("FPGA port %d must "
			"be stopped before configuration",
			dev_data->port_id);
		return -EBUSY;
	}

	if (frame_size > ETHER_MAX_LEN)
		dev_data->dev_conf.rxmode.offloads |=
			(uint64_t)(DEV_RX_OFFLOAD_JUMBO_FRAME);
	else
		dev_data->dev_conf.rxmode.offloads &=
			(uint64_t)(~DEV_RX_OFFLOAD_JUMBO_FRAME);

	dev_data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

	if (rpst->i40e_pf_eth) {
		ret = rpst->i40e_pf_eth->dev_ops->mtu_set(rpst->i40e_pf_eth,
							mtu);
		if (!ret)
			rpst->i40e_pf_eth->data->mtu = mtu;
	}

	return ret;
}

static int
ipn3ke_afu_filter_ctrl(struct rte_eth_dev *ethdev,
	enum rte_filter_type filter_type, enum rte_filter_op filter_op,
	void *arg)
{
	struct ipn3ke_hw *hw = IPN3KE_DEV_PRIVATE_TO_HW(ethdev);
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	int ret = 0;

	if (ethdev == NULL)
		return -EINVAL;

	if (hw->acc_flow)
		switch (filter_type) {
		case RTE_ETH_FILTER_GENERIC:
			if (filter_op != RTE_ETH_FILTER_GET)
				return -EINVAL;
			*(const void **)arg = &ipn3ke_flow_ops;
			break;
		default:
			IPN3KE_AFU_PMD_WARN("Filter type (%d) not supported",
					filter_type);
			ret = -EINVAL;
			break;
		}
	else if (rpst->i40e_pf_eth)
		(*rpst->i40e_pf_eth->dev_ops->filter_ctrl)(ethdev,
							filter_type,
							filter_op,
							arg);
	else
		return -EINVAL;

	return ret;
}

static const struct eth_dev_ops ipn3ke_rpst_dev_ops = {
	.dev_infos_get        = ipn3ke_rpst_dev_infos_get,

	.dev_configure        = ipn3ke_rpst_dev_configure,
	.dev_start            = ipn3ke_rpst_dev_start,
	.dev_stop             = ipn3ke_rpst_dev_stop,
	.dev_close            = ipn3ke_rpst_dev_close,
	.dev_reset            = ipn3ke_rpst_dev_reset,

	.stats_get            = ipn3ke_rpst_stats_get,
	.xstats_get           = ipn3ke_rpst_xstats_get,
	.xstats_get_names     = ipn3ke_rpst_xstats_get_names,
	.stats_reset          = ipn3ke_rpst_stats_reset,
	.xstats_reset         = ipn3ke_rpst_stats_reset,

	.filter_ctrl          = ipn3ke_afu_filter_ctrl,

	.rx_queue_start       = ipn3ke_rpst_rx_queue_start,
	.rx_queue_stop        = ipn3ke_rpst_rx_queue_stop,
	.tx_queue_start       = ipn3ke_rpst_tx_queue_start,
	.tx_queue_stop        = ipn3ke_rpst_tx_queue_stop,
	.rx_queue_setup       = ipn3ke_rpst_rx_queue_setup,
	.rx_queue_release     = ipn3ke_rpst_rx_queue_release,
	.tx_queue_setup       = ipn3ke_rpst_tx_queue_setup,
	.tx_queue_release     = ipn3ke_rpst_tx_queue_release,

	.dev_set_link_up      = ipn3ke_rpst_dev_set_link_up,
	.dev_set_link_down    = ipn3ke_rpst_dev_set_link_down,
	.link_update          = ipn3ke_rpst_link_update,

	.promiscuous_enable   = ipn3ke_rpst_promiscuous_enable,
	.promiscuous_disable  = ipn3ke_rpst_promiscuous_disable,
	.allmulticast_enable  = ipn3ke_rpst_allmulticast_enable,
	.allmulticast_disable = ipn3ke_rpst_allmulticast_disable,
	.mac_addr_set         = ipn3ke_rpst_mac_addr_set,
	.mtu_set              = ipn3ke_rpst_mtu_set,

	.tm_ops_get           = ipn3ke_tm_ops_get,
};

static uint16_t ipn3ke_rpst_recv_pkts(__rte_unused void *rx_q,
	__rte_unused struct rte_mbuf **rx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

static uint16_t
ipn3ke_rpst_xmit_pkts(__rte_unused void *tx_queue,
	__rte_unused struct rte_mbuf **tx_pkts, __rte_unused uint16_t nb_pkts)
{
	return 0;
}

int
ipn3ke_rpst_init(struct rte_eth_dev *ethdev, void *init_params)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);
	struct ipn3ke_rpst *representor_param =
			(struct ipn3ke_rpst *)init_params;

	if (representor_param->port_id >= representor_param->hw->port_num)
		return -ENODEV;

	rpst->ethdev = ethdev;
	rpst->switch_domain_id = representor_param->switch_domain_id;
	rpst->port_id = representor_param->port_id;
	rpst->hw = representor_param->hw;
	rpst->i40e_pf_eth = NULL;
	rpst->i40e_pf_eth_port_id = 0xFFFF;

	ethdev->data->mac_addrs = rte_zmalloc("ipn3ke", ETHER_ADDR_LEN, 0);
	if (!ethdev->data->mac_addrs) {
		IPN3KE_AFU_PMD_ERR("Failed to "
			"allocated memory for storing mac address");
		return -ENODEV;
	}

	if (rpst->hw->tm_hw_enable)
		ipn3ke_tm_init(rpst);

	/* Set representor device ops */
	ethdev->dev_ops = &ipn3ke_rpst_dev_ops;

	/* No data-path, but need stub Rx/Tx functions to avoid crash
	 * when testing with the likes of testpmd.
	 */
	ethdev->rx_pkt_burst = ipn3ke_rpst_recv_pkts;
	ethdev->tx_pkt_burst = ipn3ke_rpst_xmit_pkts;

	ethdev->data->nb_rx_queues = 1;
	ethdev->data->nb_tx_queues = 1;

	ethdev->data->mac_addrs = rte_zmalloc("ipn3ke_afu_representor",
						ETHER_ADDR_LEN,
						0);
	if (!ethdev->data->mac_addrs) {
		IPN3KE_AFU_PMD_ERR("Failed to "
			"allocated memory for storing mac address");
		return -ENODEV;
	}

	ethdev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;

	rte_spinlock_lock(&ipn3ke_link_notify_list_lk);
	TAILQ_INSERT_TAIL(&ipn3ke_rpst_list, rpst, next);
	ipn3ke_rpst_scan_num++;
	ipn3ke_rpst_scan_check();
	rte_spinlock_unlock(&ipn3ke_link_notify_list_lk);

	return 0;
}

int
ipn3ke_rpst_uninit(struct rte_eth_dev *ethdev)
{
	struct ipn3ke_rpst *rpst = IPN3KE_DEV_PRIVATE_TO_RPST(ethdev);

	rte_spinlock_lock(&ipn3ke_link_notify_list_lk);
	TAILQ_REMOVE(&ipn3ke_rpst_list, rpst, next);
	ipn3ke_rpst_scan_num--;
	ipn3ke_rpst_scan_check();
	rte_spinlock_unlock(&ipn3ke_link_notify_list_lk);

	return 0;
}
