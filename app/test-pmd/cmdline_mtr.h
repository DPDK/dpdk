/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#ifndef _CMDLINE_MTR_H_
#define _CMDLINE_MTR_H_

/* Traffic Metering and Policing */
extern cmdline_parse_inst_t cmd_show_port_meter_cap;
extern cmdline_parse_inst_t cmd_add_port_meter_profile_srtcm;
extern cmdline_parse_inst_t cmd_add_port_meter_profile_trtcm;
extern cmdline_parse_inst_t cmd_add_port_meter_profile_trtcm_rfc4115;
extern cmdline_parse_inst_t cmd_del_port_meter_profile;
extern cmdline_parse_inst_t cmd_create_port_meter;
extern cmdline_parse_inst_t cmd_enable_port_meter;
extern cmdline_parse_inst_t cmd_disable_port_meter;
extern cmdline_parse_inst_t cmd_del_port_meter;
extern cmdline_parse_inst_t cmd_set_port_meter_profile;
extern cmdline_parse_inst_t cmd_set_port_meter_dscp_table;
extern cmdline_parse_inst_t cmd_set_port_meter_policer_action;
extern cmdline_parse_inst_t cmd_set_port_meter_stats_mask;
extern cmdline_parse_inst_t cmd_show_port_meter_stats;

#endif /* _CMDLINE_MTR_H_ */
