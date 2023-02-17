/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2020 Marvell International Ltd.
 */

#include <rte_trace_point_register.h>

#include <ethdev_trace.h>
#include <rte_ethdev_trace_fp.h>

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_configure,
	lib.ethdev.configure)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rxq_setup,
	lib.ethdev.rxq.setup)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_txq_setup,
	lib.ethdev.txq.setup)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_start,
	lib.ethdev.start)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_stop,
	lib.ethdev.stop)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_close,
	lib.ethdev.close)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_burst,
	lib.ethdev.rx.burst)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_tx_burst,
	lib.ethdev.tx.burst)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_call_rx_callbacks,
	lib.ethdev.call_rx_callbacks)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_call_tx_callbacks,
	lib.ethdev.call_tx_callbacks)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_iterator_init,
	lib.ethdev.iterator_init)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_iterator_next,
	lib.ethdev.iterator_next)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_iterator_cleanup,
	lib.ethdev.iterator_cleanup)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_find_next,
	lib.ethdev.find_next)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_find_next_of,
	lib.ethdev.find_next_of)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_find_next_sibling,
	lib.ethdev.find_next_sibling)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_is_valid_port,
	lib.ethdev.is_valid_port)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_find_next_owned_by,
	lib.ethdev.find_next_owned_by)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_owner_new,
	lib.ethdev.owner_new)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_owner_set,
	lib.ethdev.owner_set)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_owner_unset,
	lib.ethdev.owner_unset)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_owner_delete,
	lib.ethdev.owner_delete)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_owner_get,
	lib.ethdev.owner_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_socket_id,
	lib.ethdev.socket_id)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_sec_ctx,
	lib.ethdev.get_sec_ctx)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_count_avail,
	lib.ethdev.count_avail)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_count_total,
	lib.ethdev.count_total)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_name_by_port,
	lib.ethdev.get_name_by_port)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_port_by_name,
	lib.ethdev.get_port_by_name)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_queue_start,
	lib.ethdev.rx_queue_start)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_queue_stop,
	lib.ethdev.rx_queue_stop)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_tx_queue_start,
	lib.ethdev.tx_queue_start)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_tx_queue_stop,
	lib.ethdev.tx_queue_stop)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_speed_bitflag,
	lib.ethdev.speed_bitflag)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_offload_name,
	lib.ethdev.rx_offload_name)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_tx_offload_name,
	lib.ethdev.tx_offload_name)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_capability_name,
	lib.ethdev.capability_name)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_link_up,
	lib.ethdev.set_link_up)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_link_down,
	lib.ethdev.set_link_down)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_reset,
	lib.ethdev.reset)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_is_removed,
	lib.ethdev.is_removed)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_rx_hairpin_queue_setup,
	lib.ethdev.rx_hairpin_queue_setup)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_hairpin_queue_setup,
	lib.ethdev.tx_hairpin_queue_setup)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_hairpin_bind,
	lib.ethdev.hairpin_bind)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_hairpin_unbind,
	lib.ethdev.hairpin_unbind)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_hairpin_get_peer_ports,
	lib.ethdev.hairpin_get_peer_ports)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_buffer_drop_callback,
	lib.ethdev.tx_buffer_drop_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_buffer_count_callback,
	lib.ethdev.tx_buffer_count_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_buffer_set_err_callback,
	lib.ethdev.tx_buffer_set_err_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_buffer_init,
	lib.ethdev.tx_buffer_init)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_done_cleanup,
	lib.ethdev.tx_done_cleanup)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_promiscuous_enable,
	lib.ethdev.promiscuous_enable)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_promiscuous_disable,
	lib.ethdev.promiscuous_disable)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_promiscuous_get,
	lib.ethdev.promiscuous_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_allmulticast_enable,
	lib.ethdev.allmulticast_enable)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_allmulticast_disable,
	lib.ethdev.allmulticast_disable)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_allmulticast_get,
	lib.ethdev.allmulticast_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_link_get,
	lib.ethdev.link_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_link_get_nowait,
	lib.ethdev.link_get_nowait)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_link_speed_to_str,
	lib.ethdev.link_speed_to_str)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_link_to_str,
	lib.ethdev.link_to_str)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_stats_get,
	lib.ethdev.stats_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_stats_reset,
	lib.ethdev.stats_reset)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_xstats_get_id_by_name,
	lib.ethdev.xstats_get_id_by_name)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_xstats_get_names_by_id,
	lib.ethdev.xstats_get_names_by_id)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_xstats_get_names,
	lib.ethdev.xstats_get_names)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_xstats_get_by_id,
	lib.ethdev.xstats_get_by_id)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_xstats_get,
	lib.ethdev.xstats_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_xstats_reset,
	lib.ethdev.xstats_reset)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_tx_queue_stats_mapping,
	lib.ethdev.set_tx_queue_stats_mapping)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_rx_queue_stats_mapping,
	lib.ethdev.set_rx_queue_stats_mapping)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_fw_version_get,
	lib.ethdev.fw_version_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_info_get,
	lib.ethdev.info_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_conf_get,
	lib.ethdev.conf_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_supported_ptypes,
	lib.ethdev.get_supported_ptypes)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_ptypes,
	lib.ethdev.set_ptypes)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_macaddrs_get,
	lib.ethdev.macaddrs_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_macaddr_get,
	lib.ethdev.macaddr_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_mtu,
	lib.ethdev.get_mtu)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_mtu,
	lib.ethdev.set_mtu)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_vlan_filter,
	lib.ethdev.vlan_filter)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_vlan_strip_on_queue,
	lib.ethdev.set_vlan_strip_on_queue)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_vlan_ether_type,
	lib.ethdev.set_vlan_ether_type)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_vlan_offload,
	lib.ethdev.set_vlan_offload)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_vlan_offload,
	lib.ethdev.get_vlan_offload)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_vlan_pvid,
	lib.ethdev.set_vlan_pvid)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_flow_ctrl_get,
	lib.ethdev.flow_ctrl_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_flow_ctrl_set,
	lib.ethdev.flow_ctrl_set)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_priority_flow_ctrl_set,
	lib.ethdev.priority_flow_ctrl_set)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_priority_flow_ctrl_queue_info_get,
	lib.ethdev.priority_flow_ctrl_queue_info_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_priority_flow_ctrl_queue_configure,
	lib.ethdev.priority_flow_ctrl_queue_configure)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rss_reta_update,
	lib.ethdev.rss_reta_update)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rss_reta_query,
	lib.ethdev.rss_reta_query)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rss_hash_update,
	lib.ethdev.rss_hash_update)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rss_hash_conf_get,
	lib.ethdev.rss_hash_conf_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_udp_tunnel_port_add,
	lib.ethdev.udp_tunnel_port_add)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_udp_tunnel_port_delete,
	lib.ethdev.udp_tunnel_port_delete)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_led_on,
	lib.ethdev.led_on)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_led_off,
	lib.ethdev.led_off)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_fec_get_capability,
	lib.ethdev.fec_get_capability)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_fec_get,
	lib.ethdev.fec_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_fec_set,
	lib.ethdev.fec_set)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_mac_addr_add,
	lib.ethdev.mac_addr_add)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_mac_addr_remove,
	lib.ethdev.mac_addr_remove)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_default_mac_addr_set,
	lib.ethdev.default_mac_addr_set)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_uc_hash_table_set,
	lib.ethdev.uc_hash_table_set)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_uc_all_hash_table_set,
	lib.ethdev.uc_all_hash_table_set)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_set_queue_rate_limit,
	lib.ethdev.set_queue_rate_limit)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_rx_avail_thresh_set,
	lib.ethdev.rx_avail_thresh_set)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_rx_avail_thresh_query,
	lib.ethdev.rx_avail_thresh_query)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_callback_register,
	lib.ethdev.callback_register)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_callback_unregister,
	lib.ethdev.callback_unregister)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_intr_ctl,
	lib.ethdev.rx_intr_ctl)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_intr_ctl_q_get_fd,
	lib.ethdev.rx_intr_ctl_q_get_fd)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_intr_ctl_q,
	lib.ethdev.rx_intr_ctl_q)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_intr_enable,
	lib.ethdev.rx_intr_enable)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_rx_intr_disable,
	lib.ethdev.rx_intr_disable)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_add_rx_callback,
	lib.ethdev.add_rx_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_add_first_rx_callback,
	lib.ethdev.add_first_rx_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_add_tx_callback,
	lib.ethdev.add_tx_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_remove_rx_callback,
	lib.ethdev.remove_rx_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_remove_tx_callback,
	lib.ethdev.remove_tx_callback)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_rx_queue_info_get,
	lib.ethdev.rx_queue_info_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_queue_info_get,
	lib.ethdev.tx_queue_info_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_rx_burst_mode_get,
	lib.ethdev.rx_burst_mode_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_tx_burst_mode_get,
	lib.ethdev.tx_burst_mode_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_get_monitor_addr,
	lib.ethdev.get_monitor_addr)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_mc_addr_list,
	lib.ethdev.set_mc_addr_list)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_timesync_enable,
	lib.ethdev.timesync_enable)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_timesync_disable,
	lib.ethdev.timesync_disable)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_timesync_read_rx_timestamp,
	lib.ethdev.timesync_read_rx_timestamp)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_timesync_read_tx_timestamp,
	lib.ethdev.timesync_read_tx_timestamp)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_timesync_adjust_time,
	lib.ethdev.timesync_adjust_time)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_timesync_read_time,
	lib.ethdev.timesync_read_time)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_timesync_write_time,
	lib.ethdev.timesync_write_time)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_read_clock,
	lib.ethdev.read_clock)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_reg_info,
	lib.ethdev.get_reg_info)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_eeprom_length,
	lib.ethdev.get_eeprom_length)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_eeprom,
	lib.ethdev.get_eeprom)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_set_eeprom,
	lib.ethdev.set_eeprom)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_module_info,
	lib.ethdev.get_module_info)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_module_eeprom,
	lib.ethdev.get_module_eeprom)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_get_dcb_info,
	lib.ethdev.get_dcb_info)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_adjust_nb_rx_tx_desc,
	lib.ethdev.adjust_nb_rx_tx_desc)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_hairpin_capability_get,
	lib.ethdev.hairpin_capability_get)

RTE_TRACE_POINT_REGISTER(rte_ethdev_trace_pool_ops_supported,
	lib.ethdev.pool_ops_supported)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_representor_info_get,
	lib.ethdev.representor_info_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_rx_metadata_negotiate,
	lib.ethdev.rx_metadata_negotiate)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_ip_reassembly_capability_get,
	lib.ethdev.ip_reassembly_capability_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_ip_reassembly_conf_get,
	lib.ethdev.ip_reassembly_conf_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_ip_reassembly_conf_set,
	lib.ethdev.ip_reassembly_conf_set)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_buffer_split_get_supported_hdr_ptypes,
	lib.ethdev.buffer_split_get_supported_hdr_ptypes)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_cman_info_get,
	lib.ethdev.cman_info_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_cman_config_init,
	lib.ethdev.cman_config_init)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_cman_config_set,
	lib.ethdev.cman_config_set)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_cman_config_get,
	lib.ethdev.cman_config_get)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_count_aggr_ports,
	lib.ethdev.count_aggr_ports)

RTE_TRACE_POINT_REGISTER(rte_eth_trace_map_aggr_tx_affinity,
	lib.ethdev.map_aggr_tx_affinity)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_copy,
	lib.ethdev.flow.copy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_create,
	lib.ethdev.flow.create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_destroy,
	lib.ethdev.flow.destroy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_flush,
	lib.ethdev.flow.flush)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_isolate,
	lib.ethdev.flow.isolate)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_query,
	lib.ethdev.flow.query)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_validate,
	lib.ethdev.flow.validate)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_conv,
	lib.ethdev.flow.conv)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_dynf_metadata_register,
	lib.ethdev.dynf_metadata_register)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_get_aged_flows,
	lib.ethdev.flow.get_aged_flows)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_get_q_aged_flows,
	lib.ethdev.flow.get_q_aged_flows)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_tunnel_decap_set,
	lib.ethdev.flow.tunnel_decap_set)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_tunnel_match,
	lib.ethdev.flow.tunnel_match)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_get_restore_info,
	lib.ethdev.flow.get_restore_info)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_tunnel_action_decap_release,
	lib.ethdev.flow.tunnel_action_decap_release)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_tunnel_item_release,
	lib.ethdev.flow.tunnel_item_release)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_action_handle_create,
	lib.ethdev.flow.action_handle_create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_action_handle_destroy,
	lib.ethdev.flow.action_handle_destroy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_action_handle_update,
	lib.ethdev.flow.action_handle_update)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_action_handle_query,
	lib.ethdev.flow.action_handle_query)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_flex_item_create,
	lib.ethdev.flow.flex_item_create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_flex_item_release,
	lib.ethdev.flow.flex_item_release)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_pick_transfer_proxy,
	lib.ethdev.flow.pick_transfer_proxy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_info_get,
	lib.ethdev.flow.info_get)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_configure,
	lib.ethdev.flow.configure)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_pattern_template_create,
	lib.ethdev.flow.pattern_template_create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_pattern_template_destroy,
	lib.ethdev.flow.pattern_template_destroy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_actions_template_create,
	lib.ethdev.flow.actions_template_create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_actions_template_destroy,
	lib.ethdev.flow.actions_template_destroy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_template_table_create,
	lib.ethdev.flow.template_table_create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_template_table_destroy,
	lib.ethdev.flow.template_table_destroy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_async_create,
	lib.ethdev.flow.async_create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_async_destroy,
	lib.ethdev.flow.async_destroy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_push,
	lib.ethdev.flow.push)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_pull,
	lib.ethdev.flow.pull)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_async_action_handle_create,
	lib.ethdev.flow.async_action_handle_create)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_async_action_handle_destroy,
	lib.ethdev.flow.async_action_handle_destroy)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_async_action_handle_update,
	lib.ethdev.flow.async_action_handle_update)

RTE_TRACE_POINT_REGISTER(rte_flow_trace_async_action_handle_query,
	lib.ethdev.flow.async.action.handle.query)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_capabilities_get,
	lib.ethdev.mtr.capabilities_get)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_create,
	lib.ethdev.mtr.create)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_destroy,
	lib.ethdev.mtr.destroy)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_disable,
	lib.ethdev.mtr.meter_disable)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_dscp_table_update,
	lib.ethdev.mtr.meter_dscp_table_update)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_enable,
	lib.ethdev.mtr.meter_enable)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_profile_add,
	lib.ethdev.mtr.meter_profile_add)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_profile_delete,
	lib.ethdev.mtr.meter_profile_delete)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_profile_get,
	lib.ethdev.mtr.meter_profile_get)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_profile_update,
	lib.ethdev.mtr.meter_profile_update)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_stats_read,
	lib.ethdev.mtr.stats_read)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_stats_update,
	lib.ethdev.mtr.stats_update)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_policy_add,
	lib.ethdev.mtr.meter_policy_add)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_policy_delete,
	lib.ethdev.mtr.meter_policy_delete)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_policy_get,
	lib.ethdev.mtr.meter_policy_get)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_policy_update,
	lib.ethdev.mtr.meter_policy_update)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_policy_validate,
	lib.ethdev.mtr.meter_policy_validate)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_meter_vlan_table_update,
	lib.ethdev.mtr.meter_vlan_table_update)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_color_in_protocol_get,
	lib.ethdev.mtr.color_in_protocol_get)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_color_in_protocol_priority_get,
	lib.ethdev.mtr.color_in_protocol_priority_get)

RTE_TRACE_POINT_REGISTER(rte_mtr_trace_color_in_protocol_set,
	lib.ethdev.mtr.color_in_protocol_set)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_capabilities_get,
	lib.ethdev.tm.capabilities_get)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_get_number_of_leaf_nodes,
	lib.ethdev.tm.get_number_of_leaf_nodes)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_hierarchy_commit,
	lib.ethdev.tm.hierarchy_commit)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_level_capabilities_get,
	lib.ethdev.tm.level_capabilities_get)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_mark_ip_dscp,
	lib.ethdev.tm.mark_ip_dscp)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_mark_ip_ecn,
	lib.ethdev.tm.mark_ip_ecn)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_mark_vlan_dei,
	lib.ethdev.tm.mark_vlan_dei)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_add,
	lib.ethdev.tm.node_add)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_capabilities_get,
	lib.ethdev.tm.node_capabilities_get)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_cman_update,
	lib.ethdev.tm.node_cman_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_delete,
	lib.ethdev.tm.node_delete)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_parent_update,
	lib.ethdev.tm.node_parent_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_resume,
	lib.ethdev.tm.node_resume)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_shaper_update,
	lib.ethdev.tm.node_shaper_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_shared_shaper_update,
	lib.ethdev.tm.node_shared_shaper_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_shared_wred_context_update,
	lib.ethdev.tm.node_shared_wred_context_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_stats_read,
	lib.ethdev.tm.node_stats_read)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_stats_update,
	lib.ethdev.tm.node_stats_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_suspend,
	lib.ethdev.tm.node_suspend)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_type_get,
	lib.ethdev.tm.node_type_get)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_wfq_weight_mode_update,
	lib.ethdev.tm.node_wfq_weight_mode_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_node_wred_context_update,
	lib.ethdev.tm.node_wred_context_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_shaper_profile_add,
	lib.ethdev.tm.shaper_profile_add)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_shaper_profile_delete,
	lib.ethdev.tm.shaper_profile_delete)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_shared_shaper_add_update,
	lib.ethdev.tm.shared_shaper_add_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_shared_shaper_delete,
	lib.ethdev.tm.shared_shaper_delete)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_shared_wred_context_add_update,
	lib.ethdev.tm.shared_wred_context_add_update)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_shared_wred_context_delete,
	lib.ethdev.tm.shared_wred_context_delete)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_wred_profile_add,
	lib.ethdev.tm.wred_profile_add)

RTE_TRACE_POINT_REGISTER(rte_tm_trace_wred_profile_delete,
	lib.ethdev.tm.wred_profile_delete)
