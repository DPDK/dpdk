/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Gaëtan Rivet
 */

#include "rte_ethdev.h"
#include "ethdev_driver.h"
#include "ethdev_private.h"

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
		RTE_LOG(ERR, EAL, "wrong representor format: %s\n", str);
	return str == NULL ? -1 : 0;
}

static uint16_t
dummy_eth_rx_burst(__rte_unused void *rxq,
		__rte_unused struct rte_mbuf **rx_pkts,
		__rte_unused uint16_t nb_pkts)
{
	RTE_ETHDEV_LOG(ERR, "rx_pkt_burst for not ready port\n");
	rte_errno = ENOTSUP;
	return 0;
}

static uint16_t
dummy_eth_tx_burst(__rte_unused void *txq,
		__rte_unused struct rte_mbuf **tx_pkts,
		__rte_unused uint16_t nb_pkts)
{
	RTE_ETHDEV_LOG(ERR, "tx_pkt_burst for not ready port\n");
	rte_errno = ENOTSUP;
	return 0;
}

void
eth_dev_fp_ops_reset(struct rte_eth_fp_ops *fpo)
{
	static void *dummy_data[RTE_MAX_QUEUES_PER_PORT];
	static const struct rte_eth_fp_ops dummy_ops = {
		.rx_pkt_burst = dummy_eth_rx_burst,
		.tx_pkt_burst = dummy_eth_tx_burst,
		.rxq = {.data = dummy_data, .clbk = dummy_data,},
		.txq = {.data = dummy_data, .clbk = dummy_data,},
	};

	*fpo = dummy_ops;
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
	fpo->tx_descriptor_status = dev->tx_descriptor_status;

	fpo->rxq.data = dev->data->rx_queues;
	fpo->rxq.clbk = (void **)(uintptr_t)dev->post_rx_burst_cbs;

	fpo->txq.data = dev->data->tx_queues;
	fpo->txq.clbk = (void **)(uintptr_t)dev->pre_tx_burst_cbs;
}

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

	return nb_rx;
}

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

	return nb_pkts;
}
