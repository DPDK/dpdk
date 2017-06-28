/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2017 Intel Corporation. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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

#include <rte_ethdev.h>

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
	RTE_PMD_I40E_PKG_INFO_MAX = 0xFFFFFFFF
};

#define RTE_PMD_I40E_DDP_NAME_SIZE 32

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
int rte_pmd_i40e_ping_vfs(uint8_t port, uint16_t vf);

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
int rte_pmd_i40e_set_vf_mac_anti_spoof(uint8_t port,
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
int rte_pmd_i40e_set_vf_vlan_anti_spoof(uint8_t port,
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
int rte_pmd_i40e_set_tx_loopback(uint8_t port,
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
int rte_pmd_i40e_set_vf_unicast_promisc(uint8_t port,
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
int rte_pmd_i40e_set_vf_multicast_promisc(uint8_t port,
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
int rte_pmd_i40e_set_vf_mac_addr(uint8_t port, uint16_t vf_id,
				 struct ether_addr *mac_addr);

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
rte_pmd_i40e_set_vf_vlan_stripq(uint8_t port, uint16_t vf, uint8_t on);

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
int rte_pmd_i40e_set_vf_vlan_insert(uint8_t port, uint16_t vf_id,
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
int rte_pmd_i40e_set_vf_broadcast(uint8_t port, uint16_t vf_id,
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
int rte_pmd_i40e_set_vf_vlan_tag(uint8_t port, uint16_t vf_id, uint8_t on);

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
int rte_pmd_i40e_set_vf_vlan_filter(uint8_t port, uint16_t vlan_id,
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

int rte_pmd_i40e_get_vf_stats(uint8_t port,
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
int rte_pmd_i40e_reset_vf_stats(uint8_t port,
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
int rte_pmd_i40e_set_vf_max_bw(uint8_t port,
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
int rte_pmd_i40e_set_vf_tc_bw_alloc(uint8_t port,
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
int rte_pmd_i40e_set_vf_tc_max_bw(uint8_t port,
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
int rte_pmd_i40e_set_tc_strict_prio(uint8_t port, uint8_t tc_map);

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
int rte_pmd_i40e_process_ddp_package(uint8_t port, uint8_t *buff,
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
int rte_pmd_i40e_get_ddp_list(uint8_t port, uint8_t *buff, uint32_t size);

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
			uint8_t port,
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
int rte_pmd_i40e_ptype_mapping_reset(uint8_t port);

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
			uint8_t port,
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
int rte_pmd_i40e_ptype_mapping_replace(uint8_t port,
				       uint32_t target,
				       uint8_t mask,
				       uint32_t pkt_type);

#endif /* _PMD_I40E_H_ */
