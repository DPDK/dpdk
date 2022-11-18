/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <rte_malloc.h>
#include <rte_memzone.h>
#include <rte_dev.h>
#include <errno.h>

#include "idpf_ethdev.h"
#include "idpf_rxtx.h"

#define IDPF_TX_SINGLE_Q	"tx_single"
#define IDPF_RX_SINGLE_Q	"rx_single"
#define IDPF_VPORT		"vport"

rte_spinlock_t idpf_adapter_lock;
/* A list for all adapters, one adapter matches one PCI device */
struct idpf_adapter_list idpf_adapter_list;
bool idpf_adapter_list_init;

uint64_t idpf_timestamp_dynflag;

static const char * const idpf_valid_args[] = {
	IDPF_TX_SINGLE_Q,
	IDPF_RX_SINGLE_Q,
	IDPF_VPORT,
	NULL
};

static int
idpf_dev_link_update(struct rte_eth_dev *dev,
		     __rte_unused int wait_to_complete)
{
	struct rte_eth_link new_link;

	memset(&new_link, 0, sizeof(new_link));

	new_link.link_speed = RTE_ETH_SPEED_NUM_NONE;
	new_link.link_duplex = RTE_ETH_LINK_FULL_DUPLEX;
	new_link.link_autoneg = !(dev->data->dev_conf.link_speeds &
				  RTE_ETH_LINK_SPEED_FIXED);

	return rte_eth_linkstatus_set(dev, &new_link);
}

static int
idpf_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;

	dev_info->max_rx_queues = adapter->caps->max_rx_q;
	dev_info->max_tx_queues = adapter->caps->max_tx_q;
	dev_info->min_rx_bufsize = IDPF_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = IDPF_MAX_FRAME_SIZE;

	dev_info->max_mtu = dev_info->max_rx_pktlen - IDPF_ETH_OVERHEAD;
	dev_info->min_mtu = RTE_ETHER_MIN_MTU;

	dev_info->flow_type_rss_offloads = IDPF_RSS_OFFLOAD_ALL;

	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM           |
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM            |
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM            |
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM     |
		RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM		|
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_TCP_TSO		|
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS		|
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = IDPF_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = IDPF_DEFAULT_TX_RS_THRESH,
	};

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = IDPF_DEFAULT_RX_FREE_THRESH,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = IDPF_MAX_RING_DESC,
		.nb_min = IDPF_MIN_RING_DESC,
		.nb_align = IDPF_ALIGN_RING_DESC,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = IDPF_MAX_RING_DESC,
		.nb_min = IDPF_MIN_RING_DESC,
		.nb_align = IDPF_ALIGN_RING_DESC,
	};

	return 0;
}

static int
idpf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu __rte_unused)
{
	/* mtu setting is forbidden if port is start */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port must be stopped before configuration");
		return -EBUSY;
	}

	return 0;
}

static const uint32_t *
idpf_dev_supported_ptypes_get(struct rte_eth_dev *dev __rte_unused)
{
	static const uint32_t ptypes[] = {
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4_EXT_UNKNOWN,
		RTE_PTYPE_L3_IPV6_EXT_UNKNOWN,
		RTE_PTYPE_L4_FRAG,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_UNKNOWN
	};

	return ptypes;
}

static int
idpf_init_vport_req_info(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_create_vport *vport_info;
	uint16_t idx = adapter->cur_vport_idx;

	if (idx == IDPF_INVALID_VPORT_IDX) {
		PMD_INIT_LOG(ERR, "Invalid vport index.");
		return -EINVAL;
	}

	if (adapter->vport_req_info[idx] == NULL) {
		adapter->vport_req_info[idx] = rte_zmalloc(NULL,
				sizeof(struct virtchnl2_create_vport), 0);
		if (adapter->vport_req_info[idx] == NULL) {
			PMD_INIT_LOG(ERR, "Failed to allocate vport_req_info");
			return -ENOMEM;
		}
	}

	vport_info =
		(struct virtchnl2_create_vport *)adapter->vport_req_info[idx];

	vport_info->vport_type = rte_cpu_to_le_16(VIRTCHNL2_VPORT_TYPE_DEFAULT);
	if (adapter->txq_model == 0) {
		vport_info->txq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SPLIT);
		vport_info->num_tx_q = IDPF_DEFAULT_TXQ_NUM;
		vport_info->num_tx_complq =
			IDPF_DEFAULT_TXQ_NUM * IDPF_TX_COMPLQ_PER_GRP;
	} else {
		vport_info->txq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SINGLE);
		vport_info->num_tx_q = IDPF_DEFAULT_TXQ_NUM;
		vport_info->num_tx_complq = 0;
	}
	if (adapter->rxq_model == 0) {
		vport_info->rxq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SPLIT);
		vport_info->num_rx_q = IDPF_DEFAULT_RXQ_NUM;
		vport_info->num_rx_bufq =
			IDPF_DEFAULT_RXQ_NUM * IDPF_RX_BUFQ_PER_GRP;
	} else {
		vport_info->rxq_model =
			rte_cpu_to_le_16(VIRTCHNL2_QUEUE_MODEL_SINGLE);
		vport_info->num_rx_q = IDPF_DEFAULT_RXQ_NUM;
		vport_info->num_rx_bufq = 0;
	}

	return 0;
}

static int
idpf_parse_devarg_id(char *name)
{
	uint16_t val;
	char *p;

	p = strstr(name, "vport_");

	if (p == NULL)
		return -EINVAL;

	p += sizeof("vport_") - 1;

	val = strtoul(p, NULL, 10);

	return val;
}

#define IDPF_RSS_KEY_LEN 52

static int
idpf_init_vport(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	uint16_t idx = adapter->cur_vport_idx;
	struct virtchnl2_create_vport *vport_info =
		(struct virtchnl2_create_vport *)adapter->vport_recv_info[idx];
	int i, type, ret;

	vport->vport_id = vport_info->vport_id;
	vport->txq_model = vport_info->txq_model;
	vport->rxq_model = vport_info->rxq_model;
	vport->num_tx_q = vport_info->num_tx_q;
	vport->num_tx_complq = vport_info->num_tx_complq;
	vport->num_rx_q = vport_info->num_rx_q;
	vport->num_rx_bufq = vport_info->num_rx_bufq;
	vport->max_mtu = vport_info->max_mtu;
	rte_memcpy(vport->default_mac_addr,
		   vport_info->default_mac_addr, ETH_ALEN);
	vport->rss_algorithm = vport_info->rss_algorithm;
	vport->rss_key_size = RTE_MIN(IDPF_RSS_KEY_LEN,
				     vport_info->rss_key_size);
	vport->rss_lut_size = vport_info->rss_lut_size;
	vport->sw_idx = idx;

	for (i = 0; i < vport_info->chunks.num_chunks; i++) {
		type = vport_info->chunks.chunks[i].type;
		switch (type) {
		case VIRTCHNL2_QUEUE_TYPE_TX:
			vport->chunks_info.tx_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.tx_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.tx_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_RX:
			vport->chunks_info.rx_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.rx_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.rx_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_TX_COMPLETION:
			vport->chunks_info.tx_compl_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.tx_compl_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.tx_compl_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		case VIRTCHNL2_QUEUE_TYPE_RX_BUFFER:
			vport->chunks_info.rx_buf_start_qid =
				vport_info->chunks.chunks[i].start_queue_id;
			vport->chunks_info.rx_buf_qtail_start =
				vport_info->chunks.chunks[i].qtail_reg_start;
			vport->chunks_info.rx_buf_qtail_spacing =
				vport_info->chunks.chunks[i].qtail_reg_spacing;
			break;
		default:
			PMD_INIT_LOG(ERR, "Unsupported queue type");
			break;
		}
	}

	ret = idpf_parse_devarg_id(dev->data->name);
	if (ret < 0) {
		PMD_INIT_LOG(ERR, "Failed to parse devarg id.");
		return -EINVAL;
	}
	vport->devarg_id = ret;

	vport->dev_data = dev->data;

	adapter->vports[idx] = vport;

	return 0;
}

static int
idpf_config_rss(struct idpf_vport *vport)
{
	int ret;

	ret = idpf_vc_set_rss_key(vport);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to configure RSS key");
		return ret;
	}

	ret = idpf_vc_set_rss_lut(vport);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to configure RSS lut");
		return ret;
	}

	ret = idpf_vc_set_rss_hash(vport);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to configure RSS hash");
		return ret;
	}

	return ret;
}

static int
idpf_init_rss(struct idpf_vport *vport)
{
	struct rte_eth_rss_conf *rss_conf;
	uint16_t i, nb_q, lut_size;
	int ret = 0;

	rss_conf = &vport->dev_data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = vport->dev_data->nb_rx_queues;

	vport->rss_key = rte_zmalloc("rss_key",
				     vport->rss_key_size, 0);
	if (vport->rss_key == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate RSS key");
		ret = -ENOMEM;
		goto err_alloc_key;
	}

	lut_size = vport->rss_lut_size;
	vport->rss_lut = rte_zmalloc("rss_lut",
				     sizeof(uint32_t) * lut_size, 0);
	if (vport->rss_lut == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate RSS lut");
		ret = -ENOMEM;
		goto err_alloc_lut;
	}

	if (rss_conf->rss_key == NULL) {
		for (i = 0; i < vport->rss_key_size; i++)
			vport->rss_key[i] = (uint8_t)rte_rand();
	} else if (rss_conf->rss_key_len != vport->rss_key_size) {
		PMD_INIT_LOG(ERR, "Invalid RSS key length in RSS configuration, should be %d",
			     vport->rss_key_size);
		ret = -EINVAL;
		goto err_cfg_key;
	} else {
		rte_memcpy(vport->rss_key, rss_conf->rss_key,
			   vport->rss_key_size);
	}

	for (i = 0; i < lut_size; i++)
		vport->rss_lut[i] = i % nb_q;

	vport->rss_hf = IDPF_DEFAULT_RSS_HASH_EXPANDED;

	ret = idpf_config_rss(vport);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to configure RSS");
		goto err_cfg_key;
	}

	return ret;

err_cfg_key:
	rte_free(vport->rss_lut);
	vport->rss_lut = NULL;
err_alloc_lut:
	rte_free(vport->rss_key);
	vport->rss_key = NULL;
err_alloc_key:
	return ret;
}

static int
idpf_dev_configure(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct rte_eth_conf *conf = &dev->data->dev_conf;
	struct idpf_adapter *adapter = vport->adapter;
	int ret;

	if (conf->link_speeds & RTE_ETH_LINK_SPEED_FIXED) {
		PMD_INIT_LOG(ERR, "Setting link speed is not supported");
		return -ENOTSUP;
	}

	if (conf->txmode.mq_mode != RTE_ETH_MQ_TX_NONE) {
		PMD_INIT_LOG(ERR, "Multi-queue TX mode %d is not supported",
			     conf->txmode.mq_mode);
		return -ENOTSUP;
	}

	if (conf->lpbk_mode != 0) {
		PMD_INIT_LOG(ERR, "Loopback operation mode %d is not supported",
			     conf->lpbk_mode);
		return -ENOTSUP;
	}

	if (conf->dcb_capability_en != 0) {
		PMD_INIT_LOG(ERR, "Priority Flow Control(PFC) if not supported");
		return -ENOTSUP;
	}

	if (conf->intr_conf.lsc != 0) {
		PMD_INIT_LOG(ERR, "LSC interrupt is not supported");
		return -ENOTSUP;
	}

	if (conf->intr_conf.rxq != 0) {
		PMD_INIT_LOG(ERR, "RXQ interrupt is not supported");
		return -ENOTSUP;
	}

	if (conf->intr_conf.rmv != 0) {
		PMD_INIT_LOG(ERR, "RMV interrupt is not supported");
		return -ENOTSUP;
	}

	if (adapter->caps->rss_caps != 0 && dev->data->nb_rx_queues != 0) {
		ret = idpf_init_rss(vport);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to init rss");
			return ret;
		}
	} else {
		PMD_INIT_LOG(ERR, "RSS is not supported.");
		return -1;
	}

	return 0;
}

static int
idpf_config_rx_queues_irqs(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	struct virtchnl2_queue_vector *qv_map;
	struct idpf_hw *hw = &adapter->hw;
	uint32_t dynctl_reg_start;
	uint32_t itrn_reg_start;
	uint32_t dynctl_val, itrn_val;
	uint16_t i;

	qv_map = rte_zmalloc("qv_map",
			dev->data->nb_rx_queues *
			sizeof(struct virtchnl2_queue_vector), 0);
	if (qv_map == NULL) {
		PMD_DRV_LOG(ERR, "Failed to allocate %d queue-vector map",
			    dev->data->nb_rx_queues);
		goto qv_map_alloc_err;
	}

	/* Rx interrupt disabled, Map interrupt only for writeback */

	/* The capability flags adapter->caps->other_caps should be
	 * compared with bit VIRTCHNL2_CAP_WB_ON_ITR here. The if
	 * condition should be updated when the FW can return the
	 * correct flag bits.
	 */
	dynctl_reg_start =
		vport->recv_vectors->vchunks.vchunks->dynctl_reg_start;
	itrn_reg_start =
		vport->recv_vectors->vchunks.vchunks->itrn_reg_start;
	dynctl_val = IDPF_READ_REG(hw, dynctl_reg_start);
	PMD_DRV_LOG(DEBUG, "Value of dynctl_reg_start is 0x%x",
		    dynctl_val);
	itrn_val = IDPF_READ_REG(hw, itrn_reg_start);
	PMD_DRV_LOG(DEBUG, "Value of itrn_reg_start is 0x%x", itrn_val);
	/* Force write-backs by setting WB_ON_ITR bit in DYN_CTL
	 * register. WB_ON_ITR and INTENA are mutually exclusive
	 * bits. Setting WB_ON_ITR bits means TX and RX Descs
	 * are written back based on ITR expiration irrespective
	 * of INTENA setting.
	 */
	/* TBD: need to tune INTERVAL value for better performance. */
	if (itrn_val != 0)
		IDPF_WRITE_REG(hw,
			       dynctl_reg_start,
			       VIRTCHNL2_ITR_IDX_0  <<
			       PF_GLINT_DYN_CTL_ITR_INDX_S |
			       PF_GLINT_DYN_CTL_WB_ON_ITR_M |
			       itrn_val <<
			       PF_GLINT_DYN_CTL_INTERVAL_S);
	else
		IDPF_WRITE_REG(hw,
			       dynctl_reg_start,
			       VIRTCHNL2_ITR_IDX_0  <<
			       PF_GLINT_DYN_CTL_ITR_INDX_S |
			       PF_GLINT_DYN_CTL_WB_ON_ITR_M |
			       IDPF_DFLT_INTERVAL <<
			       PF_GLINT_DYN_CTL_INTERVAL_S);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		/* map all queues to the same vector */
		qv_map[i].queue_id = vport->chunks_info.rx_start_qid + i;
		qv_map[i].vector_id =
			vport->recv_vectors->vchunks.vchunks->start_vector_id;
	}
	vport->qv_map = qv_map;

	if (idpf_vc_config_irq_map_unmap(vport, true) != 0) {
		PMD_DRV_LOG(ERR, "config interrupt mapping failed");
		goto config_irq_map_err;
	}

	return 0;

config_irq_map_err:
	rte_free(vport->qv_map);
	vport->qv_map = NULL;

qv_map_alloc_err:
	return -1;
}

static int
idpf_start_queues(struct rte_eth_dev *dev)
{
	struct idpf_rx_queue *rxq;
	struct idpf_tx_queue *txq;
	int err = 0;
	int i;

	for (i = 0; i < dev->data->nb_tx_queues; i++) {
		txq = dev->data->tx_queues[i];
		if (txq == NULL || txq->tx_deferred_start)
			continue;
		err = idpf_tx_queue_start(dev, i);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to start Tx queue %u", i);
			return err;
		}
	}

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		rxq = dev->data->rx_queues[i];
		if (rxq == NULL || rxq->rx_deferred_start)
			continue;
		err = idpf_rx_queue_start(dev, i);
		if (err != 0) {
			PMD_DRV_LOG(ERR, "Fail to start Rx queue %u", i);
			return err;
		}
	}

	return err;
}

static int
idpf_dev_start(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;
	uint16_t num_allocated_vectors =
		adapter->caps->num_allocated_vectors;
	uint16_t req_vecs_num;
	int ret;

	vport->stopped = 0;

	if (dev->data->mtu > vport->max_mtu) {
		PMD_DRV_LOG(ERR, "MTU should be less than %d", vport->max_mtu);
		ret = -EINVAL;
		goto err_mtu;
	}

	vport->max_pkt_len = dev->data->mtu + IDPF_ETH_OVERHEAD;

	req_vecs_num = IDPF_DFLT_Q_VEC_NUM;
	if (req_vecs_num + adapter->used_vecs_num > num_allocated_vectors) {
		PMD_DRV_LOG(ERR, "The accumulated request vectors' number should be less than %d",
			    num_allocated_vectors);
		ret = -EINVAL;
		goto err_mtu;
	}

	ret = idpf_vc_alloc_vectors(vport, req_vecs_num);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to allocate interrupt vectors");
		goto err_mtu;
	}
	adapter->used_vecs_num += req_vecs_num;

	ret = idpf_config_rx_queues_irqs(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to configure irqs");
		goto err_mtu;
	}

	ret = idpf_start_queues(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to start queues");
		goto err_mtu;
	}

	idpf_set_rx_function(dev);
	idpf_set_tx_function(dev);

	ret = idpf_vc_ena_dis_vport(vport, true);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to enable vport");
		goto err_vport;
	}

	return 0;

err_vport:
	idpf_stop_queues(dev);
err_mtu:
	return ret;
}

static int
idpf_dev_stop(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;

	if (vport->stopped == 1)
		return 0;

	idpf_vc_ena_dis_vport(vport, false);

	idpf_stop_queues(dev);

	idpf_vc_config_irq_map_unmap(vport, false);

	if (vport->recv_vectors != NULL)
		idpf_vc_dealloc_vectors(vport);

	vport->stopped = 1;

	return 0;
}

static int
idpf_dev_close(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = vport->adapter;

	idpf_dev_stop(dev);

	idpf_vc_destroy_vport(vport);

	rte_free(vport->rss_lut);
	vport->rss_lut = NULL;

	rte_free(vport->rss_key);
	vport->rss_key = NULL;

	rte_free(vport->recv_vectors);
	vport->recv_vectors = NULL;

	rte_free(vport->qv_map);
	vport->qv_map = NULL;

	adapter->cur_vports &= ~RTE_BIT32(vport->devarg_id);

	rte_free(vport);
	dev->data->dev_private = NULL;

	return 0;
}

static int
insert_value(struct idpf_adapter *adapter, uint16_t id)
{
	uint16_t i;

	for (i = 0; i < adapter->req_vport_nb; i++) {
		if (adapter->req_vports[i] == id)
			return 0;
	}

	if (adapter->req_vport_nb >= RTE_DIM(adapter->req_vports)) {
		PMD_INIT_LOG(ERR, "Total vport number can't be > %d",
			     IDPF_MAX_VPORT_NUM);
		return -EINVAL;
	}

	adapter->req_vports[adapter->req_vport_nb] = id;
	adapter->req_vport_nb++;

	return 0;
}

static const char *
parse_range(const char *value, struct idpf_adapter *adapter)
{
	uint16_t lo, hi, i;
	int n = 0;
	int result;
	const char *pos = value;

	result = sscanf(value, "%hu%n-%hu%n", &lo, &n, &hi, &n);
	if (result == 1) {
		if (lo >= IDPF_MAX_VPORT_NUM)
			return NULL;
		if (insert_value(adapter, lo) != 0)
			return NULL;
	} else if (result == 2) {
		if (lo > hi || hi >= IDPF_MAX_VPORT_NUM)
			return NULL;
		for (i = lo; i <= hi; i++) {
			if (insert_value(adapter, i) != 0)
				return NULL;
		}
	} else {
		return NULL;
	}

	return pos + n;
}

static int
parse_vport(const char *key, const char *value, void *args)
{
	struct idpf_adapter *adapter = args;
	const char *pos = value;
	int i;

	adapter->req_vport_nb = 0;

	if (*pos == '[')
		pos++;

	while (1) {
		pos = parse_range(pos, adapter);
		if (pos == NULL) {
			PMD_INIT_LOG(ERR, "invalid value:\"%s\" for key:\"%s\", ",
				     value, key);
			return -EINVAL;
		}
		if (*pos != ',')
			break;
		pos++;
	}

	if (*value == '[' && *pos != ']') {
		PMD_INIT_LOG(ERR, "invalid value:\"%s\" for key:\"%s\", ",
			     value, key);
		return -EINVAL;
	}

	if (adapter->cur_vport_nb + adapter->req_vport_nb >
	    IDPF_MAX_VPORT_NUM) {
		PMD_INIT_LOG(ERR, "Total vport number can't be > %d",
			     IDPF_MAX_VPORT_NUM);
		return -EINVAL;
	}

	for (i = 0; i < adapter->req_vport_nb; i++) {
		if ((adapter->cur_vports & RTE_BIT32(adapter->req_vports[i])) == 0) {
			adapter->cur_vports |= RTE_BIT32(adapter->req_vports[i]);
			adapter->cur_vport_nb++;
		} else {
			PMD_INIT_LOG(ERR, "Vport %d has been created",
				     adapter->req_vports[i]);
			return -EINVAL;
		}
	}

	return 0;
}

static int
parse_bool(const char *key, const char *value, void *args)
{
	int *i = args;
	char *end;
	int num;

	errno = 0;

	num = strtoul(value, &end, 10);

	if (errno == ERANGE || (num != 0 && num != 1)) {
		PMD_INIT_LOG(ERR, "invalid value:\"%s\" for key:\"%s\", value must be 0 or 1",
			value, key);
		return -EINVAL;
	}

	*i = num;
	return 0;
}

static int
idpf_parse_devargs(struct rte_pci_device *pci_dev, struct idpf_adapter *adapter)
{
	struct rte_devargs *devargs = pci_dev->device.devargs;
	struct rte_kvargs *kvlist;
	int ret;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, idpf_valid_args);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "invalid kvargs key");
		return -EINVAL;
	}

	ret = rte_kvargs_process(kvlist, IDPF_VPORT, &parse_vport,
				 adapter);
	if (ret != 0)
		goto bail;

	ret = rte_kvargs_process(kvlist, IDPF_TX_SINGLE_Q, &parse_bool,
				 &adapter->txq_model);
	if (ret != 0)
		goto bail;

	ret = rte_kvargs_process(kvlist, IDPF_RX_SINGLE_Q, &parse_bool,
				 &adapter->rxq_model);
	if (ret != 0)
		goto bail;

bail:
	rte_kvargs_free(kvlist);
	return ret;
}

static void
idpf_reset_pf(struct idpf_hw *hw)
{
	uint32_t reg;

	reg = IDPF_READ_REG(hw, PFGEN_CTRL);
	IDPF_WRITE_REG(hw, PFGEN_CTRL, (reg | PFGEN_CTRL_PFSWR));
}

#define IDPF_RESET_WAIT_CNT 100
static int
idpf_check_pf_reset_done(struct idpf_hw *hw)
{
	uint32_t reg;
	int i;

	for (i = 0; i < IDPF_RESET_WAIT_CNT; i++) {
		reg = IDPF_READ_REG(hw, PFGEN_RSTAT);
		if (reg != 0xFFFFFFFF && (reg & PFGEN_RSTAT_PFR_STATE_M))
			return 0;
		rte_delay_ms(1000);
	}

	PMD_INIT_LOG(ERR, "IDPF reset timeout");
	return -EBUSY;
}

#define CTLQ_NUM 2
static int
idpf_init_mbx(struct idpf_hw *hw)
{
	struct idpf_ctlq_create_info ctlq_info[CTLQ_NUM] = {
		{
			.type = IDPF_CTLQ_TYPE_MAILBOX_TX,
			.id = IDPF_CTLQ_ID,
			.len = IDPF_CTLQ_LEN,
			.buf_size = IDPF_DFLT_MBX_BUF_SIZE,
			.reg = {
				.head = PF_FW_ATQH,
				.tail = PF_FW_ATQT,
				.len = PF_FW_ATQLEN,
				.bah = PF_FW_ATQBAH,
				.bal = PF_FW_ATQBAL,
				.len_mask = PF_FW_ATQLEN_ATQLEN_M,
				.len_ena_mask = PF_FW_ATQLEN_ATQENABLE_M,
				.head_mask = PF_FW_ATQH_ATQH_M,
			}
		},
		{
			.type = IDPF_CTLQ_TYPE_MAILBOX_RX,
			.id = IDPF_CTLQ_ID,
			.len = IDPF_CTLQ_LEN,
			.buf_size = IDPF_DFLT_MBX_BUF_SIZE,
			.reg = {
				.head = PF_FW_ARQH,
				.tail = PF_FW_ARQT,
				.len = PF_FW_ARQLEN,
				.bah = PF_FW_ARQBAH,
				.bal = PF_FW_ARQBAL,
				.len_mask = PF_FW_ARQLEN_ARQLEN_M,
				.len_ena_mask = PF_FW_ARQLEN_ARQENABLE_M,
				.head_mask = PF_FW_ARQH_ARQH_M,
			}
		}
	};
	struct idpf_ctlq_info *ctlq;
	int ret;

	ret = idpf_ctlq_init(hw, CTLQ_NUM, ctlq_info);
	if (ret != 0)
		return ret;

	LIST_FOR_EACH_ENTRY_SAFE(ctlq, NULL, &hw->cq_list_head,
				 struct idpf_ctlq_info, cq_list) {
		if (ctlq->q_id == IDPF_CTLQ_ID &&
		    ctlq->cq_type == IDPF_CTLQ_TYPE_MAILBOX_TX)
			hw->asq = ctlq;
		if (ctlq->q_id == IDPF_CTLQ_ID &&
		    ctlq->cq_type == IDPF_CTLQ_TYPE_MAILBOX_RX)
			hw->arq = ctlq;
	}

	if (hw->asq == NULL || hw->arq == NULL) {
		idpf_ctlq_deinit(hw);
		ret = -ENOENT;
	}

	return ret;
}

static int
idpf_adapter_init(struct rte_pci_device *pci_dev, struct idpf_adapter *adapter)
{
	struct idpf_hw *hw = &adapter->hw;
	int ret = 0;

	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->hw_addr_len = pci_dev->mem_resource[0].len;
	hw->back = adapter;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	strncpy(adapter->name, pci_dev->device.name, PCI_PRI_STR_SIZE);

	idpf_reset_pf(hw);
	ret = idpf_check_pf_reset_done(hw);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "IDPF is still resetting");
		goto err;
	}

	ret = idpf_init_mbx(hw);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init mailbox");
		goto err;
	}

	adapter->mbx_resp = rte_zmalloc("idpf_adapter_mbx_resp",
					IDPF_DFLT_MBX_BUF_SIZE, 0);
	if (adapter->mbx_resp == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate idpf_adapter_mbx_resp memory");
		ret = -ENOMEM;
		goto err_mbx;
	}

	ret = idpf_vc_check_api_version(adapter);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to check api version");
		goto err_api;
	}

	ret = idpf_get_pkt_type(adapter);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to set ptype table");
		goto err_api;
	}

	adapter->caps = rte_zmalloc("idpf_caps",
				sizeof(struct virtchnl2_get_capabilities), 0);
	if (adapter->caps == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate idpf_caps memory");
		ret = -ENOMEM;
		goto err_api;
	}

	ret = idpf_vc_get_caps(adapter);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to get capabilities");
		goto err_caps;
	}

	adapter->max_vport_nb = adapter->caps->max_vports;

	adapter->vport_req_info = rte_zmalloc("vport_req_info",
					      adapter->max_vport_nb *
					      sizeof(*adapter->vport_req_info),
					      0);
	if (adapter->vport_req_info == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate vport_req_info memory");
		ret = -ENOMEM;
		goto err_caps;
	}

	adapter->vport_recv_info = rte_zmalloc("vport_recv_info",
					       adapter->max_vport_nb *
					       sizeof(*adapter->vport_recv_info),
					       0);
	if (adapter->vport_recv_info == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate vport_recv_info memory");
		ret = -ENOMEM;
		goto err_vport_recv_info;
	}

	adapter->vports = rte_zmalloc("vports",
				      adapter->max_vport_nb *
				      sizeof(*adapter->vports),
				      0);
	if (adapter->vports == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate vports memory");
		ret = -ENOMEM;
		goto err_vports;
	}

	adapter->max_rxq_per_msg = (IDPF_DFLT_MBX_BUF_SIZE -
				sizeof(struct virtchnl2_config_rx_queues)) /
				sizeof(struct virtchnl2_rxq_info);
	adapter->max_txq_per_msg = (IDPF_DFLT_MBX_BUF_SIZE -
				sizeof(struct virtchnl2_config_tx_queues)) /
				sizeof(struct virtchnl2_txq_info);

	adapter->cur_vports = 0;
	adapter->cur_vport_nb = 0;

	adapter->used_vecs_num = 0;

	return ret;

err_vports:
	rte_free(adapter->vport_recv_info);
	adapter->vport_recv_info = NULL;
err_vport_recv_info:
	rte_free(adapter->vport_req_info);
	adapter->vport_req_info = NULL;
err_caps:
	rte_free(adapter->caps);
	adapter->caps = NULL;
err_api:
	rte_free(adapter->mbx_resp);
	adapter->mbx_resp = NULL;
err_mbx:
	idpf_ctlq_deinit(hw);
err:
	return ret;
}

static const struct eth_dev_ops idpf_eth_dev_ops = {
	.dev_configure			= idpf_dev_configure,
	.dev_close			= idpf_dev_close,
	.rx_queue_setup			= idpf_rx_queue_setup,
	.tx_queue_setup			= idpf_tx_queue_setup,
	.dev_infos_get			= idpf_dev_info_get,
	.dev_start			= idpf_dev_start,
	.dev_stop			= idpf_dev_stop,
	.link_update			= idpf_dev_link_update,
	.rx_queue_start			= idpf_rx_queue_start,
	.tx_queue_start			= idpf_tx_queue_start,
	.rx_queue_stop			= idpf_rx_queue_stop,
	.tx_queue_stop			= idpf_tx_queue_stop,
	.rx_queue_release		= idpf_dev_rx_queue_release,
	.tx_queue_release		= idpf_dev_tx_queue_release,
	.mtu_set			= idpf_dev_mtu_set,
	.dev_supported_ptypes_get	= idpf_dev_supported_ptypes_get,
};

static uint16_t
idpf_get_vport_idx(struct idpf_vport **vports, uint16_t max_vport_nb)
{
	uint16_t vport_idx;
	uint16_t i;

	for (i = 0; i < max_vport_nb; i++) {
		if (vports[i] == NULL)
			break;
	}

	if (i == max_vport_nb)
		vport_idx = IDPF_INVALID_VPORT_IDX;
	else
		vport_idx = i;

	return vport_idx;
}

static int
idpf_dev_init(struct rte_eth_dev *dev, void *init_params)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter *adapter = init_params;
	int ret = 0;

	dev->dev_ops = &idpf_eth_dev_ops;
	vport->adapter = adapter;

	ret = idpf_init_vport_req_info(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init vport req_info.");
		goto err;
	}

	ret = idpf_vc_create_vport(adapter);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to create vport.");
		goto err_create_vport;
	}

	ret = idpf_init_vport(dev);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init vports.");
		goto err_init_vport;
	}

	adapter->cur_vport_idx = idpf_get_vport_idx(adapter->vports,
						    adapter->max_vport_nb);

	dev->data->mac_addrs = rte_zmalloc(NULL, RTE_ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate mac_addr memory.");
		ret = -ENOMEM;
		goto err_init_vport;
	}

	rte_ether_addr_copy((struct rte_ether_addr *)vport->default_mac_addr,
			    &dev->data->mac_addrs[0]);

	return 0;

err_init_vport:
	idpf_vc_destroy_vport(vport);
err_create_vport:
	rte_free(vport->adapter->vport_req_info[vport->adapter->cur_vport_idx]);
err:
	return ret;
}

static const struct rte_pci_id pci_id_idpf_map[] = {
	{ RTE_PCI_DEVICE(IDPF_INTEL_VENDOR_ID, IDPF_DEV_ID_PF) },
	{ .vendor_id = 0, /* sentinel */ },
};

struct idpf_adapter *
idpf_find_adapter(struct rte_pci_device *pci_dev)
{
	struct idpf_adapter *adapter;
	int found = 0;

	if (pci_dev == NULL)
		return NULL;

	rte_spinlock_lock(&idpf_adapter_lock);
	TAILQ_FOREACH(adapter, &idpf_adapter_list, next) {
		if (strncmp(adapter->name, pci_dev->device.name, PCI_PRI_STR_SIZE) == 0) {
			found = 1;
			break;
		}
	}
	rte_spinlock_unlock(&idpf_adapter_lock);

	if (found == 0)
		return NULL;

	return adapter;
}

static void
idpf_adapter_rel(struct idpf_adapter *adapter)
{
	struct idpf_hw *hw = &adapter->hw;
	int i;

	idpf_ctlq_deinit(hw);

	rte_free(adapter->caps);
	adapter->caps = NULL;

	rte_free(adapter->mbx_resp);
	adapter->mbx_resp = NULL;

	if (adapter->vport_req_info != NULL) {
		for (i = 0; i < adapter->max_vport_nb; i++) {
			rte_free(adapter->vport_req_info[i]);
			adapter->vport_req_info[i] = NULL;
		}
		rte_free(adapter->vport_req_info);
		adapter->vport_req_info = NULL;
	}

	if (adapter->vport_recv_info != NULL) {
		for (i = 0; i < adapter->max_vport_nb; i++) {
			rte_free(adapter->vport_recv_info[i]);
			adapter->vport_recv_info[i] = NULL;
		}
		rte_free(adapter->vport_recv_info);
		adapter->vport_recv_info = NULL;
	}

	rte_free(adapter->vports);
	adapter->vports = NULL;
}

static int
idpf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	struct idpf_adapter *adapter;
	char name[RTE_ETH_NAME_MAX_LEN];
	int i, retval;
	bool first_probe = false;

	if (!idpf_adapter_list_init) {
		rte_spinlock_init(&idpf_adapter_lock);
		TAILQ_INIT(&idpf_adapter_list);
		idpf_adapter_list_init = true;
	}

	adapter = idpf_find_adapter(pci_dev);
	if (adapter == NULL) {
		first_probe = true;
		adapter = rte_zmalloc("idpf_adapter",
						sizeof(struct idpf_adapter), 0);
		if (adapter == NULL) {
			PMD_INIT_LOG(ERR, "Failed to allocate adapter.");
			return -ENOMEM;
		}

		retval = idpf_adapter_init(pci_dev, adapter);
		if (retval != 0) {
			PMD_INIT_LOG(ERR, "Failed to init adapter.");
			return retval;
		}

		rte_spinlock_lock(&idpf_adapter_lock);
		TAILQ_INSERT_TAIL(&idpf_adapter_list, adapter, next);
		rte_spinlock_unlock(&idpf_adapter_lock);
	}

	retval = idpf_parse_devargs(pci_dev, adapter);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to parse private devargs");
		goto err;
	}

	if (adapter->req_vport_nb == 0) {
		/* If no vport devarg, create vport 0 by default. */
		snprintf(name, sizeof(name), "idpf_%s_vport_0",
			 pci_dev->device.name);
		retval = rte_eth_dev_create(&pci_dev->device, name,
					    sizeof(struct idpf_vport),
					    NULL, NULL, idpf_dev_init,
					    adapter);
		if (retval != 0)
			PMD_DRV_LOG(ERR, "Failed to create default vport 0");
		adapter->cur_vports |= RTE_BIT32(0);
		adapter->cur_vport_nb++;
	} else {
		for (i = 0; i < adapter->req_vport_nb; i++) {
			snprintf(name, sizeof(name), "idpf_%s_vport_%d",
				 pci_dev->device.name,
				 adapter->req_vports[i]);
			retval = rte_eth_dev_create(&pci_dev->device, name,
						    sizeof(struct idpf_vport),
						    NULL, NULL, idpf_dev_init,
						    adapter);
			if (retval != 0)
				PMD_DRV_LOG(ERR, "Failed to create vport %d",
					    adapter->req_vports[i]);
		}
	}

	return 0;

err:
	if (first_probe) {
		rte_spinlock_lock(&idpf_adapter_lock);
		TAILQ_REMOVE(&idpf_adapter_list, adapter, next);
		rte_spinlock_unlock(&idpf_adapter_lock);
		idpf_adapter_rel(adapter);
		rte_free(adapter);
	}
	return retval;
}

static int
idpf_pci_remove(struct rte_pci_device *pci_dev)
{
	struct idpf_adapter *adapter = idpf_find_adapter(pci_dev);
	uint16_t port_id;

	/* Ethdev created can be found RTE_ETH_FOREACH_DEV_OF through rte_device */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device) {
			rte_eth_dev_close(port_id);
	}

	rte_spinlock_lock(&idpf_adapter_lock);
	TAILQ_REMOVE(&idpf_adapter_list, adapter, next);
	rte_spinlock_unlock(&idpf_adapter_lock);
	idpf_adapter_rel(adapter);
	rte_free(adapter);

	return 0;
}

static struct rte_pci_driver rte_idpf_pmd = {
	.id_table	= pci_id_idpf_map,
	.drv_flags	= RTE_PCI_DRV_NEED_MAPPING,
	.probe		= idpf_pci_probe,
	.remove		= idpf_pci_remove,
};

/**
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI devices.
 */
RTE_PMD_REGISTER_PCI(net_idpf, rte_idpf_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_idpf, pci_id_idpf_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ice, "* igb_uio | uio_pci_generic | vfio-pci");

RTE_LOG_REGISTER_SUFFIX(idpf_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(idpf_logtype_driver, driver, NOTICE);
