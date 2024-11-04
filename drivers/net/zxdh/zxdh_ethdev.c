/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#include <ethdev_pci.h>
#include <bus_pci_driver.h>
#include <rte_ethdev.h>

#include "zxdh_ethdev.h"
#include "zxdh_logs.h"
#include "zxdh_pci.h"
#include "zxdh_msg.h"
#include "zxdh_common.h"
#include "zxdh_queue.h"

struct zxdh_hw_internal zxdh_hw_internal[RTE_MAX_ETHPORTS];

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
	dev_info->max_rx_queues    = RTE_MIN(hw->max_queue_pairs, ZXDH_RX_QUEUES_MAX);
	dev_info->max_tx_queues    = RTE_MIN(hw->max_queue_pairs, ZXDH_TX_QUEUES_MAX);
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

	for (i = 0; i < dev->data->nb_rx_queues; ++i) {
		ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2], ZXDH_MSI_NO_VECTOR);
		ZXDH_VTPCI_OPS(hw)->set_queue_irq(hw, hw->vqs[i * 2 + 1], ZXDH_MSI_NO_VECTOR);
	}
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

	if (zxdh_intr_unmask(dev) < 0)
		PMD_DRV_LOG(ERR, "interrupt enable failed");
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
		PMD_DRV_LOG(ERR, " to allocate risc_intr");
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
		 !vtpci_with_feature(hw, ZXDH_NET_F_GUEST_CSUM)) {
		PMD_DRV_LOG(ERR, "rx checksum not available on this host");
		return -ENOTSUP;
	}

	if ((rx_offloads & RTE_ETH_RX_OFFLOAD_TCP_LRO) &&
		(!vtpci_with_feature(hw, ZXDH_NET_F_GUEST_TSO4) ||
		 !vtpci_with_feature(hw, ZXDH_NET_F_GUEST_TSO6))) {
		PMD_DRV_LOG(ERR, "Large Receive Offload not available on this host");
		return -ENOTSUP;
	}
	return 0;
}

static bool
rx_offload_enabled(struct zxdh_hw *hw)
{
	return vtpci_with_feature(hw, ZXDH_NET_F_GUEST_CSUM) ||
		   vtpci_with_feature(hw, ZXDH_NET_F_GUEST_TSO4) ||
		   vtpci_with_feature(hw, ZXDH_NET_F_GUEST_TSO6);
}

static bool
tx_offload_enabled(struct zxdh_hw *hw)
{
	return vtpci_with_feature(hw, ZXDH_NET_F_CSUM) ||
		   vtpci_with_feature(hw, ZXDH_NET_F_HOST_TSO4) ||
		   vtpci_with_feature(hw, ZXDH_NET_F_HOST_TSO6) ||
		   vtpci_with_feature(hw, ZXDH_NET_F_HOST_UFO);
}

static void
zxdh_dev_free_mbufs(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t nr_vq = hw->queue_num;
	uint32_t i = 0;

	const char *type = NULL;
	struct zxdh_virtqueue *vq = NULL;
	struct rte_mbuf *buf = NULL;
	int32_t queue_type = 0;

	if (hw->vqs == NULL)
		return;

	for (i = 0; i < nr_vq; i++) {
		vq = hw->vqs[i];
		if (!vq)
			continue;

		queue_type = zxdh_get_queue_type(i);
		if (queue_type == ZXDH_VTNET_RQ)
			type = "rxq";
		else if (queue_type == ZXDH_VTNET_TQ)
			type = "txq";
		else
			continue;
		PMD_DRV_LOG(DEBUG, "Before freeing %s[%d] used and unused buf", type, i);

		while ((buf = zxdh_virtqueue_detach_unused(vq)) != NULL)
			rte_pktmbuf_free(buf);
	}
}

static int32_t
zxdh_get_available_channel(struct rte_eth_dev *dev, uint8_t queue_type)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	uint16_t base    = (queue_type == ZXDH_VTNET_RQ) ? 0 : 1;
	uint16_t i       = 0;
	uint16_t j       = 0;
	uint16_t done    = 0;
	int32_t ret = 0;

	ret = zxdh_timedlock(hw, 1000);
	if (ret) {
		PMD_DRV_LOG(ERR, "Acquiring hw lock got failed, timeout");
		return -1;
	}

	/* Iterate COI table and find free channel */
	for (i = ZXDH_QUEUES_BASE / 32; i < ZXDH_TOTAL_QUEUES_NUM / 32; i++) {
		uint32_t addr = ZXDH_QUERES_SHARE_BASE + (i * sizeof(uint32_t));
		uint32_t var = zxdh_read_bar_reg(dev, ZXDH_BAR0_INDEX, addr);

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
	zxdh_release_lock(hw);
	/* check for no channel condition */
	if (done != 1) {
		PMD_DRV_LOG(ERR, "NO availd queues");
		return -1;
	}
	/* reruen available channel ID */
	return (i * 32) + j;
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
	vring_init_packed(&vq->vq_packed.ring, ring_mem, ZXDH_PCI_VRING_ALIGN, size);
	vring_desc_init_packed(vq, size);
	virtqueue_disable_intr(vq);
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
	size = vring_size(hw, vq_size, ZXDH_PCI_VRING_ALIGN);
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

			vring_desc_init_indirect_packed(start_dp, RTE_DIM(txr[i].tx_packed_indir));
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

static int32_t
zxdh_alloc_queues(struct rte_eth_dev *dev, uint16_t nr_vq)
{
	uint16_t lch;
	struct zxdh_hw *hw = dev->data->dev_private;

	hw->vqs = rte_zmalloc(NULL, sizeof(struct zxdh_virtqueue *) * nr_vq, 0);
	if (!hw->vqs) {
		PMD_DRV_LOG(ERR, "Failed to allocate vqs");
		return -ENOMEM;
	}
	for (lch = 0; lch < nr_vq; lch++) {
		if (zxdh_acquire_channel(dev, lch) < 0) {
			PMD_DRV_LOG(ERR, "Failed to acquire the channels");
			zxdh_free_queues(dev);
			return -1;
		}
		if (zxdh_init_queue(dev, lch) < 0) {
			PMD_DRV_LOG(ERR, "Failed to alloc virtio queue");
			zxdh_free_queues(dev);
			return -1;
		}
	}
	return 0;
}


static int32_t
zxdh_dev_configure(struct rte_eth_dev *dev)
{
	const struct rte_eth_rxmode *rxmode = &dev->data->dev_conf.rxmode;
	const struct rte_eth_txmode *txmode = &dev->data->dev_conf.txmode;
	struct zxdh_hw *hw = dev->data->dev_private;
	uint32_t nr_vq = 0;
	int32_t  ret = 0;

	if (dev->data->nb_rx_queues != dev->data->nb_tx_queues) {
		PMD_DRV_LOG(ERR, "nb_rx_queues=%d and nb_tx_queues=%d not equal!",
					 dev->data->nb_rx_queues, dev->data->nb_tx_queues);
		return -EINVAL;
	}
	if ((dev->data->nb_rx_queues + dev->data->nb_tx_queues) >= ZXDH_QUEUES_NUM_MAX) {
		PMD_DRV_LOG(ERR, "nb_rx_queues=%d + nb_tx_queues=%d must < (%d)!",
					 dev->data->nb_rx_queues, dev->data->nb_tx_queues,
					 ZXDH_QUEUES_NUM_MAX);
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

	hw->has_tx_offload = tx_offload_enabled(hw);
	hw->has_rx_offload = rx_offload_enabled(hw);

	nr_vq = dev->data->nb_rx_queues + dev->data->nb_tx_queues;
	if (nr_vq == hw->queue_num)
		return 0;

	PMD_DRV_LOG(DEBUG, "queue changed need reset ");
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

	hw->queue_num = nr_vq;
	ret = zxdh_alloc_queues(dev, nr_vq);
	if (ret < 0)
		return ret;

	zxdh_datach_set(dev);

	if (zxdh_configure_intr(dev) < 0) {
		PMD_DRV_LOG(ERR, "Failed to configure interrupt");
		zxdh_free_queues(dev);
		return -1;
	}

	zxdh_pci_reinit_complete(hw);

	return ret;
}

static int
zxdh_dev_close(struct rte_eth_dev *dev)
{
	struct zxdh_hw *hw = dev->data->dev_private;
	int ret = 0;

	zxdh_intr_release(dev);
	zxdh_pci_reset(hw);

	zxdh_dev_free_mbufs(dev);
	zxdh_free_queues(dev);

	zxdh_bar_msg_chan_exit();

	if (dev->data->mac_addrs != NULL) {
		rte_free(dev->data->mac_addrs);
		dev->data->mac_addrs = NULL;
	}

	return ret;
}

/* dev_ops for zxdh, bare necessities for basic operation */
static const struct eth_dev_ops zxdh_eth_dev_ops = {
	.dev_configure			 = zxdh_dev_configure,
	.dev_close				 = zxdh_dev_close,
	.dev_infos_get			 = zxdh_dev_infos_get,
};

static int32_t
zxdh_init_device(struct rte_eth_dev *eth_dev)
{
	struct zxdh_hw *hw = eth_dev->data->dev_private;
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	int ret = 0;

	ret = zxdh_read_pci_caps(pci_dev, hw);
	if (ret) {
		PMD_DRV_LOG(ERR, "port 0x%x pci caps read failed .", hw->port_id);
		goto err;
	}

	zxdh_hw_internal[hw->port_id].zxdh_vtpci_ops = &zxdh_dev_pci_ops;
	zxdh_pci_reset(hw);
	zxdh_get_pci_dev_config(hw);

	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac_addr, &eth_dev->data->mac_addrs[0]);

	/* If host does not support both status and MSI-X then disable LSC */
	if (vtpci_with_feature(hw, ZXDH_NET_F_STATUS) && hw->use_msix != ZXDH_MSIX_NONE)
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
	PMD_DRV_LOG(INFO, "Get phyport success: 0x%x", hw->phyport);

	hw->vfid = zxdh_vport_to_vfid(hw->vport);

	if (zxdh_panelid_get(eth_dev, &hw->panel_id) != 0) {
		PMD_DRV_LOG(ERR, "Failed to get panel_id");
		return -1;
	}
	PMD_DRV_LOG(INFO, "Get panel id success: 0x%x", hw->panel_id);

	return 0;
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
		PMD_DRV_LOG(ERR, "Bad mem resource.");
		return -EIO;
	}

	hw->device_id = pci_dev->id.device_id;
	hw->port_id = eth_dev->data->port_id;
	hw->eth_dev = eth_dev;
	hw->speed = RTE_ETH_SPEED_NUM_UNKNOWN;
	hw->duplex = RTE_ETH_LINK_FULL_DUPLEX;
	hw->is_pf = 0;

	if (pci_dev->id.device_id == ZXDH_E310_PF_DEVICEID ||
		pci_dev->id.device_id == ZXDH_E312_PF_DEVICEID) {
		hw->is_pf = 1;
	}

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

	ret = zxdh_configure_intr(eth_dev);
	if (ret != 0)
		goto err_zxdh_init;

	return ret;

err_zxdh_init:
	zxdh_intr_release(eth_dev);
	zxdh_bar_msg_chan_exit();
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
