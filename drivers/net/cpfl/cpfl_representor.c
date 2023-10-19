/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include "cpfl_representor.h"
#include "cpfl_rxtx.h"
#include "cpfl_flow.h"
#include "cpfl_rules.h"

static int
cpfl_repr_allowlist_update(struct cpfl_adapter_ext *adapter,
			   struct cpfl_repr_id *repr_id,
			   struct rte_eth_dev *dev)
{
	int ret;

	if (rte_hash_lookup(adapter->repr_allowlist_hash, repr_id) < 0)
		return -ENOENT;

	ret = rte_hash_add_key_data(adapter->repr_allowlist_hash, repr_id, dev);

	return ret;
}

static int
cpfl_repr_allowlist_add(struct cpfl_adapter_ext *adapter,
			struct cpfl_repr_id *repr_id)
{
	int ret;

	rte_spinlock_lock(&adapter->repr_lock);
	if (rte_hash_lookup(adapter->repr_allowlist_hash, repr_id) >= 0) {
		ret = -EEXIST;
		goto err;
	}

	ret = rte_hash_add_key(adapter->repr_allowlist_hash, repr_id);
	if (ret < 0)
		goto err;

	rte_spinlock_unlock(&adapter->repr_lock);
	return 0;
err:
	rte_spinlock_unlock(&adapter->repr_lock);
	return ret;
}

static int
cpfl_repr_devargs_process_one(struct cpfl_adapter_ext *adapter,
			      struct rte_eth_devargs *eth_da)
{
	struct cpfl_repr_id repr_id;
	int ret, c, p, v;

	for (c = 0; c < eth_da->nb_mh_controllers; c++) {
		for (p = 0; p < eth_da->nb_ports; p++) {
			repr_id.type = eth_da->type;
			if (eth_da->type == RTE_ETH_REPRESENTOR_PF) {
				repr_id.host_id = eth_da->mh_controllers[c];
				repr_id.pf_id = eth_da->ports[p];
				repr_id.vf_id = 0;
				ret = cpfl_repr_allowlist_add(adapter, &repr_id);
				if (ret == -EEXIST)
					continue;
				if (ret) {
					PMD_DRV_LOG(ERR, "Failed to add PF repr to allowlist, "
							 "host_id = %d, pf_id = %d.",
						    repr_id.host_id, repr_id.pf_id);
					return ret;
				}
			} else if (eth_da->type == RTE_ETH_REPRESENTOR_VF) {
				for (v = 0; v < eth_da->nb_representor_ports; v++) {
					repr_id.host_id = eth_da->mh_controllers[c];
					repr_id.pf_id = eth_da->ports[p];
					repr_id.vf_id = eth_da->representor_ports[v];
					ret = cpfl_repr_allowlist_add(adapter, &repr_id);
					if (ret == -EEXIST)
						continue;
					if (ret) {
						PMD_DRV_LOG(ERR, "Failed to add VF repr to allowlist, "
								 "host_id = %d, pf_id = %d, vf_id = %d.",
							    repr_id.host_id,
							    repr_id.pf_id,
							    repr_id.vf_id);
						return ret;
					}
				}
			}
		}
	}

	return 0;
}

int
cpfl_repr_devargs_process(struct cpfl_adapter_ext *adapter, struct cpfl_devargs *devargs)
{
	int ret, i, j;

	/* check and refine repr args */
	for (i = 0; i < devargs->repr_args_num; i++) {
		struct rte_eth_devargs *eth_da = &devargs->repr_args[i];

		/* set default host_id to host */
		if (eth_da->nb_mh_controllers == 0) {
			eth_da->nb_mh_controllers = 1;
			eth_da->mh_controllers[0] = CPFL_HOST_ID_HOST;
		} else {
			for (j = 0; j < eth_da->nb_mh_controllers; j++) {
				if (eth_da->mh_controllers[j] > CPFL_HOST_ID_ACC) {
					PMD_INIT_LOG(ERR, "Invalid Host ID %d",
						     eth_da->mh_controllers[j]);
					return -EINVAL;
				}
			}
		}

		/* set default pf to APF */
		if (eth_da->nb_ports == 0) {
			eth_da->nb_ports = 1;
			eth_da->ports[0] = CPFL_PF_TYPE_APF;
		} else {
			for (j = 0; j < eth_da->nb_ports; j++) {
				if (eth_da->ports[j] > CPFL_PF_TYPE_CPF) {
					PMD_INIT_LOG(ERR, "Invalid Host ID %d",
						     eth_da->ports[j]);
					return -EINVAL;
				}
			}
		}

		ret = cpfl_repr_devargs_process_one(adapter, eth_da);
		if (ret != 0)
			return ret;
	}

	return 0;
}

static int
cpfl_repr_allowlist_del(struct cpfl_adapter_ext *adapter,
			struct cpfl_repr_id *repr_id)
{
	int ret;

	rte_spinlock_lock(&adapter->repr_lock);

	ret = rte_hash_del_key(adapter->repr_allowlist_hash, repr_id);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to delete repr from allowlist."
				 "host_id = %d, type = %d, pf_id = %d, vf_id = %d",
				 repr_id->host_id, repr_id->type,
				 repr_id->pf_id, repr_id->vf_id);
		goto err;
	}

	rte_spinlock_unlock(&adapter->repr_lock);
	return 0;
err:
	rte_spinlock_unlock(&adapter->repr_lock);
	return ret;
}

static int
cpfl_repr_uninit(struct rte_eth_dev *eth_dev)
{
	struct cpfl_repr *repr = CPFL_DEV_TO_REPR(eth_dev);
	struct cpfl_adapter_ext *adapter = repr->itf.adapter;

	eth_dev->data->mac_addrs = NULL;

	cpfl_repr_allowlist_del(adapter, &repr->repr_id);

	return 0;
}

static int
cpfl_repr_dev_configure(struct rte_eth_dev *dev)
{
	/* now only 1 RX queue is supported */
	if (dev->data->nb_rx_queues > 1)
		return -EINVAL;

	return 0;
}

static int
cpfl_repr_dev_close(struct rte_eth_dev *dev)
{
	return cpfl_repr_uninit(dev);
}

static int
cpfl_repr_dev_info_get(struct rte_eth_dev *ethdev,
		       struct rte_eth_dev_info *dev_info)
{
	struct cpfl_repr *repr = CPFL_DEV_TO_REPR(ethdev);

	dev_info->device = ethdev->device;
	dev_info->max_mac_addrs = 1;
	dev_info->max_rx_queues = 1;
	dev_info->max_tx_queues = 1;
	dev_info->min_rx_bufsize = CPFL_MIN_BUF_SIZE;
	dev_info->max_rx_pktlen = CPFL_MAX_FRAME_SIZE;

	dev_info->flow_type_rss_offloads = CPFL_RSS_OFFLOAD_ALL;

	dev_info->rx_offload_capa =
		RTE_ETH_RX_OFFLOAD_VLAN_STRIP		|
		RTE_ETH_RX_OFFLOAD_QINQ_STRIP		|
		RTE_ETH_RX_OFFLOAD_IPV4_CKSUM		|
		RTE_ETH_RX_OFFLOAD_UDP_CKSUM		|
		RTE_ETH_RX_OFFLOAD_TCP_CKSUM		|
		RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM	|
		RTE_ETH_RX_OFFLOAD_SCATTER		|
		RTE_ETH_RX_OFFLOAD_VLAN_FILTER		|
		RTE_ETH_RX_OFFLOAD_RSS_HASH		|
		RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	dev_info->tx_offload_capa =
		RTE_ETH_TX_OFFLOAD_VLAN_INSERT		|
		RTE_ETH_TX_OFFLOAD_QINQ_INSERT		|
		RTE_ETH_TX_OFFLOAD_IPV4_CKSUM		|
		RTE_ETH_TX_OFFLOAD_UDP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_TCP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_SCTP_CKSUM		|
		RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM	|
		RTE_ETH_TX_OFFLOAD_MULTI_SEGS		|
		RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = CPFL_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_free_thresh = CPFL_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = CPFL_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = CPFL_MAX_RING_DESC,
		.nb_min = CPFL_MIN_RING_DESC,
		.nb_align = CPFL_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = CPFL_MAX_RING_DESC,
		.nb_min = CPFL_MIN_RING_DESC,
		.nb_align = CPFL_ALIGN_RING_DESC,
	};

	dev_info->switch_info.name = ethdev->device->name;
	dev_info->switch_info.domain_id = 0; /* the same domain*/
	dev_info->switch_info.port_id = repr->vport_info->vport.info.vsi_id;

	return 0;
}

static int
cpfl_repr_dev_start(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STARTED;

	return 0;
}

static int
cpfl_repr_dev_stop(struct rte_eth_dev *dev)
{
	uint16_t i;

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		dev->data->tx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		dev->data->rx_queue_state[i] = RTE_ETH_QUEUE_STATE_STOPPED;

	dev->data->dev_started = 0;
	return 0;
}

static int
cpfl_repr_rx_queue_setup(__rte_unused struct rte_eth_dev *dev,
			 __rte_unused uint16_t queue_id,
			 __rte_unused uint16_t nb_desc,
			 __rte_unused unsigned int socket_id,
			 __rte_unused const struct rte_eth_rxconf *conf,
			 __rte_unused struct rte_mempool *pool)
{
	/* Dummy */
	return 0;
}

static int
cpfl_repr_tx_queue_setup(__rte_unused struct rte_eth_dev *dev,
			 __rte_unused uint16_t queue_id,
			 __rte_unused uint16_t nb_desc,
			 __rte_unused unsigned int socket_id,
			 __rte_unused const struct rte_eth_txconf *conf)
{
	/* Dummy */
	return 0;
}

static int
cpfl_func_id_get(uint8_t host_id, uint8_t pf_id)
{
	if ((host_id != CPFL_HOST_ID_HOST &&
	     host_id != CPFL_HOST_ID_ACC) ||
	    (pf_id != CPFL_PF_TYPE_APF &&
	     pf_id != CPFL_PF_TYPE_CPF))
		return -EINVAL;

	static const uint32_t func_id_map[CPFL_HOST_ID_NUM][CPFL_PF_TYPE_NUM] = {
		[CPFL_HOST_ID_HOST][CPFL_PF_TYPE_APF] = CPFL_HOST0_APF,
		[CPFL_HOST_ID_HOST][CPFL_PF_TYPE_CPF] = CPFL_HOST0_CPF_ID,
		[CPFL_HOST_ID_ACC][CPFL_PF_TYPE_APF] = CPFL_ACC_APF_ID,
		[CPFL_HOST_ID_ACC][CPFL_PF_TYPE_CPF] = CPFL_ACC_CPF_ID,
	};

	return func_id_map[host_id][pf_id];
}

static int
cpfl_repr_link_update(struct rte_eth_dev *ethdev,
		      int wait_to_complete)
{
	struct cpfl_repr *repr = CPFL_DEV_TO_REPR(ethdev);
	struct rte_eth_link *dev_link = &ethdev->data->dev_link;
	struct cpfl_adapter_ext *adapter = repr->itf.adapter;
	struct cpchnl2_get_vport_info_response response;
	struct cpfl_vport_id vi;
	int ret;

	if (!(ethdev->data->dev_flags & RTE_ETH_DEV_REPRESENTOR)) {
		PMD_INIT_LOG(ERR, "This ethdev is not representor.");
		return -EINVAL;
	}

	if (wait_to_complete) {
		if (repr->repr_id.type == RTE_ETH_REPRESENTOR_PF) {
			/* PF */
			vi.func_type = CPCHNL2_FTYPE_LAN_PF;
			vi.pf_id = cpfl_func_id_get(repr->repr_id.host_id, repr->repr_id.pf_id);
			vi.vf_id = 0;
		} else {
			/* VF */
			vi.func_type = CPCHNL2_FTYPE_LAN_VF;
			vi.pf_id = CPFL_HOST0_APF;
			vi.vf_id = repr->repr_id.vf_id;
		}
		ret = cpfl_cc_vport_info_get(adapter, &repr->vport_info->vport.vport,
					     &vi, &response);
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "Fail to get vport info.");
			return ret;
		}

		if (response.info.vport_status == CPCHNL2_VPORT_STATUS_ENABLED)
			repr->func_up = true;
		else
			repr->func_up = false;
	}

	dev_link->link_status = repr->func_up ?
		RTE_ETH_LINK_UP : RTE_ETH_LINK_DOWN;

	return 0;
}

static int
cpfl_dev_repr_flow_ops_get(struct rte_eth_dev *dev,
			   const struct rte_flow_ops **ops)
{
	if (!dev)
		return -EINVAL;

#ifdef RTE_HAS_JANSSON
	*ops = &cpfl_flow_ops;
#else
	*ops = NULL;
	PMD_DRV_LOG(NOTICE, "not support rte_flow, please install json-c library.");
#endif
	return 0;
}

static const struct eth_dev_ops cpfl_repr_dev_ops = {
	.dev_start		= cpfl_repr_dev_start,
	.dev_stop		= cpfl_repr_dev_stop,
	.dev_configure		= cpfl_repr_dev_configure,
	.dev_close		= cpfl_repr_dev_close,
	.dev_infos_get		= cpfl_repr_dev_info_get,

	.rx_queue_setup		= cpfl_repr_rx_queue_setup,
	.tx_queue_setup		= cpfl_repr_tx_queue_setup,

	.link_update		= cpfl_repr_link_update,
	.flow_ops_get		= cpfl_dev_repr_flow_ops_get,
};

static int
cpfl_repr_init(struct rte_eth_dev *eth_dev, void *init_param)
{
	struct cpfl_repr *repr = CPFL_DEV_TO_REPR(eth_dev);
	struct cpfl_repr_param *param = init_param;
	struct cpfl_adapter_ext *adapter = param->adapter;
	int ret;

	repr->repr_id = param->repr_id;
	repr->vport_info = param->vport_info;
	repr->itf.type = CPFL_ITF_TYPE_REPRESENTOR;
	repr->itf.adapter = adapter;
	repr->itf.data = eth_dev->data;
	if (repr->vport_info->vport.info.vport_status == CPCHNL2_VPORT_STATUS_ENABLED)
		repr->func_up = true;

	TAILQ_INIT(&repr->itf.flow_list);
	memset(repr->itf.dma, 0, sizeof(repr->itf.dma));
	memset(repr->itf.msg, 0, sizeof(repr->itf.msg));
	ret = cpfl_alloc_dma_mem_batch(&repr->itf.flow_dma, repr->itf.dma,
				       sizeof(union cpfl_rule_cfg_pkt_record),
				       CPFL_FLOW_BATCH_SIZE);
	if (ret < 0)
		return ret;

	eth_dev->dev_ops = &cpfl_repr_dev_ops;

	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;

	eth_dev->data->representor_id =
		CPFL_REPRESENTOR_ID(repr->repr_id.type,
				    repr->repr_id.host_id,
				    repr->repr_id.pf_id,
				    repr->repr_id.vf_id);

	eth_dev->data->mac_addrs = &repr->mac_addr;

	rte_eth_random_addr(repr->mac_addr.addr_bytes);

	return cpfl_repr_allowlist_update(adapter, &repr->repr_id, eth_dev);
}

static bool
cpfl_match_repr_with_vport(const struct cpfl_repr_id *repr_id,
			   struct cpchnl2_vport_info *info)
{
	int func_id;

	if (repr_id->type == RTE_ETH_REPRESENTOR_PF &&
	    info->func_type == CPCHNL2_FTYPE_LAN_PF) {
		func_id = cpfl_func_id_get(repr_id->host_id, repr_id->pf_id);
		if (func_id < 0 || func_id != info->pf_id)
			return false;
		else
			return true;
	} else if (repr_id->type == RTE_ETH_REPRESENTOR_VF &&
		   info->func_type == CPCHNL2_FTYPE_LAN_VF) {
		if (repr_id->vf_id == info->vf_id)
			return true;
	}

	return false;
}

static int
cpfl_repr_vport_list_query(struct cpfl_adapter_ext *adapter,
			   const struct cpfl_repr_id *repr_id,
			   struct cpchnl2_get_vport_list_response *response)
{
	struct cpfl_vport_id vi;
	int ret;

	if (repr_id->type == RTE_ETH_REPRESENTOR_PF) {
		/* PF */
		vi.func_type = CPCHNL2_FTYPE_LAN_PF;
		vi.pf_id = cpfl_func_id_get(repr_id->host_id, repr_id->pf_id);
		vi.vf_id = 0;
	} else {
		/* VF */
		vi.func_type = CPCHNL2_FTYPE_LAN_VF;
		vi.pf_id = CPFL_HOST0_APF;
		vi.vf_id = repr_id->vf_id;
	}

	ret = cpfl_cc_vport_list_get(adapter, &vi, response);

	return ret;
}

static int
cpfl_repr_vport_info_query(struct cpfl_adapter_ext *adapter,
			   const struct cpfl_repr_id *repr_id,
			   struct cpchnl2_vport_id *vport_id,
			   struct cpchnl2_get_vport_info_response *response)
{
	struct cpfl_vport_id vi;
	int ret;

	if (repr_id->type == RTE_ETH_REPRESENTOR_PF) {
		/* PF */
		vi.func_type = CPCHNL2_FTYPE_LAN_PF;
		vi.pf_id = cpfl_func_id_get(repr_id->host_id, repr_id->pf_id);
		vi.vf_id = 0;
	} else {
		/* VF */
		vi.func_type = CPCHNL2_FTYPE_LAN_VF;
		vi.pf_id = CPFL_HOST0_APF;
		vi.vf_id = repr_id->vf_id;
	}

	ret = cpfl_cc_vport_info_get(adapter, vport_id, &vi, response);

	return ret;
}

static int
cpfl_repr_vport_map_update(struct cpfl_adapter_ext *adapter,
			   const struct cpfl_repr_id *repr_id, uint32_t vport_id,
			   struct cpchnl2_get_vport_info_response *response)
{
	struct cpfl_vport_id vi;
	int ret;

	vi.vport_id = vport_id;
	if (repr_id->type == RTE_ETH_REPRESENTOR_PF) {
		/* PF */
		vi.func_type = CPCHNL2_FTYPE_LAN_VF;
		vi.pf_id = cpfl_func_id_get(repr_id->host_id, repr_id->pf_id);
	} else {
		/* VF */
		vi.func_type = CPCHNL2_FTYPE_LAN_VF;
		vi.pf_id = CPFL_HOST0_APF;
		vi.vf_id = repr_id->vf_id;
	}

	ret = cpfl_vport_info_create(adapter, &vi, (struct cpchnl2_event_vport_created *)response);
	if (ret != 0) {
		PMD_INIT_LOG(ERR, "Fail to update vport map hash for representor.");
		return ret;
	}

	return 0;
}

int
cpfl_repr_create(struct rte_pci_device *pci_dev, struct cpfl_adapter_ext *adapter)
{
	struct rte_eth_dev *dev;
	uint32_t iter = 0;
	const struct cpfl_repr_id *repr_id;
	const struct cpfl_vport_id *vp_id;
	struct cpchnl2_get_vport_list_response *vlist_resp;
	struct cpchnl2_get_vport_info_response vinfo_resp;
	int ret;

	vlist_resp = rte_zmalloc(NULL, IDPF_DFLT_MBX_BUF_SIZE, 0);
	if (vlist_resp == NULL)
		return -ENOMEM;

	rte_spinlock_lock(&adapter->repr_lock);

	while (rte_hash_iterate(adapter->repr_allowlist_hash,
				(const void **)&repr_id, (void **)&dev, &iter) >= 0) {
		struct cpfl_vport_info *vi;
		char name[RTE_ETH_NAME_MAX_LEN];
		uint32_t iter_iter = 0;
		int i;

		/* skip representor already be created */
		if (dev != NULL)
			continue;

		if (repr_id->type == RTE_ETH_REPRESENTOR_VF)
			snprintf(name, sizeof(name), "net_%s_representor_c%dpf%dvf%d",
				 pci_dev->name,
				 repr_id->host_id,
				 repr_id->pf_id,
				 repr_id->vf_id);
		else
			snprintf(name, sizeof(name), "net_%s_representor_c%dpf%d",
				 pci_dev->name,
				 repr_id->host_id,
				 repr_id->pf_id);

		/* get vport list for the port representor */
		ret = cpfl_repr_vport_list_query(adapter, repr_id, vlist_resp);
		if (ret != 0) {
			PMD_INIT_LOG(ERR, "Failed to get host%d pf%d vf%d's vport list",
				     repr_id->host_id, repr_id->pf_id, repr_id->vf_id);
			goto err;
		}

		if (vlist_resp->nof_vports == 0) {
			PMD_INIT_LOG(WARNING, "No matched vport for representor %s", name);
			continue;
		}

		/* get all vport info for the port representor */
		for (i = 0; i < vlist_resp->nof_vports; i++) {
			ret = cpfl_repr_vport_info_query(adapter, repr_id,
							 &vlist_resp->vports[i], &vinfo_resp);
			if (ret != 0) {
				PMD_INIT_LOG(ERR, "Failed to get host%d pf%d vf%d vport[%d]'s info",
					     repr_id->host_id, repr_id->pf_id, repr_id->vf_id,
					     vlist_resp->vports[i].vport_id);
				goto err;
			}

			ret = cpfl_repr_vport_map_update(adapter, repr_id,
						 vlist_resp->vports[i].vport_id, &vinfo_resp);
			if (ret != 0) {
				PMD_INIT_LOG(ERR, "Failed to update  host%d pf%d vf%d vport[%d]'s info to vport_map_hash",
					     repr_id->host_id, repr_id->pf_id, repr_id->vf_id,
					     vlist_resp->vports[i].vport_id);
				goto err;
			}
		}

		/* find the matched vport */
		rte_spinlock_lock(&adapter->vport_map_lock);

		while (rte_hash_iterate(adapter->vport_map_hash,
					(const void **)&vp_id, (void **)&vi, &iter_iter) >= 0) {
			struct cpfl_repr_param param;

			if (!cpfl_match_repr_with_vport(repr_id, &vi->vport.info))
				continue;

			param.adapter = adapter;
			param.repr_id = *repr_id;
			param.vport_info = vi;

			ret = rte_eth_dev_create(&pci_dev->device,
						 name,
						 sizeof(struct cpfl_repr),
						 NULL, NULL, cpfl_repr_init,
						 &param);
			if (ret != 0) {
				PMD_INIT_LOG(ERR, "Failed to create representor %s", name);
				rte_spinlock_unlock(&adapter->vport_map_lock);
				goto err;
			}
			break;
		}

		rte_spinlock_unlock(&adapter->vport_map_lock);
	}

err:
	rte_spinlock_unlock(&adapter->repr_lock);
	rte_free(vlist_resp);
	return ret;
}
