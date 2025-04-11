/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include <assert.h>

#include <eal_export.h>
#include <rte_debug.h>

#include "rte_ethdev.h"
#include "rte_ethdev_trace_fp.h"
#include "ethdev_driver.h"
#include "ethdev_private.h"

static const char *MZ_RTE_ETH_DEV_DATA = "rte_eth_dev_data";

static const struct rte_memzone *eth_dev_shared_mz;
struct eth_dev_shared *eth_dev_shared_data;

/* spinlock for eth device callbacks */
rte_spinlock_t eth_dev_cb_lock = RTE_SPINLOCK_INITIALIZER;

uint16_t
eth_dev_to_id(const struct rte_eth_dev *dev)
{
	if (dev == NULL)
		return RTE_MAX_ETHPORTS;
	return dev - rte_eth_devices;
}

struct rte_eth_dev *
eth_find_device(const struct rte_eth_dev *start, rte_eth_cmp_t cmp,
		const void *data)
{
	struct rte_eth_dev *edev;
	ptrdiff_t idx;

	/* Avoid Undefined Behaviour */
	if (start != NULL &&
	    (start < &rte_eth_devices[0] ||
	     start > &rte_eth_devices[RTE_MAX_ETHPORTS]))
		return NULL;
	if (start != NULL)
		idx = eth_dev_to_id(start) + 1;
	else
		idx = 0;
	for (; idx < RTE_MAX_ETHPORTS; idx++) {
		edev = &rte_eth_devices[idx];
		if (cmp(edev, data) == 0)
			return edev;
	}
	return NULL;
}

/* Put new value into list. */
static int
rte_eth_devargs_enlist(uint16_t *list, uint16_t *len_list,
		       const uint16_t max_list, uint16_t val)
{
	uint16_t i;

	for (i = 0; i < *len_list; i++) {
		if (list[i] == val)
			return 0;
	}
	if (*len_list >= max_list)
		return -1;
	list[(*len_list)++] = val;
	return 0;
}

/* Parse and enlist a range expression of "min-max" or a single value. */
static char *
rte_eth_devargs_process_range(char *str, uint16_t *list, uint16_t *len_list,
	const uint16_t max_list)
{
	uint16_t lo, hi, val;
	int result, n = 0;
	char *pos = str;

	result = sscanf(str, "%hu%n-%hu%n", &lo, &n, &hi, &n);
	if (result == 1) {
		if (rte_eth_devargs_enlist(list, len_list, max_list, lo) != 0)
			return NULL;
	} else if (result == 2) {
		if (lo > hi)
			return NULL;
		for (val = lo; val <= hi; val++) {
			if (rte_eth_devargs_enlist(list, len_list, max_list,
						   val) != 0)
				return NULL;
		}
	} else
		return NULL;
	return pos + n;
}

/*
 * Parse list of values separated by ",".
 * Each value could be a range [min-max] or single number.
 * Examples:
 *  2               - single
 *  [1,2,3]         - single list
 *  [1,3-5,7,9-11]  - list with singles and ranges
 */
static char *
rte_eth_devargs_process_list(char *str, uint16_t *list, uint16_t *len_list,
	const uint16_t max_list)
{
	char *pos = str;

	if (*pos == '[')
		pos++;
	while (1) {
		pos = rte_eth_devargs_process_range(pos, list, len_list,
						    max_list);
		if (pos == NULL)
			return NULL;
		if (*pos != ',') /* end of list */
			break;
		pos++;
	}
	if (*str == '[' && *pos != ']')
		return NULL;
	if (*pos == ']')
		pos++;
	return pos;
}

/*
 * Parse representor ports from a single value or lists.
 *
 * Representor format:
 *   #: range or single number of VF representor - legacy
 *   [[c#]pf#]vf#: VF port representor/s
 *   [[c#]pf#]sf#: SF port representor/s
 *   [c#]pf#:      PF port representor/s
 *
 * Examples of #:
 *  2               - single
 *  [1,2,3]         - single list
 *  [1,3-5,7,9-11]  - list with singles and ranges
 */
int
rte_eth_devargs_parse_representor_ports(char *str, void *data)
{
	struct rte_eth_devargs *eth_da = data;

	if (str[0] == 'c') {
		str += 1;
		str = rte_eth_devargs_process_list(str, eth_da->mh_controllers,
				&eth_da->nb_mh_controllers,
				RTE_DIM(eth_da->mh_controllers));
		if (str == NULL)
			goto done;
	}
	if (str[0] == 'p' && str[1] == 'f') {
		eth_da->type = RTE_ETH_REPRESENTOR_PF;
		str += 2;
		str = rte_eth_devargs_process_list(str, eth_da->ports,
				&eth_da->nb_ports, RTE_DIM(eth_da->ports));
		if (str == NULL || str[0] == '\0')
			goto done;
	} else if (eth_da->nb_mh_controllers > 0) {
		/* 'c' must followed by 'pf'. */
		str = NULL;
		goto done;
	}
	if (str[0] == 'v' && str[1] == 'f') {
		eth_da->type = RTE_ETH_REPRESENTOR_VF;
		str += 2;
	} else if (str[0] == 's' && str[1] == 'f') {
		eth_da->type = RTE_ETH_REPRESENTOR_SF;
		str += 2;
	} else {
		/* 'pf' must followed by 'vf' or 'sf'. */
		if (eth_da->type == RTE_ETH_REPRESENTOR_PF) {
			str = NULL;
			goto done;
		}
		eth_da->type = RTE_ETH_REPRESENTOR_VF;
	}
	str = rte_eth_devargs_process_list(str, eth_da->representor_ports,
		&eth_da->nb_representor_ports,
		RTE_DIM(eth_da->representor_ports));
done:
	if (str == NULL)
		RTE_ETHDEV_LOG_LINE(ERR, "wrong representor format: %s", str);
	return str == NULL ? -1 : 0;
}

struct dummy_queue {
	bool rx_warn_once;
	bool tx_warn_once;
};
static struct dummy_queue *dummy_queues_array[RTE_MAX_ETHPORTS][RTE_MAX_QUEUES_PER_PORT];
static struct dummy_queue per_port_queues[RTE_MAX_ETHPORTS];
RTE_INIT(dummy_queue_init)
{
	uint16_t port_id;

	for (port_id = 0; port_id < RTE_DIM(per_port_queues); port_id++) {
		unsigned int q;

		for (q = 0; q < RTE_DIM(dummy_queues_array[port_id]); q++)
			dummy_queues_array[port_id][q] = &per_port_queues[port_id];
	}
}

static uint16_t
dummy_eth_rx_burst(void *rxq,
		__rte_unused struct rte_mbuf **rx_pkts,
		__rte_unused uint16_t nb_pkts)
{
	struct dummy_queue *queue = rxq;
	uintptr_t port_id;

	port_id = queue - per_port_queues;
	if (port_id < RTE_DIM(per_port_queues) && !queue->rx_warn_once) {
		RTE_ETHDEV_LOG_LINE(ERR, "lcore %u called rx_pkt_burst for not ready port %"PRIuPTR,
			rte_lcore_id(), port_id);
		rte_dump_stack();
		queue->rx_warn_once = true;
	}
	rte_errno = ENOTSUP;
	return 0;
}

static uint16_t
dummy_eth_tx_burst(void *txq,
		__rte_unused struct rte_mbuf **tx_pkts,
		__rte_unused uint16_t nb_pkts)
{
	struct dummy_queue *queue = txq;
	uintptr_t port_id;

	port_id = queue - per_port_queues;
	if (port_id < RTE_DIM(per_port_queues) && !queue->tx_warn_once) {
		RTE_ETHDEV_LOG_LINE(ERR, "lcore %u called tx_pkt_burst for not ready port %"PRIuPTR,
			rte_lcore_id(), port_id);
		rte_dump_stack();
		queue->tx_warn_once = true;
	}
	rte_errno = ENOTSUP;
	return 0;
}

void
eth_dev_fp_ops_reset(struct rte_eth_fp_ops *fpo)
{
	static RTE_ATOMIC(void *) dummy_data[RTE_MAX_QUEUES_PER_PORT];
	uintptr_t port_id = fpo - rte_eth_fp_ops;

	per_port_queues[port_id].rx_warn_once = false;
	per_port_queues[port_id].tx_warn_once = false;
	*fpo = (struct rte_eth_fp_ops) {
		.rx_pkt_burst = dummy_eth_rx_burst,
		.tx_pkt_burst = dummy_eth_tx_burst,
		.rxq = {
			.data = (void **)&dummy_queues_array[port_id],
			.clbk = dummy_data,
		},
		.txq = {
			.data = (void **)&dummy_queues_array[port_id],
			.clbk = dummy_data,
		},
	};
}

void
eth_dev_fp_ops_setup(struct rte_eth_fp_ops *fpo,
		const struct rte_eth_dev *dev)
{
	fpo->rx_pkt_burst = dev->rx_pkt_burst;
	fpo->tx_pkt_burst = dev->tx_pkt_burst;
	fpo->tx_pkt_prepare = dev->tx_pkt_prepare;
	fpo->rx_queue_count = dev->rx_queue_count;
	fpo->rx_descriptor_status = dev->rx_descriptor_status;
	fpo->tx_queue_count = dev->tx_queue_count;
	fpo->tx_descriptor_status = dev->tx_descriptor_status;
	fpo->recycle_tx_mbufs_reuse = dev->recycle_tx_mbufs_reuse;
	fpo->recycle_rx_descriptors_refill = dev->recycle_rx_descriptors_refill;

	fpo->rxq.data = dev->data->rx_queues;
	fpo->rxq.clbk = (void * __rte_atomic *)(uintptr_t)dev->post_rx_burst_cbs;

	fpo->txq.data = dev->data->tx_queues;
	fpo->txq.clbk = (void * __rte_atomic *)(uintptr_t)dev->pre_tx_burst_cbs;
}

RTE_EXPORT_SYMBOL(rte_eth_call_rx_callbacks)
uint16_t
rte_eth_call_rx_callbacks(uint16_t port_id, uint16_t queue_id,
	struct rte_mbuf **rx_pkts, uint16_t nb_rx, uint16_t nb_pkts,
	void *opaque)
{
	const struct rte_eth_rxtx_callback *cb = opaque;

	while (cb != NULL) {
		nb_rx = cb->fn.rx(port_id, queue_id, rx_pkts, nb_rx,
				nb_pkts, cb->param);
		cb = cb->next;
	}

	if (unlikely(nb_rx))
		rte_eth_trace_call_rx_callbacks_nonempty(port_id, queue_id, (void **)rx_pkts,
						nb_rx, nb_pkts);
	else
		rte_eth_trace_call_rx_callbacks_empty(port_id, queue_id, (void **)rx_pkts,
						nb_pkts);

	return nb_rx;
}

RTE_EXPORT_SYMBOL(rte_eth_call_tx_callbacks)
uint16_t
rte_eth_call_tx_callbacks(uint16_t port_id, uint16_t queue_id,
	struct rte_mbuf **tx_pkts, uint16_t nb_pkts, void *opaque)
{
	const struct rte_eth_rxtx_callback *cb = opaque;

	while (cb != NULL) {
		nb_pkts = cb->fn.tx(port_id, queue_id, tx_pkts, nb_pkts,
				cb->param);
		cb = cb->next;
	}

	rte_eth_trace_call_tx_callbacks(port_id, queue_id, (void **)tx_pkts,
					nb_pkts);

	return nb_pkts;
}

void *
eth_dev_shared_data_prepare(void)
{
	const struct rte_memzone *mz;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		const unsigned int flags = 0;

		if (eth_dev_shared_mz != NULL)
			goto out;

		/* Allocate port data and ownership shared memory. */
		mz = rte_memzone_reserve(MZ_RTE_ETH_DEV_DATA,
				sizeof(*eth_dev_shared_data),
				SOCKET_ID_ANY, flags);
		if (mz == NULL) {
			RTE_ETHDEV_LOG_LINE(ERR, "Cannot allocate ethdev shared data");
			goto out;
		}

		eth_dev_shared_mz = mz;
		eth_dev_shared_data = mz->addr;
		eth_dev_shared_data->allocated_owners = 0;
		eth_dev_shared_data->next_owner_id =
			RTE_ETH_DEV_NO_OWNER + 1;
		eth_dev_shared_data->allocated_ports = 0;
		memset(eth_dev_shared_data->data, 0,
		       sizeof(eth_dev_shared_data->data));
	} else {
		mz = rte_memzone_lookup(MZ_RTE_ETH_DEV_DATA);
		if (mz == NULL) {
			/* Clean remaining any traces of a previous shared mem */
			eth_dev_shared_mz = NULL;
			eth_dev_shared_data = NULL;
			RTE_ETHDEV_LOG_LINE(ERR, "Cannot lookup ethdev shared data");
			goto out;
		}
		if (mz == eth_dev_shared_mz && mz->addr == eth_dev_shared_data)
			goto out;

		/* Shared mem changed in primary process, refresh pointers */
		eth_dev_shared_mz = mz;
		eth_dev_shared_data = mz->addr;
	}
out:
	return eth_dev_shared_data;
}

void
eth_dev_shared_data_release(void)
{
	RTE_ASSERT(rte_eal_process_type() == RTE_PROC_PRIMARY);

	if (eth_dev_shared_data->allocated_ports != 0)
		return;
	if (eth_dev_shared_data->allocated_owners != 0)
		return;

	rte_memzone_free(eth_dev_shared_mz);
	eth_dev_shared_mz = NULL;
	eth_dev_shared_data = NULL;
}

void
eth_dev_rxq_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void **rxq = dev->data->rx_queues;

	if (rxq[qid] == NULL)
		return;

	if (dev->dev_ops->rx_queue_release != NULL)
		dev->dev_ops->rx_queue_release(dev, qid);
	rxq[qid] = NULL;
}

void
eth_dev_txq_release(struct rte_eth_dev *dev, uint16_t qid)
{
	void **txq = dev->data->tx_queues;

	if (txq[qid] == NULL)
		return;

	if (dev->dev_ops->tx_queue_release != NULL)
		dev->dev_ops->tx_queue_release(dev, qid);
	txq[qid] = NULL;
}

int
eth_dev_rx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_rx_queues;
	unsigned int i;

	if (dev->data->rx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->rx_queues = rte_zmalloc("ethdev->rx_queues",
				sizeof(dev->data->rx_queues[0]) *
				RTE_MAX_QUEUES_PER_PORT,
				RTE_CACHE_LINE_SIZE);
		if (dev->data->rx_queues == NULL) {
			dev->data->nb_rx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->rx_queues != NULL && nb_queues != 0) { /* re-configure */
		for (i = nb_queues; i < old_nb_queues; i++)
			eth_dev_rxq_release(dev, i);

	} else if (dev->data->rx_queues != NULL && nb_queues == 0) {
		for (i = nb_queues; i < old_nb_queues; i++)
			eth_dev_rxq_release(dev, i);

		rte_free(dev->data->rx_queues);
		dev->data->rx_queues = NULL;
	}
	dev->data->nb_rx_queues = nb_queues;
	return 0;
}

int
eth_dev_tx_queue_config(struct rte_eth_dev *dev, uint16_t nb_queues)
{
	uint16_t old_nb_queues = dev->data->nb_tx_queues;
	unsigned int i;

	if (dev->data->tx_queues == NULL && nb_queues != 0) { /* first time configuration */
		dev->data->tx_queues = rte_zmalloc("ethdev->tx_queues",
				sizeof(dev->data->tx_queues[0]) *
				RTE_MAX_QUEUES_PER_PORT,
				RTE_CACHE_LINE_SIZE);
		if (dev->data->tx_queues == NULL) {
			dev->data->nb_tx_queues = 0;
			return -(ENOMEM);
		}
	} else if (dev->data->tx_queues != NULL && nb_queues != 0) { /* re-configure */
		for (i = nb_queues; i < old_nb_queues; i++)
			eth_dev_txq_release(dev, i);

	} else if (dev->data->tx_queues != NULL && nb_queues == 0) {
		for (i = nb_queues; i < old_nb_queues; i++)
			eth_dev_txq_release(dev, i);

		rte_free(dev->data->tx_queues);
		dev->data->tx_queues = NULL;
	}
	dev->data->nb_tx_queues = nb_queues;
	return 0;
}

static int
ethdev_handle_request(const struct ethdev_mp_request *req)
{
	switch (req->operation) {
	case ETH_REQ_START:
		return rte_eth_dev_start(req->port_id);

	case ETH_REQ_STOP:
		return rte_eth_dev_stop(req->port_id);

	default:
		return -EINVAL;
	}
}

static_assert(sizeof(struct ethdev_mp_request) <= RTE_MP_MAX_PARAM_LEN,
	"ethdev MP request bigger than available param space");

static_assert(sizeof(struct ethdev_mp_response) <= RTE_MP_MAX_PARAM_LEN,
	"ethdev MP response bigger than available param space");

int
ethdev_server(const struct rte_mp_msg *mp_msg, const void *peer)
{
	const struct ethdev_mp_request *req
		= (const struct ethdev_mp_request *)mp_msg->param;

	struct rte_mp_msg mp_resp = {
		.name = ETHDEV_MP,
	};
	struct ethdev_mp_response *resp;

	resp = (struct ethdev_mp_response *)mp_resp.param;
	mp_resp.len_param = sizeof(*resp);
	resp->res_op = req->operation;

	/* recv client requests */
	if (mp_msg->len_param != sizeof(*req))
		resp->err_value = -EINVAL;
	else
		resp->err_value = ethdev_handle_request(req);

	return rte_mp_reply(&mp_resp, peer);
}

int
ethdev_request(enum ethdev_mp_operation operation, uint16_t port_id,
	       uint16_t queue_id __rte_unused)
{
	struct rte_mp_msg mp_req = { };
	struct rte_mp_reply mp_reply;
	struct ethdev_mp_request *req;
	struct timespec ts = {.tv_sec = 5, .tv_nsec = 0};
	int ret;

	req = (struct ethdev_mp_request *)mp_req.param;
	strlcpy(mp_req.name, ETHDEV_MP, RTE_MP_MAX_NAME_LEN);
	mp_req.len_param = sizeof(*req);

	req->operation = operation;
	req->port_id = port_id;

	ret = rte_mp_request_sync(&mp_req, &mp_reply, &ts);
	if (ret == 0) {
		const struct rte_mp_msg *mp_rep = &mp_reply.msgs[0];
		const struct ethdev_mp_response *resp
			= (const struct ethdev_mp_response *)mp_rep->param;

		if (resp->err_value == 0)
			ret = 0;
		else
			rte_errno = -resp->err_value;
		free(mp_reply.msgs);
	}

	if (ret < 0)
		RTE_ETHDEV_LOG_LINE(ERR,
		       "port %up ethdev op %u failed", port_id, operation);
	return ret;
}
