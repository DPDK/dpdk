/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 HiSilicon Limited
 */

#include <rte_kvargs.h>
#include <bus_pci_driver.h>
#include <ethdev_pci.h>
#include <rte_pci.h>

#include "hns3_logs.h"
#include "hns3_regs.h"
#include "hns3_rxtx.h"
#include "hns3_dcb.h"
#include "hns3_common.h"

int
hns3_fw_version_get(struct rte_eth_dev *eth_dev, char *fw_version,
		    size_t fw_size)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint32_t version = hw->fw_version;
	int ret;

	ret = snprintf(fw_version, fw_size, "%lu.%lu.%lu.%lu",
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE3_M,
				      HNS3_FW_VERSION_BYTE3_S),
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE2_M,
				      HNS3_FW_VERSION_BYTE2_S),
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE1_M,
				      HNS3_FW_VERSION_BYTE1_S),
		       hns3_get_field(version, HNS3_FW_VERSION_BYTE0_M,
				      HNS3_FW_VERSION_BYTE0_S));
	if (ret < 0)
		return -EINVAL;

	ret += 1; /* add the size of '\0' */
	if (fw_size < (size_t)ret)
		return ret;
	else
		return 0;
}

int
hns3_dev_infos_get(struct rte_eth_dev *eth_dev, struct rte_eth_dev_info *info)
{
	struct hns3_adapter *hns = eth_dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint16_t queue_num = hw->tqps_num;

	/*
	 * In interrupt mode, 'max_rx_queues' is set based on the number of
	 * MSI-X interrupt resources of the hardware.
	 */
	if (hw->data->dev_conf.intr_conf.rxq == 1)
		queue_num = hw->intr_tqps_num;

	info->max_rx_queues = queue_num;
	info->max_tx_queues = hw->tqps_num;
	info->max_rx_pktlen = HNS3_MAX_FRAME_LEN; /* CRC included */
	info->min_rx_bufsize = HNS3_MIN_BD_BUF_SIZE;
	info->max_mtu = info->max_rx_pktlen - HNS3_ETH_OVERHEAD;
	info->max_lro_pkt_size = HNS3_MAX_LRO_SIZE;
	info->rx_offload_capa = (RTE_ETH_RX_OFFLOAD_IPV4_CKSUM |
				 RTE_ETH_RX_OFFLOAD_TCP_CKSUM |
				 RTE_ETH_RX_OFFLOAD_UDP_CKSUM |
				 RTE_ETH_RX_OFFLOAD_SCTP_CKSUM |
				 RTE_ETH_RX_OFFLOAD_OUTER_IPV4_CKSUM |
				 RTE_ETH_RX_OFFLOAD_OUTER_UDP_CKSUM |
				 RTE_ETH_RX_OFFLOAD_SCATTER |
				 RTE_ETH_RX_OFFLOAD_VLAN_STRIP |
				 RTE_ETH_RX_OFFLOAD_VLAN_FILTER |
				 RTE_ETH_RX_OFFLOAD_RSS_HASH |
				 RTE_ETH_RX_OFFLOAD_TCP_LRO);
	info->tx_offload_capa = (RTE_ETH_TX_OFFLOAD_OUTER_IPV4_CKSUM |
				 RTE_ETH_TX_OFFLOAD_IPV4_CKSUM |
				 RTE_ETH_TX_OFFLOAD_TCP_CKSUM |
				 RTE_ETH_TX_OFFLOAD_UDP_CKSUM |
				 RTE_ETH_TX_OFFLOAD_SCTP_CKSUM |
				 RTE_ETH_TX_OFFLOAD_MULTI_SEGS |
				 RTE_ETH_TX_OFFLOAD_TCP_TSO |
				 RTE_ETH_TX_OFFLOAD_VXLAN_TNL_TSO |
				 RTE_ETH_TX_OFFLOAD_GRE_TNL_TSO |
				 RTE_ETH_TX_OFFLOAD_GENEVE_TNL_TSO |
				 RTE_ETH_TX_OFFLOAD_MBUF_FAST_FREE |
				 RTE_ETH_TX_OFFLOAD_VLAN_INSERT);

	if (!hw->port_base_vlan_cfg.state)
		info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_QINQ_INSERT;

	if (hns3_dev_get_support(hw, OUTER_UDP_CKSUM))
		info->tx_offload_capa |= RTE_ETH_TX_OFFLOAD_OUTER_UDP_CKSUM;

	info->dev_capa = RTE_ETH_DEV_CAPA_FLOW_RULE_KEEP |
			 RTE_ETH_DEV_CAPA_FLOW_SHARED_OBJECT_KEEP;
	if (hns3_dev_get_support(hw, INDEP_TXRX))
		info->dev_capa |= RTE_ETH_DEV_CAPA_RUNTIME_RX_QUEUE_SETUP |
				  RTE_ETH_DEV_CAPA_RUNTIME_TX_QUEUE_SETUP;

	if (hns3_dev_get_support(hw, PTP))
		info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_TIMESTAMP;

	info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = HNS3_MAX_RING_DESC,
		.nb_min = HNS3_MIN_RING_DESC,
		.nb_align = HNS3_ALIGN_RING_DESC,
	};

	info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = HNS3_MAX_RING_DESC,
		.nb_min = HNS3_MIN_RING_DESC,
		.nb_align = HNS3_ALIGN_RING_DESC,
		.nb_seg_max = HNS3_MAX_TSO_BD_PER_PKT,
		.nb_mtu_seg_max = hw->max_non_tso_bd_num,
	};

	info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_free_thresh = HNS3_DEFAULT_RX_FREE_THRESH,
		/*
		 * If there are no available Rx buffer descriptors, incoming
		 * packets are always dropped by hardware based on hns3 network
		 * engine.
		 */
		.rx_drop_en = 1,
		.offloads = 0,
	};
	info->default_txconf = (struct rte_eth_txconf) {
		.tx_rs_thresh = HNS3_DEFAULT_TX_RS_THRESH,
		.offloads = 0,
	};

	info->reta_size = hw->rss_ind_tbl_size;
	info->hash_key_size = hw->rss_key_size;
	info->flow_type_rss_offloads = HNS3_ETH_RSS_SUPPORT;

	info->default_rxportconf.burst_size = HNS3_DEFAULT_PORT_CONF_BURST_SIZE;
	info->default_txportconf.burst_size = HNS3_DEFAULT_PORT_CONF_BURST_SIZE;
	info->default_rxportconf.nb_queues = HNS3_DEFAULT_PORT_CONF_QUEUES_NUM;
	info->default_txportconf.nb_queues = HNS3_DEFAULT_PORT_CONF_QUEUES_NUM;
	info->default_rxportconf.ring_size = HNS3_DEFAULT_RING_DESC;
	info->default_txportconf.ring_size = HNS3_DEFAULT_RING_DESC;

	/*
	 * Next is the PF/VF difference section.
	 */
	if (!hns->is_vf) {
		info->max_mac_addrs = HNS3_UC_MACADDR_NUM;
		info->rx_offload_capa |= RTE_ETH_RX_OFFLOAD_KEEP_CRC;
		info->speed_capa = hns3_get_speed_capa(hw);
	} else {
		info->max_mac_addrs = HNS3_VF_UC_MACADDR_NUM;
	}

	info->err_handle_mode = RTE_ETH_ERROR_HANDLE_MODE_PROACTIVE;

	return 0;
}

static int
hns3_parse_io_hint_func(const char *key, const char *value, void *extra_args)
{
	uint32_t hint = HNS3_IO_FUNC_HINT_NONE;

	RTE_SET_USED(key);

	if (value == NULL || extra_args == NULL)
		return 0;

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

	if (value == NULL || extra_args == NULL)
		return 0;

	val = strtoull(value, NULL, HNS3_CONVERT_TO_HEXADECIMAL);
	*(uint64_t *)extra_args = val;

	return 0;
}

static int
hns3_parse_mbx_time_limit(const char *key, const char *value, void *extra_args)
{
	uint32_t val;

	RTE_SET_USED(key);

	if (value == NULL || extra_args == NULL)
		return 0;

	val = strtoul(value, NULL, HNS3_CONVERT_TO_DECIMAL);

	/*
	 * 500ms is empirical value in process of mailbox communication. If
	 * the delay value is set to one lower than the empirical value, mailbox
	 * communication may fail.
	 */
	if (val > HNS3_MBX_DEF_TIME_LIMIT_MS && val <= UINT16_MAX)
		*(uint16_t *)extra_args = val;

	return 0;
}

static int
hns3_parse_vlan_match_mode(const char *key, const char *value, void *args)
{
	uint8_t mode;

	RTE_SET_USED(key);

	if (value == NULL) {
		PMD_INIT_LOG(WARNING, "no value for key:\"%s\"", key);
		return -1;
	}

	if (strcmp(value, "strict") == 0) {
		mode = HNS3_FDIR_VLAN_STRICT_MATCH;
	} else if (strcmp(value, "nostrict") == 0) {
		mode = HNS3_FDIR_VLAN_NOSTRICT_MATCH;
	} else {
		PMD_INIT_LOG(WARNING, "invalid value:\"%s\" for key:\"%s\", "
			"value must be 'strict' or 'nostrict'",
			value, key);
		return -1;
	}

	*(uint8_t *)args = mode;

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

	/* Set default value of runtime config parameters. */
	hns->rx_func_hint = HNS3_IO_FUNC_HINT_NONE;
	hns->tx_func_hint = HNS3_IO_FUNC_HINT_NONE;
	hns->dev_caps_mask = 0;
	hns->mbx_time_limit_ms = HNS3_MBX_DEF_TIME_LIMIT_MS;
	if (!hns->is_vf)
		hns->pf.fdir.vlan_match_mode = HNS3_FDIR_VLAN_STRICT_MATCH;

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
	if (!hns->is_vf)
		(void)rte_kvargs_process(kvlist,
					 HNS3_DEVARG_FDIR_VALN_MATCH_MODE,
					 &hns3_parse_vlan_match_mode,
					 &hns->pf.fdir.vlan_match_mode);

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

	if (mbx_time_limit_ms != HNS3_MBX_DEF_TIME_LIMIT_MS)
		hns3_warn(hw, "parsed %s = %u.", HNS3_DEVARG_MBX_TIME_LIMIT_MS,
				mbx_time_limit_ms);
	hns->mbx_time_limit_ms = mbx_time_limit_ms;
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
	(void)snprintf(buf, size, "%02X:**:**:**:%02X:%02X",
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
	uint16_t i;

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
			hns3_err(hw, "failed to %s mac addr(%s) index:%u ret = %d.",
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

int
hns3_init_mac_addrs(struct rte_eth_dev *dev)
{
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	const char *memory_name = hns->is_vf ? "hns3vf-mac" : "hns3-mac";
	uint16_t mac_addrs_capa = hns->is_vf ? HNS3_VF_UC_MACADDR_NUM :
						HNS3_UC_MACADDR_NUM;
	char mac_str[RTE_ETHER_ADDR_FMT_SIZE];
	struct rte_ether_addr *eth_addr;

	/* Allocate memory for storing MAC addresses */
	dev->data->mac_addrs = rte_zmalloc(memory_name,
				sizeof(struct rte_ether_addr) * mac_addrs_capa,
				0);
	if (dev->data->mac_addrs == NULL) {
		hns3_err(hw, "failed to allocate %zx bytes needed to store MAC addresses",
			 sizeof(struct rte_ether_addr) * mac_addrs_capa);
		return -ENOMEM;
	}

	eth_addr = (struct rte_ether_addr *)hw->mac.mac_addr;
	if (!hns->is_vf) {
		if (!rte_is_valid_assigned_ether_addr(eth_addr)) {
			rte_eth_random_addr(hw->mac.mac_addr);
			hns3_ether_format_addr(mac_str, RTE_ETHER_ADDR_FMT_SIZE,
				(struct rte_ether_addr *)hw->mac.mac_addr);
			hns3_warn(hw, "default mac_addr from firmware is an invalid "
				  "unicast address, using random MAC address %s",
				  mac_str);
		}
	} else {
		/*
		 * The hns3 PF ethdev driver in kernel support setting VF MAC
		 * address on the host by "ip link set ..." command. To avoid
		 * some incorrect scenes, for example, hns3 VF PMD driver fails
		 * to receive and send packets after user configure the MAC
		 * address by using the "ip link set ..." command, hns3 VF PMD
		 * driver keep the same MAC address strategy as the hns3 kernel
		 * ethdev driver in the initialization. If user configure a MAC
		 * address by the ip command for VF device, then hns3 VF PMD
		 * driver will start with it, otherwise start with a random MAC
		 * address in the initialization.
		 */
		if (rte_is_zero_ether_addr(eth_addr))
			rte_eth_random_addr(hw->mac.mac_addr);
	}

	rte_ether_addr_copy((struct rte_ether_addr *)hw->mac.mac_addr,
			    &dev->data->mac_addrs[0]);

	return 0;
}

int
hns3_init_ring_with_vector(struct hns3_hw *hw)
{
	uint16_t vec;
	uint16_t i;
	int ret;

	/*
	 * In hns3 network engine, vector 0 is always the misc interrupt of this
	 * function, vector 1~N can be used respectively for the queues of the
	 * function. Tx and Rx queues with the same number share the interrupt
	 * vector. In the initialization clearing the all hardware mapping
	 * relationship configurations between queues and interrupt vectors is
	 * needed, so some error caused by the residual configurations, such as
	 * the unexpected Tx interrupt, can be avoid.
	 */
	vec = hw->num_msi - 1; /* vector 0 for misc interrupt, not for queue */
	if (hw->intr.mapping_mode == HNS3_INTR_MAPPING_VEC_RSV_ONE)
		vec = vec - 1; /* the last interrupt is reserved */
	hw->intr_tqps_num = RTE_MIN(vec, hw->tqps_num);
	for (i = 0; i < hw->intr_tqps_num; i++) {
		/*
		 * Set gap limiter/rate limiter/quantity limiter algorithm
		 * configuration for interrupt coalesce of queue's interrupt.
		 */
		hns3_set_queue_intr_gl(hw, i, HNS3_RING_GL_RX,
				       HNS3_TQP_INTR_GL_DEFAULT);
		hns3_set_queue_intr_gl(hw, i, HNS3_RING_GL_TX,
				       HNS3_TQP_INTR_GL_DEFAULT);
		hns3_set_queue_intr_rl(hw, i, HNS3_TQP_INTR_RL_DEFAULT);
		/*
		 * QL(quantity limiter) is not used currently, just set 0 to
		 * close it.
		 */
		hns3_set_queue_intr_ql(hw, i, HNS3_TQP_INTR_QL_DEFAULT);

		ret = hw->ops.bind_ring_with_vector(hw, vec, false,
						    HNS3_RING_TYPE_TX, i);
		if (ret) {
			PMD_INIT_LOG(ERR, "fail to unbind TX ring(%u) with vector: %u, ret=%d",
				     i, vec, ret);
			return ret;
		}

		ret = hw->ops.bind_ring_with_vector(hw, vec, false,
						    HNS3_RING_TYPE_RX, i);
		if (ret) {
			PMD_INIT_LOG(ERR, "fail to unbind RX ring(%d) with vector: %u, ret=%d",
				     i, vec, ret);
			return ret;
		}
	}

	return 0;
}

int
hns3_map_rx_interrupt(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct hns3_hw *hw = HNS3_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t base = RTE_INTR_VEC_ZERO_OFFSET;
	uint16_t vec = RTE_INTR_VEC_ZERO_OFFSET;
	uint32_t intr_vector;
	uint16_t q_id;
	int ret;

	/*
	 * hns3 needs a separate interrupt to be used as event interrupt which
	 * could not be shared with task queue pair, so KERNEL drivers need
	 * support multiple interrupt vectors.
	 */
	if (dev->data->dev_conf.intr_conf.rxq == 0 ||
	    !rte_intr_cap_multiple(intr_handle))
		return 0;

	rte_intr_disable(intr_handle);
	intr_vector = hw->used_rx_queues;
	/* creates event fd for each intr vector when MSIX is used */
	if (rte_intr_efd_enable(intr_handle, intr_vector))
		return -EINVAL;

	/* Allocate vector list */
	if (rte_intr_vec_list_alloc(intr_handle, "intr_vec",
				    hw->used_rx_queues)) {
		hns3_err(hw, "failed to allocate %u rx_queues intr_vec",
			 hw->used_rx_queues);
		ret = -ENOMEM;
		goto alloc_intr_vec_error;
	}

	if (rte_intr_allow_others(intr_handle)) {
		vec = RTE_INTR_VEC_RXTX_OFFSET;
		base = RTE_INTR_VEC_RXTX_OFFSET;
	}

	for (q_id = 0; q_id < hw->used_rx_queues; q_id++) {
		ret = hw->ops.bind_ring_with_vector(hw, vec, true,
						    HNS3_RING_TYPE_RX, q_id);
		if (ret)
			goto bind_vector_error;

		if (rte_intr_vec_list_index_set(intr_handle, q_id, vec))
			goto bind_vector_error;
		/*
		 * If there are not enough efds (e.g. not enough interrupt),
		 * remaining queues will be bond to the last interrupt.
		 */
		if (vec < base + rte_intr_nb_efd_get(intr_handle) - 1)
			vec++;
	}
	rte_intr_enable(intr_handle);
	return 0;

bind_vector_error:
	rte_intr_vec_list_free(intr_handle);
alloc_intr_vec_error:
	rte_intr_efd_disable(intr_handle);
	return ret;
}

void
hns3_unmap_rx_interrupt(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	struct hns3_adapter *hns = dev->data->dev_private;
	struct hns3_hw *hw = &hns->hw;
	uint8_t base = RTE_INTR_VEC_ZERO_OFFSET;
	uint8_t vec = RTE_INTR_VEC_ZERO_OFFSET;
	uint16_t q_id;

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return;

	/* unmap the ring with vector */
	if (rte_intr_allow_others(intr_handle)) {
		vec = RTE_INTR_VEC_RXTX_OFFSET;
		base = RTE_INTR_VEC_RXTX_OFFSET;
	}
	if (rte_intr_dp_is_en(intr_handle)) {
		for (q_id = 0; q_id < hw->used_rx_queues; q_id++) {
			(void)hw->ops.bind_ring_with_vector(hw, vec, false,
							HNS3_RING_TYPE_RX,
							q_id);
			if (vec < base + rte_intr_nb_efd_get(intr_handle) - 1)
				vec++;
		}
	}
	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	rte_intr_vec_list_free(intr_handle);
}

int
hns3_restore_rx_interrupt(struct hns3_hw *hw)
{
	struct rte_eth_dev *dev = &rte_eth_devices[hw->data->port_id];
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = pci_dev->intr_handle;
	uint16_t q_id;
	int ret;

	if (dev->data->dev_conf.intr_conf.rxq == 0)
		return 0;

	if (rte_intr_dp_is_en(intr_handle)) {
		for (q_id = 0; q_id < hw->used_rx_queues; q_id++) {
			ret = hw->ops.bind_ring_with_vector(hw,
				rte_intr_vec_list_index_get(intr_handle,
								   q_id),
				true, HNS3_RING_TYPE_RX, q_id);
			if (ret)
				return ret;
		}
	}

	return 0;
}

int
hns3_get_pci_revision_id(struct hns3_hw *hw, uint8_t *revision_id)
{
	struct rte_pci_device *pci_dev;
	struct rte_eth_dev *eth_dev;
	uint8_t revision;
	int ret;

	eth_dev = &rte_eth_devices[hw->data->port_id];
	pci_dev = RTE_ETH_DEV_TO_PCI(eth_dev);
	ret = rte_pci_read_config(pci_dev, &revision, HNS3_PCI_REVISION_ID_LEN,
				  HNS3_PCI_REVISION_ID);
	if (ret != HNS3_PCI_REVISION_ID_LEN) {
		hns3_err(hw, "failed to read pci revision id, ret = %d", ret);
		return -EIO;
	}

	*revision_id = revision;

	return 0;
}

void
hns3_set_default_dev_specifications(struct hns3_hw *hw)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);

	hw->max_non_tso_bd_num = HNS3_MAX_NON_TSO_BD_PER_PKT;
	hw->rss_ind_tbl_size = HNS3_RSS_IND_TBL_SIZE;
	hw->rss_key_size = HNS3_RSS_KEY_SIZE;
	hw->intr.int_ql_max = HNS3_INTR_QL_NONE;

	if (hns->is_vf)
		return;

	hw->max_tm_rate = HNS3_ETHER_MAX_RATE;
}

static void
hns3_parse_dev_specifications(struct hns3_hw *hw, struct hns3_cmd_desc *desc)
{
	struct hns3_adapter *hns = HNS3_DEV_HW_TO_ADAPTER(hw);
	struct hns3_dev_specs_0_cmd *req0;
	struct hns3_dev_specs_1_cmd *req1;

	req0 = (struct hns3_dev_specs_0_cmd *)desc[0].data;
	req1 = (struct hns3_dev_specs_1_cmd *)desc[1].data;

	hw->max_non_tso_bd_num = req0->max_non_tso_bd_num;
	hw->rss_ind_tbl_size = rte_le_to_cpu_16(req0->rss_ind_tbl_size);
	hw->rss_key_size = rte_le_to_cpu_16(req0->rss_key_size);
	hw->intr.int_ql_max = rte_le_to_cpu_16(req0->intr_ql_max);
	hw->min_tx_pkt_len = req1->min_tx_pkt_len;

	if (hns->is_vf)
		return;

	hw->max_tm_rate = rte_le_to_cpu_32(req0->max_tm_rate);
}

static int
hns3_check_dev_specifications(struct hns3_hw *hw)
{
	if (hw->rss_ind_tbl_size == 0 ||
	    hw->rss_ind_tbl_size > HNS3_RSS_IND_TBL_SIZE_MAX) {
		hns3_err(hw, "the indirection table size obtained (%u) is invalid, and should not be zero or exceed the maximum(%u)",
			 hw->rss_ind_tbl_size, HNS3_RSS_IND_TBL_SIZE_MAX);
		return -EINVAL;
	}

	if (hw->rss_key_size == 0 || hw->rss_key_size > HNS3_RSS_KEY_SIZE_MAX) {
		hns3_err(hw, "the RSS key size obtained (%u) is invalid, and should not be zero or exceed the maximum(%u)",
			 hw->rss_key_size, HNS3_RSS_KEY_SIZE_MAX);
		return -EINVAL;
	}

	if (hw->rss_key_size > HNS3_RSS_KEY_SIZE)
		hns3_warn(hw, "the RSS key size obtained (%u) is greater than the default key size (%u)",
			  hw->rss_key_size, HNS3_RSS_KEY_SIZE);

	return 0;
}

int
hns3_query_dev_specifications(struct hns3_hw *hw)
{
	struct hns3_cmd_desc desc[HNS3_QUERY_DEV_SPECS_BD_NUM];
	int ret;
	int i;

	for (i = 0; i < HNS3_QUERY_DEV_SPECS_BD_NUM - 1; i++) {
		hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_QUERY_DEV_SPECS,
					  true);
		desc[i].flag |= rte_cpu_to_le_16(HNS3_CMD_FLAG_NEXT);
	}
	hns3_cmd_setup_basic_desc(&desc[i], HNS3_OPC_QUERY_DEV_SPECS, true);

	ret = hns3_cmd_send(hw, desc, HNS3_QUERY_DEV_SPECS_BD_NUM);
	if (ret)
		return ret;

	hns3_parse_dev_specifications(hw, desc);

	return hns3_check_dev_specifications(hw);
}
