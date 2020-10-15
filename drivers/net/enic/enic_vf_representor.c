/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2008-2019 Cisco Systems, Inc.  All rights reserved.
 */

#include <stdint.h>
#include <stdio.h>

#include <rte_bus_pci.h>
#include <rte_common.h>
#include <rte_dev.h>
#include <rte_ethdev_driver.h>
#include <rte_ethdev_pci.h>
#include <rte_flow_driver.h>
#include <rte_kvargs.h>
#include <rte_pci.h>
#include <rte_string_fns.h>

#include "enic_compat.h"
#include "enic.h"
#include "vnic_dev.h"
#include "vnic_enet.h"
#include "vnic_intr.h"
#include "vnic_cq.h"
#include "vnic_wq.h"
#include "vnic_rq.h"

static uint16_t enic_vf_recv_pkts(void *rx_queue,
				  struct rte_mbuf **rx_pkts,
				  uint16_t nb_pkts)
{
	return enic_recv_pkts(rx_queue, rx_pkts, nb_pkts);
}

static uint16_t enic_vf_xmit_pkts(void *tx_queue,
				  struct rte_mbuf **tx_pkts,
				  uint16_t nb_pkts)
{
	return enic_xmit_pkts(tx_queue, tx_pkts, nb_pkts);
}

static int enic_vf_dev_tx_queue_setup(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx,
	uint16_t nb_desc,
	unsigned int socket_id,
	const struct rte_eth_txconf *tx_conf)
{
	struct enic_vf_representor *vf;
	struct vnic_wq *wq;
	struct enic *pf;
	int err;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	/* Only one queue now */
	if (queue_idx != 0)
		return -EINVAL;
	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	wq = &pf->wq[vf->pf_wq_idx];
	wq->offloads = tx_conf->offloads |
		eth_dev->data->dev_conf.txmode.offloads;
	eth_dev->data->tx_queues[0] = (void *)wq;
	/* Pass vf not pf because of cq index calculation. See enic_alloc_wq */
	err = enic_alloc_wq(&vf->enic, queue_idx, socket_id, nb_desc);
	if (err) {
		ENICPMD_LOG(ERR, "error in allocating wq\n");
		return err;
	}
	return 0;
}

static void enic_vf_dev_tx_queue_release(void *txq)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
	enic_free_wq(txq);
}

static int enic_vf_dev_rx_queue_setup(struct rte_eth_dev *eth_dev,
	uint16_t queue_idx,
	uint16_t nb_desc,
	unsigned int socket_id,
	const struct rte_eth_rxconf *rx_conf,
	struct rte_mempool *mp)
{
	struct enic_vf_representor *vf;
	struct enic *pf;
	int ret;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	/* Only 1 queue now */
	if (queue_idx != 0)
		return -EINVAL;
	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	eth_dev->data->rx_queues[queue_idx] =
		(void *)&pf->rq[vf->pf_rq_sop_idx];
	ret = enic_alloc_rq(&vf->enic, queue_idx, socket_id, mp, nb_desc,
			    rx_conf->rx_free_thresh);
	if (ret) {
		ENICPMD_LOG(ERR, "error in allocating rq\n");
		return ret;
	}
	return 0;
}

static void enic_vf_dev_rx_queue_release(void *rxq)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return;
	enic_free_rq(rxq);
}

static int enic_vf_dev_configure(struct rte_eth_dev *eth_dev __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	return 0;
}

static int
setup_rep_vf_fwd(struct enic_vf_representor *vf)
{
	int ret;

	ENICPMD_FUNC_TRACE();
	/* Representor -> VF rule
	 * Egress packets from this representor are on the representor's WQ.
	 * So, loop back that WQ to VF.
	 */
	ret = enic_fm_add_rep2vf_flow(vf);
	if (ret) {
		ENICPMD_LOG(ERR, "Cannot create representor->VF flow");
		return ret;
	}
	/* VF -> representor rule
	 * Packets from VF loop back to the representor, unless they match
	 * user-added flows.
	 */
	ret = enic_fm_add_vf2rep_flow(vf);
	if (ret) {
		ENICPMD_LOG(ERR, "Cannot create VF->representor flow");
		return ret;
	}
	return 0;
}

static int enic_vf_dev_start(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;
	struct vnic_rq *data_rq;
	int index, cq_idx;
	struct enic *pf;
	int ret;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;

	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	/* Get representor flowman for flow API and representor path */
	ret = enic_fm_init(&vf->enic);
	if (ret)
		return ret;
	/* Set up implicit flow rules to forward between representor and VF */
	ret = setup_rep_vf_fwd(vf);
	if (ret) {
		ENICPMD_LOG(ERR, "Cannot set up representor-VF flows");
		return ret;
	}
	/* Remove all packet filters so no ingress packets go to VF.
	 * When PF enables switchdev, it will ensure packet filters
	 * are removed.  So, this is not technically needed.
	 */
	ENICPMD_LOG(DEBUG, "Clear packet filters");
	ret = vnic_dev_packet_filter(vf->enic.vdev, 0, 0, 0, 0, 0);
	if (ret) {
		ENICPMD_LOG(ERR, "Cannot clear packet filters");
		return ret;
	}

	/* Start WQ: see enic_init_vnic_resources */
	index = vf->pf_wq_idx;
	cq_idx = vf->pf_wq_cq_idx;
	vnic_wq_init(&pf->wq[index], cq_idx, 1, 0);
	vnic_cq_init(&pf->cq[cq_idx],
		     0 /* flow_control_enable */,
		     1 /* color_enable */,
		     0 /* cq_head */,
		     0 /* cq_tail */,
		     1 /* cq_tail_color */,
		     0 /* interrupt_enable */,
		     0 /* cq_entry_enable */,
		     1 /* cq_message_enable */,
		     0 /* interrupt offset */,
		     (uint64_t)pf->wq[index].cqmsg_rz->iova);
	/* enic_start_wq */
	vnic_wq_enable(&pf->wq[index]);
	eth_dev->data->tx_queue_state[0] = RTE_ETH_QUEUE_STATE_STARTED;

	/* Start RQ: see enic_init_vnic_resources */
	index = vf->pf_rq_sop_idx;
	cq_idx = enic_cq_rq(vf->pf, index);
	vnic_rq_init(&pf->rq[index], cq_idx, 1, 0);
	data_rq = &pf->rq[vf->pf_rq_data_idx];
	if (data_rq->in_use)
		vnic_rq_init(data_rq, cq_idx, 1, 0);
	vnic_cq_init(&pf->cq[cq_idx],
		     0 /* flow_control_enable */,
		     1 /* color_enable */,
		     0 /* cq_head */,
		     0 /* cq_tail */,
		     1 /* cq_tail_color */,
		     0,
		     1 /* cq_entry_enable */,
		     0 /* cq_message_enable */,
		     0,
		     0 /* cq_message_addr */);
	/* enic_enable */
	ret = enic_alloc_rx_queue_mbufs(pf, &pf->rq[index]);
	if (ret) {
		ENICPMD_LOG(ERR, "Failed to alloc sop RX queue mbufs\n");
		return ret;
	}
	ret = enic_alloc_rx_queue_mbufs(pf, data_rq);
	if (ret) {
		/* Release the allocated mbufs for the sop rq*/
		enic_rxmbuf_queue_release(pf, &pf->rq[index]);
		ENICPMD_LOG(ERR, "Failed to alloc data RX queue mbufs\n");
		return ret;
	}
	enic_start_rq(pf, vf->pf_rq_sop_idx);
	eth_dev->data->tx_queue_state[0] = RTE_ETH_QUEUE_STATE_STARTED;
	eth_dev->data->rx_queue_state[0] = RTE_ETH_QUEUE_STATE_STARTED;
	return 0;
}

static int enic_vf_dev_stop(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;
	struct vnic_rq *rq;
	struct enic *pf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	/* Undo dev_start. Disable/clean WQ */
	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	vnic_wq_disable(&pf->wq[vf->pf_wq_idx]);
	vnic_wq_clean(&pf->wq[vf->pf_wq_idx], enic_free_wq_buf);
	vnic_cq_clean(&pf->cq[vf->pf_wq_cq_idx]);
	/* Disable/clean RQ */
	rq = &pf->rq[vf->pf_rq_sop_idx];
	vnic_rq_disable(rq);
	vnic_rq_clean(rq, enic_free_rq_buf);
	rq = &pf->rq[vf->pf_rq_data_idx];
	if (rq->in_use) {
		vnic_rq_disable(rq);
		vnic_rq_clean(rq, enic_free_rq_buf);
	}
	vnic_cq_clean(&pf->cq[enic_cq_rq(vf->pf, vf->pf_rq_sop_idx)]);
	eth_dev->data->tx_queue_state[0] = RTE_ETH_QUEUE_STATE_STOPPED;
	eth_dev->data->rx_queue_state[0] = RTE_ETH_QUEUE_STATE_STOPPED;
	/* Clean up representor flowman */
	enic_fm_destroy(&vf->enic);

	return 0;
}

/*
 * "close" is no-op for now and solely exists so that rte_eth_dev_close()
 * can finish its own cleanup without errors.
 */
static int enic_vf_dev_close(struct rte_eth_dev *eth_dev __rte_unused)
{
	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;
	return 0;
}

static int
adjust_flow_attr(const struct rte_flow_attr *attrs,
		 struct rte_flow_attr *vf_attrs,
		 struct rte_flow_error *error)
{
	if (!attrs) {
		return rte_flow_error_set(error, EINVAL,
				RTE_FLOW_ERROR_TYPE_ATTR,
				NULL, "no attribute specified");
	}
	/*
	 * Swap ingress and egress as the firmware view of direction
	 * is the opposite of the representor.
	 */
	*vf_attrs = *attrs;
	if (attrs->ingress && !attrs->egress) {
		vf_attrs->ingress = 0;
		vf_attrs->egress = 1;
		return 0;
	}
	return rte_flow_error_set(error, ENOTSUP,
			RTE_FLOW_ERROR_TYPE_ATTR_EGRESS, NULL,
			"representor only supports ingress");
}

static int
enic_vf_flow_validate(struct rte_eth_dev *dev,
		      const struct rte_flow_attr *attrs,
		      const struct rte_flow_item pattern[],
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error)
{
	struct rte_flow_attr vf_attrs;
	int ret;

	ret = adjust_flow_attr(attrs, &vf_attrs, error);
	if (ret)
		return ret;
	attrs = &vf_attrs;
	return enic_fm_flow_ops.validate(dev, attrs, pattern, actions, error);
}

static struct rte_flow *
enic_vf_flow_create(struct rte_eth_dev *dev,
		    const struct rte_flow_attr *attrs,
		    const struct rte_flow_item pattern[],
		    const struct rte_flow_action actions[],
		    struct rte_flow_error *error)
{
	struct rte_flow_attr vf_attrs;

	if (adjust_flow_attr(attrs, &vf_attrs, error))
		return NULL;
	attrs = &vf_attrs;
	return enic_fm_flow_ops.create(dev, attrs, pattern, actions, error);
}

static int
enic_vf_flow_destroy(struct rte_eth_dev *dev, struct rte_flow *flow,
		     struct rte_flow_error *error)
{
	return enic_fm_flow_ops.destroy(dev, flow, error);
}

static int
enic_vf_flow_query(struct rte_eth_dev *dev,
		   struct rte_flow *flow,
		   const struct rte_flow_action *actions,
		   void *data,
		   struct rte_flow_error *error)
{
	return enic_fm_flow_ops.query(dev, flow, actions, data, error);
}

static int
enic_vf_flow_flush(struct rte_eth_dev *dev,
		   struct rte_flow_error *error)
{
	return enic_fm_flow_ops.flush(dev, error);
}

static const struct rte_flow_ops enic_vf_flow_ops = {
	.validate = enic_vf_flow_validate,
	.create = enic_vf_flow_create,
	.destroy = enic_vf_flow_destroy,
	.flush = enic_vf_flow_flush,
	.query = enic_vf_flow_query,
};

static int
enic_vf_filter_ctrl(struct rte_eth_dev *eth_dev,
		    enum rte_filter_type filter_type,
		    enum rte_filter_op filter_op,
		    void *arg)
{
	struct enic_vf_representor *vf;
	int ret = 0;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		if (vf->enic.flow_filter_mode == FILTER_FLOWMAN) {
			*(const void **)arg = &enic_vf_flow_ops;
		} else {
			ENICPMD_LOG(WARNING, "VF representors require flowman support for rte_flow API");
			ret = -EINVAL;
		}
		break;
	default:
		ENICPMD_LOG(WARNING, "Filter type (%d) not supported",
			    filter_type);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int enic_vf_link_update(struct rte_eth_dev *eth_dev,
	int wait_to_complete __rte_unused)
{
	struct enic_vf_representor *vf;
	struct rte_eth_link link;
	struct enic *pf;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	/*
	 * Link status and speed are same as PF. Update PF status and then
	 * copy it to VF.
	 */
	enic_link_update(pf->rte_dev);
	rte_eth_linkstatus_get(pf->rte_dev, &link);
	rte_eth_linkstatus_set(eth_dev, &link);
	return 0;
}

static int enic_vf_stats_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_stats *stats)
{
	struct enic_vf_representor *vf;
	struct vnic_stats *vs;
	int err;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	/* Get VF stats via PF */
	err = vnic_dev_stats_dump(vf->enic.vdev, &vs);
	if (err) {
		ENICPMD_LOG(ERR, "error in getting stats\n");
		return err;
	}
	stats->ipackets = vs->rx.rx_frames_ok;
	stats->opackets = vs->tx.tx_frames_ok;
	stats->ibytes = vs->rx.rx_bytes_ok;
	stats->obytes = vs->tx.tx_bytes_ok;
	stats->ierrors = vs->rx.rx_errors + vs->rx.rx_drop;
	stats->oerrors = vs->tx.tx_errors;
	stats->imissed = vs->rx.rx_no_bufs;
	return 0;
}

static int enic_vf_stats_reset(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;
	int err;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	/* Ask PF to clear VF stats */
	err = vnic_dev_stats_clear(vf->enic.vdev);
	if (err)
		ENICPMD_LOG(ERR, "error in clearing stats\n");
	return err;
}

static int enic_vf_dev_infos_get(struct rte_eth_dev *eth_dev,
	struct rte_eth_dev_info *device_info)
{
	struct enic_vf_representor *vf;
	struct enic *pf;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	pf = vf->pf;
	device_info->max_rx_queues = eth_dev->data->nb_rx_queues;
	device_info->max_tx_queues = eth_dev->data->nb_tx_queues;
	device_info->min_rx_bufsize = ENIC_MIN_MTU;
	/* Max packet size is same as PF */
	device_info->max_rx_pktlen = enic_mtu_to_max_rx_pktlen(pf->max_mtu);
	device_info->max_mac_addrs = ENIC_UNICAST_PERFECT_FILTERS;
	/* No offload capa, RSS, etc. until Tx/Rx handlers are added */
	device_info->rx_offload_capa = 0;
	device_info->tx_offload_capa = 0;
	device_info->switch_info.name =	pf->rte_dev->device->name;
	device_info->switch_info.domain_id = vf->switch_domain_id;
	device_info->switch_info.port_id = vf->vf_id;
	return 0;
}

static void set_vf_packet_filter(struct enic_vf_representor *vf)
{
	/* switchdev: packet filters are ignored */
	if (vf->enic.switchdev_mode)
		return;
	/* Ask PF to apply filters on VF */
	vnic_dev_packet_filter(vf->enic.vdev, 1 /* unicast */, 1 /* mcast */,
		1 /* bcast */, vf->promisc, vf->allmulti);
}

static int enic_vf_promiscuous_enable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->promisc = 1;
	set_vf_packet_filter(vf);
	return 0;
}

static int enic_vf_promiscuous_disable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->promisc = 0;
	set_vf_packet_filter(vf);
	return 0;
}

static int enic_vf_allmulticast_enable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->allmulti = 1;
	set_vf_packet_filter(vf);
	return 0;
}

static int enic_vf_allmulticast_disable(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return -E_RTE_SECONDARY;
	vf = eth_dev->data->dev_private;
	vf->allmulti = 0;
	set_vf_packet_filter(vf);
	return 0;
}

/*
 * A minimal set of handlers.
 * The representor can get/set a small set of VF settings via "proxy" devcmd.
 * With proxy devcmd, the PF driver basically tells the VIC firmware to
 * "perform this devcmd on that VF".
 */
static const struct eth_dev_ops enic_vf_representor_dev_ops = {
	.allmulticast_enable  = enic_vf_allmulticast_enable,
	.allmulticast_disable = enic_vf_allmulticast_disable,
	.dev_configure        = enic_vf_dev_configure,
	.dev_infos_get        = enic_vf_dev_infos_get,
	.dev_start            = enic_vf_dev_start,
	.dev_stop             = enic_vf_dev_stop,
	.dev_close            = enic_vf_dev_close,
	.filter_ctrl          = enic_vf_filter_ctrl,
	.link_update          = enic_vf_link_update,
	.promiscuous_enable   = enic_vf_promiscuous_enable,
	.promiscuous_disable  = enic_vf_promiscuous_disable,
	.stats_get            = enic_vf_stats_get,
	.stats_reset          = enic_vf_stats_reset,
	.rx_queue_setup	      = enic_vf_dev_rx_queue_setup,
	.rx_queue_release     = enic_vf_dev_rx_queue_release,
	.tx_queue_setup	      = enic_vf_dev_tx_queue_setup,
	.tx_queue_release     = enic_vf_dev_tx_queue_release,
};

static int get_vf_config(struct enic_vf_representor *vf)
{
	struct vnic_enet_config *c;
	struct enic *pf;
	int switch_mtu;
	int err;

	c = &vf->config;
	pf = vf->pf;
	/* VF MAC */
	err = vnic_dev_get_mac_addr(vf->enic.vdev, vf->mac_addr.addr_bytes);
	if (err) {
		ENICPMD_LOG(ERR, "error in getting MAC address\n");
		return err;
	}
	rte_ether_addr_copy(&vf->mac_addr, vf->eth_dev->data->mac_addrs);

	/* VF MTU per its vNIC setting */
	err = vnic_dev_spec(vf->enic.vdev,
			    offsetof(struct vnic_enet_config, mtu),
			    sizeof(c->mtu), &c->mtu);
	if (err) {
		ENICPMD_LOG(ERR, "error in getting MTU\n");
		return err;
	}
	/*
	 * Blade switch (fabric interconnect) port's MTU. Assume the kernel
	 * enic driver runs on VF. That driver automatically adjusts its MTU
	 * according to the switch MTU.
	 */
	switch_mtu = vnic_dev_mtu(pf->vdev);
	vf->eth_dev->data->mtu = c->mtu;
	if (switch_mtu > c->mtu)
		vf->eth_dev->data->mtu = RTE_MIN(ENIC_MAX_MTU, switch_mtu);
	return 0;
}

int enic_vf_representor_init(struct rte_eth_dev *eth_dev, void *init_params)
{
	struct enic_vf_representor *vf, *params;
	struct rte_pci_device *pdev;
	struct enic *pf, *vf_enic;
	struct rte_pci_addr *addr;
	int ret;

	ENICPMD_FUNC_TRACE();
	params = init_params;
	vf = eth_dev->data->dev_private;
	vf->switch_domain_id = params->switch_domain_id;
	vf->vf_id = params->vf_id;
	vf->eth_dev = eth_dev;
	vf->pf = params->pf;
	vf->allmulti = 1;
	vf->promisc = 0;
	pf = vf->pf;
	vf->enic.switchdev_mode = pf->switchdev_mode;
	/* Only switchdev is supported now */
	RTE_ASSERT(vf->enic.switchdev_mode);
	/* Allocate WQ, RQ, CQ for the representor */
	vf->pf_wq_idx = vf_wq_idx(vf);
	vf->pf_wq_cq_idx = vf_wq_cq_idx(vf);
	vf->pf_rq_sop_idx = vf_rq_sop_idx(vf);
	vf->pf_rq_data_idx = vf_rq_data_idx(vf);
	/* Remove these assertions once queue allocation has an easy-to-use
	 * allocator API instead of index number calculations used throughout
	 * the driver..
	 */
	RTE_ASSERT(enic_cq_rq(pf, vf->pf_rq_sop_idx) == vf->pf_rq_sop_idx);
	RTE_ASSERT(enic_rte_rq_idx_to_sop_idx(vf->pf_rq_sop_idx) ==
		   vf->pf_rq_sop_idx);
	/* RX handlers use enic_cq_rq(sop) to get CQ, so do not save it */
	pf->vf_required_wq++;
	pf->vf_required_rq += 2; /* sop and data */
	pf->vf_required_cq += 2; /* 1 for rq sop and 1 for wq */
	ENICPMD_LOG(DEBUG, "vf_id %u wq %u rq_sop %u rq_data %u wq_cq %u rq_cq %u",
		vf->vf_id, vf->pf_wq_idx, vf->pf_rq_sop_idx, vf->pf_rq_data_idx,
		vf->pf_wq_cq_idx, enic_cq_rq(pf, vf->pf_rq_sop_idx));
	if (enic_cq_rq(pf, vf->pf_rq_sop_idx) >= pf->conf_cq_count) {
		ENICPMD_LOG(ERR, "Insufficient CQs. Please ensure number of CQs (%u)"
			    " >= number of RQs (%u) in CIMC or UCSM",
			    pf->conf_cq_count, pf->conf_rq_count);
		return -EINVAL;
	}

	/* Check for non-existent VFs */
	pdev = RTE_ETH_DEV_TO_PCI(pf->rte_dev);
	if (vf->vf_id >= pdev->max_vfs) {
		ENICPMD_LOG(ERR, "VF ID is invalid. vf_id %u max_vfs %u",
			    vf->vf_id, pdev->max_vfs);
		return -ENODEV;
	}

	eth_dev->device->driver = pf->rte_dev->device->driver;
	eth_dev->dev_ops = &enic_vf_representor_dev_ops;
	eth_dev->data->dev_flags |= RTE_ETH_DEV_REPRESENTOR;
	eth_dev->data->representor_id = vf->vf_id;
	eth_dev->data->mac_addrs = rte_zmalloc("enic_mac_addr_vf",
		sizeof(struct rte_ether_addr) *
		ENIC_UNICAST_PERFECT_FILTERS, 0);
	if (eth_dev->data->mac_addrs == NULL)
		return -ENOMEM;
	/* Use 1 RX queue and 1 TX queue for representor path */
	eth_dev->data->nb_rx_queues = 1;
	eth_dev->data->nb_tx_queues = 1;
	eth_dev->rx_pkt_burst = &enic_vf_recv_pkts;
	eth_dev->tx_pkt_burst = &enic_vf_xmit_pkts;
	/* Initial link state copied from PF */
	eth_dev->data->dev_link = pf->rte_dev->data->dev_link;
	/* Representor vdev to perform devcmd */
	vf->enic.vdev = vnic_vf_rep_register(&vf->enic, pf->vdev, vf->vf_id);
	if (vf->enic.vdev == NULL)
		return -ENOMEM;
	ret = vnic_dev_alloc_stats_mem(vf->enic.vdev);
	if (ret)
		return ret;
	/* Get/copy VF vNIC MAC, MTU, etc. into eth_dev */
	ret = get_vf_config(vf);
	if (ret)
		return ret;

	/*
	 * Calculate VF BDF. The firmware ensures that PF BDF is always
	 * bus:dev.0, and VF BDFs are dev.1, dev.2, and so on.
	 */
	vf->bdf = pdev->addr;
	vf->bdf.function += vf->vf_id + 1;

	/* Copy a few fields used by enic_fm_flow */
	vf_enic = &vf->enic;
	vf_enic->switch_domain_id = vf->switch_domain_id;
	vf_enic->flow_filter_mode = pf->flow_filter_mode;
	vf_enic->rte_dev = eth_dev;
	vf_enic->dev_data = eth_dev->data;
	LIST_INIT(&vf_enic->flows);
	LIST_INIT(&vf_enic->memzone_list);
	rte_spinlock_init(&vf_enic->memzone_list_lock);
	addr = &vf->bdf;
	snprintf(vf_enic->bdf_name, ENICPMD_BDF_LENGTH, "%04x:%02x:%02x.%x",
		 addr->domain, addr->bus, addr->devid, addr->function);
	return 0;
}

int enic_vf_representor_uninit(struct rte_eth_dev *eth_dev)
{
	struct enic_vf_representor *vf;

	ENICPMD_FUNC_TRACE();
	vf = eth_dev->data->dev_private;
	vnic_dev_unregister(vf->enic.vdev);
	return 0;
}
