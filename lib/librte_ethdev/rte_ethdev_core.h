/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _RTE_ETHDEV_CORE_H_
#define _RTE_ETHDEV_CORE_H_

/**
 * @file
 *
 * RTE Ethernet Device internal header.
 *
 * This header contains internal data types. But they are still part of the
 * public API because they are used by inline functions in the published API.
 *
 * Applications should not use these directly.
 *
 */

struct rte_eth_dev_callback;
/** @internal Structure to keep track of registered callbacks */
TAILQ_HEAD(rte_eth_dev_cb_list, rte_eth_dev_callback);

/*
 * Definitions of all functions exported by an Ethernet driver through the
 * the generic structure of type *eth_dev_ops* supplied in the *rte_eth_dev*
 * structure associated with an Ethernet device.
 */
struct rte_eth_dev;

typedef int  (*eth_dev_configure_t)(struct rte_eth_dev *dev);
/**< @internal Ethernet device configuration. */

typedef int  (*eth_dev_start_t)(struct rte_eth_dev *dev);
/**< @internal Function used to start a configured Ethernet device. */

typedef void (*eth_dev_stop_t)(struct rte_eth_dev *dev);
/**< @internal Function used to stop a configured Ethernet device. */

typedef int  (*eth_dev_set_link_up_t)(struct rte_eth_dev *dev);
/**< @internal Function used to link up a configured Ethernet device. */

typedef int  (*eth_dev_set_link_down_t)(struct rte_eth_dev *dev);
/**< @internal Function used to link down a configured Ethernet device. */

typedef void (*eth_dev_close_t)(struct rte_eth_dev *dev);
/**< @internal Function used to close a configured Ethernet device. */

typedef int (*eth_dev_reset_t)(struct rte_eth_dev *dev);
/** <@internal Function used to reset a configured Ethernet device. */

typedef int (*eth_is_removed_t)(struct rte_eth_dev *dev);
/**< @internal Function used to detect an Ethernet device removal. */

typedef void (*eth_promiscuous_enable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to enable the RX promiscuous mode of an Ethernet device. */

typedef void (*eth_promiscuous_disable_t)(struct rte_eth_dev *dev);
/**< @internal Function used to disable the RX promiscuous mode of an Ethernet device. */

typedef void (*eth_allmulticast_enable_t)(struct rte_eth_dev *dev);
/**< @internal Enable the receipt of all multicast packets by an Ethernet device. */

typedef void (*eth_allmulticast_disable_t)(struct rte_eth_dev *dev);
/**< @internal Disable the receipt of all multicast packets by an Ethernet device. */

typedef int (*eth_link_update_t)(struct rte_eth_dev *dev,
				int wait_to_complete);
/**< @internal Get link speed, duplex mode and state (up/down) of an Ethernet device. */

typedef int (*eth_stats_get_t)(struct rte_eth_dev *dev,
				struct rte_eth_stats *igb_stats);
/**< @internal Get global I/O statistics of an Ethernet device. */

typedef void (*eth_stats_reset_t)(struct rte_eth_dev *dev);
/**< @internal Reset global I/O statistics of an Ethernet device to 0. */

typedef int (*eth_xstats_get_t)(struct rte_eth_dev *dev,
	struct rte_eth_xstat *stats, unsigned n);
/**< @internal Get extended stats of an Ethernet device. */

typedef int (*eth_xstats_get_by_id_t)(struct rte_eth_dev *dev,
				      const uint64_t *ids,
				      uint64_t *values,
				      unsigned int n);
/**< @internal Get extended stats of an Ethernet device. */

typedef void (*eth_xstats_reset_t)(struct rte_eth_dev *dev);
/**< @internal Reset extended stats of an Ethernet device. */

typedef int (*eth_xstats_get_names_t)(struct rte_eth_dev *dev,
	struct rte_eth_xstat_name *xstats_names, unsigned size);
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

typedef void (*eth_dev_infos_get_t)(struct rte_eth_dev *dev,
				    struct rte_eth_dev_info *dev_info);
/**< @internal Get specific informations of an Ethernet device. */

typedef const uint32_t *(*eth_dev_supported_ptypes_get_t)(struct rte_eth_dev *dev);
/**< @internal Get supported ptypes of an Ethernet device. */

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

typedef uint32_t (*eth_rx_queue_count_t)(struct rte_eth_dev *dev,
					 uint16_t rx_queue_id);
/**< @internal Get number of used descriptors on a receive queue. */

typedef int (*eth_rx_descriptor_done_t)(void *rxq, uint16_t offset);
/**< @internal Check DD bit of specific RX descriptor */

typedef int (*eth_rx_descriptor_status_t)(void *rxq, uint16_t offset);
/**< @internal Check the status of a Rx descriptor */

typedef int (*eth_tx_descriptor_status_t)(void *txq, uint16_t offset);
/**< @internal Check the status of a Tx descriptor */

typedef int (*eth_fw_version_get_t)(struct rte_eth_dev *dev,
				     char *fw_version, size_t fw_size);
/**< @internal Get firmware information of an Ethernet device. */

typedef int (*eth_tx_done_cleanup_t)(void *txq, uint32_t free_cnt);
/**< @internal Force mbufs to be from TX ring. */

typedef void (*eth_rxq_info_get_t)(struct rte_eth_dev *dev,
	uint16_t rx_queue_id, struct rte_eth_rxq_info *qinfo);

typedef void (*eth_txq_info_get_t)(struct rte_eth_dev *dev,
	uint16_t tx_queue_id, struct rte_eth_txq_info *qinfo);

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

typedef uint16_t (*eth_rx_burst_t)(void *rxq,
				   struct rte_mbuf **rx_pkts,
				   uint16_t nb_pkts);
/**< @internal Retrieve input packets from a receive queue of an Ethernet device. */

typedef uint16_t (*eth_tx_burst_t)(void *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);
/**< @internal Send output packets on a transmit queue of an Ethernet device. */

typedef uint16_t (*eth_tx_prep_t)(void *txq,
				   struct rte_mbuf **tx_pkts,
				   uint16_t nb_pkts);
/**< @internal Prepare output packets on a transmit queue of an Ethernet device. */

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
				  struct ether_addr *mac_addr,
				  uint32_t index,
				  uint32_t vmdq);
/**< @internal Set a MAC address into Receive Address Address Register */

typedef int (*eth_mac_addr_set_t)(struct rte_eth_dev *dev,
				  struct ether_addr *mac_addr);
/**< @internal Set a MAC address into Receive Address Address Register */

typedef int (*eth_uc_hash_table_set_t)(struct rte_eth_dev *dev,
				  struct ether_addr *mac_addr,
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
				      struct ether_addr *mc_addr_set,
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

typedef int (*eth_l2_tunnel_eth_type_conf_t)
	(struct rte_eth_dev *dev, struct rte_eth_l2_tunnel_conf *l2_tunnel);
/**< @internal config l2 tunnel ether type */

typedef int (*eth_l2_tunnel_offload_set_t)
	(struct rte_eth_dev *dev,
	 struct rte_eth_l2_tunnel_conf *l2_tunnel,
	 uint32_t mask,
	 uint8_t en);
/**< @internal enable/disable the l2 tunnel offload functions */


typedef int (*eth_filter_ctrl_t)(struct rte_eth_dev *dev,
				 enum rte_filter_type filter_type,
				 enum rte_filter_op filter_op,
				 void *arg);
/**< @internal Take operations to assigned filter type on an Ethernet device */

typedef int (*eth_tm_ops_get_t)(struct rte_eth_dev *dev, void *ops);
/**< @internal Get Traffic Management (TM) operations on an Ethernet device */

typedef int (*eth_mtr_ops_get_t)(struct rte_eth_dev *dev, void *ops);
/**< @internal Get Trafffic Metering and Policing (MTR) operations */

typedef int (*eth_get_dcb_info)(struct rte_eth_dev *dev,
				 struct rte_eth_dcb_info *dcb_info);
/**< @internal Get dcb information on an Ethernet device */

typedef int (*eth_pool_ops_supported_t)(struct rte_eth_dev *dev,
						const char *pool);
/**< @internal Test if a port supports specific mempool ops */

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
	eth_fw_version_get_t       fw_version_get; /**< Get firmware version. */
	eth_dev_supported_ptypes_get_t dev_supported_ptypes_get;
	/**< Get packet types supported and identified by device. */

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
	eth_rx_queue_count_t       rx_queue_count;
	/**< Get the number of used RX descriptors. */
	eth_rx_descriptor_done_t   rx_descriptor_done; /**< Check rxd DD bit. */
	eth_rx_descriptor_status_t rx_descriptor_status;
	/**< Check the status of a Rx descriptor. */
	eth_tx_descriptor_status_t tx_descriptor_status;
	/**< Check the status of a Tx descriptor. */
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
	eth_l2_tunnel_eth_type_conf_t l2_tunnel_eth_type_conf;
	/** Config ether type of l2 tunnel. */
	eth_l2_tunnel_offload_set_t   l2_tunnel_offload_set;
	/** Enable/disable l2 tunnel offload functions. */

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
};

/**
 * @internal
 * Structure used to hold information about the callbacks to be called for a
 * queue on RX and TX.
 */
struct rte_eth_rxtx_callback {
	struct rte_eth_rxtx_callback *next;
	union{
		rte_rx_callback_fn rx;
		rte_tx_callback_fn tx;
	} fn;
	void *param;
};

/**
 * @internal
 * The generic data structure associated with each ethernet device.
 *
 * Pointers to burst-oriented packet receive and transmit functions are
 * located at the beginning of the structure, along with the pointer to
 * where all the data elements for the particular device are stored in shared
 * memory. This split allows the function pointer and driver data to be per-
 * process, while the actual configuration data for the device is shared.
 */
struct rte_eth_dev {
	eth_rx_burst_t rx_pkt_burst; /**< Pointer to PMD receive function. */
	eth_tx_burst_t tx_pkt_burst; /**< Pointer to PMD transmit function. */
	eth_tx_prep_t tx_pkt_prepare; /**< Pointer to PMD transmit prepare function. */
	struct rte_eth_dev_data *data;  /**< Pointer to device data */
	const struct eth_dev_ops *dev_ops; /**< Functions exported by PMD */
	struct rte_device *device; /**< Backing device */
	struct rte_intr_handle *intr_handle; /**< Device interrupt handle */
	/** User application callbacks for NIC interrupts */
	struct rte_eth_dev_cb_list link_intr_cbs;
	/**
	 * User-supplied functions called from rx_burst to post-process
	 * received packets before passing them to the user
	 */
	struct rte_eth_rxtx_callback *post_rx_burst_cbs[RTE_MAX_QUEUES_PER_PORT];
	/**
	 * User-supplied functions called from tx_burst to pre-process
	 * received packets before passing them to the driver for transmission.
	 */
	struct rte_eth_rxtx_callback *pre_tx_burst_cbs[RTE_MAX_QUEUES_PER_PORT];
	enum rte_eth_dev_state state; /**< Flag indicating the port state */
	void *security_ctx; /**< Context for security ops */
} __rte_cache_aligned;

struct rte_eth_dev_sriov;
struct rte_eth_dev_owner;

/**
 * @internal
 * The data part, with no function pointers, associated with each ethernet device.
 *
 * This structure is safe to place in shared memory to be common among different
 * processes in a multi-process configuration.
 */
struct rte_eth_dev_data {
	char name[RTE_ETH_NAME_MAX_LEN]; /**< Unique identifier name */

	void **rx_queues; /**< Array of pointers to RX queues. */
	void **tx_queues; /**< Array of pointers to TX queues. */
	uint16_t nb_rx_queues; /**< Number of RX queues. */
	uint16_t nb_tx_queues; /**< Number of TX queues. */

	struct rte_eth_dev_sriov sriov;    /**< SRIOV data */

	void *dev_private;              /**< PMD-specific private data */

	struct rte_eth_link dev_link;
	/**< Link-level information & status */

	struct rte_eth_conf dev_conf;   /**< Configuration applied to device. */
	uint16_t mtu;                   /**< Maximum Transmission Unit. */

	uint32_t min_rx_buf_size;
	/**< Common rx buffer size handled by all queues */

	uint64_t rx_mbuf_alloc_failed; /**< RX ring mbuf allocation failures. */
	struct ether_addr* mac_addrs;/**< Device Ethernet Link address. */
	uint64_t mac_pool_sel[ETH_NUM_RECEIVE_MAC_ADDR];
	/** bitmap array of associating Ethernet MAC addresses to pools */
	struct ether_addr* hash_mac_addrs;
	/** Device Ethernet MAC addresses of hash filtering. */
	uint16_t port_id;           /**< Device [external] port identifier. */
	__extension__
	uint8_t promiscuous   : 1, /**< RX promiscuous mode ON(1) / OFF(0). */
		scattered_rx : 1,  /**< RX of scattered packets is ON(1) / OFF(0) */
		all_multicast : 1, /**< RX all multicast mode ON(1) / OFF(0). */
		dev_started : 1,   /**< Device state: STARTED(1) / STOPPED(0). */
		lro         : 1;   /**< RX LRO is ON(1) / OFF(0) */
	uint8_t rx_queue_state[RTE_MAX_QUEUES_PER_PORT];
	/** Queues state: STARTED(1) / STOPPED(0) */
	uint8_t tx_queue_state[RTE_MAX_QUEUES_PER_PORT];
	/** Queues state: STARTED(1) / STOPPED(0) */
	uint32_t dev_flags; /**< Capabilities */
	enum rte_kernel_driver kdrv;    /**< Kernel driver passthrough */
	int numa_node;  /**< NUMA node connection */
	struct rte_vlan_filter_conf vlan_filter_conf;
	/**< VLAN filter configuration. */
	struct rte_eth_dev_owner owner; /**< The port owner. */
} __rte_cache_aligned;

/**
 * @internal
 * The pool of *rte_eth_dev* structures. The size of the pool
 * is configured at compile-time in the <rte_ethdev.c> file.
 */
extern struct rte_eth_dev rte_eth_devices[];

#endif /* _RTE_ETHDEV_CORE_H_ */
