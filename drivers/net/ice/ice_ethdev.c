/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_ethdev_pci.h>

#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#include "base/ice_sched.h"
#include "base/ice_flow.h"
#include "base/ice_dcb.h"
#include "ice_ethdev.h"
#include "ice_rxtx.h"

#define ICE_MAX_QP_NUM "max_queue_pair_num"
#define ICE_DFLT_OUTER_TAG_TYPE ICE_AQ_VSI_OUTER_TAG_VLAN_9100
#define ICE_DFLT_PKG_FILE "/lib/firmware/intel/ice/ddp/ice.pkg"

int ice_logtype_init;
int ice_logtype_driver;

static int ice_dev_configure(struct rte_eth_dev *dev);
static int ice_dev_start(struct rte_eth_dev *dev);
static void ice_dev_stop(struct rte_eth_dev *dev);
static void ice_dev_close(struct rte_eth_dev *dev);
static int ice_dev_reset(struct rte_eth_dev *dev);
static void ice_dev_info_get(struct rte_eth_dev *dev,
			     struct rte_eth_dev_info *dev_info);
static int ice_link_update(struct rte_eth_dev *dev,
			   int wait_to_complete);
static int ice_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);
static int ice_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int ice_vlan_tpid_set(struct rte_eth_dev *dev,
			     enum rte_vlan_type vlan_type,
			     uint16_t tpid);
static int ice_rss_reta_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_reta_entry64 *reta_conf,
			       uint16_t reta_size);
static int ice_rss_reta_query(struct rte_eth_dev *dev,
			      struct rte_eth_rss_reta_entry64 *reta_conf,
			      uint16_t reta_size);
static int ice_rss_hash_update(struct rte_eth_dev *dev,
			       struct rte_eth_rss_conf *rss_conf);
static int ice_rss_hash_conf_get(struct rte_eth_dev *dev,
				 struct rte_eth_rss_conf *rss_conf);
static void ice_promisc_enable(struct rte_eth_dev *dev);
static void ice_promisc_disable(struct rte_eth_dev *dev);
static void ice_allmulti_enable(struct rte_eth_dev *dev);
static void ice_allmulti_disable(struct rte_eth_dev *dev);
static int ice_vlan_filter_set(struct rte_eth_dev *dev,
			       uint16_t vlan_id,
			       int on);
static int ice_macaddr_set(struct rte_eth_dev *dev,
			   struct ether_addr *mac_addr);
static int ice_macaddr_add(struct rte_eth_dev *dev,
			   struct ether_addr *mac_addr,
			   __rte_unused uint32_t index,
			   uint32_t pool);
static void ice_macaddr_remove(struct rte_eth_dev *dev, uint32_t index);
static int ice_rx_queue_intr_enable(struct rte_eth_dev *dev,
				    uint16_t queue_id);
static int ice_rx_queue_intr_disable(struct rte_eth_dev *dev,
				     uint16_t queue_id);
static int ice_fw_version_get(struct rte_eth_dev *dev, char *fw_version,
			      size_t fw_size);
static int ice_vlan_pvid_set(struct rte_eth_dev *dev,
			     uint16_t pvid, int on);
static int ice_get_eeprom_length(struct rte_eth_dev *dev);
static int ice_get_eeprom(struct rte_eth_dev *dev,
			  struct rte_dev_eeprom_info *eeprom);
static int ice_stats_get(struct rte_eth_dev *dev,
			 struct rte_eth_stats *stats);
static void ice_stats_reset(struct rte_eth_dev *dev);
static int ice_xstats_get(struct rte_eth_dev *dev,
			  struct rte_eth_xstat *xstats, unsigned int n);
static int ice_xstats_get_names(struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				unsigned int limit);

static const struct rte_pci_id pci_id_ice_map[] = {
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_BACKPLANE) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_QSFP) },
	{ RTE_PCI_DEVICE(ICE_INTEL_VENDOR_ID, ICE_DEV_ID_E810C_SFP) },
	{ .vendor_id = 0, /* sentinel */ },
};

static const struct eth_dev_ops ice_eth_dev_ops = {
	.dev_configure                = ice_dev_configure,
	.dev_start                    = ice_dev_start,
	.dev_stop                     = ice_dev_stop,
	.dev_close                    = ice_dev_close,
	.dev_reset                    = ice_dev_reset,
	.rx_queue_start               = ice_rx_queue_start,
	.rx_queue_stop                = ice_rx_queue_stop,
	.tx_queue_start               = ice_tx_queue_start,
	.tx_queue_stop                = ice_tx_queue_stop,
	.rx_queue_setup               = ice_rx_queue_setup,
	.rx_queue_release             = ice_rx_queue_release,
	.tx_queue_setup               = ice_tx_queue_setup,
	.tx_queue_release             = ice_tx_queue_release,
	.dev_infos_get                = ice_dev_info_get,
	.dev_supported_ptypes_get     = ice_dev_supported_ptypes_get,
	.link_update                  = ice_link_update,
	.mtu_set                      = ice_mtu_set,
	.mac_addr_set                 = ice_macaddr_set,
	.mac_addr_add                 = ice_macaddr_add,
	.mac_addr_remove              = ice_macaddr_remove,
	.vlan_filter_set              = ice_vlan_filter_set,
	.vlan_offload_set             = ice_vlan_offload_set,
	.vlan_tpid_set                = ice_vlan_tpid_set,
	.reta_update                  = ice_rss_reta_update,
	.reta_query                   = ice_rss_reta_query,
	.rss_hash_update              = ice_rss_hash_update,
	.rss_hash_conf_get            = ice_rss_hash_conf_get,
	.promiscuous_enable           = ice_promisc_enable,
	.promiscuous_disable          = ice_promisc_disable,
	.allmulticast_enable          = ice_allmulti_enable,
	.allmulticast_disable         = ice_allmulti_disable,
	.rx_queue_intr_enable         = ice_rx_queue_intr_enable,
	.rx_queue_intr_disable        = ice_rx_queue_intr_disable,
	.fw_version_get               = ice_fw_version_get,
	.vlan_pvid_set                = ice_vlan_pvid_set,
	.rxq_info_get                 = ice_rxq_info_get,
	.txq_info_get                 = ice_txq_info_get,
	.get_eeprom_length            = ice_get_eeprom_length,
	.get_eeprom                   = ice_get_eeprom,
	.rx_queue_count               = ice_rx_queue_count,
	.rx_descriptor_status         = ice_rx_descriptor_status,
	.tx_descriptor_status         = ice_tx_descriptor_status,
	.stats_get                    = ice_stats_get,
	.stats_reset                  = ice_stats_reset,
	.xstats_get                   = ice_xstats_get,
	.xstats_get_names             = ice_xstats_get_names,
	.xstats_reset                 = ice_stats_reset,
};

/* store statistics names and its offset in stats structure */
struct ice_xstats_name_off {
	char name[RTE_ETH_XSTATS_NAME_SIZE];
	unsigned int offset;
};

static const struct ice_xstats_name_off ice_stats_strings[] = {
	{"rx_unicast_packets", offsetof(struct ice_eth_stats, rx_unicast)},
	{"rx_multicast_packets", offsetof(struct ice_eth_stats, rx_multicast)},
	{"rx_broadcast_packets", offsetof(struct ice_eth_stats, rx_broadcast)},
	{"rx_dropped", offsetof(struct ice_eth_stats, rx_discards)},
	{"rx_unknown_protocol_packets", offsetof(struct ice_eth_stats,
		rx_unknown_protocol)},
	{"tx_unicast_packets", offsetof(struct ice_eth_stats, tx_unicast)},
	{"tx_multicast_packets", offsetof(struct ice_eth_stats, tx_multicast)},
	{"tx_broadcast_packets", offsetof(struct ice_eth_stats, tx_broadcast)},
	{"tx_dropped", offsetof(struct ice_eth_stats, tx_discards)},
};

#define ICE_NB_ETH_XSTATS (sizeof(ice_stats_strings) / \
		sizeof(ice_stats_strings[0]))

static const struct ice_xstats_name_off ice_hw_port_strings[] = {
	{"tx_link_down_dropped", offsetof(struct ice_hw_port_stats,
		tx_dropped_link_down)},
	{"rx_crc_errors", offsetof(struct ice_hw_port_stats, crc_errors)},
	{"rx_illegal_byte_errors", offsetof(struct ice_hw_port_stats,
		illegal_bytes)},
	{"rx_error_bytes", offsetof(struct ice_hw_port_stats, error_bytes)},
	{"mac_local_errors", offsetof(struct ice_hw_port_stats,
		mac_local_faults)},
	{"mac_remote_errors", offsetof(struct ice_hw_port_stats,
		mac_remote_faults)},
	{"rx_len_errors", offsetof(struct ice_hw_port_stats,
		rx_len_errors)},
	{"tx_xon_packets", offsetof(struct ice_hw_port_stats, link_xon_tx)},
	{"rx_xon_packets", offsetof(struct ice_hw_port_stats, link_xon_rx)},
	{"tx_xoff_packets", offsetof(struct ice_hw_port_stats, link_xoff_tx)},
	{"rx_xoff_packets", offsetof(struct ice_hw_port_stats, link_xoff_rx)},
	{"rx_size_64_packets", offsetof(struct ice_hw_port_stats, rx_size_64)},
	{"rx_size_65_to_127_packets", offsetof(struct ice_hw_port_stats,
		rx_size_127)},
	{"rx_size_128_to_255_packets", offsetof(struct ice_hw_port_stats,
		rx_size_255)},
	{"rx_size_256_to_511_packets", offsetof(struct ice_hw_port_stats,
		rx_size_511)},
	{"rx_size_512_to_1023_packets", offsetof(struct ice_hw_port_stats,
		rx_size_1023)},
	{"rx_size_1024_to_1522_packets", offsetof(struct ice_hw_port_stats,
		rx_size_1522)},
	{"rx_size_1523_to_max_packets", offsetof(struct ice_hw_port_stats,
		rx_size_big)},
	{"rx_undersized_errors", offsetof(struct ice_hw_port_stats,
		rx_undersize)},
	{"rx_oversize_errors", offsetof(struct ice_hw_port_stats,
		rx_oversize)},
	{"rx_mac_short_pkt_dropped", offsetof(struct ice_hw_port_stats,
		mac_short_pkt_dropped)},
	{"rx_fragmented_errors", offsetof(struct ice_hw_port_stats,
		rx_fragments)},
	{"rx_jabber_errors", offsetof(struct ice_hw_port_stats, rx_jabber)},
	{"tx_size_64_packets", offsetof(struct ice_hw_port_stats, tx_size_64)},
	{"tx_size_65_to_127_packets", offsetof(struct ice_hw_port_stats,
		tx_size_127)},
	{"tx_size_128_to_255_packets", offsetof(struct ice_hw_port_stats,
		tx_size_255)},
	{"tx_size_256_to_511_packets", offsetof(struct ice_hw_port_stats,
		tx_size_511)},
	{"tx_size_512_to_1023_packets", offsetof(struct ice_hw_port_stats,
		tx_size_1023)},
	{"tx_size_1024_to_1522_packets", offsetof(struct ice_hw_port_stats,
		tx_size_1522)},
	{"tx_size_1523_to_max_packets", offsetof(struct ice_hw_port_stats,
		tx_size_big)},
};

#define ICE_NB_HW_PORT_XSTATS (sizeof(ice_hw_port_strings) / \
		sizeof(ice_hw_port_strings[0]))

static void
ice_init_controlq_parameter(struct ice_hw *hw)
{
	/* fields for adminq */
	hw->adminq.num_rq_entries = ICE_ADMINQ_LEN;
	hw->adminq.num_sq_entries = ICE_ADMINQ_LEN;
	hw->adminq.rq_buf_size = ICE_ADMINQ_BUF_SZ;
	hw->adminq.sq_buf_size = ICE_ADMINQ_BUF_SZ;

	/* fields for mailboxq, DPDK used as PF host */
	hw->mailboxq.num_rq_entries = ICE_MAILBOXQ_LEN;
	hw->mailboxq.num_sq_entries = ICE_MAILBOXQ_LEN;
	hw->mailboxq.rq_buf_size = ICE_MAILBOXQ_BUF_SZ;
	hw->mailboxq.sq_buf_size = ICE_MAILBOXQ_BUF_SZ;
}

static int
ice_check_qp_num(const char *key, const char *qp_value,
		 __rte_unused void *opaque)
{
	char *end = NULL;
	int num = 0;

	while (isblank(*qp_value))
		qp_value++;

	num = strtoul(qp_value, &end, 10);

	if (!num || (*end == '-') || errno) {
		PMD_DRV_LOG(WARNING, "invalid value:\"%s\" for key:\"%s\", "
			    "value must be > 0",
			    qp_value, key);
		return -1;
	}

	return num;
}

static int
ice_config_max_queue_pair_num(struct rte_devargs *devargs)
{
	struct rte_kvargs *kvlist;
	const char *queue_num_key = ICE_MAX_QP_NUM;
	int ret;

	if (!devargs)
		return 0;

	kvlist = rte_kvargs_parse(devargs->args, NULL);
	if (!kvlist)
		return 0;

	if (!rte_kvargs_count(kvlist, queue_num_key)) {
		rte_kvargs_free(kvlist);
		return 0;
	}

	if (rte_kvargs_process(kvlist, queue_num_key,
			       ice_check_qp_num, NULL) < 0) {
		rte_kvargs_free(kvlist);
		return 0;
	}
	ret = rte_kvargs_process(kvlist, queue_num_key,
				 ice_check_qp_num, NULL);
	rte_kvargs_free(kvlist);

	return ret;
}

static int
ice_res_pool_init(struct ice_res_pool_info *pool, uint32_t base,
		  uint32_t num)
{
	struct pool_entry *entry;

	if (!pool || !num)
		return -EINVAL;

	entry = rte_zmalloc(NULL, sizeof(*entry), 0);
	if (!entry) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for resource pool");
		return -ENOMEM;
	}

	/* queue heap initialize */
	pool->num_free = num;
	pool->num_alloc = 0;
	pool->base = base;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);

	/* Initialize element  */
	entry->base = 0;
	entry->len = num;

	LIST_INSERT_HEAD(&pool->free_list, entry, next);
	return 0;
}

static int
ice_res_pool_alloc(struct ice_res_pool_info *pool,
		   uint16_t num)
{
	struct pool_entry *entry, *valid_entry;

	if (!pool || !num) {
		PMD_INIT_LOG(ERR, "Invalid parameter");
		return -EINVAL;
	}

	if (pool->num_free < num) {
		PMD_INIT_LOG(ERR, "No resource. ask:%u, available:%u",
			     num, pool->num_free);
		return -ENOMEM;
	}

	valid_entry = NULL;
	/* Lookup  in free list and find most fit one */
	LIST_FOREACH(entry, &pool->free_list, next) {
		if (entry->len >= num) {
			/* Find best one */
			if (entry->len == num) {
				valid_entry = entry;
				break;
			}
			if (!valid_entry ||
			    valid_entry->len > entry->len)
				valid_entry = entry;
		}
	}

	/* Not find one to satisfy the request, return */
	if (!valid_entry) {
		PMD_INIT_LOG(ERR, "No valid entry found");
		return -ENOMEM;
	}
	/**
	 * The entry have equal queue number as requested,
	 * remove it from alloc_list.
	 */
	if (valid_entry->len == num) {
		LIST_REMOVE(valid_entry, next);
	} else {
		/**
		 * The entry have more numbers than requested,
		 * create a new entry for alloc_list and minus its
		 * queue base and number in free_list.
		 */
		entry = rte_zmalloc(NULL, sizeof(*entry), 0);
		if (!entry) {
			PMD_INIT_LOG(ERR,
				     "Failed to allocate memory for "
				     "resource pool");
			return -ENOMEM;
		}
		entry->base = valid_entry->base;
		entry->len = num;
		valid_entry->base += num;
		valid_entry->len -= num;
		valid_entry = entry;
	}

	/* Insert it into alloc list, not sorted */
	LIST_INSERT_HEAD(&pool->alloc_list, valid_entry, next);

	pool->num_free -= valid_entry->len;
	pool->num_alloc += valid_entry->len;

	return valid_entry->base + pool->base;
}

static void
ice_res_pool_destroy(struct ice_res_pool_info *pool)
{
	struct pool_entry *entry, *next_entry;

	if (!pool)
		return;

	for (entry = LIST_FIRST(&pool->alloc_list);
	     entry && (next_entry = LIST_NEXT(entry, next), 1);
	     entry = next_entry) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	for (entry = LIST_FIRST(&pool->free_list);
	     entry && (next_entry = LIST_NEXT(entry, next), 1);
	     entry = next_entry) {
		LIST_REMOVE(entry, next);
		rte_free(entry);
	}

	pool->num_free = 0;
	pool->num_alloc = 0;
	pool->base = 0;
	LIST_INIT(&pool->alloc_list);
	LIST_INIT(&pool->free_list);
}

static void
ice_vsi_config_default_rss(struct ice_aqc_vsi_props *info)
{
	/* Set VSI LUT selection */
	info->q_opt_rss = ICE_AQ_VSI_Q_OPT_RSS_LUT_VSI &
			  ICE_AQ_VSI_Q_OPT_RSS_LUT_M;
	/* Set Hash scheme */
	info->q_opt_rss |= ICE_AQ_VSI_Q_OPT_RSS_TPLZ &
			   ICE_AQ_VSI_Q_OPT_RSS_HASH_M;
	/* enable TC */
	info->q_opt_tc = ICE_AQ_VSI_Q_OPT_TC_OVR_M;
}

static enum ice_status
ice_vsi_config_tc_queue_mapping(struct ice_vsi *vsi,
				struct ice_aqc_vsi_props *info,
				uint8_t enabled_tcmap)
{
	uint16_t bsf, qp_idx;

	/* default tc 0 now. Multi-TC supporting need to be done later.
	 * Configure TC and queue mapping parameters, for enabled TC,
	 * allocate qpnum_per_tc queues to this traffic.
	 */
	if (enabled_tcmap != 0x01) {
		PMD_INIT_LOG(ERR, "only TC0 is supported");
		return -ENOTSUP;
	}

	vsi->nb_qps = RTE_MIN(vsi->nb_qps, ICE_MAX_Q_PER_TC);
	bsf = rte_bsf32(vsi->nb_qps);
	/* Adjust the queue number to actual queues that can be applied */
	vsi->nb_qps = 0x1 << bsf;

	qp_idx = 0;
	/* Set tc and queue mapping with VSI */
	info->tc_mapping[0] = rte_cpu_to_le_16((qp_idx <<
						ICE_AQ_VSI_TC_Q_OFFSET_S) |
					       (bsf << ICE_AQ_VSI_TC_Q_NUM_S));

	/* Associate queue number with VSI */
	info->mapping_flags |= rte_cpu_to_le_16(ICE_AQ_VSI_Q_MAP_CONTIG);
	info->q_mapping[0] = rte_cpu_to_le_16(vsi->base_queue);
	info->q_mapping[1] = rte_cpu_to_le_16(vsi->nb_qps);
	info->valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_RXQ_MAP_VALID);
	/* Set the info.ingress_table and info.egress_table
	 * for UP translate table. Now just set it to 1:1 map by default
	 * -- 0b 111 110 101 100 011 010 001 000 == 0xFAC688
	 */
#define ICE_TC_QUEUE_TABLE_DFLT 0x00FAC688
	info->ingress_table  = rte_cpu_to_le_32(ICE_TC_QUEUE_TABLE_DFLT);
	info->egress_table   = rte_cpu_to_le_32(ICE_TC_QUEUE_TABLE_DFLT);
	info->outer_up_table = rte_cpu_to_le_32(ICE_TC_QUEUE_TABLE_DFLT);
	return 0;
}

static int
ice_init_mac_address(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	if (!is_unicast_ether_addr
		((struct ether_addr *)hw->port_info[0].mac.lan_addr)) {
		PMD_INIT_LOG(ERR, "Invalid MAC address");
		return -EINVAL;
	}

	ether_addr_copy((struct ether_addr *)hw->port_info[0].mac.lan_addr,
			(struct ether_addr *)hw->port_info[0].mac.perm_addr);

	dev->data->mac_addrs = rte_zmalloc(NULL, sizeof(struct ether_addr), 0);
	if (!dev->data->mac_addrs) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory to store mac address");
		return -ENOMEM;
	}
	/* store it to dev data */
	ether_addr_copy((struct ether_addr *)hw->port_info[0].mac.perm_addr,
			&dev->data->mac_addrs[0]);
	return 0;
}

/* Find out specific MAC filter */
static struct ice_mac_filter *
ice_find_mac_filter(struct ice_vsi *vsi, struct ether_addr *macaddr)
{
	struct ice_mac_filter *f;

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (is_same_ether_addr(macaddr, &f->mac_info.mac_addr))
			return f;
	}

	return NULL;
}

static int
ice_add_mac_filter(struct ice_vsi *vsi, struct ether_addr *mac_addr)
{
	struct ice_fltr_list_entry *m_list_itr = NULL;
	struct ice_mac_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret = 0;

	/* If it's added and configured, return */
	f = ice_find_mac_filter(vsi, mac_addr);
	if (f) {
		PMD_DRV_LOG(INFO, "This MAC filter already exists.");
		return 0;
	}

	INIT_LIST_HEAD(&list_head);

	m_list_itr = (struct ice_fltr_list_entry *)
		ice_malloc(hw, sizeof(*m_list_itr));
	if (!m_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}
	ice_memcpy(m_list_itr->fltr_info.l_data.mac.mac_addr,
		   mac_addr, ETH_ALEN, ICE_NONDMA_TO_NONDMA);
	m_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	m_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	m_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_MAC;
	m_list_itr->fltr_info.flag = ICE_FLTR_TX;
	m_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&m_list_itr->list_entry, &list_head);

	/* Add the mac */
	ret = ice_add_mac(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MAC filter");
		ret = -EINVAL;
		goto DONE;
	}
	/* Add the mac addr into mac list */
	f = rte_zmalloc(NULL, sizeof(*f), 0);
	if (!f) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		ret = -ENOMEM;
		goto DONE;
	}
	rte_memcpy(&f->mac_info.mac_addr, mac_addr, ETH_ADDR_LEN);
	TAILQ_INSERT_TAIL(&vsi->mac_list, f, next);
	vsi->mac_num++;

	ret = 0;

DONE:
	rte_free(m_list_itr);
	return ret;
}

static int
ice_remove_mac_filter(struct ice_vsi *vsi, struct ether_addr *mac_addr)
{
	struct ice_fltr_list_entry *m_list_itr = NULL;
	struct ice_mac_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret = 0;

	/* Can't find it, return an error */
	f = ice_find_mac_filter(vsi, mac_addr);
	if (!f)
		return -EINVAL;

	INIT_LIST_HEAD(&list_head);

	m_list_itr = (struct ice_fltr_list_entry *)
		ice_malloc(hw, sizeof(*m_list_itr));
	if (!m_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}
	ice_memcpy(m_list_itr->fltr_info.l_data.mac.mac_addr,
		   mac_addr, ETH_ALEN, ICE_NONDMA_TO_NONDMA);
	m_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	m_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	m_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_MAC;
	m_list_itr->fltr_info.flag = ICE_FLTR_TX;
	m_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&m_list_itr->list_entry, &list_head);

	/* remove the mac filter */
	ret = ice_remove_mac(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to remove MAC filter");
		ret = -EINVAL;
		goto DONE;
	}

	/* Remove the mac addr from mac list */
	TAILQ_REMOVE(&vsi->mac_list, f, next);
	rte_free(f);
	vsi->mac_num--;

	ret = 0;
DONE:
	rte_free(m_list_itr);
	return ret;
}

/* Find out specific VLAN filter */
static struct ice_vlan_filter *
ice_find_vlan_filter(struct ice_vsi *vsi, uint16_t vlan_id)
{
	struct ice_vlan_filter *f;

	TAILQ_FOREACH(f, &vsi->vlan_list, next) {
		if (vlan_id == f->vlan_info.vlan_id)
			return f;
	}

	return NULL;
}

static int
ice_add_vlan_filter(struct ice_vsi *vsi, uint16_t vlan_id)
{
	struct ice_fltr_list_entry *v_list_itr = NULL;
	struct ice_vlan_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw;
	int ret = 0;

	if (!vsi || vlan_id > ETHER_MAX_VLAN_ID)
		return -EINVAL;

	hw = ICE_VSI_TO_HW(vsi);

	/* If it's added and configured, return. */
	f = ice_find_vlan_filter(vsi, vlan_id);
	if (f) {
		PMD_DRV_LOG(INFO, "This VLAN filter already exists.");
		return 0;
	}

	if (!vsi->vlan_anti_spoof_on && !vsi->vlan_filter_on)
		return 0;

	INIT_LIST_HEAD(&list_head);

	v_list_itr = (struct ice_fltr_list_entry *)
		      ice_malloc(hw, sizeof(*v_list_itr));
	if (!v_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}
	v_list_itr->fltr_info.l_data.vlan.vlan_id = vlan_id;
	v_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	v_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	v_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_VLAN;
	v_list_itr->fltr_info.flag = ICE_FLTR_TX;
	v_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&v_list_itr->list_entry, &list_head);

	/* Add the vlan */
	ret = ice_add_vlan(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add VLAN filter");
		ret = -EINVAL;
		goto DONE;
	}

	/* Add vlan into vlan list */
	f = rte_zmalloc(NULL, sizeof(*f), 0);
	if (!f) {
		PMD_DRV_LOG(ERR, "failed to allocate memory");
		ret = -ENOMEM;
		goto DONE;
	}
	f->vlan_info.vlan_id = vlan_id;
	TAILQ_INSERT_TAIL(&vsi->vlan_list, f, next);
	vsi->vlan_num++;

	ret = 0;

DONE:
	rte_free(v_list_itr);
	return ret;
}

static int
ice_remove_vlan_filter(struct ice_vsi *vsi, uint16_t vlan_id)
{
	struct ice_fltr_list_entry *v_list_itr = NULL;
	struct ice_vlan_filter *f;
	struct LIST_HEAD_TYPE list_head;
	struct ice_hw *hw;
	int ret = 0;

	/**
	 * Vlan 0 is the generic filter for untagged packets
	 * and can't be removed.
	 */
	if (!vsi || vlan_id == 0 || vlan_id > ETHER_MAX_VLAN_ID)
		return -EINVAL;

	hw = ICE_VSI_TO_HW(vsi);

	/* Can't find it, return an error */
	f = ice_find_vlan_filter(vsi, vlan_id);
	if (!f)
		return -EINVAL;

	INIT_LIST_HEAD(&list_head);

	v_list_itr = (struct ice_fltr_list_entry *)
		      ice_malloc(hw, sizeof(*v_list_itr));
	if (!v_list_itr) {
		ret = -ENOMEM;
		goto DONE;
	}

	v_list_itr->fltr_info.l_data.vlan.vlan_id = vlan_id;
	v_list_itr->fltr_info.src_id = ICE_SRC_ID_VSI;
	v_list_itr->fltr_info.fltr_act = ICE_FWD_TO_VSI;
	v_list_itr->fltr_info.lkup_type = ICE_SW_LKUP_VLAN;
	v_list_itr->fltr_info.flag = ICE_FLTR_TX;
	v_list_itr->fltr_info.vsi_handle = vsi->idx;

	LIST_ADD(&v_list_itr->list_entry, &list_head);

	/* remove the vlan filter */
	ret = ice_remove_vlan(hw, &list_head);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to remove VLAN filter");
		ret = -EINVAL;
		goto DONE;
	}

	/* Remove the vlan id from vlan list */
	TAILQ_REMOVE(&vsi->vlan_list, f, next);
	rte_free(f);
	vsi->vlan_num--;

	ret = 0;
DONE:
	rte_free(v_list_itr);
	return ret;
}

static int
ice_remove_all_mac_vlan_filters(struct ice_vsi *vsi)
{
	struct ice_mac_filter *m_f;
	struct ice_vlan_filter *v_f;
	int ret = 0;

	if (!vsi || !vsi->mac_num)
		return -EINVAL;

	TAILQ_FOREACH(m_f, &vsi->mac_list, next) {
		ret = ice_remove_mac_filter(vsi, &m_f->mac_info.mac_addr);
		if (ret != ICE_SUCCESS) {
			ret = -EINVAL;
			goto DONE;
		}
	}

	if (vsi->vlan_num == 0)
		return 0;

	TAILQ_FOREACH(v_f, &vsi->vlan_list, next) {
		ret = ice_remove_vlan_filter(vsi, v_f->vlan_info.vlan_id);
		if (ret != ICE_SUCCESS) {
			ret = -EINVAL;
			goto DONE;
		}
	}

DONE:
	return ret;
}

static int
ice_vsi_config_qinq_insertion(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t qinq_flags;
	int ret = 0;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID)) {
		if (on) {
			if ((vsi->info.outer_tag_flags &
			     ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST) ==
			    ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST)
				return 0; /* already on */
		} else {
			if (!(vsi->info.outer_tag_flags &
			      ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST))
				return 0; /* already off */
		}
	}

	if (on)
		qinq_flags = ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST;
	else
		qinq_flags = 0;
	/* clear global insertion and use per packet insertion */
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_INSERT);
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_ACCEPT_HOST);
	vsi->info.outer_tag_flags |= qinq_flags;
	/* use default vlan type 0x8100 */
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_TYPE_M);
	vsi->info.outer_tag_flags |= ICE_DFLT_OUTER_TAG_TYPE <<
				     ICE_AQ_VSI_OUTER_TAG_TYPE_S;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
			rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	ctxt.vsi_num = vsi->vsi_id;
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO,
			    "Update VSI failed to %s qinq stripping",
			    on ? "enable" : "disable");
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);

	return ret;
}

static int
ice_vsi_config_qinq_stripping(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t qinq_flags;
	int ret = 0;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID)) {
		if (on) {
			if ((vsi->info.outer_tag_flags &
			     ICE_AQ_VSI_OUTER_TAG_MODE_M) ==
			    ICE_AQ_VSI_OUTER_TAG_COPY)
				return 0; /* already on */
		} else {
			if ((vsi->info.outer_tag_flags &
			     ICE_AQ_VSI_OUTER_TAG_MODE_M) ==
			    ICE_AQ_VSI_OUTER_TAG_NOTHING)
				return 0; /* already off */
		}
	}

	if (on)
		qinq_flags = ICE_AQ_VSI_OUTER_TAG_COPY;
	else
		qinq_flags = ICE_AQ_VSI_OUTER_TAG_NOTHING;
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_MODE_M);
	vsi->info.outer_tag_flags |= qinq_flags;
	/* use default vlan type 0x8100 */
	vsi->info.outer_tag_flags &= ~(ICE_AQ_VSI_OUTER_TAG_TYPE_M);
	vsi->info.outer_tag_flags |= ICE_DFLT_OUTER_TAG_TYPE <<
				     ICE_AQ_VSI_OUTER_TAG_TYPE_S;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
			rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);
	ctxt.vsi_num = vsi->vsi_id;
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO,
			    "Update VSI failed to %s qinq stripping",
			    on ? "enable" : "disable");
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_OUTER_TAG_VALID);

	return ret;
}

static int
ice_vsi_config_double_vlan(struct ice_vsi *vsi, int on)
{
	int ret;

	ret = ice_vsi_config_qinq_stripping(vsi, on);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to set qinq stripping - %d", ret);

	ret = ice_vsi_config_qinq_insertion(vsi, on);
	if (ret)
		PMD_DRV_LOG(ERR, "Fail to set qinq insertion - %d", ret);

	return ret;
}

/* Enable IRQ0 */
static void
ice_pf_enable_irq0(struct ice_hw *hw)
{
	/* reset the registers */
	ICE_WRITE_REG(hw, PFINT_OICR_ENA, 0);
	ICE_READ_REG(hw, PFINT_OICR);

#ifdef ICE_LSE_SPT
	ICE_WRITE_REG(hw, PFINT_OICR_ENA,
		      (uint32_t)(PFINT_OICR_ENA_INT_ENA_M &
				 (~PFINT_OICR_LINK_STAT_CHANGE_M)));

	ICE_WRITE_REG(hw, PFINT_OICR_CTL,
		      (0 & PFINT_OICR_CTL_MSIX_INDX_M) |
		      ((0 << PFINT_OICR_CTL_ITR_INDX_S) &
		       PFINT_OICR_CTL_ITR_INDX_M) |
		      PFINT_OICR_CTL_CAUSE_ENA_M);

	ICE_WRITE_REG(hw, PFINT_FW_CTL,
		      (0 & PFINT_FW_CTL_MSIX_INDX_M) |
		      ((0 << PFINT_FW_CTL_ITR_INDX_S) &
		       PFINT_FW_CTL_ITR_INDX_M) |
		      PFINT_FW_CTL_CAUSE_ENA_M);
#else
	ICE_WRITE_REG(hw, PFINT_OICR_ENA, PFINT_OICR_ENA_INT_ENA_M);
#endif

	ICE_WRITE_REG(hw, GLINT_DYN_CTL(0),
		      GLINT_DYN_CTL_INTENA_M |
		      GLINT_DYN_CTL_CLEARPBA_M |
		      GLINT_DYN_CTL_ITR_INDX_M);

	ice_flush(hw);
}

/* Disable IRQ0 */
static void
ice_pf_disable_irq0(struct ice_hw *hw)
{
	/* Disable all interrupt types */
	ICE_WRITE_REG(hw, GLINT_DYN_CTL(0), GLINT_DYN_CTL_WB_ON_ITR_M);
	ice_flush(hw);
}

#ifdef ICE_LSE_SPT
static void
ice_handle_aq_msg(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_ctl_q_info *cq = &hw->adminq;
	struct ice_rq_event_info event;
	uint16_t pending, opcode;
	int ret;

	event.buf_len = ICE_AQ_MAX_BUF_LEN;
	event.msg_buf = rte_zmalloc(NULL, event.buf_len, 0);
	if (!event.msg_buf) {
		PMD_DRV_LOG(ERR, "Failed to allocate mem");
		return;
	}

	pending = 1;
	while (pending) {
		ret = ice_clean_rq_elem(hw, cq, &event, &pending);

		if (ret != ICE_SUCCESS) {
			PMD_DRV_LOG(INFO,
				    "Failed to read msg from AdminQ, "
				    "adminq_err: %u",
				    hw->adminq.sq_last_status);
			break;
		}
		opcode = rte_le_to_cpu_16(event.desc.opcode);

		switch (opcode) {
		case ice_aqc_opc_get_link_status:
			ret = ice_link_update(dev, 0);
			if (!ret)
				_rte_eth_dev_callback_process
					(dev, RTE_ETH_EVENT_INTR_LSC, NULL);
			break;
		default:
			PMD_DRV_LOG(DEBUG, "Request %u is not supported yet",
				    opcode);
			break;
		}
	}
	rte_free(event.msg_buf);
}
#endif

/**
 * Interrupt handler triggered by NIC for handling
 * specific interrupt.
 *
 * @param handle
 *  Pointer to interrupt handle.
 * @param param
 *  The address of parameter (struct rte_eth_dev *) regsitered before.
 *
 * @return
 *  void
 */
static void
ice_interrupt_handler(void *param)
{
	struct rte_eth_dev *dev = (struct rte_eth_dev *)param;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t oicr;
	uint32_t reg;
	uint8_t pf_num;
	uint8_t event;
	uint16_t queue;
#ifdef ICE_LSE_SPT
	uint32_t int_fw_ctl;
#endif

	/* Disable interrupt */
	ice_pf_disable_irq0(hw);

	/* read out interrupt causes */
	oicr = ICE_READ_REG(hw, PFINT_OICR);
#ifdef ICE_LSE_SPT
	int_fw_ctl = ICE_READ_REG(hw, PFINT_FW_CTL);
#endif

	/* No interrupt event indicated */
	if (!(oicr & PFINT_OICR_INTEVENT_M)) {
		PMD_DRV_LOG(INFO, "No interrupt event");
		goto done;
	}

#ifdef ICE_LSE_SPT
	if (int_fw_ctl & PFINT_FW_CTL_INTEVENT_M) {
		PMD_DRV_LOG(INFO, "FW_CTL: link state change event");
		ice_handle_aq_msg(dev);
	}
#else
	if (oicr & PFINT_OICR_LINK_STAT_CHANGE_M) {
		PMD_DRV_LOG(INFO, "OICR: link state change event");
		ice_link_update(dev, 0);
	}
#endif

	if (oicr & PFINT_OICR_MAL_DETECT_M) {
		PMD_DRV_LOG(WARNING, "OICR: MDD event");
		reg = ICE_READ_REG(hw, GL_MDET_TX_PQM);
		if (reg & GL_MDET_TX_PQM_VALID_M) {
			pf_num = (reg & GL_MDET_TX_PQM_PF_NUM_M) >>
				 GL_MDET_TX_PQM_PF_NUM_S;
			event = (reg & GL_MDET_TX_PQM_MAL_TYPE_M) >>
				GL_MDET_TX_PQM_MAL_TYPE_S;
			queue = (reg & GL_MDET_TX_PQM_QNUM_M) >>
				GL_MDET_TX_PQM_QNUM_S;

			PMD_DRV_LOG(WARNING, "Malicious Driver Detection event "
				    "%d by PQM on TX queue %d PF# %d",
				    event, queue, pf_num);
		}

		reg = ICE_READ_REG(hw, GL_MDET_TX_TCLAN);
		if (reg & GL_MDET_TX_TCLAN_VALID_M) {
			pf_num = (reg & GL_MDET_TX_TCLAN_PF_NUM_M) >>
				 GL_MDET_TX_TCLAN_PF_NUM_S;
			event = (reg & GL_MDET_TX_TCLAN_MAL_TYPE_M) >>
				GL_MDET_TX_TCLAN_MAL_TYPE_S;
			queue = (reg & GL_MDET_TX_TCLAN_QNUM_M) >>
				GL_MDET_TX_TCLAN_QNUM_S;

			PMD_DRV_LOG(WARNING, "Malicious Driver Detection event "
				    "%d by TCLAN on TX queue %d PF# %d",
				    event, queue, pf_num);
		}
	}
done:
	/* Enable interrupt */
	ice_pf_enable_irq0(hw);
	rte_intr_enable(dev->intr_handle);
}

/*  Initialize SW parameters of PF */
static int
ice_pf_sw_init(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_PF_TO_HW(pf);

	if (ice_config_max_queue_pair_num(dev->device->devargs) > 0)
		pf->lan_nb_qp_max =
			ice_config_max_queue_pair_num(dev->device->devargs);
	else
		pf->lan_nb_qp_max =
			(uint16_t)RTE_MIN(hw->func_caps.common_cap.num_txq,
					  hw->func_caps.common_cap.num_rxq);

	pf->lan_nb_qps = pf->lan_nb_qp_max;

	return 0;
}

static struct ice_vsi *
ice_setup_vsi(struct ice_pf *pf, enum ice_vsi_type type)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = NULL;
	struct ice_vsi_ctx vsi_ctx;
	int ret;
	struct ether_addr broadcast = {
		.addr_bytes = {0xff, 0xff, 0xff, 0xff, 0xff, 0xff} };
	struct ether_addr mac_addr;
	uint16_t max_txqs[ICE_MAX_TRAFFIC_CLASS] = { 0 };
	uint8_t tc_bitmap = 0x1;

	/* hw->num_lports = 1 in NIC mode */
	vsi = rte_zmalloc(NULL, sizeof(struct ice_vsi), 0);
	if (!vsi)
		return NULL;

	vsi->idx = pf->next_vsi_idx;
	pf->next_vsi_idx++;
	vsi->type = type;
	vsi->adapter = ICE_PF_TO_ADAPTER(pf);
	vsi->max_macaddrs = ICE_NUM_MACADDR_MAX;
	vsi->vlan_anti_spoof_on = 0;
	vsi->vlan_filter_on = 1;
	TAILQ_INIT(&vsi->mac_list);
	TAILQ_INIT(&vsi->vlan_list);

	memset(&vsi_ctx, 0, sizeof(vsi_ctx));
	/* base_queue in used in queue mapping of VSI add/update command.
	 * Suppose vsi->base_queue is 0 now, don't consider SRIOV, VMDQ
	 * cases in the first stage. Only Main VSI.
	 */
	vsi->base_queue = 0;
	switch (type) {
	case ICE_VSI_PF:
		vsi->nb_qps = pf->lan_nb_qps;
		ice_vsi_config_default_rss(&vsi_ctx.info);
		vsi_ctx.alloc_from_pool = true;
		vsi_ctx.flags = ICE_AQ_VSI_TYPE_PF;
		/* switch_id is queried by get_switch_config aq, which is done
		 * by ice_init_hw
		 */
		vsi_ctx.info.sw_id = hw->port_info->sw_id;
		vsi_ctx.info.sw_flags2 = ICE_AQ_VSI_SW_FLAG_LAN_ENA;
		/* Allow all untagged or tagged packets */
		vsi_ctx.info.vlan_flags = ICE_AQ_VSI_VLAN_MODE_ALL;
		vsi_ctx.info.vlan_flags |= ICE_AQ_VSI_VLAN_EMOD_NOTHING;
		vsi_ctx.info.q_opt_rss = ICE_AQ_VSI_Q_OPT_RSS_LUT_PF |
					 ICE_AQ_VSI_Q_OPT_RSS_TPLZ;
		/* Enable VLAN/UP trip */
		ret = ice_vsi_config_tc_queue_mapping(vsi,
						      &vsi_ctx.info,
						      ICE_DEFAULT_TCMAP);
		if (ret) {
			PMD_INIT_LOG(ERR,
				     "tc queue mapping with vsi failed, "
				     "err = %d",
				     ret);
			goto fail_mem;
		}

		break;
	default:
		/* for other types of VSI */
		PMD_INIT_LOG(ERR, "other types of VSI not supported");
		goto fail_mem;
	}

	/* VF has MSIX interrupt in VF range, don't allocate here */
	if (type == ICE_VSI_PF) {
		ret = ice_res_pool_alloc(&pf->msix_pool,
					 RTE_MIN(vsi->nb_qps,
						 RTE_MAX_RXTX_INTR_VEC_ID));
		if (ret < 0) {
			PMD_INIT_LOG(ERR, "VSI MAIN %d get heap failed %d",
				     vsi->vsi_id, ret);
		}
		vsi->msix_intr = ret;
		vsi->nb_msix = RTE_MIN(vsi->nb_qps, RTE_MAX_RXTX_INTR_VEC_ID);
	} else {
		vsi->msix_intr = 0;
		vsi->nb_msix = 0;
	}
	ret = ice_add_vsi(hw, vsi->idx, &vsi_ctx, NULL);
	if (ret != ICE_SUCCESS) {
		PMD_INIT_LOG(ERR, "add vsi failed, err = %d", ret);
		goto fail_mem;
	}
	/* store vsi information is SW structure */
	vsi->vsi_id = vsi_ctx.vsi_num;
	vsi->info = vsi_ctx.info;
	pf->vsis_allocated = vsi_ctx.vsis_allocd;
	pf->vsis_unallocated = vsi_ctx.vsis_unallocated;

	/* MAC configuration */
	rte_memcpy(pf->dev_addr.addr_bytes,
		   hw->port_info->mac.perm_addr,
		   ETH_ADDR_LEN);

	rte_memcpy(&mac_addr, &pf->dev_addr, ETHER_ADDR_LEN);
	ret = ice_add_mac_filter(vsi, &mac_addr);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(ERR, "Failed to add dflt MAC filter");

	rte_memcpy(&mac_addr, &broadcast, ETHER_ADDR_LEN);
	ret = ice_add_mac_filter(vsi, &mac_addr);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(ERR, "Failed to add MAC filter");

	/* At the beginning, only TC0. */
	/* What we need here is the maximam number of the TX queues.
	 * Currently vsi->nb_qps means it.
	 * Correct it if any change.
	 */
	max_txqs[0] = vsi->nb_qps;
	ret = ice_cfg_vsi_lan(hw->port_info, vsi->idx,
			      tc_bitmap, max_txqs);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(ERR, "Failed to config vsi sched");

	return vsi;
fail_mem:
	rte_free(vsi);
	pf->next_vsi_idx--;
	return NULL;
}

static int
ice_send_driver_ver(struct ice_hw *hw)
{
	struct ice_driver_ver dv;

	/* we don't have driver version use 0 for dummy */
	dv.major_ver = 0;
	dv.minor_ver = 0;
	dv.build_ver = 0;
	dv.subbuild_ver = 0;
	strncpy((char *)dv.driver_string, "dpdk", sizeof(dv.driver_string));

	return ice_aq_send_driver_ver(hw, &dv, NULL);
}

static int
ice_pf_setup(struct ice_pf *pf)
{
	struct ice_vsi *vsi;

	/* Clear all stats counters */
	pf->offset_loaded = FALSE;
	memset(&pf->stats, 0, sizeof(struct ice_hw_port_stats));
	memset(&pf->stats_offset, 0, sizeof(struct ice_hw_port_stats));
	memset(&pf->internal_stats, 0, sizeof(struct ice_eth_stats));
	memset(&pf->internal_stats_offset, 0, sizeof(struct ice_eth_stats));

	vsi = ice_setup_vsi(pf, ICE_VSI_PF);
	if (!vsi) {
		PMD_INIT_LOG(ERR, "Failed to add vsi for PF");
		return -EINVAL;
	}

	pf->main_vsi = vsi;

	return 0;
}

static int ice_load_pkg(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	const char *pkg_file = ICE_DFLT_PKG_FILE;
	int err;
	uint8_t *buf;
	int buf_len;
	FILE *file;
	struct stat fstat;

	file = fopen(pkg_file, "rb");
	if (!file)  {
		PMD_INIT_LOG(ERR, "failed to open file: %s\n", pkg_file);
		return -1;
	}

	err = stat(pkg_file, &fstat);
	if (err) {
		PMD_INIT_LOG(ERR, "failed to get file stats\n");
		fclose(file);
		return err;
	}

	buf_len = fstat.st_size;
	buf = rte_malloc(NULL, buf_len, 0);

	if (!buf) {
		PMD_INIT_LOG(ERR, "failed to allocate buf of size %d for package\n",
				buf_len);
		fclose(file);
		return -1;
	}

	err = fread(buf, buf_len, 1, file);
	if (err != 1) {
		PMD_INIT_LOG(ERR, "failed to read package data\n");
		fclose(file);
		err = -1;
		goto fail_exit;
	}

	fclose(file);

	err = ice_copy_and_init_pkg(hw, buf, buf_len);
	if (err) {
		PMD_INIT_LOG(ERR, "ice_copy_and_init_hw failed: %d\n", err);
		goto fail_exit;
	}
	err = ice_init_hw_tbls(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "ice_init_hw_tbls failed: %d\n", err);
		goto fail_init_tbls;
	}

	return 0;

fail_init_tbls:
	rte_free(hw->pkg_copy);
fail_exit:
	rte_free(buf);
	return err;
}

static int
ice_dev_init(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev;
	struct rte_intr_handle *intr_handle;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct ice_vsi *vsi;
	int ret;

	dev->dev_ops = &ice_eth_dev_ops;
	dev->rx_pkt_burst = ice_recv_pkts;
	dev->tx_pkt_burst = ice_xmit_pkts;
	dev->tx_pkt_prepare = ice_prep_pkts;

	ice_set_default_ptype_table(dev);
	pci_dev = RTE_DEV_TO_PCI(dev->device);
	intr_handle = &pci_dev->intr_handle;

	pf->adapter = ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	pf->adapter->eth_dev = dev;
	pf->dev_data = dev->data;
	hw->back = pf->adapter;
	hw->hw_addr = (uint8_t *)pci_dev->mem_resource[0].addr;
	hw->vendor_id = pci_dev->id.vendor_id;
	hw->device_id = pci_dev->id.device_id;
	hw->subsystem_vendor_id = pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = pci_dev->id.subsystem_device_id;
	hw->bus.device = pci_dev->addr.devid;
	hw->bus.func = pci_dev->addr.function;

	ice_init_controlq_parameter(hw);

	ret = ice_init_hw(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to initialize HW");
		return -EINVAL;
	}

	ret = ice_load_pkg(dev);
	if (ret) {
		PMD_INIT_LOG(WARNING, "Failed to load the DDP package,"
				"Entering Safe Mode");
		ad->is_safe_mode = 1;
	}

	PMD_INIT_LOG(INFO, "FW %d.%d.%05d API %d.%d",
		     hw->fw_maj_ver, hw->fw_min_ver, hw->fw_build,
		     hw->api_maj_ver, hw->api_min_ver);

	ice_pf_sw_init(dev);
	ret = ice_init_mac_address(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to initialize mac address");
		goto err_init_mac;
	}

	ret = ice_res_pool_init(&pf->msix_pool, 1,
				hw->func_caps.common_cap.num_msix_vectors - 1);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to init MSIX pool");
		goto err_msix_pool_init;
	}

	ret = ice_pf_setup(pf);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to setup PF");
		goto err_pf_setup;
	}

	ret = ice_send_driver_ver(hw);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to send driver version");
		goto err_pf_setup;
	}

	vsi = pf->main_vsi;

	/* Disable double vlan by default */
	ice_vsi_config_double_vlan(vsi, FALSE);

	ret = ice_aq_stop_lldp(hw, TRUE, NULL);
	if (ret != ICE_SUCCESS)
		PMD_INIT_LOG(DEBUG, "lldp has already stopped\n");

	/* register callback func to eal lib */
	rte_intr_callback_register(intr_handle,
				   ice_interrupt_handler, dev);

	ice_pf_enable_irq0(hw);

	/* enable uio intr after callback register */
	rte_intr_enable(intr_handle);

	return 0;

err_pf_setup:
	ice_res_pool_destroy(&pf->msix_pool);
err_msix_pool_init:
	rte_free(dev->data->mac_addrs);
err_init_mac:
	ice_sched_cleanup_all(hw);
	rte_free(hw->port_info);
	ice_shutdown_all_ctrlq(hw);

	return ret;
}

static int
ice_release_vsi(struct ice_vsi *vsi)
{
	struct ice_hw *hw;
	struct ice_vsi_ctx vsi_ctx;
	enum ice_status ret;

	if (!vsi)
		return 0;

	hw = ICE_VSI_TO_HW(vsi);

	ice_remove_all_mac_vlan_filters(vsi);

	memset(&vsi_ctx, 0, sizeof(vsi_ctx));

	vsi_ctx.vsi_num = vsi->vsi_id;
	vsi_ctx.info = vsi->info;
	ret = ice_free_vsi(hw, vsi->idx, &vsi_ctx, false, NULL);
	if (ret != ICE_SUCCESS) {
		PMD_INIT_LOG(ERR, "Failed to free vsi by aq, %u", vsi->vsi_id);
		rte_free(vsi);
		return -1;
	}

	rte_free(vsi);
	return 0;
}

static void
ice_vsi_disable_queues_intr(struct ice_vsi *vsi)
{
	struct rte_eth_dev *dev = vsi->adapter->eth_dev;
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint16_t msix_intr, i;

	/* disable interrupt and also clear all the exist config */
	for (i = 0; i < vsi->nb_qps; i++) {
		ICE_WRITE_REG(hw, QINT_TQCTL(vsi->base_queue + i), 0);
		ICE_WRITE_REG(hw, QINT_RQCTL(vsi->base_queue + i), 0);
		rte_wmb();
	}

	if (rte_intr_allow_others(intr_handle))
		/* vfio-pci */
		for (i = 0; i < vsi->nb_msix; i++) {
			msix_intr = vsi->msix_intr + i;
			ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr),
				      GLINT_DYN_CTL_WB_ON_ITR_M);
		}
	else
		/* igb_uio */
		ICE_WRITE_REG(hw, GLINT_DYN_CTL(0), GLINT_DYN_CTL_WB_ON_ITR_M);
}

static void
ice_dev_stop(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *main_vsi = pf->main_vsi;
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	uint16_t i;

	/* avoid stopping again */
	if (pf->adapter_stopped)
		return;

	/* stop and clear all Rx queues */
	for (i = 0; i < data->nb_rx_queues; i++)
		ice_rx_queue_stop(dev, i);

	/* stop and clear all Tx queues */
	for (i = 0; i < data->nb_tx_queues; i++)
		ice_tx_queue_stop(dev, i);

	/* disable all queue interrupts */
	ice_vsi_disable_queues_intr(main_vsi);

	/* Clear all queues and release mbufs */
	ice_clear_queues(dev);

	/* Clean datapath event and queue/vec mapping */
	rte_intr_efd_disable(intr_handle);
	if (intr_handle->intr_vec) {
		rte_free(intr_handle->intr_vec);
		intr_handle->intr_vec = NULL;
	}

	pf->adapter_stopped = true;
}

static void
ice_dev_close(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	ice_dev_stop(dev);

	/* release all queue resource */
	ice_free_queues(dev);

	ice_res_pool_destroy(&pf->msix_pool);
	ice_release_vsi(pf->main_vsi);
	ice_sched_cleanup_all(hw);
	rte_free(hw->port_info);
	ice_shutdown_all_ctrlq(hw);
}

static int
ice_dev_uninit(struct rte_eth_dev *dev)
{
	struct rte_pci_device *pci_dev = RTE_ETH_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;

	ice_dev_close(dev);

	dev->dev_ops = NULL;
	dev->rx_pkt_burst = NULL;
	dev->tx_pkt_burst = NULL;

	rte_free(dev->data->mac_addrs);
	dev->data->mac_addrs = NULL;

	/* disable uio intr before callback unregister */
	rte_intr_disable(intr_handle);

	/* unregister callback func from eal lib */
	rte_intr_callback_unregister(intr_handle,
				     ice_interrupt_handler, dev);

	return 0;
}

static int
ice_dev_configure(__rte_unused struct rte_eth_dev *dev)
{
	struct ice_adapter *ad =
		ICE_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);

	/* Initialize to TRUE. If any of Rx queues doesn't meet the
	 * bulk allocation or vector Rx preconditions we will reset it.
	 */
	ad->rx_bulk_alloc_allowed = true;
	ad->tx_simple_allowed = true;

	return 0;
}

static int ice_init_rss(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev *dev = pf->adapter->eth_dev;
	struct rte_eth_rss_conf *rss_conf;
	struct ice_aqc_get_set_rss_keys key;
	uint16_t i, nb_q;
	int ret = 0;
	bool is_safe_mode = pf->adapter->is_safe_mode;

	rss_conf = &dev->data->dev_conf.rx_adv_conf.rss_conf;
	nb_q = dev->data->nb_rx_queues;
	vsi->rss_key_size = ICE_AQC_GET_SET_RSS_KEY_DATA_RSS_KEY_SIZE;
	vsi->rss_lut_size = hw->func_caps.common_cap.rss_table_size;

	if (is_safe_mode) {
		PMD_DRV_LOG(WARNING, "RSS is not supported in safe mode\n");
		return 0;
	}

	if (!vsi->rss_key)
		vsi->rss_key = rte_zmalloc(NULL,
					   vsi->rss_key_size, 0);
	if (!vsi->rss_lut)
		vsi->rss_lut = rte_zmalloc(NULL,
					   vsi->rss_lut_size, 0);

	/* configure RSS key */
	if (!rss_conf->rss_key) {
		/* Calculate the default hash key */
		for (i = 0; i <= vsi->rss_key_size; i++)
			vsi->rss_key[i] = (uint8_t)rte_rand();
	} else {
		rte_memcpy(vsi->rss_key, rss_conf->rss_key,
			   RTE_MIN(rss_conf->rss_key_len,
				   vsi->rss_key_size));
	}
	rte_memcpy(key.standard_rss_key, vsi->rss_key, vsi->rss_key_size);
	ret = ice_aq_set_rss_key(hw, vsi->idx, &key);
	if (ret)
		return -EINVAL;

	/* init RSS LUT table */
	for (i = 0; i < vsi->rss_lut_size; i++)
		vsi->rss_lut[i] = i % nb_q;

	ret = ice_aq_set_rss_lut(hw, vsi->idx,
				 ICE_AQC_GSET_RSS_LUT_TABLE_TYPE_PF,
				 vsi->rss_lut, vsi->rss_lut_size);
	if (ret)
		return -EINVAL;

	/* configure RSS for IPv4 with input set IPv4 src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_FLOW_HASH_IPV4,
			      ICE_FLOW_SEG_HDR_IPV4);
	if (ret)
		PMD_DRV_LOG(ERR, "%s IPV4 rss flow fail %d", __func__, ret);

	/* configure RSS for IPv6 with input set IPv6 src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_FLOW_HASH_IPV6,
			      ICE_FLOW_SEG_HDR_IPV6);
	if (ret)
		PMD_DRV_LOG(ERR, "%s IPV6 rss flow fail %d", __func__, ret);

	/* configure RSS for tcp6 with input set IPv6 src/dst, TCP src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_HASH_TCP_IPV6,
			      ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_IPV6);
	if (ret)
		PMD_DRV_LOG(ERR, "%s TCP_IPV6 rss flow fail %d", __func__, ret);

	/* configure RSS for udp6 with input set IPv6 src/dst, UDP src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_HASH_UDP_IPV6,
			      ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_IPV6);
	if (ret)
		PMD_DRV_LOG(ERR, "%s UDP_IPV6 rss flow fail %d", __func__, ret);

	/* configure RSS for sctp6 with input set IPv6 src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_FLOW_HASH_IPV6,
			      ICE_FLOW_SEG_HDR_SCTP | ICE_FLOW_SEG_HDR_IPV6);
	if (ret)
		PMD_DRV_LOG(ERR, "%s SCTP_IPV6 rss flow fail %d",
				__func__, ret);

	/* configure RSS for tcp4 with input set IP src/dst, TCP src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_HASH_TCP_IPV4,
			      ICE_FLOW_SEG_HDR_TCP | ICE_FLOW_SEG_HDR_IPV4);
	if (ret)
		PMD_DRV_LOG(ERR, "%s TCP_IPV4 rss flow fail %d", __func__, ret);

	/* configure RSS for udp4 with input set IP src/dst, UDP src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_HASH_UDP_IPV4,
			      ICE_FLOW_SEG_HDR_UDP | ICE_FLOW_SEG_HDR_IPV4);
	if (ret)
		PMD_DRV_LOG(ERR, "%s UDP_IPV4 rss flow fail %d", __func__, ret);

	/* configure RSS for sctp4 with input set IP src/dst */
	ret = ice_add_rss_cfg(hw, vsi->idx, ICE_FLOW_HASH_IPV4,
			      ICE_FLOW_SEG_HDR_SCTP | ICE_FLOW_SEG_HDR_IPV4);
	if (ret)
		PMD_DRV_LOG(ERR, "%s SCTP_IPV4 rss flow fail %d",
				__func__, ret);

	return 0;
}

static void
__vsi_queues_bind_intr(struct ice_vsi *vsi, uint16_t msix_vect,
		       int base_queue, int nb_queue)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint32_t val, val_tx;
	int i;

	for (i = 0; i < nb_queue; i++) {
		/*do actual bind*/
		val = (msix_vect & QINT_RQCTL_MSIX_INDX_M) |
		      (0 < QINT_RQCTL_ITR_INDX_S) | QINT_RQCTL_CAUSE_ENA_M;
		val_tx = (msix_vect & QINT_TQCTL_MSIX_INDX_M) |
			 (0 < QINT_TQCTL_ITR_INDX_S) | QINT_TQCTL_CAUSE_ENA_M;

		PMD_DRV_LOG(INFO, "queue %d is binding to vect %d",
			    base_queue + i, msix_vect);
		/* set ITR0 value */
		ICE_WRITE_REG(hw, GLINT_ITR(0, msix_vect), 0x10);
		ICE_WRITE_REG(hw, QINT_RQCTL(base_queue + i), val);
		ICE_WRITE_REG(hw, QINT_TQCTL(base_queue + i), val_tx);
	}
}

static void
ice_vsi_queues_bind_intr(struct ice_vsi *vsi)
{
	struct rte_eth_dev *dev = vsi->adapter->eth_dev;
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint16_t msix_vect = vsi->msix_intr;
	uint16_t nb_msix = RTE_MIN(vsi->nb_msix, intr_handle->nb_efd);
	uint16_t queue_idx = 0;
	int record = 0;
	int i;

	/* clear Rx/Tx queue interrupt */
	for (i = 0; i < vsi->nb_used_qps; i++) {
		ICE_WRITE_REG(hw, QINT_TQCTL(vsi->base_queue + i), 0);
		ICE_WRITE_REG(hw, QINT_RQCTL(vsi->base_queue + i), 0);
	}

	/* PF bind interrupt */
	if (rte_intr_dp_is_en(intr_handle)) {
		queue_idx = 0;
		record = 1;
	}

	for (i = 0; i < vsi->nb_used_qps; i++) {
		if (nb_msix <= 1) {
			if (!rte_intr_allow_others(intr_handle))
				msix_vect = ICE_MISC_VEC_ID;

			/* uio mapping all queue to one msix_vect */
			__vsi_queues_bind_intr(vsi, msix_vect,
					       vsi->base_queue + i,
					       vsi->nb_used_qps - i);

			for (; !!record && i < vsi->nb_used_qps; i++)
				intr_handle->intr_vec[queue_idx + i] =
					msix_vect;
			break;
		}

		/* vfio 1:1 queue/msix_vect mapping */
		__vsi_queues_bind_intr(vsi, msix_vect,
				       vsi->base_queue + i, 1);

		if (!!record)
			intr_handle->intr_vec[queue_idx + i] = msix_vect;

		msix_vect++;
		nb_msix--;
	}
}

static void
ice_vsi_enable_queues_intr(struct ice_vsi *vsi)
{
	struct rte_eth_dev *dev = vsi->adapter->eth_dev;
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	uint16_t msix_intr, i;

	if (rte_intr_allow_others(intr_handle))
		for (i = 0; i < vsi->nb_used_qps; i++) {
			msix_intr = vsi->msix_intr + i;
			ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr),
				      GLINT_DYN_CTL_INTENA_M |
				      GLINT_DYN_CTL_CLEARPBA_M |
				      GLINT_DYN_CTL_ITR_INDX_M |
				      GLINT_DYN_CTL_WB_ON_ITR_M);
		}
	else
		ICE_WRITE_REG(hw, GLINT_DYN_CTL(0),
			      GLINT_DYN_CTL_INTENA_M |
			      GLINT_DYN_CTL_CLEARPBA_M |
			      GLINT_DYN_CTL_ITR_INDX_M |
			      GLINT_DYN_CTL_WB_ON_ITR_M);
}

static int
ice_rxq_intr_setup(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_vsi *vsi = pf->main_vsi;
	uint32_t intr_vector = 0;

	rte_intr_disable(intr_handle);

	/* check and configure queue intr-vector mapping */
	if ((rte_intr_cap_multiple(intr_handle) ||
	     !RTE_ETH_DEV_SRIOV(dev).active) &&
	    dev->data->dev_conf.intr_conf.rxq != 0) {
		intr_vector = dev->data->nb_rx_queues;
		if (intr_vector > ICE_MAX_INTR_QUEUE_NUM) {
			PMD_DRV_LOG(ERR, "At most %d intr queues supported",
				    ICE_MAX_INTR_QUEUE_NUM);
			return -ENOTSUP;
		}
		if (rte_intr_efd_enable(intr_handle, intr_vector))
			return -1;
	}

	if (rte_intr_dp_is_en(intr_handle) && !intr_handle->intr_vec) {
		intr_handle->intr_vec =
		rte_zmalloc(NULL, dev->data->nb_rx_queues * sizeof(int),
			    0);
		if (!intr_handle->intr_vec) {
			PMD_DRV_LOG(ERR,
				    "Failed to allocate %d rx_queues intr_vec",
				    dev->data->nb_rx_queues);
			return -ENOMEM;
		}
	}

	/* Map queues with MSIX interrupt */
	vsi->nb_used_qps = dev->data->nb_rx_queues;
	ice_vsi_queues_bind_intr(vsi);

	/* Enable interrupts for all the queues */
	ice_vsi_enable_queues_intr(vsi);

	rte_intr_enable(intr_handle);

	return 0;
}

static int
ice_dev_start(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	uint16_t nb_rxq = 0;
	uint16_t nb_txq, i;
	int mask, ret;

	/* program Tx queues' context in hardware */
	for (nb_txq = 0; nb_txq < data->nb_tx_queues; nb_txq++) {
		ret = ice_tx_queue_start(dev, nb_txq);
		if (ret) {
			PMD_DRV_LOG(ERR, "fail to start Tx queue %u", nb_txq);
			goto tx_err;
		}
	}

	/* program Rx queues' context in hardware*/
	for (nb_rxq = 0; nb_rxq < data->nb_rx_queues; nb_rxq++) {
		ret = ice_rx_queue_start(dev, nb_rxq);
		if (ret) {
			PMD_DRV_LOG(ERR, "fail to start Rx queue %u", nb_rxq);
			goto rx_err;
		}
	}

	ret = ice_init_rss(pf);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to enable rss for PF");
		goto rx_err;
	}

	ice_set_rx_function(dev);
	ice_set_tx_function(dev);

	mask = ETH_VLAN_STRIP_MASK | ETH_VLAN_FILTER_MASK |
			ETH_VLAN_EXTEND_MASK;
	ret = ice_vlan_offload_set(dev, mask);
	if (ret) {
		PMD_INIT_LOG(ERR, "Unable to set VLAN offload");
		goto rx_err;
	}

	/* enable Rx interrput and mapping Rx queue to interrupt vector */
	if (ice_rxq_intr_setup(dev))
		return -EIO;

	/* Enable receiving broadcast packets and transmitting packets */
	ret = ice_set_vsi_promisc(hw, vsi->idx,
				  ICE_PROMISC_BCAST_RX | ICE_PROMISC_BCAST_TX |
				  ICE_PROMISC_UCAST_TX | ICE_PROMISC_MCAST_TX,
				  0);
	if (ret != ICE_SUCCESS)
		PMD_DRV_LOG(INFO, "fail to set vsi broadcast");

	ret = ice_aq_set_event_mask(hw, hw->port_info->lport,
				    ((u16)(ICE_AQ_LINK_EVENT_LINK_FAULT |
				     ICE_AQ_LINK_EVENT_PHY_TEMP_ALARM |
				     ICE_AQ_LINK_EVENT_EXCESSIVE_ERRORS |
				     ICE_AQ_LINK_EVENT_SIGNAL_DETECT |
				     ICE_AQ_LINK_EVENT_AN_COMPLETED |
				     ICE_AQ_LINK_EVENT_PORT_TX_SUSPENDED)),
				     NULL);
	if (ret != ICE_SUCCESS)
		PMD_DRV_LOG(WARNING, "Fail to set phy mask");

	/* Call get_link_info aq commond to enable/disable LSE */
	ice_link_update(dev, 0);

	pf->adapter_stopped = false;

	return 0;

	/* stop the started queues if failed to start all queues */
rx_err:
	for (i = 0; i < nb_rxq; i++)
		ice_rx_queue_stop(dev, i);
tx_err:
	for (i = 0; i < nb_txq; i++)
		ice_tx_queue_stop(dev, i);

	return -EIO;
}

static int
ice_dev_reset(struct rte_eth_dev *dev)
{
	int ret;

	if (dev->data->sriov.active)
		return -ENOTSUP;

	ret = ice_dev_uninit(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to uninit device, status = %d", ret);
		return -ENXIO;
	}

	ret = ice_dev_init(dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "failed to init device, status = %d", ret);
		return -ENXIO;
	}

	return 0;
}

static void
ice_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_pci_device *pci_dev = RTE_DEV_TO_PCI(dev->device);
	bool is_safe_mode = pf->adapter->is_safe_mode;
	u64 phy_type_low;
	u64 phy_type_high;

	dev_info->min_rx_bufsize = ICE_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = ICE_FRAME_SIZE_MAX;
	dev_info->max_rx_queues = vsi->nb_qps;
	dev_info->max_tx_queues = vsi->nb_qps;
	dev_info->max_mac_addrs = vsi->max_macaddrs;
	dev_info->max_vfs = pci_dev->max_vfs;

	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_VLAN_STRIP |
		DEV_RX_OFFLOAD_JUMBO_FRAME |
		DEV_RX_OFFLOAD_KEEP_CRC |
		DEV_RX_OFFLOAD_SCATTER |
		DEV_RX_OFFLOAD_VLAN_FILTER;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_VLAN_INSERT |
		DEV_TX_OFFLOAD_TCP_TSO |
		DEV_TX_OFFLOAD_MULTI_SEGS |
		DEV_TX_OFFLOAD_MBUF_FAST_FREE;
	dev_info->flow_type_rss_offloads = 0;

	if (!is_safe_mode) {
		dev_info->rx_offload_capa |=
			DEV_RX_OFFLOAD_IPV4_CKSUM |
			DEV_RX_OFFLOAD_UDP_CKSUM |
			DEV_RX_OFFLOAD_TCP_CKSUM |
			DEV_RX_OFFLOAD_QINQ_STRIP |
			DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM |
			DEV_RX_OFFLOAD_VLAN_EXTEND;
		dev_info->tx_offload_capa |=
			DEV_TX_OFFLOAD_QINQ_INSERT |
			DEV_TX_OFFLOAD_IPV4_CKSUM |
			DEV_TX_OFFLOAD_UDP_CKSUM |
			DEV_TX_OFFLOAD_TCP_CKSUM |
			DEV_TX_OFFLOAD_SCTP_CKSUM |
			DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;
		dev_info->flow_type_rss_offloads |= ICE_RSS_OFFLOAD_ALL;
	}

	dev_info->rx_queue_offload_capa = 0;
	dev_info->tx_queue_offload_capa = 0;

	dev_info->reta_size = hw->func_caps.common_cap.rss_table_size;
	dev_info->hash_key_size = (VSIQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t);

	dev_info->default_rxconf = (struct rte_eth_rxconf) {
		.rx_thresh = {
			.pthresh = ICE_DEFAULT_RX_PTHRESH,
			.hthresh = ICE_DEFAULT_RX_HTHRESH,
			.wthresh = ICE_DEFAULT_RX_WTHRESH,
		},
		.rx_free_thresh = ICE_DEFAULT_RX_FREE_THRESH,
		.rx_drop_en = 0,
		.offloads = 0,
	};

	dev_info->default_txconf = (struct rte_eth_txconf) {
		.tx_thresh = {
			.pthresh = ICE_DEFAULT_TX_PTHRESH,
			.hthresh = ICE_DEFAULT_TX_HTHRESH,
			.wthresh = ICE_DEFAULT_TX_WTHRESH,
		},
		.tx_free_thresh = ICE_DEFAULT_TX_FREE_THRESH,
		.tx_rs_thresh = ICE_DEFAULT_TX_RSBIT_THRESH,
		.offloads = 0,
	};

	dev_info->rx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

	dev_info->tx_desc_lim = (struct rte_eth_desc_lim) {
		.nb_max = ICE_MAX_RING_DESC,
		.nb_min = ICE_MIN_RING_DESC,
		.nb_align = ICE_ALIGN_RING_DESC,
	};

	dev_info->speed_capa = ETH_LINK_SPEED_10M |
			       ETH_LINK_SPEED_100M |
			       ETH_LINK_SPEED_1G |
			       ETH_LINK_SPEED_2_5G |
			       ETH_LINK_SPEED_5G |
			       ETH_LINK_SPEED_10G |
			       ETH_LINK_SPEED_20G |
			       ETH_LINK_SPEED_25G;

	phy_type_low = hw->port_info->phy.phy_type_low;
	phy_type_high = hw->port_info->phy.phy_type_high;

	if (ICE_PHY_TYPE_SUPPORT_50G(phy_type_low))
		dev_info->speed_capa |= ETH_LINK_SPEED_50G;

	if (ICE_PHY_TYPE_SUPPORT_100G_LOW(phy_type_low) ||
			ICE_PHY_TYPE_SUPPORT_100G_HIGH(phy_type_high))
		dev_info->speed_capa |= ETH_LINK_SPEED_100G;

	dev_info->nb_rx_queues = dev->data->nb_rx_queues;
	dev_info->nb_tx_queues = dev->data->nb_tx_queues;

	dev_info->default_rxportconf.burst_size = ICE_RX_MAX_BURST;
	dev_info->default_txportconf.burst_size = ICE_TX_MAX_BURST;
	dev_info->default_rxportconf.nb_queues = 1;
	dev_info->default_txportconf.nb_queues = 1;
	dev_info->default_rxportconf.ring_size = ICE_BUF_SIZE_MIN;
	dev_info->default_txportconf.ring_size = ICE_BUF_SIZE_MIN;
}

static inline int
ice_atomic_read_link_status(struct rte_eth_dev *dev,
			    struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &dev->data->dev_link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static inline int
ice_atomic_write_link_status(struct rte_eth_dev *dev,
			     struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static int
ice_link_update(struct rte_eth_dev *dev, int wait_to_complete)
{
#define CHECK_INTERVAL 100  /* 100ms */
#define MAX_REPEAT_TIME 10  /* 1s (10 * 100ms) in total */
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_link_status link_status;
	struct rte_eth_link link, old;
	int status;
	unsigned int rep_cnt = MAX_REPEAT_TIME;
	bool enable_lse = dev->data->dev_conf.intr_conf.lsc ? true : false;

	memset(&link, 0, sizeof(link));
	memset(&old, 0, sizeof(old));
	memset(&link_status, 0, sizeof(link_status));
	ice_atomic_read_link_status(dev, &old);

	do {
		/* Get link status information from hardware */
		status = ice_aq_get_link_info(hw->port_info, enable_lse,
					      &link_status, NULL);
		if (status != ICE_SUCCESS) {
			link.link_speed = ETH_SPEED_NUM_100M;
			link.link_duplex = ETH_LINK_FULL_DUPLEX;
			PMD_DRV_LOG(ERR, "Failed to get link info");
			goto out;
		}

		link.link_status = link_status.link_info & ICE_AQ_LINK_UP;
		if (!wait_to_complete || link.link_status)
			break;

		rte_delay_ms(CHECK_INTERVAL);
	} while (--rep_cnt);

	if (!link.link_status)
		goto out;

	/* Full-duplex operation at all supported speeds */
	link.link_duplex = ETH_LINK_FULL_DUPLEX;

	/* Parse the link status */
	switch (link_status.link_speed) {
	case ICE_AQ_LINK_SPEED_10MB:
		link.link_speed = ETH_SPEED_NUM_10M;
		break;
	case ICE_AQ_LINK_SPEED_100MB:
		link.link_speed = ETH_SPEED_NUM_100M;
		break;
	case ICE_AQ_LINK_SPEED_1000MB:
		link.link_speed = ETH_SPEED_NUM_1G;
		break;
	case ICE_AQ_LINK_SPEED_2500MB:
		link.link_speed = ETH_SPEED_NUM_2_5G;
		break;
	case ICE_AQ_LINK_SPEED_5GB:
		link.link_speed = ETH_SPEED_NUM_5G;
		break;
	case ICE_AQ_LINK_SPEED_10GB:
		link.link_speed = ETH_SPEED_NUM_10G;
		break;
	case ICE_AQ_LINK_SPEED_20GB:
		link.link_speed = ETH_SPEED_NUM_20G;
		break;
	case ICE_AQ_LINK_SPEED_25GB:
		link.link_speed = ETH_SPEED_NUM_25G;
		break;
	case ICE_AQ_LINK_SPEED_40GB:
		link.link_speed = ETH_SPEED_NUM_40G;
		break;
	case ICE_AQ_LINK_SPEED_50GB:
		link.link_speed = ETH_SPEED_NUM_50G;
		break;
	case ICE_AQ_LINK_SPEED_100GB:
		link.link_speed = ETH_SPEED_NUM_100G;
		break;
	case ICE_AQ_LINK_SPEED_UNKNOWN:
	default:
		PMD_DRV_LOG(ERR, "Unknown link speed");
		link.link_speed = ETH_SPEED_NUM_NONE;
		break;
	}

	link.link_autoneg = !(dev->data->dev_conf.link_speeds &
			      ETH_LINK_SPEED_FIXED);

out:
	ice_atomic_write_link_status(dev, &link);
	if (link.link_status == old.link_status)
		return -1;

	return 0;
}

static int
ice_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct rte_eth_dev_data *dev_data = pf->dev_data;
	uint32_t frame_size = mtu + ICE_ETH_OVERHEAD;

	/* check if mtu is within the allowed range */
	if (mtu < ETHER_MIN_MTU || frame_size > ICE_FRAME_SIZE_MAX)
		return -EINVAL;

	/* mtu setting is forbidden if port is start */
	if (dev_data->dev_started) {
		PMD_DRV_LOG(ERR,
			    "port %d must be stopped before configuration",
			    dev_data->port_id);
		return -EBUSY;
	}

	if (frame_size > ETHER_MAX_LEN)
		dev_data->dev_conf.rxmode.offloads |=
			DEV_RX_OFFLOAD_JUMBO_FRAME;
	else
		dev_data->dev_conf.rxmode.offloads &=
			~DEV_RX_OFFLOAD_JUMBO_FRAME;

	dev_data->dev_conf.rxmode.max_rx_pkt_len = frame_size;

	return 0;
}

static int ice_macaddr_set(struct rte_eth_dev *dev,
			   struct ether_addr *mac_addr)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct ice_mac_filter *f;
	uint8_t flags = 0;
	int ret;

	if (!is_valid_assigned_ether_addr(mac_addr)) {
		PMD_DRV_LOG(ERR, "Tried to set invalid MAC address.");
		return -EINVAL;
	}

	TAILQ_FOREACH(f, &vsi->mac_list, next) {
		if (is_same_ether_addr(&pf->dev_addr, &f->mac_info.mac_addr))
			break;
	}

	if (!f) {
		PMD_DRV_LOG(ERR, "Failed to find filter for default mac");
		return -EIO;
	}

	ret = ice_remove_mac_filter(vsi, &f->mac_info.mac_addr);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to delete mac filter");
		return -EIO;
	}
	ret = ice_add_mac_filter(vsi, mac_addr);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add mac filter");
		return -EIO;
	}
	memcpy(&pf->dev_addr, mac_addr, ETH_ADDR_LEN);

	flags = ICE_AQC_MAN_MAC_UPDATE_LAA_WOL;
	ret = ice_aq_manage_mac_write(hw, mac_addr->addr_bytes, flags, NULL);
	if (ret != ICE_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to set manage mac");

	return 0;
}

/* Add a MAC address, and update filters */
static int
ice_macaddr_add(struct rte_eth_dev *dev,
		struct ether_addr *mac_addr,
		__rte_unused uint32_t index,
		__rte_unused uint32_t pool)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;

	ret = ice_add_mac_filter(vsi, mac_addr);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR, "Failed to add MAC filter");
		return -EINVAL;
	}

	return ICE_SUCCESS;
}

/* Remove a MAC address, and update filters */
static void
ice_macaddr_remove(struct rte_eth_dev *dev, uint32_t index)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *data = dev->data;
	struct ether_addr *macaddr;
	int ret;

	macaddr = &data->mac_addrs[index];
	ret = ice_remove_mac_filter(vsi, macaddr);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to remove MAC filter");
		return;
	}
}

static int
ice_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (on) {
		ret = ice_add_vlan_filter(vsi, vlan_id);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to add vlan filter");
			return -EINVAL;
		}
	} else {
		ret = ice_remove_vlan_filter(vsi, vlan_id);
		if (ret < 0) {
			PMD_DRV_LOG(ERR, "Failed to remove vlan filter");
			return -EINVAL;
		}
	}

	return 0;
}

/* Configure vlan filter on or off */
static int
ice_vsi_config_vlan_filter(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t sec_flags, sw_flags2;
	int ret = 0;

	sec_flags = ICE_AQ_VSI_SEC_TX_VLAN_PRUNE_ENA <<
		    ICE_AQ_VSI_SEC_TX_PRUNE_ENA_S;
	sw_flags2 = ICE_AQ_VSI_SW_FLAG_RX_VLAN_PRUNE_ENA;

	if (on) {
		vsi->info.sec_flags |= sec_flags;
		vsi->info.sw_flags2 |= sw_flags2;
	} else {
		vsi->info.sec_flags &= ~sec_flags;
		vsi->info.sw_flags2 &= ~sw_flags2;
	}
	vsi->info.sw_id = hw->port_info->sw_id;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_SW_VALID |
				 ICE_AQ_VSI_PROP_SECURITY_VALID);
	ctxt.vsi_num = vsi->vsi_id;

	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO, "Update VSI failed to %s vlan rx pruning",
			    on ? "enable" : "disable");
		return -EINVAL;
	} else {
		vsi->info.valid_sections |=
			rte_cpu_to_le_16(ICE_AQ_VSI_PROP_SW_VALID |
					 ICE_AQ_VSI_PROP_SECURITY_VALID);
	}

	/* consist with other drivers, allow untagged packet when vlan filter on */
	if (on)
		ret = ice_add_vlan_filter(vsi, 0);
	else
		ret = ice_remove_vlan_filter(vsi, 0);

	return 0;
}

static int
ice_vsi_config_vlan_stripping(struct ice_vsi *vsi, bool on)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	struct ice_vsi_ctx ctxt;
	uint8_t vlan_flags;
	int ret = 0;

	/* Check if it has been already on or off */
	if (vsi->info.valid_sections &
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID)) {
		if (on) {
			if ((vsi->info.vlan_flags &
			     ICE_AQ_VSI_VLAN_EMOD_M) ==
			    ICE_AQ_VSI_VLAN_EMOD_STR_BOTH)
				return 0; /* already on */
		} else {
			if ((vsi->info.vlan_flags &
			     ICE_AQ_VSI_VLAN_EMOD_M) ==
			    ICE_AQ_VSI_VLAN_EMOD_NOTHING)
				return 0; /* already off */
		}
	}

	if (on)
		vlan_flags = ICE_AQ_VSI_VLAN_EMOD_STR_BOTH;
	else
		vlan_flags = ICE_AQ_VSI_VLAN_EMOD_NOTHING;
	vsi->info.vlan_flags &= ~(ICE_AQ_VSI_VLAN_EMOD_M);
	vsi->info.vlan_flags |= vlan_flags;
	(void)rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);
	ctxt.vsi_num = vsi->vsi_id;
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret) {
		PMD_DRV_LOG(INFO, "Update VSI failed to %s vlan stripping",
			    on ? "enable" : "disable");
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);

	return ret;
}

static int
ice_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_rxmode *rxmode;

	rxmode = &dev->data->dev_conf.rxmode;
	if (mask & ETH_VLAN_FILTER_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_FILTER)
			ice_vsi_config_vlan_filter(vsi, TRUE);
		else
			ice_vsi_config_vlan_filter(vsi, FALSE);
	}

	if (mask & ETH_VLAN_STRIP_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_STRIP)
			ice_vsi_config_vlan_stripping(vsi, TRUE);
		else
			ice_vsi_config_vlan_stripping(vsi, FALSE);
	}

	if (mask & ETH_VLAN_EXTEND_MASK) {
		if (rxmode->offloads & DEV_RX_OFFLOAD_VLAN_EXTEND)
			ice_vsi_config_double_vlan(vsi, TRUE);
		else
			ice_vsi_config_double_vlan(vsi, FALSE);
	}

	return 0;
}

static int
ice_vlan_tpid_set(struct rte_eth_dev *dev,
		  enum rte_vlan_type vlan_type,
		  uint16_t tpid)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint64_t reg_r = 0, reg_w = 0;
	uint16_t reg_id = 0;
	int ret = 0;
	int qinq = dev->data->dev_conf.rxmode.offloads &
		   DEV_RX_OFFLOAD_VLAN_EXTEND;

	switch (vlan_type) {
	case ETH_VLAN_TYPE_OUTER:
		if (qinq)
			reg_id = 3;
		else
			reg_id = 5;
		break;
	case ETH_VLAN_TYPE_INNER:
		if (qinq) {
			reg_id = 5;
		} else {
			PMD_DRV_LOG(ERR,
				    "Unsupported vlan type in single vlan.");
			return -EINVAL;
		}
		break;
	default:
		PMD_DRV_LOG(ERR, "Unsupported vlan type %d", vlan_type);
		return -EINVAL;
	}
	reg_r = ICE_READ_REG(hw, GL_SWT_L2TAGCTRL(reg_id));
	PMD_DRV_LOG(DEBUG, "Debug read from ICE GL_SWT_L2TAGCTRL[%d]: "
		    "0x%08"PRIx64"", reg_id, reg_r);

	reg_w = reg_r & (~(GL_SWT_L2TAGCTRL_ETHERTYPE_M));
	reg_w |= ((uint64_t)tpid << GL_SWT_L2TAGCTRL_ETHERTYPE_S);
	if (reg_r == reg_w) {
		PMD_DRV_LOG(DEBUG, "No need to write");
		return 0;
	}

	ICE_WRITE_REG(hw, GL_SWT_L2TAGCTRL(reg_id), reg_w);
	PMD_DRV_LOG(DEBUG, "Debug write 0x%08"PRIx64" to "
		    "ICE GL_SWT_L2TAGCTRL[%d]", reg_w, reg_id);

	return ret;
}

static int
ice_get_rss_lut(struct ice_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct ice_pf *pf = ICE_VSI_TO_PF(vsi);
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret;

	if (!lut)
		return -EINVAL;

	if (pf->flags & ICE_FLAG_RSS_AQ_CAPABLE) {
		ret = ice_aq_get_rss_lut(hw, vsi->idx, TRUE,
					 lut, lut_size);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to get RSS lookup table");
			return -EINVAL;
		}
	} else {
		uint64_t *lut_dw = (uint64_t *)lut;
		uint16_t i, lut_size_dw = lut_size / 4;

		for (i = 0; i < lut_size_dw; i++)
			lut_dw[i] = ICE_READ_REG(hw, PFQF_HLUT(i));
	}

	return 0;
}

static int
ice_set_rss_lut(struct ice_vsi *vsi, uint8_t *lut, uint16_t lut_size)
{
	struct ice_pf *pf;
	struct ice_hw *hw;
	int ret;

	if (!vsi || !lut)
		return -EINVAL;

	pf = ICE_VSI_TO_PF(vsi);
	hw = ICE_VSI_TO_HW(vsi);

	if (pf->flags & ICE_FLAG_RSS_AQ_CAPABLE) {
		ret = ice_aq_set_rss_lut(hw, vsi->idx, TRUE,
					 lut, lut_size);
		if (ret) {
			PMD_DRV_LOG(ERR, "Failed to set RSS lookup table");
			return -EINVAL;
		}
	} else {
		uint64_t *lut_dw = (uint64_t *)lut;
		uint16_t i, lut_size_dw = lut_size / 4;

		for (i = 0; i < lut_size_dw; i++)
			ICE_WRITE_REG(hw, PFQF_HLUT(i), lut_dw[i]);

		ice_flush(hw);
	}

	return 0;
}

static int
ice_rss_reta_update(struct rte_eth_dev *dev,
		    struct rte_eth_rss_reta_entry64 *reta_conf,
		    uint16_t reta_size)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t i, lut_size = hw->func_caps.common_cap.rss_table_size;
	uint16_t idx, shift;
	uint8_t *lut;
	int ret;

	if (reta_size != lut_size ||
	    reta_size > ETH_RSS_RETA_SIZE_512) {
		PMD_DRV_LOG(ERR,
			    "The size of hash lookup table configured (%d)"
			    "doesn't match the number hardware can "
			    "supported (%d)",
			    reta_size, lut_size);
		return -EINVAL;
	}

	lut = rte_zmalloc(NULL, reta_size, 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}
	ret = ice_get_rss_lut(pf->main_vsi, lut, reta_size);
	if (ret)
		goto out;

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			lut[i] = reta_conf[idx].reta[shift];
	}
	ret = ice_set_rss_lut(pf->main_vsi, lut, reta_size);

out:
	rte_free(lut);

	return ret;
}

static int
ice_rss_reta_query(struct rte_eth_dev *dev,
		   struct rte_eth_rss_reta_entry64 *reta_conf,
		   uint16_t reta_size)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t i, lut_size = hw->func_caps.common_cap.rss_table_size;
	uint16_t idx, shift;
	uint8_t *lut;
	int ret;

	if (reta_size != lut_size ||
	    reta_size > ETH_RSS_RETA_SIZE_512) {
		PMD_DRV_LOG(ERR,
			    "The size of hash lookup table configured (%d)"
			    "doesn't match the number hardware can "
			    "supported (%d)",
			    reta_size, lut_size);
		return -EINVAL;
	}

	lut = rte_zmalloc(NULL, reta_size, 0);
	if (!lut) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}

	ret = ice_get_rss_lut(pf->main_vsi, lut, reta_size);
	if (ret)
		goto out;

	for (i = 0; i < reta_size; i++) {
		idx = i / RTE_RETA_GROUP_SIZE;
		shift = i % RTE_RETA_GROUP_SIZE;
		if (reta_conf[idx].mask & (1ULL << shift))
			reta_conf[idx].reta[shift] = lut[i];
	}

out:
	rte_free(lut);

	return ret;
}

static int
ice_set_rss_key(struct ice_vsi *vsi, uint8_t *key, uint8_t key_len)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret = 0;

	if (!key || key_len == 0) {
		PMD_DRV_LOG(DEBUG, "No key to be configured");
		return 0;
	} else if (key_len != (VSIQF_HKEY_MAX_INDEX + 1) *
		   sizeof(uint32_t)) {
		PMD_DRV_LOG(ERR, "Invalid key length %u", key_len);
		return -EINVAL;
	}

	struct ice_aqc_get_set_rss_keys *key_dw =
		(struct ice_aqc_get_set_rss_keys *)key;

	ret = ice_aq_set_rss_key(hw, vsi->idx, key_dw);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to configure RSS key via AQ");
		ret = -EINVAL;
	}

	return ret;
}

static int
ice_get_rss_key(struct ice_vsi *vsi, uint8_t *key, uint8_t *key_len)
{
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int ret;

	if (!key || !key_len)
		return -EINVAL;

	ret = ice_aq_get_rss_key
		(hw, vsi->idx,
		 (struct ice_aqc_get_set_rss_keys *)key);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to get RSS key via AQ");
		return -EINVAL;
	}
	*key_len = (VSIQF_HKEY_MAX_INDEX + 1) * sizeof(uint32_t);

	return 0;
}

static int
ice_rss_hash_update(struct rte_eth_dev *dev,
		    struct rte_eth_rss_conf *rss_conf)
{
	enum ice_status status = ICE_SUCCESS;
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;

	/* set hash key */
	status = ice_set_rss_key(vsi, rss_conf->rss_key, rss_conf->rss_key_len);
	if (status)
		return status;

	/* TODO: hash enable config, ice_add_rss_cfg */
	return 0;
}

static int
ice_rss_hash_conf_get(struct rte_eth_dev *dev,
		      struct rte_eth_rss_conf *rss_conf)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;

	ice_get_rss_key(vsi, rss_conf->rss_key,
			&rss_conf->rss_key_len);

	/* TODO: default set to 0 as hf config is not supported now */
	rss_conf->rss_hf = 0;
	return 0;
}

static void
ice_promisc_enable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;

	pmask = ICE_PROMISC_UCAST_RX | ICE_PROMISC_UCAST_TX |
		ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_set_vsi_promisc(hw, vsi->idx, pmask, 0);
	if (status == ICE_ERR_ALREADY_EXISTS)
		PMD_DRV_LOG(DEBUG, "Promisc mode has already been enabled");
	else if (status != ICE_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable promisc, err=%d", status);
}

static void
ice_promisc_disable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;

	pmask = ICE_PROMISC_UCAST_RX | ICE_PROMISC_UCAST_TX |
		ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_clear_vsi_promisc(hw, vsi->idx, pmask, 0);
	if (status != ICE_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to clear promisc, err=%d", status);
}

static void
ice_allmulti_enable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;

	pmask = ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_set_vsi_promisc(hw, vsi->idx, pmask, 0);
	if (status != ICE_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to enable allmulti, err=%d", status);
}

static void
ice_allmulti_disable(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	enum ice_status status;
	uint8_t pmask;

	if (dev->data->promiscuous == 1)
		return; /* must remain in all_multicast mode */

	pmask = ICE_PROMISC_MCAST_RX | ICE_PROMISC_MCAST_TX;

	status = ice_clear_vsi_promisc(hw, vsi->idx, pmask, 0);
	if (status != ICE_SUCCESS)
		PMD_DRV_LOG(ERR, "Failed to clear allmulti, err=%d", status);
}

static int ice_rx_queue_intr_enable(struct rte_eth_dev *dev,
				    uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint32_t val;
	uint16_t msix_intr;

	msix_intr = intr_handle->intr_vec[queue_id];

	val = GLINT_DYN_CTL_INTENA_M | GLINT_DYN_CTL_CLEARPBA_M |
	      GLINT_DYN_CTL_ITR_INDX_M;
	val &= ~GLINT_DYN_CTL_WB_ON_ITR_M;

	ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr), val);
	rte_intr_enable(&pci_dev->intr_handle);

	return 0;
}

static int ice_rx_queue_intr_disable(struct rte_eth_dev *dev,
				     uint16_t queue_id)
{
	struct rte_pci_device *pci_dev = ICE_DEV_TO_PCI(dev);
	struct rte_intr_handle *intr_handle = &pci_dev->intr_handle;
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t msix_intr;

	msix_intr = intr_handle->intr_vec[queue_id];

	ICE_WRITE_REG(hw, GLINT_DYN_CTL(msix_intr), GLINT_DYN_CTL_WB_ON_ITR_M);

	return 0;
}

static int
ice_fw_version_get(struct rte_eth_dev *dev, char *fw_version, size_t fw_size)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	u32 full_ver;
	u8 ver, patch;
	u16 build;
	int ret;

	full_ver = hw->nvm.oem_ver;
	ver = (u8)(full_ver >> 24);
	build = (u16)((full_ver >> 8) & 0xffff);
	patch = (u8)(full_ver & 0xff);

	ret = snprintf(fw_version, fw_size,
			"%d.%d%d 0x%08x %d.%d.%d",
			((hw->nvm.ver >> 12) & 0xf),
			((hw->nvm.ver >> 4) & 0xff),
			(hw->nvm.ver & 0xf), hw->nvm.eetrack,
			ver, build, patch);

	/* add the size of '\0' */
	ret += 1;
	if (fw_size < (u32)ret)
		return ret;
	else
		return 0;
}

static int
ice_vsi_vlan_pvid_set(struct ice_vsi *vsi, struct ice_vsi_vlan_pvid_info *info)
{
	struct ice_hw *hw;
	struct ice_vsi_ctx ctxt;
	uint8_t vlan_flags = 0;
	int ret;

	if (!vsi || !info) {
		PMD_DRV_LOG(ERR, "invalid parameters");
		return -EINVAL;
	}

	if (info->on) {
		vsi->info.pvid = info->config.pvid;
		/**
		 * If insert pvid is enabled, only tagged pkts are
		 * allowed to be sent out.
		 */
		vlan_flags = ICE_AQ_VSI_PVLAN_INSERT_PVID |
			     ICE_AQ_VSI_VLAN_MODE_UNTAGGED;
	} else {
		vsi->info.pvid = 0;
		if (info->config.reject.tagged == 0)
			vlan_flags |= ICE_AQ_VSI_VLAN_MODE_TAGGED;

		if (info->config.reject.untagged == 0)
			vlan_flags |= ICE_AQ_VSI_VLAN_MODE_UNTAGGED;
	}
	vsi->info.vlan_flags &= ~(ICE_AQ_VSI_PVLAN_INSERT_PVID |
				  ICE_AQ_VSI_VLAN_MODE_M);
	vsi->info.vlan_flags |= vlan_flags;
	memset(&ctxt, 0, sizeof(ctxt));
	rte_memcpy(&ctxt.info, &vsi->info, sizeof(vsi->info));
	ctxt.info.valid_sections =
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);
	ctxt.vsi_num = vsi->vsi_id;

	hw = ICE_VSI_TO_HW(vsi);
	ret = ice_update_vsi(hw, vsi->idx, &ctxt, NULL);
	if (ret != ICE_SUCCESS) {
		PMD_DRV_LOG(ERR,
			    "update VSI for VLAN insert failed, err %d",
			    ret);
		return -EINVAL;
	}

	vsi->info.valid_sections |=
		rte_cpu_to_le_16(ICE_AQ_VSI_PROP_VLAN_VALID);

	return ret;
}

static int
ice_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_vsi *vsi = pf->main_vsi;
	struct rte_eth_dev_data *data = pf->dev_data;
	struct ice_vsi_vlan_pvid_info info;
	int ret;

	memset(&info, 0, sizeof(info));
	info.on = on;
	if (info.on) {
		info.config.pvid = pvid;
	} else {
		info.config.reject.tagged =
			data->dev_conf.txmode.hw_vlan_reject_tagged;
		info.config.reject.untagged =
			data->dev_conf.txmode.hw_vlan_reject_untagged;
	}

	ret = ice_vsi_vlan_pvid_set(vsi, &info);
	if (ret < 0) {
		PMD_DRV_LOG(ERR, "Failed to set pvid.");
		return -EINVAL;
	}

	return 0;
}

static int
ice_get_eeprom_length(struct rte_eth_dev *dev)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Convert word count to byte count */
	return hw->nvm.sr_words << 1;
}

static int
ice_get_eeprom(struct rte_eth_dev *dev,
	       struct rte_dev_eeprom_info *eeprom)
{
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	uint16_t *data = eeprom->data;
	uint16_t first_word, last_word, nwords;
	enum ice_status status = ICE_SUCCESS;

	first_word = eeprom->offset >> 1;
	last_word = (eeprom->offset + eeprom->length - 1) >> 1;
	nwords = last_word - first_word + 1;

	if (first_word >= hw->nvm.sr_words ||
	    last_word >= hw->nvm.sr_words) {
		PMD_DRV_LOG(ERR, "Requested EEPROM bytes out of range.");
		return -EINVAL;
	}

	eeprom->magic = hw->vendor_id | (hw->device_id << 16);

	status = ice_read_sr_buf(hw, first_word, &nwords, data);
	if (status) {
		PMD_DRV_LOG(ERR, "EEPROM read failed.");
		eeprom->length = sizeof(uint16_t) * nwords;
		return -EIO;
	}

	return 0;
}

static void
ice_stat_update_32(struct ice_hw *hw,
		   uint32_t reg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)ICE_READ_REG(hw, reg);
	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = (uint64_t)(new_data - *offset);
	else
		*stat = (uint64_t)((new_data +
				    ((uint64_t)1 << ICE_32_BIT_WIDTH))
				   - *offset);
}

static void
ice_stat_update_40(struct ice_hw *hw,
		   uint32_t hireg,
		   uint32_t loreg,
		   bool offset_loaded,
		   uint64_t *offset,
		   uint64_t *stat)
{
	uint64_t new_data;

	new_data = (uint64_t)ICE_READ_REG(hw, loreg);
	new_data |= (uint64_t)(ICE_READ_REG(hw, hireg) & ICE_8_BIT_MASK) <<
		    ICE_32_BIT_WIDTH;

	if (!offset_loaded)
		*offset = new_data;

	if (new_data >= *offset)
		*stat = new_data - *offset;
	else
		*stat = (uint64_t)((new_data +
				    ((uint64_t)1 << ICE_40_BIT_WIDTH)) -
				   *offset);

	*stat &= ICE_40_BIT_MASK;
}

/* Get all the statistics of a VSI */
static void
ice_update_vsi_stats(struct ice_vsi *vsi)
{
	struct ice_eth_stats *oes = &vsi->eth_stats_offset;
	struct ice_eth_stats *nes = &vsi->eth_stats;
	struct ice_hw *hw = ICE_VSI_TO_HW(vsi);
	int idx = rte_le_to_cpu_16(vsi->vsi_id);

	ice_stat_update_40(hw, GLV_GORCH(idx), GLV_GORCL(idx),
			   vsi->offset_loaded, &oes->rx_bytes,
			   &nes->rx_bytes);
	ice_stat_update_40(hw, GLV_UPRCH(idx), GLV_UPRCL(idx),
			   vsi->offset_loaded, &oes->rx_unicast,
			   &nes->rx_unicast);
	ice_stat_update_40(hw, GLV_MPRCH(idx), GLV_MPRCL(idx),
			   vsi->offset_loaded, &oes->rx_multicast,
			   &nes->rx_multicast);
	ice_stat_update_40(hw, GLV_BPRCH(idx), GLV_BPRCL(idx),
			   vsi->offset_loaded, &oes->rx_broadcast,
			   &nes->rx_broadcast);
	/* exclude CRC bytes */
	nes->rx_bytes -= (nes->rx_unicast + nes->rx_multicast +
			  nes->rx_broadcast) * ETHER_CRC_LEN;

	ice_stat_update_32(hw, GLV_RDPC(idx), vsi->offset_loaded,
			   &oes->rx_discards, &nes->rx_discards);
	/* GLV_REPC not supported */
	/* GLV_RMPC not supported */
	ice_stat_update_32(hw, GLSWID_RUPP(idx), vsi->offset_loaded,
			   &oes->rx_unknown_protocol,
			   &nes->rx_unknown_protocol);
	ice_stat_update_40(hw, GLV_GOTCH(idx), GLV_GOTCL(idx),
			   vsi->offset_loaded, &oes->tx_bytes,
			   &nes->tx_bytes);
	ice_stat_update_40(hw, GLV_UPTCH(idx), GLV_UPTCL(idx),
			   vsi->offset_loaded, &oes->tx_unicast,
			   &nes->tx_unicast);
	ice_stat_update_40(hw, GLV_MPTCH(idx), GLV_MPTCL(idx),
			   vsi->offset_loaded, &oes->tx_multicast,
			   &nes->tx_multicast);
	ice_stat_update_40(hw, GLV_BPTCH(idx), GLV_BPTCL(idx),
			   vsi->offset_loaded,  &oes->tx_broadcast,
			   &nes->tx_broadcast);
	/* GLV_TDPC not supported */
	ice_stat_update_32(hw, GLV_TEPC(idx), vsi->offset_loaded,
			   &oes->tx_errors, &nes->tx_errors);
	vsi->offset_loaded = true;

	PMD_DRV_LOG(DEBUG, "************** VSI[%u] stats start **************",
		    vsi->vsi_id);
	PMD_DRV_LOG(DEBUG, "rx_bytes:            %"PRIu64"", nes->rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:          %"PRIu64"", nes->rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:        %"PRIu64"", nes->rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:        %"PRIu64"", nes->rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:         %"PRIu64"", nes->rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol: %"PRIu64"",
		    nes->rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:            %"PRIu64"", nes->tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:          %"PRIu64"", nes->tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:        %"PRIu64"", nes->tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:        %"PRIu64"", nes->tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:         %"PRIu64"", nes->tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:           %"PRIu64"", nes->tx_errors);
	PMD_DRV_LOG(DEBUG, "************** VSI[%u] stats end ****************",
		    vsi->vsi_id);
}

static void
ice_read_stats_registers(struct ice_pf *pf, struct ice_hw *hw)
{
	struct ice_hw_port_stats *ns = &pf->stats; /* new stats */
	struct ice_hw_port_stats *os = &pf->stats_offset; /* old stats */

	/* Get statistics of struct ice_eth_stats */
	ice_stat_update_40(hw, GLPRT_GORCH(hw->port_info->lport),
			   GLPRT_GORCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_bytes,
			   &ns->eth.rx_bytes);
	ice_stat_update_40(hw, GLPRT_UPRCH(hw->port_info->lport),
			   GLPRT_UPRCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_unicast,
			   &ns->eth.rx_unicast);
	ice_stat_update_40(hw, GLPRT_MPRCH(hw->port_info->lport),
			   GLPRT_MPRCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_multicast,
			   &ns->eth.rx_multicast);
	ice_stat_update_40(hw, GLPRT_BPRCH(hw->port_info->lport),
			   GLPRT_BPRCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.rx_broadcast,
			   &ns->eth.rx_broadcast);
	ice_stat_update_32(hw, PRTRPB_RDPC,
			   pf->offset_loaded, &os->eth.rx_discards,
			   &ns->eth.rx_discards);

	/* Workaround: CRC size should not be included in byte statistics,
	 * so subtract ETHER_CRC_LEN from the byte counter for each rx packet.
	 */
	ns->eth.rx_bytes -= (ns->eth.rx_unicast + ns->eth.rx_multicast +
			     ns->eth.rx_broadcast) * ETHER_CRC_LEN;

	/* GLPRT_REPC not supported */
	/* GLPRT_RMPC not supported */
	ice_stat_update_32(hw, GLSWID_RUPP(hw->port_info->lport),
			   pf->offset_loaded,
			   &os->eth.rx_unknown_protocol,
			   &ns->eth.rx_unknown_protocol);
	ice_stat_update_40(hw, GLPRT_GOTCH(hw->port_info->lport),
			   GLPRT_GOTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_bytes,
			   &ns->eth.tx_bytes);
	ice_stat_update_40(hw, GLPRT_UPTCH(hw->port_info->lport),
			   GLPRT_UPTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_unicast,
			   &ns->eth.tx_unicast);
	ice_stat_update_40(hw, GLPRT_MPTCH(hw->port_info->lport),
			   GLPRT_MPTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_multicast,
			   &ns->eth.tx_multicast);
	ice_stat_update_40(hw, GLPRT_BPTCH(hw->port_info->lport),
			   GLPRT_BPTCL(hw->port_info->lport),
			   pf->offset_loaded, &os->eth.tx_broadcast,
			   &ns->eth.tx_broadcast);
	ns->eth.tx_bytes -= (ns->eth.tx_unicast + ns->eth.tx_multicast +
			     ns->eth.tx_broadcast) * ETHER_CRC_LEN;

	/* GLPRT_TEPC not supported */

	/* additional port specific stats */
	ice_stat_update_32(hw, GLPRT_TDOLD(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_dropped_link_down,
			   &ns->tx_dropped_link_down);
	ice_stat_update_32(hw, GLPRT_CRCERRS(hw->port_info->lport),
			   pf->offset_loaded, &os->crc_errors,
			   &ns->crc_errors);
	ice_stat_update_32(hw, GLPRT_ILLERRC(hw->port_info->lport),
			   pf->offset_loaded, &os->illegal_bytes,
			   &ns->illegal_bytes);
	/* GLPRT_ERRBC not supported */
	ice_stat_update_32(hw, GLPRT_MLFC(hw->port_info->lport),
			   pf->offset_loaded, &os->mac_local_faults,
			   &ns->mac_local_faults);
	ice_stat_update_32(hw, GLPRT_MRFC(hw->port_info->lport),
			   pf->offset_loaded, &os->mac_remote_faults,
			   &ns->mac_remote_faults);

	ice_stat_update_32(hw, GLPRT_RLEC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_len_errors,
			   &ns->rx_len_errors);

	ice_stat_update_32(hw, GLPRT_LXONRXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xon_rx,
			   &ns->link_xon_rx);
	ice_stat_update_32(hw, GLPRT_LXOFFRXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xoff_rx,
			   &ns->link_xoff_rx);
	ice_stat_update_32(hw, GLPRT_LXONTXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xon_tx,
			   &ns->link_xon_tx);
	ice_stat_update_32(hw, GLPRT_LXOFFTXC(hw->port_info->lport),
			   pf->offset_loaded, &os->link_xoff_tx,
			   &ns->link_xoff_tx);
	ice_stat_update_40(hw, GLPRT_PRC64H(hw->port_info->lport),
			   GLPRT_PRC64L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_64,
			   &ns->rx_size_64);
	ice_stat_update_40(hw, GLPRT_PRC127H(hw->port_info->lport),
			   GLPRT_PRC127L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_127,
			   &ns->rx_size_127);
	ice_stat_update_40(hw, GLPRT_PRC255H(hw->port_info->lport),
			   GLPRT_PRC255L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_255,
			   &ns->rx_size_255);
	ice_stat_update_40(hw, GLPRT_PRC511H(hw->port_info->lport),
			   GLPRT_PRC511L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_511,
			   &ns->rx_size_511);
	ice_stat_update_40(hw, GLPRT_PRC1023H(hw->port_info->lport),
			   GLPRT_PRC1023L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_1023,
			   &ns->rx_size_1023);
	ice_stat_update_40(hw, GLPRT_PRC1522H(hw->port_info->lport),
			   GLPRT_PRC1522L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_1522,
			   &ns->rx_size_1522);
	ice_stat_update_40(hw, GLPRT_PRC9522H(hw->port_info->lport),
			   GLPRT_PRC9522L(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_size_big,
			   &ns->rx_size_big);
	ice_stat_update_32(hw, GLPRT_RUC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_undersize,
			   &ns->rx_undersize);
	ice_stat_update_32(hw, GLPRT_RFC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_fragments,
			   &ns->rx_fragments);
	ice_stat_update_32(hw, GLPRT_ROC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_oversize,
			   &ns->rx_oversize);
	ice_stat_update_32(hw, GLPRT_RJC(hw->port_info->lport),
			   pf->offset_loaded, &os->rx_jabber,
			   &ns->rx_jabber);
	ice_stat_update_40(hw, GLPRT_PTC64H(hw->port_info->lport),
			   GLPRT_PTC64L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_64,
			   &ns->tx_size_64);
	ice_stat_update_40(hw, GLPRT_PTC127H(hw->port_info->lport),
			   GLPRT_PTC127L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_127,
			   &ns->tx_size_127);
	ice_stat_update_40(hw, GLPRT_PTC255H(hw->port_info->lport),
			   GLPRT_PTC255L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_255,
			   &ns->tx_size_255);
	ice_stat_update_40(hw, GLPRT_PTC511H(hw->port_info->lport),
			   GLPRT_PTC511L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_511,
			   &ns->tx_size_511);
	ice_stat_update_40(hw, GLPRT_PTC1023H(hw->port_info->lport),
			   GLPRT_PTC1023L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_1023,
			   &ns->tx_size_1023);
	ice_stat_update_40(hw, GLPRT_PTC1522H(hw->port_info->lport),
			   GLPRT_PTC1522L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_1522,
			   &ns->tx_size_1522);
	ice_stat_update_40(hw, GLPRT_PTC9522H(hw->port_info->lport),
			   GLPRT_PTC9522L(hw->port_info->lport),
			   pf->offset_loaded, &os->tx_size_big,
			   &ns->tx_size_big);

	/* GLPRT_MSPDC not supported */
	/* GLPRT_XEC not supported */

	pf->offset_loaded = true;

	if (pf->main_vsi)
		ice_update_vsi_stats(pf->main_vsi);
}

/* Get all statistics of a port */
static int
ice_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ice_hw_port_stats *ns = &pf->stats; /* new stats */

	/* call read registers - updates values, now write them to struct */
	ice_read_stats_registers(pf, hw);

	stats->ipackets = pf->main_vsi->eth_stats.rx_unicast +
			  pf->main_vsi->eth_stats.rx_multicast +
			  pf->main_vsi->eth_stats.rx_broadcast -
			  pf->main_vsi->eth_stats.rx_discards;
	stats->opackets = ns->eth.tx_unicast +
			  ns->eth.tx_multicast +
			  ns->eth.tx_broadcast;
	stats->ibytes   = pf->main_vsi->eth_stats.rx_bytes;
	stats->obytes   = ns->eth.tx_bytes;
	stats->oerrors  = ns->eth.tx_errors +
			  pf->main_vsi->eth_stats.tx_errors;

	/* Rx Errors */
	stats->imissed  = ns->eth.rx_discards +
			  pf->main_vsi->eth_stats.rx_discards;
	stats->ierrors  = ns->crc_errors +
			  ns->rx_undersize +
			  ns->rx_oversize + ns->rx_fragments + ns->rx_jabber;

	PMD_DRV_LOG(DEBUG, "*************** PF stats start *****************");
	PMD_DRV_LOG(DEBUG, "rx_bytes:	%"PRIu64"", ns->eth.rx_bytes);
	PMD_DRV_LOG(DEBUG, "rx_unicast:	%"PRIu64"", ns->eth.rx_unicast);
	PMD_DRV_LOG(DEBUG, "rx_multicast:%"PRIu64"", ns->eth.rx_multicast);
	PMD_DRV_LOG(DEBUG, "rx_broadcast:%"PRIu64"", ns->eth.rx_broadcast);
	PMD_DRV_LOG(DEBUG, "rx_discards:%"PRIu64"", ns->eth.rx_discards);
	PMD_DRV_LOG(DEBUG, "vsi rx_discards:%"PRIu64"",
		    pf->main_vsi->eth_stats.rx_discards);
	PMD_DRV_LOG(DEBUG, "rx_unknown_protocol:  %"PRIu64"",
		    ns->eth.rx_unknown_protocol);
	PMD_DRV_LOG(DEBUG, "tx_bytes:	%"PRIu64"", ns->eth.tx_bytes);
	PMD_DRV_LOG(DEBUG, "tx_unicast:	%"PRIu64"", ns->eth.tx_unicast);
	PMD_DRV_LOG(DEBUG, "tx_multicast:%"PRIu64"", ns->eth.tx_multicast);
	PMD_DRV_LOG(DEBUG, "tx_broadcast:%"PRIu64"", ns->eth.tx_broadcast);
	PMD_DRV_LOG(DEBUG, "tx_discards:%"PRIu64"", ns->eth.tx_discards);
	PMD_DRV_LOG(DEBUG, "vsi tx_discards:%"PRIu64"",
		    pf->main_vsi->eth_stats.tx_discards);
	PMD_DRV_LOG(DEBUG, "tx_errors:		%"PRIu64"", ns->eth.tx_errors);

	PMD_DRV_LOG(DEBUG, "tx_dropped_link_down:	%"PRIu64"",
		    ns->tx_dropped_link_down);
	PMD_DRV_LOG(DEBUG, "crc_errors:	%"PRIu64"", ns->crc_errors);
	PMD_DRV_LOG(DEBUG, "illegal_bytes:	%"PRIu64"",
		    ns->illegal_bytes);
	PMD_DRV_LOG(DEBUG, "error_bytes:	%"PRIu64"", ns->error_bytes);
	PMD_DRV_LOG(DEBUG, "mac_local_faults:	%"PRIu64"",
		    ns->mac_local_faults);
	PMD_DRV_LOG(DEBUG, "mac_remote_faults:	%"PRIu64"",
		    ns->mac_remote_faults);
	PMD_DRV_LOG(DEBUG, "link_xon_rx:	%"PRIu64"", ns->link_xon_rx);
	PMD_DRV_LOG(DEBUG, "link_xoff_rx:	%"PRIu64"", ns->link_xoff_rx);
	PMD_DRV_LOG(DEBUG, "link_xon_tx:	%"PRIu64"", ns->link_xon_tx);
	PMD_DRV_LOG(DEBUG, "link_xoff_tx:	%"PRIu64"", ns->link_xoff_tx);
	PMD_DRV_LOG(DEBUG, "rx_size_64:		%"PRIu64"", ns->rx_size_64);
	PMD_DRV_LOG(DEBUG, "rx_size_127:	%"PRIu64"", ns->rx_size_127);
	PMD_DRV_LOG(DEBUG, "rx_size_255:	%"PRIu64"", ns->rx_size_255);
	PMD_DRV_LOG(DEBUG, "rx_size_511:	%"PRIu64"", ns->rx_size_511);
	PMD_DRV_LOG(DEBUG, "rx_size_1023:	%"PRIu64"", ns->rx_size_1023);
	PMD_DRV_LOG(DEBUG, "rx_size_1522:	%"PRIu64"", ns->rx_size_1522);
	PMD_DRV_LOG(DEBUG, "rx_size_big:	%"PRIu64"", ns->rx_size_big);
	PMD_DRV_LOG(DEBUG, "rx_undersize:	%"PRIu64"", ns->rx_undersize);
	PMD_DRV_LOG(DEBUG, "rx_fragments:	%"PRIu64"", ns->rx_fragments);
	PMD_DRV_LOG(DEBUG, "rx_oversize:	%"PRIu64"", ns->rx_oversize);
	PMD_DRV_LOG(DEBUG, "rx_jabber:		%"PRIu64"", ns->rx_jabber);
	PMD_DRV_LOG(DEBUG, "tx_size_64:		%"PRIu64"", ns->tx_size_64);
	PMD_DRV_LOG(DEBUG, "tx_size_127:	%"PRIu64"", ns->tx_size_127);
	PMD_DRV_LOG(DEBUG, "tx_size_255:	%"PRIu64"", ns->tx_size_255);
	PMD_DRV_LOG(DEBUG, "tx_size_511:	%"PRIu64"", ns->tx_size_511);
	PMD_DRV_LOG(DEBUG, "tx_size_1023:	%"PRIu64"", ns->tx_size_1023);
	PMD_DRV_LOG(DEBUG, "tx_size_1522:	%"PRIu64"", ns->tx_size_1522);
	PMD_DRV_LOG(DEBUG, "tx_size_big:	%"PRIu64"", ns->tx_size_big);
	PMD_DRV_LOG(DEBUG, "rx_len_errors:	%"PRIu64"", ns->rx_len_errors);
	PMD_DRV_LOG(DEBUG, "************* PF stats end ****************");
	return 0;
}

/* Reset the statistics */
static void
ice_stats_reset(struct rte_eth_dev *dev)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	/* Mark PF and VSI stats to update the offset, aka "reset" */
	pf->offset_loaded = false;
	if (pf->main_vsi)
		pf->main_vsi->offset_loaded = false;

	/* read the stats, reading current register values into offset */
	ice_read_stats_registers(pf, hw);
}

static uint32_t
ice_xstats_calc_num(void)
{
	uint32_t num;

	num = ICE_NB_ETH_XSTATS + ICE_NB_HW_PORT_XSTATS;

	return num;
}

static int
ice_xstats_get(struct rte_eth_dev *dev, struct rte_eth_xstat *xstats,
	       unsigned int n)
{
	struct ice_pf *pf = ICE_DEV_PRIVATE_TO_PF(dev->data->dev_private);
	struct ice_hw *hw = ICE_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	unsigned int i;
	unsigned int count;
	struct ice_hw_port_stats *hw_stats = &pf->stats;

	count = ice_xstats_calc_num();
	if (n < count)
		return count;

	ice_read_stats_registers(pf, hw);

	if (!xstats)
		return 0;

	count = 0;

	/* Get stats from ice_eth_stats struct */
	for (i = 0; i < ICE_NB_ETH_XSTATS; i++) {
		xstats[count].value =
			*(uint64_t *)((char *)&hw_stats->eth +
				      ice_stats_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	/* Get individiual stats from ice_hw_port struct */
	for (i = 0; i < ICE_NB_HW_PORT_XSTATS; i++) {
		xstats[count].value =
			*(uint64_t *)((char *)hw_stats +
				      ice_hw_port_strings[i].offset);
		xstats[count].id = count;
		count++;
	}

	return count;
}

static int ice_xstats_get_names(__rte_unused struct rte_eth_dev *dev,
				struct rte_eth_xstat_name *xstats_names,
				__rte_unused unsigned int limit)
{
	unsigned int count = 0;
	unsigned int i;

	if (!xstats_names)
		return ice_xstats_calc_num();

	/* Note: limit checked in rte_eth_xstats_names() */

	/* Get stats from ice_eth_stats struct */
	for (i = 0; i < ICE_NB_ETH_XSTATS; i++) {
		strlcpy(xstats_names[count].name, ice_stats_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	/* Get individiual stats from ice_hw_port struct */
	for (i = 0; i < ICE_NB_HW_PORT_XSTATS; i++) {
		strlcpy(xstats_names[count].name, ice_hw_port_strings[i].name,
			sizeof(xstats_names[count].name));
		count++;
	}

	return count;
}

static int
ice_pci_probe(struct rte_pci_driver *pci_drv __rte_unused,
	      struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_probe(pci_dev,
					     sizeof(struct ice_adapter),
					     ice_dev_init);
}

static int
ice_pci_remove(struct rte_pci_device *pci_dev)
{
	return rte_eth_dev_pci_generic_remove(pci_dev, ice_dev_uninit);
}

static struct rte_pci_driver rte_ice_pmd = {
	.id_table = pci_id_ice_map,
	.drv_flags = RTE_PCI_DRV_NEED_MAPPING | RTE_PCI_DRV_INTR_LSC |
		     RTE_PCI_DRV_IOVA_AS_VA,
	.probe = ice_pci_probe,
	.remove = ice_pci_remove,
};

/**
 * Driver initialization routine.
 * Invoked once at EAL init time.
 * Register itself as the [Poll Mode] Driver of PCI devices.
 */
RTE_PMD_REGISTER_PCI(net_ice, rte_ice_pmd);
RTE_PMD_REGISTER_PCI_TABLE(net_ice, pci_id_ice_map);
RTE_PMD_REGISTER_KMOD_DEP(net_ice, "* igb_uio | uio_pci_generic | vfio-pci");
RTE_PMD_REGISTER_PARAM_STRING(net_ice,
			      ICE_MAX_QP_NUM "=<int>");

RTE_INIT(ice_init_log)
{
	ice_logtype_init = rte_log_register("pmd.net.ice.init");
	if (ice_logtype_init >= 0)
		rte_log_set_level(ice_logtype_init, RTE_LOG_NOTICE);
	ice_logtype_driver = rte_log_register("pmd.net.ice.driver");
	if (ice_logtype_driver >= 0)
		rte_log_set_level(ice_logtype_driver, RTE_LOG_NOTICE);
}
