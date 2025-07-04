/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2015 6WIND S.A.
 * Copyright 2015 Mellanox Technologies, Ltd
 */

#include <stddef.h>
#include <errno.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>
#include <inttypes.h>

#include <eal_export.h>
#include <rte_mbuf.h>
#include <rte_malloc.h>
#include <ethdev_driver.h>
#include <bus_pci_driver.h>
#include <rte_common.h>
#include <rte_eal_paging.h>
#include <rte_bitops.h>

#include <mlx5_common.h>
#include <mlx5_common_mr.h>
#include <mlx5_malloc.h>

#include "mlx5_defs.h"
#include "mlx5_utils.h"
#include "mlx5.h"
#include "mlx5_tx.h"
#include "mlx5_rxtx.h"
#include "mlx5_autoconf.h"
#include "mlx5_devx.h"
#include "rte_pmd_mlx5.h"
#include "mlx5_flow.h"

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
	struct rte_mbuf *(*elts)[] = &txq_ctrl->txq.elts;

	DRV_LOG(DEBUG, "port %u Tx queue %u freeing WRs",
		PORT_ID(txq_ctrl->priv), txq_ctrl->txq.idx);
	txq_ctrl->txq.elts_head = 0;
	txq_ctrl->txq.elts_tail = 0;
	txq_ctrl->txq.elts_comp = 0;

	while (elts_tail != elts_head) {
		struct rte_mbuf *elt = (*elts)[elts_tail & elts_m];

		MLX5_ASSERT(elt != NULL);
		rte_pktmbuf_free_seg(elt);
#ifdef RTE_LIBRTE_MLX5_DEBUG
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
	uint64_t offloads = (RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
			     RTE_ETH_TX_OFFLOAD_VLAN_INSERT);
	struct mlx5_port_config *config = &priv->config;
	struct mlx5_dev_cap *dev_cap = &priv->sh->dev_cap;

	if (dev_cap->hw_csum)
		offloads |= (RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
			     RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
			     RTE_ETH_TX_OFFLOAD_TCP_CKSUM);
	if (dev_cap->tso)
		offloads |= RTE_ETH_TX_OFFLOAD_TCP_TSO;
	if (priv->sh->config.tx_pp ||
	    priv->sh->cdev->config.hca_attr.wait_on_time)
		offloads |= RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP;
	if (dev_cap->swp) {
		if (dev_cap->swp & MLX5_SW_PARSING_CSUM_CAP)
			offloads |= RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM;
		if (dev_cap->swp & MLX5_SW_PARSING_TSO_CAP)
			offloads |= (RTE_ETH_TX_OFFLOAD_IP_TNL_TSO |
				     RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO);
	}
	if (dev_cap->tunnel_en) {
		if (dev_cap->hw_csum)
			offloads |= RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM;
		if (dev_cap->tso) {
			if (dev_cap->tunnel_en &
				MLX5_TUNNELED_OFFLOADS_VXLAN_CAP)
				offloads |= RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO;
			if (dev_cap->tunnel_en &
				MLX5_TUNNELED_OFFLOADS_GRE_CAP)
				offloads |= RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO;
			if (dev_cap->tunnel_en &
				MLX5_TUNNELED_OFFLOADS_GENEVE_CAP)
				offloads |= RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO;
		}
	}
	if (!config->mprq.enabled)
		offloads |= RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE;
	return offloads;
}

/* Fetches and drops all SW-owned and error CQEs to synchronize CQ. */
static void
txq_sync_cq(struct mlx5_txq_data *txq)
{
	volatile struct mlx5_cqe *cqe;
	int ret, i;

	i = txq->cqe_s;
	do {
		cqe = &txq->cqes[txq->cq_ci & txq->cqe_m];
		ret = check_cqe(cqe, txq->cqe_s, txq->cq_ci);
		if (unlikely(ret != MLX5_CQE_STATUS_SW_OWN)) {
			if (likely(ret != MLX5_CQE_STATUS_ERR)) {
				/* No new CQEs in completion queue. */
				MLX5_ASSERT(ret == MLX5_CQE_STATUS_HW_OWN);
				break;
			}
		}
		++txq->cq_ci;
	} while (--i);
	/* Move all CQEs to HW ownership. */
	for (i = 0; i < txq->cqe_s; i++) {
		cqe = &txq->cqes[i];
		cqe->op_own = MLX5_CQE_INVALIDATE;
	}
	/* Resync CQE and WQE (WQ in reset state). */
	rte_io_wmb();
	*txq->cq_db = rte_cpu_to_be_32(txq->cq_ci);
	txq->cq_pi = txq->cq_ci;
	rte_io_wmb();
}

/**
 * Tx queue stop. Device queue goes to the idle state,
 * all involved mbufs are freed from elts/WQ.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Tx queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_tx_queue_stop_primary(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq, struct mlx5_txq_ctrl, txq);
	int ret;

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);
	/* Move QP to RESET state. */
	ret = priv->obj_ops.txq_obj_modify(txq_ctrl->obj, MLX5_TXQ_MOD_RDY2RST,
					   (uint8_t)priv->dev_port);
	if (ret)
		return ret;
	/* Handle all send completions. */
	txq_sync_cq(txq);
	/* Free elts stored in the SQ. */
	txq_free_elts(txq_ctrl);
	/* Prevent writing new pkts to SQ by setting no free WQE.*/
	txq->wqe_ci = txq->wqe_s;
	txq->wqe_pi = 0;
	txq->elts_comp = 0;
	/* Set the actual queue state. */
	dev->data->tx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STOPPED;
	return 0;
}

/**
 * Tx queue stop. Device queue goes to the idle state,
 * all involved mbufs are freed from elts/WQ.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   Tx queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_tx_queue_stop(struct rte_eth_dev *dev, uint16_t idx)
{
	int ret;

	if (rte_eth_dev_is_tx_hairpin_queue(dev, idx)) {
		DRV_LOG(ERR, "Hairpin queue can't be stopped");
		rte_errno = EINVAL;
		return -EINVAL;
	}
	if (dev->data->tx_queue_state[idx] == RTE_ETH_QUEUE_STATE_STOPPED)
		return 0;
	if (rte_eal_process_type() ==  RTE_PROC_SECONDARY) {
		ret = mlx5_mp_os_req_queue_control(dev, idx,
						   MLX5_MP_REQ_QUEUE_TX_STOP);
	} else {
		ret = mlx5_tx_queue_stop_primary(dev, idx);
	}
	return ret;
}

/**
 * Rx queue start. Device queue goes to the ready state,
 * all required mbufs are allocated and WQ is replenished.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_tx_queue_start_primary(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_data *txq = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *txq_ctrl =
			container_of(txq, struct mlx5_txq_ctrl, txq);
	int ret;

	MLX5_ASSERT(rte_eal_process_type() ==  RTE_PROC_PRIMARY);
	ret = priv->obj_ops.txq_obj_modify(txq_ctrl->obj,
					   MLX5_TXQ_MOD_RST2RDY,
					   (uint8_t)priv->dev_port);
	if (ret)
		return ret;
	txq_ctrl->txq.wqe_ci = 0;
	txq_ctrl->txq.wqe_pi = 0;
	txq_ctrl->txq.elts_comp = 0;
	/* Set the actual queue state. */
	dev->data->tx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

/**
 * Rx queue start. Device queue goes to the ready state,
 * all required mbufs are allocated and WQ is replenished.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param idx
 *   RX queue index.
 *
 * @return
 *   0 on success, a negative errno value otherwise and rte_errno is set.
 */
int
mlx5_tx_queue_start(struct rte_eth_dev *dev, uint16_t idx)
{
	int ret;

	if (rte_eth_dev_is_tx_hairpin_queue(dev, idx)) {
		DRV_LOG(ERR, "Hairpin queue can't be started");
		rte_errno = EINVAL;
		return -EINVAL;
	}
	if (dev->data->tx_queue_state[idx] == RTE_ETH_QUEUE_STATE_STARTED)
		return 0;
	if (rte_eal_process_type() ==  RTE_PROC_SECONDARY) {
		ret = mlx5_mp_os_req_queue_control(dev, idx,
						   MLX5_MP_REQ_QUEUE_TX_START);
	} else {
		ret = mlx5_tx_queue_start_primary(dev, idx);
	}
	return ret;
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
mlx5_tx_queue_pre_setup(struct rte_eth_dev *dev, uint16_t idx, uint16_t *desc)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	if (*desc > mlx5_dev_get_max_wq_size(priv->sh)) {
		DRV_LOG(ERR,
			"port %u number of descriptors requested for Tx queue"
			" %u is more than supported",
			dev->data->port_id, idx);
		rte_errno = EINVAL;
		return -EINVAL;
	}
	if (*desc <= MLX5_TX_COMP_THRESH) {
		DRV_LOG(WARNING,
			"port %u number of descriptors requested for Tx queue"
			" %u must be higher than MLX5_TX_COMP_THRESH, using %u"
			" instead of %u", dev->data->port_id, idx,
			MLX5_TX_COMP_THRESH + 1, *desc);
		*desc = MLX5_TX_COMP_THRESH + 1;
	}
	if (!rte_is_power_of_2(*desc)) {
		*desc = 1 << log2above(*desc);
		DRV_LOG(WARNING,
			"port %u increased number of descriptors in Tx queue"
			" %u to the next power of two (%d)",
			dev->data->port_id, idx, *desc);
	}
	DRV_LOG(DEBUG, "port %u configuring queue %u for %u descriptors",
		dev->data->port_id, idx, *desc);
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

	res = mlx5_tx_queue_pre_setup(dev, idx, &desc);
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

	res = mlx5_tx_queue_pre_setup(dev, idx, &desc);
	if (res)
		return res;
	if (hairpin_conf->peer_count != 1) {
		rte_errno = EINVAL;
		DRV_LOG(ERR, "port %u unable to setup Tx hairpin queue index %u"
			" peer count is %u", dev->data->port_id,
			idx, hairpin_conf->peer_count);
		return -rte_errno;
	}
	if (hairpin_conf->peers[0].port == dev->data->port_id) {
		if (hairpin_conf->peers[0].queue >= priv->rxqs_n) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u unable to setup Tx hairpin queue"
				" index %u, Rx %u is larger than %u",
				dev->data->port_id, idx,
				hairpin_conf->peers[0].queue, priv->txqs_n);
			return -rte_errno;
		}
	} else {
		if (hairpin_conf->manual_bind == 0 ||
		    hairpin_conf->tx_explicit == 0) {
			rte_errno = EINVAL;
			DRV_LOG(ERR, "port %u unable to setup Tx hairpin queue"
				" index %u peer port %u with attributes %u %u",
				dev->data->port_id, idx,
				hairpin_conf->peers[0].port,
				hairpin_conf->manual_bind,
				hairpin_conf->tx_explicit);
			return -rte_errno;
		}
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
	dev->data->tx_queue_state[idx] = RTE_ETH_QUEUE_STATE_HAIRPIN;
	return 0;
}

/**
 * DPDK callback to release a TX queue.
 *
 * @param dev
 *   Pointer to Ethernet device structure.
 * @param qid
 *   Transmit queue index.
 */
void
mlx5_tx_queue_release(struct rte_eth_dev *dev, uint16_t qid)
{
	struct mlx5_txq_data *txq = dev->data->tx_queues[qid];

	if (txq == NULL)
		return;
	DRV_LOG(DEBUG, "port %u removing Tx queue %u from list",
		dev->data->port_id, qid);
	mlx5_txq_release(dev, qid);
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
	struct mlx5_proc_priv *primary_ppriv = priv->sh->pppriv;
	struct mlx5_txq_data *txq = &txq_ctrl->txq;
	void *addr;
	uintptr_t uar_va;
	uintptr_t offset;
	const size_t page_size = rte_mem_page_size();
	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
		return -rte_errno;
	}

	if (txq_ctrl->is_hairpin)
		return 0;
	MLX5_ASSERT(ppriv);
	/*
	 * As rdma-core, UARs are mapped in size of OS page
	 * size. Ref to libmlx5 function: mlx5_init_context()
	 */
	uar_va = (uintptr_t)primary_ppriv->uar_table[txq->idx].db;
	offset = uar_va & (page_size - 1); /* Offset in page. */
	addr = rte_mem_map(NULL, page_size, RTE_PROT_WRITE, RTE_MAP_SHARED,
			   fd, txq_ctrl->uar_mmap_offset);
	if (!addr) {
		DRV_LOG(ERR, "Port %u mmap failed for BF reg of txq %u.",
			txq->port_id, txq->idx);
		rte_errno = ENXIO;
		return -rte_errno;
	}
	addr = RTE_PTR_ADD(addr, offset);
	ppriv->uar_table[txq->idx].db = addr;
#ifndef RTE_ARCH_64
	ppriv->uar_table[txq->idx].sl_p =
			primary_ppriv->uar_table[txq->idx].sl_p;
#endif
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
	void *addr;
	const size_t page_size = rte_mem_page_size();
	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		rte_errno = ENOMEM;
	}

	if (txq_ctrl->is_hairpin)
		return;
	addr = ppriv->uar_table[txq_ctrl->txq.idx].db;
	rte_mem_unmap(RTE_PTR_ALIGN_FLOOR(addr, page_size), page_size);
}

/**
 * Deinitialize Tx UAR registers for secondary process.
 *
 * @param dev
 *   Pointer to Ethernet device.
 */
void
mlx5_tx_uar_uninit_secondary(struct rte_eth_dev *dev)
{
	struct mlx5_proc_priv *ppriv = (struct mlx5_proc_priv *)
					dev->process_private;
	const size_t page_size = rte_mem_page_size();
	void *addr;
	unsigned int i;

	if (page_size == (size_t)-1) {
		DRV_LOG(ERR, "Failed to get mem page size");
		return;
	}
	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	for (i = 0; i != ppriv->uar_table_sz; ++i) {
		if (!ppriv->uar_table[i].db)
			continue;
		addr = ppriv->uar_table[i].db;
		rte_mem_unmap(RTE_PTR_ALIGN_FLOOR(addr, page_size), page_size);

	}
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

	MLX5_ASSERT(rte_eal_process_type() == RTE_PROC_SECONDARY);
	for (i = 0; i != priv->txqs_n; ++i) {
		if (!(*priv->txqs)[i])
			continue;
		txq = (*priv->txqs)[i];
		txq_ctrl = container_of(txq, struct mlx5_txq_ctrl, txq);
		if (txq_ctrl->is_hairpin)
			continue;
		MLX5_ASSERT(txq->idx == (uint16_t)i);
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
	wqe_size = RTE_MAX(wqe_size, MLX5_WQE_SIZE);
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

	wqe_size = mlx5_dev_get_max_wq_size(priv->sh) / desc;
	if (!wqe_size)
		return 0;
	/*
	 * This calculation is derived from the source of
	 * mlx5_calc_send_wqe() in rdma_core library.
	 */
	wqe_size = wqe_size * MLX5_WQE_SIZE -
		   MLX5_WQE_CSEG_SIZE -
		   MLX5_WQE_ESEG_SIZE -
		   MLX5_WSEG_SIZE -
		   MLX5_WSEG_SIZE +
		   MLX5_ESEG_MIN_INLINE_SIZE;
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
	struct mlx5_port_config *config = &priv->config;
	struct mlx5_dev_cap *dev_cap = &priv->sh->dev_cap;
	unsigned int inlen_send; /* Inline data for ordinary SEND.*/
	unsigned int inlen_empw; /* Inline data for enhanced MPW. */
	unsigned int inlen_mode; /* Minimal required Inline data. */
	unsigned int txqs_inline; /* Min Tx queues to enable inline. */
	uint64_t dev_txoff = priv->dev_data->dev_conf.txmode.offloads;
	bool tso = txq_ctrl->txq.offloads & (RTE_ETH_TX_OFFLOAD_TCP_TSO |
					    RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
					    RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
					    RTE_ETH_TX_OFFLOAD_IP_TNL_TSO |
					    RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO);
	bool vlan_inline;
	unsigned int temp;

	txq_ctrl->txq.fast_free =
		!!((txq_ctrl->txq.offloads & RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE) &&
		   !(txq_ctrl->txq.offloads & RTE_ETH_TX_OFFLOAD_MULTI_SEGS) &&
		   !config->mprq.enabled);
	if (config->txqs_inline == MLX5_ARG_UNSET)
		txqs_inline =
#if defined(RTE_ARCH_ARM64)
		(priv->pci_dev && priv->pci_dev->id.device_id ==
			PCI_DEVICE_ID_MELLANOX_BLUEFIELD) ?
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
	 * and ConnectX-4 Lx which requires L2-L4 inlined to
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
	vlan_inline = (dev_txoff & RTE_ETH_TX_OFFLOAD_VLAN_INSERT) &&
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
		MLX5_ASSERT(inlen_send >= MLX5_ESEG_MIN_INLINE_SIZE);
		MLX5_ASSERT(inlen_send <= MLX5_WQE_SIZE_MAX +
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
		MLX5_ASSERT(inlen_empw >= MLX5_ESEG_MIN_INLINE_SIZE);
		MLX5_ASSERT(inlen_empw <= MLX5_WQE_SIZE_MAX +
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
	if (((RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO & txq_ctrl->txq.offloads) &&
	    (dev_cap->tunnel_en & MLX5_TUNNELED_OFFLOADS_VXLAN_CAP)) |
	   ((RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO & txq_ctrl->txq.offloads) &&
	    (dev_cap->tunnel_en & MLX5_TUNNELED_OFFLOADS_GRE_CAP)) |
	   ((RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO & txq_ctrl->txq.offloads) &&
	    (dev_cap->tunnel_en & MLX5_TUNNELED_OFFLOADS_GENEVE_CAP)) |
	   (dev_cap->swp  & MLX5_SW_PARSING_TSO_CAP))
		txq_ctrl->txq.tunnel_en = 1;
	txq_ctrl->txq.swp_en = (((RTE_ETH_TX_OFFLOAD_IP_TNL_TSO |
				  RTE_ETH_TX_OFFLOAD_UDP_TNL_TSO) &
				  txq_ctrl->txq.offloads) && (dev_cap->swp &
				  MLX5_SW_PARSING_TSO_CAP)) |
				((RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM &
				 txq_ctrl->txq.offloads) && (dev_cap->swp &
				 MLX5_SW_PARSING_CSUM_CAP));
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
 */
static void
txq_adjust_params(struct mlx5_txq_ctrl *txq_ctrl)
{
	struct mlx5_priv *priv = txq_ctrl->priv;
	struct mlx5_port_config *config = &priv->config;
	unsigned int max_inline;

	max_inline = txq_calc_inline_max(txq_ctrl);
	if (!txq_ctrl->txq.inlen_send) {
		/*
		 * Inline data feature is not engaged at all.
		 * There is nothing to adjust.
		 */
		return;
	}
	if (txq_ctrl->max_inline_data <= max_inline) {
		/*
		 * The requested inline data length does not
		 * exceed queue capabilities.
		 */
		return;
	}
	if (txq_ctrl->txq.inlen_mode > max_inline) {
		DRV_LOG(WARNING,
			"minimal data inline requirements (%u) are not satisfied (%u) on port %u",
			txq_ctrl->txq.inlen_mode, max_inline, priv->dev_data->port_id);
	}
	if (txq_ctrl->txq.inlen_send > max_inline &&
	    config->txq_inline_max != MLX5_ARG_UNSET &&
	    config->txq_inline_max > (int)max_inline) {
		DRV_LOG(WARNING,
			"txq_inline_max requirements (%u) are not satisfied (%u) on port %u",
			txq_ctrl->txq.inlen_send, max_inline, priv->dev_data->port_id);
	}
	if (txq_ctrl->txq.inlen_empw > max_inline &&
	    config->txq_inline_mpw != MLX5_ARG_UNSET &&
	    config->txq_inline_mpw > (int)max_inline) {
		DRV_LOG(WARNING,
			"txq_inline_mpw requirements (%u) are not satisfied (%u) on port %u",
			txq_ctrl->txq.inlen_empw, max_inline, priv->dev_data->port_id);
	}
	if (txq_ctrl->txq.tso_en && max_inline < MLX5_MAX_TSO_HEADER) {
		DRV_LOG(WARNING,
			"tso header inline requirements (%u) are not satisfied (%u) on port %u",
			MLX5_MAX_TSO_HEADER, max_inline, priv->dev_data->port_id);
	}
	if (txq_ctrl->txq.inlen_send > max_inline) {
		DRV_LOG(WARNING,
			"adjust txq_inline_max (%u->%u) due to large Tx queue on port %u",
			txq_ctrl->txq.inlen_send, max_inline, priv->dev_data->port_id);
		txq_ctrl->txq.inlen_send = max_inline;
	}
	if (txq_ctrl->txq.inlen_empw > max_inline) {
		DRV_LOG(WARNING,
			"adjust txq_inline_mpw (%u->%u) due to large Tx queue on port %u",
			txq_ctrl->txq.inlen_empw, max_inline, priv->dev_data->port_id);
		txq_ctrl->txq.inlen_empw = max_inline;
	}
	txq_ctrl->max_inline_data = RTE_MAX(txq_ctrl->txq.inlen_send,
					    txq_ctrl->txq.inlen_empw);
	MLX5_ASSERT(txq_ctrl->txq.inlen_mode <= txq_ctrl->txq.inlen_send);
	MLX5_ASSERT(txq_ctrl->txq.inlen_mode <= txq_ctrl->txq.inlen_empw ||
		    !txq_ctrl->txq.inlen_empw);
}

/*
 * Calculate WQ memory length for a Tx queue.
 *
 * @param log_wqe_cnt
 *   Logarithm value of WQE numbers.
 *
 * @return
 *   memory length of this WQ.
 */
static uint32_t mlx5_txq_wq_mem_length(uint32_t log_wqe_cnt)
{
	uint32_t num_of_wqbbs = 1U << log_wqe_cnt;
	uint32_t umem_size;

	umem_size = MLX5_WQE_SIZE * num_of_wqbbs;
	return umem_size;
}

/*
 * Calculate CQ memory length for a Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param txq_ctrl
 *   Pointer to the TxQ control structure of the CQ.
 *
 * @return
 *   memory length of this CQ.
 */
static uint32_t
mlx5_txq_cq_mem_length(struct rte_eth_dev *dev, struct mlx5_txq_ctrl *txq_ctrl)
{
	uint32_t cqe_n, log_desc_n;

	if (__rte_trace_point_fp_is_enabled() &&
	    txq_ctrl->txq.offloads & RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP)
		cqe_n = UINT16_MAX / 2 - 1;
	else
		cqe_n = (1UL << txq_ctrl->txq.elts_n) / MLX5_TX_COMP_THRESH +
			1 + MLX5_TX_COMP_THRESH_INLINE_DIV;
	log_desc_n = log2above(cqe_n);
	cqe_n = 1UL << log_desc_n;
	if (cqe_n > UINT16_MAX) {
		DRV_LOG(ERR, "Port %u Tx queue %u requests to many CQEs %u.",
			dev->data->port_id, txq_ctrl->txq.idx, cqe_n);
		rte_errno = EINVAL;
		return 0;
	}
	return sizeof(struct mlx5_cqe) * cqe_n;
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
	int ret;
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_txq_ctrl *tmpl;
	uint16_t max_wqe;
	uint32_t wqebb_cnt, log_desc_n;

	if (socket != (unsigned int)SOCKET_ID_ANY) {
		tmpl = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, sizeof(*tmpl) +
			   desc * sizeof(struct rte_mbuf *), 0, socket);
	} else {
		tmpl = mlx5_malloc_numa_tolerant(MLX5_MEM_RTE | MLX5_MEM_ZERO, sizeof(*tmpl) +
					 desc * sizeof(struct rte_mbuf *), 0,
					 dev->device->numa_node);
	}
	if (!tmpl) {
		rte_errno = ENOMEM;
		return NULL;
	}
	if (socket != (unsigned int)SOCKET_ID_ANY) {
		if (mlx5_mr_ctrl_init(&tmpl->txq.mr_ctrl,
					&priv->sh->cdev->mr_scache.dev_gen, socket))
			/* rte_errno is already set. */
			goto error;
	} else {
		ret = mlx5_mr_ctrl_init(&tmpl->txq.mr_ctrl,
					&priv->sh->cdev->mr_scache.dev_gen, dev->device->numa_node);
		if (ret == -ENOMEM) {
			ret = mlx5_mr_ctrl_init(&tmpl->txq.mr_ctrl,
						&priv->sh->cdev->mr_scache.dev_gen, SOCKET_ID_ANY);
		}
		if (ret)
			/* rte_errno is already set. */
			goto error;
	}
	MLX5_ASSERT(desc > MLX5_TX_COMP_THRESH);
	tmpl->txq.offloads = conf->offloads |
			     dev->data->dev_conf.txmode.offloads;
	tmpl->priv = priv;
	tmpl->socket = (socket == (unsigned int)SOCKET_ID_ANY ?
			(unsigned int)dev->device->numa_node : socket);
	tmpl->txq.elts_n = log2above(desc);
	tmpl->txq.elts_s = desc;
	tmpl->txq.elts_m = desc - 1;
	tmpl->txq.port_id = dev->data->port_id;
	tmpl->txq.idx = idx;
	txq_set_params(tmpl);
	txq_adjust_params(tmpl);
	wqebb_cnt = txq_calc_wqebb_cnt(tmpl);
	max_wqe = mlx5_dev_get_max_wq_size(priv->sh);
	if (wqebb_cnt > max_wqe) {
		DRV_LOG(ERR,
			"port %u Tx WQEBB count (%d) exceeds the limit (%d),"
			" try smaller queue size",
			dev->data->port_id, wqebb_cnt, max_wqe);
		rte_errno = ENOMEM;
		goto error;
	}
	if (priv->sh->config.txq_mem_algn != 0) {
		log_desc_n = log2above(wqebb_cnt);
		tmpl->txq.sq_mem_len = mlx5_txq_wq_mem_length(log_desc_n);
		tmpl->txq.cq_mem_len = mlx5_txq_cq_mem_length(dev, tmpl);
		DRV_LOG(DEBUG, "Port %u TxQ %u WQ length %u, CQ length %u before align.",
			dev->data->port_id, idx, tmpl->txq.sq_mem_len, tmpl->txq.cq_mem_len);
		priv->consec_tx_mem.sq_total_size += tmpl->txq.sq_mem_len;
		priv->consec_tx_mem.cq_total_size += tmpl->txq.cq_mem_len;
	}
	rte_atomic_fetch_add_explicit(&tmpl->refcnt, 1, rte_memory_order_relaxed);
	tmpl->is_hairpin = false;
	LIST_INSERT_HEAD(&priv->txqsctrl, tmpl, next);
	return tmpl;
error:
	mlx5_mr_btree_free(&tmpl->txq.mr_ctrl.cache_bh);
	mlx5_free(tmpl);
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

	tmpl = mlx5_malloc(MLX5_MEM_RTE | MLX5_MEM_ZERO, sizeof(*tmpl), 0,
			   SOCKET_ID_ANY);
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
	tmpl->is_hairpin = true;
	rte_atomic_fetch_add_explicit(&tmpl->refcnt, 1, rte_memory_order_relaxed);
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
	struct mlx5_txq_data *txq_data = (*priv->txqs)[idx];
	struct mlx5_txq_ctrl *ctrl = NULL;

	if (txq_data) {
		ctrl = container_of(txq_data, struct mlx5_txq_ctrl, txq);
		rte_atomic_fetch_add_explicit(&ctrl->refcnt, 1, rte_memory_order_relaxed);
	}
	return ctrl;
}

/**
 * Get an external Tx queue.
 *
 * @param dev
 *   Pointer to Ethernet device.
 * @param idx
 *   External Tx queue index.
 *
 * @return
 *   A pointer to the queue if it exists, NULL otherwise.
 */
struct mlx5_external_q *
mlx5_ext_txq_get(struct rte_eth_dev *dev, uint16_t idx)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	MLX5_ASSERT(mlx5_is_external_txq(dev, idx));
	return &priv->ext_txqs[idx - MLX5_EXTERNAL_TX_QUEUE_ID_MIN];
}

/**
 * Verify the external Tx Queue list is empty.
 *
 * @param dev
 *   Pointer to Ethernet device.
 *
 * @return
 *   The number of object not released.
 */
int
mlx5_ext_txq_verify(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_external_q *txq;
	uint32_t i;
	int ret = 0;

	if (priv->ext_txqs == NULL)
		return 0;

	for (i = MLX5_EXTERNAL_TX_QUEUE_ID_MIN; i <= UINT16_MAX ; ++i) {
		txq = mlx5_ext_txq_get(dev, i);
		if (txq->refcnt < 2)
			continue;
		DRV_LOG(DEBUG, "Port %u external TxQ %u still referenced.",
			dev->data->port_id, i);
		++ret;
	}
	return ret;
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
	struct mlx5_txq_ctrl *txq_ctrl;

	if (priv->txqs == NULL || (*priv->txqs)[idx] == NULL)
		return 0;
	txq_ctrl = container_of((*priv->txqs)[idx], struct mlx5_txq_ctrl, txq);
	if (rte_atomic_fetch_sub_explicit(&txq_ctrl->refcnt, 1, rte_memory_order_relaxed) - 1 > 1)
		return 1;
	if (txq_ctrl->obj) {
		priv->obj_ops.txq_obj_release(txq_ctrl->obj);
		LIST_REMOVE(txq_ctrl->obj, next);
		mlx5_free(txq_ctrl->obj);
		txq_ctrl->obj = NULL;
	}
	if (!txq_ctrl->is_hairpin) {
		if (txq_ctrl->txq.fcqs) {
			mlx5_free(txq_ctrl->txq.fcqs);
			txq_ctrl->txq.fcqs = NULL;
		}
		txq_free_elts(txq_ctrl);
		dev->data->tx_queue_state[idx] = RTE_ETH_QUEUE_STATE_STOPPED;
	}
	if (!rte_atomic_load_explicit(&txq_ctrl->refcnt, rte_memory_order_relaxed)) {
		if (!txq_ctrl->is_hairpin)
			mlx5_mr_btree_free(&txq_ctrl->txq.mr_ctrl.cache_bh);
		LIST_REMOVE(txq_ctrl, next);
		mlx5_free(txq_ctrl);
		(*priv->txqs)[idx] = NULL;
	}
	return 0;
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
	return (rte_atomic_load_explicit(&txq->refcnt, rte_memory_order_relaxed) == 1);
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

int
mlx5_txq_get_sqn(struct mlx5_txq_ctrl *txq)
{
	return txq->is_hairpin ? txq->obj->sq->id : txq->obj->sq_obj.sq->id;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_external_sq_enable, 22.07)
int
rte_pmd_mlx5_external_sq_enable(uint16_t port_id, uint32_t sq_num)
{
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	uint32_t flow;

	if (rte_eth_dev_is_valid_port(port_id) < 0) {
		DRV_LOG(ERR, "There is no Ethernet device for port %u.",
			port_id);
		rte_errno = ENODEV;
		return -rte_errno;
	}
	dev = &rte_eth_devices[port_id];
	priv = dev->data->dev_private;
	if ((!priv->representor && !priv->master) ||
	    !priv->sh->config.dv_esw_en) {
		DRV_LOG(ERR, "Port %u must be represetnor or master port in E-Switch mode.",
			port_id);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (sq_num == 0) {
		DRV_LOG(ERR, "Invalid SQ number.");
		rte_errno = EINVAL;
		return -rte_errno;
	}
#ifdef HAVE_MLX5_HWS_SUPPORT
	if (priv->sh->config.dv_flow_en == 2) {
		bool sq_miss_created = false;

		if (priv->sh->config.fdb_def_rule) {
			if (mlx5_flow_hw_esw_create_sq_miss_flow(dev, sq_num, true))
				return -rte_errno;
			sq_miss_created = true;
		}

		if (priv->sh->config.repr_matching &&
		    mlx5_flow_hw_tx_repr_matching_flow(dev, sq_num, true)) {
			if (sq_miss_created)
				mlx5_flow_hw_esw_destroy_sq_miss_flow(dev, sq_num);
			return -rte_errno;
		}
		return 0;
	}
#endif
	flow = mlx5_flow_create_devx_sq_miss_flow(dev, sq_num);
	if (flow > 0)
		return 0;
	DRV_LOG(ERR, "Port %u failed to create default miss flow for SQ %u.",
		port_id, sq_num);
	return -rte_errno;
}

/**
 * Set the Tx queue dynamic timestamp (mask and offset)
 *
 * @param[in] dev
 *   Pointer to the Ethernet device structure.
 */
void
mlx5_txq_dynf_timestamp_set(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;
	struct mlx5_dev_ctx_shared *sh = priv->sh;
	struct mlx5_txq_data *data;
	int off, nbit;
	unsigned int i;
	uint64_t mask = 0;
	uint64_t ts_mask;

	if (sh->dev_cap.rt_timestamp ||
	    !sh->cdev->config.hca_attr.dev_freq_khz)
		ts_mask = MLX5_TS_MASK_SECS << 32;
	else
		ts_mask = rte_align64pow2(MLX5_TS_MASK_SECS * 1000ull *
				sh->cdev->config.hca_attr.dev_freq_khz);
	ts_mask = rte_cpu_to_be_64(ts_mask - 1ull);
	nbit = rte_mbuf_dynflag_lookup
				(RTE_MBUF_DYNFLAG_TX_TIMESTAMP_NAME, NULL);
	off = rte_mbuf_dynfield_lookup
				(RTE_MBUF_DYNFIELD_TIMESTAMP_NAME, NULL);
	if (nbit >= 0 && off >= 0 &&
	    (sh->txpp.refcnt || priv->sh->cdev->config.hca_attr.wait_on_time))
		mask = 1ULL << nbit;
	for (i = 0; i != priv->txqs_n; ++i) {
		data = (*priv->txqs)[i];
		if (!data)
			continue;
		data->sh = sh;
		data->ts_mask = mask;
		data->ts_offset = off;
		data->rt_timestamp = sh->dev_cap.rt_timestamp;
		data->rt_timemask = (data->offloads &
				     RTE_ETH_TX_OFFLOAD_SEND_ON_TIMESTAMP) ?
				     ts_mask : 0;
	}
}

int mlx5_count_aggr_ports(struct rte_eth_dev *dev)
{
	struct mlx5_priv *priv = dev->data->dev_private;

	return priv->sh->bond.n_port;
}

int mlx5_map_aggr_tx_affinity(struct rte_eth_dev *dev, uint16_t tx_queue_id,
			      uint8_t affinity)
{
	struct mlx5_txq_ctrl *txq_ctrl;
	struct mlx5_txq_data *txq;
	struct mlx5_priv *priv;

	priv = dev->data->dev_private;
	if (!mlx5_devx_obj_ops_en(priv->sh)) {
		DRV_LOG(ERR, "Tx affinity mapping isn't supported by Verbs API.");
		rte_errno = ENOTSUP;
		return -rte_errno;
	}
	txq = (*priv->txqs)[tx_queue_id];
	if (!txq)
		return -1;
	txq_ctrl = container_of(txq, struct mlx5_txq_ctrl, txq);
	if (tx_queue_id >= priv->txqs_n) {
		DRV_LOG(ERR, "port %u Tx queue index out of range (%u >= %u)",
			dev->data->port_id, tx_queue_id, priv->txqs_n);
		rte_errno = EOVERFLOW;
		return -rte_errno;
	}
	if (affinity > priv->num_lag_ports) {
		DRV_LOG(ERR, "port %u unable to setup Tx queue index %u"
			" affinity is %u exceeds the maximum %u", dev->data->port_id,
			tx_queue_id, affinity, priv->num_lag_ports);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	DRV_LOG(DEBUG, "port %u configuring queue %u for aggregated affinity %u",
		dev->data->port_id, tx_queue_id, affinity);
	txq_ctrl->txq.tx_aggr_affinity = affinity;
	return 0;
}

/**
 * Validate given external TxQ rte_flow index, and get pointer to concurrent
 * external TxQ object to map/unmap.
 *
 * @param[in] port_id
 *   The port identifier of the Ethernet device.
 * @param[in] dpdk_idx
 *   Tx Queue index in rte_flow.
 *
 * @return
 *   Pointer to concurrent external TxQ on success,
 *   NULL otherwise and rte_errno is set.
 */
static struct mlx5_external_q *
mlx5_external_tx_queue_get_validate(uint16_t port_id, uint16_t dpdk_idx)
{
	struct rte_eth_dev *dev;
	struct mlx5_priv *priv;
	int ret;

	if (dpdk_idx < MLX5_EXTERNAL_TX_QUEUE_ID_MIN) {
		DRV_LOG(ERR, "Queue index %u should be in range: [%u, %u].",
			dpdk_idx, MLX5_EXTERNAL_TX_QUEUE_ID_MIN, UINT16_MAX);
		rte_errno = EINVAL;
		return NULL;
	}
	ret = mlx5_devx_extq_port_validate(port_id);
	if (unlikely(ret))
		return NULL;
	dev = &rte_eth_devices[port_id];
	priv = dev->data->dev_private;
	/*
	 * When user configures remote PD and CTX and device creates TxQ by
	 * DevX, external TxQs array is allocated.
	 */
	MLX5_ASSERT(priv->ext_txqs != NULL);
	return &priv->ext_txqs[dpdk_idx - MLX5_EXTERNAL_TX_QUEUE_ID_MIN];
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_external_tx_queue_id_map, 24.07)
int
rte_pmd_mlx5_external_tx_queue_id_map(uint16_t port_id, uint16_t dpdk_idx,
				      uint32_t hw_idx)
{
	struct mlx5_external_q *ext_txq;
	uint32_t unmapped = 0;

	ext_txq = mlx5_external_tx_queue_get_validate(port_id, dpdk_idx);
	if (ext_txq == NULL)
		return -rte_errno;
	if (!rte_atomic_compare_exchange_strong_explicit(&ext_txq->refcnt, &unmapped, 1,
					 rte_memory_order_relaxed, rte_memory_order_relaxed)) {
		if (ext_txq->hw_id != hw_idx) {
			DRV_LOG(ERR, "Port %u external TxQ index %u "
				"is already mapped to HW index (requesting is "
				"%u, existing is %u).",
				port_id, dpdk_idx, hw_idx, ext_txq->hw_id);
			rte_errno = EEXIST;
			return -rte_errno;
		}
		DRV_LOG(WARNING, "Port %u external TxQ index %u "
			"is already mapped to the requested HW index (%u)",
			port_id, dpdk_idx, hw_idx);

	} else {
		ext_txq->hw_id = hw_idx;
		DRV_LOG(DEBUG, "Port %u external TxQ index %u "
			"is successfully mapped to the requested HW index (%u)",
			port_id, dpdk_idx, hw_idx);
	}
	return 0;
}

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_pmd_mlx5_external_tx_queue_id_unmap, 24.07)
int
rte_pmd_mlx5_external_tx_queue_id_unmap(uint16_t port_id, uint16_t dpdk_idx)
{
	struct mlx5_external_q *ext_txq;
	uint32_t mapped = 1;

	ext_txq = mlx5_external_tx_queue_get_validate(port_id, dpdk_idx);
	if (ext_txq == NULL)
		return -rte_errno;
	if (ext_txq->refcnt > 1) {
		DRV_LOG(ERR, "Port %u external TxQ index %u still referenced.",
			port_id, dpdk_idx);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	if (!rte_atomic_compare_exchange_strong_explicit(&ext_txq->refcnt, &mapped, 0,
					 rte_memory_order_relaxed, rte_memory_order_relaxed)) {
		DRV_LOG(ERR, "Port %u external TxQ index %u doesn't exist.",
			port_id, dpdk_idx);
		rte_errno = EINVAL;
		return -rte_errno;
	}
	DRV_LOG(DEBUG,
		"Port %u external TxQ index %u is successfully unmapped.",
		port_id, dpdk_idx);
	return 0;
}
