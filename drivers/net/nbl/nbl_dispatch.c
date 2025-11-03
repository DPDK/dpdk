/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_dispatch.h"

static int nbl_disp_configure_msix_map(void *priv, u16 num_net_msix, u16 num_others_msix,
				       bool net_msix_mask_en)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	int ret = 0;

	ret = NBL_OPS_CALL(res_ops->configure_msix_map,
				(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), 0, num_net_msix,
				num_others_msix, net_msix_mask_en));
	return ret;
}

static int nbl_disp_chan_configure_msix_map_req(void *priv, u16 num_net_msix, u16 num_others_msix,
						bool net_msix_mask_en)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_cfg_msix_map param = {0};
	struct nbl_chan_send_info chan_send;

	param.num_net_msix = num_net_msix;
	param.num_others_msix = num_others_msix;
	param.msix_mask_en = net_msix_mask_en;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_CONFIGURE_MSIX_MAP,
		      &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_destroy_msix_map(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	int ret = 0;

	ret = NBL_OPS_CALL(res_ops->destroy_msix_map, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), 0));
	return ret;
}

static int nbl_disp_chan_destroy_msix_map_req(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_DESTROY_MSIX_MAP, NULL, 0, NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_enable_mailbox_irq(void *priv, u16 vector_id, bool enable_msix)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	int ret = 0;

	ret = NBL_OPS_CALL(res_ops->enable_mailbox_irq,
			   (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), 0, vector_id, enable_msix));
	return ret;
}

static int nbl_disp_chan_enable_mailbox_irq_req(void *priv, u16 vector_id, bool enable_msix)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_enable_mailbox_irq param = {0};
	struct nbl_chan_send_info chan_send;

	param.vector_id = vector_id;
	param.enable_msix = enable_msix;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_MAILBOX_ENABLE_IRQ,
		      &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_alloc_txrx_queues(void *priv, u16 vsi_id, u16 queue_num)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->alloc_txrx_queues,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id, queue_num));
}

static int nbl_disp_chan_alloc_txrx_queues_req(void *priv, u16 vsi_id,
					       u16 queue_num)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_alloc_txrx_queues param = {0};
	struct nbl_chan_param_alloc_txrx_queues result = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;
	param.queue_num = queue_num;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_ALLOC_TXRX_QUEUES, &param,
		      sizeof(param), &result, sizeof(result), 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);

	return 0;
}

static void nbl_disp_free_txrx_queues(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->free_txrx_queues, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_free_txrx_queues_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_free_txrx_queues param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_FREE_TXRX_QUEUES, &param,
		      sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_clear_queues(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->clear_queues, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_clear_queues_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send = {0};

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_CLEAR_QUEUE, &vsi_id, sizeof(vsi_id),
		      NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_start_tx_ring(void *priv,
				  struct nbl_start_tx_ring_param *param,
				  u64 *dma_addr)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->start_tx_ring,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), param, dma_addr));
}

static void nbl_disp_release_tx_ring(void *priv, u16 queue_idx)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->release_tx_ring, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), queue_idx));
}

static void nbl_disp_stop_tx_ring(void *priv, u16 queue_idx)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->stop_tx_ring, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), queue_idx));
}

static int nbl_disp_start_rx_ring(void *priv,
				  struct nbl_start_rx_ring_param *param,
				  u64 *dma_addr)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->start_rx_ring,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), param, dma_addr));
}

static int nbl_disp_alloc_rx_bufs(void *priv, u16 queue_idx)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->alloc_rx_bufs,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), queue_idx));
}

static void nbl_disp_release_rx_ring(void *priv, u16 queue_idx)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->release_rx_ring,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), queue_idx));
}

static void nbl_disp_stop_rx_ring(void *priv, u16 queue_idx)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->stop_rx_ring,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), queue_idx));
}

static void nbl_disp_update_rx_ring(void *priv, u16 index)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->update_rx_ring, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), index));
}

static int nbl_disp_alloc_rings(void *priv, u16 tx_num, u16 rx_num, u16 queue_offset)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->alloc_rings,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), tx_num, rx_num, queue_offset));
}

static void nbl_disp_remove_rings(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->remove_rings, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt)));
}

static int
nbl_disp_setup_queue(void *priv, struct nbl_txrx_queue_param *param, bool is_tx)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->setup_queue,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), param, is_tx));
}

static int
nbl_disp_chan_setup_queue_req(void *priv,
			      struct nbl_txrx_queue_param *queue_param,
			      bool is_tx)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_setup_queue param = {0};
	struct nbl_chan_send_info chan_send;

	memcpy(&param.queue_param, queue_param, sizeof(param.queue_param));
	param.is_tx = is_tx;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_SETUP_QUEUE, &param,
		      sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_remove_all_queues(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->remove_all_queues, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_remove_all_queues_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_remove_all_queues param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_REMOVE_ALL_QUEUES,
		      &param, sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_get_mac_addr(void *priv __rte_unused, u8 *mac)
{
	rte_eth_random_addr(mac);

	return 0;
}

static int nbl_disp_get_mac_addr_req(void *priv, u8 *mac)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_common_info *common = NBL_DISP_MGT_TO_COMMON(disp_mgt);
	int ret = -1;

	if (common->nl_socket_route >= 0 && common->ifindex >= 0)
		ret = nbl_userdev_get_mac_addr(common, mac);

	if (ret) {
		if (rte_is_zero_ether_addr((struct rte_ether_addr *)common->mac))
			rte_eth_random_addr(mac);
		else
			rte_ether_addr_copy((struct rte_ether_addr *)common->mac,
					    (struct rte_ether_addr *)mac);
	}

	return 0;
}

static int nbl_disp_register_net(void *priv,
				struct nbl_register_net_param *register_param,
				struct nbl_register_net_result *register_result)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->register_net,
			(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), register_param, register_result));
}

static int nbl_disp_chan_register_net_req(void *priv,
				struct nbl_register_net_param *register_param,
				struct nbl_register_net_result *register_result)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_register_net_info param = {0};
	struct nbl_chan_send_info chan_send;
	int ret = 0;

	param.pf_bar_start = register_param->pf_bar_start;
	param.pf_bdf = register_param->pf_bdf;
	param.vf_bar_start = register_param->vf_bar_start;
	param.vf_bar_size = register_param->vf_bar_size;
	param.total_vfs = register_param->total_vfs;
	param.offset = register_param->offset;
	param.stride = register_param->stride;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_REGISTER_NET,
		      &param, sizeof(param),
		      (void *)register_result, sizeof(*register_result), 1);

	ret = chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
	return ret;
}

static int nbl_disp_unregister_net(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->unregister_net, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt)));
}

static int nbl_disp_chan_unregister_net_req(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_UNREGISTER_NET, NULL,
		      0, NULL, 0, 1);

	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt),
				  &chan_send);
}

static u16 nbl_disp_get_vsi_id(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->get_vsi_id, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt)));
}

static u16 nbl_disp_chan_get_vsi_id_req(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_get_vsi_id param = {0};
	struct nbl_chan_param_get_vsi_id result = {0};
	struct nbl_chan_send_info chan_send;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_GET_VSI_ID, &param,
		      sizeof(param), &result, sizeof(result), 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);

	return result.vsi_id;
}

static void nbl_disp_get_eth_id(void *priv, u16 vsi_id, u8 *eth_mode, u8 *eth_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->get_eth_id, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt),
					   vsi_id, eth_mode, eth_id));
}

static void nbl_disp_chan_get_eth_id_req(void *priv, u16 vsi_id, u8 *eth_mode, u8 *eth_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_get_eth_id param = {0};
	struct nbl_chan_param_get_eth_id result = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_GET_ETH_ID,  &param, sizeof(param),
		      &result, sizeof(result), 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);

	*eth_mode = result.eth_mode;
	*eth_id = result.eth_id;
}

static u16 nbl_disp_get_vsi_global_qid(void *priv, u16 vsi_id, u16 local_qid)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->get_vsi_global_qid,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id, local_qid));
}

static u16
nbl_disp_chan_get_vsi_global_qid_req(void *priv, u16 vsi_id, u16 local_qid)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_vsi_qid_info param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;
	param.local_qid = local_qid;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_GET_VSI_GLOBAL_QUEUE_ID,
		      &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_chan_setup_q2vsi(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->setup_q2vsi,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static int nbl_disp_chan_setup_q2vsi_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_cfg_q2vsi param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_SETUP_Q2VSI, &param,
		      sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_chan_remove_q2vsi(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->remove_q2vsi,
		     (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_remove_q2vsi_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_cfg_q2vsi param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_REMOVE_Q2VSI, &param,
		      sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_chan_register_vsi2q(void *priv, u16 vsi_index, u16 vsi_id,
					u16 queue_offset, u16 queue_num)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->register_vsi2q,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_index,
			     vsi_id, queue_offset, queue_num));
}

static int nbl_disp_chan_register_vsi2q_req(void *priv, u16 vsi_index, u16 vsi_id,
					    u16 queue_offset, u16 queue_num)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_register_vsi2q param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_index = vsi_index;
	param.vsi_id = vsi_id;
	param.queue_offset = queue_offset;
	param.queue_num = queue_num;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_REGISTER_VSI2Q, &param, sizeof(param),
		      NULL, 0, 1);

	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_chan_setup_rss(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->setup_rss,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static int nbl_disp_chan_setup_rss_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_cfg_rss param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_SETUP_RSS, &param,
		      sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_chan_remove_rss(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->remove_rss,
		     (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_remove_rss_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_cfg_rss param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_REMOVE_RSS, &param,
		      sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_chan_get_board_info(void *priv, struct nbl_board_port_info *board_info)
{
	RTE_SET_USED(priv);
	RTE_SET_USED(board_info);
}

static void nbl_disp_chan_get_board_info_req(void *priv, struct nbl_board_port_info *board_info)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_GET_BOARD_INFO, NULL,
		      0, board_info, sizeof(*board_info), 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_clear_flow(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->clear_flow, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_clear_flow_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send = {0};

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_CLEAR_FLOW, &vsi_id, sizeof(vsi_id), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_add_macvlan(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->add_macvlan,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), mac, vlan_id, vsi_id));
}

static int
nbl_disp_chan_add_macvlan_req(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send = {0};
	struct nbl_chan_param_macvlan_cfg param = {0};

	memcpy(&param.mac, mac, sizeof(param.mac));
	param.vlan = vlan_id;
	param.vsi = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_ADD_MACVLAN, &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_del_macvlan(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->del_macvlan,
		     (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), mac, vlan_id, vsi_id));
}

static void
nbl_disp_chan_del_macvlan_req(void *priv, u8 *mac, u16 vlan_id, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send = {0};
	struct nbl_chan_param_macvlan_cfg param = {0};

	memcpy(&param.mac, mac, sizeof(param.mac));
	param.vlan = vlan_id;
	param.vsi = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_DEL_MACVLAN, &param, sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_add_multi_rule(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->add_multi_rule, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static int nbl_disp_chan_add_multi_rule_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_add_multi_rule param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_ADD_MULTI_RULE, &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_del_multi_rule(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->del_multi_rule, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_del_multi_rule_req(void *priv, u16 vsi)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_del_multi_rule param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi = vsi;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_DEL_MULTI_RULE, &param, sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_cfg_dsch(void *priv, u16 vsi_id, bool vld)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->cfg_dsch, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id, vld));
}

static int nbl_disp_chan_cfg_dsch_req(void *priv, u16 vsi_id, bool vld)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_cfg_dsch param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;
	param.vld = vld;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_CFG_DSCH, &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_setup_cqs(void *priv, u16 vsi_id, u16 real_qps, bool rss_indir_set)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return NBL_OPS_CALL(res_ops->setup_cqs,
			    (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id, real_qps, rss_indir_set));
}

static int nbl_disp_chan_setup_cqs_req(void *priv, u16 vsi_id, u16 real_qps, bool rss_indir_set)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_setup_cqs param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;
	param.real_qps = real_qps;
	param.rss_indir_set = rss_indir_set;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_SETUP_CQS, &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_remove_cqs(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	NBL_OPS_CALL(res_ops->remove_cqs, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id));
}

static void nbl_disp_chan_remove_cqs_req(void *priv, u16 vsi_id)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_remove_cqs param = {0};
	struct nbl_chan_send_info chan_send;

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_REMOVE_CQS, &param, sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static void nbl_disp_get_res_pt_ops(void *priv, struct nbl_resource_pt_ops *pt_ops, bool offload)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	NBL_OPS_CALL(res_ops->get_resource_pt_ops,
		     (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), pt_ops, offload));
}

static void nbl_disp_get_link_state(void *priv, u8 eth_id, struct nbl_eth_link_info *eth_link_info)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	/* if do not have res_ops->get_link_state(), default eth is up */
	if (res_ops->get_link_state) {
		res_ops->get_link_state(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt),
					eth_id, eth_link_info);
	} else {
		eth_link_info->link_status = 1;
		eth_link_info->link_speed = RTE_ETH_LINK_SPEED_25G;
	}
}

static void nbl_disp_chan_get_link_state_req(void *priv, u8 eth_id,
					     struct nbl_eth_link_info *eth_link_info)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_get_link_state param = {0};
	struct nbl_chan_send_info chan_send;

	param.eth_id = eth_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_GET_LINK_STATE, &param, sizeof(param),
		      eth_link_info, sizeof(*eth_link_info), 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_get_stats(void *priv, struct rte_eth_stats *rte_stats,
			      struct eth_queue_stats *qstats)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return res_ops->get_stats(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), rte_stats, qstats);
}

static int nbl_disp_reset_stats(void *priv)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);

	return res_ops->reset_stats(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt));
}

static int nbl_disp_get_txrx_xstats_cnt(void *priv, u16 *xstats_cnt)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	return res_ops->get_txrx_xstats_cnt(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), xstats_cnt);
}

static int nbl_disp_get_txrx_xstats(void *priv, struct rte_eth_xstat *xstats,
				    u16 need_xstats_cnt, u16 *xstats_cnt)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	return res_ops->get_txrx_xstats(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), xstats,
					need_xstats_cnt, xstats_cnt);
}

static int nbl_disp_get_txrx_xstats_names(void *priv, struct rte_eth_xstat_name *xstats_names,
					  u16 need_xstats_cnt, u16 *xstats_cnt)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	return res_ops->get_txrx_xstats_names(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt),
					 xstats_names, need_xstats_cnt, xstats_cnt);
}

static int nbl_disp_get_hw_xstats_cnt(void *priv, u16 *xstats_cnt)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	return res_ops->get_hw_xstats_cnt(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), xstats_cnt);
}

static int nbl_disp_get_hw_xstats_names(void *priv, struct rte_eth_xstat_name *xstats_names,
					u16 need_xstats_cnt, u16 *xstats_cnt)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	return res_ops->get_hw_xstats_names(NBL_DISP_MGT_TO_RES_PRIV(disp_mgt),
					    xstats_names, need_xstats_cnt, xstats_cnt);
}

static void nbl_disp_get_private_stat_data(void *priv, u32 eth_id, u64 *data,
					   __rte_unused u32 data_len)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	NBL_OPS_CALL(res_ops->get_private_stat_data,
		     (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), eth_id, data));
}

static void nbl_disp_get_private_stat_data_req(void *priv, u32 eth_id, u64 *data, u32 data_len)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send = {0};
	struct nbl_chan_param_get_private_stat_data param = {0};

	param.eth_id = eth_id;
	param.data_len = data_len;
	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_GET_ETH_STATS, &param,
		      sizeof(param), data, data_len, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
}

static int nbl_disp_set_mtu(void *priv, u16 vsi_id, u16 mtu)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;
	int ret = 0;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	ret = NBL_OPS_CALL(res_ops->set_mtu, (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id, mtu));
	return ret;
}

static int nbl_disp_chan_set_mtu_req(void *priv, u16 vsi_id, u16 mtu)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_send_info chan_send = {0};
	struct nbl_chan_param_set_mtu param = {0};

	param.mtu = mtu;
	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_MTU_SET,
		      &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt),
				  &chan_send);
}

static int nbl_disp_set_promisc_mode(void *priv, u16 vsi_id, u16 mode)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_resource_ops *res_ops;
	int ret = 0;

	res_ops = NBL_DISP_MGT_TO_RES_OPS(disp_mgt);
	ret = NBL_OPS_CALL(res_ops->set_promisc_mode,
			   (NBL_DISP_MGT_TO_RES_PRIV(disp_mgt), vsi_id, mode));
	return ret;
}

static int nbl_disp_chan_set_promisc_mode_req(void *priv, u16 vsi_id, u16 mode)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)priv;
	struct nbl_common_info *common;
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	struct nbl_chan_param_set_promisc_mode param = {0};
	struct nbl_chan_send_info chan_send = {0};
	int ret = 0;

	common = NBL_DISP_MGT_TO_COMMON(disp_mgt);
	if (NBL_IS_COEXISTENCE(common)) {
		ret = ioctl(common->devfd, NBL_DEV_USER_SET_PROMISC_MODE, &mode);
		if (ret) {
			NBL_LOG(ERR, "userspace send set_promisc_mode ioctl msg failed ret %d",
				ret);
			return ret;
		}
		return 0;
	}

	param.vsi_id = vsi_id;
	param.mode = mode;
	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_SET_PROSISC_MODE,
		      &param, sizeof(param), NULL, 0, 1);
	return chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt),
				  &chan_send);
}

#define NBL_DISP_OPS_TBL						\
do {									\
	NBL_DISP_SET_OPS(configure_msix_map, nbl_disp_configure_msix_map,			\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_CONFIGURE_MSIX_MAP,		\
			 nbl_disp_chan_configure_msix_map_req,					\
			 NULL);				\
	NBL_DISP_SET_OPS(destroy_msix_map, nbl_disp_destroy_msix_map,				\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_DESTROY_MSIX_MAP,			\
			 nbl_disp_chan_destroy_msix_map_req,					\
			 NULL);					\
	NBL_DISP_SET_OPS(enable_mailbox_irq, nbl_disp_enable_mailbox_irq,			\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_MAILBOX_ENABLE_IRQ,		\
			 nbl_disp_chan_enable_mailbox_irq_req,					\
			 NULL);				\
	NBL_DISP_SET_OPS(alloc_txrx_queues, nbl_disp_alloc_txrx_queues,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_ALLOC_TXRX_QUEUES,		\
			 nbl_disp_chan_alloc_txrx_queues_req,		\
			 NULL);						\
	NBL_DISP_SET_OPS(free_txrx_queues, nbl_disp_free_txrx_queues,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_FREE_TXRX_QUEUES,			\
			 nbl_disp_chan_free_txrx_queues_req,		\
			 NULL);						\
	NBL_DISP_SET_OPS(alloc_rings, nbl_disp_alloc_rings,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(remove_rings, nbl_disp_remove_rings,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(start_tx_ring, nbl_disp_start_tx_ring,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1, NULL, NULL);	\
	NBL_DISP_SET_OPS(stop_tx_ring, nbl_disp_stop_tx_ring,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1, NULL, NULL);	\
	NBL_DISP_SET_OPS(release_tx_ring, nbl_disp_release_tx_ring,	\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1, NULL, NULL);	\
	NBL_DISP_SET_OPS(start_rx_ring, nbl_disp_start_rx_ring,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1, NULL, NULL);	\
	NBL_DISP_SET_OPS(alloc_rx_bufs, nbl_disp_alloc_rx_bufs,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1, NULL, NULL);	\
	NBL_DISP_SET_OPS(stop_rx_ring, nbl_disp_stop_rx_ring,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1, NULL, NULL);	\
	NBL_DISP_SET_OPS(release_rx_ring, nbl_disp_release_rx_ring,	\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1, NULL, NULL);	\
	NBL_DISP_SET_OPS(update_rx_ring, nbl_disp_update_rx_ring,	\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(setup_queue, nbl_disp_setup_queue,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_SETUP_QUEUE,			\
			 nbl_disp_chan_setup_queue_req, NULL);		\
	NBL_DISP_SET_OPS(remove_all_queues, nbl_disp_remove_all_queues,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_REMOVE_ALL_QUEUES,		\
			 nbl_disp_chan_remove_all_queues_req, NULL);	\
	NBL_DISP_SET_OPS(clear_queues, nbl_disp_clear_queues,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_CLEAR_QUEUE,			\
			 nbl_disp_chan_clear_queues_req, NULL);		\
	NBL_DISP_SET_OPS(get_mac_addr, nbl_disp_get_mac_addr,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 -1, nbl_disp_get_mac_addr_req, NULL);		\
	NBL_DISP_SET_OPS(register_net, nbl_disp_register_net,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_REGISTER_NET,			\
			 nbl_disp_chan_register_net_req,		\
			 NULL);						\
	NBL_DISP_SET_OPS(unregister_net, nbl_disp_unregister_net,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_UNREGISTER_NET,			\
			 nbl_disp_chan_unregister_net_req, NULL);	\
	NBL_DISP_SET_OPS(get_vsi_id, nbl_disp_get_vsi_id,		\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_GET_VSI_ID,\
			 nbl_disp_chan_get_vsi_id_req, NULL);		\
	NBL_DISP_SET_OPS(get_eth_id, nbl_disp_get_eth_id,		\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_GET_ETH_ID,\
			 nbl_disp_chan_get_eth_id_req, NULL);		\
	NBL_DISP_SET_OPS(get_vsi_global_qid,				\
			 nbl_disp_get_vsi_global_qid,			\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_GET_VSI_GLOBAL_QUEUE_ID,		\
			 nbl_disp_chan_get_vsi_global_qid_req, NULL);	\
	NBL_DISP_SET_OPS(setup_q2vsi, nbl_disp_chan_setup_q2vsi,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_SETUP_Q2VSI,			\
			 nbl_disp_chan_setup_q2vsi_req, NULL);		\
	NBL_DISP_SET_OPS(remove_q2vsi, nbl_disp_chan_remove_q2vsi,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_REMOVE_Q2VSI,			\
			 nbl_disp_chan_remove_q2vsi_req, NULL);		\
	NBL_DISP_SET_OPS(register_vsi2q, nbl_disp_chan_register_vsi2q,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_REGISTER_VSI2Q,			\
			 nbl_disp_chan_register_vsi2q_req, NULL);	\
	NBL_DISP_SET_OPS(setup_rss, nbl_disp_chan_setup_rss,		\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_SETUP_RSS,	\
			 nbl_disp_chan_setup_rss_req, NULL);		\
	NBL_DISP_SET_OPS(remove_rss, nbl_disp_chan_remove_rss,		\
			 NBL_DISP_CTRL_LVL_MGT,	NBL_CHAN_MSG_REMOVE_RSS,\
			 nbl_disp_chan_remove_rss_req, NULL);		\
	NBL_DISP_SET_OPS(get_board_info, nbl_disp_chan_get_board_info,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_GET_BOARD_INFO,			\
			 nbl_disp_chan_get_board_info_req, NULL);	\
	NBL_DISP_SET_OPS(clear_flow, nbl_disp_clear_flow,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_CLEAR_FLOW,			\
			 nbl_disp_chan_clear_flow_req, NULL);		\
	NBL_DISP_SET_OPS(add_macvlan, nbl_disp_add_macvlan,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_ADD_MACVLAN,			\
			 nbl_disp_chan_add_macvlan_req, NULL);		\
	NBL_DISP_SET_OPS(del_macvlan, nbl_disp_del_macvlan,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_DEL_MACVLAN,			\
			 nbl_disp_chan_del_macvlan_req, NULL);		\
	NBL_DISP_SET_OPS(add_multi_rule, nbl_disp_add_multi_rule,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_ADD_MULTI_RULE,			\
			 nbl_disp_chan_add_multi_rule_req, NULL);	\
	NBL_DISP_SET_OPS(del_multi_rule, nbl_disp_del_multi_rule,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_DEL_MULTI_RULE,			\
			 nbl_disp_chan_del_multi_rule_req, NULL);	\
	NBL_DISP_SET_OPS(cfg_dsch, nbl_disp_cfg_dsch,			\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_CFG_DSCH,	\
			 nbl_disp_chan_cfg_dsch_req, NULL);		\
	NBL_DISP_SET_OPS(setup_cqs, nbl_disp_setup_cqs,			\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_SETUP_CQS,	\
			 nbl_disp_chan_setup_cqs_req, NULL);		\
	NBL_DISP_SET_OPS(remove_cqs, nbl_disp_remove_cqs,		\
			 NBL_DISP_CTRL_LVL_MGT, NBL_CHAN_MSG_REMOVE_CQS,\
			 nbl_disp_chan_remove_cqs_req, NULL);		\
	NBL_DISP_SET_OPS(get_resource_pt_ops,				\
			 nbl_disp_get_res_pt_ops,			\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(get_link_state, nbl_disp_get_link_state,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_GET_LINK_STATE,			\
			 nbl_disp_chan_get_link_state_req, NULL);	\
	NBL_DISP_SET_OPS(get_stats, nbl_disp_get_stats,			\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(reset_stats, nbl_disp_reset_stats,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(get_txrx_xstats_cnt,				\
			 nbl_disp_get_txrx_xstats_cnt,			\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(get_txrx_xstats, nbl_disp_get_txrx_xstats,	\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(get_txrx_xstats_names,				\
			 nbl_disp_get_txrx_xstats_names,		\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(get_hw_xstats_cnt, nbl_disp_get_hw_xstats_cnt,	\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(get_hw_xstats_names,				\
			 nbl_disp_get_hw_xstats_names,			\
			 NBL_DISP_CTRL_LVL_ALWAYS, -1,			\
			 NULL, NULL);					\
	NBL_DISP_SET_OPS(get_private_stat_data,				\
			 nbl_disp_get_private_stat_data,		\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_GET_ETH_STATS,			\
			 nbl_disp_get_private_stat_data_req, NULL);	\
	NBL_DISP_SET_OPS(set_promisc_mode, nbl_disp_set_promisc_mode,	\
			 NBL_DISP_CTRL_LVL_MGT,				\
			 NBL_CHAN_MSG_SET_PROSISC_MODE,			\
			 nbl_disp_chan_set_promisc_mode_req, NULL);	\
	NBL_DISP_SET_OPS(set_mtu, nbl_disp_set_mtu,			\
			 NBL_DISP_CTRL_LVL_MGT,	NBL_CHAN_MSG_MTU_SET,	\
			 nbl_disp_chan_set_mtu_req,			\
			 NULL);						\
} while (0)

/* Structure starts here, adding an op should not modify anything below */
static int nbl_disp_setup_msg(struct nbl_dispatch_mgt *disp_mgt)
{
	struct nbl_dispatch_ops *disp_ops = NBL_DISP_MGT_TO_DISP_OPS(disp_mgt);
	const struct nbl_channel_ops *chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);
	int ret = 0;

#define NBL_DISP_SET_OPS(disp_op, res_func, ctrl_lvl2, msg_type, msg_req, msg_resp)		\
do {												\
	typeof(msg_type) _msg_type = (msg_type);						\
	typeof(msg_resp) _msg_resp = (msg_resp);						\
	RTE_SET_USED(disp_ops->NBL_NAME(disp_op));						\
	RTE_SET_USED(res_func);								\
	RTE_SET_USED(msg_req);									\
	uint32_t _ctrl_lvl = rte_bit_relaxed_get32(ctrl_lvl2, &disp_mgt->ctrl_lvl);		\
	if (_msg_type >= 0 && _msg_resp != NULL && _ctrl_lvl)					\
		ret += chan_ops->register_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt),		\
					      _msg_type, _msg_resp, disp_mgt);			\
} while (0)
	NBL_DISP_OPS_TBL;
#undef  NBL_DISP_SET_OPS

	return ret;
}

/* Ctrl lvl means that if a certain level is set, then all disp_ops that declared this lvl
 * will go directly to res_ops, rather than send a channel msg, and vice versa.
 */
static int nbl_disp_setup_ctrl_lvl(struct nbl_dispatch_mgt *disp_mgt, u32 lvl)
{
	struct nbl_dispatch_ops *disp_ops = NBL_DISP_MGT_TO_DISP_OPS(disp_mgt);

	rte_bit_relaxed_set32(lvl, &disp_mgt->ctrl_lvl);

#define NBL_DISP_SET_OPS(disp_op, res_func, ctrl, msg_type, msg_req, msg_resp)		\
do {											\
	typeof(msg_type) _msg_type = (msg_type);					\
	typeof(msg_resp) _msg_resp = (msg_resp);					\
	RTE_SET_USED(_msg_type);							\
	RTE_SET_USED(_msg_resp);							\
	disp_ops->NBL_NAME(disp_op) =							\
		rte_bit_relaxed_get32(ctrl, &disp_mgt->ctrl_lvl) ? res_func : msg_req; ;\
} while (0)
	NBL_DISP_OPS_TBL;
#undef  NBL_DISP_SET_OPS

	return 0;
}

static int nbl_disp_setup_disp_mgt(struct nbl_dispatch_mgt **disp_mgt)
{
	*disp_mgt = rte_zmalloc("nbl_disp_mgt", sizeof(struct nbl_dispatch_mgt), 0);
	if (!*disp_mgt)
		return -ENOMEM;

	return 0;
}

static void nbl_disp_remove_disp_mgt(struct nbl_dispatch_mgt **disp_mgt)
{
	rte_free(*disp_mgt);
	*disp_mgt = NULL;
}

static void nbl_disp_remove_ops(struct nbl_dispatch_ops_tbl **disp_ops_tbl)
{
	free(NBL_DISP_OPS_TBL_TO_OPS(*disp_ops_tbl));
	free(*disp_ops_tbl);
	*disp_ops_tbl = NULL;
}

static int nbl_disp_setup_ops(struct nbl_dispatch_ops_tbl **disp_ops_tbl,
			      struct nbl_dispatch_mgt *disp_mgt)
{
	struct nbl_dispatch_ops *disp_ops;

	*disp_ops_tbl = calloc(1, sizeof(struct nbl_dispatch_ops_tbl));
	if (!*disp_ops_tbl)
		return -ENOMEM;

	disp_ops = calloc(1, sizeof(struct nbl_dispatch_ops));
	if (!disp_ops) {
		free(*disp_ops_tbl);
		return -ENOMEM;
	}

	NBL_DISP_OPS_TBL_TO_OPS(*disp_ops_tbl) = disp_ops;
	NBL_DISP_OPS_TBL_TO_PRIV(*disp_ops_tbl) = disp_mgt;

	return 0;
}

int nbl_disp_init(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_dispatch_mgt **disp_mgt;
	struct nbl_dispatch_ops_tbl **disp_ops_tbl;
	struct nbl_resource_ops_tbl *res_ops_tbl;
	struct nbl_channel_ops_tbl *chan_ops_tbl;
	struct nbl_product_dispatch_ops *disp_product_ops = NULL;
	int ret = 0;

	disp_mgt = (struct nbl_dispatch_mgt **)&NBL_ADAPTER_TO_DISP_MGT(adapter);
	disp_ops_tbl = &NBL_ADAPTER_TO_DISP_OPS_TBL(adapter);
	res_ops_tbl = NBL_ADAPTER_TO_RES_OPS_TBL(adapter);
	chan_ops_tbl = NBL_ADAPTER_TO_CHAN_OPS_TBL(adapter);
	disp_product_ops = nbl_dispatch_get_product_ops(adapter->caps.product_type);

	ret = nbl_disp_setup_disp_mgt(disp_mgt);
	if (ret)
		return ret;

	ret = nbl_disp_setup_ops(disp_ops_tbl, *disp_mgt);
	if (ret)
		goto setup_ops_fail;

	NBL_DISP_MGT_TO_RES_OPS_TBL(*disp_mgt) = res_ops_tbl;
	NBL_DISP_MGT_TO_CHAN_OPS_TBL(*disp_mgt) = chan_ops_tbl;
	NBL_DISP_MGT_TO_DISP_OPS_TBL(*disp_mgt) = *disp_ops_tbl;
	NBL_DISP_MGT_TO_COMMON(*disp_mgt) = NBL_ADAPTER_TO_COMMON(adapter);

	if (disp_product_ops->dispatch_init) {
		ret = disp_product_ops->dispatch_init(*disp_mgt);
		if (ret)
			goto dispatch_init_fail;
	}

	ret = nbl_disp_setup_ctrl_lvl(*disp_mgt, NBL_DISP_CTRL_LVL_ALWAYS);
	if (ret)
		goto setup_ctrl_lvl_fail;
	return 0;

setup_ctrl_lvl_fail:
	disp_product_ops->dispatch_uninit(*disp_mgt);
dispatch_init_fail:
	nbl_disp_remove_ops(disp_ops_tbl);
setup_ops_fail:
	nbl_disp_remove_disp_mgt(disp_mgt);

	return ret;
}

void nbl_disp_remove(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_dispatch_mgt **disp_mgt;
	struct nbl_dispatch_ops_tbl **disp_ops_tbl;

	disp_mgt = (struct nbl_dispatch_mgt **)&NBL_ADAPTER_TO_DISP_MGT(adapter);
	disp_ops_tbl = &NBL_ADAPTER_TO_DISP_OPS_TBL(adapter);

	nbl_disp_remove_ops(disp_ops_tbl);
	nbl_disp_remove_disp_mgt(disp_mgt);
}

static int nbl_disp_leonis_init(void *p)
{
	struct nbl_dispatch_mgt *disp_mgt = (struct nbl_dispatch_mgt *)p;
	int ret;

	nbl_disp_setup_ctrl_lvl(disp_mgt, NBL_DISP_CTRL_LVL_NET);
	ret = nbl_disp_setup_msg(disp_mgt);

	return ret;
}

static int nbl_disp_leonis_uninit(void *p)
{
	RTE_SET_USED(p);
	return 0;
}

static struct nbl_product_dispatch_ops nbl_product_dispatch_ops[NBL_PRODUCT_MAX] = {
	{
		.dispatch_init		= nbl_disp_leonis_init,
		.dispatch_uninit	= nbl_disp_leonis_uninit,
	},
};

struct nbl_product_dispatch_ops *nbl_dispatch_get_product_ops(enum nbl_product_type product_type)
{
	return &nbl_product_dispatch_ops[product_type];
}
