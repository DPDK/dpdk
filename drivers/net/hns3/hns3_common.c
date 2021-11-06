/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#include <rte_kvargs.h>

#include "hns3_logs.h"
#include "hns3_common.h"

static int
hns3_parse_io_hint_func(const char *key, const char *value, void *extra_args)
{
	uint32_t hint = HNS3_IO_FUNC_HINT_NONE;

	RTE_SET_USED(key);

	if (strcmp(value, "vec") == 0)
		hint = HNS3_IO_FUNC_HINT_VEC;
	else if (strcmp(value, "sve") == 0)
		hint = HNS3_IO_FUNC_HINT_SVE;
	else if (strcmp(value, "simple") == 0)
		hint = HNS3_IO_FUNC_HINT_SIMPLE;
	else if (strcmp(value, "common") == 0)
		hint = HNS3_IO_FUNC_HINT_COMMON;

	/* If the hint is valid then update output parameters */
	if (hint != HNS3_IO_FUNC_HINT_NONE)
		*(uint32_t *)extra_args = hint;

	return 0;
}

static const char *
hns3_get_io_hint_func_name(uint32_t hint)
{
	switch (hint) {
	case HNS3_IO_FUNC_HINT_VEC:
		return "vec";
	case HNS3_IO_FUNC_HINT_SVE:
		return "sve";
	case HNS3_IO_FUNC_HINT_SIMPLE:
		return "simple";
	case HNS3_IO_FUNC_HINT_COMMON:
		return "common";
	default:
		return "none";
	}
}

static int
hns3_parse_dev_caps_mask(const char *key, const char *value, void *extra_args)
{
	uint64_t val;

	RTE_SET_USED(key);

	val = strtoull(value, NULL, HNS3_CONVERT_TO_HEXADECIMAL);
	*(uint64_t *)extra_args = val;

	return 0;
}

static int
hns3_parse_mbx_time_limit(const char *key, const char *value, void *extra_args)
{
	uint32_t val;

	RTE_SET_USED(key);

	val = strtoul(value, NULL, HNS3_CONVERT_TO_DECIMAL);
	if (val > HNS3_MBX_DEF_TIME_LIMIT_MS && val <= UINT16_MAX)
		*(uint16_t *)extra_args = val;

	return 0;
}

void
hns3_parse_devargs(struct rte_eth_dev *dev)
{
	uint16_t mbx_time_limit_ms = HNS3_MBX_DEF_TIME_LIMIT_MS;
	struct hns3_adapter *hns = dev->data->dev_private;
	uint32_t rx_func_hint = HNS3_IO_FUNC_HINT_NONE;
	uint32_t tx_func_hint = HNS3_IO_FUNC_HINT_NONE;
	struct hns3_hw *hw = &hns->hw;
	uint64_t dev_caps_mask = 0;
	struct rte_kvargs *kvlist;

	if (dev->device->devargs == NULL)
		return;

	kvlist = rte_kvargs_parse(dev->device->devargs->args, NULL);
	if (!kvlist)
		return;

	(void)rte_kvargs_process(kvlist, HNS3_DEVARG_RX_FUNC_HINT,
			   &hns3_parse_io_hint_func, &rx_func_hint);
	(void)rte_kvargs_process(kvlist, HNS3_DEVARG_TX_FUNC_HINT,
			   &hns3_parse_io_hint_func, &tx_func_hint);
	(void)rte_kvargs_process(kvlist, HNS3_DEVARG_DEV_CAPS_MASK,
			   &hns3_parse_dev_caps_mask, &dev_caps_mask);
	(void)rte_kvargs_process(kvlist, HNS3_DEVARG_MBX_TIME_LIMIT_MS,
			   &hns3_parse_mbx_time_limit, &mbx_time_limit_ms);

	rte_kvargs_free(kvlist);

	if (rx_func_hint != HNS3_IO_FUNC_HINT_NONE)
		hns3_warn(hw, "parsed %s = %s.", HNS3_DEVARG_RX_FUNC_HINT,
			  hns3_get_io_hint_func_name(rx_func_hint));
	hns->rx_func_hint = rx_func_hint;
	if (tx_func_hint != HNS3_IO_FUNC_HINT_NONE)
		hns3_warn(hw, "parsed %s = %s.", HNS3_DEVARG_TX_FUNC_HINT,
			  hns3_get_io_hint_func_name(tx_func_hint));
	hns->tx_func_hint = tx_func_hint;

	if (dev_caps_mask != 0)
		hns3_warn(hw, "parsed %s = 0x%" PRIx64 ".",
			  HNS3_DEVARG_DEV_CAPS_MASK, dev_caps_mask);
	hns->dev_caps_mask = dev_caps_mask;
}

void
hns3_clock_gettime(struct timeval *tv)
{
#ifdef CLOCK_MONOTONIC_RAW /* Defined in glibc bits/time.h */
#define CLOCK_TYPE CLOCK_MONOTONIC_RAW
#else
#define CLOCK_TYPE CLOCK_MONOTONIC
#endif
#define NSEC_TO_USEC_DIV 1000

	struct timespec spec;
	(void)clock_gettime(CLOCK_TYPE, &spec);

	tv->tv_sec = spec.tv_sec;
	tv->tv_usec = spec.tv_nsec / NSEC_TO_USEC_DIV;
}

uint64_t
hns3_clock_calctime_ms(struct timeval *tv)
{
	return (uint64_t)tv->tv_sec * MSEC_PER_SEC +
		tv->tv_usec / USEC_PER_MSEC;
}

uint64_t
hns3_clock_gettime_ms(void)
{
	struct timeval tv;

	hns3_clock_gettime(&tv);
	return hns3_clock_calctime_ms(&tv);
}

void hns3_ether_format_addr(char *buf, uint16_t size,
			    const struct rte_ether_addr *ether_addr)
{
	snprintf(buf, size, "%02X:**:**:**:%02X:%02X",
		ether_addr->addr_bytes[0],
		ether_addr->addr_bytes[4],
		ether_addr->addr_bytes[5]);
}

static int
hns3_set_mc_addr_chk_param(struct hns3_hw *hw,
			   struct rte_ether_addr *mc_addr_set,
			   uint32_t nb_mc_addr)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *addr;
	uint16_t mac_addrs_capa;
	uint32_t i;
	uint32_t j;

	if (nb_mc_addr > HNS3_MC_MACADDR_NUM) {
		hns3_err(hw, "failed to set mc mac addr, nb_mc_addr(%u) "
			 "invalid. valid range: 0~%d",
			 nb_mc_addr, HNS3_MC_MACADDR_NUM);
		return -EINVAL;
	}

	/* Check if input mac addresses are valid */
	for (i = 0; i < nb_mc_addr; i++) {
		addr = &mc_addr_set[i];
		if (!rte_is_multicast_ether_addr(addr)) {
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      addr);
			hns3_err(hw,
				 "failed to set mc mac addr, addr(%s) invalid.",
				 mac_str);
			return -EINVAL;
		}

		/* Check if there are duplicate addresses */
		for (j = i + 1; j < nb_mc_addr; j++) {
			if (rte_is_same_ether_addr(addr, &mc_addr_set[j])) {
				hns3_ether_format_addr(mac_str,
						      RTE_ETHER_ADDR_FMT_SIZE,
						      addr);
				hns3_err(hw, "failed to set mc mac addr, "
					 "addrs invalid. two same addrs(%s).",
					 mac_str);
				return -EINVAL;
			}
		}

		/*
		 * Check if there are duplicate addresses between mac_addrs
		 * and mc_addr_set
		 */
		mac_addrs_capa = hns->is_vf ? HNS3_VF_UC_MACADDR_NUM :
					      HNS3_UC_MACADDR_NUM;
		for (j = 0; j < mac_addrs_capa; j++) {
			if (rte_is_same_ether_addr(addr,
						   &hw->data->mac_addrs[j])) {
				hns3_ether_format_addr(mac_str,
						       RTE_ETHER_ADDR_FMT_SIZE,
						       addr);
				hns3_err(hw, "failed to set mc mac addr, "
					 "addrs invalid. addrs(%s) has already "
					 "configured in mac_addr add API",
					 mac_str);
				return -EINVAL;
			}
		}
	}

	return 0;
}

int
hns3_set_mc_mac_addr_list(struct rte_eth_dev *dev,
			  struct rte_ether_addr *mc_addr_set,
			  uint32_t nb_mc_addr)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct rte_ether_addr *addr;
	int cur_addr_num;
	int set_addr_num;
	int num;
	int ret;
	int i;

	/* Check if input parameters are valid */
	ret = hns3_set_mc_addr_chk_param(hw, mc_addr_set, nb_mc_addr);
	if (ret)
		return ret;

	rte_spinlock_lock(&hw->lock);
	cur_addr_num = hw->mc_addrs_num;
	for (i = 0; i < cur_addr_num; i++) {
		num = cur_addr_num - i - 1;
		addr = &hw->mc_addrs[num];
		ret = hw->ops.del_mc_mac_addr(hw, addr);
		if (ret) {
			rte_spinlock_unlock(&hw->lock);
			return ret;
		}

		hw->mc_addrs_num--;
	}

	set_addr_num = (int)nb_mc_addr;
	for (i = 0; i < set_addr_num; i++) {
		addr = &mc_addr_set[i];
		ret = hw->ops.add_mc_mac_addr(hw, addr);
		if (ret) {
			rte_spinlock_unlock(&hw->lock);
			return ret;
		}

		rte_ether_addr_copy(addr, &hw->mc_addrs[hw->mc_addrs_num]);
		hw->mc_addrs_num++;
	}
	rte_spinlock_unlock(&hw->lock);

	return 0;
}

int
hns3_configure_all_mc_mac_addr(struct hns3_adapter *hns, bool del)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct hns3_hw *hw = &hns->hw;
	struct rte_ether_addr *addr;
	int ret = 0;
	int i;

	for (i = 0; i < hw->mc_addrs_num; i++) {
		addr = &hw->mc_addrs[i];
		if (!rte_is_multicast_ether_addr(addr))
			continue;
		if (del)
			ret = hw->ops.del_mc_mac_addr(hw, addr);
		else
			ret = hw->ops.add_mc_mac_addr(hw, addr);
		if (ret) {
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					      addr);
			hns3_dbg(hw, "failed to %s mc mac addr: %s ret = %d",
				 del ? "Remove" : "Restore", mac_str, ret);
		}
	}
	return ret;
}

int
hns3_configure_all_mac_addr(struct hns3_adapter *hns, bool del)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct hns3_hw *hw = &hns->hw;
	struct hns3_hw_ops *ops = &hw->ops;
	struct rte_ether_addr *addr;
	uint16_t mac_addrs_capa;
	int ret = 0;
	int i;

	mac_addrs_capa =
		hns->is_vf ? HNS3_VF_UC_MACADDR_NUM : HNS3_UC_MACADDR_NUM;
	for (i = 0; i < mac_addrs_capa; i++) {
		addr = &hw->data->mac_addrs[i];
		if (rte_is_zero_ether_addr(addr))
			continue;
		if (rte_is_multicast_ether_addr(addr))
			ret = del ? ops->del_mc_mac_addr(hw, addr) :
			      ops->add_mc_mac_addr(hw, addr);
		else
			ret = del ? ops->del_uc_mac_addr(hw, addr) :
			      ops->add_uc_mac_addr(hw, addr);

		if (ret) {
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					       addr);
			hns3_err(hw, "failed to %s mac addr(%s) index:%d ret = %d.",
				 del ? "remove" : "restore", mac_str, i, ret);
		}
	}

	return ret;
}

static bool
hns3_find_duplicate_mc_addr(struct hns3_hw *hw, struct rte_ether_addr *mc_addr)
{
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *addr;
	int i;

	for (i = 0; i < hw->mc_addrs_num; i++) {
		addr = &hw->mc_addrs[i];
		/* Check if there are duplicate addresses in mc_addrs[] */
		if (rte_is_same_ether_addr(addr, mc_addr)) {
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
					       addr);
			hns3_err(hw, "failed to add mc mac addr, same addrs"
				 "(%s) is added by the set_mc_mac_addr_list "
				 "API", mac_str);
			return true;
		}
	}

	return false;
}

int
hns3_add_mac_addr(struct rte_eth_dev *dev, struct rte_ether_addr *mac_addr,
		  __rte_unused uint32_t idx, __rte_unused uint32_t pool)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	rte_spinlock_lock(&hw->lock);

	/*
	 * In hns3 network engine adding UC and MC mac address with different
	 * commands with firmware. We need to determine whether the input
	 * address is a UC or a MC address to call different commands.
	 * By the way, it is recommended calling the API function named
	 * rte_eth_dev_set_mc_addr_list to set the MC mac address, because
	 * using the rte_eth_dev_mac_addr_add API function to set MC mac address
	 * may affect the specifications of UC mac addresses.
	 */
	if (rte_is_multicast_ether_addr(mac_addr)) {
		if (hns3_find_duplicate_mc_addr(hw, mac_addr)) {
			rte_spinlock_unlock(&hw->lock);
			return -EINVAL;
		}
		ret = hw->ops.add_mc_mac_addr(hw, mac_addr);
	} else {
		ret = hw->ops.add_uc_mac_addr(hw, mac_addr);
	}
	rte_spinlock_unlock(&hw->lock);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to add mac addr(%s), ret = %d", mac_str,
			 ret);
	}

	return ret;
}

void
hns3_remove_mac_addr(struct rte_eth_dev *dev, uint32_t idx)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	/* index will be checked by upper level rte interface */
	struct rte_ether_addr *mac_addr = &dev->data->mac_addrs[idx];
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	int ret;

	rte_spinlock_lock(&hw->lock);

	if (rte_is_multicast_ether_addr(mac_addr))
		ret = hw->ops.del_mc_mac_addr(hw, mac_addr);
	else
		ret = hw->ops.del_uc_mac_addr(hw, mac_addr);
	rte_spinlock_unlock(&hw->lock);
	if (ret) {
		hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				      mac_addr);
		hns3_err(hw, "failed to remove mac addr(%s), ret = %d", mac_str,
			 ret);
	}
}
