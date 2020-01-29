/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <assert.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <sys/mman.h>
#include <inttypes.h>

/* Verbs header. */
/* ISO C doesn't support unnamed structs/unions, disabling -pedantic. */
#ifdef PEDANTIC
#pragma GCC diagnostic ignored "-Wpedantic"
#endif
#include <infiniband/verbs.h>
#include <infiniband/mlx5dv.h>
#ifdef PEDANTIC
#pragma GCC diagnostic error "-Wpedantic"
#endif

#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <rte_ethdev_driver.h>
#include <rte_common.h>

#include <mlx5_glue.h>
#include <mlx5_devx_cmds.h>

#include "mlx5_defs.h"
#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"

/**
 * Allocate TX queue elements.
 *
 * @param txq_ctrl
 *   Pointer to TX queue structure.
 */
void
txq_alloc_elts(struct mlx5_txq_ctrl *txq_ctrl)
{
	const unsigned int elts_n = 1 << txq_ctrl->txq.elts_n;
	unsigned int i;

	for (i = 0; (i != elts_n); ++i)
		txq_ctrl->txq.elts[i] = NULL;
	DRV_LOG(DEBUG, "port %u Tx queue %u allocated and configured %u WRs",
		PORT_ID(txq_ctrl->priv), txq_ctrl->txq.idx, elts_n);
	txq_ctrl->txq.elts_head = 0;
	txq_ctrl->txq.elts_tail = 0;
	txq_ctrl->txq.elts_comp = 0;
}

/**
 * Free TX queue elements.
 *
 * @param txq_ctrl
 *   Pointer to TX queue structure.
 */
void
txq_free_elts(struct mlx5_txq_ctrl *txq_ctrl)
{
	const uint16_t elts_n = 1 << txq_ctrl->txq.elts_n;
	const uint16_t elts_m = elts_n - 1;
	uint16_t elts_head = txq_ctrl->txq.elts_head;
	uint16_t elts_tail = txq_ctrl->txq.elts_tail;
	struct rte_mbuf *(*elts)[elts_n] = &txq_ctrl->txq.elts;

	DRV_LOG(DEBUG, "port %u Tx queue %u freeing WRs",
		PORT_ID(txq_ctrl->priv), txq_ctrl->txq.idx);
	txq_ctrl->txq.elts_head = 0;
	txq_ctrl->txq.elts_tail = 0;
	txq_ctrl->txq.elts_comp = 0;

	while (elts_tail != elts_head) {
		struct rte_mbuf *elt = (*elts)[elts_tail & elts_m];

		assert(elt != NULL);
		rte_pktmbuf_free_seg(elt);
#ifndef NDEBUG
		/* Poisoning. */
		memset(&(*elts)[elts_tail & elts_m],
		       0x77,
		       sizeof((*elts)[elts_tail & elts_m]));
#endif
		++elts_tail;
	}
}

/**
 * Returns the per-port supported offloads.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   Supported Tx offloads.
 */
uint64_t
mlx5_get_tx_port_offloads(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	uint64_t offloads = (DEV_TX_OFFLOAD_MULTI_SEGS |
			     DEV_TX_OFFLOAD_VLAN_INSERT);
	struct mlx5_dev_config *config = &priv->config;

	if (config->hw_csum)
		offloads |= (DEV_TX_OFFLOAD_IPV4_CKSUM |
			     DEV_TX_OFFLOAD_UDP_CKSUM |
			     DEV_TX_OFFLOAD_TCP_CKSUM);
	if (config->tso)
		offloads |= DEV_TX_OFFLOAD_TCP_TSO;
	if (config->swp) {
		if (config->hw_csum)
			offloads |= DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;
		if (config->tso)
			offloads |= (DEV_TX_OFFLOAD_IP_TNL_TSO |
				     DEV_TX_OFFLOAD_UDP_TNL_TSO);
	}
	if (config->tunnel_en) {
		if (config->hw_csum)
			offloads |= DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;
		if (config->tso)
			offloads |= (DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
				     DEV_TX_OFFLOAD_GRE_TNL_TSO |
				     DEV_TX_OFFLOAD_GENEVE_TNL_TSO);
	}
	return offloads;
}

/**
 * Tx queue presetup checks.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Tx queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
mlx5_tx_queue_pre_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (desc <= MLX5_TX_COMP_THRESH) {
		DRV_LOG(WARNING,
			"port %u number of descriptors requested for Tx queue"
			" %u must be higher than MLX5_TX_COMP_THRESH, using %u"
			" instead of %u",
			dev->data->port_id, idx, MLX5_TX_COMP_THRESH + 1, desc);
		desc = MLX5_TX_COMP_THRESH + 1;
	}
	if (!rte_is_power_of_2(desc)) {
		desc = 1 << log2above(desc);
		DRV_LOG(WARNING,
			"port %u increased number of descriptors in Tx queue"
			" %u to the next power of two (%d)",
			dev->data->port_id, idx, desc);
	}
	DRV_LOG(DEBUG, "port %u configuring queue %u for %u descriptors",
		dev->data->port_id, idx, desc);
	if (idx >= priv->txqs_n) {
		DRV_LOG(ERR, "port %u Tx queue index out of range (%u >= %u)",
			dev->data->port_id, idx, priv->txqs_n);
		rte_errno = EOVERFLOW;
		return -rte_errno;
	}
	if (!mlx5_txq_releasable(dev, idx)) {
		rte_errno = EBUSY;
		DRV_LOG(ERR, "port %u unable to release queue index %u",
			dev->data->port_id, idx);
		return -rte_errno;
	}
	mlx5_txq_release(dev, idx);
	return 0;
}
/**
 * DPDK callback to configure a TX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *   Thresholds parameters.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_tx_queue_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		    unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	int res;

	res = mlx5_tx_queue_pre_setup(dev, idx, desc);
	if (res)
		return res;
	txq_ctrl = mlx5_txq_new(dev, idx, desc, socket, conf);
	if (!txq_ctrl) {
		DRV_LOG(ERR, "port %u unable to allocate queue index %u",
			dev->data->port_id, idx);
		return -rte_errno;
	}
	DRV_LOG(DEBUG, "port %u adding Tx queue %u to list",
		dev->data->port_id, idx);
	(*priv->txqs)[idx] = &txq_ctrl->txq;
	return 0;
}

/**
 * DPDK callback to configure a TX hairpin queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param[in] hairpin_conf
 *   The hairpin binding configuration.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_tx_hairpin_queue_setup(struct rte_eth_dev *dev, uint16_t idx,
			    uint16_t desc,
			    const struct rte_eth_hairpin_conf *hairpin_conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq, struct mlx5_txq_ctrl, txq);
	int res;

	res = mlx5_tx_queue_pre_setup(dev, idx, desc);
	if (res)
		return res;
	if (hairpin_conf->peer_count != 1 ||
	    hairpin_conf->peers[0].port != dev->data->port_id ||
	    hairpin_conf->peers[0].queue >= priv->rxqs_n) {
		DRV_LOG(ERR, "port %u unable to setup hairpin queue index %u "
			" invalid hairpind configuration", dev->data->port_id,
			idx);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	txq_ctrl = mlx5_txq_hairpin_new(dev, idx, desc,	hairpin_conf);
	if (!txq_ctrl) {
		DRV_LOG(ERR, "port %u unable to allocate queue index %u",
			dev->data->port_id, idx);
		return -rte_errno;
	}
	DRV_LOG(DEBUG, "port %u adding Tx queue %u to list",
		dev->data->port_id, idx);
	(*priv->txqs)[idx] = &txq_ctrl->txq;
	return 0;
}

/**
 * DPDK callback to release a TX queue.
 *
 * @param dpdk_txq
 *   Generic TX queue pointer.
 */
void
mlx5_tx_queue_release(void *dpdk_txq)
{
	struct mlx5_txq_data *txq = (struct mlx5_txq_data *)dpdk_txq;
	struct mlx5_txq_ctrl *txq_ctrl;
	struct mlx5_priv *priv;
	unsigned int i;

	if (txq == NULL)
		return;
	txq_ctrl = container_of(txq, struct mlx5_txq_ctrl, txq);
	priv = txq_ctrl->priv;
	for (i = 0; (i != priv->txqs_n); ++i)
		if ((*priv->txqs)[i] == txq) {
			mlx5_txq_release(ETH_DEV(priv), i);
			DRV_LOG(DEBUG, "port %u removing Tx queue %u from list",
				PORT_ID(priv), txq->idx);
			break;
		}
}

/**
 * Configure the doorbell register non-cached attribute.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 * @param page_size
 *   Systme page size
 */
static void
txq_uar_ncattr_init(struct mlx5_txq_ctrl *txq_ctrl, size_t page_size)
{
	struct mlx5_priv *priv = txq_ctrl->priv;
	off_t cmd;

	txq_ctrl->txq.db_heu = priv->config.dbnc == MLX5_TXDB_HEURISTIC;
	txq_ctrl->txq.db_nc = 0;
	/* Check the doorbell register mapping type. */
	cmd = txq_ctrl->uar_mmap_offset / page_size;
	cmd >>= MLX5_UAR_MMAP_CMD_SHIFT;
	cmd &= MLX5_UAR_MMAP_CMD_MASK;
	if (cmd == MLX5_MMAP_GET_NC_PAGES_CMD)
		txq_ctrl->txq.db_nc = 1;
}

/**
 * Initialize Tx UAR registers for primary process.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 */
static void
txq_uar_init(struct mlx5_txq_ctrl *txq_ctrl)
{
	struct mlx5_priv *priv = txq_ctrl->priv;
	struct mlx5_proc_priv *ppriv = MLX5_PROC_PRIV(PORT_ID(priv));
	const size_t page_size = sysconf(_SC_PAGESIZE);
#ifndef RTE_ARCH_64
	unsigned int lock_idx;
#endif

	if (txq_ctrl->type != MLX5_TXQ_TYPE_STANDARD)
		return;
	assert(rte_eal_process_type() == RTE_PROC_PRIMARY);
	assert(ppriv);
	ppriv->uar_table[txq_ctrl->txq.idx] = txq_ctrl->bf_reg;
	txq_uar_ncattr_init(txq_ctrl, page_size);
#ifndef RTE_ARCH_64
	/* Assign an UAR lock according to UAR page number */
	lock_idx = (txq_ctrl->uar_mmap_offset / page_size) &
		   MLX5_UAR_PAGE_NUM_MASK;
	txq_ctrl->txq.uar_lock = &priv->uar_lock[lock_idx];
#endif
}

/**
 * Remap UAR register of a Tx queue for secondary process.
 *
 * Remapped address is stored at the table in the process private structure of
 * the device, indexed by queue index.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 * @param fd
 *   Verbs file descriptor to map UAR pages.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
static int
txq_uar_init_secondary(struct mlx5_txq_ctrl *txq_ctrl, int fd)
{
	struct mlx5_priv *priv = txq_ctrl->priv;
	struct mlx5_proc_priv *ppriv = MLX5_PROC_PRIV(PORT_ID(priv));
	struct mlx5_txq_data *txq = &txq_ctrl->txq;
	void *addr;
	uintptr_t uar_va;
	uintptr_t offset;
	const size_t page_size = sysconf(_SC_PAGESIZE);

	if (txq_ctrl->type != MLX5_TXQ_TYPE_STANDARD)
		return 0;
	assert(ppriv);
	/*
	 * As rdma-core, UARs are mapped in size of OS page
	 * size. Ref to libmlx5 function: mlx5_init_context()
	 */
	uar_va = (uintptr_t)txq_ctrl->bf_reg;
	offset = uar_va & (page_size - 1); /* Offset in page. */
	addr = mmap(NULL, page_size, PROT_WRITE, MAP_SHARED, fd,
			txq_ctrl->uar_mmap_offset);
	if (addr == MAP_FAILED) {
		DRV_LOG(ERR,
			"port %u mmap failed for BF reg of txq %u",
			txq->port_id, txq->idx);
		rte_errno = ENXIO;
		return -rte_errno;
	}
	addr = RTE_PTR_ADD(addr, offset);
	ppriv->uar_table[txq->idx] = addr;
	txq_uar_ncattr_init(txq_ctrl, page_size);
	return 0;
}

/**
 * Unmap UAR register of a Tx queue for secondary process.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 */
static void
txq_uar_uninit_secondary(struct mlx5_txq_ctrl *txq_ctrl)
{
	struct mlx5_proc_priv *ppriv = MLX5_PROC_PRIV(PORT_ID(txq_ctrl->priv));
	const size_t page_size = sysconf(_SC_PAGESIZE);
	void *addr;

	if (txq_ctrl->type != MLX5_TXQ_TYPE_STANDARD)
		return;
	addr = ppriv->uar_table[txq_ctrl->txq.idx];
	munmap(RTE_PTR_ALIGN_FLOOR(addr, page_size), page_size);
}

/**
 * Initialize Tx UAR registers for secondary process.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param fd
 *   Verbs file descriptor to map UAR pages.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_tx_uar_init_secondary(struct rte_eth_dev *dev, int fd)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq;
	struct mlx5_txq_ctrl *txq_ctrl;
	unsigned int i;
	int ret;

	assert(rte_eal_process_type() == RTE_PROC_SECONDARY);
	for (i = 0; i != priv->txqs_n; ++i) {
		if (!(*priv->txqs)[i])
			continue;
		txq = (*priv->txqs)[i];
		txq_ctrl = container_of(txq, struct mlx5_txq_ctrl, txq);
		if (txq_ctrl->type != MLX5_TXQ_TYPE_STANDARD)
			continue;
		assert(txq->idx == (uint16_t)i);
		ret = txq_uar_init_secondary(txq_ctrl, fd);
		if (ret)
			goto error;
	}
	return 0;
error:
	/* Rollback. */
	do {
		if (!(*priv->txqs)[i])
			continue;
		txq = (*priv->txqs)[i];
		txq_ctrl = container_of(txq, struct mlx5_txq_ctrl, txq);
		txq_uar_uninit_secondary(txq_ctrl);
	} while (i--);
	return -rte_errno;
}

/**
 * Create the Tx hairpin queue object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array
 *
 * @return
 *   The hairpin DevX object initialised, NULL otherwise and rte_errno is set.
 */
static struct mlx5_txq_obj *
mlx5_txq_obj_hairpin_new(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq_data, struct mlx5_txq_ctrl, txq);
	struct mlx5_devx_create_sq_attr attr = { 0 };
	struct mlx5_txq_obj *tmpl = NULL;
	int ret = 0;

	assert(txq_data);
	assert(!txq_ctrl->obj);
	tmpl = rte_calloc_socket(__func__, 1, sizeof(*tmpl), 0,
				 txq_ctrl->socket);
	if (!tmpl) {
		DRV_LOG(ERR,
			"port %u Tx queue %u cannot allocate memory resources",
			dev->data->port_id, txq_data->idx);
		rte_errno = ENOMEM;
		goto error;
	}
	tmpl->type = MLX5_TXQ_OBJ_TYPE_DEVX_HAIRPIN;
	tmpl->txq_ctrl = txq_ctrl;
	attr.hairpin = 1;
	attr.tis_lst_sz = 1;
	/* Workaround for hairpin startup */
	attr.wq_attr.log_hairpin_num_packets = log2above(32);
	/* Workaround for packets larger than 1KB */
	attr.wq_attr.log_hairpin_data_sz =
			priv->config.hca_attr.log_max_hairpin_wq_data_sz;
	attr.tis_num = priv->sh->tis->id;
	tmpl->sq = mlx5_devx_cmd_create_sq(priv->sh->ctx, &attr);
	if (!tmpl->sq) {
		DRV_LOG(ERR,
			"port %u tx hairpin queue %u can't create sq object",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	DRV_LOG(DEBUG, "port %u sxq %u updated with %p", dev->data->port_id,
		idx, (void *)&tmpl);
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->txqsobj, tmpl, next);
	return tmpl;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (tmpl->tis)
		mlx5_devx_cmd_destroy(tmpl->tis);
	if (tmpl->sq)
		mlx5_devx_cmd_destroy(tmpl->sq);
	rte_errno = ret; /* Restore rte_errno. */
	return NULL;
}

/**
 * Create the Tx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 * @param type
 *   Type of the Tx queue object to create.
 *
 * @return
 *   The Verbs object initialised, NULL otherwise and rte_errno is set.
 */
struct mlx5_txq_obj *
mlx5_txq_obj_new(struct rte_eth_dev *dev, uint16_t idx,
		 enum mlx5_txq_obj_type type)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
		container_of(txq_data, struct mlx5_txq_ctrl, txq);
	struct mlx5_txq_obj tmpl;
	struct mlx5_txq_obj *txq_obj = NULL;
	union {
		struct ibv_qp_init_attr_ex init;
		struct ibv_cq_init_attr_ex cq;
		struct ibv_qp_attr mod;
	} attr;
	unsigned int cqe_n;
	struct mlx5dv_qp qp = { .comp_mask = MLX5DV_QP_MASK_UAR_MMAP_OFFSET };
	struct mlx5dv_cq cq_info;
	struct mlx5dv_obj obj;
	const int desc = 1 << txq_data->elts_n;
	int ret = 0;

	if (type == MLX5_TXQ_OBJ_TYPE_DEVX_HAIRPIN)
		return mlx5_txq_obj_hairpin_new(dev, idx);
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	/* If using DevX, need additional mask to read tisn value. */
	if (priv->config.devx && !priv->sh->tdn)
		qp.comp_mask |= MLX5DV_QP_MASK_RAW_QP_HANDLES;
#endif
	assert(txq_data);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_TX_QUEUE;
	priv->verbs_alloc_ctx.obj = txq_ctrl;
	if (mlx5_getenv_int("MLX5_ENABLE_CQE_COMPRESSION")) {
		DRV_LOG(ERR,
			"port %u MLX5_ENABLE_CQE_COMPRESSION must never be set",
			dev->data->port_id);
		rte_errno = EINVAL;
		return NULL;
	}
	memset(&tmpl, 0, sizeof(struct mlx5_txq_obj));
	attr.cq = (struct ibv_cq_init_attr_ex){
		.comp_mask = 0,
	};
	cqe_n = desc / MLX5_TX_COMP_THRESH +
		1 + MLX5_TX_COMP_THRESH_INLINE_DIV;
	tmpl.cq = mlx5_glue->create_cq(priv->sh->ctx, cqe_n, NULL, NULL, 0);
	if (tmpl.cq == NULL) {
		DRV_LOG(ERR, "port %u Tx queue %u CQ creation failure",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	attr.init = (struct ibv_qp_init_attr_ex){
		/* CQ to be associated with the send queue. */
		.send_cq = tmpl.cq,
		/* CQ to be associated with the receive queue. */
		.recv_cq = tmpl.cq,
		.cap = {
			/* Max number of outstanding WRs. */
			.max_send_wr =
				((priv->sh->device_attr.orig_attr.max_qp_wr <
				  desc) ?
				 priv->sh->device_attr.orig_attr.max_qp_wr :
				 desc),
			/*
			 * Max number of scatter/gather elements in a WR,
			 * must be 1 to prevent libmlx5 from trying to affect
			 * too much memory. TX gather is not impacted by the
			 * device_attr.max_sge limit and will still work
			 * properly.
			 */
			.max_send_sge = 1,
		},
		.qp_type = IBV_QPT_RAW_PACKET,
		/*
		 * Do *NOT* enable this, completions events are managed per
		 * Tx burst.
		 */
		.sq_sig_all = 0,
		.pd = priv->sh->pd,
		.comp_mask = IBV_QP_INIT_ATTR_PD,
	};
	if (txq_data->inlen_send)
		attr.init.cap.max_inline_data = txq_ctrl->max_inline_data;
	if (txq_data->tso_en) {
		attr.init.max_tso_header = txq_ctrl->max_tso_header;
		attr.init.comp_mask |= IBV_QP_INIT_ATTR_MAX_TSO_HEADER;
	}
	tmpl.qp = mlx5_glue->create_qp_ex(priv->sh->ctx, &attr.init);
	if (tmpl.qp == NULL) {
		DRV_LOG(ERR, "port %u Tx queue %u QP creation failure",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	attr.mod = (struct ibv_qp_attr){
		/* Move the QP to this state. */
		.qp_state = IBV_QPS_INIT,
		/* IB device port number. */
		.port_num = (uint8_t)priv->ibv_port,
	};
	ret = mlx5_glue->modify_qp(tmpl.qp, &attr.mod,
				   (IBV_QP_STATE | IBV_QP_PORT));
	if (ret) {
		DRV_LOG(ERR,
			"port %u Tx queue %u QP state to IBV_QPS_INIT failed",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	attr.mod = (struct ibv_qp_attr){
		.qp_state = IBV_QPS_RTR
	};
	ret = mlx5_glue->modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		DRV_LOG(ERR,
			"port %u Tx queue %u QP state to IBV_QPS_RTR failed",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	attr.mod.qp_state = IBV_QPS_RTS;
	ret = mlx5_glue->modify_qp(tmpl.qp, &attr.mod, IBV_QP_STATE);
	if (ret) {
		DRV_LOG(ERR,
			"port %u Tx queue %u QP state to IBV_QPS_RTS failed",
			dev->data->port_id, idx);
		rte_errno = errno;
		goto error;
	}
	txq_obj = rte_calloc_socket(__func__, 1, sizeof(struct mlx5_txq_obj), 0,
				    txq_ctrl->socket);
	if (!txq_obj) {
		DRV_LOG(ERR, "port %u Tx queue %u cannot allocate memory",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
	obj.cq.in = tmpl.cq;
	obj.cq.out = &cq_info;
	obj.qp.in = tmpl.qp;
	obj.qp.out = &qp;
	ret = mlx5_glue->dv_init_obj(&obj, MLX5DV_OBJ_CQ | MLX5DV_OBJ_QP);
	if (ret != 0) {
		rte_errno = errno;
		goto error;
	}
	if (cq_info.cqe_size != RTE_CACHE_LINE_SIZE) {
		DRV_LOG(ERR,
			"port %u wrong MLX5_CQE_SIZE environment variable"
			" value: it should be set to %u",
			dev->data->port_id, RTE_CACHE_LINE_SIZE);
		rte_errno = EINVAL;
		goto error;
	}
	txq_data->cqe_n = log2above(cq_info.cqe_cnt);
	txq_data->cqe_s = 1 << txq_data->cqe_n;
	txq_data->cqe_m = txq_data->cqe_s - 1;
	txq_data->qp_num_8s = tmpl.qp->qp_num << 8;
	txq_data->wqes = qp.sq.buf;
	txq_data->wqe_n = log2above(qp.sq.wqe_cnt);
	txq_data->wqe_s = 1 << txq_data->wqe_n;
	txq_data->wqe_m = txq_data->wqe_s - 1;
	txq_data->wqes_end = txq_data->wqes + txq_data->wqe_s;
	txq_data->qp_db = &qp.dbrec[MLX5_SND_DBR];
	txq_data->cq_db = cq_info.dbrec;
	txq_data->cqes = (volatile struct mlx5_cqe *)cq_info.buf;
	txq_data->cq_ci = 0;
	txq_data->cq_pi = 0;
	txq_data->wqe_ci = 0;
	txq_data->wqe_pi = 0;
	txq_data->wqe_comp = 0;
	txq_data->wqe_thres = txq_data->wqe_s / MLX5_TX_COMP_THRESH_INLINE_DIV;
	txq_data->fcqs = rte_calloc_socket(__func__,
					   txq_data->cqe_s,
					   sizeof(*txq_data->fcqs),
					   RTE_CACHE_LINE_SIZE,
					   txq_ctrl->socket);
	if (!txq_data->fcqs) {
		DRV_LOG(ERR, "port %u Tx queue %u cannot allocate memory (FCQ)",
			dev->data->port_id, idx);
		rte_errno = ENOMEM;
		goto error;
	}
#ifdef HAVE_IBV_FLOW_DV_SUPPORT
	/*
	 * If using DevX need to query and store TIS transport domain value.
	 * This is done once per port.
	 * Will use this value on Rx, when creating matching TIR.
	 */
	if (priv->config.devx && !priv->sh->tdn) {
		ret = mlx5_devx_cmd_qp_query_tis_td(tmpl.qp, qp.tisn,
						    &priv->sh->tdn);
		if (ret) {
			DRV_LOG(ERR, "Fail to query port %u Tx queue %u QP TIS "
				"transport domain", dev->data->port_id, idx);
			rte_errno = EINVAL;
			goto error;
		} else {
			DRV_LOG(DEBUG, "port %u Tx queue %u TIS number %d "
				"transport domain %d", dev->data->port_id,
				idx, qp.tisn, priv->sh->tdn);
		}
	}
#endif
	txq_obj->qp = tmpl.qp;
	txq_obj->cq = tmpl.cq;
	rte_atomic32_inc(&txq_obj->refcnt);
	txq_ctrl->bf_reg = qp.bf.reg;
	if (qp.comp_mask & MLX5DV_QP_MASK_UAR_MMAP_OFFSET) {
		txq_ctrl->uar_mmap_offset = qp.uar_mmap_offset;
		DRV_LOG(DEBUG, "port %u: uar_mmap_offset 0x%"PRIx64,
			dev->data->port_id, txq_ctrl->uar_mmap_offset);
	} else {
		DRV_LOG(ERR,
			"port %u failed to retrieve UAR info, invalid"
			" libmlx5.so",
			dev->data->port_id);
		rte_errno = EINVAL;
		goto error;
	}
	txq_uar_init(txq_ctrl);
	LIST_INSERT_HEAD(&priv->txqsobj, txq_obj, next);
	txq_obj->txq_ctrl = txq_ctrl;
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	return txq_obj;
error:
	ret = rte_errno; /* Save rte_errno before cleanup. */
	if (tmpl.cq)
		claim_zero(mlx5_glue->destroy_cq(tmpl.cq));
	if (tmpl.qp)
		claim_zero(mlx5_glue->destroy_qp(tmpl.qp));
	if (txq_data && txq_data->fcqs)
		rte_free(txq_data->fcqs);
	if (txq_obj)
		rte_free(txq_obj);
	priv->verbs_alloc_ctx.type = MLX5_VERBS_ALLOC_TYPE_NONE;
	rte_errno = ret; /* Restore rte_errno. */
	return NULL;
}

/**
 * Get an Tx queue Verbs object.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   Queue index in DPDK Tx queue array.
 *
 * @return
 *   The Verbs object if it exists.
 */
struct mlx5_txq_obj *
mlx5_txq_obj_get(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *txq_ctrl;

	if (idx >= priv->txqs_n)
		return NULL;
	if (!(*priv->txqs)[idx])
		return NULL;
	txq_ctrl = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl, txq);
	if (txq_ctrl->obj)
		rte_atomic32_inc(&txq_ctrl->obj->refcnt);
	return txq_ctrl->obj;
}

/**
 * Release an Tx verbs queue object.
 *
 * @param txq_obj
 *   Verbs Tx queue object.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_txq_obj_release(struct mlx5_txq_obj *txq_obj)
{
	assert(txq_obj);
	if (rte_atomic32_dec_and_test(&txq_obj->refcnt)) {
		if (txq_obj->type == MLX5_TXQ_OBJ_TYPE_DEVX_HAIRPIN) {
			if (txq_obj->tis)
				claim_zero(mlx5_devx_cmd_destroy(txq_obj->tis));
		} else {
			claim_zero(mlx5_glue->destroy_qp(txq_obj->qp));
			claim_zero(mlx5_glue->destroy_cq(txq_obj->cq));
				if (txq_obj->txq_ctrl->txq.fcqs)
					rte_free(txq_obj->txq_ctrl->txq.fcqs);
		}
		LIST_REMOVE(txq_obj, next);
		rte_free(txq_obj);
		return 0;
	}
	return 1;
}

/**
 * Verify the Verbs Tx queue list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_txq_obj_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	int ret = 0;
	struct mlx5_txq_obj *txq_obj;

	LIST_FOREACH(txq_obj, &priv->txqsobj, next) {
		DRV_LOG(DEBUG, "port %u Verbs Tx queue %u still referenced",
			dev->data->port_id, txq_obj->txq_ctrl->txq.idx);
		++ret;
	}
	return ret;
}

/**
 * Calculate the total number of WQEBB for Tx queue.
 *
 * Simplified version of calc_sq_size() in rdma-core.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 *
 * @return
 *   The number of WQEBB.
 */
static int
txq_calc_wqebb_cnt(struct mlx5_txq_ctrl *txq_ctrl)
{
	unsigned int wqe_size;
	const unsigned int desc = 1 << txq_ctrl->txq.elts_n;

	wqe_size = MLX5_WQE_CSEG_SIZE +
		   MLX5_WQE_ESEG_SIZE +
		   MLX5_WSEG_SIZE -
		   MLX5_ESEG_MIN_INLINE_SIZE +
		   txq_ctrl->max_inline_data;
	return rte_align32pow2(wqe_size * desc) / MLX5_WQE_SIZE;
}

/**
 * Calculate the maximal inline data size for Tx queue.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 *
 * @return
 *   The maximal inline data size.
 */
static unsigned int
txq_calc_inline_max(struct mlx5_txq_ctrl *txq_ctrl)
{
	const unsigned int desc = 1 << txq_ctrl->txq.elts_n;
	struct mlx5_priv *priv = txq_ctrl->priv;
	unsigned int wqe_size;

	wqe_size = priv->sh->device_attr.orig_attr.max_qp_wr / desc;
	if (!wqe_size)
		return 0;
	/*
	 * This calculation is derived from tthe source of
	 * mlx5_calc_send_wqe() in rdma_core library.
	 */
	wqe_size = wqe_size * MLX5_WQE_SIZE -
		   MLX5_WQE_CSEG_SIZE -
		   MLX5_WQE_ESEG_SIZE -
		   MLX5_WSEG_SIZE -
		   MLX5_WSEG_SIZE +
		   MLX5_DSEG_MIN_INLINE_SIZE;
	return wqe_size;
}

/**
 * Set Tx queue parameters from device configuration.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 */
static void
txq_set_params(struct mlx5_txq_ctrl *txq_ctrl)
{
	struct mlx5_priv *priv = txq_ctrl->priv;
	struct mlx5_dev_config *config = &priv->config;
	unsigned int inlen_send; /* Inline data for ordinary SEND.*/
	unsigned int inlen_empw; /* Inline data for enhanced MPW. */
	unsigned int inlen_mode; /* Minimal required Inline data. */
	unsigned int txqs_inline; /* Min Tx queues to enable inline. */
	uint64_t dev_txoff = priv->dev_data->dev_conf.txmode.offloads;
	bool tso = txq_ctrl->txq.offloads & (DEV_TX_OFFLOAD_TCP_TSO |
					    DEV_TX_OFFLOAD_VXLAN_TNL_TSO |
					    DEV_TX_OFFLOAD_GRE_TNL_TSO |
					    DEV_TX_OFFLOAD_IP_TNL_TSO |
					    DEV_TX_OFFLOAD_UDP_TNL_TSO);
	bool vlan_inline;
	unsigned int temp;

	if (config->txqs_inline == MLX5_ARG_UNSET)
		txqs_inline =
#if defined(RTE_ARCH_ARM64)
		(priv->pci_dev->id.device_id ==
			PCI_DEVICE_ID_MELLANOX_CONNECTX5BF) ?
			MLX5_INLINE_MAX_TXQS_BLUEFIELD :
#endif
			MLX5_INLINE_MAX_TXQS;
	else
		txqs_inline = (unsigned int)config->txqs_inline;
	inlen_send = (config->txq_inline_max == MLX5_ARG_UNSET) ?
		     MLX5_SEND_DEF_INLINE_LEN :
		     (unsigned int)config->txq_inline_max;
	inlen_empw = (config->txq_inline_mpw == MLX5_ARG_UNSET) ?
		     MLX5_EMPW_DEF_INLINE_LEN :
		     (unsigned int)config->txq_inline_mpw;
	inlen_mode = (config->txq_inline_min == MLX5_ARG_UNSET) ?
		     0 : (unsigned int)config->txq_inline_min;
	if (config->mps != MLX5_MPW_ENHANCED && config->mps != MLX5_MPW)
		inlen_empw = 0;
	/*
	 * If there is requested minimal amount of data to inline
	 * we MUST enable inlining. This is a case for ConnectX-4
	 * which usually requires L2 inlined for correct operating
	 * and ConnectX-4LX which requires L2-L4 inlined to
	 * support E-Switch Flows.
	 */
	if (inlen_mode) {
		if (inlen_mode <= MLX5_ESEG_MIN_INLINE_SIZE) {
			/*
			 * Optimize minimal inlining for single
			 * segment packets to fill one WQEBB
			 * without gaps.
			 */
			temp = MLX5_ESEG_MIN_INLINE_SIZE;
		} else {
			temp = inlen_mode - MLX5_ESEG_MIN_INLINE_SIZE;
			temp = RTE_ALIGN(temp, MLX5_WSEG_SIZE) +
			       MLX5_ESEG_MIN_INLINE_SIZE;
			temp = RTE_MIN(temp, MLX5_SEND_MAX_INLINE_LEN);
		}
		if (temp != inlen_mode) {
			DRV_LOG(INFO,
				"port %u minimal required inline setting"
				" aligned from %u to %u",
				PORT_ID(priv), inlen_mode, temp);
			inlen_mode = temp;
		}
	}
	/*
	 * If port is configured to support VLAN insertion and device
	 * does not support this feature by HW (for NICs before ConnectX-5
	 * or in case of wqe_vlan_insert flag is not set) we must enable
	 * data inline on all queues because it is supported by single
	 * tx_burst routine.
	 */
	txq_ctrl->txq.vlan_en = config->hw_vlan_insert;
	vlan_inline = (dev_txoff & DEV_TX_OFFLOAD_VLAN_INSERT) &&
		      !config->hw_vlan_insert;
	/*
	 * If there are few Tx queues it is prioritized
	 * to save CPU cycles and disable data inlining at all.
	 */
	if (inlen_send && priv->txqs_n >= txqs_inline) {
		/*
		 * The data sent with ordinal MLX5_OPCODE_SEND
		 * may be inlined in Ethernet Segment, align the
		 * length accordingly to fit entire WQEBBs.
		 */
		temp = RTE_MAX(inlen_send,
			       MLX5_ESEG_MIN_INLINE_SIZE + MLX5_WQE_DSEG_SIZE);
		temp -= MLX5_ESEG_MIN_INLINE_SIZE + MLX5_WQE_DSEG_SIZE;
		temp = RTE_ALIGN(temp, MLX5_WQE_SIZE);
		temp += MLX5_ESEG_MIN_INLINE_SIZE + MLX5_WQE_DSEG_SIZE;
		temp = RTE_MIN(temp, MLX5_WQE_SIZE_MAX +
				     MLX5_ESEG_MIN_INLINE_SIZE -
				     MLX5_WQE_CSEG_SIZE -
				     MLX5_WQE_ESEG_SIZE -
				     MLX5_WQE_DSEG_SIZE * 2);
		temp = RTE_MIN(temp, MLX5_SEND_MAX_INLINE_LEN);
		temp = RTE_MAX(temp, inlen_mode);
		if (temp != inlen_send) {
			DRV_LOG(INFO,
				"port %u ordinary send inline setting"
				" aligned from %u to %u",
				PORT_ID(priv), inlen_send, temp);
			inlen_send = temp;
		}
		/*
		 * Not aligned to cache lines, but to WQEs.
		 * First bytes of data (initial alignment)
		 * is going to be copied explicitly at the
		 * beginning of inlining buffer in Ethernet
		 * Segment.
		 */
		assert(inlen_send >= MLX5_ESEG_MIN_INLINE_SIZE);
		assert(inlen_send <= MLX5_WQE_SIZE_MAX +
				     MLX5_ESEG_MIN_INLINE_SIZE -
				     MLX5_WQE_CSEG_SIZE -
				     MLX5_WQE_ESEG_SIZE -
				     MLX5_WQE_DSEG_SIZE * 2);
	} else if (inlen_mode) {
		/*
		 * If minimal inlining is requested we must
		 * enable inlining in general, despite the
		 * number of configured queues. Ignore the
		 * txq_inline_max devarg, this is not
		 * full-featured inline.
		 */
		inlen_send = inlen_mode;
		inlen_empw = 0;
	} else if (vlan_inline) {
		/*
		 * Hardware does not report offload for
		 * VLAN insertion, we must enable data inline
		 * to implement feature by software.
		 */
		inlen_send = MLX5_ESEG_MIN_INLINE_SIZE;
		inlen_empw = 0;
	} else {
		inlen_send = 0;
		inlen_empw = 0;
	}
	txq_ctrl->txq.inlen_send = inlen_send;
	txq_ctrl->txq.inlen_mode = inlen_mode;
	txq_ctrl->txq.inlen_empw = 0;
	if (inlen_send && inlen_empw && priv->txqs_n >= txqs_inline) {
		/*
		 * The data sent with MLX5_OPCODE_ENHANCED_MPSW
		 * may be inlined in Data Segment, align the
		 * length accordingly to fit entire WQEBBs.
		 */
		temp = RTE_MAX(inlen_empw,
			       MLX5_WQE_SIZE + MLX5_DSEG_MIN_INLINE_SIZE);
		temp -= MLX5_DSEG_MIN_INLINE_SIZE;
		temp = RTE_ALIGN(temp, MLX5_WQE_SIZE);
		temp += MLX5_DSEG_MIN_INLINE_SIZE;
		temp = RTE_MIN(temp, MLX5_WQE_SIZE_MAX +
				     MLX5_DSEG_MIN_INLINE_SIZE -
				     MLX5_WQE_CSEG_SIZE -
				     MLX5_WQE_ESEG_SIZE -
				     MLX5_WQE_DSEG_SIZE);
		temp = RTE_MIN(temp, MLX5_EMPW_MAX_INLINE_LEN);
		if (temp != inlen_empw) {
			DRV_LOG(INFO,
				"port %u enhanced empw inline setting"
				" aligned from %u to %u",
				PORT_ID(priv), inlen_empw, temp);
			inlen_empw = temp;
		}
		assert(inlen_empw >= MLX5_ESEG_MIN_INLINE_SIZE);
		assert(inlen_empw <= MLX5_WQE_SIZE_MAX +
				     MLX5_DSEG_MIN_INLINE_SIZE -
				     MLX5_WQE_CSEG_SIZE -
				     MLX5_WQE_ESEG_SIZE -
				     MLX5_WQE_DSEG_SIZE);
		txq_ctrl->txq.inlen_empw = inlen_empw;
	}
	txq_ctrl->max_inline_data = RTE_MAX(inlen_send, inlen_empw);
	if (tso) {
		txq_ctrl->max_tso_header = MLX5_MAX_TSO_HEADER;
		txq_ctrl->max_inline_data = RTE_MAX(txq_ctrl->max_inline_data,
						    MLX5_MAX_TSO_HEADER);
		txq_ctrl->txq.tso_en = 1;
	}
	txq_ctrl->txq.tunnel_en = config->tunnel_en | config->swp;
	txq_ctrl->txq.swp_en = ((DEV_TX_OFFLOAD_IP_TNL_TSO |
				 DEV_TX_OFFLOAD_UDP_TNL_TSO |
				 DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM) &
				txq_ctrl->txq.offloads) && config->swp;
}

/**
 * Adjust Tx queue data inline parameters for large queue sizes.
 * The data inline feature requires multiple WQEs to fit the packets,
 * and if the large amount of Tx descriptors is requested by application
 * the total WQE amount may exceed the hardware capabilities. If the
 * default inline setting are used we can try to adjust these ones and
 * meet the hardware requirements and not exceed the queue size.
 *
 * @param txq_ctrl
 *   Pointer to Tx queue control structure.
 *
 * @return
 *   Zero on success, otherwise the parameters can not be adjusted.
 */
static int
txq_adjust_params(struct mlx5_txq_ctrl *txq_ctrl)
{
	struct mlx5_priv *priv = txq_ctrl->priv;
	struct mlx5_dev_config *config = &priv->config;
	unsigned int max_inline;

	max_inline = txq_calc_inline_max(txq_ctrl);
	if (!txq_ctrl->txq.inlen_send) {
		/*
		 * Inline data feature is not engaged at all.
		 * There is nothing to adjust.
		 */
		return 0;
	}
	if (txq_ctrl->max_inline_data <= max_inline) {
		/*
		 * The requested inline data length does not
		 * exceed queue capabilities.
		 */
		return 0;
	}
	if (txq_ctrl->txq.inlen_mode > max_inline) {
		DRV_LOG(ERR,
			"minimal data inline requirements (%u) are not"
			" satisfied (%u) on port %u, try the smaller"
			" Tx queue size (%d)",
			txq_ctrl->txq.inlen_mode, max_inline,
			priv->dev_data->port_id,
			priv->sh->device_attr.orig_attr.max_qp_wr);
		goto error;
	}
	if (txq_ctrl->txq.inlen_send > max_inline &&
	    config->txq_inline_max != MLX5_ARG_UNSET &&
	    config->txq_inline_max > (int)max_inline) {
		DRV_LOG(ERR,
			"txq_inline_max requirements (%u) are not"
			" satisfied (%u) on port %u, try the smaller"
			" Tx queue size (%d)",
			txq_ctrl->txq.inlen_send, max_inline,
			priv->dev_data->port_id,
			priv->sh->device_attr.orig_attr.max_qp_wr);
		goto error;
	}
	if (txq_ctrl->txq.inlen_empw > max_inline &&
	    config->txq_inline_mpw != MLX5_ARG_UNSET &&
	    config->txq_inline_mpw > (int)max_inline) {
		DRV_LOG(ERR,
			"txq_inline_mpw requirements (%u) are not"
			" satisfied (%u) on port %u, try the smaller"
			" Tx queue size (%d)",
			txq_ctrl->txq.inlen_empw, max_inline,
			priv->dev_data->port_id,
			priv->sh->device_attr.orig_attr.max_qp_wr);
		goto error;
	}
	if (txq_ctrl->txq.tso_en && max_inline < MLX5_MAX_TSO_HEADER) {
		DRV_LOG(ERR,
			"tso header inline requirements (%u) are not"
			" satisfied (%u) on port %u, try the smaller"
			" Tx queue size (%d)",
			MLX5_MAX_TSO_HEADER, max_inline,
			priv->dev_data->port_id,
			priv->sh->device_attr.orig_attr.max_qp_wr);
		goto error;
	}
	if (txq_ctrl->txq.inlen_send > max_inline) {
		DRV_LOG(WARNING,
			"adjust txq_inline_max (%u->%u)"
			" due to large Tx queue on port %u",
			txq_ctrl->txq.inlen_send, max_inline,
			priv->dev_data->port_id);
		txq_ctrl->txq.inlen_send = max_inline;
	}
	if (txq_ctrl->txq.inlen_empw > max_inline) {
		DRV_LOG(WARNING,
			"adjust txq_inline_mpw (%u->%u)"
			"due to large Tx queue on port %u",
			txq_ctrl->txq.inlen_empw, max_inline,
			priv->dev_data->port_id);
		txq_ctrl->txq.inlen_empw = max_inline;
	}
	txq_ctrl->max_inline_data = RTE_MAX(txq_ctrl->txq.inlen_send,
					    txq_ctrl->txq.inlen_empw);
	assert(txq_ctrl->max_inline_data <= max_inline);
	assert(txq_ctrl->txq.inlen_mode <= max_inline);
	assert(txq_ctrl->txq.inlen_mode <= txq_ctrl->txq.inlen_send);
	assert(txq_ctrl->txq.inlen_mode <= txq_ctrl->txq.inlen_empw ||
	       !txq_ctrl->txq.inlen_empw);
	return 0;
error:
	rte_errno = ENOMEM;
	return -ENOMEM;
}

/**
 * Create a DPDK Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param socket
 *   NUMA socket on which memory must be allocated.
 * @param[in] conf
 *  Thresholds parameters.
 *
 * @return
 *   A DPDK queue object on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_txq_ctrl *
mlx5_txq_new(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
	     unsigned int socket, const struct rte_eth_txconf *conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *tmpl;

	tmpl = rte_calloc_socket("TXQ", 1,
				 sizeof(*tmpl) +
				 desc * sizeof(struct rte_mbuf *),
				 0, socket);
	if (!tmpl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	if (mlx5_mr_btree_init(&tmpl->txq.mr_ctrl.cache_bh,
			       MLX5_MR_BTREE_CACHE_N, socket)) {
		/* rte_errno is already set. */
		goto error;
	}
	/* Save pointer of global generation number to check memory event. */
	tmpl->txq.mr_ctrl.dev_gen_ptr = &priv->sh->mr.dev_gen;
	assert(desc > MLX5_TX_COMP_THRESH);
	tmpl->txq.offloads = conf->offloads |
			     dev->data->dev_conf.txmode.offloads;
	tmpl->priv = priv;
	tmpl->socket = socket;
	tmpl->txq.elts_n = log2above(desc);
	tmpl->txq.elts_s = desc;
	tmpl->txq.elts_m = desc - 1;
	tmpl->txq.port_id = dev->data->port_id;
	tmpl->txq.idx = idx;
	txq_set_params(tmpl);
	if (txq_adjust_params(tmpl))
		goto error;
	if (txq_calc_wqebb_cnt(tmpl) >
	    priv->sh->device_attr.orig_attr.max_qp_wr) {
		DRV_LOG(ERR,
			"port %u Tx WQEBB count (%d) exceeds the limit (%d),"
			" try smaller queue size",
			dev->data->port_id, txq_calc_wqebb_cnt(tmpl),
			priv->sh->device_attr.orig_attr.max_qp_wr);
		rte_errno = ENOMEM;
		goto error;
	}
	rte_atomic32_inc(&tmpl->refcnt);
	tmpl->type = MLX5_TXQ_TYPE_STANDARD;
	LIST_INSERT_HEAD(&priv->txqsctrl, tmpl, next);
	return tmpl;
error:
	rte_free(tmpl);
	return NULL;
}

/**
 * Create a DPDK Tx hairpin queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   TX queue index.
 * @param desc
 *   Number of descriptors to configure in queue.
 * @param hairpin_conf
 *  The hairpin configuration.
 *
 * @return
 *   A DPDK queue object on success, NULL otherwise and rte_errno is set.
 */
struct mlx5_txq_ctrl *
mlx5_txq_hairpin_new(struct rte_eth_dev *dev, uint16_t idx, uint16_t desc,
		     const struct rte_eth_hairpin_conf *hairpin_conf)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *tmpl;

	tmpl = rte_calloc_socket("TXQ", 1,
				 sizeof(*tmpl), 0, SOCKET_ID_ANY);
	if (!tmpl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	tmpl->priv = priv;
	tmpl->socket = SOCKET_ID_ANY;
	tmpl->txq.elts_n = log2above(desc);
	tmpl->txq.port_id = dev->data->port_id;
	tmpl->txq.idx = idx;
	tmpl->hairpin_conf = *hairpin_conf;
	tmpl->type = MLX5_TXQ_TYPE_HAIRPIN;
	rte_atomic32_inc(&tmpl->refcnt);
	LIST_INSERT_HEAD(&priv->txqsctrl, tmpl, next);
	return tmpl;
}

/**
 * Get a Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   A pointer to the queue if it exists.
 */
struct mlx5_txq_ctrl *
mlx5_txq_get(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *ctrl = NULL;

	if ((*priv->txqs)[idx]) {
		ctrl = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl,
				    txq);
		mlx5_txq_obj_get(dev, idx);
		rte_atomic32_inc(&ctrl->refcnt);
	}
	return ctrl;
}

/**
 * Release a Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   1 while a reference on it exists, 0 when freed.
 */
int
mlx5_txq_release(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *txq;

	if (!(*priv->txqs)[idx])
		return 0;
	txq = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl, txq);
	if (txq->obj && !mlx5_txq_obj_release(txq->obj))
		txq->obj = NULL;
	if (rte_atomic32_dec_and_test(&txq->refcnt)) {
		txq_free_elts(txq);
		mlx5_mr_btree_free(&txq->txq.mr_ctrl.cache_bh);
		LIST_REMOVE(txq, next);
		rte_free(txq);
		(*priv->txqs)[idx] = NULL;
		return 0;
	}
	return 1;
}

/**
 * Verify if the queue can be released.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   TX queue index.
 *
 * @return
 *   1 if the queue can be released.
 */
int
mlx5_txq_releasable(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *txq;

	if (!(*priv->txqs)[idx])
		return -1;
	txq = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl, txq);
	return (rte_atomic32_read(&txq->refcnt) == 1);
}

/**
 * Verify the Tx Queue list is empty
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_txq_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *txq_ctrl;
	int ret = 0;

	LIST_FOREACH(txq_ctrl, &priv->txqsctrl, next) {
		DRV_LOG(DEBUG, "port %u Tx queue %u still referenced",
			dev->data->port_id, txq_ctrl->txq.idx);
		++ret;
	}
	return ret;
}
