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

#ifndef _AVF_PROTOTYPE_H_
#define _AVF_PROTOTYPE_H_

#include "avf_type.h"
#include "avf_alloc.h"
#include "virtchnl.h"

/* Prototypes for shared code functions that are not in
 * the standard function pointer structures.  These are
 * mostly because they are needed even before the init
 * has happened and will assist in the early SW and FW
 * setup.
 */

/* adminq functions */
enum avf_status_code avf_init_adminq(struct avf_hw *hw);
enum avf_status_code avf_shutdown_adminq(struct avf_hw *hw);
enum avf_status_code avf_init_asq(struct avf_hw *hw);
enum avf_status_code avf_init_arq(struct avf_hw *hw);
enum avf_status_code avf_alloc_adminq_asq_ring(struct avf_hw *hw);
enum avf_status_code avf_alloc_adminq_arq_ring(struct avf_hw *hw);
enum avf_status_code avf_shutdown_asq(struct avf_hw *hw);
enum avf_status_code avf_shutdown_arq(struct avf_hw *hw);
u16 avf_clean_asq(struct avf_hw *hw);
void avf_free_adminq_asq(struct avf_hw *hw);
void avf_free_adminq_arq(struct avf_hw *hw);
enum avf_status_code avf_validate_mac_addr(u8 *mac_addr);
void avf_adminq_init_ring_data(struct avf_hw *hw);
enum avf_status_code avf_clean_arq_element(struct avf_hw *hw,
					     struct avf_arq_event_info *e,
					     u16 *events_pending);
enum avf_status_code avf_asq_send_command(struct avf_hw *hw,
				struct avf_aq_desc *desc,
				void *buff, /* can be NULL */
				u16  buff_size,
				struct avf_asq_cmd_details *cmd_details);
bool avf_asq_done(struct avf_hw *hw);

/* debug function for adminq */
void avf_debug_aq(struct avf_hw *hw, enum avf_debug_mask mask,
		   void *desc, void *buffer, u16 buf_len);

void avf_idle_aq(struct avf_hw *hw);
bool avf_check_asq_alive(struct avf_hw *hw);
enum avf_status_code avf_aq_queue_shutdown(struct avf_hw *hw, bool unloading);

enum avf_status_code avf_aq_get_rss_lut(struct avf_hw *hw, u16 seid,
					  bool pf_lut, u8 *lut, u16 lut_size);
enum avf_status_code avf_aq_set_rss_lut(struct avf_hw *hw, u16 seid,
					  bool pf_lut, u8 *lut, u16 lut_size);
enum avf_status_code avf_aq_get_rss_key(struct avf_hw *hw,
				     u16 seid,
				     struct avf_aqc_get_set_rss_key_data *key);
enum avf_status_code avf_aq_set_rss_key(struct avf_hw *hw,
				     u16 seid,
				     struct avf_aqc_get_set_rss_key_data *key);
const char *avf_aq_str(struct avf_hw *hw, enum avf_admin_queue_err aq_err);
const char *avf_stat_str(struct avf_hw *hw, enum avf_status_code stat_err);


enum avf_status_code avf_set_mac_type(struct avf_hw *hw);

extern struct avf_rx_ptype_decoded avf_ptype_lookup[];

STATIC INLINE struct avf_rx_ptype_decoded decode_rx_desc_ptype(u8 ptype)
{
	return avf_ptype_lookup[ptype];
}

/* prototype for functions used for SW spinlocks */
void avf_init_spinlock(struct avf_spinlock *sp);
void avf_acquire_spinlock(struct avf_spinlock *sp);
void avf_release_spinlock(struct avf_spinlock *sp);
void avf_destroy_spinlock(struct avf_spinlock *sp);

/* avf_common for VF drivers*/
void avf_parse_hw_config(struct avf_hw *hw,
			     struct virtchnl_vf_resource *msg);
enum avf_status_code avf_reset(struct avf_hw *hw);
enum avf_status_code avf_aq_send_msg_to_pf(struct avf_hw *hw,
				enum virtchnl_ops v_opcode,
				enum avf_status_code v_retval,
				u8 *msg, u16 msglen,
				struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_set_filter_control(struct avf_hw *hw,
				struct avf_filter_control_settings *settings);
enum avf_status_code avf_aq_add_rem_control_packet_filter(struct avf_hw *hw,
				u8 *mac_addr, u16 ethtype, u16 flags,
				u16 vsi_seid, u16 queue, bool is_add,
				struct avf_control_filter_stats *stats,
				struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_aq_debug_dump(struct avf_hw *hw, u8 cluster_id,
				u8 table_id, u32 start_index, u16 buff_size,
				void *buff, u16 *ret_buff_size,
				u8 *ret_next_table, u32 *ret_next_index,
				struct avf_asq_cmd_details *cmd_details);
void avf_add_filter_to_drop_tx_flow_control_frames(struct avf_hw *hw,
						    u16 vsi_seid);
enum avf_status_code avf_aq_rx_ctl_read_register(struct avf_hw *hw,
				u32 reg_addr, u32 *reg_val,
				struct avf_asq_cmd_details *cmd_details);
u32 avf_read_rx_ctl(struct avf_hw *hw, u32 reg_addr);
enum avf_status_code avf_aq_rx_ctl_write_register(struct avf_hw *hw,
				u32 reg_addr, u32 reg_val,
				struct avf_asq_cmd_details *cmd_details);
void avf_write_rx_ctl(struct avf_hw *hw, u32 reg_addr, u32 reg_val);
enum avf_status_code avf_aq_set_phy_register(struct avf_hw *hw,
				u8 phy_select, u8 dev_addr,
				u32 reg_addr, u32 reg_val,
				struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_aq_get_phy_register(struct avf_hw *hw,
				u8 phy_select, u8 dev_addr,
				u32 reg_addr, u32 *reg_val,
				struct avf_asq_cmd_details *cmd_details);

enum avf_status_code avf_aq_set_arp_proxy_config(struct avf_hw *hw,
			struct avf_aqc_arp_proxy_data *proxy_config,
			struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_aq_set_ns_proxy_table_entry(struct avf_hw *hw,
			struct avf_aqc_ns_proxy_data *ns_proxy_table_entry,
			struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_aq_set_clear_wol_filter(struct avf_hw *hw,
			u8 filter_index,
			struct avf_aqc_set_wol_filter_data *filter,
			bool set_filter, bool no_wol_tco,
			bool filter_valid, bool no_wol_tco_valid,
			struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_aq_get_wake_event_reason(struct avf_hw *hw,
			u16 *wake_reason,
			struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_aq_clear_all_wol_filters(struct avf_hw *hw,
			struct avf_asq_cmd_details *cmd_details);
enum avf_status_code avf_read_phy_register_clause22(struct avf_hw *hw,
					u16 reg, u8 phy_addr, u16 *value);
enum avf_status_code avf_write_phy_register_clause22(struct avf_hw *hw,
					u16 reg, u8 phy_addr, u16 value);
enum avf_status_code avf_read_phy_register_clause45(struct avf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 *value);
enum avf_status_code avf_write_phy_register_clause45(struct avf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 value);
enum avf_status_code avf_read_phy_register(struct avf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 *value);
enum avf_status_code avf_write_phy_register(struct avf_hw *hw,
				u8 page, u16 reg, u8 phy_addr, u16 value);
u8 avf_get_phy_address(struct avf_hw *hw, u8 dev_num);
enum avf_status_code avf_blink_phy_link_led(struct avf_hw *hw,
					      u32 time, u32 interval);
enum avf_status_code avf_aq_write_ddp(struct avf_hw *hw, void *buff,
					u16 buff_size, u32 track_id,
					u32 *error_offset, u32 *error_info,
					struct avf_asq_cmd_details *
					cmd_details);
enum avf_status_code avf_aq_get_ddp_list(struct avf_hw *hw, void *buff,
					   u16 buff_size, u8 flags,
					   struct avf_asq_cmd_details *
					   cmd_details);
struct avf_generic_seg_header *
avf_find_segment_in_package(u32 segment_type,
			     struct avf_package_header *pkg_header);
struct avf_profile_section_header *
avf_find_section_in_profile(u32 section_type,
			     struct avf_profile_segment *profile);
enum avf_status_code
avf_write_profile(struct avf_hw *hw, struct avf_profile_segment *avf_seg,
		   u32 track_id);
enum avf_status_code
avf_rollback_profile(struct avf_hw *hw, struct avf_profile_segment *avf_seg,
		      u32 track_id);
enum avf_status_code
avf_add_pinfo_to_list(struct avf_hw *hw,
		       struct avf_profile_segment *profile,
		       u8 *profile_info_sec, u32 track_id);
#endif /* _AVF_PROTOTYPE_H_ */
