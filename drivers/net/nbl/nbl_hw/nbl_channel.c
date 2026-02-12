/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2025 Nebulamatrix Technology Co., Ltd.
 */

#include "nbl_channel.h"

static int nbl_chan_send_ack(void *priv, struct nbl_chan_ack_info *chan_ack);
static void nbl_chan_set_queue_state(void *priv, enum nbl_chan_queue_state state, u8 set);

static void nbl_chan_init_queue_param(union nbl_chan_info *chan_info,
				      u16 num_txq_entries, u16 num_rxq_entries,
				      u16 txq_buf_size, u16 rxq_buf_size)
{
	rte_spinlock_init(&chan_info->mailbox.txq_lock);
	chan_info->mailbox.num_txq_entries = num_txq_entries;
	chan_info->mailbox.num_rxq_entries = num_rxq_entries;
	chan_info->mailbox.txq_buf_size = txq_buf_size;
	chan_info->mailbox.rxq_buf_size = rxq_buf_size;
}

static int nbl_chan_init_tx_queue(union nbl_chan_info *chan_info)
{
	struct nbl_chan_ring *txq = &chan_info->mailbox.txq;
	size_t size = chan_info->mailbox.num_txq_entries * sizeof(struct nbl_chan_tx_desc);
	int i;

	txq->desc = nbl_alloc_dma_mem(&txq->desc_mem, size);
	if (!txq->desc) {
		NBL_LOG(ERR, "Allocate DMA for chan tx descriptor ring failed");
		return -ENOMEM;
	}

	chan_info->mailbox.wait = rte_calloc("nbl_chan_wait", chan_info->mailbox.num_txq_entries,
					     sizeof(struct nbl_chan_waitqueue_head), 0);
	if (!chan_info->mailbox.wait) {
		NBL_LOG(ERR, "Allocate Txq wait_queue_head array failed");
		goto req_wait_queue_failed;
	}

	for (i = 0; i < chan_info->mailbox.num_txq_entries; i++)
		rte_atomic_store_explicit(&chan_info->mailbox.wait[i].status, NBL_MBX_STATUS_IDLE,
					  rte_memory_order_relaxed);

	size = (u64)chan_info->mailbox.num_txq_entries * (u64)chan_info->mailbox.txq_buf_size;
	txq->buf = nbl_alloc_dma_mem(&txq->buf_mem, size);
	if (!txq->buf) {
		NBL_LOG(ERR, "Allocate memory for chan tx buffer arrays failed");
		goto req_num_txq_entries;
	}

	return 0;

req_num_txq_entries:
	rte_free(chan_info->mailbox.wait);
req_wait_queue_failed:
	nbl_free_dma_mem(&txq->desc_mem);
	txq->desc = NULL;
	chan_info->mailbox.wait = NULL;

	return -ENOMEM;
}

static int nbl_chan_init_rx_queue(union nbl_chan_info *chan_info)
{
	struct nbl_chan_ring *rxq = &chan_info->mailbox.rxq;
	size_t size = chan_info->mailbox.num_rxq_entries * sizeof(struct nbl_chan_rx_desc);

	rxq->desc = nbl_alloc_dma_mem(&rxq->desc_mem, size);
	if (!rxq->desc) {
		NBL_LOG(ERR, "Allocate DMA for chan rx descriptor ring failed");
		return -ENOMEM;
	}

	size = (u64)chan_info->mailbox.num_rxq_entries * (u64)chan_info->mailbox.rxq_buf_size;
	rxq->buf = nbl_alloc_dma_mem(&rxq->buf_mem, size);
	if (!rxq->buf) {
		NBL_LOG(ERR, "Allocate memory for chan rx buffer arrays failed");
		nbl_free_dma_mem(&rxq->desc_mem);
		rxq->desc = NULL;
		return -ENOMEM;
	}

	return 0;
}

static void nbl_chan_remove_tx_queue(union nbl_chan_info *chan_info)
{
	struct nbl_chan_ring *txq = &chan_info->mailbox.txq;

	nbl_free_dma_mem(&txq->buf_mem);
	txq->buf = NULL;

	rte_free(chan_info->mailbox.wait);
	chan_info->mailbox.wait = NULL;

	nbl_free_dma_mem(&txq->desc_mem);
	txq->desc = NULL;
}

static void nbl_chan_remove_rx_queue(union nbl_chan_info *chan_info)
{
	struct nbl_chan_ring *rxq = &chan_info->mailbox.rxq;

	nbl_free_dma_mem(&rxq->buf_mem);
	rxq->buf = NULL;

	nbl_free_dma_mem(&rxq->desc_mem);
	rxq->desc = NULL;
}

static int nbl_chan_init_queue(union nbl_chan_info *chan_info)
{
	int err;

	err = nbl_chan_init_tx_queue(chan_info);
	if (err)
		return err;

	err = nbl_chan_init_rx_queue(chan_info);
	if (err)
		goto setup_rx_queue_err;

	return 0;

setup_rx_queue_err:
	nbl_chan_remove_tx_queue(chan_info);
	return err;
}

static void nbl_chan_config_queue(struct nbl_channel_mgt *chan_mgt,
				  union nbl_chan_info *chan_info)
{
	const struct nbl_hw_ops *hw_ops;
	struct nbl_chan_ring *rxq = &chan_info->mailbox.rxq;
	struct nbl_chan_ring *txq = &chan_info->mailbox.txq;
	int size_bwid = rte_log2_u32(chan_info->mailbox.num_rxq_entries);

	hw_ops = NBL_CHAN_MGT_TO_HW_OPS(chan_mgt);

	hw_ops->config_mailbox_rxq(NBL_CHAN_MGT_TO_HW_PRIV(chan_mgt),
				    rxq->desc_mem.pa, size_bwid);
	hw_ops->config_mailbox_txq(NBL_CHAN_MGT_TO_HW_PRIV(chan_mgt),
				    txq->desc_mem.pa, size_bwid);
}

#define NBL_UPDATE_QUEUE_TAIL_PTR(hw_ops, chan_mgt, tail_ptr, qid)			\
do {											\
	typeof(hw_ops) _hw_ops = (hw_ops);						\
	typeof(chan_mgt) _chan_mgt = (chan_mgt);					\
	typeof(tail_ptr) _tail_ptr = (tail_ptr);					\
	typeof(qid) _qid = (qid);							\
	(_hw_ops)->update_mailbox_queue_tail_ptr(NBL_CHAN_MGT_TO_HW_PRIV(_chan_mgt),	\
							_tail_ptr, _qid);		\
} while (0)

static int nbl_chan_prepare_rx_bufs(struct nbl_channel_mgt *chan_mgt,
				    union nbl_chan_info *chan_info)
{
	const struct nbl_hw_ops *hw_ops;
	struct nbl_chan_ring *rxq = &chan_info->mailbox.rxq;
	struct nbl_chan_rx_desc *desc;
	void *hw_priv;
	u16 rx_tail_ptr;
	u32 retry_times = 0;
	u16 i;

	hw_ops = NBL_CHAN_MGT_TO_HW_OPS(chan_mgt);
	desc = rxq->desc;
	for (i = 0; i < chan_info->mailbox.num_rxq_entries - 1; i++) {
		desc[i].flags = NBL_CHAN_RX_DESC_AVAIL;
		desc[i].buf_addr = rxq->buf_mem.pa + (u64)i * (u64)chan_info->mailbox.rxq_buf_size;
		desc[i].buf_len = chan_info->mailbox.rxq_buf_size;
	}

	rxq->next_to_clean = 0;
	rxq->next_to_use = chan_info->mailbox.num_rxq_entries - 1;
	rxq->tail_ptr = chan_info->mailbox.num_rxq_entries - 1;
	rte_mb();

	NBL_UPDATE_QUEUE_TAIL_PTR(hw_ops, chan_mgt, rxq->tail_ptr, NBL_MB_RX_QID);

	while (retry_times < 100) {
		hw_priv = NBL_CHAN_MGT_TO_HW_PRIV(chan_mgt);

		rx_tail_ptr = hw_ops->get_mailbox_rx_tail_ptr(hw_priv);

		if (rx_tail_ptr != rxq->tail_ptr)
			NBL_UPDATE_QUEUE_TAIL_PTR(hw_ops, chan_mgt, rxq->tail_ptr, NBL_MB_RX_QID);
		else
			break;

		rte_delay_us(NBL_CHAN_TX_WAIT_US * 50);
		retry_times++;
	}

	return 0;
}

static void nbl_chan_stop_queue(struct nbl_channel_mgt *chan_mgt)
{
	const struct nbl_hw_ops *hw_ops;

	hw_ops = NBL_CHAN_MGT_TO_HW_OPS(chan_mgt);

	hw_ops->stop_mailbox_rxq(NBL_CHAN_MGT_TO_HW_PRIV(chan_mgt));
	hw_ops->stop_mailbox_txq(NBL_CHAN_MGT_TO_HW_PRIV(chan_mgt));
}

static void nbl_chan_remove_queue(union nbl_chan_info *chan_info)
{
	nbl_chan_remove_tx_queue(chan_info);
	nbl_chan_remove_rx_queue(chan_info);
}

static int nbl_chan_kick_tx_ring(struct nbl_channel_mgt *chan_mgt,
				 union nbl_chan_info *chan_info)
{
	const struct nbl_hw_ops *hw_ops;
	struct nbl_chan_ring *txq;
	struct nbl_chan_tx_desc *tx_desc;
	int i;

	hw_ops = NBL_CHAN_MGT_TO_HW_OPS(chan_mgt);

	txq = &chan_info->mailbox.txq;
	rte_mb();

	NBL_UPDATE_QUEUE_TAIL_PTR(hw_ops, chan_mgt, txq->tail_ptr, NBL_MB_TX_QID);

	tx_desc = NBL_CHAN_TX_DESC(txq, txq->next_to_clean);

	i = 0;
	while (!(tx_desc->flags & NBL_CHAN_TX_DESC_USED)) {
		rte_delay_us(NBL_CHAN_TX_WAIT_US);
		i++;

		if (!(i % NBL_CHAN_TX_REKICK_WAIT_TIMES))
			NBL_UPDATE_QUEUE_TAIL_PTR(hw_ops, chan_mgt, txq->tail_ptr, NBL_MB_TX_QID);

		if (i == NBL_CHAN_TX_WAIT_TIMES) {
			NBL_LOG(ERR, "chan send message type: %d timeout",
				tx_desc->msg_type);
			return -1;
		}
	}

	txq->next_to_clean = txq->next_to_use;
	return 0;
}

static void nbl_chan_recv_ack_msg(void *priv, uint16_t srcid, uint16_t msgid,
				  void *data, uint32_t data_len)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NULL;
	struct nbl_chan_waitqueue_head *wait_head;
	uint32_t *payload = (uint32_t *)data;
	uint32_t ack_msgid;
	uint32_t ack_msgtype;
	uint32_t copy_len;
	int status = NBL_MBX_STATUS_WAITING;

	chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	ack_msgtype = *payload;
	ack_msgid = *(payload + 1);

	if (ack_msgid >= chan_mgt->chan_info->mailbox.num_txq_entries) {
		NBL_LOG(ERR, "Invalid msg id %u, msg type %u", ack_msgid, ack_msgtype);
		return;
	}

	wait_head = &chan_info->mailbox.wait[ack_msgid];
	if (wait_head->msg_type != ack_msgtype) {
		NBL_LOG(ERR, "Invalid msg id %u, msg type %u, wait msg type %u",
			ack_msgid, ack_msgtype, wait_head->msg_type);
		return;
	}

	if (!rte_atomic_compare_exchange_strong_explicit(&wait_head->status, &status,
							 NBL_MBX_STATUS_ACKING,
							 rte_memory_order_acq_rel,
							 rte_memory_order_relaxed)) {
		NBL_LOG(ERR, "Invalid wait status %d msg id %u, msg type %u",
			wait_head->status, ack_msgid, ack_msgtype);
		return;
	}

	wait_head->ack_err = *(payload + 2);

	NBL_LOG(DEBUG, "recv ack, srcid:%u, msgtype:%u, msgid:%u, ack_msgid:%u,"
		" data_len:%u, ack_data_len:%u, ack_err:%d",
		srcid, ack_msgtype, msgid, ack_msgid, data_len,
		wait_head->ack_data_len, wait_head->ack_err);

	if (wait_head->ack_err >= 0 && (data_len > 3 * sizeof(uint32_t))) {
		/* the mailbox msg parameter structure may change */
		if (data_len - 3 * sizeof(uint32_t) != wait_head->ack_data_len)
			NBL_LOG(INFO, "payload_len does not match ack_len!,"
				" srcid:%u, msgtype:%u, msgid:%u, ack_msgid %u,"
				" data_len:%u, ack_data_len:%u",
				srcid, ack_msgtype, msgid,
				ack_msgid, data_len, wait_head->ack_data_len);
		copy_len = RTE_MIN(wait_head->ack_data_len, data_len - 3 * sizeof(uint32_t));
		if (copy_len)
			memcpy(wait_head->ack_data, payload + 3, copy_len);
	}

	rte_atomic_store_explicit(&wait_head->status, NBL_MBX_STATUS_ACKED,
				  rte_memory_order_release);
}

static void nbl_chan_recv_msg(struct nbl_channel_mgt *chan_mgt, void *data)
{
	struct nbl_chan_ack_info chan_ack;
	struct nbl_chan_tx_desc *tx_desc;
	struct nbl_chan_msg_handler *msg_handler;
	u16 msg_type, payload_len, srcid, msgid;
	void *payload;

	tx_desc = data;
	msg_type = tx_desc->msg_type;

	srcid = tx_desc->srcid;
	msgid = tx_desc->msgid;
	if (msg_type >= NBL_CHAN_MSG_MAX) {
		NBL_LOG(ERR, "Invalid chan message type %hu", msg_type);
		return;
	}

	if (tx_desc->data_len) {
		payload = (void *)tx_desc->data;
		payload_len = tx_desc->data_len;
	} else {
		payload = (void *)(tx_desc + 1);
		payload_len = tx_desc->buf_len;
	}

	msg_handler = &chan_mgt->msg_handler[msg_type];
	if (!msg_handler->func) {
		NBL_CHAN_ACK(chan_ack, srcid, msg_type, msgid, -EPERM, NULL, 0);
		nbl_chan_send_ack(chan_mgt, &chan_ack);
		NBL_LOG(ERR, "msg:%u no func, check af-driver is ok", msg_type);
		return;
	}

	msg_handler->func(msg_handler->priv, srcid, msgid, payload, payload_len);
}

static void nbl_chan_advance_rx_ring(struct nbl_channel_mgt *chan_mgt,
				     union nbl_chan_info *chan_info,
				     struct nbl_chan_ring *rxq)
{
	const struct nbl_hw_ops *hw_ops;
	struct nbl_chan_rx_desc *rx_desc;
	u16 next_to_use;

	hw_ops = NBL_CHAN_MGT_TO_HW_OPS(chan_mgt);

	next_to_use = rxq->next_to_use;
	rx_desc = NBL_CHAN_RX_DESC(rxq, next_to_use);

	rx_desc->flags = NBL_CHAN_RX_DESC_AVAIL;
	rx_desc->buf_addr = rxq->buf_mem.pa +
				(u64)chan_info->mailbox.rxq_buf_size * (u64)next_to_use;
	rx_desc->buf_len = chan_info->mailbox.rxq_buf_size;

	rte_wmb();
	rxq->next_to_use++;
	if (rxq->next_to_use == chan_info->mailbox.num_rxq_entries)
		rxq->next_to_use = 0;
	rxq->tail_ptr++;

	NBL_UPDATE_QUEUE_TAIL_PTR(hw_ops, chan_mgt, rxq->tail_ptr, NBL_MB_RX_QID);
}

static void nbl_chan_clean_queue(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	struct nbl_chan_ring *rxq = &chan_info->mailbox.rxq;
	struct nbl_chan_rx_desc *rx_desc;
	u8 *data;
	u16 next_to_clean;

	next_to_clean = rxq->next_to_clean;
	rx_desc = NBL_CHAN_RX_DESC(rxq, next_to_clean);
	data = (u8 *)rxq->buf + (u64)next_to_clean * (u64)chan_info->mailbox.rxq_buf_size;
	while (rx_desc->flags & NBL_CHAN_RX_DESC_USED) {
		rte_rmb();
		nbl_chan_recv_msg(chan_mgt, data);

		nbl_chan_advance_rx_ring(chan_mgt, chan_info, rxq);

		next_to_clean++;
		if (next_to_clean == chan_info->mailbox.num_rxq_entries)
			next_to_clean = 0;
		rx_desc = NBL_CHAN_RX_DESC(rxq, next_to_clean);
		data = (u8 *)rxq->buf + (u64)next_to_clean * (u64)chan_info->mailbox.rxq_buf_size;
	}
	rxq->next_to_clean = next_to_clean;
}

static uint16_t nbl_chan_update_txqueue(union nbl_chan_info *chan_info,
					uint16_t dstid,
					enum nbl_chan_msg_type msg_type,
					void *arg, size_t arg_len)
{
	struct nbl_chan_ring *txq;
	struct nbl_chan_tx_desc *tx_desc;
	struct nbl_chan_waitqueue_head *wait_head;
	uint64_t pa;
	void *va;
	uint16_t next_to_use;
	int status;

	if (arg_len > NBL_CHAN_BUF_LEN - sizeof(*tx_desc)) {
		NBL_LOG(ERR, "arg_len: %zu, too long!", arg_len);
		return 0xFFFF;
	}

	txq = &chan_info->mailbox.txq;
	next_to_use = txq->next_to_use;

	wait_head = &chan_info->mailbox.wait[next_to_use];
	status = rte_atomic_load_explicit(&wait_head->status, rte_memory_order_acquire);
	if (status != NBL_MBX_STATUS_IDLE && status != NBL_MBX_STATUS_TIMEOUT) {
		NBL_LOG(ERR, "too much inflight mailbox msg, msg id %u", next_to_use);
		return 0xFFFF;
	}

	va = (u8 *)txq->buf + (u64)next_to_use * (u64)chan_info->mailbox.txq_buf_size;
	pa = txq->buf_mem.pa + (u64)next_to_use * (u64)chan_info->mailbox.txq_buf_size;
	tx_desc = NBL_CHAN_TX_DESC(txq, next_to_use);

	tx_desc->dstid = dstid;
	tx_desc->msg_type = msg_type;
	tx_desc->msgid = next_to_use;

	if (arg_len > NBL_CHAN_TX_DESC_EMBEDDED_DATA_LEN) {
		memcpy(va, arg, arg_len);
		tx_desc->buf_addr = pa;
		tx_desc->buf_len = arg_len;
		tx_desc->data_len = 0;
	} else {
		memcpy(tx_desc->data, arg, arg_len);
		tx_desc->buf_len = 0;
		tx_desc->data_len = arg_len;
	}
	tx_desc->flags = NBL_CHAN_TX_DESC_AVAIL;

	rte_wmb();
	txq->next_to_use++;
	if (txq->next_to_use == chan_info->mailbox.num_txq_entries)
		txq->next_to_use = 0;
	txq->tail_ptr++;

	return next_to_use;
}

static int nbl_chan_send_msg(void *priv, struct nbl_chan_send_info *chan_send)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NULL;
	struct nbl_chan_waitqueue_head *wait_head;
	uint16_t msgid;
	int ret;
	int retry_time = 0;
	int status = NBL_MBX_STATUS_WAITING;

	if (chan_mgt->state)
		return -EIO;

	chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);

	rte_spinlock_lock(&chan_info->mailbox.txq_lock);
	msgid = nbl_chan_update_txqueue(chan_info, chan_send->dstid,
					chan_send->msg_type,
					chan_send->arg, chan_send->arg_len);

	if (msgid == 0xFFFF) {
		rte_spinlock_unlock(&chan_info->mailbox.txq_lock);
		NBL_LOG(ERR, "chan tx queue full, send msgtype:%u to dstid:%u failed",
			chan_send->msg_type, chan_send->dstid);
		return -ECOMM;
	}

	if (!chan_send->ack) {
		ret = nbl_chan_kick_tx_ring(chan_mgt, chan_info);
		rte_spinlock_unlock(&chan_info->mailbox.txq_lock);
		return ret;
	}

	wait_head = &chan_info->mailbox.wait[msgid];
	wait_head->ack_data = chan_send->resp;
	wait_head->ack_data_len = chan_send->resp_len;
	wait_head->msg_type = chan_send->msg_type;
	rte_atomic_store_explicit(&wait_head->status, NBL_MBX_STATUS_WAITING,
				  rte_memory_order_release);
	nbl_chan_kick_tx_ring(chan_mgt, chan_info);
	rte_spinlock_unlock(&chan_info->mailbox.txq_lock);

	while (1) {
		if (rte_atomic_load_explicit(&wait_head->status, rte_memory_order_acquire) ==
		    NBL_MBX_STATUS_ACKED) {
			ret = wait_head->ack_err;
			rte_atomic_store_explicit(&wait_head->status, NBL_MBX_STATUS_IDLE,
						  rte_memory_order_release);
			return ret;
		}

		rte_delay_us(50);
		retry_time++;
		if (retry_time > NBL_CHAN_RETRY_TIMES &&
		    rte_atomic_compare_exchange_strong_explicit(&wait_head->status,
								&status, NBL_MBX_STATUS_TIMEOUT,
								rte_memory_order_acq_rel,
								rte_memory_order_relaxed)) {
			NBL_LOG(ERR, "send msgtype:%u msgid %u to dstid:%u timeout",
				chan_send->msg_type, msgid, chan_send->dstid);
			break;
		}
	}
	return -EIO;
}

static int nbl_chan_send_ack(void *priv, struct nbl_chan_ack_info *chan_ack)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	struct nbl_chan_send_info chan_send;
	u32 *tmp;
	u32 len = 3 * sizeof(u32) + chan_ack->data_len;

	tmp = calloc(1, len);
	if (!tmp) {
		NBL_LOG(ERR, "Chan send ack data malloc failed");
		return -ENOMEM;
	}

	tmp[0] = chan_ack->msg_type;
	tmp[1] = chan_ack->msgid;
	tmp[2] = (u32)chan_ack->err;
	if (chan_ack->data && chan_ack->data_len)
		memcpy(&tmp[3], chan_ack->data, chan_ack->data_len);

	NBL_CHAN_SEND(chan_send, chan_ack->dstid, NBL_CHAN_MSG_ACK, tmp, len, NULL, 0, 0);
	nbl_chan_send_msg(chan_mgt, &chan_send);
	free(tmp);

	return 0;
}

static int nbl_chan_register_msg(void *priv, uint16_t msg_type, nbl_chan_resp func,
				 void *callback_priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;

	chan_mgt->msg_handler[msg_type].priv = callback_priv;
	chan_mgt->msg_handler[msg_type].func = func;

	return 0;
}

static uint32_t nbl_chan_thread_polling_task(void *param)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)param;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	struct timespec time;
	char unused[16];
	ssize_t nr = 0;

	time.tv_sec = 0;
	time.tv_nsec = 100000;

	while (true) {
		if (rte_bitmap_get(chan_info->mailbox.state_bmp, NBL_CHAN_INTERRUPT_READY)) {
			nr = read(chan_info->mailbox.fd[0], &unused, sizeof(unused));
			if (nr <= 0)
				break;
		} else if (rte_bitmap_get(chan_info->mailbox.state_bmp, NBL_CHAN_TEARDOWN)) {
			break;
		}

		nbl_chan_clean_queue(chan_mgt);
		nanosleep(&time, 0);
	}

	return 0;
}

static int nbl_chan_task_init(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	int ret = 0;

	ret = pipe(chan_info->mailbox.fd);
	if (ret) {
		NBL_LOG(ERR, "pipe failed, ret %d", ret);
		return ret;
	}

	ret = rte_thread_create_internal_control(&chan_info->mailbox.tid, "nbl_mailbox_thread",
						 nbl_chan_thread_polling_task, chan_mgt);
	if (ret) {
		NBL_LOG(ERR, "create mailbox thread failed, ret %d", ret);
		return ret;
	}

	return 0;
}

static int nbl_chan_task_finish(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);

	/* set teardown state to cause mailbox thread to exit */
	nbl_chan_set_queue_state(priv, NBL_CHAN_TEARDOWN, true);
	close(chan_info->mailbox.fd[0]);
	close(chan_info->mailbox.fd[1]);
	chan_info->mailbox.fd[0] = -1;
	chan_info->mailbox.fd[1] = -1;
	rte_thread_join(chan_info->mailbox.tid, NULL);
	return 0;
}

static int nbl_chan_notify_interrupt(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	char notify_byte = 0;
	ssize_t nw = 0;

	nw = write(chan_info->mailbox.fd[1], &notify_byte, 1);
	RTE_SET_USED(nw);
	return 0;
}

static int nbl_chan_teardown_queue(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);

	nbl_chan_task_finish(chan_mgt);
	nbl_chan_stop_queue(chan_mgt);
	nbl_chan_remove_queue(chan_info);

	return 0;
}

static int nbl_chan_setup_queue(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	int err;

	nbl_chan_init_queue_param(chan_info, NBL_CHAN_QUEUE_LEN,
				  NBL_CHAN_QUEUE_LEN,  NBL_CHAN_BUF_LEN,
				  NBL_CHAN_BUF_LEN);

	err = nbl_chan_init_queue(chan_info);
	if (err)
		return err;

	err = nbl_chan_task_init(chan_mgt);
	if (err)
		goto tear_down;

	nbl_chan_config_queue(chan_mgt, chan_info);

	err = nbl_chan_prepare_rx_bufs(chan_mgt, chan_info);
	if (err)
		goto tear_down;

	return 0;

tear_down:
	nbl_chan_teardown_queue(chan_mgt);
	return err;
}

static void nbl_chan_set_state(void *priv, enum nbl_chan_state state)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;

	chan_mgt->state = state;
}

static void nbl_chan_set_queue_state(void *priv, enum nbl_chan_queue_state state, u8 set)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);

	if (set)
		rte_bitmap_set(chan_info->mailbox.state_bmp, state);
	else
		rte_bitmap_clear(chan_info->mailbox.state_bmp, state);
}

const struct nbl_channel_ops nbl_chan_ops = {
	.send_msg			= nbl_chan_send_msg,
	.send_ack			= nbl_chan_send_ack,
	.register_msg			= nbl_chan_register_msg,
	.setup_queue			= nbl_chan_setup_queue,
	.teardown_queue			= nbl_chan_teardown_queue,
	.set_state			= nbl_chan_set_state,
	.set_queue_state		= nbl_chan_set_queue_state,
	.notify_interrupt		= nbl_chan_notify_interrupt,
};

static int nbl_chan_userdev_send_msg(void *priv, struct nbl_chan_send_info *chan_send)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	struct nbl_common_info *common = NBL_CHAN_MGT_TO_COMMON(chan_mgt);
	struct nbl_dev_user_channel_msg msg;
	uint32_t *result;
	int ret;

	if (chan_mgt->state)
		return -EIO;

	msg.msg_type = chan_send->msg_type;
	msg.dst_id = chan_send->dstid;
	msg.arg_len = chan_send->arg_len;
	msg.ack = chan_send->ack;
	msg.ack_length = chan_send->resp_len;
	memcpy(&msg.data, chan_send->arg, chan_send->arg_len);

	ret = ioctl(common->devfd, NBL_DEV_USER_CHANNEL, &msg);
	if (ret) {
		NBL_LOG(ERR, "user mailbox failed, type %u, ret %d", msg.msg_type, ret);
		return -1;
	}

	/* 4bytes align */
	result = (uint32_t *)RTE_PTR_ALIGN(((unsigned char *)msg.data) + chan_send->arg_len, 4);
	memcpy(chan_send->resp, result, RTE_MIN(chan_send->resp_len, msg.ack_length));

	return msg.ack_err;
}

static int nbl_chan_userdev_send_ack(void *priv, struct nbl_chan_ack_info *chan_ack)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	struct nbl_chan_send_info chan_send;
	u32 *tmp;
	u32 len = 3 * sizeof(u32) + chan_ack->data_len;

	tmp = rte_zmalloc("nbl_chan_send_tmp", len, 0);
	if (!tmp) {
		NBL_LOG(ERR, "Chan send ack data malloc failed");
		return -ENOMEM;
	}

	tmp[0] = chan_ack->msg_type;
	tmp[1] = chan_ack->msgid;
	tmp[2] = (u32)chan_ack->err;
	if (chan_ack->data && chan_ack->data_len)
		memcpy(&tmp[3], chan_ack->data, chan_ack->data_len);

	NBL_CHAN_SEND(chan_send, chan_ack->dstid, NBL_CHAN_MSG_ACK, tmp, len, NULL, 0, 0);
	nbl_chan_userdev_send_msg(chan_mgt, &chan_send);
	rte_free(tmp);

	return 0;
}

static void nbl_chan_userdev_eventfd_handler(void *cn_arg)
{
	size_t page_size = rte_mem_page_size();
	char *bak_buf = malloc(page_size);
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)cn_arg;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	char *data = (char *)chan_info->userdev.shm_msg_ring + 8;
	char *payload;
	u64 buf;
	int nbytes __rte_unused;
	u32 total_len;
	u32 *head = (u32 *)chan_info->userdev.shm_msg_ring;
	u32 *tail = (u32 *)chan_info->userdev.shm_msg_ring + 1, tmp_tail;
	u32 shmmsgbuf_size = page_size - 8;

	if (!bak_buf) {
		NBL_LOG(ERR, "nbl chan handler malloc failed");
		return;
	}
	tmp_tail = *tail;
	nbytes = read(chan_info->userdev.eventfd, &buf, sizeof(buf));

	while (*head != tmp_tail) {
		total_len = *(u32 *)(data + tmp_tail);
		if (tmp_tail + total_len > shmmsgbuf_size) {
			u32 copy_len;

			copy_len = shmmsgbuf_size - tmp_tail;
			memcpy(bak_buf, data + tmp_tail, copy_len);
			memcpy(bak_buf + copy_len, data, total_len - copy_len);
			payload = bak_buf;

		} else {
			payload  = (data + tmp_tail);
		}

		nbl_chan_recv_msg(chan_mgt, payload + 4);
		tmp_tail += total_len;
		if (tmp_tail >= shmmsgbuf_size)
			tmp_tail -= shmmsgbuf_size;
	}

	free(bak_buf);
	*tail = tmp_tail;
}

static int nbl_chan_userdev_setup_queue(void *priv)
{
	size_t page_size = rte_mem_page_size();
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	struct nbl_common_info *common = NBL_CHAN_MGT_TO_COMMON(chan_mgt);
	int ret;

	if (common->devfd < 0 || common->eventfd < 0)
		return -EINVAL;

	chan_info->userdev.eventfd = common->eventfd;
	chan_info->userdev.intr_handle.fd = common->eventfd;
	chan_info->userdev.intr_handle.type = RTE_INTR_HANDLE_EXT;

	ret = rte_intr_callback_register(&chan_info->userdev.intr_handle,
					 nbl_chan_userdev_eventfd_handler, chan_mgt);

	if (ret) {
		NBL_LOG(ERR, "channel userdev event handler register failed, %d", ret);
		return ret;
	}

	chan_info->userdev.shm_msg_ring = rte_mem_map(NULL, page_size,
						      RTE_PROT_READ | RTE_PROT_WRITE,
						      RTE_MAP_SHARED, common->devfd,
						      NBL_DEV_USER_INDEX_TO_OFFSET
						      (NBL_DEV_SHM_MSG_RING_INDEX));
	if (!chan_info->userdev.shm_msg_ring) {
		rte_intr_callback_unregister(&chan_info->userdev.intr_handle,
					     nbl_chan_userdev_eventfd_handler, chan_mgt);
		return -EINVAL;
	}

	return 0;
}

static int nbl_chan_userdev_teardown_queue(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);

	rte_mem_unmap(chan_info->userdev.shm_msg_ring, rte_mem_page_size());
	rte_intr_callback_unregister(&chan_info->userdev.intr_handle,
				     nbl_chan_userdev_eventfd_handler, chan_mgt);

	return 0;
}

static int nbl_chan_userdev_register_msg(void *priv, uint16_t msg_type, nbl_chan_resp func,
					 void *callback_priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	struct nbl_common_info *common = NBL_CHAN_MGT_TO_COMMON(chan_mgt);
	int ret, type;

	type = msg_type;
	nbl_chan_register_msg(priv, msg_type, func, callback_priv);
	ret = ioctl(common->devfd, NBL_DEV_USER_SET_LISTENER, &type);

	return ret;
}

const struct nbl_channel_ops nbl_userdev_ops = {
	.send_msg			= nbl_chan_userdev_send_msg,
	.send_ack			= nbl_chan_userdev_send_ack,
	.register_msg			= nbl_chan_userdev_register_msg,
	.setup_queue			= nbl_chan_userdev_setup_queue,
	.teardown_queue			= nbl_chan_userdev_teardown_queue,
	.set_state			= nbl_chan_set_state,
};

static int nbl_chan_init_state_bitmap(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);
	int n_bits = NBL_CHAN_STATE_NBITS;
	uint32_t cnt_bmp_size;
	void *state_bmp_mem;
	struct rte_bitmap *state_bmp;

	cnt_bmp_size = rte_bitmap_get_memory_footprint(n_bits);
	state_bmp_mem = rte_zmalloc("nbl_state_bitmap", cnt_bmp_size, 0);
	if (!state_bmp_mem) {
		NBL_LOG(ERR, "alloc nbl_state_bitmap mem failed");
		return -ENOMEM;
	}
	state_bmp = rte_bitmap_init(n_bits, state_bmp_mem, cnt_bmp_size);
	if (!state_bmp) {
		NBL_LOG(ERR, "state bitmap init failed");
		rte_free(state_bmp_mem);
		state_bmp_mem = NULL;
		return -ENOMEM;
	}
	chan_info->mailbox.state_bmp_mem = state_bmp_mem;
	chan_info->mailbox.state_bmp = state_bmp;
	return 0;
}

static void nbl_chan_remove_state_bitmap(void *priv)
{
	struct nbl_channel_mgt *chan_mgt = (struct nbl_channel_mgt *)priv;
	union nbl_chan_info *chan_info = NBL_CHAN_MGT_TO_CHAN_INFO(chan_mgt);

	if (chan_info->mailbox.state_bmp) {
		rte_bitmap_free(chan_info->mailbox.state_bmp);
		chan_info->mailbox.state_bmp = NULL;
	}
	if (chan_info->mailbox.state_bmp_mem) {
		rte_free(chan_info->mailbox.state_bmp_mem);
		chan_info->mailbox.state_bmp_mem = NULL;
	}
}

static int nbl_chan_setup_chan_mgt(struct nbl_adapter *adapter,
				   struct nbl_channel_mgt_leonis **chan_mgt_leonis)
{
	struct nbl_hw_ops_tbl *hw_ops_tbl;
	union nbl_chan_info *mailbox;
	int ret = 0;

	hw_ops_tbl = NBL_ADAPTER_TO_HW_OPS_TBL(adapter);

	*chan_mgt_leonis = rte_zmalloc("nbl_chan_mgt", sizeof(struct nbl_channel_mgt_leonis), 0);
	if (!*chan_mgt_leonis)
		goto alloc_channel_mgt_leonis_fail;

	(*chan_mgt_leonis)->chan_mgt.hw_ops_tbl = hw_ops_tbl;

	mailbox = rte_zmalloc("nbl_mailbox", sizeof(union nbl_chan_info), 0);
	if (!mailbox)
		goto alloc_mailbox_fail;

	NBL_CHAN_MGT_TO_CHAN_INFO(&(*chan_mgt_leonis)->chan_mgt) = mailbox;
	NBL_CHAN_MGT_TO_COMMON(&(*chan_mgt_leonis)->chan_mgt) = &adapter->common;

	ret = nbl_chan_init_state_bitmap(*chan_mgt_leonis);
	if (ret)
		goto state_bitmap_init_fail;

	return 0;

state_bitmap_init_fail:
alloc_mailbox_fail:
	rte_free(*chan_mgt_leonis);
alloc_channel_mgt_leonis_fail:
	return -ENOMEM;
}

static void nbl_chan_remove_chan_mgt(struct nbl_channel_mgt_leonis **chan_mgt_leonis)
{
	nbl_chan_remove_state_bitmap(*chan_mgt_leonis);
	rte_free(NBL_CHAN_MGT_TO_CHAN_INFO(&(*chan_mgt_leonis)->chan_mgt));
	rte_free(*chan_mgt_leonis);
	*chan_mgt_leonis = NULL;
}

static void nbl_chan_remove_ops(struct nbl_channel_ops_tbl **chan_ops_tbl)
{
	free(*chan_ops_tbl);
	*chan_ops_tbl = NULL;
}

static int nbl_chan_setup_ops(struct nbl_channel_ops_tbl **chan_ops_tbl,
			      struct nbl_channel_mgt_leonis *chan_mgt_leonis)
{
	struct nbl_common_info *common;

	*chan_ops_tbl = calloc(1, sizeof(struct nbl_channel_ops_tbl));
	if (!*chan_ops_tbl)
		return -ENOMEM;

	common = NBL_CHAN_MGT_TO_COMMON(&chan_mgt_leonis->chan_mgt);
	if (NBL_IS_NOT_COEXISTENCE(common))
		NBL_CHAN_OPS_TBL_TO_OPS(*chan_ops_tbl) = &nbl_chan_ops;
	else
		NBL_CHAN_OPS_TBL_TO_OPS(*chan_ops_tbl) = &nbl_userdev_ops;
	NBL_CHAN_OPS_TBL_TO_PRIV(*chan_ops_tbl) = chan_mgt_leonis;

	chan_mgt_leonis->chan_mgt.msg_handler[NBL_CHAN_MSG_ACK].func = nbl_chan_recv_ack_msg;
	chan_mgt_leonis->chan_mgt.msg_handler[NBL_CHAN_MSG_ACK].priv = chan_mgt_leonis;

	return 0;
}

int nbl_chan_init_leonis(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_channel_mgt_leonis **chan_mgt_leonis;
	struct nbl_channel_ops_tbl **chan_ops_tbl;
	int ret = 0;

	chan_mgt_leonis = (struct nbl_channel_mgt_leonis **)&NBL_ADAPTER_TO_CHAN_MGT(adapter);
	chan_ops_tbl = &NBL_ADAPTER_TO_CHAN_OPS_TBL(adapter);

	ret = nbl_chan_setup_chan_mgt(adapter, chan_mgt_leonis);
	if (ret)
		goto setup_mgt_fail;

	ret = nbl_chan_setup_ops(chan_ops_tbl, *chan_mgt_leonis);
	if (ret)
		goto setup_ops_fail;

	return 0;

setup_ops_fail:
	nbl_chan_remove_chan_mgt(chan_mgt_leonis);
setup_mgt_fail:
	return ret;
}

void nbl_chan_remove_leonis(void *p)
{
	struct nbl_adapter *adapter = (struct nbl_adapter *)p;
	struct nbl_channel_mgt_leonis **chan_mgt_leonis;
	struct nbl_channel_ops_tbl **chan_ops_tbl;

	chan_mgt_leonis = (struct nbl_channel_mgt_leonis **)&NBL_ADAPTER_TO_CHAN_MGT(adapter);
	chan_ops_tbl = &NBL_ADAPTER_TO_CHAN_OPS_TBL(adapter);

	nbl_chan_remove_chan_mgt(chan_mgt_leonis);
	nbl_chan_remove_ops(chan_ops_tbl);
}
