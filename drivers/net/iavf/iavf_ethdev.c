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

#include "iavf_log.h"
#include "base/iavf_prototype.h"
#include "base/iavf_adminq_cmd.h"
#include "base/iavf_type.h"

#include "iavf.h"
#include "iavf_rxtx.h"

static int iavf_dev_configure(struct rte_eth_dev *dev);
static int iavf_dev_start(struct rte_eth_dev *dev);
static void iavf_dev_stop(struct rte_eth_dev *dev);
static void iavf_dev_close(struct rte_eth_dev *dev);
static void iavf_dev_info_get(struct rte_eth_dev *dev,
			     struct rte_eth_dev_info *dev_info);
static const uint32_t *iavf_dev_supported_ptypes_get(struct rte_eth_dev *dev);
static int iavf_dev_stats_get(struct rte_eth_dev *dev,
			     struct rte_eth_stats *stats);
static void iavf_dev_stats_reset(struct rte_eth_dev *dev);
static void iavf_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void iavf_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void iavf_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void iavf_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int iavf_dev_add_mac_addr(struct rte_eth_dev *dev,
				struct ether_addr *addr,
				uint32_t index,
				uint32_t pool);
static void iavf_dev_del_mac_addr(struct rte_eth_dev *dev, uint32_t index);
static int iavf_dev_vlan_filter_set(struct rte_eth_dev *dev,
				   uint16_t vlan_id, int on);
static int iavf_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int iavf_dev_rss_reta_update(struct rte_eth_dev *dev,
				   struct rte_eth_rss_reta_entry64 *reta_conf,
				   uint16_t reta_size);
static int iavf_dev_rss_reta_query(struct rte_eth_dev *dev,
				  struct rte_eth_rss_reta_entry64 *reta_conf,
				  uint16_t reta_size);
static int iavf_dev_rss_hash_update(struct rte_eth_dev *dev,
				   struct rte_eth_rss_conf *rss_conf);
static int iavf_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
				     struct rte_eth_rss_conf *rss_conf);
static int iavf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int iavf_dev_set_default_mac_addr(struct rte_eth_dev *dev,
					 struct ether_addr *mac_addr);
static int iavf_dev_rx_queue_intr_enable(struct rte_eth_dev *dev,
					uint16_t queue_id);
static int iavf_dev_rx_queue_intr_disable(struct rte_eth_dev *dev,
					 uint16_t queue_id);

int iavf_logtype_init;
int iavf_logtype_driver;

static const struct rte_pci_id pci_id_iavf_map[] = {
	{ RTE_PCI_DEVICE(IAVF_INTEL_VENDOR_ID, IAVF_DEV_ID_ADAPTIVE_VF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops iavf_eth_dev_ops = {
	.dev_configure              = iavf_dev_configure,
	.dev_start                  = iavf_dev_start,
	.dev_stop                   = iavf_dev_stop,
	.dev_close                  = iavf_dev_close,
	.dev_infos_get              = iavf_dev_info_get,
	.dev_supported_ptypes_get   = iavf_dev_supported_ptypes_get,
	.link_update                = iavf_dev_link_update,
	.stats_get                  = iavf_dev_stats_get,
	.stats_reset                = iavf_dev_stats_reset,
	.promiscuous_enable         = iavf_dev_promiscuous_enable,
	.promiscuous_disable        = iavf_dev_promiscuous_disable,
	.allmulticast_enable        = iavf_dev_allmulticast_enable,
	.allmulticast_disable       = iavf_dev_allmulticast_disable,
	.mac_addr_add               = iavf_dev_add_mac_addr,
	.mac_addr_remove            = iavf_dev_del_mac_addr,
	.vlan_filter_set            = iavf_dev_vlan_filter_set,
	.vlan_offload_set           = iavf_dev_vlan_offload_set,
	.rx_queue_start             = iavf_dev_rx_queue_start,
	.rx_queue_stop              = iavf_dev_rx_queue_stop,
	.tx_queue_start             = iavf_dev_tx_queue_start,
	.tx_queue_stop              = iavf_dev_tx_queue_stop,
	.rx_queue_setup             = iavf_dev_rx_queue_setup,
	.rx_queue_release           = iavf_dev_rx_queue_release,
	.tx_queue_setup             = iavf_dev_tx_queue_setup,
	.tx_queue_release           = iavf_dev_tx_queue_release,
	.mac_addr_set               = iavf_dev_set_default_mac_addr,
	.reta_update                = iavf_dev_rss_reta_update,
	.reta_query                 = iavf_dev_rss_reta_query,
	.rss_hash_update            = iavf_dev_rss_hash_update,
	.rss_hash_conf_get          = iavf_dev_rss_hash_conf_get,
	.rxq_info_get               = iavf_dev_rxq_info_get,
	.txq_info_get               = iavf_dev_txq_info_get,
	.rx_queue_count             = iavf_dev_rxq_count,
	.rx_descriptor_status       = iavf_dev_rx_desc_status,
	.tx_descriptor_status       = iavf_dev_tx_desc_status,
	.mtu_set                    = iavf_dev_mtu_set,
	.rx_queue_intr_enable       = iavf_dev_rx_queue_intr_enable,
	.rx_queue_intr_disable      = iavf_dev_rx_queue_intr_disable,
};

static int
iavf_dev_configure(struct rte_eth_dev *dev)
{
	struct iavf_adapter *ad =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(ad);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;

	ad->rx_bulk_alloc_allowed = true;
#ifdef RTE_LIBRTE_IAVF_INC_VECTOR
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
			iavf_enable_vlan_strip(ad);
		else
			iavf_disable_vlan_strip(ad);
	}
	return 0;
}

static int
iavf_init_rss(struct iavf_adapter *adapter)
{
	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_eth_rss_conf *rss_conf;
	uint8_t i, j, nb_q;
	int ret;

	rss_conf = &adapter->eth_dev->data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = RTE_MIN(adapter->eth_dev->data->nb_rx_queues,
		       IAVF_MAX_NUM_QUEUES);

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF)) {
		PMD_DRV_LOG(DEBUG, "RSS is not supported");
		return -ENOTSUP;
	}
	if (adapter->eth_dev->data->dev_conf.rxmode.mq_mode != ETH_MQ_RX_RSS) {
		PMD_DRV_LOG(WARNING, "RSS is enabled by PF by default");
		/* set all lut items to default queue */
		for (i = 0; i < vf->vf_res->rss_lut_size; i++)
			vf->rss_lut[i] = 0;
		ret = iavf_configure_rss_lut(adapter);
		return ret;
	}

	/* In IAVF, RSS enablement is set by PF driver. It is not supported
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
	ret = iavf_configure_rss_lut(adapter);
	if (ret)
		return ret;
	ret = iavf_configure_rss_key(adapter);
	if (ret)
		return ret;

	return 0;
}

static int
iavf_init_rxq(struct rte_eth_dev *dev, struct iavf_rx_queue *rxq)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_eth_dev_data *dev_data = dev->data;
	uint16_t buf_size, max_pkt_len, len;

	buf_size = rte_pktmbuf_data_room_size(rxq->mp) - RTE_PKTMBUF_HEADROOM;

	/* Calculate the maximum packet length allowed */
	len = rxq->rx_buf_len * IAVF_MAX_CHAINED_RX_BUFFERS;
	max_pkt_len = RTE_MIN(len, dev->data->dev_conf.rxmode.max_rx_pkt_len);

	/* Check if the jumbo frame and maximum packet length are set
	 * correctly.
	 */
	if (dev->data->dev_conf.rxmode.offloads & DEV_RX_OFFLOAD_JUMBO_FRAME) {
		if (max_pkt_len <= ETHER_MAX_LEN ||
		    max_pkt_len > IAVF_FRAME_SIZE_MAX) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
				    "larger than %u and smaller than %u, "
				    "as jumbo frame is enabled",
				    (uint32_t)ETHER_MAX_LEN,
				    (uint32_t)IAVF_FRAME_SIZE_MAX);
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
	    (rxq->max_pkt_len + 2 * IAVF_VLAN_TAG_SIZE) > buf_size) {
		dev_data->scattered_rx = 1;
	}
	IAVF_PCI_REG_WRITE(rxq->qrx_tail, rxq->nb_rx_desc - 1);
	IAVF_WRITE_FLUSH(hw);

	return 0;
}

static int
iavf_init_queues(struct rte_eth_dev *dev)
{
	struct iavf_rx_queue **rxq =
		(struct iavf_rx_queue **)dev->data->rx_queues;
	int i, ret = IAVF_SUCCESS;

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (!rxq[i] || !rxq[i]->q_set)
			continue;
		ret = iavf_init_rxq(dev, rxq[i]);
		if (ret != IAVF_SUCCESS)
			break;
	}
	/* set rx/tx function to vector/scatter/single-segment
	 * according to parameters
	 */
	iavf_set_rx_function(dev);
	iavf_set_tx_function(dev);

	return ret;
}

static int iavf_config_rx_queues_irqs(struct rte_eth_dev *dev,
				     struct rte_intr_handle *intr_handle)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
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
			vf->msix_base = IAVF_RX_VEC_START;
			IAVF_WRITE_REG(hw, IAVFINT_DYN_CTLN1(vf->msix_base - 1),
				      IAVFINT_DYN_CTLN1_ITR_INDX_MASK |
				      IAVFINT_DYN_CTLN1_WB_ON_ITR_MASK);
		} else {
			/* If no WB_ON_ITR offload flags, need to set
			 * interrupt for descriptor write back.
			 */
			vf->msix_base = IAVF_MISC_VEC_ID;

			/* set ITR to max */
			interval = iavf_calc_itr_interval(
					IAVF_QUEUE_ITR_INTERVAL_MAX);
			IAVF_WRITE_REG(hw, IAVFINT_DYN_CTL01,
				      IAVFINT_DYN_CTL01_INTENA_MASK |
				      (IAVF_ITR_INDEX_DEFAULT <<
				       IAVFINT_DYN_CTL01_ITR_INDX_SHIFT) |
				      (interval <<
				       IAVFINT_DYN_CTL01_INTERVAL_SHIFT));
		}
		IAVF_WRITE_FLUSH(hw);
		/* map all queues to the same interrupt */
		for (i = 0; i < dev->data->nb_rx_queues; i++)
			vf->rxq_map[vf->msix_base] |= 1 << i;
	} else {
		if (!rte_intr_allow_others(intr_handle)) {
			vf->nb_msix = 1;
			vf->msix_base = IAVF_MISC_VEC_ID;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				vf->rxq_map[vf->msix_base] |= 1 << i;
				intr_handle->intr_vec[i] = IAVF_MISC_VEC_ID;
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
			vf->msix_base = IAVF_RX_VEC_START;
			vec = IAVF_RX_VEC_START;
			for (i = 0; i < dev->data->nb_rx_queues; i++) {
				vf->rxq_map[vec] |= 1 << i;
				intr_handle->intr_vec[i] = vec++;
				if (vec >= vf->nb_msix)
					vec = IAVF_RX_VEC_START;
			}
			PMD_DRV_LOG(DEBUG,
				    "%u vectors are mapping to %u Rx queues",
				    vf->nb_msix, dev->data->nb_rx_queues);
		}
	}

	if (iavf_config_irq_map(adapter)) {
		PMD_DRV_LOG(ERR, "config interrupt mapping failed");
		return -1;
	}
	return 0;
}

static int
iavf_start_queues(struct rte_eth_dev *dev)
{
	struct iavf_rx_queue *rxq;
	struct iavf_tx_queue *txq;
	int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq->tx_deferred_start)
			continue;
		if (iavf_dev_tx_queue_start(dev, i) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start queue %u", i);
			return -1;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq->rx_deferred_start)
			continue;
		if (iavf_dev_rx_queue_start(dev, i) != 0) {
			PMD_DRV_LOG(ERR, "Fail to start queue %u", i);
			return -1;
		}
	}

	return 0;
}

static int
iavf_dev_start(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_intr_handle *intr_handle = dev->intr_handle;

	PMD_INIT_FUNC_TRACE();

	hw->adapter_stopped = 0;

	vf->max_pkt_len = dev->data->dev_conf.rxmode.max_rx_pkt_len;
	vf->num_queue_pairs = RTE_MAX(dev->data->nb_rx_queues,
				      dev->data->nb_tx_queues);

	if (iavf_init_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "failed to do Queue init");
		return -1;
	}

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF) {
		if (iavf_init_rss(adapter) != 0) {
			PMD_DRV_LOG(ERR, "configure rss failed");
			goto err_rss;
		}
	}

	if (iavf_configure_queues(adapter) != 0) {
		PMD_DRV_LOG(ERR, "configure queues failed");
		goto err_queue;
	}

	if (iavf_config_rx_queues_irqs(dev, intr_handle) != 0) {
		PMD_DRV_LOG(ERR, "configure irq failed");
		goto err_queue;
	}
	/* re-enable intr again, because efd assign may change */
	if (dev->data->dev_conf.intr_conf.rxq != 0) {
		rte_intr_disable(intr_handle);
		rte_intr_enable(intr_handle);
	}

	/* Set all mac addrs */
	iavf_add_del_all_mac_addr(adapter, TRUE);

	if (iavf_start_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "enable queues failed");
		goto err_mac;
	}

	return 0;

err_mac:
	iavf_add_del_all_mac_addr(adapter, FALSE);
err_queue:
err_rss:
	return -1;
}

static void
iavf_dev_stop(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_intr_handle *intr_handle = dev->intr_handle;

	PMD_INIT_FUNC_TRACE();

	if (hw->adapter_stopped == 1)
		return;

	iavf_stop_queues(dev);

	/* Disable the interrupt for Rx */
	rte_intr_efd_disable(intr_handle);
	/* Rx interrupt vector mapping free */
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}

	/* remove all mac addrs */
	iavf_add_del_all_mac_addr(adapter, FALSE);
	hw->adapter_stopped = 1;
}

static void
iavf_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	dev_info->max_rx_queues = vf->vsi_res->num_queue_pairs;
	dev_info->max_tx_queues = vf->vsi_res->num_queue_pairs;
	dev_info->min_rx_bufsize = IAVF_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = IAVF_FRAME_SIZE_MAX;
	dev_info->hash_key_size = vf->vf_res->rss_key_size;
	dev_info->reta_size = vf->vf_res->rss_lut_size;
	dev_info->flow_type_rss_offloads = IAVF_RSS_OFFLOAD_ALL;
	dev_info->max_mac_addrs = IAVF_NUM_MACADDR_MAX;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_QINQ_STRIP |
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
		DEV_RX_OFFLOAD_SCATTER |
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_VLAN_FILTER;
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
		DEV_TX_OFFLOAD_MULTI_SEGS;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = IAVF_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = IAVF_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = IAVF_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = IAVF_MAX_RING_DESC,
		.nb_min = IAVF_MIN_RING_DESC,
		.nb_align = IAVF_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = IAVF_MAX_RING_DESC,
		.nb_min = IAVF_MIN_RING_DESC,
		.nb_align = IAVF_ALIGN_RING_DESC,
	};
}

static const uint32_t *
iavf_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
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
iavf_dev_link_update(struct rte_eth_dev *dev,
		    __rte_unused int wait_to_complete)
{
	struct rte_eth_link new_link;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	/* Only read status info stored in VF, and the info is updated
	 *  when receive LINK_CHANGE evnet from PF by Virtchnnl.
	 */
	switch (vf->link_speed) {
	case 10:
		new_link.link_speed = ETH_SPEED_NUM_10M;
		break;
	case 100:
		new_link.link_speed = ETH_SPEED_NUM_100M;
		break;
	case 1000:
		new_link.link_speed = ETH_SPEED_NUM_1G;
		break;
	case 10000:
		new_link.link_speed = ETH_SPEED_NUM_10G;
		break;
	case 20000:
		new_link.link_speed = ETH_SPEED_NUM_20G;
		break;
	case 25000:
		new_link.link_speed = ETH_SPEED_NUM_25G;
		break;
	case 40000:
		new_link.link_speed = ETH_SPEED_NUM_40G;
		break;
	case 50000:
		new_link.link_speed = ETH_SPEED_NUM_50G;
		break;
	case 100000:
		new_link.link_speed = ETH_SPEED_NUM_100G;
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
iavf_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (vf->promisc_unicast_enabled)
		return;

	ret = iavf_config_promisc(adapter, TRUE, vf->promisc_multicast_enabled);
	if (!ret)
		vf->promisc_unicast_enabled = TRUE;
}

static void
iavf_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (!vf->promisc_unicast_enabled)
		return;

	ret = iavf_config_promisc(adapter, FALSE, vf->promisc_multicast_enabled);
	if (!ret)
		vf->promisc_unicast_enabled = FALSE;
}

static void
iavf_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (vf->promisc_multicast_enabled)
		return;

	ret = iavf_config_promisc(adapter, vf->promisc_unicast_enabled, TRUE);
	if (!ret)
		vf->promisc_multicast_enabled = TRUE;
}

static void
iavf_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;

	if (!vf->promisc_multicast_enabled)
		return;

	ret = iavf_config_promisc(adapter, vf->promisc_unicast_enabled, FALSE);
	if (!ret)
		vf->promisc_multicast_enabled = FALSE;
}

static int
iavf_dev_add_mac_addr(struct rte_eth_dev *dev, struct ether_addr *addr,
		     __rte_unused uint32_t index,
		     __rte_unused uint32_t pool)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int err;

	if (is_zero_ether_addr(addr)) {
		PMD_DRV_LOG(ERR, "Invalid Ethernet Address");
		return -EINVAL;
	}

	err = iavf_add_del_eth_addr(adapter, addr, TRUE);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to add MAC address");
		return -EIO;
	}

	vf->mac_num++;

	return 0;
}

static void
iavf_dev_del_mac_addr(struct rte_eth_dev *dev, uint32_t index)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct ether_addr *addr;
	int err;

	addr = &dev->data->mac_addrs[index];

	err = iavf_add_del_eth_addr(adapter, addr, FALSE);
	if (err)
		PMD_DRV_LOG(ERR, "fail to delete MAC address");

	vf->mac_num--;
}

static int
iavf_dev_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int err;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN))
		return -ENOTSUP;

	err = iavf_add_del_vlan(adapter, vlan_id, on);
	if (err)
		return -EIO;
	return 0;
}

static int
iavf_dev_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	int err;

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_VLAN))
		return -ENOTSUP;

	/* Vlan stripping setting */
	if (mask & ETH_VLAN_STRIP_MASK) {
		/* Enable or disable VLAN stripping */
		if (dev_conf->rxmode.offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			err = iavf_enable_vlan_strip(adapter);
		else
			err = iavf_disable_vlan_strip(adapter);

		if (err)
			return -EIO;
	}
	return 0;
}

static int
iavf_dev_rss_reta_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_reta_entry64 *reta_conf,
			uint16_t reta_size)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
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
	ret = iavf_configure_rss_lut(adapter);
	if (ret) /* revert back */
		rte_memcpy(vf->rss_lut, lut, reta_size);
	rte_free(lut);

	return ret;
}

static int
iavf_dev_rss_reta_query(struct rte_eth_dev *dev,
		       struct rte_eth_rss_reta_entry64 *reta_conf,
		       uint16_t reta_size)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
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
iavf_dev_rss_hash_update(struct rte_eth_dev *dev,
			struct rte_eth_rss_conf *rss_conf)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

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

	return iavf_configure_rss_key(adapter);
}

static int
iavf_dev_rss_hash_conf_get(struct rte_eth_dev *dev,
			  struct rte_eth_rss_conf *rss_conf)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);

	if (!(vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_RSS_PF))
		return -ENOTSUP;

	 /* Just set it to default value now. */
	rss_conf->rss_hf = IAVF_RSS_OFFLOAD_ALL;

	if (!rss_conf->rss_key)
		return 0;

	rss_conf->rss_key_len = vf->vf_res->rss_key_size;
	rte_memcpy(rss_conf->rss_key, vf->rss_key, rss_conf->rss_key_len);

	return 0;
}

static int
iavf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	uint32_t frame_size = mtu + IAVF_ETH_OVERHEAD;
	int ret = 0;

	if (mtu < ETHER_MIN_MTU || frame_size > IAVF_FRAME_SIZE_MAX)
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
iavf_dev_set_default_mac_addr(struct rte_eth_dev *dev,
			     struct ether_addr *mac_addr)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct ether_addr *perm_addr, *old_addr;
	int ret;

	old_addr = (struct ether_addr *)hw->mac.addr;
	perm_addr = (struct ether_addr *)hw->mac.perm_addr;

	if (is_same_ether_addr(mac_addr, old_addr))
		return 0;

	/* If the MAC address is configured by host, skip the setting */
	if (is_valid_assigned_ether_addr(perm_addr))
		return -EPERM;

	ret = iavf_add_del_eth_addr(adapter, old_addr, FALSE);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to delete old MAC:"
			    " %02X:%02X:%02X:%02X:%02X:%02X",
			    old_addr->addr_bytes[0],
			    old_addr->addr_bytes[1],
			    old_addr->addr_bytes[2],
			    old_addr->addr_bytes[3],
			    old_addr->addr_bytes[4],
			    old_addr->addr_bytes[5]);

	ret = iavf_add_del_eth_addr(adapter, mac_addr, TRUE);
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

static void
iavf_stat_update_48(uint64_t *offset, uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = *stat - *offset;
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << IAVF_48_BIT_WIDTH)) - *offset);

	*stat &= IAVF_48_BIT_MASK;
}

static void
iavf_stat_update_32(uint64_t *offset, uint64_t *stat)
{
	if (*stat >= *offset)
		*stat = (uint64_t)(*stat - *offset);
	else
		*stat = (uint64_t)((*stat +
			((uint64_t)1 << IAVF_32_BIT_WIDTH)) - *offset);
}

static void
iavf_update_stats(struct iavf_vsi *vsi, struct virtchnl_eth_stats *nes)
{
	struct virtchnl_eth_stats *oes = &vsi->eth_stats_offset;

	iavf_stat_update_48(&oes->rx_bytes, &nes->rx_bytes);
	iavf_stat_update_48(&oes->rx_unicast, &nes->rx_unicast);
	iavf_stat_update_48(&oes->rx_multicast, &nes->rx_multicast);
	iavf_stat_update_48(&oes->rx_broadcast, &nes->rx_broadcast);
	iavf_stat_update_32(&oes->rx_discards, &nes->rx_discards);
	iavf_stat_update_48(&oes->tx_bytes, &nes->tx_bytes);
	iavf_stat_update_48(&oes->tx_unicast, &nes->tx_unicast);
	iavf_stat_update_48(&oes->tx_multicast, &nes->tx_multicast);
	iavf_stat_update_48(&oes->tx_broadcast, &nes->tx_broadcast);
	iavf_stat_update_32(&oes->tx_errors, &nes->tx_errors);
	iavf_stat_update_32(&oes->tx_discards, &nes->tx_discards);
}

static int
iavf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_vsi *vsi = &vf->vsi;
	struct virtchnl_eth_stats *pstats = NULL;
	int ret;

	ret = iavf_query_stats(adapter, &pstats);
	if (ret == 0) {
		iavf_update_stats(vsi, pstats);
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

static void
iavf_dev_stats_reset(struct rte_eth_dev *dev)
{
	int ret;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_vsi *vsi = &vf->vsi;
	struct virtchnl_eth_stats *pstats = NULL;

	/* read stat values to clear hardware registers */
	ret = iavf_query_stats(adapter, &pstats);

	/* set stats offset base on current values */
	if (ret == 0)
		vsi->eth_stats_offset = *pstats;
}

static int
iavf_dev_rx_queue_intr_enable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	uint16_t msix_intr;

	msix_intr = pci_dev->intr_handle.intr_vec[queue_id];
	if (msix_intr == IAVF_MISC_VEC_ID) {
		PMD_DRV_LOG(INFO, "MISC is also enabled for control");
		IAVF_WRITE_REG(hw, IAVFINT_DYN_CTL01,
			      IAVFINT_DYN_CTL01_INTENA_MASK |
			      IAVFINT_DYN_CTL01_CLEARPBA_MASK |
			      IAVFINT_DYN_CTL01_ITR_INDX_MASK);
	} else {
		IAVF_WRITE_REG(hw,
			      IAVFINT_DYN_CTLN1(msix_intr - IAVF_RX_VEC_START),
			      IAVFINT_DYN_CTLN1_INTENA_MASK |
			      IAVFINT_DYN_CTL01_CLEARPBA_MASK |
			      IAVFINT_DYN_CTLN1_ITR_INDX_MASK);
	}

	IAVF_WRITE_FLUSH(hw);

	rte_intr_enable(&pci_dev->intr_handle);

	return 0;
}

static int
iavf_dev_rx_queue_intr_disable(struct rte_eth_dev *dev, uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t msix_intr;

	msix_intr = pci_dev->intr_handle.intr_vec[queue_id];
	if (msix_intr == IAVF_MISC_VEC_ID) {
		PMD_DRV_LOG(ERR, "MISC is used for control, cannot disable it");
		return -EIO;
	}

	IAVF_WRITE_REG(hw,
		      IAVFINT_DYN_CTLN1(msix_intr - IAVF_RX_VEC_START),
		      0);

	IAVF_WRITE_FLUSH(hw);
	return 0;
}

static int
iavf_check_vf_reset_done(struct iavf_hw *hw)
{
	int i, reset;

	for (i = 0; i < IAVF_RESET_WAIT_CNT; i++) {
		reset = IAVF_READ_REG(hw, IAVFGEN_RSTAT) &
			IAVFGEN_RSTAT_VFR_STATE_MASK;
		reset = reset >> IAVFGEN_RSTAT_VFR_STATE_SHIFT;
		if (reset == VIRTCHNL_VFR_VFACTIVE ||
		    reset == VIRTCHNL_VFR_COMPLETED)
			break;
		rte_delay_ms(20);
	}

	if (i >= IAVF_RESET_WAIT_CNT)
		return -1;

	return 0;
}

static int
iavf_init_vf(struct rte_eth_dev *dev)
{
	int err, bufsz;
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	err = iavf_set_mac_type(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "set_mac_type failed: %d", err);
		goto err;
	}

	err = iavf_check_vf_reset_done(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "VF is still resetting");
		goto err;
	}

	iavf_init_adminq_parameter(hw);
	err = iavf_init_adminq(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "init_adminq failed: %d", err);
		goto err;
	}

	vf->aq_resp = rte_zmalloc("vf_aq_resp", IAVF_AQ_BUF_SZ, 0);
	if (!vf->aq_resp) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_aq_resp memory");
		goto err_aq;
	}
	if (iavf_check_api_version(adapter) != 0) {
		PMD_INIT_LOG(ERR, "check_api version failed");
		goto err_api;
	}

	bufsz = sizeof(struct virtchnl_vf_resource) +
		(IAVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource));
	vf->vf_res = rte_zmalloc("vf_res", bufsz, 0);
	if (!vf->vf_res) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_res memory");
		goto err_api;
	}
	if (iavf_get_vf_resource(adapter) != 0) {
		PMD_INIT_LOG(ERR, "iavf_get_vf_config failed");
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
	iavf_shutdown_adminq(hw);
err:
	return -1;
}

/* Enable default admin queue interrupt setting */
static inline void
iavf_enable_irq0(struct iavf_hw *hw)
{
	/* Enable admin queue interrupt trigger */
	IAVF_WRITE_REG(hw, IAVFINT_ICR0_ENA1, IAVFINT_ICR0_ENA1_ADMINQ_MASK);

	IAVF_WRITE_REG(hw, IAVFINT_DYN_CTL01, IAVFINT_DYN_CTL01_INTENA_MASK |
		IAVFINT_DYN_CTL01_CLEARPBA_MASK | IAVFINT_DYN_CTL01_ITR_INDX_MASK);

	IAVF_WRITE_FLUSH(hw);
}

static inline void
iavf_disable_irq0(struct iavf_hw *hw)
{
	/* Disable all interrupt types */
	IAVF_WRITE_REG(hw, IAVFINT_ICR0_ENA1, 0);
	IAVF_WRITE_REG(hw, IAVFINT_DYN_CTL01,
		      IAVFINT_DYN_CTL01_ITR_INDX_MASK);
	IAVF_WRITE_FLUSH(hw);
}

static void
iavf_dev_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	iavf_disable_irq0(hw);

	iavf_handle_virtchnl_msg(dev);

	iavf_enable_irq0(hw);
}

static int
iavf_dev_init(struct rte_eth_dev *eth_dev)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);

	PMD_INIT_FUNC_TRACE();

	/* assign ops func pointer */
	eth_dev->dev_ops = &iavf_eth_dev_ops;
	eth_dev->rx_pkt_burst = &iavf_recv_pkts;
	eth_dev->tx_pkt_burst = &iavf_xmit_pkts;
	eth_dev->tx_pkt_prepare = &iavf_prep_pkts;

	/* For secondary processes, we don't initialise any further as primary
	 * has already done this work. Only check if we need a different RX
	 * and TX function.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY) {
		iavf_set_rx_function(eth_dev);
		iavf_set_tx_function(eth_dev);
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
	hw->back = IAVF_DEV_PRIVATE_TO_ADAPTER(eth_dev->data->dev_private);
	adapter->eth_dev = eth_dev;

	if (iavf_init_vf(eth_dev) != 0) {
		PMD_INIT_LOG(ERR, "Init vf failed");
		return -1;
	}

	/* copy mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc(
					"iavf_mac",
					ETHER_ADDR_LEN * IAVF_NUM_MACADDR_MAX,
					0);
	if (!eth_dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes needed to"
			     " store MAC addresses",
			     ETHER_ADDR_LEN * IAVF_NUM_MACADDR_MAX);
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
				   iavf_dev_interrupt_handler,
				   (void *)eth_dev);

	/* enable uio intr after callback register */
	rte_intr_enable(&pci_dev->intr_handle);

	/* configure and enable device interrupt */
	iavf_enable_irq0(hw);

	return 0;
}

static void
iavf_dev_close(struct rte_eth_dev *dev)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	iavf_dev_stop(dev);
	iavf_shutdown_adminq(hw);
	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* unregister callback func from eal lib */
	rte_intr_callback_unregister(intr_handle,
				     iavf_dev_interrupt_handler, dev);
	iavf_disable_irq0(hw);
}

static int
iavf_dev_uninit(struct rte_eth_dev *dev)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -EPERM;

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;
	if (hw->adapter_stopped == 0)
		iavf_dev_close(dev);

	rte_free(vf->vf_res);
	vf->vsi_res = NULL;
	vf->vf_res = NULL;

	rte_free(vf->aq_resp);
	vf->aq_resp = NULL;

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

static int eth_iavf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
			     struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
		sizeof(struct iavf_adapter), iavf_dev_init);
}

static int eth_iavf_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, iavf_dev_uninit);
}

/* Adaptive virtual function driver struct */
static struct rte_pci_driver rte_iavf_pmd = {
	.id_table = pci_id_iavf_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_IOVA_AS_VA,
	.probe = eth_iavf_pci_probe,
	.remove = eth_iavf_pci_remove,
};

RTE_PMD_REGISTER_PCI(net_iavf, rte_iavf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_iavf, pci_id_iavf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_iavf, "* igb_uio | vfio-pci");
RTE_INIT(iavf_init_log)
{
	iavf_logtype_init = rte_log_register("pmd.net.iavf.init");
	if (iavf_logtype_init >= 0)
		rte_log_set_level(iavf_logtype_init, RTE_LOG_NOTICE);
	iavf_logtype_driver = rte_log_register("pmd.net.iavf.driver");
	if (iavf_logtype_driver >= 0)
		rte_log_set_level(iavf_logtype_driver, RTE_LOG_NOTICE);
}

/* memory func for base code */
enum iavf_status_code
iavf_allocate_dma_mem_d(__rte_unused struct iavf_hw *hw,
		       struct iavf_dma_mem *mem,
		       u64 size,
		       u32 alignment)
{
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];

	if (!mem)
		return IAVF_ERR_PARAM;

	snprintf(z_name, sizeof(z_name), "iavf_dma_%"PRIu64, rte_rand());
	mz = rte_memzone_reserve_bounded(z_name, size, SOCKET_ID_ANY,
			RTE_MEMZONE_IOVA_CONTIG, alignment, RTE_PGSIZE_2M);
	if (!mz)
		return IAVF_ERR_NO_MEMORY;

	mem->size = size;
	mem->va = mz->addr;
	mem->pa = mz->phys_addr;
	mem->zone = (const void *)mz;
	PMD_DRV_LOG(DEBUG,
		    "memzone %s allocated with physical address: %"PRIu64,
		    mz->name, mem->pa);

	return IAVF_SUCCESS;
}

enum iavf_status_code
iavf_free_dma_mem_d(__rte_unused struct iavf_hw *hw,
		   struct iavf_dma_mem *mem)
{
	if (!mem)
		return IAVF_ERR_PARAM;

	PMD_DRV_LOG(DEBUG,
		    "memzone %s to be freed with physical address: %"PRIu64,
		    ((const struct rte_memzone *)mem->zone)->name, mem->pa);
	rte_memzone_free((const struct rte_memzone *)mem->zone);
	mem->zone = NULL;
	mem->va = NULL;
	mem->pa = (u64)0;

	return IAVF_SUCCESS;
}

enum iavf_status_code
iavf_allocate_virt_mem_d(__rte_unused struct iavf_hw *hw,
			struct iavf_virt_mem *mem,
			u32 size)
{
	if (!mem)
		return IAVF_ERR_PARAM;

	mem->size = size;
	mem->va = rte_zmalloc("iavf", size, 0);

	if (mem->va)
		return IAVF_SUCCESS;
	else
		return IAVF_ERR_NO_MEMORY;
}

enum iavf_status_code
iavf_free_virt_mem_d(__rte_unused struct iavf_hw *hw,
		    struct iavf_virt_mem *mem)
{
	if (!mem)
		return IAVF_ERR_PARAM;

	rte_free(mem->va);
	mem->va = NULL;

	return IAVF_SUCCESS;
}
