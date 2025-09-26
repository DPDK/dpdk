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
	const struct nbl_channel_ops *chan_ops;
	struct nbl_chan_send_info chan_send;

	if (!disp_mgt)
		return -EINVAL;

	chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);

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
	const struct nbl_channel_ops *chan_ops;
	struct nbl_chan_param_alloc_txrx_queues param = {0};
	struct nbl_chan_param_alloc_txrx_queues result = {0};
	struct nbl_chan_send_info chan_send;

	chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);

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
	const struct nbl_channel_ops *chan_ops;
	struct nbl_chan_param_free_txrx_queues param = {0};
	struct nbl_chan_send_info chan_send;

	chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);

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
	const struct nbl_channel_ops *chan_ops;
	struct nbl_chan_param_setup_queue param = {0};
	struct nbl_chan_send_info chan_send;

	chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);

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
	const struct nbl_channel_ops *chan_ops;
	struct nbl_chan_param_remove_all_queues param = {0};
	struct nbl_chan_send_info chan_send;

	chan_ops = NBL_DISP_MGT_TO_CHAN_OPS(disp_mgt);

	param.vsi_id = vsi_id;

	NBL_CHAN_SEND(chan_send, 0, NBL_CHAN_MSG_REMOVE_ALL_QUEUES,
		      &param, sizeof(param), NULL, 0, 1);
	chan_ops->send_msg(NBL_DISP_MGT_TO_CHAN_PRIV(disp_mgt), &chan_send);
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
