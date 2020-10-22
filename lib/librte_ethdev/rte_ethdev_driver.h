/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_ETHDEV_DRIVER_H_
#define _RTE_ETHDEV_DRIVER_H_

/**
 * @file
 *
 * RTE Ethernet Device PMD API
 *
 * These APIs for the use from Ethernet drivers, user applications shouldn't
 * use them.
 *
 */

#include <rte_ethdev.h>

#ifdef __cplusplus
extern "C" {
#endif

/**< @internal Declaration of the hairpin peer queue information structure. */
struct rte_hairpin_peer_info;

/*
 * Definitions of all functions exported by an Ethernet driver through the
 * generic structure of type *eth_dev_ops* supplied in the *rte_eth_dev*
 * structure associated with an Ethernet device.
 */

typedef int  (*eth_dev_configure_t)(struct rte_eth_dev *dev);
/**< @internal Ethernet device configuration. */

typedef int  (*eth_dev_start_t)(struct rte_eth_dev *dev);
/**< @internal Function used to start a configured Ethernet device. */

typedef int (*eth_dev_stop_t)(struct rte_eth_dev *dev);
/**< @internal Function used to stop a configured Ethernet device. */

typedef int  (*eth_dev_set_link_up_t)(struct rte_eth_dev *dev);
/**< @internal Function used to link up a configured Ethernet device. */

typedef int  (*eth_dev_set_link_down_t)(struct rte_eth_dev *dev);
/**< @internal Function used to link down a configured Ethernet device. */

typedef int (*eth_dev_close_t)(struct rte_eth_dev *dev);
/**< @internal Function used to close a configured Ethernet device. */

typedef int (*eth_dev_reset_t)(struct rte_eth_dev *dev);
/** <@internal Function used to reset a configured Ethernet device. */

typedef int (*eth_is_removed_t)(struct rte_eth_dev *dev);
/**< @internal Function used to detect an Ethernet device removal. */

/**
 * @internal
 * Function used to enable the Rx promiscuous mode of an Ethernet device.
 *
 * @param dev
 *   ethdev handle of port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, promiscuous mode is enabled.
 * @retval -ENOTSUP
 *   Promiscuous mode is not supported.
 * @retval -ENODEV
 *   Device is gone.
 * @retval -E_RTE_SECONDARY
 *   Function was called from a secondary process instance and not supported.
 * @retval -ETIMEDOUT
 *   Attempt to enable promiscuos mode failed because of timeout.
 * @retval -EAGAIN
 *   Failed to enable promiscuous mode.
 */
typedef int (*eth_promiscuous_enable_t)(struct rte_eth_dev *dev);

/**
 * @internal
 * Function used to disable the Rx promiscuous mode of an Ethernet device.
 *
 * @param dev
 *   ethdev handle of port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, promiscuous mode is disabled.
 * @retval -ENOTSUP
 *   Promiscuous mode disabling is not supported.
 * @retval -ENODEV
 *   Device is gone.
 * @retval -E_RTE_SECONDARY
 *   Function was called from a secondary process instance and not supported.
 * @retval -ETIMEDOUT
 *   Attempt to disable promiscuos mode failed because of timeout.
 * @retval -EAGAIN
 *   Failed to disable promiscuous mode.
 */
typedef int (*eth_promiscuous_disable_t)(struct rte_eth_dev *dev);

/**
 * @internal
 * Enable the receipt of all multicast packets by an Ethernet device.
 *
 * @param dev
 *   ethdev handle of port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, all-multicast mode is enabled.
 * @retval -ENOTSUP
 *   All-multicast mode is not supported.
 * @retval -ENODEV
 *   Device is gone.
 * @retval -E_RTE_SECONDARY
 *   Function was called from a secondary process instance and not supported.
 * @retval -ETIMEDOUT
 *   Attempt to enable all-multicast mode failed because of timeout.
 * @retval -EAGAIN
 *   Failed to enable all-multicast mode.
 */
typedef int (*eth_allmulticast_enable_t)(struct rte_eth_dev *dev);

/**
 * @internal
 * Disable the receipt of all multicast packets by an Ethernet device.
 *
 * @param dev
 *   ethdev handle of port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, all-multicast mode is disabled.
 * @retval -ENOTSUP
 *   All-multicast mode disabling is not supported.
 * @retval -ENODEV
 *   Device is gone.
 * @retval -E_RTE_SECONDARY
 *   Function was called from a secondary process instance and not supported.
 * @retval -ETIMEDOUT
 *   Attempt to disable all-multicast mode failed because of timeout.
 * @retval -EAGAIN
 *   Failed to disable all-multicast mode.
 */
typedef int (*eth_allmulticast_disable_t)(struct rte_eth_dev *dev);

typedef int (*eth_link_update_t)(struct rte_eth_dev *dev,
				int wait_to_complete);
/**< @internal Get link speed, duplex mode and state (up/down) of an Ethernet device. */

typedef int (*eth_stats_get_t)(struct rte_eth_dev *dev,
				struct rte_eth_stats *igb_stats);
/**< @internal Get global I/O statistics of an Ethernet device. */

/**
 * @internal
 * Reset global I/O statistics of an Ethernet device to 0.
 *
 * @param dev
 *   ethdev handle of port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, statistics has been reset.
 * @retval -ENOTSUP
 *   Resetting statistics is not supported.
 * @retval -EINVAL
 *   Resetting statistics is not valid.
 * @retval -ENOMEM
 *   Not enough memory to get the stats.
 */
typedef int (*eth_stats_reset_t)(struct rte_eth_dev *dev);

typedef int (*eth_xstats_get_t)(struct rte_eth_dev *dev,
	struct rte_eth_xstat *stats, unsigned int n);
/**< @internal Get extended stats of an Ethernet device. */

typedef int (*eth_xstats_get_by_id_t)(struct rte_eth_dev *dev,
				      const uint64_t *ids,
				      uint64_t *values,
				      unsigned int n);
/**< @internal Get extended stats of an Ethernet device. */

/**
 * @internal
 * Reset extended stats of an Ethernet device.
 *
 * @param dev
 *   ethdev handle of port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, statistics has been reset.
 * @retval -ENOTSUP
 *   Resetting statistics is not supported.
 * @retval -EINVAL
 *   Resetting statistics is not valid.
 * @retval -ENOMEM
 *   Not enough memory to get the stats.
 */
typedef int (*eth_xstats_reset_t)(struct rte_eth_dev *dev);

typedef int (*eth_xstats_get_names_t)(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned int size);
/**< @internal Get names of extended stats of an Ethernet device. */

typedef int (*eth_xstats_get_names_by_id_t)(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, const uint64_t *ids,
	unsigned int size);
/**< @internal Get names of extended stats of an Ethernet device. */

typedef int (*eth_queue_stats_mapping_set_t)(struct rte_eth_dev *dev,
					     uint16_t queue_id,
					     uint8_t stat_idx,
					     uint8_t is_rx);
/**< @internal Set a queue statistics mapping for a tx/rx queue of an Ethernet device. */

typedef int (*eth_dev_infos_get_t)(struct rte_eth_dev *dev,
				   struct rte_eth_dev_info *dev_info);
/**< @internal Get specific information of an Ethernet device. */

typedef const uint32_t *(*eth_dev_supported_ptypes_get_t)(struct rte_eth_dev *dev);
/**< @internal Get supported ptypes of an Ethernet device. */

/**
 * @internal
 * Inform Ethernet device about reduced range of packet types to handle.
 *
 * @param dev
 *   The Ethernet device identifier.
 * @param ptype_mask
 *   The ptype family that application is interested in should be bitwise OR of
 *   RTE_PTYPE_*_MASK or 0.
 * @return
 *   - (0) if Success.
 */
typedef int (*eth_dev_ptypes_set_t)(struct rte_eth_dev *dev,
				     uint32_t ptype_mask);

typedef int (*eth_queue_start_t)(struct rte_eth_dev *dev,
				    uint16_t queue_id);
/**< @internal Start rx and tx of a queue of an Ethernet device. */

typedef int (*eth_queue_stop_t)(struct rte_eth_dev *dev,
				    uint16_t queue_id);
/**< @internal Stop rx and tx of a queue of an Ethernet device. */

typedef int (*eth_rx_queue_setup_t)(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id,
				    uint16_t nb_rx_desc,
				    unsigned int socket_id,
				    const struct rte_eth_rxconf *rx_conf,
				    struct rte_mempool *mb_pool);
/**< @internal Set up a receive queue of an Ethernet device. */

typedef int (*eth_tx_queue_setup_t)(struct rte_eth_dev *dev,
				    uint16_t tx_queue_id,
				    uint16_t nb_tx_desc,
				    unsigned int socket_id,
				    const struct rte_eth_txconf *tx_conf);
/**< @internal Setup a transmit queue of an Ethernet device. */

typedef int (*eth_rx_enable_intr_t)(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id);
/**< @internal Enable interrupt of a receive queue of an Ethernet device. */

typedef int (*eth_rx_disable_intr_t)(struct rte_eth_dev *dev,
				    uint16_t rx_queue_id);
/**< @internal Disable interrupt of a receive queue of an Ethernet device. */

typedef void (*eth_queue_release_t)(void *queue);
/**< @internal Release memory resources allocated by given RX/TX queue. */

typedef int (*eth_fw_version_get_t)(struct rte_eth_dev *dev,
				     char *fw_version, size_t fw_size);
/**< @internal Get firmware information of an Ethernet device. */

typedef int (*eth_tx_done_cleanup_t)(void *txq, uint32_t free_cnt);
/**< @internal Force mbufs to be from TX ring. */

typedef void (*eth_rxq_info_get_t)(struct rte_eth_dev *dev,
	uint16_t rx_queue_id, struct rte_eth_rxq_info *qinfo);

typedef void (*eth_txq_info_get_t)(struct rte_eth_dev *dev,
	uint16_t tx_queue_id, struct rte_eth_txq_info *qinfo);

typedef int (*eth_burst_mode_get_t)(struct rte_eth_dev *dev,
	uint16_t queue_id, struct rte_eth_burst_mode *mode);

typedef int (*mtu_set_t)(struct rte_eth_dev *dev, uint16_t mtu);
/**< @internal Set MTU. */

typedef int (*vlan_filter_set_t)(struct rte_eth_dev *dev,
				  uint16_t vlan_id,
				  int on);
/**< @internal filtering of a VLAN Tag Identifier by an Ethernet device. */

typedef int (*vlan_tpid_set_t)(struct rte_eth_dev *dev,
			       enum rte_vlan_type type, uint16_t tpid);
/**< @internal set the outer/inner VLAN-TPID by an Ethernet device. */

typedef int (*vlan_offload_set_t)(struct rte_eth_dev *dev, int mask);
/**< @internal set VLAN offload function by an Ethernet device. */

typedef int (*vlan_pvid_set_t)(struct rte_eth_dev *dev,
			       uint16_t vlan_id,
			       int on);
/**< @internal set port based TX VLAN insertion by an Ethernet device. */

typedef void (*vlan_strip_queue_set_t)(struct rte_eth_dev *dev,
				  uint16_t rx_queue_id,
				  int on);
/**< @internal VLAN stripping enable/disable by an queue of Ethernet device. */

typedef int (*flow_ctrl_get_t)(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf);
/**< @internal Get current flow control parameter on an Ethernet device */

typedef int (*flow_ctrl_set_t)(struct rte_eth_dev *dev,
			       struct rte_eth_fc_conf *fc_conf);
/**< @internal Setup flow control parameter on an Ethernet device */

typedef int (*priority_flow_ctrl_set_t)(struct rte_eth_dev *dev,
				struct rte_eth_pfc_conf *pfc_conf);
/**< @internal Setup priority flow control parameter on an Ethernet device */

typedef int (*reta_update_t)(struct rte_eth_dev *dev,
			     struct rte_eth_rss_reta_entry64 *reta_conf,
			     uint16_t reta_size);
/**< @internal Update RSS redirection table on an Ethernet device */

typedef int (*reta_query_t)(struct rte_eth_dev *dev,
			    struct rte_eth_rss_reta_entry64 *reta_conf,
			    uint16_t reta_size);
/**< @internal Query RSS redirection table on an Ethernet device */

typedef int (*rss_hash_update_t)(struct rte_eth_dev *dev,
				 struct rte_eth_rss_conf *rss_conf);
/**< @internal Update RSS hash configuration of an Ethernet device */

typedef int (*rss_hash_conf_get_t)(struct rte_eth_dev *dev,
				   struct rte_eth_rss_conf *rss_conf);
/**< @internal Get current RSS hash configuration of an Ethernet device */

typedef int (*eth_dev_led_on_t)(struct rte_eth_dev *dev);
/**< @internal Turn on SW controllable LED on an Ethernet device */

typedef int (*eth_dev_led_off_t)(struct rte_eth_dev *dev);
/**< @internal Turn off SW controllable LED on an Ethernet device */

typedef void (*eth_mac_addr_remove_t)(struct rte_eth_dev *dev, uint32_t index);
/**< @internal Remove MAC address from receive address register */

typedef int (*eth_mac_addr_add_t)(struct rte_eth_dev *dev,
				  struct rte_ether_addr *mac_addr,
				  uint32_t index,
				  uint32_t vmdq);
/**< @internal Set a MAC address into Receive Address Register */

typedef int (*eth_mac_addr_set_t)(struct rte_eth_dev *dev,
				  struct rte_ether_addr *mac_addr);
/**< @internal Set a MAC address into Receive Address Register */

typedef int (*eth_uc_hash_table_set_t)(struct rte_eth_dev *dev,
				  struct rte_ether_addr *mac_addr,
				  uint8_t on);
/**< @internal Set a Unicast Hash bitmap */

typedef int (*eth_uc_all_hash_table_set_t)(struct rte_eth_dev *dev,
				  uint8_t on);
/**< @internal Set all Unicast Hash bitmap */

typedef int (*eth_set_queue_rate_limit_t)(struct rte_eth_dev *dev,
				uint16_t queue_idx,
				uint16_t tx_rate);
/**< @internal Set queue TX rate */

typedef int (*eth_mirror_rule_set_t)(struct rte_eth_dev *dev,
				  struct rte_eth_mirror_conf *mirror_conf,
				  uint8_t rule_id,
				  uint8_t on);
/**< @internal Add a traffic mirroring rule on an Ethernet device */

typedef int (*eth_mirror_rule_reset_t)(struct rte_eth_dev *dev,
				  uint8_t rule_id);
/**< @internal Remove a traffic mirroring rule on an Ethernet device */

typedef int (*eth_udp_tunnel_port_add_t)(struct rte_eth_dev *dev,
					 struct rte_eth_udp_tunnel *tunnel_udp);
/**< @internal Add tunneling UDP port */

typedef int (*eth_udp_tunnel_port_del_t)(struct rte_eth_dev *dev,
					 struct rte_eth_udp_tunnel *tunnel_udp);
/**< @internal Delete tunneling UDP port */

typedef int (*eth_set_mc_addr_list_t)(struct rte_eth_dev *dev,
				      struct rte_ether_addr *mc_addr_set,
				      uint32_t nb_mc_addr);
/**< @internal set the list of multicast addresses on an Ethernet device */

typedef int (*eth_timesync_enable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to enable IEEE1588/802.1AS timestamping. */

typedef int (*eth_timesync_disable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to disable IEEE1588/802.1AS timestamping. */

typedef int (*eth_timesync_read_rx_timestamp_t)(struct rte_eth_dev *dev,
						struct timespec *timestamp,
						uint32_t flags);
/**< @internal Function used to read an RX IEEE1588/802.1AS timestamp. */

typedef int (*eth_timesync_read_tx_timestamp_t)(struct rte_eth_dev *dev,
						struct timespec *timestamp);
/**< @internal Function used to read a TX IEEE1588/802.1AS timestamp. */

typedef int (*eth_timesync_adjust_time)(struct rte_eth_dev *dev, int64_t);
/**< @internal Function used to adjust the device clock */

typedef int (*eth_timesync_read_time)(struct rte_eth_dev *dev,
				      struct timespec *timestamp);
/**< @internal Function used to get time from the device clock. */

typedef int (*eth_timesync_write_time)(struct rte_eth_dev *dev,
				       const struct timespec *timestamp);
/**< @internal Function used to get time from the device clock */

typedef int (*eth_read_clock)(struct rte_eth_dev *dev,
				      uint64_t *timestamp);
/**< @internal Function used to get the current value of the device clock. */

typedef int (*eth_get_reg_t)(struct rte_eth_dev *dev,
				struct rte_dev_reg_info *info);
/**< @internal Retrieve registers  */

typedef int (*eth_get_eeprom_length_t)(struct rte_eth_dev *dev);
/**< @internal Retrieve eeprom size  */

typedef int (*eth_get_eeprom_t)(struct rte_eth_dev *dev,
				struct rte_dev_eeprom_info *info);
/**< @internal Retrieve eeprom data  */

typedef int (*eth_set_eeprom_t)(struct rte_eth_dev *dev,
				struct rte_dev_eeprom_info *info);
/**< @internal Program eeprom data  */

typedef int (*eth_get_module_info_t)(struct rte_eth_dev *dev,
				     struct rte_eth_dev_module_info *modinfo);
/**< @internal Retrieve type and size of plugin module eeprom */

typedef int (*eth_get_module_eeprom_t)(struct rte_eth_dev *dev,
				       struct rte_dev_eeprom_info *info);
/**< @internal Retrieve plugin module eeprom data */

/**
 * Feature filter types
 */
enum rte_filter_type {
	RTE_ETH_FILTER_NONE = 0,
	RTE_ETH_FILTER_ETHERTYPE,
	RTE_ETH_FILTER_FLEXIBLE,
	RTE_ETH_FILTER_SYN,
	RTE_ETH_FILTER_NTUPLE,
	RTE_ETH_FILTER_TUNNEL,
	RTE_ETH_FILTER_FDIR,
	RTE_ETH_FILTER_HASH,
	RTE_ETH_FILTER_L2_TUNNEL,
	RTE_ETH_FILTER_GENERIC,
};

/**
 * Generic operations on filters
 */
enum rte_filter_op {
	RTE_ETH_FILTER_GET,      /**< get flow API ops */
};

typedef int (*eth_filter_ctrl_t)(struct rte_eth_dev *dev,
				 enum rte_filter_type filter_type,
				 enum rte_filter_op filter_op,
				 void *arg);
/**< @internal Take operations to assigned filter type on an Ethernet device */

typedef int (*eth_tm_ops_get_t)(struct rte_eth_dev *dev, void *ops);
/**< @internal Get Traffic Management (TM) operations on an Ethernet device */

typedef int (*eth_mtr_ops_get_t)(struct rte_eth_dev *dev, void *ops);
/**< @internal Get Traffic Metering and Policing (MTR) operations */

typedef int (*eth_get_dcb_info)(struct rte_eth_dev *dev,
				 struct rte_eth_dcb_info *dcb_info);
/**< @internal Get dcb information on an Ethernet device */

typedef int (*eth_pool_ops_supported_t)(struct rte_eth_dev *dev,
						const char *pool);
/**< @internal Test if a port supports specific mempool ops */

/**
 * @internal
 * Get the hairpin capabilities.
 *
 * @param dev
 *   ethdev handle of port.
 * @param cap
 *   returns the hairpin capabilities from the device.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, hairpin is supported.
 * @retval -ENOTSUP
 *   Hairpin is not supported.
 */
typedef int (*eth_hairpin_cap_get_t)(struct rte_eth_dev *dev,
				     struct rte_eth_hairpin_cap *cap);

/**
 * @internal
 * Setup RX hairpin queue.
 *
 * @param dev
 *   ethdev handle of port.
 * @param rx_queue_id
 *   the selected RX queue index.
 * @param nb_rx_desc
 *   the requested number of descriptors for this queue. 0 - use PMD default.
 * @param conf
 *   the RX hairpin configuration structure.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, hairpin is supported.
 * @retval -ENOTSUP
 *   Hairpin is not supported.
 * @retval -EINVAL
 *   One of the parameters is invalid.
 * @retval -ENOMEM
 *   Unable to allocate resources.
 */
typedef int (*eth_rx_hairpin_queue_setup_t)
	(struct rte_eth_dev *dev, uint16_t rx_queue_id,
	 uint16_t nb_rx_desc,
	 const struct rte_eth_hairpin_conf *conf);

/**
 * @internal
 * Setup TX hairpin queue.
 *
 * @param dev
 *   ethdev handle of port.
 * @param tx_queue_id
 *   the selected TX queue index.
 * @param nb_tx_desc
 *   the requested number of descriptors for this queue. 0 - use PMD default.
 * @param conf
 *   the TX hairpin configuration structure.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, hairpin is supported.
 * @retval -ENOTSUP
 *   Hairpin is not supported.
 * @retval -EINVAL
 *   One of the parameters is invalid.
 * @retval -ENOMEM
 *   Unable to allocate resources.
 */
typedef int (*eth_tx_hairpin_queue_setup_t)
	(struct rte_eth_dev *dev, uint16_t tx_queue_id,
	 uint16_t nb_tx_desc,
	 const struct rte_eth_hairpin_conf *hairpin_conf);

/**
 * @internal
 * Get Forward Error Correction(FEC) capability.
 *
 * @param dev
 *   ethdev handle of port.
 * @param speed_fec_capa
 *   speed_fec_capa is out only with per-speed capabilities.
 * @param num
 *   a number of elements in an speed_fec_capa array.
 *
 * @return
 *   Negative errno value on error, positive value on success.
 *
 * @retval positive value
 *   A non-negative value lower or equal to num: success. The return value
 *   is the number of entries filled in the fec capa array.
 *   A non-negative value higher than num: error, the given fec capa array
 *   is too small. The return value corresponds to the num that should
 *   be given to succeed. The entries in the fec capa array are not valid
 *   and shall not be used by the caller.
 * @retval -ENOTSUP
 *   Operation is not supported.
 * @retval -EIO
 *   Device is removed.
 * @retval -EINVAL
 *   *num* or *speed_fec_capa* invalid.
 */
typedef int (*eth_fec_get_capability_t)(struct rte_eth_dev *dev,
		struct rte_eth_fec_capa *speed_fec_capa, unsigned int num);

/**
 * @internal
 * Get Forward Error Correction(FEC) mode.
 *
 * @param dev
 *   ethdev handle of port.
 * @param fec_capa
 *   a bitmask of enabled FEC modes. If AUTO bit is set, other
 *   bits specify FEC modes which may be negotiated. If AUTO
 *   bit is clear, specify FEC modes to be used (only one valid
 *   mode per speed may be set).
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, get FEC success.
 * @retval -ENOTSUP
 *   Operation is not supported.
 * @retval -EIO
 *   Device is removed.
 */
typedef int (*eth_fec_get_t)(struct rte_eth_dev *dev,
			     uint32_t *fec_capa);

/**
 * @internal
 * Set Forward Error Correction(FEC) mode.
 *
 * @param dev
 *   ethdev handle of port.
 * @param fec_capa
 *   bitmask of allowed FEC modes. It must be only one
 *   if AUTO is disabled. If AUTO is enabled, other
 *   bits specify FEC modes which may be negotiated.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, set FEC success.
 * @retval -ENOTSUP
 *   Operation is not supported.
 * @retval -EINVAL
 *   Unsupported FEC mode requested.
 * @retval -EIO
 *   Device is removed.
 */
typedef int (*eth_fec_set_t)(struct rte_eth_dev *dev, uint32_t fec_capa);

/**
 * @internal
 * Get all hairpin Tx/Rx peer ports of the current device, if any.
 *
 * @param dev
 *   ethdev handle of port.
 * @param peer_ports
 *   array to save the ports list.
 * @param len
 *   array length.
 * @param direction
 *   value to decide the current to peer direction
 *   positive - used as Tx to get all peer Rx ports.
 *   zero - used as Rx to get all peer Tx ports.
 *
 * @return
 *   Negative errno value on error, 0 or positive on success.
 *
 * @retval 0
 *   Success, no peer ports.
 * @retval >0
 *   Actual number of the peer ports.
 * @retval -ENOTSUP
 *   Get peer ports API is not supported.
 * @retval -EINVAL
 *   One of the parameters is invalid.
 */
typedef int (*hairpin_get_peer_ports_t)(struct rte_eth_dev *dev,
					uint16_t *peer_ports, size_t len,
					uint32_t direction);

/**
 * @internal
 * Bind all hairpin Tx queues of one port to the Rx queues of the peer port.
 *
 * @param dev
 *   ethdev handle of port.
 * @param rx_port
 *   the peer Rx port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, bind successfully.
 * @retval -ENOTSUP
 *   Bind API is not supported.
 * @retval -EINVAL
 *   One of the parameters is invalid.
 * @retval -EBUSY
 *   Device is not started.
 */
typedef int (*eth_hairpin_bind_t)(struct rte_eth_dev *dev,
				uint16_t rx_port);

/**
 * @internal
 * Unbind all hairpin Tx queues of one port from the Rx queues of the peer port.
 *
 * @param dev
 *   ethdev handle of port.
 * @param rx_port
 *   the peer Rx port.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 *
 * @retval 0
 *   Success, unbind successfully.
 * @retval -ENOTSUP
 *   Bind API is not supported.
 * @retval -EINVAL
 *   One of the parameters is invalid.
 * @retval -EBUSY
 *   Device is already stopped.
 */
typedef int (*eth_hairpin_unbind_t)(struct rte_eth_dev *dev,
				  uint16_t rx_port);

typedef int (*eth_hairpin_queue_peer_update_t)
	(struct rte_eth_dev *dev, uint16_t peer_queue,
	 struct rte_hairpin_peer_info *current_info,
	 struct rte_hairpin_peer_info *peer_info, uint32_t direction);
/**< @internal Update and fetch peer queue information. */

typedef int (*eth_hairpin_queue_peer_bind_t)
	(struct rte_eth_dev *dev, uint16_t cur_queue,
	 struct rte_hairpin_peer_info *peer_info, uint32_t direction);
/**< @internal Bind peer queue to the current queue with fetched information. */

typedef int (*eth_hairpin_queue_peer_unbind_t)
	(struct rte_eth_dev *dev, uint16_t cur_queue, uint32_t direction);
/**< @internal Unbind peer queue from the current queue. */

/**
 * @internal A structure containing the functions exported by an Ethernet driver.
 */
struct eth_dev_ops {
	eth_dev_configure_t        dev_configure; /**< Configure device. */
	eth_dev_start_t            dev_start;     /**< Start device. */
	eth_dev_stop_t             dev_stop;      /**< Stop device. */
	eth_dev_set_link_up_t      dev_set_link_up;   /**< Device link up. */
	eth_dev_set_link_down_t    dev_set_link_down; /**< Device link down. */
	eth_dev_close_t            dev_close;     /**< Close device. */
	eth_dev_reset_t		   dev_reset;	  /**< Reset device. */
	eth_link_update_t          link_update;   /**< Get device link state. */
	eth_is_removed_t           is_removed;
	/**< Check if the device was physically removed. */

	eth_promiscuous_enable_t   promiscuous_enable; /**< Promiscuous ON. */
	eth_promiscuous_disable_t  promiscuous_disable;/**< Promiscuous OFF. */
	eth_allmulticast_enable_t  allmulticast_enable;/**< RX multicast ON. */
	eth_allmulticast_disable_t allmulticast_disable;/**< RX multicast OFF. */
	eth_mac_addr_remove_t      mac_addr_remove; /**< Remove MAC address. */
	eth_mac_addr_add_t         mac_addr_add;  /**< Add a MAC address. */
	eth_mac_addr_set_t         mac_addr_set;  /**< Set a MAC address. */
	eth_set_mc_addr_list_t     set_mc_addr_list; /**< set list of mcast addrs. */
	mtu_set_t                  mtu_set;       /**< Set MTU. */

	eth_stats_get_t            stats_get;     /**< Get generic device statistics. */
	eth_stats_reset_t          stats_reset;   /**< Reset generic device statistics. */
	eth_xstats_get_t           xstats_get;    /**< Get extended device statistics. */
	eth_xstats_reset_t         xstats_reset;  /**< Reset extended device statistics. */
	eth_xstats_get_names_t     xstats_get_names;
	/**< Get names of extended statistics. */
	eth_queue_stats_mapping_set_t queue_stats_mapping_set;
	/**< Configure per queue stat counter mapping. */

	eth_dev_infos_get_t        dev_infos_get; /**< Get device info. */
	eth_rxq_info_get_t         rxq_info_get; /**< retrieve RX queue information. */
	eth_txq_info_get_t         txq_info_get; /**< retrieve TX queue information. */
	eth_burst_mode_get_t       rx_burst_mode_get; /**< Get RX burst mode */
	eth_burst_mode_get_t       tx_burst_mode_get; /**< Get TX burst mode */
	eth_fw_version_get_t       fw_version_get; /**< Get firmware version. */
	eth_dev_supported_ptypes_get_t dev_supported_ptypes_get;
	/**< Get packet types supported and identified by device. */
	eth_dev_ptypes_set_t dev_ptypes_set;
	/**< Inform Ethernet device about reduced range of packet types to handle. */

	vlan_filter_set_t          vlan_filter_set; /**< Filter VLAN Setup. */
	vlan_tpid_set_t            vlan_tpid_set; /**< Outer/Inner VLAN TPID Setup. */
	vlan_strip_queue_set_t     vlan_strip_queue_set; /**< VLAN Stripping on queue. */
	vlan_offload_set_t         vlan_offload_set; /**< Set VLAN Offload. */
	vlan_pvid_set_t            vlan_pvid_set; /**< Set port based TX VLAN insertion. */

	eth_queue_start_t          rx_queue_start;/**< Start RX for a queue. */
	eth_queue_stop_t           rx_queue_stop; /**< Stop RX for a queue. */
	eth_queue_start_t          tx_queue_start;/**< Start TX for a queue. */
	eth_queue_stop_t           tx_queue_stop; /**< Stop TX for a queue. */
	eth_rx_queue_setup_t       rx_queue_setup;/**< Set up device RX queue. */
	eth_queue_release_t        rx_queue_release; /**< Release RX queue. */

	eth_rx_enable_intr_t       rx_queue_intr_enable;  /**< Enable Rx queue interrupt. */
	eth_rx_disable_intr_t      rx_queue_intr_disable; /**< Disable Rx queue interrupt. */
	eth_tx_queue_setup_t       tx_queue_setup;/**< Set up device TX queue. */
	eth_queue_release_t        tx_queue_release; /**< Release TX queue. */
	eth_tx_done_cleanup_t      tx_done_cleanup;/**< Free tx ring mbufs */

	eth_dev_led_on_t           dev_led_on;    /**< Turn on LED. */
	eth_dev_led_off_t          dev_led_off;   /**< Turn off LED. */

	flow_ctrl_get_t            flow_ctrl_get; /**< Get flow control. */
	flow_ctrl_set_t            flow_ctrl_set; /**< Setup flow control. */
	priority_flow_ctrl_set_t   priority_flow_ctrl_set; /**< Setup priority flow control. */

	eth_uc_hash_table_set_t    uc_hash_table_set; /**< Set Unicast Table Array. */
	eth_uc_all_hash_table_set_t uc_all_hash_table_set; /**< Set Unicast hash bitmap. */

	eth_mirror_rule_set_t	   mirror_rule_set; /**< Add a traffic mirror rule. */
	eth_mirror_rule_reset_t	   mirror_rule_reset; /**< reset a traffic mirror rule. */

	eth_udp_tunnel_port_add_t  udp_tunnel_port_add; /** Add UDP tunnel port. */
	eth_udp_tunnel_port_del_t  udp_tunnel_port_del; /** Del UDP tunnel port. */

	eth_set_queue_rate_limit_t set_queue_rate_limit; /**< Set queue rate limit. */

	rss_hash_update_t          rss_hash_update; /** Configure RSS hash protocols. */
	rss_hash_conf_get_t        rss_hash_conf_get; /** Get current RSS hash configuration. */
	reta_update_t              reta_update;   /** Update redirection table. */
	reta_query_t               reta_query;    /** Query redirection table. */

	eth_get_reg_t              get_reg;           /**< Get registers. */
	eth_get_eeprom_length_t    get_eeprom_length; /**< Get eeprom length. */
	eth_get_eeprom_t           get_eeprom;        /**< Get eeprom data. */
	eth_set_eeprom_t           set_eeprom;        /**< Set eeprom. */

	eth_get_module_info_t      get_module_info;
	/** Get plugin module eeprom attribute. */
	eth_get_module_eeprom_t    get_module_eeprom;
	/** Get plugin module eeprom data. */

	eth_filter_ctrl_t          filter_ctrl; /**< common filter control. */

	eth_get_dcb_info           get_dcb_info; /** Get DCB information. */

	eth_timesync_enable_t      timesync_enable;
	/** Turn IEEE1588/802.1AS timestamping on. */
	eth_timesync_disable_t     timesync_disable;
	/** Turn IEEE1588/802.1AS timestamping off. */
	eth_timesync_read_rx_timestamp_t timesync_read_rx_timestamp;
	/** Read the IEEE1588/802.1AS RX timestamp. */
	eth_timesync_read_tx_timestamp_t timesync_read_tx_timestamp;
	/** Read the IEEE1588/802.1AS TX timestamp. */
	eth_timesync_adjust_time   timesync_adjust_time; /** Adjust the device clock. */
	eth_timesync_read_time     timesync_read_time; /** Get the device clock time. */
	eth_timesync_write_time    timesync_write_time; /** Set the device clock time. */

	eth_read_clock             read_clock;

	eth_xstats_get_by_id_t     xstats_get_by_id;
	/**< Get extended device statistic values by ID. */
	eth_xstats_get_names_by_id_t xstats_get_names_by_id;
	/**< Get name of extended device statistics by ID. */

	eth_tm_ops_get_t tm_ops_get;
	/**< Get Traffic Management (TM) operations. */

	eth_mtr_ops_get_t mtr_ops_get;
	/**< Get Traffic Metering and Policing (MTR) operations. */

	eth_pool_ops_supported_t pool_ops_supported;
	/**< Test if a port supports specific mempool ops */

	eth_hairpin_cap_get_t hairpin_cap_get;
	/**< Returns the hairpin capabilities. */
	eth_rx_hairpin_queue_setup_t rx_hairpin_queue_setup;
	/**< Set up device RX hairpin queue. */
	eth_tx_hairpin_queue_setup_t tx_hairpin_queue_setup;
	/**< Set up device TX hairpin queue. */

	eth_fec_get_capability_t fec_get_capability;
	/**< Get Forward Error Correction(FEC) capability. */
	eth_fec_get_t fec_get;
	/**< Get Forward Error Correction(FEC) mode. */
	eth_fec_set_t fec_set;
	/**< Set Forward Error Correction(FEC) mode. */
	hairpin_get_peer_ports_t hairpin_get_peer_ports;
	/**< Get hairpin peer ports list. */
	eth_hairpin_bind_t hairpin_bind;
	/**< Bind all hairpin Tx queues of device to the peer port Rx queues. */
	eth_hairpin_unbind_t hairpin_unbind;
	/**< Unbind all hairpin Tx queues from the peer port Rx queues. */
	eth_hairpin_queue_peer_update_t hairpin_queue_peer_update;
	/**< Pass the current queue info and get the peer queue info. */
	eth_hairpin_queue_peer_bind_t hairpin_queue_peer_bind;
	/**< Set up the connection between the pair of hairpin queues. */
	eth_hairpin_queue_peer_unbind_t hairpin_queue_peer_unbind;
	/**< Disconnect the hairpin queues of a pair from each other. */
};

/**
 * RX/TX queue states
 */
#define RTE_ETH_QUEUE_STATE_STOPPED 0
#define RTE_ETH_QUEUE_STATE_STARTED 1
#define RTE_ETH_QUEUE_STATE_HAIRPIN 2

/**
 * @internal
 * Check if the selected Rx queue is hairpin queue.
 *
 * @param dev
 *  Pointer to the selected device.
 * @param queue_id
 *  The selected queue.
 *
 * @return
 *   - (1) if the queue is hairpin queue, 0 otherwise.
 */
__rte_internal
int rte_eth_dev_is_rx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id);

/**
 * @internal
 * Check if the selected Tx queue is hairpin queue.
 *
 * @param dev
 *  Pointer to the selected device.
 * @param queue_id
 *  The selected queue.
 *
 * @return
 *   - (1) if the queue is hairpin queue, 0 otherwise.
 */
__rte_internal
int rte_eth_dev_is_tx_hairpin_queue(struct rte_eth_dev *dev, uint16_t queue_id);

/**
 * @internal
 * Returns a ethdev slot specified by the unique identifier name.
 *
 * @param	name
 *  The pointer to the Unique identifier name for each Ethernet device
 * @return
 *   - The pointer to the ethdev slot, on success. NULL on error
 */
__rte_internal
struct rte_eth_dev *rte_eth_dev_allocated(const char *name);

/**
 * @internal
 * Allocates a new ethdev slot for an ethernet device and returns the pointer
 * to that slot for the driver to use.
 *
 * @param	name	Unique identifier name for each Ethernet device
 * @return
 *   - Slot in the rte_dev_devices array for a new device;
 */
__rte_internal
struct rte_eth_dev *rte_eth_dev_allocate(const char *name);

/**
 * @internal
 * Attach to the ethdev already initialized by the primary
 * process.
 *
 * @param       name    Ethernet device's name.
 * @return
 *   - Success: Slot in the rte_dev_devices array for attached
 *        device.
 *   - Error: Null pointer.
 */
__rte_internal
struct rte_eth_dev *rte_eth_dev_attach_secondary(const char *name);

/**
 * @internal
 * Notify RTE_ETH_EVENT_DESTROY and release the specified ethdev port.
 *
 * The following PMD-managed data fields will be freed:
 *   - dev_private
 *   - mac_addrs
 *   - hash_mac_addrs
 * If one of these fields should not be freed,
 * it must be reset to NULL by the PMD, typically in dev_close method.
 *
 * @param eth_dev
 * Device to be detached.
 * @return
 *   - 0 on success, negative on error
 */
__rte_internal
int rte_eth_dev_release_port(struct rte_eth_dev *eth_dev);

/**
 * @internal
 * Release device queues and clear its configuration to force the user
 * application to reconfigure it. It is for internal use only.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 *
 * @return
 *  void
 */
__rte_internal
void rte_eth_dev_internal_reset(struct rte_eth_dev *dev);

/**
 * @internal Executes all the user application registered callbacks for
 * the specific device. It is for DPDK internal user only. User
 * application should not call it directly.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param event
 *  Eth device interrupt event type.
 * @param ret_param
 *  To pass data back to user application.
 *  This allows the user application to decide if a particular function
 *  is permitted or not.
 *
 * @return
 *  int
 */
__rte_internal
int rte_eth_dev_callback_process(struct rte_eth_dev *dev,
		enum rte_eth_event_type event, void *ret_param);

/**
 * @internal
 * This is the last step of device probing.
 * It must be called after a port is allocated and initialized successfully.
 *
 * The notification RTE_ETH_EVENT_NEW is sent to other entities
 * (libraries and applications).
 * The state is set as RTE_ETH_DEV_ATTACHED.
 *
 * @param dev
 *  New ethdev port.
 */
__rte_internal
void rte_eth_dev_probing_finish(struct rte_eth_dev *dev);

/**
 * Create memzone for HW rings.
 * malloc can't be used as the physical address is needed.
 * If the memzone is already created, then this function returns a ptr
 * to the old one.
 *
 * @param eth_dev
 *   The *eth_dev* pointer is the address of the *rte_eth_dev* structure
 * @param name
 *   The name of the memory zone
 * @param queue_id
 *   The index of the queue to add to name
 * @param size
 *   The sizeof of the memory area
 * @param align
 *   Alignment for resulting memzone. Must be a power of 2.
 * @param socket_id
 *   The *socket_id* argument is the socket identifier in case of NUMA.
 */
__rte_internal
const struct rte_memzone *
rte_eth_dma_zone_reserve(const struct rte_eth_dev *eth_dev, const char *name,
			 uint16_t queue_id, size_t size,
			 unsigned align, int socket_id);

/**
 * Free previously allocated memzone for HW rings.
 *
 * @param eth_dev
 *   The *eth_dev* pointer is the address of the *rte_eth_dev* structure
 * @param name
 *   The name of the memory zone
 * @param queue_id
 *   The index of the queue to add to name
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_dma_zone_free(const struct rte_eth_dev *eth_dev, const char *name,
		 uint16_t queue_id);

/**
 * @internal
 * Atomically set the link status for the specific device.
 * It is for use by DPDK device driver use only.
 * User applications should not call it
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param link
 *  New link status value.
 * @return
 *  Same convention as eth_link_update operation.
 *  0   if link up status has changed
 *  -1  if link up status was unchanged
 */
static inline int
rte_eth_linkstatus_set(struct rte_eth_dev *dev,
		       const struct rte_eth_link *new_link)
{
	uint64_t *dev_link = (uint64_t *)&(dev->data->dev_link);
	union {
		uint64_t val64;
		struct rte_eth_link link;
	} orig;

	RTE_BUILD_BUG_ON(sizeof(*new_link) != sizeof(uint64_t));

	orig.val64 = __atomic_exchange_n(dev_link, *(const uint64_t *)new_link,
					__ATOMIC_SEQ_CST);

	return (orig.link.link_status == new_link->link_status) ? -1 : 0;
}

/**
 * @internal
 * Atomically get the link speed and status.
 *
 * @param dev
 *  Pointer to struct rte_eth_dev.
 * @param link
 *  link status value.
 */
static inline void
rte_eth_linkstatus_get(const struct rte_eth_dev *dev,
		       struct rte_eth_link *link)
{
	uint64_t *src = (uint64_t *)&(dev->data->dev_link);
	uint64_t *dst = (uint64_t *)link;

	RTE_BUILD_BUG_ON(sizeof(*link) != sizeof(uint64_t));

	*dst = __atomic_load_n(src, __ATOMIC_SEQ_CST);
}

/**
 * Allocate an unique switch domain identifier.
 *
 * A pool of switch domain identifiers which can be allocated on request. This
 * will enabled devices which support the concept of switch domains to request
 * a switch domain id which is guaranteed to be unique from other devices
 * running in the same process.
 *
 * @param domain_id
 *  switch domain identifier parameter to pass back to application
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_switch_domain_alloc(uint16_t *domain_id);

/**
 * Free switch domain.
 *
 * Return a switch domain identifier to the pool of free identifiers after it is
 * no longer in use by device.
 *
 * @param domain_id
 *  switch domain identifier to free
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_switch_domain_free(uint16_t domain_id);

/** Generic Ethernet device arguments  */
struct rte_eth_devargs {
	uint16_t ports[RTE_MAX_ETHPORTS];
	/** port/s number to enable on a multi-port single function */
	uint16_t nb_ports;
	/** number of ports in ports field */
	uint16_t representor_ports[RTE_MAX_ETHPORTS];
	/** representor port/s identifier to enable on device */
	uint16_t nb_representor_ports;
	/** number of ports in representor port field */
};

/**
 * PMD helper function to parse ethdev arguments
 *
 * @param devargs
 *  device arguments
 * @param eth_devargs
 *  parsed ethdev specific arguments.
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_devargs_parse(const char *devargs, struct rte_eth_devargs *eth_devargs);


typedef int (*ethdev_init_t)(struct rte_eth_dev *ethdev, void *init_params);
typedef int (*ethdev_bus_specific_init)(struct rte_eth_dev *ethdev,
	void *bus_specific_init_params);

/**
 * PMD helper function for the creation of a new ethdev ports.
 *
 * @param device
 *  rte_device handle.
 * @param name
 *  port name.
 * @param priv_data_size
 *  size of private data required for port.
 * @param bus_specific_init
 *  port bus specific initialisation callback function
 * @param bus_init_params
 *  port bus specific initialisation parameters
 * @param ethdev_init
 *  device specific port initialization callback function
 * @param init_params
 *  port initialisation parameters
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_dev_create(struct rte_device *device, const char *name,
	size_t priv_data_size,
	ethdev_bus_specific_init bus_specific_init, void *bus_init_params,
	ethdev_init_t ethdev_init, void *init_params);


typedef int (*ethdev_uninit_t)(struct rte_eth_dev *ethdev);

/**
 * PMD helper function for cleaning up the resources of a ethdev port on it's
 * destruction.
 *
 * @param ethdev
 *   ethdev handle of port.
 * @param ethdev_uninit
 *   device specific port un-initialise callback function
 *
 * @return
 *   Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_dev_destroy(struct rte_eth_dev *ethdev, ethdev_uninit_t ethdev_uninit);

/**
 * @internal
 * Pass the current hairpin queue HW and/or SW information to the peer queue
 * and fetch back the information of the peer queue.
 *
 * @param peer_port
 *  Peer port identifier of the Ethernet device.
 * @param peer_queue
 *  Peer queue index of the port.
 * @param cur_info
 *  Pointer to the current information structure.
 * @param peer_info
 *  Pointer to the peer information, output.
 * @param direction
 *  Direction to pass the information.
 *  positive - pass Tx queue information and get peer Rx queue information
 *  zero - pass Rx queue information and get peer Tx queue information
 *
 * @return
 *  Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_hairpin_queue_peer_update(uint16_t peer_port, uint16_t peer_queue,
				  struct rte_hairpin_peer_info *cur_info,
				  struct rte_hairpin_peer_info *peer_info,
				  uint32_t direction);

/**
 * @internal
 * Configure current hairpin queue with the peer information fetched to create
 * the connection (bind) with peer queue in the specified direction.
 * This function might need to be called twice to fully create the connections.
 *
 * @param cur_port
 *  Current port identifier of the Ethernet device.
 * @param cur_queue
 *  Current queue index of the port.
 * @param peer_info
 *  Pointer to the peer information, input.
 * @param direction
 *  Direction to create the connection.
 *  positive - bind current Tx queue to peer Rx queue
 *  zero - bind current Rx queue to peer Tx queue
 *
 * @return
 *  Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_hairpin_queue_peer_bind(uint16_t cur_port, uint16_t cur_queue,
				struct rte_hairpin_peer_info *peer_info,
				uint32_t direction);

/**
 * @internal
 * Reset the current queue state and configuration to disconnect (unbind) it
 * from the peer queue.
 * This function might need to be called twice to disconnect each other.
 *
 * @param cur_port
 *  Current port identifier of the Ethernet device.
 * @param cur_queue
 *  Current queue index of the port.
 * @param direction
 *  Direction to destroy the connection.
 *  positive - unbind current Tx queue from peer Rx queue
 *  zero - unbind current Rx queue from peer Tx queue
 *
 * @return
 *  Negative errno value on error, 0 on success.
 */
__rte_internal
int
rte_eth_hairpin_queue_peer_unbind(uint16_t cur_port, uint16_t cur_queue,
				  uint32_t direction);


/*
 * Legacy ethdev API used internally by drivers.
 */

/**
 * Define all structures for Ethertype Filter type.
 */

#define RTE_ETHTYPE_FLAGS_MAC    0x0001 /**< If set, compare mac */
#define RTE_ETHTYPE_FLAGS_DROP   0x0002 /**< If set, drop packet when match */

/**
 * A structure used to define the ethertype filter entry
 * to support RTE_ETH_FILTER_ETHERTYPE data representation.
 */
struct rte_eth_ethertype_filter {
	struct rte_ether_addr mac_addr;   /**< Mac address to match. */
	uint16_t ether_type;          /**< Ether type to match */
	uint16_t flags;               /**< Flags from RTE_ETHTYPE_FLAGS_* */
	uint16_t queue;               /**< Queue assigned to when match*/
};

/**
 * A structure used to define the TCP syn filter entry
 * to support RTE_ETH_FILTER_SYN data representation.
 */
struct rte_eth_syn_filter {
	/** 1 - higher priority than other filters, 0 - lower priority. */
	uint8_t hig_pri;
	uint16_t queue;      /**< Queue assigned to when match */
};

/**
 * filter type of tunneling packet
 */
#define ETH_TUNNEL_FILTER_OMAC  0x01 /**< filter by outer MAC addr */
#define ETH_TUNNEL_FILTER_OIP   0x02 /**< filter by outer IP Addr */
#define ETH_TUNNEL_FILTER_TENID 0x04 /**< filter by tenant ID */
#define ETH_TUNNEL_FILTER_IMAC  0x08 /**< filter by inner MAC addr */
#define ETH_TUNNEL_FILTER_IVLAN 0x10 /**< filter by inner VLAN ID */
#define ETH_TUNNEL_FILTER_IIP   0x20 /**< filter by inner IP addr */

#define RTE_TUNNEL_FILTER_IMAC_IVLAN (ETH_TUNNEL_FILTER_IMAC | \
					ETH_TUNNEL_FILTER_IVLAN)
#define RTE_TUNNEL_FILTER_IMAC_IVLAN_TENID (ETH_TUNNEL_FILTER_IMAC | \
					ETH_TUNNEL_FILTER_IVLAN | \
					ETH_TUNNEL_FILTER_TENID)
#define RTE_TUNNEL_FILTER_IMAC_TENID (ETH_TUNNEL_FILTER_IMAC | \
					ETH_TUNNEL_FILTER_TENID)
#define RTE_TUNNEL_FILTER_OMAC_TENID_IMAC (ETH_TUNNEL_FILTER_OMAC | \
					ETH_TUNNEL_FILTER_TENID | \
					ETH_TUNNEL_FILTER_IMAC)

/**
 *  Select IPv4 or IPv6 for tunnel filters.
 */
enum rte_tunnel_iptype {
	RTE_TUNNEL_IPTYPE_IPV4 = 0, /**< IPv4. */
	RTE_TUNNEL_IPTYPE_IPV6,     /**< IPv6. */
};

/**
 * Tunneling Packet filter configuration.
 */
struct rte_eth_tunnel_filter_conf {
	struct rte_ether_addr outer_mac;    /**< Outer MAC address to match. */
	struct rte_ether_addr inner_mac;    /**< Inner MAC address to match. */
	uint16_t inner_vlan;            /**< Inner VLAN to match. */
	enum rte_tunnel_iptype ip_type; /**< IP address type. */
	/**
	 * Outer destination IP address to match if ETH_TUNNEL_FILTER_OIP
	 * is set in filter_type, or inner destination IP address to match
	 * if ETH_TUNNEL_FILTER_IIP is set in filter_type.
	 */
	union {
		uint32_t ipv4_addr;     /**< IPv4 address in big endian. */
		uint32_t ipv6_addr[4];  /**< IPv6 address in big endian. */
	} ip_addr;
	/** Flags from ETH_TUNNEL_FILTER_XX - see above. */
	uint16_t filter_type;
	enum rte_eth_tunnel_type tunnel_type; /**< Tunnel Type. */
	uint32_t tenant_id;     /**< Tenant ID to match. VNI, GRE key... */
	uint16_t queue_id;      /**< Queue assigned to if match. */
};

#ifdef __cplusplus
}
#endif

#endif /* _RTE_ETHDEV_DRIVER_H_ */
