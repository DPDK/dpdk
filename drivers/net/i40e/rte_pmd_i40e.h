/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _PMD_I40E_H_
#define _PMD_I40E_H_

/**
 * @file rte_pmd_i40e.h
 *
 * i40e PMD specific functions.
 *
 * @b EXPERIMENTAL: this API may change, or be removed, without prior notice
 *
 */

#include <rte_compat.h>
#include <rte_ethdev.h>
#include <rte_ether.h>

/**
 * Response sent back to i40e driver from user app after callback
 */
enum rte_pmd_i40e_mb_event_rsp {
	RTE_PMD_I40E_MB_EVENT_NOOP_ACK,  /**< skip mbox request and ACK */
	RTE_PMD_I40E_MB_EVENT_NOOP_NACK, /**< skip mbox request and NACK */
	RTE_PMD_I40E_MB_EVENT_PROCEED,  /**< proceed with mbox request  */
	RTE_PMD_I40E_MB_EVENT_MAX       /**< max value of this enum */
};

/**
 * Data sent to the user application when the callback is executed.
 */
struct rte_pmd_i40e_mb_event_param {
	uint16_t vfid;     /**< Virtual Function number */
	uint16_t msg_type; /**< VF to PF message type, see virtchnl_ops */
	uint16_t retval;   /**< return value */
	void *msg;         /**< pointer to message */
	uint16_t msglen;   /**< length of the message */
};

/**
 * Option of package processing.
 */
enum rte_pmd_i40e_package_op {
	RTE_PMD_I40E_PKG_OP_UNDEFINED = 0,
	RTE_PMD_I40E_PKG_OP_WR_ADD,   /**< load package and add to info list */
	RTE_PMD_I40E_PKG_OP_WR_DEL, /**< load package and delete from info list */
	RTE_PMD_I40E_PKG_OP_WR_ONLY, /**< load package without modifying info list */
	RTE_PMD_I40E_PKG_OP_MAX = 32
};

/**
 * Types of package information.
 */
enum rte_pmd_i40e_package_info {
	RTE_PMD_I40E_PKG_INFO_UNDEFINED = 0,
	RTE_PMD_I40E_PKG_INFO_GLOBAL_HEADER,
	RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES_SIZE,
	RTE_PMD_I40E_PKG_INFO_GLOBAL_NOTES,
	RTE_PMD_I40E_PKG_INFO_GLOBAL_MAX = 1024,
	RTE_PMD_I40E_PKG_INFO_HEADER,
	RTE_PMD_I40E_PKG_INFO_DEVID_NUM,
	RTE_PMD_I40E_PKG_INFO_DEVID_LIST,
	RTE_PMD_I40E_PKG_INFO_PROTOCOL_NUM,
	RTE_PMD_I40E_PKG_INFO_PROTOCOL_LIST,
	RTE_PMD_I40E_PKG_INFO_PCTYPE_NUM,
	RTE_PMD_I40E_PKG_INFO_PCTYPE_LIST,
	RTE_PMD_I40E_PKG_INFO_PTYPE_NUM,
	RTE_PMD_I40E_PKG_INFO_PTYPE_LIST,
	RTE_PMD_I40E_PKG_INFO_MAX = (int)0xFFFFFFFF
};

/**
 *  Option types of queue region.
 */
enum rte_pmd_i40e_queue_region_op {
	RTE_PMD_I40E_RSS_QUEUE_REGION_UNDEFINED,
	/** add queue region set */
	RTE_PMD_I40E_RSS_QUEUE_REGION_SET,
	/** add PF region pctype set */
	RTE_PMD_I40E_RSS_QUEUE_REGION_FLOWTYPE_SET,
	/** add queue region user priority set */
	RTE_PMD_I40E_RSS_QUEUE_REGION_USER_PRIORITY_SET,
	/**
	 * ALL configuration about queue region from up layer
	 * at first will only keep in DPDK software stored in driver,
	 * only after " FLUSH_ON ", it commit all configuration to HW.
	 * Because PMD had to set hardware configuration at a time, so
	 * it will record all up layer command at first.
	 */
	RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_ON,
	/**
	 * "FLUSH_OFF " is just clean all configuration about queue
	 * region just now, and restore all to DPDK i40e driver default
	 * config when start up.
	 */
	RTE_PMD_I40E_RSS_QUEUE_REGION_ALL_FLUSH_OFF,
	RTE_PMD_I40E_RSS_QUEUE_REGION_INFO_GET,
	RTE_PMD_I40E_RSS_QUEUE_REGION_OP_MAX
};

#define RTE_PMD_I40E_DDP_NAME_SIZE     32
#define RTE_PMD_I40E_PCTYPE_MAX        64
#define RTE_PMD_I40E_REGION_MAX_NUM    8
#define RTE_PMD_I40E_MAX_USER_PRIORITY 8

/**
 * Version for dynamic device personalization.
 * Version in "major.minor.update.draft" format.
 */
struct rte_pmd_i40e_ddp_version {
	uint8_t major;
	uint8_t minor;
	uint8_t update;
	uint8_t draft;
};

/**
 * Device ID for dynamic device personalization.
 */
struct rte_pmd_i40e_ddp_device_id {
	uint32_t vendor_dev_id;
	uint32_t sub_vendor_dev_id;
};

/**
 * Profile information in profile info list.
 */
struct rte_pmd_i40e_profile_info {
	uint32_t track_id;
	struct rte_pmd_i40e_ddp_version version;
	uint8_t owner;
	uint8_t reserved[7];
	uint8_t name[RTE_PMD_I40E_DDP_NAME_SIZE];
};

#define RTE_PMD_I40E_DDP_OWNER_UNKNOWN 0xFF

/**
 * Profile information list returned from HW.
 */
struct rte_pmd_i40e_profile_list {
	uint32_t p_count;
	struct rte_pmd_i40e_profile_info p_info[1];
};

#define RTE_PMD_I40E_PROTO_NUM 6
#define RTE_PMD_I40E_PROTO_UNUSED 0xFF

/**
 * Protocols information stored in profile
 */
struct rte_pmd_i40e_proto_info {
	uint8_t proto_id;
	char name[RTE_PMD_I40E_DDP_NAME_SIZE];
};

/**
 * Packet classification/ packet type information stored in profile
 */
struct rte_pmd_i40e_ptype_info {
	uint8_t ptype_id;
	uint8_t protocols[RTE_PMD_I40E_PROTO_NUM];
};

/**
 * ptype mapping table only accept RTE_PTYPE_XXX or "user defined" ptype.
 * A ptype with MSB set will be regarded as a user defined ptype.
 * Below macro help to create a user defined ptype.
 */
#define RTE_PMD_I40E_PTYPE_USER_DEFINE_MASK 0x80000000

struct rte_pmd_i40e_ptype_mapping {
	uint16_t hw_ptype; /**< hardware defined packet type*/
	uint32_t sw_ptype; /**< software defined packet type */
};

/**
 * Queue region related information.
 */
struct rte_pmd_i40e_queue_region_conf {
	/** the region id for this configuration */
	uint8_t region_id;
	/** the pctype or hardware flowtype of packet,
	 * the specific index for each type has been defined
	 * in file i40e_type.h as enum i40e_filter_pctype.
	 */
	uint8_t hw_flowtype;
	/** the start queue index for this region */
	uint8_t queue_start_index;
	/** the total queue number of this queue region */
	uint8_t queue_num;
	/** the packet's user priority for this region */
	uint8_t user_priority;
};

/* queue region info */
struct rte_pmd_i40e_queue_region_info {
	/** the region id for this configuration */
	uint8_t region_id;
	/** the start queue index for this region */
	uint8_t queue_start_index;
	/** the total queue number of this queue region */
	uint8_t queue_num;
	/** the total number of user priority for this region */
	uint8_t user_priority_num;
	/** the packet's user priority for this region */
	uint8_t user_priority[RTE_PMD_I40E_MAX_USER_PRIORITY];
	/** the total number of flowtype for this region */
	uint8_t flowtype_num;
	/**
	 * the pctype or hardware flowtype of packet,
	 * the specific index for each type has been defined
	 * in file i40e_type.h as enum i40e_filter_pctype.
	 */
	uint8_t hw_flowtype[RTE_PMD_I40E_PCTYPE_MAX];
};

struct rte_pmd_i40e_queue_regions {
	/** the total number of queue region for this port */
	uint16_t queue_region_number;
	struct rte_pmd_i40e_queue_region_info
		region[RTE_PMD_I40E_REGION_MAX_NUM];
};

/**
 * Behavior will be taken if raw packet template is matched.
 */
enum rte_pmd_i40e_pkt_template_behavior {
	RTE_PMD_I40E_PKT_TEMPLATE_ACCEPT,
	RTE_PMD_I40E_PKT_TEMPLATE_REJECT,
	RTE_PMD_I40E_PKT_TEMPLATE_PASSTHRU,
};

/**
 * Flow director report status
 * It defines what will be reported if raw packet template is matched.
 */
enum rte_pmd_i40e_pkt_template_status {
	/** report nothing */
	RTE_PMD_I40E_PKT_TEMPLATE_NO_REPORT_STATUS,
	/** only report FD ID */
	RTE_PMD_I40E_PKT_TEMPLATE_REPORT_ID,
	/** report FD ID and 4 flex bytes */
	RTE_PMD_I40E_PKT_TEMPLATE_REPORT_ID_FLEX_4,
	/** report 8 flex bytes */
	RTE_PMD_I40E_PKT_TEMPLATE_REPORT_FLEX_8,
};

/**
 * A structure used to define an action when raw packet template is matched.
 */
struct rte_pmd_i40e_pkt_template_action {
	/** queue assigned to if raw packet template match */
	uint16_t rx_queue;
	/** behavior will be taken */
	enum rte_pmd_i40e_pkt_template_behavior behavior;
	/** status report option */
	enum rte_pmd_i40e_pkt_template_status report_status;
	/**
	 * If report_status is RTE_PMD_I40E_PKT_TEMPLATE_REPORT_ID_FLEX_4 or
	 * RTE_PMD_I40E_PKT_TEMPLATE_REPORT_FLEX_8, flex_off specifies
	 * where the reported flex bytes start from in flexible payload.
	 */
	uint8_t flex_off;
};

/**
 * A structure used to define the input for raw packet template.
 */
struct rte_pmd_i40e_pkt_template_input {
	/** the pctype used for raw packet template */
	uint16_t pctype;
	/** the buffer containing raw packet template */
	void *packet;
	/** the length of buffer with raw packet template */
	uint32_t length;
};

/**
 * A structure used to define the configuration parameters
 * for raw packet template.
 */
struct rte_pmd_i40e_pkt_template_conf {
	/** the input for raw packet template. */
	struct rte_pmd_i40e_pkt_template_input input;
	/** the action to be taken when raw packet template is matched */
	struct rte_pmd_i40e_pkt_template_action action;
	/** ID, an unique software index for the raw packet template filter */
	uint32_t soft_id;
};

enum rte_pmd_i40e_inset_type {
	INSET_NONE = 0,
	INSET_HASH,
	INSET_FDIR,
	INSET_FDIR_FLX,
};

struct  rte_pmd_i40e_inset_mask {
	uint8_t field_idx;
	uint16_t mask;
};

struct rte_pmd_i40e_inset {
	uint64_t inset;
	struct rte_pmd_i40e_inset_mask mask[2];
};

/**
 * Add or remove raw packet template filter to Flow Director.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param conf
 *   Specifies configuration parameters of raw packet template filter.
 * @param add
 *   Specifies an action to be taken - add or remove raw packet template filter.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *conf* invalid.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_i40e_flow_add_del_packet_template(
			uint16_t port,
			const struct rte_pmd_i40e_pkt_template_conf *conf,
			uint8_t add);

/**
 * Notify VF when PF link status changes.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf
 *   VF id.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* invalid.
 */
int rte_pmd_i40e_ping_vfs(uint16_t port, uint16_t vf);

/**
 * Enable/Disable VF MAC anti spoofing.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set MAC anti spoofing.
 * @param on
 *    1 - Enable VFs MAC anti spoofing.
 *    0 - Disable VFs MAC anti spoofing.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_mac_anti_spoof(uint16_t port,
				       uint16_t vf_id,
				       uint8_t on);

/**
 * Enable/Disable VF VLAN anti spoofing.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set VLAN anti spoofing.
 * @param on
 *    1 - Enable VFs VLAN anti spoofing.
 *    0 - Disable VFs VLAN anti spoofing.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_vlan_anti_spoof(uint16_t port,
					uint16_t vf_id,
					uint8_t on);

/**
 * Enable/Disable TX loopback on all the PF and VFs.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param on
 *    1 - Enable TX loopback.
 *    0 - Disable TX loopback.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_tx_loopback(uint16_t port,
				 uint8_t on);

/**
 * Enable/Disable VF unicast promiscuous mode.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set.
 * @param on
 *    1 - Enable.
 *    0 - Disable.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_unicast_promisc(uint16_t port,
					uint16_t vf_id,
					uint8_t on);

/**
 * Enable/Disable VF multicast promiscuous mode.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to set.
 * @param on
 *    1 - Enable.
 *    0 - Disable.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_multicast_promisc(uint16_t port,
					  uint16_t vf_id,
					  uint8_t on);

/**
 * Set the VF MAC address.
 *
 * PF should set MAC address before VF initialized, if PF sets the MAC
 * address after VF initialized, new MAC address won't be effective until
 * VF reinitialize.
 *
 * This will remove all existing MAC filters.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf_id
 *   VF id.
 * @param mac_addr
 *   VF MAC address.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *mac_addr* is invalid.
 */
int rte_pmd_i40e_set_vf_mac_addr(uint16_t port, uint16_t vf_id,
				 struct rte_ether_addr *mac_addr);

/**
 * Remove the VF MAC address.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf_id
 *   VF id.
 * @param mac_addr
 *   VF MAC address.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *mac_addr* is invalid.
 */
int
rte_pmd_i40e_remove_vf_mac_addr(uint16_t port, uint16_t vf_id,
	struct rte_ether_addr *mac_addr);

/**
 * Enable/Disable vf vlan strip for all queues in a pool
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf
 *    ID specifying VF.
 * @param on
 *    1 - Enable VF's vlan strip on RX queues.
 *    0 - Disable VF's vlan strip on RX queues.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int
rte_pmd_i40e_set_vf_vlan_stripq(uint16_t port, uint16_t vf, uint8_t on);

/**
 * Enable/Disable vf vlan insert
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param vlan_id
 *    0 - Disable VF's vlan insert.
 *    n - Enable; n is inserted as the vlan id.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_vlan_insert(uint16_t port, uint16_t vf_id,
				    uint16_t vlan_id);

/**
 * Enable/Disable vf broadcast mode
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param on
 *    0 - Disable broadcast.
 *    1 - Enable broadcast.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_broadcast(uint16_t port, uint16_t vf_id,
				  uint8_t on);

/**
 * Enable/Disable vf vlan tag
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param on
 *    0 - Disable VF's vlan tag.
 *    n - Enable VF's vlan tag.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_set_vf_vlan_tag(uint16_t port, uint16_t vf_id, uint8_t on);

/**
 * Enable/Disable VF VLAN filter
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vlan_id
 *    ID specifying VLAN
 * @param vf_mask
 *    Mask to filter VF's
 * @param on
 *    0 - Disable VF's VLAN filter.
 *    1 - Enable VF's VLAN filter.
 *
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_i40e_set_vf_vlan_filter(uint16_t port, uint16_t vlan_id,
				    uint64_t vf_mask, uint8_t on);

/**
 * Get VF's statistics
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to get.
 * @param stats
 *    A pointer to a structure of type *rte_eth_stats* to be filled with
 *    the values of device counters for the following set of statistics:
 *   - *ipackets* with the total of successfully received packets.
 *   - *opackets* with the total of successfully transmitted packets.
 *   - *ibytes*   with the total of successfully received bytes.
 *   - *obytes*   with the total of successfully transmitted bytes.
 *   - *ierrors*  with the total of erroneous received packets.
 *   - *oerrors*  with the total of failed transmitted packets.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */

int rte_pmd_i40e_get_vf_stats(uint16_t port,
			      uint16_t vf_id,
			      struct rte_eth_stats *stats);

/**
 * Clear VF's statistics
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    VF on which to get.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_reset_vf_stats(uint16_t port,
				uint16_t vf_id);

/**
 * Set VF's max bandwidth.
 *
 * Per VF bandwidth limitation and per TC bandwidth limitation cannot
 * be enabled in parallel. If per TC bandwidth is enabled, this function
 * will disable it.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param bw
 *    Bandwidth for this VF.
 *    The value should be an absolute bandwidth in Mbps.
 *    The bandwidth is a L2 bandwidth counting the bytes of ethernet packets.
 *    Not count the bytes added by physical layer.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_i40e_set_vf_max_bw(uint16_t port,
			       uint16_t vf_id,
			       uint32_t bw);

/**
 * Set all the TCs' bandwidth weight on a specific VF.
 *
 * The bw_weight means the percentage occupied by the TC.
 * It can be taken as the relative min bandwidth setting.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param tc_num
 *    Number of TCs.
 * @param bw_weight
 *    An array of relative bandwidth weight for all the TCs.
 *    The summary of the bw_weight should be 100.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_i40e_set_vf_tc_bw_alloc(uint16_t port,
				    uint16_t vf_id,
				    uint8_t tc_num,
				    uint8_t *bw_weight);

/**
 * Set a specific TC's max bandwidth on a specific VF.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param vf_id
 *    ID specifying VF.
 * @param tc_no
 *    Number specifying TC.
 * @param bw
 *    Max bandwidth for this TC.
 *    The value should be an absolute bandwidth in Mbps.
 *    The bandwidth is a L2 bandwidth counting the bytes of ethernet packets.
 *    Not count the bytes added by physical layer.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_i40e_set_vf_tc_max_bw(uint16_t port,
				  uint16_t vf_id,
				  uint8_t tc_no,
				  uint32_t bw);

/**
 * Set some TCs to strict priority mode on a physical port.
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param tc_map
 *    A bit map for the TCs.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) not supported by firmware.
 */
int rte_pmd_i40e_set_tc_strict_prio(uint16_t port, uint8_t tc_map);

/**
 * Load/Unload a ddp package
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param buff
 *    buffer of package.
 * @param size
 *    size of buffer.
 * @param op
 *   Operation of package processing
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-EEXIST) if profile exists.
 *   - (-EACCES) if profile does not exist.
 *   - (-ENOTSUP) if operation not supported.
 */
int rte_pmd_i40e_process_ddp_package(uint16_t port, uint8_t *buff,
				     uint32_t size,
				     enum rte_pmd_i40e_package_op op);

/**
 * rte_pmd_i40e_get_ddp_info - Get profile's info
 * @param pkg
 *    buffer of package.
 * @param pkg_size
 *    package buffer size
 * @param info
 *    buffer for response
 * @param size
 *    response buffer size
 * @param type
 *    type of information requested
 * @return
 *   - (0) if successful.
 *   - (-ENOTSUP) if information type not supported by the profile.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_get_ddp_info(uint8_t *pkg, uint32_t pkg_size,
				     uint8_t *info, uint32_t size,
				     enum rte_pmd_i40e_package_info type);

/**
 * rte_pmd_i40e_get_ddp_list - Get loaded profile list
 * @param port
 *    port id
 * @param buff
 *    buffer for response
 * @param size
 *    buffer size
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 */
int rte_pmd_i40e_get_ddp_list(uint16_t port, uint8_t *buff, uint32_t size);

/**
 * Update hardware defined ptype to software defined packet type
 * mapping table.
 *
 * @param port
 *    pointer to port identifier of the device.
 * @param mapping_items
 *    the base address of the mapping items array.
 * @param count
 *    number of mapping items.
 * @param exclusive
 *    the flag indicate different ptype mapping update method.
 *    -(0) only overwrite referred PTYPE mapping,
 *	keep other PTYPEs mapping unchanged.
 *    -(!0) overwrite referred PTYPE mapping,
 *	set other PTYPEs maps to PTYPE_UNKNOWN.
 */
int rte_pmd_i40e_ptype_mapping_update(
			uint16_t port,
			struct rte_pmd_i40e_ptype_mapping *mapping_items,
			uint16_t count,
			uint8_t exclusive);

/**
 * Reset hardware defined ptype to software defined ptype
 * mapping table to default.
 *
 * @param port
 *    pointer to port identifier of the device
 */
int rte_pmd_i40e_ptype_mapping_reset(uint16_t port);

/**
 * Get hardware defined ptype to software defined ptype
 * mapping items.
 *
 * @param port
 *    pointer to port identifier of the device.
 * @param mapping_items
 *    the base address of the array to store returned items.
 * @param size
 *    the size of the input array.
 * @param count
 *    the place to store the number of returned items.
 * @param valid_only
 *    -(0) return full mapping table.
 *    -(!0) only return mapping items which packet_type != RTE_PTYPE_UNKNOWN.
 */
int rte_pmd_i40e_ptype_mapping_get(
			uint16_t port,
			struct rte_pmd_i40e_ptype_mapping *mapping_items,
			uint16_t size,
			uint16_t *count,
			uint8_t valid_only);

/**
 * Replace a specific or a group of software defined ptypes
 * with a new one
 *
 * @param port
 *    pointer to port identifier of the device
 * @param target
 *    the packet type to be replaced
 * @param mask
 *    -(0) target represent a specific software defined ptype.
 *    -(!0) target is a mask to represent a group of software defined ptypes.
 * @param pkt_type
 *    the new packet type to overwrite
 */
int rte_pmd_i40e_ptype_mapping_replace(uint16_t port,
				       uint32_t target,
				       uint8_t mask,
				       uint32_t pkt_type);

/**
 * Add a VF MAC address.
 *
 * Add more MAC address for VF. The existing MAC addresses
 * are still effective.
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param vf_id
 *   VF id.
 * @param mac_addr
 *   VF MAC address.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if *vf* or *mac_addr* is invalid.
 */
int rte_pmd_i40e_add_vf_mac_addr(uint16_t port, uint16_t vf_id,
				 struct rte_ether_addr *mac_addr);

#define RTE_PMD_I40E_PCTYPE_MAX		64
#define RTE_PMD_I40E_FLOW_TYPE_MAX	64

struct rte_pmd_i40e_flow_type_mapping {
	uint16_t flow_type; /**< software defined flow type*/
	uint64_t pctype;    /**< hardware defined pctype */
};

/**
 * Update hardware defined pctype to software defined flow type
 * mapping table.
 *
 * @param port
 *    pointer to port identifier of the device.
 * @param mapping_items
 *    the base address of the mapping items array.
 * @param count
 *    number of mapping items.
 * @param exclusive
 *    the flag indicate different pctype mapping update method.
 *    -(0) only overwrite referred PCTYPE mapping,
 *	keep other PCTYPEs mapping unchanged.
 *    -(!0) overwrite referred PCTYPE mapping,
 *	set other PCTYPEs maps to PCTYPE_INVALID.
 */
int rte_pmd_i40e_flow_type_mapping_update(
			uint16_t port,
			struct rte_pmd_i40e_flow_type_mapping *mapping_items,
			uint16_t count,
			uint8_t exclusive);

/**
 * Get software defined flow type to hardware defined pctype
 * mapping items.
 *
 * @param port
 *    pointer to port identifier of the device.
 * @param mapping_items
 *    the base address of the array to store returned items.
 *    array should be allocated by caller with minimum size of
 *    RTE_PMD_I40E_FLOW_TYPE_MAX items
 */
int rte_pmd_i40e_flow_type_mapping_get(
			uint16_t port,
			struct rte_pmd_i40e_flow_type_mapping *mapping_items);

/**
 * Reset hardware defined pctype to software defined flow type
 * mapping table to default.
 *
 * @param port
 *    pointer to port identifier of the device
 */
int rte_pmd_i40e_flow_type_mapping_reset(uint16_t port);

/**
 * On the PF, find VF index based on VF MAC address
 *
 * @param port
 *    pointer to port identifier of the device
 * @param vf_mac
 *    the mac address of the vf to determine index of
 * @return
 *    The index of vfid If successful.
 *    -EINVAL: vf mac address does not exist for this port
 *    -ENOTSUP: i40e not supported for this port.
 */
int rte_pmd_i40e_query_vfid_by_mac(uint16_t port,
					const struct rte_ether_addr *vf_mac);

/**
 * Do RSS queue region configuration for that port as
 * the command option type
 *
 * @param port_id
 *    The port identifier of the Ethernet device.
 * @param op_type
 *    Queue region operation type
 * @param arg
 *    Queue region operation type specific data
 */
int rte_pmd_i40e_rss_queue_region_conf(uint16_t port_id,
			enum rte_pmd_i40e_queue_region_op op_type, void *arg);

int rte_pmd_i40e_cfg_hash_inset(uint16_t port,
				uint64_t pctype, uint64_t inset);

/**
 * Get input set
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param pctype
 *    HW pctype.
 * @param inset
 *    Buffer for input set info.
 * @param inset_type
 *    Type of input set.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) if operation not supported.
 */
int rte_pmd_i40e_inset_get(uint16_t port, uint8_t pctype,
			   struct rte_pmd_i40e_inset *inset,
			   enum rte_pmd_i40e_inset_type inset_type);

/**
 * Set input set
 *
 * @param port
 *    The port identifier of the Ethernet device.
 * @param pctype
 *    HW pctype.
 * @param inset
 *    Input set info.
 * @param inset_type
 *    Type of input set.
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-EINVAL) if bad parameter.
 *   - (-ENOTSUP) if operation not supported.
 */
int rte_pmd_i40e_inset_set(uint16_t port, uint8_t pctype,
			   struct rte_pmd_i40e_inset *inset,
			   enum rte_pmd_i40e_inset_type inset_type);

/**
 * Get bit value for some field index
 *
 * @param inset
 *    Input set value.
 * @param field_idx
 *    Field index for input set.
 * @return
 *   - (1) if set.
 *   - (0) if cleared.
 */
static inline int
rte_pmd_i40e_inset_field_get(uint64_t inset, uint8_t field_idx)
{
	uint8_t bit_idx;

	if (field_idx > 63)
		return 0;

	bit_idx = 63 - field_idx;
	if (inset & (1ULL << bit_idx))
		return 1;

	return 0;
}

/**
 * Set bit value for some field index
 *
 * @param inset
 *    Input set value.
 * @param field_idx
 *    Field index for input set.
 * @return
 *   - (-1) if failed.
 *   - (0) if success.
 */
static inline int
rte_pmd_i40e_inset_field_set(uint64_t *inset, uint8_t field_idx)
{
	uint8_t bit_idx;

	if (field_idx > 63)
		return -1;

	bit_idx = 63 - field_idx;
	*inset = *inset | (1ULL << bit_idx);

	return 0;
}

/**
 * Clear bit value for some field index
 *
 * @param inset
 *    Input set value.
 * @param field_idx
 *    Field index for input set.
 * @return
 *   - (-1) if failed.
 *   - (0) if success.
 */
static inline int
rte_pmd_i40e_inset_field_clear(uint64_t *inset, uint8_t field_idx)
{
	uint8_t bit_idx;

	if (field_idx > 63)
		return -1;

	bit_idx = 63 - field_idx;
	*inset = *inset & ~(1ULL << bit_idx);

	return 0;
}

/**
 * Get port fdir info
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param fdir_info
 *   The fdir info of the port
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if operation not supported.
 */
__rte_experimental
int
rte_pmd_i40e_get_fdir_info(uint16_t port, struct rte_eth_fdir_info *fdir_info);

/**
 * Get port fdir status
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param fdir_stat
 *   The fdir status of the port
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if operation not supported.
 */
__rte_experimental
int
rte_pmd_i40e_get_fdir_stats(uint16_t port,
			    struct rte_eth_fdir_stats *fdir_stat);

/**
 * Set GRE key length
 *
 * @param port
 *   The port identifier of the Ethernet device.
 * @param len
 *   Length of gre-key
 * @return
 *   - (0) if successful.
 *   - (-ENODEV) if *port* invalid.
 *   - (-ENOTSUP) if operation not supported.
 */
__rte_experimental
int
rte_pmd_i40e_set_gre_key_len(uint16_t port, uint8_t len);

/**
 * For ipn3ke, i40e works with FPGA.
 * In this situation, i40e get link status from fpga,
 * fpga works as switch_dev for i40e.
 * This function set switch_dev for i40e.
 *
 * @param port_id
 *    port_id of i40e device to be set switch device.
 * @param switch_dev
 *    target switch device from which i40e device to get link status from.
 * @return
 *   - (less than 0) if failed.
 *   - (0) if success.
 */
__rte_experimental
int
rte_pmd_i40e_set_switch_dev(uint16_t port_id, struct rte_eth_dev *switch_dev);

#endif /* _PMD_I40E_H_ */
