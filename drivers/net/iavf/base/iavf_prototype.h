/*******************************************************************************

Copyright (c) 2013 - 2015, Intel Corporation
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

 1. Redistributions of source code must retain the above copyright notice,
    this list of conditions and the following disclaimer.

 2. Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.

 3. Neither the name of the Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived from
    this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE
LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.

***************************************************************************/

#ifndef _IAVF_PROTOTYPE_H_
#define _IAVF_PROTOTYPE_H_

#include "iavf_type.h"
#include "iavf_alloc.h"
#include "virtchnl.h"

/* Prototypes for shared code functions that are not in
 * the standard function pointer structures.  These are
 * mostly because they are needed even before the init
 * has happened and will assist in the early SW and FW
 * setup.
 */

/* adminq functions */
enum iavf_status_code iavf_init_adminq(struct iavf_hw *hw);
enum iavf_status_code iavf_shutdown_adminq(struct iavf_hw *hw);
enum iavf_status_code iavf_init_asq(struct iavf_hw *hw);
enum iavf_status_code iavf_init_arq(struct iavf_hw *hw);
enum iavf_status_code iavf_alloc_adminq_asq_ring(struct iavf_hw *hw);
enum iavf_status_code iavf_alloc_adminq_arq_ring(struct iavf_hw *hw);
enum iavf_status_code iavf_shutdown_asq(struct iavf_hw *hw);
enum iavf_status_code iavf_shutdown_arq(struct iavf_hw *hw);
u16 iavf_clean_asq(struct iavf_hw *hw);
void iavf_free_adminq_asq(struct iavf_hw *hw);
void iavf_free_adminq_arq(struct iavf_hw *hw);
enum iavf_status_code iavf_validate_mac_addr(u8 *mac_addr);
void iavf_adminq_init_ring_data(struct iavf_hw *hw);
enum iavf_status_code iavf_clean_arq_element(struct iavf_hw *hw,
					     struct iavf_arq_event_info *e,
					     u16 *events_pending);
enum iavf_status_code iavf_asq_send_command(struct iavf_hw *hw,
				struct iavf_aq_desc *desc,
				void *buff, /* can be NULL */
				u16  buff_size,
				struct iavf_asq_cmd_details *cmd_details);
bool iavf_asq_done(struct iavf_hw *hw);

/* debug function for adminq */
void iavf_debug_aq(struct iavf_hw *hw, enum iavf_debug_mask mask,
		   void *desc, void *buffer, u16 buf_len);

void iavf_idle_aq(struct iavf_hw *hw);
bool iavf_check_asq_alive(struct iavf_hw *hw);
enum iavf_status_code iavf_aq_queue_shutdown(struct iavf_hw *hw, bool unloading);

enum iavf_status_code iavf_aq_get_rss_lut(struct iavf_hw *hw, u16 seid,
					  bool pf_lut, u8 *lut, u16 lut_size);
enum iavf_status_code iavf_aq_set_rss_lut(struct iavf_hw *hw, u16 seid,
					  bool pf_lut, u8 *lut, u16 lut_size);
enum iavf_status_code iavf_aq_get_rss_key(struct iavf_hw *hw,
				     u16 seid,
				     struct iavf_aqc_get_set_rss_key_data *key);
enum iavf_status_code iavf_aq_set_rss_key(struct iavf_hw *hw,
				     u16 seid,
				     struct iavf_aqc_get_set_rss_key_data *key);
const char *iavf_aq_str(struct iavf_hw *hw, enum iavf_admin_queue_err aq_err);
const char *iavf_stat_str(struct iavf_hw *hw, enum iavf_status_code stat_err);


enum iavf_status_code iavf_set_mac_type(struct iavf_hw *hw);

extern struct iavf_rx_ptype_decoded iavf_ptype_lookup[];

STATIC INLINE struct iavf_rx_ptype_decoded decode_rx_desc_ptype(u8 ptype)
{
	return iavf_ptype_lookup[ptype];
}

/* prototype for functions used for SW spinlocks */
void iavf_init_spinlock(struct iavf_spinlock *sp);
void iavf_acquire_spinlock(struct iavf_spinlock *sp);
void iavf_release_spinlock(struct iavf_spinlock *sp);
void iavf_destroy_spinlock(struct iavf_spinlock *sp);

/* iavf_common for VF drivers*/
void iavf_parse_hw_config(struct iavf_hw *hw,
			     struct virtchnl_vf_resource *msg);
enum iavf_status_code iavf_reset(struct iavf_hw *hw);
enum iavf_status_code iavf_aq_send_msg_to_pf(struct iavf_hw *hw,
				enum virtchnl_ops v_opcode,
				enum iavf_status_code v_retval,
				u8 *msg, u16 msglen,
				struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_set_filter_control(struct iavf_hw *hw,
				struct iavf_filter_control_settings *settings);
enum iavf_status_code iavf_aq_add_rem_control_packet_filter(struct iavf_hw *hw,
				u8 *mac_addr, u16 ethtype, u16 flags,
				u16 vsi_seid, u16 queue, bool is_add,
				struct iavf_control_filter_stats *stats,
				struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_aq_debug_dump(struct iavf_hw *hw, u8 cluster_id,
				u8 table_id, u32 start_index, u16 buff_size,
				void *buff, u16 *ret_buff_size,
				u8 *ret_next_table, u32 *ret_next_index,
				struct iavf_asq_cmd_details *cmd_details);
void iavf_add_filter_to_drop_tx_flow_control_frames(struct iavf_hw *hw,
						    u16 vsi_seid);
enum iavf_status_code iavf_aq_rx_ctl_read_register(struct iavf_hw *hw,
				u32 reg_addr, u32 *reg_val,
				struct iavf_asq_cmd_details *cmd_details);
u32 iavf_read_rx_ctl(struct iavf_hw *hw, u32 reg_addr);
enum iavf_status_code iavf_aq_rx_ctl_write_register(struct iavf_hw *hw,
				u32 reg_addr, u32 reg_val,
				struct iavf_asq_cmd_details *cmd_details);
void iavf_write_rx_ctl(struct iavf_hw *hw, u32 reg_addr, u32 reg_val);
enum iavf_status_code iavf_aq_set_phy_register(struct iavf_hw *hw,
				u8 phy_select, u8 dev_addr,
				u32 reg_addr, u32 reg_val,
				struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_aq_get_phy_register(struct iavf_hw *hw,
				u8 phy_select, u8 dev_addr,
				u32 reg_addr, u32 *reg_val,
				struct iavf_asq_cmd_details *cmd_details);

enum iavf_status_code iavf_aq_set_arp_proxy_config(struct iavf_hw *hw,
			struct iavf_aqc_arp_proxy_data *proxy_config,
			struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_aq_set_ns_proxy_table_entry(struct iavf_hw *hw,
			struct iavf_aqc_ns_proxy_data *ns_proxy_table_entry,
			struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_aq_set_clear_wol_filter(struct iavf_hw *hw,
			u8 filter_index,
			struct iavf_aqc_set_wol_filter_data *filter,
			bool set_filter, bool no_wol_tco,
			bool filter_valid, bool no_wol_tco_valid,
			struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_aq_get_wake_event_reason(struct iavf_hw *hw,
			u16 *wake_reason,
			struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_aq_clear_all_wol_filters(struct iavf_hw *hw,
			struct iavf_asq_cmd_details *cmd_details);
enum iavf_status_code iavf_read_phy_register_clause22(struct iavf_hw *hw,
					u16 reg, u8 phy_addr, u16 *value);
enum iavf_status_code iavf_write_phy_register_clause22(struct iavf_hw *hw,
					u16 reg, u8 phy_addr, u16 value);
enum iavf_status_code iavf_read_phy_register_clause45(struct iavf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 *value);
enum iavf_status_code iavf_write_phy_register_clause45(struct iavf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 value);
enum iavf_status_code iavf_read_phy_register(struct iavf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 *value);
enum iavf_status_code iavf_write_phy_register(struct iavf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 value);
u8 iavf_get_phy_address(struct iavf_hw *hw, u8 dev_num);
enum iavf_status_code iavf_blink_phy_link_led(struct iavf_hw *hw,
					      u32 time, u32 interval);
enum iavf_status_code iavf_aq_write_ddp(struct iavf_hw *hw, void *buff,
					u16 buff_size, u32 track_id,
					u32 *error_offset, u32 *error_info,
					struct iavf_asq_cmd_details *
					cmd_details);
enum iavf_status_code iavf_aq_get_ddp_list(struct iavf_hw *hw, void *buff,
					   u16 buff_size, u8 flags,
					   struct iavf_asq_cmd_details *
					   cmd_details);
struct iavf_generic_seg_header *
iavf_find_segment_in_package(u32 segment_type,
			     struct iavf_package_header *pkg_header);
struct iavf_profile_section_header *
iavf_find_section_in_profile(u32 section_type,
			     struct iavf_profile_segment *profile);
enum iavf_status_code
iavf_write_profile(struct iavf_hw *hw, struct iavf_profile_segment *iavf_seg,
		   u32 track_id);
enum iavf_status_code
iavf_rollback_profile(struct iavf_hw *hw, struct iavf_profile_segment *iavf_seg,
		      u32 track_id);
enum iavf_status_code
iavf_add_pinfo_to_list(struct iavf_hw *hw,
		       struct iavf_profile_segment *profile,
		       u8 *profile_info_sec, u32 track_id);
#endif /* _IAVF_PROTOTYPE_H_ */
