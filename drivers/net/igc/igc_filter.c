/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2020 Intel Corporation
 */

#include "rte_malloc.h"
#include "igc_logs.h"
#include "igc_txrx.h"
#include "igc_filter.h"
#include "igc_flow.h"

/*
 * igc_ethertype_filter_lookup - lookup ether-type filter
 *
 * @igc, IGC filter pointer
 * @ethertype, ethernet type
 * @empty, a place to store the index of empty entry if the item not found
 *  it's not smaller than 0 if valid, otherwise -1 for no empty entry.
 *  empty parameter is only valid if the return value of the function is -1
 *
 * Return value
 * >= 0, item index of the ether-type filter
 * -1, the item not been found
 */
static inline int
igc_ethertype_filter_lookup(const struct igc_adapter *igc,
			uint16_t ethertype, int *empty)
{
	int i = 0;

	if (empty) {
		/* set to invalid valid */
		*empty = -1;

		/* search the filters array */
		for (; i < IGC_MAX_ETQF_FILTERS; i++) {
			if (igc->ethertype_filters[i].ether_type == ethertype)
				return i;
			if (igc->ethertype_filters[i].ether_type == 0) {
				/* get empty entry */
				*empty = i;
				i++;
				break;
			}
		}
	}

	/* search the rest of filters */
	for (; i < IGC_MAX_ETQF_FILTERS; i++) {
		if (igc->ethertype_filters[i].ether_type == ethertype)
			return i;	/* filter be found, return index */
	}

	return -1;
}

int
igc_del_ethertype_filter(struct rte_eth_dev *dev,
			const struct igc_ethertype_filter *filter)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	int ret;

	if (filter->ether_type == 0) {
		PMD_DRV_LOG(ERR, "Ethertype 0 is not been supported");
		return -EINVAL;
	}

	ret = igc_ethertype_filter_lookup(igc, filter->ether_type, NULL);
	if (ret < 0) {
		/* not found */
		PMD_DRV_LOG(ERR,
			"Ethertype (0x%04x) filter doesn't exist",
			filter->ether_type);
		return -ENOENT;
	}

	igc->ethertype_filters[ret].ether_type = 0;

	IGC_WRITE_REG(hw, IGC_ETQF(ret), 0);
	IGC_WRITE_FLUSH(hw);
	return 0;
}

int
igc_add_ethertype_filter(struct rte_eth_dev *dev,
			const struct igc_ethertype_filter *filter)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	uint32_t etqf;
	int ret, empty;

	if (filter->ether_type == RTE_ETHER_TYPE_IPV4 ||
		filter->ether_type == RTE_ETHER_TYPE_IPV6 ||
		filter->ether_type == 0) {
		PMD_DRV_LOG(ERR,
			"Unsupported ether_type(0x%04x) in ethertype filter",
			filter->ether_type);
		return -EINVAL;
	}

	ret = igc_ethertype_filter_lookup(igc, filter->ether_type, &empty);
	if (ret >= 0) {
		PMD_DRV_LOG(ERR, "ethertype (0x%04x) filter exists.",
				filter->ether_type);
		return -EEXIST;
	}

	if (empty < 0) {
		PMD_DRV_LOG(ERR, "no ethertype filter entry.");
		return -ENOSPC;
	}
	ret = empty;

	etqf = filter->ether_type;
	etqf |= IGC_ETQF_FILTER_ENABLE | IGC_ETQF_QUEUE_ENABLE;
	etqf |= (uint32_t)filter->queue << IGC_ETQF_QUEUE_SHIFT;

	memcpy(&igc->ethertype_filters[ret], filter, sizeof(*filter));

	IGC_WRITE_REG(hw, IGC_ETQF(ret), etqf);
	IGC_WRITE_FLUSH(hw);
	return 0;
}

/* clear all the ether type filters */
static void
igc_clear_all_ethertype_filter(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	int i;

	for (i = 0; i < IGC_MAX_ETQF_FILTERS; i++)
		IGC_WRITE_REG(hw, IGC_ETQF(i), 0);
	IGC_WRITE_FLUSH(hw);

	memset(&igc->ethertype_filters, 0, sizeof(igc->ethertype_filters));
}

/*
 * igc_tuple_filter_lookup - lookup n-tuple filter
 *
 * @igc, igc filter pointer
 * @ntuple, n-tuple filter pointer
 * @empty, a place to store the index of empty entry if the item not found
 *  it's not smaller than 0 if valid, otherwise -1 for no empty entry.
 *  The value of empty is uncertain if the return value of the function is
 *  not -1.
 *
 * Return value
 * >= 0, item index of the filter
 * -1, the item not been found
 */
static int
igc_tuple_filter_lookup(const struct igc_adapter *igc,
			const struct igc_ntuple_filter *ntuple,
			int *empty)
{
	int i = 0;

	if (empty) {
		/* set initial value */
		*empty = -1;

		/* search the filter array */
		for (; i < IGC_MAX_NTUPLE_FILTERS; i++) {
			if (igc->ntuple_filters[i].hash_val) {
				/* compare the hase value */
				if (ntuple->hash_val ==
					igc->ntuple_filters[i].hash_val)
					/* filter be found, return index */
					return i;
			} else {
				/* get the empty entry */
				*empty = i;
				i++;
				break;
			}
		}
	}

	/* search the rest of filters */
	for (; i < IGC_MAX_NTUPLE_FILTERS; i++) {
		if (ntuple->hash_val == igc->ntuple_filters[i].hash_val)
			/* filter be found, return index */
			return i;
	}

	return -1;
}

/* Set hardware register values */
static void
igc_enable_tuple_filter(struct rte_eth_dev *dev,
			const struct igc_adapter *igc, uint8_t index)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	const struct igc_ntuple_filter *filter = &igc->ntuple_filters[index];
	const struct igc_ntuple_info *info = &filter->tuple_info;
	uint32_t ttqf, imir, imir_ext = IGC_IMIREXT_SIZE_BP;

	imir = info->dst_port;
	imir |= (uint32_t)info->priority << IGC_IMIR_PRIORITY_SHIFT;

	/* 0b means not compare. */
	if (info->dst_port_mask == 0)
		imir |= IGC_IMIR_PORT_BP;

	ttqf = IGC_TTQF_DISABLE_MASK | IGC_TTQF_QUEUE_ENABLE;
	ttqf |= (uint32_t)filter->queue << IGC_TTQF_QUEUE_SHIFT;
	ttqf |= info->proto;

	if (info->proto_mask)
		ttqf &= ~IGC_TTQF_MASK_ENABLE;

	/* TCP flags bits setting. */
	if (info->tcp_flags & RTE_NTUPLE_TCP_FLAGS_MASK) {
		if (info->tcp_flags & RTE_TCP_URG_FLAG)
			imir_ext |= IGC_IMIREXT_CTRL_URG;
		if (info->tcp_flags & RTE_TCP_ACK_FLAG)
			imir_ext |= IGC_IMIREXT_CTRL_ACK;
		if (info->tcp_flags & RTE_TCP_PSH_FLAG)
			imir_ext |= IGC_IMIREXT_CTRL_PSH;
		if (info->tcp_flags & RTE_TCP_RST_FLAG)
			imir_ext |= IGC_IMIREXT_CTRL_RST;
		if (info->tcp_flags & RTE_TCP_SYN_FLAG)
			imir_ext |= IGC_IMIREXT_CTRL_SYN;
		if (info->tcp_flags & RTE_TCP_FIN_FLAG)
			imir_ext |= IGC_IMIREXT_CTRL_FIN;
	} else {
		imir_ext |= IGC_IMIREXT_CTRL_BP;
	}

	IGC_WRITE_REG(hw, IGC_IMIR(index), imir);
	IGC_WRITE_REG(hw, IGC_TTQF(index), ttqf);
	IGC_WRITE_REG(hw, IGC_IMIREXT(index), imir_ext);
	IGC_WRITE_FLUSH(hw);
}

/* Reset hardware register values */
static void
igc_disable_tuple_filter(struct rte_eth_dev *dev, uint8_t index)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);

	IGC_WRITE_REG(hw, IGC_TTQF(index), IGC_TTQF_DISABLE_MASK);
	IGC_WRITE_REG(hw, IGC_IMIR(index), 0);
	IGC_WRITE_REG(hw, IGC_IMIREXT(index), 0);
	IGC_WRITE_FLUSH(hw);
}

int
igc_add_ntuple_filter(struct rte_eth_dev *dev,
		const struct igc_ntuple_filter *ntuple)
{
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	int ret, empty;

	ret = igc_tuple_filter_lookup(igc, ntuple, &empty);
	if (ret >= 0) {
		PMD_DRV_LOG(ERR, "filter exists.");
		return -EEXIST;
	}

	if (empty < 0) {
		PMD_DRV_LOG(ERR, "filter no entry.");
		return -ENOSPC;
	}

	ret = empty;
	memcpy(&igc->ntuple_filters[ret], ntuple, sizeof(*ntuple));
	igc_enable_tuple_filter(dev, igc, (uint8_t)ret);
	return 0;
}

int
igc_del_ntuple_filter(struct rte_eth_dev *dev,
		const struct igc_ntuple_filter *ntuple)
{
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	int ret;

	ret = igc_tuple_filter_lookup(igc, ntuple, NULL);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "filter not exists.");
		return -ENOENT;
	}

	memset(&igc->ntuple_filters[ret], 0, sizeof(*ntuple));
	igc_disable_tuple_filter(dev, (uint8_t)ret);
	return 0;
}

/* Clear all the n-tuple filters */
static void
igc_clear_all_ntuple_filter(struct rte_eth_dev *dev)
{
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);
	int i;

	for (i = 0; i < IGC_MAX_NTUPLE_FILTERS; i++)
		igc_disable_tuple_filter(dev, i);

	memset(&igc->ntuple_filters, 0, sizeof(igc->ntuple_filters));
}

int
igc_set_syn_filter(struct rte_eth_dev *dev,
		const struct igc_syn_filter *filter)
{
	struct igc_hw *hw;
	struct igc_adapter *igc;
	uint32_t synqf, rfctl;

	if (filter->queue >= IGC_QUEUE_PAIRS_NUM) {
		PMD_DRV_LOG(ERR, "out of range queue %u(max is %u)",
			filter->queue, IGC_QUEUE_PAIRS_NUM);
		return -EINVAL;
	}

	igc = IGC_DEV_PRIVATE(dev);

	if (igc->syn_filter.enable) {
		PMD_DRV_LOG(ERR, "SYN filter has been enabled before!");
		return -EEXIST;
	}

	hw = IGC_DEV_PRIVATE_HW(dev);
	synqf = (uint32_t)filter->queue << IGC_SYN_FILTER_QUEUE_SHIFT;
	synqf |= IGC_SYN_FILTER_ENABLE;

	rfctl = IGC_READ_REG(hw, IGC_RFCTL);
	if (filter->hig_pri)
		rfctl |= IGC_RFCTL_SYNQFP;
	else
		rfctl &= ~IGC_RFCTL_SYNQFP;

	memcpy(&igc->syn_filter, filter, sizeof(igc->syn_filter));
	igc->syn_filter.enable = 1;

	IGC_WRITE_REG(hw, IGC_RFCTL, rfctl);
	IGC_WRITE_REG(hw, IGC_SYNQF(0), synqf);
	IGC_WRITE_FLUSH(hw);
	return 0;
}

/* clear the SYN filter */
void
igc_clear_syn_filter(struct rte_eth_dev *dev)
{
	struct igc_hw *hw = IGC_DEV_PRIVATE_HW(dev);
	struct igc_adapter *igc = IGC_DEV_PRIVATE(dev);

	IGC_WRITE_REG(hw, IGC_SYNQF(0), 0);
	IGC_WRITE_FLUSH(hw);

	memset(&igc->syn_filter, 0, sizeof(igc->syn_filter));
}

void
igc_clear_all_filter(struct rte_eth_dev *dev)
{
	igc_clear_all_ethertype_filter(dev);
	igc_clear_all_ntuple_filter(dev);
	igc_clear_syn_filter(dev);
	igc_clear_rss_filter(dev);
}

int
eth_igc_filter_ctrl(struct rte_eth_dev *dev, enum rte_filter_type filter_type,
		enum rte_filter_op filter_op, void *arg)
{
	int ret = 0;

	RTE_SET_USED(dev);

	switch (filter_type) {
	case RTE_ETH_FILTER_GENERIC:
		if (filter_op != RTE_ETH_FILTER_GET)
			return -EINVAL;
		*(const void **)arg = &igc_flow_ops;
		break;
	default:
		PMD_DRV_LOG(WARNING, "Filter type (%d) not supported",
							filter_type);
		ret = -EINVAL;
	}

	return ret;
}
