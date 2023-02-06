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

	dev_info->max_rx_queues = adapter->caps.max_rx_q;
	dev_info->max_tx_queues = adapter->caps.max_tx_q;
	dev_info->min_rx_bufsize = IDPF_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = vport->max_mtu + IDPF_ETH_OVERHEAD;

	dev_info->max_mtu = vport->max_mtu;
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
idpf_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct idpf_vport *vport = dev->data->dev_private;

	/* mtu setting is forbidden if port is start */
	if (dev->data->dev_started) {
		PMD_DRV_LOG(ERR, "port must be stopped before configuration");
		return -EBUSY;
	}

	if (mtu > vport->max_mtu) {
		PMD_DRV_LOG(ERR, "MTU should be less than %d", vport->max_mtu);
		return -EINVAL;
	}

	vport->max_pkt_len = mtu + IDPF_ETH_OVERHEAD;

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
idpf_init_vport_req_info(struct rte_eth_dev *dev,
			 struct virtchnl2_create_vport *vport_info)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter_ext *adapter = IDPF_ADAPTER_TO_EXT(vport->adapter);

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
idpf_init_rss(struct idpf_vport *vport)
{
	struct rte_eth_rss_conf *rss_conf;
	struct rte_eth_dev_data *dev_data;
	uint16_t i, nb_q;
	int ret = 0;

	dev_data = vport->dev_data;
	rss_conf = &dev_data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = dev_data->nb_rx_queues;

	if (rss_conf->rss_key == NULL) {
		for (i = 0; i < vport->rss_key_size; i++)
			vport->rss_key[i] = (uint8_t)rte_rand();
	} else if (rss_conf->rss_key_len != vport->rss_key_size) {
		PMD_INIT_LOG(ERR, "Invalid RSS key length in RSS configuration, should be %d",
			     vport->rss_key_size);
		return -EINVAL;
	} else {
		rte_memcpy(vport->rss_key, rss_conf->rss_key,
			   vport->rss_key_size);
	}

	for (i = 0; i < vport->rss_lut_size; i++)
		vport->rss_lut[i] = i % nb_q;

	vport->rss_hf = IDPF_DEFAULT_RSS_HASH_EXPANDED;

	ret = idpf_config_rss(vport);
	if (ret != 0)
		PMD_INIT_LOG(ERR, "Failed to configure RSS");

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

	if (adapter->caps.rss_caps != 0 && dev->data->nb_rx_queues != 0) {
		ret = idpf_init_rss(vport);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to init rss");
			return ret;
		}
	} else {
		PMD_INIT_LOG(ERR, "RSS is not supported.");
		return -1;
	}

	vport->max_pkt_len =
		(dev->data->mtu == 0) ? IDPF_DEFAULT_MTU : dev->data->mtu +
		IDPF_ETH_OVERHEAD;

	return 0;
}

static int
idpf_config_rx_queues_irqs(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	uint16_t nb_rx_queues = dev->data->nb_rx_queues;

	return idpf_config_irq_map(vport, nb_rx_queues);
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
	struct idpf_adapter *base = vport->adapter;
	struct idpf_adapter_ext *adapter = IDPF_ADAPTER_TO_EXT(base);
	uint16_t num_allocated_vectors = base->caps.num_allocated_vectors;
	uint16_t req_vecs_num;
	int ret;

	req_vecs_num = IDPF_DFLT_Q_VEC_NUM;
	if (req_vecs_num + adapter->used_vecs_num > num_allocated_vectors) {
		PMD_DRV_LOG(ERR, "The accumulated request vectors' number should be less than %d",
			    num_allocated_vectors);
		ret = -EINVAL;
		goto err_vec;
	}

	ret = idpf_vc_alloc_vectors(vport, req_vecs_num);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to allocate interrupt vectors");
		goto err_vec;
	}
	adapter->used_vecs_num += req_vecs_num;

	ret = idpf_config_rx_queues_irqs(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to configure irqs");
		goto err_irq;
	}

	ret = idpf_start_queues(dev);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to start queues");
		goto err_startq;
	}

	idpf_set_rx_function(dev);
	idpf_set_tx_function(dev);

	ret = idpf_vc_ena_dis_vport(vport, true);
	if (ret != 0) {
		PMD_DRV_LOG(ERR, "Failed to enable vport");
		goto err_vport;
	}

	vport->stopped = 0;

	return 0;

err_vport:
	idpf_stop_queues(dev);
err_startq:
	idpf_config_irq_unmap(vport, dev->data->nb_rx_queues);
err_irq:
	idpf_vc_dealloc_vectors(vport);
err_vec:
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

	idpf_config_irq_unmap(vport, dev->data->nb_rx_queues);

	idpf_vc_dealloc_vectors(vport);

	vport->stopped = 1;

	return 0;
}

static int
idpf_dev_close(struct rte_eth_dev *dev)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_adapter_ext *adapter = IDPF_ADAPTER_TO_EXT(vport->adapter);

	idpf_dev_stop(dev);

	idpf_vport_deinit(vport);

	adapter->cur_vports &= ~RTE_BIT32(vport->devarg_id);
	adapter->cur_vport_nb--;
	dev->data->dev_private = NULL;
	adapter->vports[vport->sw_idx] = NULL;
	rte_free(vport);

	return 0;
}

static int
insert_value(struct idpf_devargs *devargs, uint16_t id)
{
	uint16_t i;

	/* ignore duplicate */
	for (i = 0; i < devargs->req_vport_nb; i++) {
		if (devargs->req_vports[i] == id)
			return 0;
	}

	if (devargs->req_vport_nb >= RTE_DIM(devargs->req_vports)) {
		PMD_INIT_LOG(ERR, "Total vport number can't be > %d",
			     IDPF_MAX_VPORT_NUM);
		return -EINVAL;
	}

	devargs->req_vports[devargs->req_vport_nb] = id;
	devargs->req_vport_nb++;

	return 0;
}

static const char *
parse_range(const char *value, struct idpf_devargs *devargs)
{
	uint16_t lo, hi, i;
	int n = 0;
	int result;
	const char *pos = value;

	result = sscanf(value, "%hu%n-%hu%n", &lo, &n, &hi, &n);
	if (result == 1) {
		if (lo >= IDPF_MAX_VPORT_NUM)
			return NULL;
		if (insert_value(devargs, lo) != 0)
			return NULL;
	} else if (result == 2) {
		if (lo > hi || hi >= IDPF_MAX_VPORT_NUM)
			return NULL;
		for (i = lo; i <= hi; i++) {
			if (insert_value(devargs, i) != 0)
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
	struct idpf_devargs *devargs = args;
	const char *pos = value;

	devargs->req_vport_nb = 0;

	if (*pos == '[')
		pos++;

	while (1) {
		pos = parse_range(pos, devargs);
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
idpf_parse_devargs(struct rte_pci_device *pci_dev, struct idpf_adapter_ext *adapter,
		   struct idpf_devargs *idpf_args)
{
	struct rte_devargs *devargs = pci_dev->device.devargs;
	struct rte_kvargs *kvlist;
	int i, ret;

	idpf_args->req_vport_nb = 0;

	if (devargs == NULL)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, idpf_valid_args);
	if (kvlist == NULL) {
		PMD_INIT_LOG(ERR, "invalid kvargs key");
		return -EINVAL;
	}

	/* check parsed devargs */
	if (adapter->cur_vport_nb + idpf_args->req_vport_nb >
	    IDPF_MAX_VPORT_NUM) {
		PMD_INIT_LOG(ERR, "Total vport number can't be > %d",
			     IDPF_MAX_VPORT_NUM);
		ret = -EINVAL;
		goto bail;
	}

	for (i = 0; i < idpf_args->req_vport_nb; i++) {
		if (adapter->cur_vports & RTE_BIT32(idpf_args->req_vports[i])) {
			PMD_INIT_LOG(ERR, "Vport %d has been created",
				     idpf_args->req_vports[i]);
			ret = -EINVAL;
			goto bail;
		}
	}

	ret = rte_kvargs_process(kvlist, IDPF_VPORT, &parse_vport,
				 idpf_args);
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

static int
idpf_adapter_ext_init(struct rte_pci_device *pci_dev, struct idpf_adapter_ext *adapter)
{
	struct idpf_adapter *base = &adapter->base;
	struct idpf_hw *hw = &base->hw;
	int ret = 0;

	hw->hw_addr = (void *)pci_dev->mem_resource[0].addr;
	hw->hw_addr_len = pci_dev->mem_resource[0].len;
	hw->back = base;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;

	strncpy(adapter->name, pci_dev->device.name, PCI_PRI_STR_SIZE);

	ret = idpf_adapter_init(base);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init adapter");
		goto err_adapter_init;
	}

	ret = idpf_get_pkt_type(adapter);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to set ptype table");
		goto err_get_ptype;
	}

	adapter->max_vport_nb = adapter->base.caps.max_vports;

	adapter->vports = rte_zmalloc("vports",
				      adapter->max_vport_nb *
				      sizeof(*adapter->vports),
				      0);
	if (adapter->vports == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate vports memory");
		ret = -ENOMEM;
		goto err_get_ptype;
	}

	adapter->cur_vports = 0;
	adapter->cur_vport_nb = 0;

	adapter->used_vecs_num = 0;

	return ret;

err_get_ptype:
	idpf_adapter_deinit(base);
err_adapter_init:
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
idpf_vport_idx_alloc(struct idpf_adapter_ext *ad)
{
	uint16_t vport_idx;
	uint16_t i;

	for (i = 0; i < ad->max_vport_nb; i++) {
		if (ad->vports[i] == NULL)
			break;
	}

	if (i == ad->max_vport_nb)
		vport_idx = IDPF_INVALID_VPORT_IDX;
	else
		vport_idx = i;

	return vport_idx;
}

static int
idpf_dev_vport_init(struct rte_eth_dev *dev, void *init_params)
{
	struct idpf_vport *vport = dev->data->dev_private;
	struct idpf_vport_param *param = init_params;
	struct idpf_adapter_ext *adapter = param->adapter;
	/* for sending create vport virtchnl msg prepare */
	struct virtchnl2_create_vport vport_req_info;
	int ret = 0;

	dev->dev_ops = &idpf_eth_dev_ops;
	vport->adapter = &adapter->base;
	vport->sw_idx = param->idx;
	vport->devarg_id = param->devarg_id;

	memset(&vport_req_info, 0, sizeof(vport_req_info));
	ret = idpf_init_vport_req_info(dev, &vport_req_info);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init vport req_info.");
		goto err;
	}

	ret = idpf_vport_init(vport, &vport_req_info, dev->data);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Failed to init vports.");
		goto err;
	}

	adapter->vports[param->idx] = vport;
	adapter->cur_vports |= RTE_BIT32(param->devarg_id);
	adapter->cur_vport_nb++;

	dev->data->mac_addrs = rte_zmalloc(NULL, RTE_ETHER_ADDR_LEN, 0);
	if (dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Cannot allocate mac_addr memory.");
		ret = -ENOMEM;
		goto err_mac_addrs;
	}

	rte_ether_addr_copy((struct rte_ether_addr *)vport->default_mac_addr,
			    &dev->data->mac_addrs[0]);

	return 0;

err_mac_addrs:
	adapter->vports[param->idx] = NULL;  /* reset */
	idpf_vport_deinit(vport);
err:
	return ret;
}

static const struct rte_pci_id pci_id_idpf_map[] = {
	{ RTE_PCI_DEVICE(IDPF_INTEL_VENDOR_ID, IDPF_DEV_ID_PF) },
	{ .vendor_id = 0, /* sentinel */ },
};

static struct idpf_adapter_ext *
idpf_find_adapter_ext(struct rte_pci_device *pci_dev)
{
	struct idpf_adapter_ext *adapter;
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
idpf_adapter_ext_deinit(struct idpf_adapter_ext *adapter)
{
	idpf_adapter_deinit(&adapter->base);

	rte_free(adapter->vports);
	adapter->vports = NULL;
}

static int
idpf_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	       struct rte_pci_device *pci_dev)
{
	struct idpf_vport_param vport_param;
	struct idpf_adapter_ext *adapter;
	struct idpf_devargs devargs;
	char name[RTE_ETH_NAME_MAX_LEN];
	int i, retval;
	bool first_probe = false;

	if (!idpf_adapter_list_init) {
		rte_spinlock_init(&idpf_adapter_lock);
		TAILQ_INIT(&idpf_adapter_list);
		idpf_adapter_list_init = true;
	}

	adapter = idpf_find_adapter_ext(pci_dev);
	if (adapter == NULL) {
		first_probe = true;
		adapter = rte_zmalloc("idpf_adapter_ext",
				      sizeof(struct idpf_adapter_ext), 0);
		if (adapter == NULL) {
			PMD_INIT_LOG(ERR, "Failed to allocate adapter.");
			return -ENOMEM;
		}

		retval = idpf_adapter_ext_init(pci_dev, adapter);
		if (retval != 0) {
			PMD_INIT_LOG(ERR, "Failed to init adapter.");
			return retval;
		}

		rte_spinlock_lock(&idpf_adapter_lock);
		TAILQ_INSERT_TAIL(&idpf_adapter_list, adapter, next);
		rte_spinlock_unlock(&idpf_adapter_lock);
	}

	retval = idpf_parse_devargs(pci_dev, adapter, &devargs);
	if (retval != 0) {
		PMD_INIT_LOG(ERR, "Failed to parse private devargs");
		goto err;
	}

	if (devargs.req_vport_nb == 0) {
		/* If no vport devarg, create vport 0 by default. */
		vport_param.adapter = adapter;
		vport_param.devarg_id = 0;
		vport_param.idx = idpf_vport_idx_alloc(adapter);
		if (vport_param.idx == IDPF_INVALID_VPORT_IDX) {
			PMD_INIT_LOG(ERR, "No space for vport %u", vport_param.devarg_id);
			return 0;
		}
		snprintf(name, sizeof(name), "idpf_%s_vport_0",
			 pci_dev->device.name);
		retval = rte_eth_dev_create(&pci_dev->device, name,
					    sizeof(struct idpf_vport),
					    NULL, NULL, idpf_dev_vport_init,
					    &vport_param);
		if (retval != 0)
			PMD_DRV_LOG(ERR, "Failed to create default vport 0");
	} else {
		for (i = 0; i < devargs.req_vport_nb; i++) {
			vport_param.adapter = adapter;
			vport_param.devarg_id = devargs.req_vports[i];
			vport_param.idx = idpf_vport_idx_alloc(adapter);
			if (vport_param.idx == IDPF_INVALID_VPORT_IDX) {
				PMD_INIT_LOG(ERR, "No space for vport %u", vport_param.devarg_id);
				break;
			}
			snprintf(name, sizeof(name), "idpf_%s_vport_%d",
				 pci_dev->device.name,
				 devargs.req_vports[i]);
			retval = rte_eth_dev_create(&pci_dev->device, name,
						    sizeof(struct idpf_vport),
						    NULL, NULL, idpf_dev_vport_init,
						    &vport_param);
			if (retval != 0)
				PMD_DRV_LOG(ERR, "Failed to create vport %d",
					    vport_param.devarg_id);
		}
	}

	return 0;

err:
	if (first_probe) {
		rte_spinlock_lock(&idpf_adapter_lock);
		TAILQ_REMOVE(&idpf_adapter_list, adapter, next);
		rte_spinlock_unlock(&idpf_adapter_lock);
		idpf_adapter_ext_deinit(adapter);
		rte_free(adapter);
	}
	return retval;
}

static int
idpf_pci_remove(struct rte_pci_device *pci_dev)
{
	struct idpf_adapter_ext *adapter = idpf_find_adapter_ext(pci_dev);
	uint16_t port_id;

	/* Ethdev created can be found RTE_ETH_FOREACH_DEV_OF through rte_device */
	RTE_ETH_FOREACH_DEV_OF(port_id, &pci_dev->device) {
			rte_eth_dev_close(port_id);
	}

	rte_spinlock_lock(&idpf_adapter_lock);
	TAILQ_REMOVE(&idpf_adapter_list, adapter, next);
	rte_spinlock_unlock(&idpf_adapter_lock);
	idpf_adapter_ext_deinit(adapter);
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
RTE_PMD_REGISTER_KMOD_DEP(net_idpf, "* igb_uio | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_idpf,
			      IDPF_TX_SINGLE_Q "=<0|1> "
			      IDPF_RX_SINGLE_Q "=<0|1> "
			      IDPF_VPORT "=[vport_set0,[vport_set1],...]");

RTE_LOG_REGISTER_SUFFIX(idpf_logtype_init, init, NOTICE);
RTE_LOG_REGISTER_SUFFIX(idpf_logtype_driver, driver, NOTICE);
