/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>

#include <rte_interrupts.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>

#include "avf_log.h"
#include "base/avf_prototype.h"
#include "base/avf_adminq_cmd.h"
#include "base/avf_type.h"

#include "avf.h"
#include "avf_rxtx.h"

static int avf_dev_configure(struct rte_eth_dev *dev);
static int avf_dev_start(struct rte_eth_dev *dev);
static void avf_dev_stop(struct rte_eth_dev *dev);
static void avf_dev_close(struct rte_eth_dev *dev);
static void avf_dev_info_get(struct rte_eth_dev *dev,
			     struct rte_eth_dev_info *dev_info);
static const uint32_t *avf_dev_supported_ptypes_get(struct rte_eth_dev *dev);
static int avf_dev_stats_get(struct rte_eth_dev *dev,
			     struct rte_eth_stats *stats);
static void avf_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void avf_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void avf_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void avf_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int avf_dev_add_mac_addr(struct rte_eth_dev *dev,
				struct ether_addr *addr,
				uint32_t index,
				uint32_t pool);
static void avf_dev_del_mac_addr(struct rte_eth_dev *dev, uint32_t index);
static int avf_dev_vlan_filter_set(struct rte_eth_dev *dev,
				   uint16_t vlan_id, int on);
static int avf_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int avf_dev_rss_reta_update(struct rte_eth_dev *dev,
				   struct rte_eth_rss_reta_entry64 *reta_conf,
				   uint16_t reta_size);
static int avf_dev_rss_reta_query(struct rte_eth_dev *dev,
				  struct rte_eth_rss_reta_entry64 *reta_conf,
				  uint16_t reta_size);
static int avf_dev_rss_hash_update(struct rte_eth_dev *dev,
				   struct rte_eth_rss_conf *rss_conf);
static int avf_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				     struct rte_eth_rss_conf *rss_conf);
static int avf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int avf_dev_set_default_mac_addr(struct rte_eth_dev *dev,
					 struct ether_addr *mac_addr);
static int avf_dev_rx_queue_intr_enable(struct rte_eth_dev *dev,
					uint16_t queue_id);
static int avf_dev_rx_queue_intr_disable(struct rte_eth_dev *dev,
					 uint16_t queue_id);

int avf_logtype_init;
int avf_logtype_driver;

static const struct rte_pci_id pci_id_avf_map[] = {
	{ RTE_PCI_DEVICE(AVF_INTEL_VENDOR_ID, AVF_DEV_ID_ADAPTIVE_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops avf_eth_dev_ops = {
	.dev_configure              = avf_dev_configure,
	.dev_start                  = avf_dev_start,
	.dev_stop                   = avf_dev_stop,
	.dev_close                  = avf_dev_close,
	.dev_infos_get              = avf_dev_info_get,
	.dev_supported_ptypes_get   = avf_dev_supported_ptypes_get,
	.link_update                = avf_dev_link_update,
	.stats_get                  = avf_dev_stats_get,
	.promiscuous_enable         = avf_dev_promiscuous_enable,
	.promiscuous_disable        = avf_dev_promiscuous_disable,
	.allmulticast_enable        = avf_dev_allmulticast_enable,
	.allmulticast_disable       = avf_dev_allmulticast_disable,
	.mac_addr_add               = avf_dev_add_mac_addr,
	.mac_addr_remove            = avf_dev_del_mac_addr,
	.vlan_filter_set            = avf_dev_vlan_filter_set,
	.vlan_offload_set           = avf_dev_vlan_offload_set,
	.rx_queue_start             = avf_dev_rx_queue_start,
	.rx_queue_stop              = avf_dev_rx_queue_stop,
	.tx_queue_start             = avf_dev_tx_queue_start,
	.tx_queue_stop              = avf_dev_tx_queue_stop,
	.rx_queue_setup             = avf_dev_rx_queue_setup,
	.rx_queue_release           = avf_dev_rx_queue_release,
	.tx_queue_setup             = avf_dev_tx_queue_setup,
	.tx_queue_release           = avf_dev_tx_queue_release,
	.mac_addr_set               = avf_dev_set_default_mac_addr,
	.reta_update                = avf_dev_rss_reta_update,
	.reta_query                 = avf_dev_rss_reta_query,
	.rss_hash_update            = avf_dev_rss_hash_update,
	.rss_hash_conf_get          = avf_dev_rss_hash_conf_get,
	.rxq_info_get               = avf_dev_rxq_info_get,
	.txq_info_get               = avf_dev_txq_info_get,
	.rx_queue_count             = avf_dev_rxq_count,
	.rx_descriptor_status       = avf_dev_rx_desc_status,
	.tx_descriptor_status       = avf_dev_tx_desc_status,
	.mtu_set                    = avf_dev_mtu_set,
	.rx_queue_intr_enable       = avf_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable      = avf_dev_rx_queue_intr_disable,
};

static int
avf_dev_configure(struct rte_eth_dev *dev)
{
	struct avf_adapter *ad =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf =  AVF_DEV_PRIVATE_TO_VF(ad);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;

	ad->rx_bulk_alloc_allowed = true;
#ifdef RTE_LIBRTE_AVF_INC_VECTOR
	/* Initialize to TRUE. If any of Rx queues doesn't meet the
	 * vector Rx/Tx preconditions, it will be reset.
	 */
	ad->rx_vec_allowed = true;
	ad->tx_vec_allowed = true;
#else
	ad->rx_vec_allowed = false;
	ad->tx_vec_allowed = false;
#endif

	/* Vlan stripping setting */
	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN) {
		if (dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			avf_enable_vlan_strip(ad);
		else
			avf_disable_vlan_strip(ad);
	}
	return 0;
}

static int
avf_init_rss(struct avf_adapter *adapter)
{
	struct avf_info *vf =  AVF_DEV_PRIVATE_TO_VF(adapter);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	struct rte_eth_rss_conf *rss_conf;
	uint8_t i, j, nb_q;
	int ret;

	rss_conf = &adapter->eth_dev->data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = RTE_MIN(adapter->eth_dev->data->nb_rx_queues,
		       AVF_MAX_NUM_QUEUES);

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF)) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}
	if (adapter->eth_dev->data->dev_conf.rxmode.mq_mode != ETH_MQ_RX_RSS) {
		PMD_DRV_LOG(WARNING, "RSS is enabled by PF by default");
		/* set all lut items to default queue */
		for (i = 0; i < vf->vf_res->rss_lut_size; i++)
			vf->rss_lut[i] = 0;
		ret = avf_configure_rss_lut(adapter);
		return ret;
	}

	/* In AVF, RSS enablement is set by PF driver. It is not supported
	 * to set based on rss_conf->rss_hf.
	 */

	/* configure RSS key */
	if (!rss_conf->rss_key) {
		/* Calculate the default hash key */
		for (i = 0; i <= vf->vf_res->rss_key_size; i++)
			vf->rss_key[i] = (uint8_t)rte_rand();
	} else
		rte_memcpy(vf->rss_key, rss_conf->rss_key,
			   RTE_MIN(rss_conf->rss_key_len,
				   vf->vf_res->rss_key_size));

	/* init RSS LUT table */
	for (i = 0, j = 0; i < vf->vf_res->rss_lut_size; i++, j++) {
		if (j >= nb_q)
			j = 0;
		vf->rss_lut[i] = j;
	}
	/* send virtchnnl ops to configure rss*/
	ret = avf_configure_rss_lut(adapter);
	if (ret)
		return ret;
	ret = avf_configure_rss_key(adapter);
	if (ret)
		return ret;

	return 0;
}

static int
avf_init_rxq(struct rte_eth_dev *dev, struct avf_rx_queue *rxq)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_dev_data *dev_data = dev->data;
	uint16_t buf_size, max_pkt_len, len;

	buf_size = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;

	/* Calculate the maximum packet length allowed */
	len = rxq->rx_buf_len * AVF_MAX_CHAINED_RX_BUFFERS;
	max_pkt_len = RTE_MIN(len, dev->data->dev_conf.rxmode.max_rx_pkt_len);

	/* Check if the jumbo frame and maximum packet length are set
	 * correctly.
	 */
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		if (max_pkt_len <= ETHER_MAX_LEN ||
		    max_pkt_len > AVF_FRAME_SIZE_MAX) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
				    "larger than %u and smaller than %u, "
				    "as jumbo frame is enabled",
				    (uint32_t)ETHER_MAX_LEN,
				    (uint32_t)AVF_FRAME_SIZE_MAX);
			return -EINVAL;
		}
	} else {
		if (max_pkt_len < ETHER_MIN_LEN ||
		    max_pkt_len > ETHER_MAX_LEN) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
				    "larger than %u and smaller than %u, "
				    "as jumbo frame is disabled",
				    (uint32_t)ETHER_MIN_LEN,
				    (uint32_t)ETHER_MAX_LEN);
			return -EINVAL;
		}
	}

	rxq->max_pkt_len = max_pkt_len;
	if ((dev_data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_SCATTER) ||
	    (rxq->max_pkt_len + 2 * AVF_VLAN_TAG_SIZE) > buf_size) {
		dev_data->scattered_rx = 1;
	}
	AVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	AVF_WRITE_FLUSH(hw);

	return 0;
}

static int
avf_init_queues(struct rte_eth_dev *dev)
{
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct avf_rx_queue **rxq =
		(struct avf_rx_queue **)dev->data->rx_queues;
	struct avf_tx_queue **txq =
		(struct avf_tx_queue **)dev->data->tx_queues;
	int i, ret = AVF_SUCCESS;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!rxq[i] || !rxq[i]->q_set)
			continue;
		ret = avf_init_rxq(dev, rxq[i]);
		if (ret != AVF_SUCCESS)
			break;
	}
	/* set rx/tx function to vector/scatter/single-segment
	 * according to parameters
	 */
	avf_set_rx_function(dev);
	avf_set_tx_function(dev);

	return ret;
}

static int avf_config_rx_queues_irqs(struct rte_eth_dev *dev,
				     struct rte_intr_handle *intr_handle)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	uint16_t interval, i;
	int vec;

	if (rte_intr_cap_multiple(intr_handle) &&
	    dev->data->dev_conf.intr_conf.rxq) {
		if (rte_intr_efd_enable(intr_handle, dev->data->nb_rx_queues))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec =
			rte_zmalloc("intr_vec",
				    dev->data->nb_rx_queues * sizeof(int), 0);
		if (!intr_handle->intr_vec) {
			PMD_DRV_LOG(ERR, "Failed to allocate %d rx intr_vec",
				    dev->data->nb_rx_queues);
			return -1;
		}
	}

	if (!dev->data->dev_conf.intr_conf.rxq ||
	    !rte_intr_dp_is_en(intr_handle)) {
		/* Rx interrupt disabled, Map interrupt only for writeback */
		vf->nb_msix = 1;
		if (vf->vf_res->vf_cap_flags &
		    VIRTCHNL_VF_OFFLOAD_WB_ON_ITR) {
			/* If WB_ON_ITR supports, enable it */
			vf->msix_base = AVF_RX_VEC_START;
			AVF_WRITE_REG(hw, AVFINT_DYN_CTLN1(vf->msix_base - 1),
				      AVFINT_DYN_CTLN1_ITR_INDX_MASK |
				      AVFINT_DYN_CTLN1_WB_ON_ITR_MASK);
		} else {
			/* If no WB_ON_ITR offload flags, need to set
			 * interrupt for descriptor write back.
			 */
			vf->msix_base = AVF_MISC_VEC_ID;

			/* set ITR to max */
			interval = avf_calc_itr_interval(
					AVF_QUEUE_ITR_INTERVAL_MAX);
			AVF_WRITE_REG(hw, AVFINT_DYN_CTL01,
				      AVFINT_DYN_CTL01_INTENA_MASK |
				      (AVF_ITR_INDEX_DEFAULT <<
				       AVFINT_DYN_CTL01_ITR_INDX_SHIFT) |
				      (interval <<
				       AVFINT_DYN_CTL01_INTERVAL_SHIFT));
		}
		AVF_WRITE_FLUSH(hw);
		/* map all queues to the same interrupt */
		for (i = 0; i < dev->data->nb_rx_queues; i++)
			vf->rxq_map[vf->msix_base] |= 1 << i;
	} else {
		if (!rte_intr_allow_others(intr_handle)) {
			vf->nb_msix = 1;
			vf->msix_base = AVF_MISC_VEC_ID;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				vf->rxq_map[vf->msix_base] |= 1 << i;
				intr_handle->intr_vec[i] = AVF_MISC_VEC_ID;
			}
			PMD_DRV_LOG(DEBUG,
				    "vector %u are mapping to all Rx queues",
				    vf->msix_base);
		} else {
			/* If Rx interrupt is reuquired, and we can use
			 * multi interrupts, then the vec is from 1
			 */
			vf->nb_msix = RTE_MIN(vf->vf_res->max_vectors,
					      intr_handle->nb_efd);
			vf->msix_base = AVF_RX_VEC_START;
			vec = AVF_RX_VEC_START;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				vf->rxq_map[vec] |= 1 << i;
				intr_handle->intr_vec[i] = vec++;
				if (vec >= vf->nb_msix)
					vec = AVF_RX_VEC_START;
			}
			PMD_DRV_LOG(DEBUG,
				    "%u vectors are mapping to %u Rx queues",
				    vf->nb_msix, dev->data->nb_rx_queues);
		}
	}

	if (avf_config_irq_map(adapter)) {
		PMD_DRV_LOG(ERR, "config interrupt mapping failed");
		return -1;
	}
	return 0;
}

static int
avf_start_queues(struct rte_eth_dev *dev)
{
	struct avf_rx_queue *rxq;
	struct avf_tx_queue *txq;
	int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq->tx_deferred_start)
			continue;
		if (avf_dev_tx_queue_start(dev, i) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start queue %u", i);
			return -1;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq->rx_deferred_start)
			continue;
		if (avf_dev_rx_queue_start(dev, i) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start queue %u", i);
			return -1;
		}
	}

	return 0;
}

static int
avf_dev_start(struct rte_eth_dev *dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = dev->intr_handle;

	PMD_INIT_FUNC_TRACE();

	hw->adapter_stopped = 0;

	vf->max_pkt_len = dev->data->dev_conf.rxmode.max_rx_pkt_len;
	vf->num_queue_pairs = RTE_MAX(dev->data->nb_rx_queues,
				      dev->data->nb_tx_queues);

	if (avf_init_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "failed to do Queue init");
		return -1;
	}

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		if (avf_init_rss(adapter) != 0) {
			PMD_DRV_LOG(ERR, "configure rss failed");
			goto err_rss;
		}
	}

	if (avf_configure_queues(adapter) != 0) {
		PMD_DRV_LOG(ERR, "configure queues failed");
		goto err_queue;
	}

	if (avf_config_rx_queues_irqs(dev, intr_handle) != 0) {
		PMD_DRV_LOG(ERR, "configure irq failed");
		goto err_queue;
	}
	/* re-enable intr again, because efd assign may change */
	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		rte_intr_disable(intr_handle);
		rte_intr_enable(intr_handle);
	}

	/* Set all mac addrs */
	avf_add_del_all_mac_addr(adapter, TRUE);

	if (avf_start_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "enable queues failed");
		goto err_mac;
	}

	return 0;

err_mac:
	avf_add_del_all_mac_addr(adapter, FALSE);
err_queue:
err_rss:
	return -1;
}

static void
avf_dev_stop(struct rte_eth_dev *dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = dev->intr_handle;
	int ret, i;

	PMD_INIT_FUNC_TRACE();

	if (hw->adapter_stopped == 1)
		return;

	avf_stop_queues(dev);

	/* Disable the interrupt for Rx */
	rte_intr_efd_disable(intr_handle);
	/* Rx interrupt vector mapping free */
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}

	/* remove all mac addrs */
	avf_add_del_all_mac_addr(adapter, FALSE);
	hw->adapter_stopped = 1;
}

static void
avf_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	memset(dev_info, 0, sizeof(*dev_info));
	dev_info->max_rx_queues = vf->vsi_res->num_queue_pairs;
	dev_info->max_tx_queues = vf->vsi_res->num_queue_pairs;
	dev_info->min_rx_bufsize = AVF_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = AVF_FRAME_SIZE_MAX;
	dev_info->hash_key_size = vf->vf_res->rss_key_size;
	dev_info->reta_size = vf->vf_res->rss_lut_size;
	dev_info->flow_type_rss_offloads = AVF_RSS_OFFLOAD_ALL;
	dev_info->max_mac_addrs = AVF_NUM_MACADDR_MAX;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_TCP_TSO;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = AVF_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = AVF_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = AVF_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = AVF_MAX_RING_DESC,
		.nb_min = AVF_MIN_RING_DESC,
		.nb_align = AVF_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = AVF_MAX_RING_DESC,
		.nb_min = AVF_MIN_RING_DESC,
		.nb_align = AVF_ALIGN_RING_DESC,
	};
}

static const uint32_t *
avf_dev_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_L4_NONFRAG,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_UNKNOWN
	};
	return ptypes;
}

int
avf_dev_link_update(struct rte_eth_dev *dev,
		    __rte_unused int wait_to_complete)
{
	struct rte_eth_link new_link;
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	/* Only read status info stored in VF, and the info is updated
	 *  when receive LINK_CHANGE evnet from PF by Virtchnnl.
	 */
	switch (vf->link_speed) {
	case VIRTCHNL_LINK_SPEED_100MB:
		new_link.link_speed = ETH_SPEED_NUM_100M;
		break;
	case VIRTCHNL_LINK_SPEED_1GB:
		new_link.link_speed = ETH_SPEED_NUM_1G;
		break;
	case VIRTCHNL_LINK_SPEED_10GB:
		new_link.link_speed = ETH_SPEED_NUM_10G;
		break;
	case VIRTCHNL_LINK_SPEED_20GB:
		new_link.link_speed = ETH_SPEED_NUM_20G;
		break;
	case VIRTCHNL_LINK_SPEED_25GB:
		new_link.link_speed = ETH_SPEED_NUM_25G;
		break;
	case VIRTCHNL_LINK_SPEED_40GB:
		new_link.link_speed = ETH_SPEED_NUM_40G;
		break;
	default:
		new_link.link_speed = ETH_SPEED_NUM_NONE;
		break;
	}

	new_link.link_duplex = ETH_LINK_FULL_DUPLEX;
	new_link.link_status = vf->link_up ? ETH_LINK_UP :
					     ETH_LINK_DOWN;
	new_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				ETH_LINK_SPEED_FIXED);

	if (rte_atomic64_cmpset((uint64_t *)&dev->data->dev_link,
				*(uint64_t *)&dev->data->dev_link,
				*(uint64_t *)&new_link) == 0)
		return -1;

	return 0;
}

static void
avf_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (vf->promisc_unicast_enabled)
		return;

	ret = avf_config_promisc(adapter, TRUE, vf->promisc_multicast_enabled);
	if (!ret)
		vf->promisc_unicast_enabled = TRUE;
}

static void
avf_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (!vf->promisc_unicast_enabled)
		return;

	ret = avf_config_promisc(adapter, FALSE, vf->promisc_multicast_enabled);
	if (!ret)
		vf->promisc_unicast_enabled = FALSE;
}

static void
avf_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (vf->promisc_multicast_enabled)
		return;

	ret = avf_config_promisc(adapter, vf->promisc_unicast_enabled, TRUE);
	if (!ret)
		vf->promisc_multicast_enabled = TRUE;
}

static void
avf_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (!vf->promisc_multicast_enabled)
		return;

	ret = avf_config_promisc(adapter, vf->promisc_unicast_enabled, FALSE);
	if (!ret)
		vf->promisc_multicast_enabled = FALSE;
}

static int
avf_dev_add_mac_addr(struct rte_eth_dev *dev, struct ether_addr *addr,
		     __rte_unused uint32_t index,
		     __rte_unused uint32_t pool)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	int err;

	if (is_zero_ether_addr(addr)) {
		PMD_DRV_LOG(ERR, "Invalid Ethernet Address");
		return -EINVAL;
	}

	err = avf_add_del_eth_addr(adapter, addr, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to add MAC address");
		return -EIO;
	}

	vf->mac_num++;

	return 0;
}

static void
avf_dev_del_mac_addr(struct rte_eth_dev *dev, uint32_t index)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	struct ether_addr *addr;
	int err;

	addr = &dev->data->mac_addrs[index];

	err = avf_add_del_eth_addr(adapter, addr, FALSE);
	if (err)
		PMD_DRV_LOG(ERR, "fail to delete MAC address");

	vf->mac_num--;
}

static int
avf_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	int err;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN))
		return -ENOTSUP;

	err = avf_add_del_vlan(adapter, vlan_id, on);
	if (err)
		return -EIO;
	return 0;
}

static int
avf_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	int err;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN))
		return -ENOTSUP;

	/* Vlan stripping setting */
	if (mask & ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		if (dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			err = avf_enable_vlan_strip(adapter);
		else
			err = avf_disable_vlan_strip(adapter);

		if (err)
			return -EIO;
	}
	return 0;
}

static int
avf_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	uint8_t *lut;
	uint16_t i, idx, shift;
	int ret;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	if (reta_size != vf->vf_res->rss_lut_size) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number of hardware can "
			"support (%d)", reta_size, vf->vf_res->rss_lut_size);
		return -EINVAL;
	}

	lut = rte_zmalloc("rss_lut", reta_size, 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}
	/* store the old lut table temporarily */
	rte_memcpy(lut, vf->rss_lut, reta_size);

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			lut[i] = reta_conf[idx].reta[shift];
	}

	rte_memcpy(vf->rss_lut, lut, reta_size);
	/* send virtchnnl ops to configure rss*/
	ret = avf_configure_rss_lut(adapter);
	if (ret) /* revert back */
		rte_memcpy(vf->rss_lut, lut, reta_size);
	rte_free(lut);

	return ret;
}

static int
avf_dev_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);
	uint16_t i, idx, shift;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	if (reta_size != vf->vf_res->rss_lut_size) {
		PMD_DRV_LOG(ERR, "The size of hash lookup table configured "
			"(%d) doesn't match the number of hardware can "
			"support (%d)", reta_size, vf->vf_res->rss_lut_size);
		return -EINVAL;
	}

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = vf->rss_lut[i];
	}

	return 0;
}

static int
avf_dev_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	/* HENA setting, it is enabled by default, no change */
	if (!rss_conf->rss_key || rss_conf->rss_key_len == 0) {
		PMD_DRV_LOG(DEBUG, "No key to be configured");
		return 0;
	} else if (rss_conf->rss_key_len != vf->vf_res->rss_key_size) {
		PMD_DRV_LOG(ERR, "The size of hash key configured "
			"(%d) doesn't match the size of hardware can "
			"support (%d)", rss_conf->rss_key_len,
			vf->vf_res->rss_key_size);
		return -EINVAL;
	}

	rte_memcpy(vf->rss_key, rss_conf->rss_key, rss_conf->rss_key_len);

	return avf_configure_rss_key(adapter);
}

static int
avf_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(adapter);

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	 /* Just set it to default value now. */
	rss_conf->rss_hf = AVF_RSS_OFFLOAD_ALL;

	if (!rss_conf->rss_key)
		return 0;

	rss_conf->rss_key_len = vf->vf_res->rss_key_size;
	rte_memcpy(rss_conf->rss_key, vf->rss_key, rss_conf->rss_key_len);

	return 0;
}

static int
avf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	uint32_t frame_size = mtu + AVF_ETH_OVERHEAD;
	int ret = 0;

	if (mtu < ETHER_MIN_MTU || frame_size > AVF_FRAME_SIZE_MAX)
		return -EINVAL;

	/* mtu setting is forbidden if port is start */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port must be stopped before configuration");
		return -EBUSY;
	}

	if (frame_size > ETHER_MAX_LEN)
		dev->data->dev_conf.rxmode.offloads |=
				DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		dev->data->dev_conf.rxmode.offloads &=
				~DEV_RX_OFFLOAD_JUMBO_FRAME;

	dev->data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

	return ret;
}

static int
avf_dev_set_default_mac_addr(struct rte_eth_dev *dev,
			     struct ether_addr *mac_addr)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	struct ether_addr *perm_addr, *old_addr;
	int ret;

	old_addr = (struct ether_addr *)hw->mac.addr;
	perm_addr = (struct ether_addr *)hw->mac.perm_addr;

	if (is_same_ether_addr(mac_addr, old_addr))
		return 0;

	/* If the MAC address is configured by host, skip the setting */
	if (is_valid_assigned_ether_addr(perm_addr))
		return -EPERM;

	ret = avf_add_del_eth_addr(adapter, old_addr, FALSE);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to delete old MAC:"
			    " %02X:%02X:%02X:%02X:%02X:%02X",
			    old_addr->addr_bytes[0],
			    old_addr->addr_bytes[1],
			    old_addr->addr_bytes[2],
			    old_addr->addr_bytes[3],
			    old_addr->addr_bytes[4],
			    old_addr->addr_bytes[5]);

	ret = avf_add_del_eth_addr(adapter, mac_addr, TRUE);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to add new MAC:"
			    " %02X:%02X:%02X:%02X:%02X:%02X",
			    mac_addr->addr_bytes[0],
			    mac_addr->addr_bytes[1],
			    mac_addr->addr_bytes[2],
			    mac_addr->addr_bytes[3],
			    mac_addr->addr_bytes[4],
			    mac_addr->addr_bytes[5]);

	if (ret)
		return -EIO;

	ether_addr_copy(mac_addr, (struct ether_addr *)hw->mac.addr);
	return 0;
}

static int
avf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct virtchnl_eth_stats *pstats = NULL;
	int ret;

	ret = avf_query_stats(adapter, &pstats);
	if (ret == 0) {
		stats->ipackets = pstats->rx_unicast + pstats->rx_multicast +
						pstats->rx_broadcast;
		stats->opackets = pstats->tx_broadcast + pstats->tx_multicast +
						pstats->tx_unicast;
		stats->imissed = pstats->rx_discards;
		stats->oerrors = pstats->tx_errors + pstats->tx_discards;
		stats->ibytes = pstats->rx_bytes;
		stats->obytes = pstats->tx_bytes;
	} else {
		PMD_DRV_LOG(ERR, "Get statistics failed");
	}
	return -EIO;
}

static int
avf_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	uint16_t msix_intr;

	msix_intr = pci_dev->intr_handle.intr_vec[queue_id];
	if (msix_intr == AVF_MISC_VEC_ID) {
		PMD_DRV_LOG(INFO, "MISC is also enabled for control");
		AVF_WRITE_REG(hw, AVFINT_DYN_CTL01,
			      AVFINT_DYN_CTL01_INTENA_MASK |
			      AVFINT_DYN_CTL01_ITR_INDX_MASK);
	} else {
		AVF_WRITE_REG(hw,
			      AVFINT_DYN_CTLN1(msix_intr - AVF_RX_VEC_START),
			      AVFINT_DYN_CTLN1_INTENA_MASK |
			      AVFINT_DYN_CTLN1_ITR_INDX_MASK);
	}

	AVF_WRITE_FLUSH(hw);

	rte_intr_enable(&pci_dev->intr_handle);

	return 0;
}

static int
avf_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t msix_intr;

	msix_intr = pci_dev->intr_handle.intr_vec[queue_id];
	if (msix_intr == AVF_MISC_VEC_ID) {
		PMD_DRV_LOG(ERR, "MISC is used for control, cannot disable it");
		return -EIO;
	}

	AVF_WRITE_REG(hw,
		      AVFINT_DYN_CTLN1(msix_intr - AVF_RX_VEC_START),
		      0);

	AVF_WRITE_FLUSH(hw);
	return 0;
}

static int
avf_check_vf_reset_done(struct avf_hw *hw)
{
	int i, reset;

	for (i = 0; i < AVF_RESET_WAIT_CNT; i++) {
		reset = AVF_READ_REG(hw, AVFGEN_RSTAT) &
			AVFGEN_RSTAT_VFR_STATE_MASK;
		reset = reset >> AVFGEN_RSTAT_VFR_STATE_SHIFT;
		if (reset == VIRTCHNL_VFR_VFACTIVE ||
		    reset == VIRTCHNL_VFR_COMPLETED)
			break;
		rte_delay_ms(20);
	}

	if (i >= AVF_RESET_WAIT_CNT)
		return -1;

	return 0;
}

static int
avf_init_vf(struct rte_eth_dev *dev)
{
	int i, err, bufsz;
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	err = avf_set_mac_type(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "set_mac_type failed: %d", err);
		goto err;
	}

	err = avf_check_vf_reset_done(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "VF is still resetting");
		goto err;
	}

	avf_init_adminq_parameter(hw);
	err = avf_init_adminq(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "init_adminq failed: %d", err);
		goto err;
	}

	vf->aq_resp = rte_zmalloc("vf_aq_resp", AVF_AQ_BUF_SZ, 0);
	if (!vf->aq_resp) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_aq_resp memory");
		goto err_aq;
	}
	if (avf_check_api_version(adapter) != 0) {
		PMD_INIT_LOG(ERR, "check_api version failed");
		goto err_api;
	}

	bufsz = sizeof(struct virtchnl_vf_resource) +
		(AVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource));
	vf->vf_res = rte_zmalloc("vf_res", bufsz, 0);
	if (!vf->vf_res) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_res memory");
		goto err_api;
	}
	if (avf_get_vf_resource(adapter) != 0) {
		PMD_INIT_LOG(ERR, "avf_get_vf_config failed");
		goto err_alloc;
	}
	/* Allocate memort for RSS info */
	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		vf->rss_key = rte_zmalloc("rss_key",
					  vf->vf_res->rss_key_size, 0);
		if (!vf->rss_key) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_key memory");
			goto err_rss;
		}
		vf->rss_lut = rte_zmalloc("rss_lut",
					  vf->vf_res->rss_lut_size, 0);
		if (!vf->rss_lut) {
			PMD_INIT_LOG(ERR, "unable to allocate rss_lut memory");
			goto err_rss;
		}
	}
	return 0;
err_rss:
	rte_free(vf->rss_key);
	rte_free(vf->rss_lut);
err_alloc:
	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
err_api:
	rte_free(vf->aq_resp);
err_aq:
	avf_shutdown_adminq(hw);
err:
	return -1;
}

/* Enable default admin queue interrupt setting */
static inline void
avf_enable_irq0(struct avf_hw *hw)
{
	/* Enable admin queue interrupt trigger */
	AVF_WRITE_REG(hw, AVFINT_ICR0_ENA1, AVFINT_ICR0_ENA1_ADMINQ_MASK);

	AVF_WRITE_REG(hw, AVFINT_DYN_CTL01, AVFINT_DYN_CTL01_INTENA_MASK |
					    AVFINT_DYN_CTL01_ITR_INDX_MASK);

	AVF_WRITE_FLUSH(hw);
}

static inline void
avf_disable_irq0(struct avf_hw *hw)
{
	/* Disable all interrupt types */
	AVF_WRITE_REG(hw, AVFINT_ICR0_ENA1, 0);
	AVF_WRITE_REG(hw, AVFINT_DYN_CTL01,
		      AVFINT_DYN_CTL01_ITR_INDX_MASK);
	AVF_WRITE_FLUSH(hw);
}

static void
avf_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	avf_disable_irq0(hw);

	avf_handle_virtchnl_msg(dev);

done:
	avf_enable_irq0(hw);
}

static int
avf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct avf_adapter *adapter =
		AVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(adapter);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_INIT_FUNC_TRACE();

	/* assign ops func pointer */
	eth_dev->dev_ops = &avf_eth_dev_ops;
	eth_dev->rx_pkt_burst = &avf_recv_pkts;
	eth_dev->tx_pkt_burst = &avf_xmit_pkts;
	eth_dev->tx_pkt_prepare = &avf_prep_pkts;

	/* For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check if we need a different RX
	 * and TX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		avf_set_rx_function(eth_dev);
		avf_set_tx_function(eth_dev);
		return 0;
	}
	rte_eth_copy_pci_info(eth_dev, pci_dev);

	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.bus_id = pci_dev->addr.bus;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;
	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->back = AVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	adapter->eth_dev = eth_dev;

	if (avf_init_vf(eth_dev) != 0) {
		PMD_INIT_LOG(ERR, "Init vf failed");
		return -1;
	}

	/* copy mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc(
					"avf_mac",
					ETHER_ADDR_LEN * AVF_NUM_MACADDR_MAX,
					0);
	if (!eth_dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes needed to"
			     " store MAC addresses",
			     ETHER_ADDR_LEN * AVF_NUM_MACADDR_MAX);
		return -ENOMEM;
	}
	/* If the MAC address is not configured by host,
	 * generate a random one.
	 */
	if (!is_valid_assigned_ether_addr((struct ether_addr *)hw->mac.addr))
		eth_random_addr(hw->mac.addr);
	ether_addr_copy((struct ether_addr *)hw->mac.addr,
			&eth_dev->data->mac_addrs[0]);

	/* register callback func to eal lib */
	rte_intr_callback_register(&pci_dev->intr_handle,
				   avf_dev_interrupt_handler,
				   (void *)eth_dev);

	/* enable uio intr after callback register */
	rte_intr_enable(&pci_dev->intr_handle);

	/* configure and enable device interrupt */
	avf_enable_irq0(hw);

	return 0;
}

static void
avf_dev_close(struct rte_eth_dev *dev)
{
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	avf_dev_stop(dev);
	avf_shutdown_adminq(hw);
	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* unregister callback func from eal lib */
	rte_intr_callback_unregister(intr_handle,
				     avf_dev_interrupt_handler, dev);
	avf_disable_irq0(hw);
}

static int
avf_dev_uninit(struct rte_eth_dev *dev)
{
	struct avf_info *vf = AVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct avf_hw *hw = AVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;
	if (hw->adapter_stopped == 0)
		avf_dev_close(dev);

	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
	vf->vf_res = NULL;

	rte_free(vf->aq_resp);
	vf->aq_resp = NULL;

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	if (vf->rss_lut) {
		rte_free(vf->rss_lut);
		vf->rss_lut = NULL;
	}
	if (vf->rss_key) {
		rte_free(vf->rss_key);
		vf->rss_key = NULL;
	}

	return 0;
}

static int eth_avf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			     struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct avf_adapter), avf_dev_init);
}

static int eth_avf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, avf_dev_uninit);
}

/* Adaptive virtual function driver struct */
static struct rte_pci_driver rte_avf_pmd = {
	.id_table = pci_id_avf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_IOVA_AS_VA,
	.probe = eth_avf_pci_probe,
	.remove = eth_avf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_avf, rte_avf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_avf, pci_id_avf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_avf, "* igb_uio | vfio-pci");
RTE_INIT(avf_init_log);
static void
avf_init_log(void)
{
	avf_logtype_init = rte_log_register("pmd.net.avf.init");
	if (avf_logtype_init >= 0)
		rte_log_set_level(avf_logtype_init, RTE_LOG_NOTICE);
	avf_logtype_driver = rte_log_register("pmd.net.avf.driver");
	if (avf_logtype_driver >= 0)
		rte_log_set_level(avf_logtype_driver, RTE_LOG_NOTICE);
}

/* memory func for base code */
enum avf_status_code
avf_allocate_dma_mem_d(__rte_unused struct avf_hw *hw,
		       struct avf_dma_mem *mem,
		       u64 size,
		       u32 alignment)
{
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return AVF_ERR_PARAM;

	snprintf(z_name, sizeof(z_name), "avf_dma_%"PRIu64, rte_rand());
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG, alignment, RTE_PGSIZE_2M);
	if (!mz)
		return AVF_ERR_NO_MEMORY;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->phys_addr;
	mem->zone = (const void *)mz;
	PMD_DRV_LOG(DEBUG,
		    "memzone %s allocated with physical address: %"PRIu64,
		    mz->name, mem->pa);

	return AVF_SUCCESS;
}

enum avf_status_code
avf_free_dma_mem_d(__rte_unused struct avf_hw *hw,
		   struct avf_dma_mem *mem)
{
	if (!mem)
		return AVF_ERR_PARAM;

	PMD_DRV_LOG(DEBUG,
		    "memzone %s to be freed with physical address: %"PRIu64,
		    ((const struct rte_memzone *)mem->zone)->name, mem->pa);
	rte_memzone_free((const struct rte_memzone *)mem->zone);
	mem->zone = NULL;
	mem->va = NULL;
	mem->pa = (u64)0;

	return AVF_SUCCESS;
}

enum avf_status_code
avf_allocate_virt_mem_d(__rte_unused struct avf_hw *hw,
			struct avf_virt_mem *mem,
			u32 size)
{
	if (!mem)
		return AVF_ERR_PARAM;

	mem->size = size;
	mem->va = rte_zmalloc("avf", size, 0);

	if (mem->va)
		return AVF_SUCCESS;
	else
		return AVF_ERR_NO_MEMORY;
}

enum avf_status_code
avf_free_virt_mem_d(__rte_unused struct avf_hw *hw,
		    struct avf_virt_mem *mem)
{
	if (!mem)
		return AVF_ERR_PARAM;

	rte_free(mem->va);
	mem->va = NULL;

	return AVF_SUCCESS;
}
