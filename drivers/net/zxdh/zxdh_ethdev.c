/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <ethdev_pci.h>
#include <bus_pci_driver.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_io.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_msg.h"
#include "zxdh_common.h"
#include "zxdh_queue.h"
#include "zxdh_np.h"
#include "zxdh_tables.h"
#include "zxdh_rxtx.h"
#include "zxdh_ethdev_ops.h"

struct zxdh_hw_internal zxdh_hw_internal[RTE_MAX_ETHPORTS];
struct zxdh_dev_nic_shared_data dev_nic_sd[ZXDH_SLOT_MAX];
static rte_spinlock_t zxdh_shared_data_lock = RTE_SPINLOCK_INITIALIZER;
struct zxdh_shared_data *zxdh_shared_data;
struct zxdh_net_hdr_dl g_net_hdr_dl[RTE_MAX_ETHPORTS];
struct zxdh_mtr_res g_mtr_res;

#define ZXDH_INVALID_DTBQUE      0xFFFF
#define ZXDH_INVALID_SLOT_IDX    0xFFFF
#define ZXDH_PF_QUEUE_PAIRS_ADDR            0x5742
#define ZXDH_VF_QUEUE_PAIRS_ADDR            0x5744
#define ZXDH_QUEUE_POOL_ADDR                0x56A0

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

static int32_t
zxdh_dev_infos_get(struct rte_eth_dev *dev,
		struct rte_eth_dev_info *dev_info)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	dev_info->speed_capa       = rte_eth_speed_bitflag(hw->speed, RTE_ETH_LINK_FULL_DUPLEX);
	dev_info->max_rx_queues    = hw->max_queue_pairs;
	dev_info->max_tx_queues    = hw->max_queue_pairs;
	dev_info->min_rx_bufsize   = ZXDH_MIN_RX_BUFSIZE;
	dev_info->max_rx_pktlen    = ZXDH_MAX_RX_PKTLEN;
	dev_info->max_mac_addrs    = ZXDH_MAX_MAC_ADDRS;
	dev_info->rx_offload_capa  = (RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
					RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
					RTE_ETH_RX_OFFLOAD_QINQ_STRIP);
	dev_info->rx_offload_capa |= (RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
					RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
					RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
					RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM);
	dev_info->rx_offload_capa |= (RTE_ETH_RX_OFFLOAD_SCATTER);
	dev_info->rx_offload_capa |=  RTE_ETH_RX_OFFLOAD_TCP_LRO;
	dev_info->rx_offload_capa |=  RTE_ETH_RX_OFFLOAD_RSS_HASH;

	dev_info->reta_size = RTE_ETH_RSS_RETA_SIZE_256;
	dev_info->flow_type_rss_offloads = ZXDH_RSS_HF;

	dev_info->max_mtu = ZXDH_MAX_RX_PKTLEN - RTE_ETHER_HDR_LEN -
		RTE_VLAN_HLEN - ZXDH_DL_NET_HDR_SIZE;
	dev_info->min_mtu = ZXDH_ETHER_MIN_MTU;

	dev_info->tx_offload_capa = (RTE_ETH_TX_OFFLOAD_MULTI_SEGS);
	dev_info->tx_offload_capa |= (RTE_ETH_TX_OFFLOAD_TCP_TSO |
					RTE_ETH_TX_OFFLOAD_UDP_TSO);
	dev_info->tx_offload_capa |= (RTE_ETH_TX_OFFLOAD_VLAN_INSERT |
					RTE_ETH_TX_OFFLOAD_QINQ_INSERT |
					RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO);
	dev_info->tx_offload_capa |= (RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
					RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
					RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
					RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
					RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM);

	return 0;
}

static void
zxdh_queues_unbind_intr(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int32_t i;

	for (i = 0; i < dev->data->nb_rx_queues; ++i)
		ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2], ZXDH_MSI_NO_VECTOR);

	for (i = 0; i < dev->data->nb_tx_queues; ++i)
		ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2 + 1], ZXDH_MSI_NO_VECTOR);
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
	struct zxdh_hw *hw = dev->data->dev_private;

	uint8_t isr = zxdh_pci_isr(hw);

	if (zxdh_intr_unmask(dev) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");
	if (isr & ZXDH_PCI_ISR_CONFIG) {
		if (zxdh_dev_link_update(dev, 0) == 0)
			rte_eth_dev_callback_process(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
	}
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
		PMD_DRV_LOG(ERR, "to allocate risc_intr");
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

static void
zxdh_update_net_hdr_dl(struct zxdh_hw *hw)
{
	struct zxdh_net_hdr_dl *net_hdr_dl = &g_net_hdr_dl[hw->port_id];
	memset(net_hdr_dl, 0, ZXDH_DL_NET_HDR_SIZE);

	if (zxdh_tx_offload_enabled(hw)) {
		net_hdr_dl->type_hdr.port = ZXDH_PORT_DTP;
		net_hdr_dl->type_hdr.pd_len = ZXDH_DL_NET_HDR_SIZE >> 1;

		net_hdr_dl->pipd_hdr_dl.pi_hdr.pi_len = (ZXDH_PI_HDR_SIZE >> 4) - 1;
		net_hdr_dl->pipd_hdr_dl.pi_hdr.pkt_flag_hi8 = ZXDH_PI_FLAG | ZXDH_PI_TYPE_PI;
		net_hdr_dl->pipd_hdr_dl.pi_hdr.pkt_type = ZXDH_PKT_FORM_CPU;
		hw->dl_net_hdr_len = ZXDH_DL_NET_HDR_SIZE;

	} else {
		net_hdr_dl->type_hdr.port = ZXDH_PORT_NP;
		net_hdr_dl->type_hdr.pd_len = ZXDH_DL_NET_HDR_NOPI_SIZE >> 1;
		hw->dl_net_hdr_len = ZXDH_DL_NET_HDR_NOPI_SIZE;
	}
}

static int32_t
zxdh_features_update(struct zxdh_hw *hw,
		const struct rte_eth_rxmode *rxmode,
		const struct rte_eth_txmode *txmode)
{
	uint64_t rx_offloads = rxmode->offloads;
	uint64_t tx_offloads = txmode->offloads;
	uint64_t req_features = hw->guest_features;

	if (rx_offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
		req_features |= (1ULL << ZXDH_NET_F_GUEST_CSUM);

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO)
		req_features |= (1ULL << ZXDH_NET_F_GUEST_TSO4) |
						(1ULL << ZXDH_NET_F_GUEST_TSO6);

	if (tx_offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
		req_features |= (1ULL << ZXDH_NET_F_CSUM);

	if (tx_offloads & RTE_ETH_TX_OFFLOAD_TCP_TSO)
		req_features |= (1ULL << ZXDH_NET_F_HOST_TSO4) |
						(1ULL << ZXDH_NET_F_HOST_TSO6);

	if (tx_offloads & RTE_ETH_TX_OFFLOAD_UDP_TSO)
		req_features |= (1ULL << ZXDH_NET_F_HOST_UFO);

	req_features = req_features & hw->host_features;
	hw->guest_features = req_features;

	ZXDH_VTPCI_OPS(hw)->set_features(hw, req_features);

	if ((rx_offloads & (RTE_ETH_TX_OFFLOAD_UDP_CKSUM | RTE_ETH_TX_OFFLOAD_TCP_CKSUM)) &&
		 !zxdh_pci_with_feature(hw, ZXDH_NET_F_GUEST_CSUM)) {
		PMD_DRV_LOG(ERR, "rx checksum not available on this host");
		return -ENOTSUP;
	}

	if ((rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) &&
		(!zxdh_pci_with_feature(hw, ZXDH_NET_F_GUEST_TSO4) ||
		 !zxdh_pci_with_feature(hw, ZXDH_NET_F_GUEST_TSO6))) {
		PMD_DRV_LOG(ERR, "Large Receive Offload not available on this host");
		return -ENOTSUP;
	}
	return 0;
}

static void
zxdh_dev_free_mbufs(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_virtqueue *vq;
	struct rte_mbuf *buf;
	int i;

	if (hw->vqs == NULL)
		return;

	for (i = 0; i < hw->rx_qnum; i++) {
		vq = hw->vqs[i * 2];
		if (!vq)
			continue;
		while ((buf = zxdh_queue_detach_unused(vq)) != NULL)
			rte_pktmbuf_free(buf);
		PMD_DRV_LOG(DEBUG, "freeing %s[%d] used and unused buf",
		"rxq", i * 2);
	}
	for (i = 0; i < hw->tx_qnum; i++) {
		vq = hw->vqs[i * 2 + 1];
		if (!vq)
			continue;
		while ((buf = zxdh_queue_detach_unused(vq)) != NULL)
			rte_pktmbuf_free(buf);
		PMD_DRV_LOG(DEBUG, "freeing %s[%d] used and unused buf",
		"txq", i * 2 + 1);
	}
}

static int32_t
zxdh_get_available_channel(struct rte_eth_dev *dev, uint8_t queue_type)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t base	 = (queue_type == ZXDH_VTNET_RQ) ? 0 : 1;  /* txq only polls odd bits*/
	uint16_t j		 = 0;
	uint16_t done	 = 0;
	uint32_t phy_vq_reg = 0;
	uint16_t total_queue_num = hw->queue_pool_count * 2;
	uint16_t start_qp_id = hw->queue_pool_start * 2;
	uint32_t phy_vq_reg_oft = start_qp_id / 32;
	uint32_t inval_bit = start_qp_id % 32;
	uint32_t res_bit = (total_queue_num + inval_bit) % 32;
	uint32_t vq_reg_num = (total_queue_num + inval_bit) / 32 + (res_bit ? 1 : 0);
	int32_t ret = 0;

	ret = zxdh_timedlock(hw, 1000);
	if (ret) {
		PMD_DRV_LOG(ERR, "Acquiring hw lock got failed, timeout");
		return -1;
	}

	for (phy_vq_reg = 0; phy_vq_reg < vq_reg_num; phy_vq_reg++) {
		uint32_t addr = ZXDH_QUERES_SHARE_BASE +
		(phy_vq_reg + phy_vq_reg_oft) * sizeof(uint32_t);
		uint32_t var = zxdh_read_bar_reg(dev, ZXDH_BAR0_INDEX, addr);
		if (phy_vq_reg == 0) {
			for (j = (inval_bit + base); j < 32; j += 2) {
				/* Got the available channel & update COI table */
				if ((var & (1 << j)) == 0) {
					var |= (1 << j);
					zxdh_write_bar_reg(dev, ZXDH_BAR0_INDEX, addr, var);
					done = 1;
					break;
				}
			}
			if (done)
				break;
		} else if ((phy_vq_reg == (vq_reg_num - 1)) && (res_bit != 0)) {
			for (j = base; j < res_bit; j += 2) {
				/* Got the available channel & update COI table */
				if ((var & (1 << j)) == 0) {
					var |= (1 << j);
					zxdh_write_bar_reg(dev, ZXDH_BAR0_INDEX, addr, var);
					done = 1;
					break;
				}
			}
			if (done)
				break;
		} else {
			for (j = base; j < 32; j += 2) {
				/* Got the available channel & update COI table */
				if ((var & (1 << j)) == 0) {
					var |= (1 << j);
					zxdh_write_bar_reg(dev, ZXDH_BAR0_INDEX, addr, var);
					done = 1;
					break;
				}
			}
			if (done)
				break;
		}
	}

	zxdh_release_lock(hw);
	/* check for no channel condition */
	if (done != 1) {
		PMD_DRV_LOG(ERR, "NO availd queues");
		return -1;
	}
	/* reruen available channel ID */
	return (phy_vq_reg + phy_vq_reg_oft) * 32 + j;
}

static int32_t
zxdh_acquire_channel(struct rte_eth_dev *dev, uint16_t lch)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (hw->channel_context[lch].valid == 1) {
		PMD_DRV_LOG(DEBUG, "Logic channel:%u already acquired Physics channel:%u",
				lch, hw->channel_context[lch].ph_chno);
		return hw->channel_context[lch].ph_chno;
	}
	int32_t pch = zxdh_get_available_channel(dev, zxdh_get_queue_type(lch));

	if (pch < 0) {
		PMD_DRV_LOG(ERR, "Failed to acquire channel");
		return -1;
	}
	hw->channel_context[lch].ph_chno = (uint16_t)pch;
	hw->channel_context[lch].valid = 1;
	PMD_DRV_LOG(DEBUG, "Acquire channel success lch:%u --> pch:%d", lch, pch);
	return 0;
}

static void
zxdh_init_vring(struct zxdh_virtqueue *vq)
{
	int32_t  size	  = vq->vq_nentries;
	uint8_t *ring_mem = vq->vq_ring_virt_mem;

	memset(ring_mem, 0, vq->vq_ring_size);

	vq->vq_used_cons_idx = 0;
	vq->vq_desc_head_idx = 0;
	vq->vq_avail_idx	 = 0;
	vq->vq_desc_tail_idx = (uint16_t)(vq->vq_nentries - 1);
	vq->vq_free_cnt = vq->vq_nentries;
	memset(vq->vq_descx, 0, sizeof(struct zxdh_vq_desc_extra) * vq->vq_nentries);
	zxdh_vring_init_packed(&vq->vq_packed.ring, ring_mem, ZXDH_PCI_VRING_ALIGN, size);
	zxdh_vring_desc_init_packed(vq, size);
	zxdh_queue_disable_intr(vq);
}

static int32_t
zxdh_init_queue(struct rte_eth_dev *dev, uint16_t vtpci_logic_qidx)
{
	char vq_name[ZXDH_VIRTQUEUE_MAX_NAME_SZ] = {0};
	char vq_hdr_name[ZXDH_VIRTQUEUE_MAX_NAME_SZ] = {0};
	const struct rte_memzone *mz = NULL;
	const struct rte_memzone *hdr_mz = NULL;
	uint32_t size = 0;
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_virtnet_rx *rxvq = NULL;
	struct zxdh_virtnet_tx *txvq = NULL;
	struct zxdh_virtqueue *vq = NULL;
	size_t sz_hdr_mz = 0;
	void *sw_ring = NULL;
	int32_t queue_type = zxdh_get_queue_type(vtpci_logic_qidx);
	int32_t numa_node = dev->device->numa_node;
	uint16_t vtpci_phy_qidx = 0;
	uint32_t vq_size = 0;
	int32_t ret = 0;

	if (hw->channel_context[vtpci_logic_qidx].valid == 0) {
		PMD_DRV_LOG(ERR, "lch %d is invalid", vtpci_logic_qidx);
		return -EINVAL;
	}
	vtpci_phy_qidx = hw->channel_context[vtpci_logic_qidx].ph_chno;

	PMD_DRV_LOG(DEBUG, "vtpci_logic_qidx :%d setting up physical queue: %u on NUMA node %d",
			vtpci_logic_qidx, vtpci_phy_qidx, numa_node);

	vq_size = ZXDH_QUEUE_DEPTH;

	if (ZXDH_VTPCI_OPS(hw)->set_queue_num != NULL)
		ZXDH_VTPCI_OPS(hw)->set_queue_num(hw, vtpci_phy_qidx, vq_size);

	snprintf(vq_name, sizeof(vq_name), "port%d_vq%d", dev->data->port_id, vtpci_phy_qidx);

	size = RTE_ALIGN_CEIL(sizeof(*vq) + vq_size * sizeof(struct zxdh_vq_desc_extra),
				RTE_CACHE_LINE_SIZE);
	if (queue_type == ZXDH_VTNET_TQ) {
		/*
		 * For each xmit packet, allocate a zxdh_net_hdr
		 * and indirect ring elements
		 */
		sz_hdr_mz = vq_size * sizeof(struct zxdh_tx_region);
	}

	vq = rte_zmalloc_socket(vq_name, size, RTE_CACHE_LINE_SIZE, numa_node);
	if (vq == NULL) {
		PMD_DRV_LOG(ERR, "can not allocate vq");
		return -ENOMEM;
	}
	hw->vqs[vtpci_logic_qidx] = vq;

	vq->hw = hw;
	vq->vq_queue_index = vtpci_phy_qidx;
	vq->vq_nentries = vq_size;

	vq->vq_packed.used_wrap_counter = 1;
	vq->vq_packed.cached_flags = ZXDH_VRING_PACKED_DESC_F_AVAIL;
	vq->vq_packed.event_flags_shadow = 0;
	if (queue_type == ZXDH_VTNET_RQ)
		vq->vq_packed.cached_flags |= ZXDH_VRING_DESC_F_WRITE;

	/*
	 * Reserve a memzone for vring elements
	 */
	size = zxdh_vring_size(hw, vq_size, ZXDH_PCI_VRING_ALIGN);
	vq->vq_ring_size = RTE_ALIGN_CEIL(size, ZXDH_PCI_VRING_ALIGN);
	PMD_DRV_LOG(DEBUG, "vring_size: %d, rounded_vring_size: %d", size, vq->vq_ring_size);

	mz = rte_memzone_reserve_aligned(vq_name, vq->vq_ring_size,
				numa_node, RTE_MEMZONE_IOVA_CONTIG,
				ZXDH_PCI_VRING_ALIGN);
	if (mz == NULL) {
		if (rte_errno == EEXIST)
			mz = rte_memzone_lookup(vq_name);
		if (mz == NULL) {
			ret = -ENOMEM;
			goto fail_q_alloc;
		}
	}

	memset(mz->addr, 0, mz->len);

	vq->vq_ring_mem = mz->iova;
	vq->vq_ring_virt_mem = mz->addr;

	zxdh_init_vring(vq);

	if (sz_hdr_mz) {
		snprintf(vq_hdr_name, sizeof(vq_hdr_name), "port%d_vq%d_hdr",
					dev->data->port_id, vtpci_phy_qidx);
		hdr_mz = rte_memzone_reserve_aligned(vq_hdr_name, sz_hdr_mz,
					numa_node, RTE_MEMZONE_IOVA_CONTIG,
					RTE_CACHE_LINE_SIZE);
		if (hdr_mz == NULL) {
			if (rte_errno == EEXIST)
				hdr_mz = rte_memzone_lookup(vq_hdr_name);
			if (hdr_mz == NULL) {
				ret = -ENOMEM;
				goto fail_q_alloc;
			}
		}
	}

	if (queue_type == ZXDH_VTNET_RQ) {
		size_t sz_sw = (ZXDH_MBUF_BURST_SZ + vq_size) * sizeof(vq->sw_ring[0]);

		sw_ring = rte_zmalloc_socket("sw_ring", sz_sw, RTE_CACHE_LINE_SIZE, numa_node);
		if (!sw_ring) {
			PMD_DRV_LOG(ERR, "can not allocate RX soft ring");
			ret = -ENOMEM;
			goto fail_q_alloc;
		}

		vq->sw_ring = sw_ring;
		rxvq = &vq->rxq;
		rxvq->vq = vq;
		rxvq->port_id = dev->data->port_id;
		rxvq->mz = mz;
	} else {             /* queue_type == VTNET_TQ */
		txvq = &vq->txq;
		txvq->vq = vq;
		txvq->port_id = dev->data->port_id;
		txvq->mz = mz;
		txvq->zxdh_net_hdr_mz = hdr_mz;
		txvq->zxdh_net_hdr_mem = hdr_mz->iova;
	}

	vq->offset = offsetof(struct rte_mbuf, buf_iova);
	if (queue_type == ZXDH_VTNET_TQ) {
		struct zxdh_tx_region *txr = hdr_mz->addr;
		uint32_t i;

		memset(txr, 0, vq_size * sizeof(*txr));
		for (i = 0; i < vq_size; i++) {
			/* first indirect descriptor is always the tx header */
			struct zxdh_vring_packed_desc *start_dp = txr[i].tx_packed_indir;

			zxdh_vring_desc_init_indirect_packed(start_dp,
					RTE_DIM(txr[i].tx_packed_indir));
			start_dp->addr = txvq->zxdh_net_hdr_mem + i * sizeof(*txr) +
					offsetof(struct zxdh_tx_region, tx_hdr);
			/* length will be updated to actual pi hdr size when xmit pkt */
			start_dp->len = 0;
		}
	}
	if (ZXDH_VTPCI_OPS(hw)->setup_queue(hw, vq) < 0) {
		PMD_DRV_LOG(ERR, "setup_queue failed");
		return -EINVAL;
	}
	return 0;
fail_q_alloc:
	rte_free(sw_ring);
	rte_memzone_free(hdr_mz);
	rte_memzone_free(mz);
	rte_free(vq);
	return ret;
}

static int
zxdh_inic_pf_init_qid(struct zxdh_hw *hw)
{
	uint16_t start_qid, enabled_qp;
	int ret = zxdh_inic_pf_get_qp_from_vcb(hw, hw->vfid, &start_qid, &enabled_qp);

	if (ret != 0) {
		PMD_DRV_LOG(ERR, "vqm_vfid %u, get_qp_from_vcb fail", hw->vfid);
		return ret;
	}

	uint16_t i, num_queues = rte_read16(&hw->common_cfg->num_queues);
	PMD_DRV_LOG(INFO, "vqm_vfid:%u, get num_queues:%u (%s CQ)",
		hw->vfid, num_queues, (num_queues & 0x1) ? "with" : "without");
	for (i = 0; i < (num_queues & 0xfffe); ++i) {
		hw->channel_context[i].ph_chno = start_qid + i;
		hw->channel_context[i].valid = 1;
	}
	return 0;
}

static int32_t
zxdh_alloc_queues(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	u_int16_t rxq_num = hw->rx_qnum;
	u_int16_t txq_num = hw->tx_qnum;
	uint16_t nr_vq = (rxq_num > txq_num) ? 2 * rxq_num : 2 * txq_num;
	hw->vqs = rte_zmalloc(NULL, sizeof(struct zxdh_virtqueue *) * nr_vq, 0);
	uint16_t lch, i;

	if (!hw->vqs) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d vqs", nr_vq);
		return -ENOMEM;
	}

	if (hw->switchoffload && !(hw->host_features & (1ULL << ZXDH_F_RING_PACKED))) {
		if (zxdh_inic_pf_init_qid(hw) != 0)
			goto free;

		for (i = 0 ; i < rxq_num; i++) {
			lch = i * 2;
			if (zxdh_init_queue(dev, lch) < 0) {
				PMD_DRV_LOG(ERR, "Failed to alloc virtio queue");
				goto free;
			}
		}
		for (i = 0 ; i < txq_num; i++) {
			lch = i * 2 + 1;
			if (zxdh_init_queue(dev, lch) < 0) {
				PMD_DRV_LOG(ERR, "Failed to alloc virtio queue");
				goto free;
			}
		}
		return 0;
	}

	for (i = 0 ; i < rxq_num; i++) {
		lch = i * 2;
		if (zxdh_acquire_channel(dev, lch) < 0) {
			PMD_DRV_LOG(ERR, "Failed to acquire the channels");
			goto free;
		}
		if (zxdh_init_queue(dev, lch) < 0) {
			PMD_DRV_LOG(ERR, "Failed to alloc virtio queue");
			goto free;
		}
	}
	for (i = 0 ; i < txq_num; i++) {
		lch = i * 2 + 1;
		if (zxdh_acquire_channel(dev, lch) < 0) {
			PMD_DRV_LOG(ERR, "Failed to acquire the channels");
			goto free;
		}
		if (zxdh_init_queue(dev, lch) < 0) {
			PMD_DRV_LOG(ERR, "Failed to alloc virtio queue");
			goto free;
		}
	}
	return 0;

free:
	zxdh_free_queues(dev);
	return -1;
}

static int
zxdh_vlan_offload_configure(struct rte_eth_dev *dev)
{
	int ret;
	int mask = RTE_ETH_VLAN_STRIP_MASK | RTE_ETH_VLAN_FILTER_MASK |	RTE_ETH_QINQ_STRIP_MASK;

	ret = zxdh_dev_vlan_offload_set(dev, mask);
	if (ret) {
		PMD_DRV_LOG(ERR, "vlan offload set error");
		return -1;
	}

	return 0;
}

static int
zxdh_rx_csum_lro_offload_configure(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	uint32_t need_accelerator = rxmode->offloads & (RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
		RTE_ETH_RX_OFFLOAD_TCP_LRO);
	int ret;

	if (hw->is_pf) {
		struct zxdh_port_attr_table port_attr = {0};
		zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
		port_attr.outer_ip_checksum_offload =
			(rxmode->offloads & RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM) ? true : false;
		port_attr.ip_checksum_offload =
			(rxmode->offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM) ? true : false;
		port_attr.tcp_udp_checksum_offload  =
		(rxmode->offloads & (RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM))
					? true : false;
		port_attr.lro_offload =
				(rxmode->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) ? true : false;
		port_attr.accelerator_offload_flag  = need_accelerator ? true : false;
		ret = zxdh_set_port_attr(hw, hw->vport.vport, &port_attr);
		if (ret) {
			PMD_DRV_LOG(ERR, "%s set port attr failed", __func__);
			return -1;
		}
	} else {
		struct zxdh_msg_info msg_info = {0};
		struct zxdh_port_attr_set_msg *attr_msg = &msg_info.data.port_attr_msg;

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		attr_msg->mode = ZXDH_PORT_IP_CHKSUM_FLAG;
		attr_msg->value =
			(rxmode->offloads & RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM) ? true : false;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "%s outer ip cksum config failed", __func__);
			return -1;
		}

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		attr_msg->mode = ZXDH_PORT_OUTER_IP_CHECKSUM_OFFLOAD_FLAG;
		attr_msg->value = (rxmode->offloads & RTE_ETH_RX_OFFLOAD_IPV4_CKSUM) ? true : false;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "%s ip_checksum config failed to send msg", __func__);
			return -1;
		}

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		attr_msg->mode = ZXDH_PORT_TCP_UDP_CHKSUM_FLAG;
		attr_msg->value = (rxmode->offloads &
			(RTE_ETH_RX_OFFLOAD_UDP_CKSUM | RTE_ETH_RX_OFFLOAD_TCP_CKSUM)) ?
				true : false;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "%s tcp_udp_checksum config failed to send msg", __func__);
			return -1;
		}

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		attr_msg->mode = ZXDH_PORT_LRO_OFFLOAD_FLAG;
		attr_msg->value = (rxmode->offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) ? true : false;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "%s lro offload config failed to send msg", __func__);
			return -1;
		}

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		attr_msg->mode = ZXDH_PORT_ACCELERATOR_OFFLOAD_FLAG_FLAG;
		attr_msg->value = need_accelerator ? true : false;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR,
				"%s accelerator offload config failed to send msg", __func__);
			return -1;
		}
	}

	return 0;
}

static int
zxdh_dev_conf_offload(struct rte_eth_dev *dev)
{
	int ret = 0;

	ret = zxdh_vlan_offload_configure(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "zxdh_vlan_offload_configure failed");
		return ret;
	}

	ret = zxdh_rss_configure(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "rss configure failed");
		return ret;
	}

	ret = zxdh_rx_csum_lro_offload_configure(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "rx csum lro configure failed");
		return ret;
	}

	return 0;
}

static int
zxdh_rss_qid_config(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_port_attr_table port_attr = {0};
	struct zxdh_msg_info msg_info = {0};
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_get_port_attr(hw, hw->vport.vport, &port_attr);
		port_attr.port_base_qid = hw->channel_context[0].ph_chno & 0xfff;

		ret = zxdh_set_port_attr(hw, hw->vport.vport, &port_attr);
		if (ret) {
			PMD_DRV_LOG(ERR, "PF:%d port_base_qid insert failed", hw->vfid);
			return ret;
		}
	} else {
		struct zxdh_port_attr_set_msg *attr_msg = &msg_info.data.port_attr_msg;

		zxdh_msg_head_build(hw, ZXDH_PORT_ATTRS_SET, &msg_info);
		attr_msg->mode = ZXDH_PORT_BASE_QID_FLAG;
		attr_msg->value = hw->channel_context[0].ph_chno & 0xfff;
		ret = zxdh_vf_send_msg_to_pf(dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: port 0x%x msg type %d",
					hw->vport.vport, ZXDH_PORT_BASE_QID_FLAG);
			return ret;
		}
	}
	return ret;
}

static int32_t
zxdh_dev_configure(struct rte_eth_dev *dev)
{
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	const struct rte_eth_txmode *txmode = &dev->data->dev_conf.txmode;
	struct zxdh_hw *hw = dev->data->dev_private;
	uint64_t rx_offloads = rxmode->offloads;
	int32_t  ret = 0;

	if (dev->data->nb_rx_queues > hw->max_queue_pairs ||
		dev->data->nb_tx_queues > hw->max_queue_pairs) {
		PMD_DRV_LOG(ERR, "nb_rx_queues=%d or nb_tx_queues=%d must < (%d)!",
		dev->data->nb_rx_queues, dev->data->nb_tx_queues, hw->max_queue_pairs);
		return -EINVAL;
	}

	if (rxmode->mq_mode != RTE_ETH_MQ_RX_RSS && rxmode->mq_mode != RTE_ETH_MQ_RX_NONE) {
		PMD_DRV_LOG(ERR, "Unsupported Rx multi queue mode %d", rxmode->mq_mode);
		return -EINVAL;
	}

	if (txmode->mq_mode != RTE_ETH_MQ_TX_NONE) {
		PMD_DRV_LOG(ERR, "Unsupported Tx multi queue mode %d", txmode->mq_mode);
		return -EINVAL;
	}
	if (rxmode->mq_mode != RTE_ETH_MQ_RX_RSS && rxmode->mq_mode != RTE_ETH_MQ_RX_NONE) {
		PMD_DRV_LOG(ERR, "Unsupported Rx multi queue mode %d", rxmode->mq_mode);
		return -EINVAL;
	}

	if (txmode->mq_mode != RTE_ETH_MQ_TX_NONE) {
		PMD_DRV_LOG(ERR, "Unsupported Tx multi queue mode %d", txmode->mq_mode);
		return -EINVAL;
	}

	ret = zxdh_features_update(hw, rxmode, txmode);
	if (ret < 0)
		return ret;

	/* check if lsc interrupt feature is enabled */
	if (dev->data->dev_conf.intr_conf.lsc) {
		if (!(dev->data->dev_flags & RTE_ETH_DEV_INTR_LSC)) {
			PMD_DRV_LOG(ERR, "link status not supported by host");
			return -ENOTSUP;
		}
	}

	if (rx_offloads & RTE_ETH_RX_OFFLOAD_VLAN_STRIP)
		hw->vlan_offload_cfg.vlan_strip = 1;

	hw->has_tx_offload = zxdh_tx_offload_enabled(hw);
	hw->has_rx_offload = zxdh_rx_offload_enabled(hw);

	if (dev->data->nb_rx_queues == hw->rx_qnum &&
			dev->data->nb_tx_queues == hw->tx_qnum) {
		PMD_DRV_LOG(DEBUG, "The queue not need to change. queue_rx %d queue_tx %d",
				hw->rx_qnum, hw->tx_qnum);
		/*no queue changed */
		goto end;
	}

	PMD_DRV_LOG(DEBUG, "queue changed need reset");
	/* Reset the device although not necessary at startup */
	zxdh_pci_reset(hw);

	/* Tell the host we've noticed this device. */
	zxdh_pci_set_status(hw, ZXDH_CONFIG_STATUS_ACK);

	/* Tell the host we've known how to drive the device. */
	zxdh_pci_set_status(hw, ZXDH_CONFIG_STATUS_DRIVER);
	/* The queue needs to be released when reconfiguring*/
	if (hw->vqs != NULL) {
		zxdh_dev_free_mbufs(dev);
		zxdh_free_queues(dev);
	}

	hw->rx_qnum = dev->data->nb_rx_queues;
	hw->tx_qnum = dev->data->nb_tx_queues;
	ret = zxdh_alloc_queues(dev);
	if (ret < 0)
		return ret;

	zxdh_datach_set(dev);

	if (zxdh_configure_intr(dev) < 0) {
		PMD_DRV_LOG(ERR, "Failed to configure interrupt");
		zxdh_free_queues(dev);
		return -1;
	}

	ret = zxdh_rss_qid_config(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to configure base qid!");
		return -1;
	}

	zxdh_pci_reinit_complete(hw);

end:
	zxdh_dev_conf_offload(dev);
	zxdh_update_net_hdr_dl(hw);
	return ret;
}

static void
zxdh_np_dtb_data_res_free(struct zxdh_hw *hw)
{
	struct rte_eth_dev *dev = hw->eth_dev;
	struct zxdh_dtb_shared_data *dtb_data = &hw->dev_sd->dtb_sd;
	int ret;
	int i;

	if (dtb_data->init_done && dtb_data->bind_device == dev) {
		ret = zxdh_np_online_uninit(hw->dev_id, dev->data->name, dtb_data->queueid);
		if (ret)
			PMD_DRV_LOG(ERR, "%s dpp_np_online_uninstall failed", dev->data->name);

		hw->dev_nic_sd->dtb_used_num--;
		dtb_data->init_done = 0;
		dtb_data->bind_device = NULL;
	}

	rte_memzone_free(dtb_data->dtb_table_conf_mz);
	dtb_data->dtb_table_conf_mz = NULL;
	rte_memzone_free(dtb_data->dtb_table_dump_mz);
	dtb_data->dtb_table_dump_mz = NULL;

	for (i = 0; i < ZXDH_MAX_BASE_DTB_TABLE_COUNT; i++) {
		if (dtb_data->dtb_table_bulk_dump_mz[i]) {
			rte_memzone_free(dtb_data->dtb_table_bulk_dump_mz[i]);
			dtb_data->dtb_table_bulk_dump_mz[i] = NULL;
		}
	}
}

static int
zxdh_tbl_entry_online_destroy(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_dtb_shared_data *dtb_data = &hw->dev_sd->dtb_sd;
	uint32_t sdt_no;
	int ret = 0;
	if (!dtb_data->init_done)
		return ret;
	if (hw->is_pf) {
		sdt_no = ZXDH_SDT_L2_ENTRY_TABLE0 + hw->hash_search_index;
		ret = zxdh_np_dtb_hash_online_delete(hw->dev_id, dtb_data->queueid, sdt_no);
		if (ret)
			PMD_DRV_LOG(ERR, "%s sdt_no %d failed. code:%d ",
				dev->data->name, sdt_no, ret);
		sdt_no = ZXDH_SDT_MC_TABLE0 + hw->hash_search_index;
		ret = zxdh_np_dtb_hash_online_delete(hw->dev_id, dtb_data->queueid, sdt_no);
		if (ret)
			PMD_DRV_LOG(ERR, "%s sdt_no %d failed. code:%d",
				dev->data->name, sdt_no, ret);
	}
	return ret;
}

static void
zxdh_np_uninit(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;

	if (!hw->is_pf)
		return;

	zxdh_tbl_entry_online_destroy(dev);
	zxdh_np_dtb_data_res_free(hw);

	if (hw->dev_nic_sd->init_done && hw->dev_nic_sd->dtb_used_num == 0) {
		zxdh_np_soft_res_uninstall(hw->dev_id);
		hw->dev_nic_sd->init_done = 0;
	}
	PMD_DRV_LOG(DEBUG, "zxdh_np_destroy: dtb_used_num %d", hw->dev_nic_sd->dtb_used_num);

}

static int
zxdh_tables_uninit(struct rte_eth_dev *dev)
{
	int ret;

	ret = zxdh_port_attr_uninit(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "zxdh_port_attr_uninit failed");
		return ret;
	}

	ret = zxdh_promisc_table_uninit(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "uninit promisc_table failed");
		return ret;
	}

	return ret;
}

static int
zxdh_dev_stop(struct rte_eth_dev *dev)
{
	uint16_t i;
	int ret = 0;

	if (dev->data->dev_started == 0)
		return 0;

	ret = zxdh_intr_disable(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "intr disable failed");
		goto end;
	}

	ret = zxdh_dev_set_link_down(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "set port %s link down failed!", dev->device->name);
		goto end;
	}

end:
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	return ret;
}

static void
zxdh_priv_res_free(struct zxdh_hw *priv)
{
	rte_free(priv->vfinfo);
	priv->vfinfo = NULL;

	rte_free(priv->channel_context);
	priv->channel_context = NULL;
}

static int
zxdh_dev_close(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	ret = zxdh_dev_stop(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "stop port %s failed.", dev->device->name);
		return -1;
	}

	ret = zxdh_tables_uninit(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "%s :tables uninit %s failed", __func__, dev->device->name);
		return -1;
	}

	if (zxdh_shared_data != NULL) {
		zxdh_mtr_release(dev);
		zxdh_flow_release(dev);
	}

	zxdh_intr_release(dev);
	zxdh_np_uninit(dev);
	zxdh_pci_reset(hw);

	zxdh_dev_free_mbufs(dev);
	zxdh_free_queues(dev);

	zxdh_bar_msg_chan_exit();
	zxdh_priv_res_free(hw);

	rte_free(hw->dev_sd);
	if (dev->data->mac_addrs != NULL) {
		rte_free(dev->data->mac_addrs);
		dev->data->mac_addrs = NULL;
	}

	return ret;
}

static int32_t
zxdh_set_rxtx_funcs(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;

	if (!zxdh_pci_with_feature(hw, ZXDH_NET_F_MRG_RXBUF)) {
		PMD_DRV_LOG(ERR, "port %u not support rx mergeable", eth_dev->data->port_id);
		return -1;
	}
	eth_dev->tx_pkt_prepare = zxdh_xmit_pkts_prepare;
	eth_dev->tx_pkt_burst = &zxdh_xmit_pkts_packed;
	eth_dev->rx_pkt_burst = &zxdh_recv_pkts_packed;

	return 0;
}

static int
zxdh_mac_config(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	struct zxdh_msg_info msg_info = {0};
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_add_mac_table(hw, hw->vport.vport,
				&eth_dev->data->mac_addrs[0], hw->hash_search_index, 0, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to add mac: port 0x%x", hw->vport.vport);
			return ret;
		}
		hw->uc_num++;
	} else {
		struct zxdh_mac_filter *mac_filter = &msg_info.data.mac_filter_msg;
		mac_filter->filter_flag = 0xff;
		mac_filter->mac = eth_dev->data->mac_addrs[0];
		zxdh_msg_head_build(hw, ZXDH_MAC_ADD, &msg_info);
		ret = zxdh_vf_send_msg_to_pf(eth_dev, &msg_info, sizeof(msg_info), NULL, 0);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to send msg: msg type %d", ZXDH_MAC_ADD);
			return ret;
		}
		hw->uc_num++;
	}
	return ret;
}

static int
zxdh_dev_start(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_virtqueue *vq;
	int32_t ret;
	uint16_t logic_qidx;
	uint16_t i;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		logic_qidx = 2 * i + ZXDH_RQ_QUEUE_IDX;
		ret = zxdh_dev_rx_queue_setup_finish(dev, logic_qidx);
		if (ret < 0)
			return ret;
	}

	zxdh_set_rxtx_funcs(dev);
	ret = zxdh_intr_enable(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "interrupt enable failed");
		return -EINVAL;
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		logic_qidx = 2 * i + ZXDH_RQ_QUEUE_IDX;
		vq = hw->vqs[logic_qidx];
		/* Flush the old packets */
		zxdh_queue_rxvq_flush(vq);
		zxdh_queue_notify(vq);
	}
	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		logic_qidx = 2 * i + ZXDH_TQ_QUEUE_IDX;
		vq = hw->vqs[logic_qidx];
		zxdh_queue_notify(vq);
	}

	hw->admin_status = RTE_ETH_LINK_UP;
	zxdh_dev_link_update(dev, 0);

	ret = zxdh_mac_config(hw->eth_dev);
	if (ret)
		PMD_DRV_LOG(ERR, "mac config failed");

	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static void
zxdh_rxq_info_get(struct rte_eth_dev *dev, uint16_t rx_queue_id, struct rte_eth_rxq_info *qinfo)
{
	struct zxdh_virtnet_rx *rxq = NULL;

	if (rx_queue_id < dev->data->nb_rx_queues)
		rxq = dev->data->rx_queues[rx_queue_id];
	if (!rxq) {
		PMD_RX_LOG(ERR, "rxq is null");
		return;
	}
	qinfo->nb_desc = rxq->vq->vq_nentries;
	qinfo->conf.rx_free_thresh = rxq->vq->vq_free_thresh;
	qinfo->conf.offloads = dev->data->dev_conf.rxmode.offloads;
	qinfo->queue_state = dev->data->rx_queue_state[rx_queue_id];
}

static void
zxdh_txq_info_get(struct rte_eth_dev *dev, uint16_t tx_queue_id, struct rte_eth_txq_info *qinfo)
{
	struct zxdh_virtnet_tx *txq = NULL;

	if (tx_queue_id < dev->data->nb_tx_queues)
		txq = dev->data->tx_queues[tx_queue_id];
	if (!txq) {
		PMD_TX_LOG(ERR, "txq is null");
		return;
	}
	qinfo->nb_desc = txq->vq->vq_nentries;
	qinfo->conf.tx_free_thresh = txq->vq->vq_free_thresh;
	qinfo->conf.offloads = dev->data->dev_conf.txmode.offloads;
	qinfo->queue_state = dev->data->tx_queue_state[tx_queue_id];
}

static const uint32_t *
zxdh_dev_supported_ptypes_get(struct rte_eth_dev *dev, size_t *ptype_sz)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_INNER_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_INNER_L4_NONFRAG,
		RTE_PTYPE_INNER_L4_FRAG,
		RTE_PTYPE_INNER_L4_TCP,
		RTE_PTYPE_INNER_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};

	if (!dev->rx_pkt_burst)
		return NULL;
	*ptype_sz = sizeof(ptypes);
	return ptypes;
}

/* dev_ops for zxdh, bare necessities for basic operation */
static const struct eth_dev_ops zxdh_eth_dev_ops = {
	.dev_configure			 = zxdh_dev_configure,
	.dev_start				 = zxdh_dev_start,
	.dev_stop				 = zxdh_dev_stop,
	.dev_close				 = zxdh_dev_close,
	.dev_infos_get			 = zxdh_dev_infos_get,
	.rx_queue_setup			 = zxdh_dev_rx_queue_setup,
	.tx_queue_setup			 = zxdh_dev_tx_queue_setup,
	.rx_queue_intr_enable	 = zxdh_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable	 = zxdh_dev_rx_queue_intr_disable,
	.rxq_info_get			 = zxdh_rxq_info_get,
	.txq_info_get			 = zxdh_txq_info_get,
	.link_update			 = zxdh_dev_link_update,
	.dev_set_link_up		 = zxdh_dev_set_link_up,
	.dev_set_link_down		 = zxdh_dev_set_link_down,
	.mac_addr_add			 = zxdh_dev_mac_addr_add,
	.mac_addr_remove		 = zxdh_dev_mac_addr_remove,
	.mac_addr_set			 = zxdh_dev_mac_addr_set,
	.promiscuous_enable		 = zxdh_dev_promiscuous_enable,
	.promiscuous_disable	 = zxdh_dev_promiscuous_disable,
	.allmulticast_enable	 = zxdh_dev_allmulticast_enable,
	.allmulticast_disable	 = zxdh_dev_allmulticast_disable,
	.vlan_tpid_set			 = zxdh_vlan_tpid_set,
	.vlan_filter_set		 = zxdh_dev_vlan_filter_set,
	.vlan_offload_set		 = zxdh_dev_vlan_offload_set,
	.reta_update			 = zxdh_dev_rss_reta_update,
	.reta_query				 = zxdh_dev_rss_reta_query,
	.rss_hash_update		 = zxdh_rss_hash_update,
	.rss_hash_conf_get		 = zxdh_rss_hash_conf_get,
	.stats_get				 = zxdh_dev_stats_get,
	.stats_reset			 = zxdh_dev_stats_reset,
	.xstats_get				 = zxdh_dev_xstats_get,
	.xstats_get_names		 = zxdh_dev_xstats_get_names,
	.xstats_reset			 = zxdh_dev_stats_reset,
	.mtu_set				 = zxdh_dev_mtu_set,
	.fw_version_get			 = zxdh_dev_fw_version_get,
	.get_module_info		 = zxdh_dev_get_module_info,
	.get_module_eeprom		 = zxdh_dev_get_module_eeprom,
	.dev_supported_ptypes_get = zxdh_dev_supported_ptypes_get,
	.mtr_ops_get			 = zxdh_meter_ops_get,
	.flow_ops_get			 = zxdh_flow_ops_get,
};

static int32_t
zxdh_init_device(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret = 0;

	ret = zxdh_read_pci_caps(pci_dev, hw);
	if (ret) {
		PMD_DRV_LOG(ERR, "port 0x%x pci caps read failed", hw->port_id);
		goto err;
	}

	zxdh_hw_internal[hw->port_id].zxdh_vtpci_ops = &zxdh_dev_pci_ops;
	zxdh_pci_reset(hw);
	zxdh_get_pci_dev_config(hw);

	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr, &eth_dev->data->mac_addrs[0]);

	/* If host does not support both status and MSI-X then disable LSC */
	if (zxdh_pci_with_feature(hw, ZXDH_NET_F_STATUS) && hw->use_msix != ZXDH_MSIX_NONE)
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
	PMD_DRV_LOG(DEBUG, "Get phyport success: 0x%x", hw->phyport);

	hw->vfid = zxdh_vport_to_vfid(hw->vport);

	if (zxdh_hashidx_get(eth_dev, &hw->hash_search_index) != 0) {
		PMD_DRV_LOG(ERR, "Failed to get hash idx");
		return -1;
	}

	if (zxdh_panelid_get(eth_dev, &hw->panel_id) != 0) {
		PMD_DRV_LOG(ERR, "Failed to get panel_id");
		return -1;
	}

	if (hw->switchoffload)
		hw->phyport = 9;

	PMD_DRV_LOG(DEBUG, "Get panel id success: 0x%x", hw->panel_id);

	return 0;
}

static inline int
zxdh_dtb_dump_res_init(struct zxdh_hw *hw, ZXDH_DEV_INIT_CTRL_T *dpp_ctrl)
{
	int ret = 0, i;

	struct zxdh_dtb_bulk_dump_info dtb_dump_baseres[] = {
		{"sdt_vport_att_table", 4 * 1024 * 1024, ZXDH_SDT_VPORT_ATT_TABLE, NULL},
		{"sdt_vlan_att_table", 4 * 1024 * 1024, ZXDH_SDT_VLAN_ATT_TABLE, NULL},
		{"sdt_rss_table", 4 * 1024 * 1024, ZXDH_SDT_RSS_ATT_TABLE, NULL},
		{"sdt_l2_entry_table0", 5 * 1024 * 1024, ZXDH_SDT_L2_ENTRY_TABLE0, NULL},
		{"sdt_l2_entry_table1", 5 * 1024 * 1024, ZXDH_SDT_L2_ENTRY_TABLE1, NULL},
		{"sdt_l2_entry_table2", 5 * 1024 * 1024, ZXDH_SDT_L2_ENTRY_TABLE2, NULL},
		{"sdt_l2_entry_table3", 5 * 1024 * 1024, ZXDH_SDT_L2_ENTRY_TABLE3, NULL},
		{"sdt_mc_table0",       5 * 1024 * 1024, ZXDH_SDT_MC_TABLE0, NULL},
		{"sdt_mc_table1",       5 * 1024 * 1024, ZXDH_SDT_MC_TABLE1, NULL},
		{"sdt_mc_table2",       5 * 1024 * 1024, ZXDH_SDT_MC_TABLE2, NULL},
		{"sdt_mc_table3",       5 * 1024 * 1024, ZXDH_SDT_MC_TABLE3, NULL},
		{"sdt_acl_index_mng",   4 * 1024 * 1024, 30, NULL},
		{"sdt_fd_table",        4 * 1024 * 1024, ZXDH_SDT_FD_TABLE, NULL},
	};

	struct zxdh_dev_shared_data *dev_sd = hw->dev_sd;
	struct zxdh_dtb_shared_data *dtb_data = &dev_sd->dtb_sd;

	for (i = 0; i < (int)RTE_DIM(dtb_dump_baseres); i++) {
		struct zxdh_dtb_bulk_dump_info *p = dtb_dump_baseres + i;
		char buf[ZXDH_MAX_NAME_LEN] = {0};
		snprintf(buf, sizeof(buf), "%s_%x", p->mz_name, hw->dev_id);
		p->mz_name = buf;

		const struct rte_memzone *generic_dump_mz =
				rte_memzone_reserve_aligned(p->mz_name, p->mz_size,
				SOCKET_ID_ANY, 0, RTE_CACHE_LINE_SIZE);
		if (generic_dump_mz == NULL) {
			PMD_DRV_LOG(ERR, "Cannot alloc mem for dtb table bulk dump, mz_name is %s, mz_size is %u",
				p->mz_name, p->mz_size);
			ret = -ENOMEM;
			return ret;
		}
		p->mz = generic_dump_mz;
		dpp_ctrl->dump_addr_info[i].vir_addr = generic_dump_mz->addr_64;
		dpp_ctrl->dump_addr_info[i].phy_addr = generic_dump_mz->iova;
		dpp_ctrl->dump_addr_info[i].sdt_no   = p->sdt_no;
		dpp_ctrl->dump_addr_info[i].size	 = p->mz_size;

		dtb_data->dtb_table_bulk_dump_mz[dpp_ctrl->dump_sdt_num] = generic_dump_mz;
		dpp_ctrl->dump_sdt_num++;
	}
	return ret;
}

static int
zxdh_np_dtb_res_init(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	struct zxdh_bar_offset_params param = {0};
	struct zxdh_bar_offset_res res = {0};
	char buf[ZXDH_MAX_NAME_LEN] = {0};
	struct zxdh_dtb_shared_data *dtb_data = &hw->dev_sd->dtb_sd;
	struct zxdh_dev_nic_shared_data *dev_nic_sd = hw->dev_nic_sd;
	int ret = 0;

	if (dtb_data->init_done) {
		PMD_DRV_LOG(DEBUG, "DTB res already init done, dev %s no need init",
			dev->device->name);
		return 0;
	}

	dtb_data->queueid = ZXDH_INVALID_DTBQUE;
	dtb_data->bind_device = dev;

	ZXDH_DEV_INIT_CTRL_T *dpp_ctrl = rte_zmalloc(NULL, sizeof(*dpp_ctrl) +
			sizeof(ZXDH_DTB_ADDR_INFO_T) * 256, 0);
	if (dpp_ctrl == NULL) {
		PMD_DRV_LOG(ERR, "dev %s annot allocate memory for dpp_ctrl", dev->device->name);
		ret = -ENOMEM;
		return ret;
	}
	dpp_ctrl->queue_id = 0xff;
	dpp_ctrl->vport = hw->vport.vport;
	dpp_ctrl->vector = ZXDH_MSIX_INTR_DTB_VEC;
	strlcpy(dpp_ctrl->port_name, dev->device->name, sizeof(dpp_ctrl->port_name));
	dpp_ctrl->pcie_vir_addr = hw->bar_addr[0];

	param.pcie_id = hw->pcie_id;
	param.virt_addr = hw->bar_addr[0] + ZXDH_CTRLCH_OFFSET;
	param.type = ZXDH_URI_NP;

	ret = zxdh_get_bar_offset(&param, &res);
	if (ret) {
		PMD_DRV_LOG(ERR, "dev %s get npbar offset failed", dev->device->name);
		goto free_res;
	}
	dpp_ctrl->np_bar_len = res.bar_length;
	dpp_ctrl->np_bar_offset = res.bar_offset;

	if (!dtb_data->dtb_table_conf_mz) {
		snprintf(buf, sizeof(buf), "%s_%x", "zxdh_dtb_table_conf_mz", hw->dev_id);
		const struct rte_memzone *conf_mz = rte_memzone_reserve_aligned(buf,
				ZXDH_DTB_TABLE_CONF_SIZE, SOCKET_ID_ANY, 0, RTE_CACHE_LINE_SIZE);

		if (conf_mz == NULL) {
			PMD_DRV_LOG(ERR,
				"dev %s annot allocate memory for dtb table conf",
				dev->device->name);
			ret = -ENOMEM;
			goto free_res;
		}
		dpp_ctrl->down_vir_addr = conf_mz->addr_64;
		dpp_ctrl->down_phy_addr = conf_mz->iova;
		dtb_data->dtb_table_conf_mz = conf_mz;
	}

	if (!dtb_data->dtb_table_dump_mz) {
		memset(buf, '\0', sizeof(buf));
		snprintf(buf, sizeof(buf), "%s_%x", "zxdh_dtb_table_dump_mz", hw->dev_id);
		const struct rte_memzone *dump_mz = rte_memzone_reserve_aligned(buf,
				ZXDH_DTB_TABLE_DUMP_SIZE, SOCKET_ID_ANY, 0, RTE_CACHE_LINE_SIZE);

		if (dump_mz == NULL) {
			PMD_DRV_LOG(ERR,
				"dev %s Cannot allocate memory for dtb table dump",
				dev->device->name);
			ret = -ENOMEM;
			goto free_res;
		}
		dpp_ctrl->dump_vir_addr = dump_mz->addr_64;
		dpp_ctrl->dump_phy_addr = dump_mz->iova;
		dtb_data->dtb_table_dump_mz = dump_mz;
	}

	ret = zxdh_dtb_dump_res_init(hw, dpp_ctrl);
	if (ret) {
		PMD_DRV_LOG(ERR, "dev %s zxdh_dtb_dump_res_init failed", dev->device->name);
		goto free_res;
	}

	ret = zxdh_np_host_init(hw->dev_id, dpp_ctrl);
	if (ret) {
		PMD_DRV_LOG(ERR, "dev %s dpp host np init failed", dev->device->name);
		goto free_res;
	}

	PMD_DRV_LOG(DEBUG, "dev %s dpp host np init ok.dtb queue %u",
		dev->device->name, dpp_ctrl->queue_id);
	dtb_data->queueid = dpp_ctrl->queue_id;
	dtb_data->dev_refcnt++;
	dev_nic_sd->dtb_used_num++;
	dtb_data->init_done = 1;
	rte_free(dpp_ctrl);
	return 0;

free_res:
	zxdh_np_dtb_data_res_free(hw);
	rte_free(dpp_ctrl);
	return ret;
}

static inline uint16_t
zxdh_get_dev_shared_data_idx(uint32_t dev_serial_id)
{
	uint16_t idx = 0;
	for (; idx < ZXDH_SLOT_MAX; idx++) {
		if (dev_nic_sd[idx].serial_id == dev_serial_id || dev_nic_sd[idx].serial_id == 0)
			return idx;
	}

	PMD_DRV_LOG(ERR, "dev serial_id[%u] can not found in global dev_nic_share_data arrays",
		dev_serial_id);
	return ZXDH_INVALID_SLOT_IDX;
}

static int zxdh_init_dev_share_data(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	uint32_t serial_id = (pci_dev->addr.domain << 16) |
				(pci_dev->addr.bus << 8) | pci_dev->addr.devid;
	uint16_t slot_id = 0;

	if (serial_id <= 0) {
		PMD_DRV_LOG(ERR, "failed to get pcie bus-info %s", pci_dev->name);
		return -EINVAL;
	}

	slot_id = zxdh_get_dev_shared_data_idx(serial_id);
	if (slot_id == ZXDH_INVALID_SLOT_IDX)
		return -EINVAL;

	hw->slot_id = slot_id;
	hw->dev_id = hw->pcie_id;
	dev_nic_sd[slot_id].serial_id = serial_id;
	hw->dev_nic_sd = &dev_nic_sd[slot_id];
	hw->dev_sd = rte_zmalloc("zxdh_dtb", sizeof(struct zxdh_dev_shared_data), 0);

	return 0;
}

static int
zxdh_init_shared_data(void)
{
	const struct rte_memzone *mz;
	int ret = 0;

	rte_spinlock_lock(&zxdh_shared_data_lock);
	if (zxdh_shared_data == NULL) {
		if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
			/* Allocate shared memory. */
			mz = rte_memzone_reserve("zxdh_pmd_shared_data",
					sizeof(*zxdh_shared_data), SOCKET_ID_ANY, 0);
			if (mz == NULL) {
				PMD_DRV_LOG(ERR, "Cannot allocate zxdh shared data");
				ret = -rte_errno;
				goto error;
			}
			zxdh_shared_data = mz->addr;
			memset(zxdh_shared_data, 0, sizeof(*zxdh_shared_data));
			rte_spinlock_init(&zxdh_shared_data->lock);
		} else { /* Lookup allocated shared memory. */
			mz = rte_memzone_lookup("zxdh_pmd_shared_data");
			if (mz == NULL) {
				PMD_DRV_LOG(ERR, "Cannot attach zxdh shared data");
				ret = -rte_errno;
				goto error;
			}
			zxdh_shared_data = mz->addr;
		}
	}

error:
	rte_spinlock_unlock(&zxdh_shared_data_lock);
	return ret;
}

static void
zxdh_free_sh_res(void)
{
	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		rte_spinlock_lock(&zxdh_shared_data_lock);
		if (zxdh_shared_data != NULL && zxdh_shared_data->init_done &&
			(--zxdh_shared_data->dev_refcnt == 0)) {
			rte_mempool_free(zxdh_shared_data->flow_mp);
			rte_mempool_free(zxdh_shared_data->mtr_mp);
			rte_mempool_free(zxdh_shared_data->mtr_profile_mp);
			rte_mempool_free(zxdh_shared_data->mtr_policy_mp);
		}
		rte_spinlock_unlock(&zxdh_shared_data_lock);
	}
}

static int
zxdh_init_sh_res(struct zxdh_shared_data *sd)
{
	const char *MZ_ZXDH_FLOW_MP        = "zxdh_flow_mempool";
	const char *MZ_ZXDH_MTR_MP         = "zxdh_mtr_mempool";
	const char *MZ_ZXDH_MTR_PROFILE_MP = "zxdh_mtr_profile_mempool";
	const char *MZ_ZXDH_MTR_POLICY_MP = "zxdh_mtr_policy_mempool";
	struct rte_mempool *flow_mp = NULL;
	struct rte_mempool *mtr_mp = NULL;
	struct rte_mempool *mtr_profile_mp = NULL;
	struct rte_mempool *mtr_policy_mp = NULL;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		flow_mp = rte_mempool_create(MZ_ZXDH_FLOW_MP, ZXDH_MAX_FLOW_NUM,
			sizeof(struct zxdh_flow), 64, 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
		if (flow_mp == NULL) {
			PMD_DRV_LOG(ERR, "Cannot allocate zxdh flow mempool");
			goto error;
		}
		mtr_mp = rte_mempool_create(MZ_ZXDH_MTR_MP, ZXDH_MAX_MTR_NUM,
			sizeof(struct zxdh_mtr_object), 64, 0,
			NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
		if (mtr_mp == NULL) {
			PMD_DRV_LOG(ERR, "Cannot allocate zxdh mtr mempool");
			goto error;
		}
		mtr_profile_mp = rte_mempool_create(MZ_ZXDH_MTR_PROFILE_MP,
			MAX_MTR_PROFILE_NUM, sizeof(struct zxdh_meter_profile),
			64, 0, NULL, NULL, NULL,
			NULL, SOCKET_ID_ANY, 0);
		if (mtr_profile_mp == NULL) {
			PMD_DRV_LOG(ERR, "Cannot allocate zxdh mtr profile mempool");
			goto error;
		}
		mtr_policy_mp = rte_mempool_create(MZ_ZXDH_MTR_POLICY_MP,
			ZXDH_MAX_POLICY_NUM, sizeof(struct zxdh_meter_policy),
			64, 0, NULL, NULL, NULL, NULL, SOCKET_ID_ANY, 0);
		if (mtr_policy_mp == NULL) {
			PMD_DRV_LOG(ERR, "Cannot allocate zxdh mtr profile mempool");
			goto error;
		}
		sd->flow_mp = flow_mp;
		sd->mtr_mp = mtr_mp;
		sd->mtr_profile_mp = mtr_profile_mp;
		sd->mtr_policy_mp = mtr_policy_mp;
		TAILQ_INIT(&zxdh_shared_data->meter_profile_list);
		TAILQ_INIT(&zxdh_shared_data->mtr_list);
		TAILQ_INIT(&zxdh_shared_data->mtr_policy_list);
	}
	return 0;

error:
	rte_mempool_free(mtr_policy_mp);
	rte_mempool_free(mtr_profile_mp);
	rte_mempool_free(mtr_mp);
	rte_mempool_free(flow_mp);
	return -rte_errno;
}

static int
zxdh_init_once(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	int ret = 0;

	if (hw->is_pf) {
		ret = zxdh_init_dev_share_data(eth_dev);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "init dev share date failed");
			return ret;
		}
	}

	if (zxdh_init_shared_data())
		return -1;

	struct zxdh_shared_data *sd = zxdh_shared_data;
	rte_spinlock_lock(&sd->lock);
	if (rte_eal_process_type() == RTE_PROC_SECONDARY) {
		if (!sd->init_done) {
			++sd->secondary_cnt;
			sd->init_done = true;
		}
		goto out;
	}
	/* RTE_PROC_PRIMARY */
	if (!sd->init_done) {
		/*shared struct and res init */
		ret = zxdh_init_sh_res(sd);
		if (ret != 0)
			goto out;
		zxdh_flow_global_init();
		rte_spinlock_init(&g_mtr_res.hw_plcr_res_lock);
		memset(&g_mtr_res, 0, sizeof(g_mtr_res));
		sd->init_done = true;
	}
	sd->dev_refcnt++;
out:
	rte_spinlock_unlock(&sd->lock);
	return ret;
}

static int
zxdh_tbl_entry_offline_destroy(struct zxdh_hw *hw)
{
	struct zxdh_dtb_shared_data *dtb_data = &hw->dev_sd->dtb_sd;
	int ret = 0;
	if (!dtb_data->init_done)
		return ret;
	if (hw->is_pf) {
		uint32_t sdt_no;
		sdt_no = ZXDH_SDT_L2_ENTRY_TABLE0 + hw->hash_search_index;
		ret = zxdh_np_dtb_hash_offline_delete(hw->dev_id, dtb_data->queueid, sdt_no, 0);
		if (ret)
			PMD_DRV_LOG(ERR, "sdt_no %d delete failed. code:%d ", sdt_no, ret);
		sdt_no = ZXDH_SDT_MC_TABLE0 + hw->hash_search_index;
		ret = zxdh_np_dtb_hash_offline_delete(hw->dev_id, dtb_data->queueid, sdt_no, 0);
		if (ret)
			PMD_DRV_LOG(ERR, "sdt_no %d delete failed. code:%d ", sdt_no, ret);

		ret = zxdh_np_dtb_acl_offline_delete(hw->dev_id, dtb_data->queueid,
					ZXDH_SDT_FD_TABLE, hw->vport.vport,
					ZXDH_FLOW_STATS_INGRESS_BASE, 1);
		if (ret)
			PMD_DRV_LOG(ERR, "flow offline delete failed. code:%d", ret);
	}
	return ret;
}

static int
zxdh_np_init(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	struct zxdh_dtb_shared_data *dtb_data = &hw->dev_sd->dtb_sd;
	struct zxdh_dev_nic_shared_data *dev_nic_sd = hw->dev_nic_sd;
	int ret = 0;

	if (!hw->is_pf)
		return 0;

	hw->dev_id = (hw->pcie_id << 16) | (hw->slot_id & 0xffff);

	ret = zxdh_np_dtb_res_init(eth_dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "np dtb init failed, ret:%d", ret);
		return ret;
	}

	if (dev_nic_sd->init_done) {
		zxdh_tbl_entry_offline_destroy(hw);
		PMD_DRV_LOG(ERR, "no need to init apt res. slot_id %d dtb chanenl %d dtb_used_num %d",
			hw->slot_id, dtb_data->queueid, hw->dev_nic_sd->dtb_used_num);
		return 0;
	}
	ret = zxdh_np_se_res_get_and_init(hw->dev_id, ZXDH_SE_STD_NIC_RES_TYPE);
	if (ret) {
		PMD_DRV_LOG(ERR, "dpp apt init failed, code:%d ", ret);
		return -ret;
	}
	if (!hw->switchoffload) {
		if (hw->hash_search_index >= ZXDH_HASHIDX_MAX) {
			PMD_DRV_LOG(ERR, "invalid hash idx %d", hw->hash_search_index);
			return -1;
		}
		zxdh_tbl_entry_offline_destroy(hw);
	}
	dev_nic_sd->init_done = 1;

	PMD_DRV_LOG(DEBUG, "np init ok");
	return 0;
}

static int
zxdh_tables_init(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	ret = zxdh_port_attr_init(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "zxdh_port_attr_init failed");
		return ret;
	}

	ret = zxdh_panel_table_init(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "panel table init failed");
		return ret;
	}

	ret = zxdh_promisc_table_init(dev);
	if (ret) {
		PMD_DRV_LOG(ERR, "promisc_table_init failed");
		return ret;
	}

	ret = zxdh_vlan_filter_table_init(hw, hw->vport.vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "vlan filter table init failed");
		return ret;
	}

	ret = zxdh_port_vlan_table_init(hw, hw->vport.vport);
	if (ret) {
		PMD_DRV_LOG(ERR, "port vlan table init failed");
		return ret;
	}

	return ret;
}

static void
zxdh_queue_res_get(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	uint32_t value = 0;
	uint16_t offset = 0;

	offset = hw->vport.epid * 8 + hw->vport.pfid;
	if (hw->is_pf) {
		hw->max_queue_pairs = *(volatile uint8_t *)(hw->bar_addr[0] +
		ZXDH_PF_QUEUE_PAIRS_ADDR);
		PMD_DRV_LOG(DEBUG, "is_pf max_queue_pairs is %x", hw->max_queue_pairs);
	} else {
		hw->max_queue_pairs = *(volatile uint8_t *)(hw->bar_addr[0] +
		ZXDH_VF_QUEUE_PAIRS_ADDR + offset);
		PMD_DRV_LOG(DEBUG, "is_vf max_queue_pairs is %x", hw->max_queue_pairs);
	}

	/*  pf/vf read queue start id and queue_max cfg */
	value = *(volatile uint32_t *)(hw->bar_addr[0] + ZXDH_QUEUE_POOL_ADDR + offset * 4);
	hw->queue_pool_count = value & 0x0000ffff;
	hw->queue_pool_start = value >> 16;
	if (hw->max_queue_pairs > ZXDH_RX_QUEUES_MAX || hw->max_queue_pairs == 0)
		hw->max_queue_pairs = ZXDH_RX_QUEUES_MAX;
	if (hw->queue_pool_count > ZXDH_TOTAL_QUEUES_NUM / 2 || hw->queue_pool_count == 0)
		hw->queue_pool_count = ZXDH_TOTAL_QUEUES_NUM / 2;
	if (hw->queue_pool_start > ZXDH_TOTAL_QUEUES_NUM / 2)
		hw->queue_pool_start = 0;
}

static int
zxdh_priv_res_init(struct zxdh_hw *hw)
{
	if (hw->is_pf) {
		hw->vfinfo = rte_zmalloc("vfinfo", ZXDH_MAX_VF * sizeof(struct vfinfo), 4);
		if (hw->vfinfo == NULL) {
			PMD_DRV_LOG(ERR, "vfinfo malloc failed");
			return -ENOMEM;
		}
	} else {
		hw->vfinfo = NULL;
	}

	hw->channel_context = rte_zmalloc("zxdh_chnlctx",
			sizeof(struct zxdh_chnl_context) * ZXDH_QUEUES_NUM_MAX, 0);
	if (hw->channel_context == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate channel_context");
		return -ENOMEM;
	}
	return 0;
}


static uint8_t
is_pf(uint16_t device_id)
{
	return (device_id == ZXDH_E310_PF_DEVICEID ||
			device_id == ZXDH_E312_PF_DEVICEID ||
			device_id == ZXDH_E312S_PF_DEVICEID ||
			device_id == ZXDH_E316_PF_DEVICEID ||
			device_id == ZXDH_E310_RDMA_PF_DEVICEID ||
			device_id == ZXDH_E312_RDMA_PF_DEVICEID ||
			device_id == ZXDH_I510_OVS_PF_DEVICEID ||
			device_id == ZXDH_I510_BOND_PF_DEVICEID ||
			device_id == ZXDH_I511_OVS_PF_DEVICEID ||
			device_id == ZXDH_I511_BOND_PF_DEVICEID);
}

static uint8_t
is_inic_pf(uint16_t device_id)
{
	return (device_id == ZXDH_I510_OVS_PF_DEVICEID ||
			device_id == ZXDH_I510_BOND_PF_DEVICEID ||
			device_id == ZXDH_I511_OVS_PF_DEVICEID ||
			device_id == ZXDH_I511_BOND_PF_DEVICEID);
}

static int
zxdh_eth_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	int ret = 0;

	eth_dev->dev_ops = &zxdh_eth_dev_ops;

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
		PMD_DRV_LOG(ERR, "Bad mem resource");
		return -EIO;
	}

	hw->device_id = pci_dev->id.device_id;
	hw->port_id = eth_dev->data->port_id;
	hw->eth_dev = eth_dev;
	hw->speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	hw->duplex = RTE_ETH_LINK_FULL_DUPLEX;
	hw->slot_id = ZXDH_INVALID_SLOT_IDX;
	hw->is_pf = 0;

	if (is_pf(pci_dev->id.device_id)) {
		hw->is_pf = 1;
		if (is_inic_pf(pci_dev->id.device_id))
			hw->switchoffload = 1;
	}

	ret = zxdh_init_once(eth_dev);
	if (ret != 0)
		goto err_zxdh_init;

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

	ret = zxdh_np_init(eth_dev);
	if (ret)
		goto err_zxdh_init;

	zxdh_flow_init(eth_dev);
	zxdh_queue_res_get(eth_dev);
	zxdh_msg_cb_reg(hw);
	if (zxdh_priv_res_init(hw) != 0)
		goto err_zxdh_init;
	ret = zxdh_configure_intr(eth_dev);
	if (ret != 0)
		goto err_zxdh_init;

	ret = zxdh_tables_init(eth_dev);
	if (ret != 0)
		goto err_zxdh_init;

	return ret;

err_zxdh_init:
	zxdh_intr_release(eth_dev);
	zxdh_np_uninit(eth_dev);
	zxdh_bar_msg_chan_exit();
	zxdh_priv_res_free(hw);
	zxdh_free_sh_res();
	rte_free(hw->dev_sd);
	hw->dev_sd = NULL;
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
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312S_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312S_VF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E316_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E316_VF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E310_RDMA_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E310_RDMA_VF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312_RDMA_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_E312_RDMA_VF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_I510_OVS_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_I510_BOND_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_I511_OVS_PF_DEVICEID)},
	{RTE_PCI_DEVICE(ZXDH_PCI_VENDOR_ID, ZXDH_I511_BOND_PF_DEVICEID)},
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
