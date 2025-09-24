/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NTHW_MAC_PCS_H_
#define NTHW_MAC_PCS_H_

enum nthw_mac_pcs_led_mode_e {
	NTHW_MAC_PCS_LED_AUTO = 0x00,
	NTHW_MAC_PCS_LED_ON = 0x01,
	NTHW_MAC_PCS_LED_OFF = 0x02,
	NTHW_MAC_PCS_LED_PORTID = 0x03,
};

#define nthw_mac_pcs_receiver_mode_dfe (0)
#define nthw_mac_pcs_receiver_mode_lpm (1)

struct nthw_mac_pcs {
	uint8_t m_port_no;

	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_mac_pcs;
	int mn_instance;

	/* Block lock status */
	nthw_field_t *mp_fld_block_lock_lock;
	uint32_t m_fld_block_lock_lock_mask;

	/* Lane lock status */
	nthw_field_t *mp_fld_vl_demuxed_lock;
	uint32_t m_fld_vl_demuxed_lock_mask;

	/* GTY_STAT */
	nthw_field_t *mp_fld_gty_stat_rx_rst_done0;
	nthw_field_t *mp_fld_gty_stat_rx_rst_done1;
	nthw_field_t *mp_fld_gty_stat_rx_rst_done2;
	nthw_field_t *mp_fld_gty_stat_rx_rst_done3;
	nthw_field_t *mp_fld_gty_stat_tx_rst_done0;
	nthw_field_t *mp_fld_gty_stat_tx_rst_done1;
	nthw_field_t *mp_fld_gty_stat_tx_rst_done2;
	nthw_field_t *mp_fld_gty_stat_tx_rst_done3;
	uint32_t m_fld_gty_stat_rx_rst_done_mask;
	uint32_t m_fld_gty_stat_tx_rst_done_mask;

	/* GTY_LOOP */
	nthw_register_t *mp_reg_gty_loop;
	nthw_field_t *mp_fld_gty_loop_gt_loop0;
	nthw_field_t *mp_fld_gty_loop_gt_loop1;
	nthw_field_t *mp_fld_gty_loop_gt_loop2;
	nthw_field_t *mp_fld_gty_loop_gt_loop3;

	/* MAC_PCS_CONFIG */
	nthw_field_t *mp_fld_pcs_config_tx_path_rst;
	nthw_field_t *mp_fld_pcs_config_rx_path_rst;
	nthw_field_t *mp_fld_pcs_config_rx_enable;
	nthw_field_t *mp_fld_pcs_config_rx_force_resync;
	nthw_field_t *mp_fld_pcs_config_rx_test_pattern;
	nthw_field_t *mp_fld_pcs_config_tx_enable;
	nthw_field_t *mp_fld_pcs_config_tx_send_idle;
	nthw_field_t *mp_fld_pcs_config_tx_send_rfi;
	nthw_field_t *mp_fld_pcs_config_tx_test_pattern;

	/* STAT PCS */
	nthw_field_t *mp_fld_stat_pcs_rx_status;
	nthw_field_t *mp_fld_stat_pcs_rx_aligned;
	nthw_field_t *mp_fld_stat_pcs_rx_aligned_err;
	nthw_field_t *mp_fld_stat_pcs_rx_misaligned;
	nthw_field_t *mp_fld_stat_pcs_rx_internal_local_fault;
	nthw_field_t *mp_fld_stat_pcs_rx_received_local_fault;
	nthw_field_t *mp_fld_stat_pcs_rx_local_fault;
	nthw_field_t *mp_fld_stat_pcs_rx_remote_fault;
	nthw_field_t *mp_fld_stat_pcs_rx_hi_ber;

	/* STAT_PCS_RX_LATCH */
	nthw_field_t *mp_fld_stat_pcs_rx_latch_status;

	/* PHYMAC_MISC */
	nthw_field_t *mp_fld_phymac_misc_tx_sel_host;
	nthw_field_t *mp_fld_phymac_misc_tx_sel_tfg;
	nthw_field_t *mp_fld_phymac_misc_tx_sel_rx_loop;
	nthw_field_t *mp_fld_phymac_misc_ts_eop;

	/* LINK_SUMMARY */
	nthw_register_t *mp_reg_link_summary;
	nthw_field_t *mp_fld_link_summary_abs;
	nthw_field_t *mp_fld_link_summary_nt_phy_link_state;
	nthw_field_t *mp_fld_link_summary_lh_abs;
	nthw_field_t *mp_fld_link_summary_ll_nt_phy_link_state;
	nthw_field_t *mp_fld_link_summary_link_down_cnt;
	nthw_field_t *mp_fld_link_summary_nim_interr;
	nthw_field_t *mp_fld_link_summary_lh_local_fault;
	nthw_field_t *mp_fld_link_summary_lh_remote_fault;
	nthw_field_t *mp_fld_link_summary_local_fault;
	nthw_field_t *mp_fld_link_summary_remote_fault;

	/* BIP_ERR */
	nthw_register_t *mp_reg_bip_err;
	nthw_field_t *mp_fld_reg_bip_err_bip_err;

	/* FEC_CTRL */
	nthw_register_t *mp_reg_fec_ctrl;
	nthw_field_t *mp_field_fec_ctrl_reg_rs_fec_ctrl_in;

	/* FEC_STAT */
	nthw_register_t *mp_reg_fec_stat;
	nthw_field_t *mp_field_fec_stat_bypass;
	nthw_field_t *mp_field_fec_stat_valid;
	nthw_field_t *mp_field_fec_stat_am_lock0;
	nthw_field_t *mp_field_fec_stat_am_lock1;
	nthw_field_t *mp_field_fec_stat_am_lock2;
	nthw_field_t *mp_field_fec_stat_am_lock3;
	nthw_field_t *mp_field_fec_stat_fec_lane_algn;

	/* FEC Corrected code word count */
	nthw_register_t *mp_reg_fec_cw_cnt;
	nthw_field_t *mp_field_fec_cw_cnt_cw_cnt;

	/* FEC Uncorrected code word count */
	nthw_register_t *mp_reg_fec_ucw_cnt;
	nthw_field_t *mp_field_fec_ucw_cnt_ucw_cnt;

	/* GTY_RX_BUF_STAT */
	nthw_register_t *mp_reg_gty_rx_buf_stat;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat0;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat1;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat2;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat3;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat_changed0;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat_changed1;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat_changed2;
	nthw_field_t *mp_field_gty_rx_buf_stat_rx_buf_stat_changed3;

	/* GTY_PRE_CURSOR */
	nthw_register_t *mp_reg_gty_pre_cursor;
	nthw_field_t *mp_field_gty_pre_cursor_tx_pre_csr0;
	nthw_field_t *mp_field_gty_pre_cursor_tx_pre_csr1;
	nthw_field_t *mp_field_gty_pre_cursor_tx_pre_csr2;
	nthw_field_t *mp_field_gty_pre_cursor_tx_pre_csr3;

	/* GTY_DIFF_CTL */
	nthw_register_t *mp_reg_gty_diff_ctl;
	nthw_field_t *mp_field_gty_gty_diff_ctl_tx_diff_ctl0;
	nthw_field_t *mp_field_gty_gty_diff_ctl_tx_diff_ctl1;
	nthw_field_t *mp_field_gty_gty_diff_ctl_tx_diff_ctl2;
	nthw_field_t *mp_field_gty_gty_diff_ctl_tx_diff_ctl3;

	/* GTY_POST_CURSOR */
	nthw_register_t *mp_reg_gty_post_cursor;
	nthw_field_t *mp_field_gty_post_cursor_tx_post_csr0;
	nthw_field_t *mp_field_gty_post_cursor_tx_post_csr1;
	nthw_field_t *mp_field_gty_post_cursor_tx_post_csr2;
	nthw_field_t *mp_field_gty_post_cursor_tx_post_csr3;

	/* GTY_CTL */
	nthw_register_t *mp_reg_gty_ctl;
	nthw_register_t *mp_reg_gty_ctl_tx;
	nthw_field_t *mp_field_gty_ctl_tx_pol0;
	nthw_field_t *mp_field_gty_ctl_tx_pol1;
	nthw_field_t *mp_field_gty_ctl_tx_pol2;
	nthw_field_t *mp_field_gty_ctl_tx_pol3;
	nthw_field_t *mp_field_gty_ctl_rx_pol0;
	nthw_field_t *mp_field_gty_ctl_rx_pol1;
	nthw_field_t *mp_field_gty_ctl_rx_pol2;
	nthw_field_t *mp_field_gty_ctl_rx_pol3;
	nthw_field_t *mp_field_gty_ctl_rx_lpm_en0;
	nthw_field_t *mp_field_gty_ctl_rx_lpm_en1;
	nthw_field_t *mp_field_gty_ctl_rx_lpm_en2;
	nthw_field_t *mp_field_gty_ctl_rx_lpm_en3;
	nthw_field_t *mp_field_gty_ctl_rx_equa_rst0;
	nthw_field_t *mp_field_gty_ctl_rx_equa_rst1;
	nthw_field_t *mp_field_gty_ctl_rx_equa_rst2;
	nthw_field_t *mp_field_gty_ctl_rx_equa_rst3;

	/* DEBOUNCE_CTRL */
	nthw_register_t *mp_reg_debounce_ctrl;
	nthw_field_t *mp_field_debounce_ctrl_nt_port_ctrl;

	/* TIMESTAMP_COMP */
	nthw_register_t *mp_reg_time_stamp_comp;
	nthw_field_t *mp_field_time_stamp_comp_rx_dly;
	nthw_field_t *mp_field_time_stamp_comp_tx_dly;

	/* STAT_PCS_RX */
	nthw_register_t *mp_reg_stat_pcs_rx;

	/* STAT_PCS_RX */
	nthw_register_t *mp_reg_stat_pcs_rx_latch;

	/* PHYMAC_MISC */
	nthw_register_t *mp_reg_phymac_misc;

	/* BLOCK_LOCK */
	nthw_register_t *mp_reg_block_lock;
};

typedef struct nthw_mac_pcs nthw_mac_pcs_t;

int nthw_mac_pcs_init(nthw_mac_pcs_t *p, nthw_fpga_t *p_fpga, int n_instance);

void nthw_mac_pcs_rx_path_rst(nthw_mac_pcs_t *p, bool enable);
bool nthw_mac_pcs_is_rx_path_rst(nthw_mac_pcs_t *p);
/* wrapper - for ease of use */
void nthw_mac_pcs_set_rx_enable(nthw_mac_pcs_t *p, bool enable);
void nthw_mac_pcs_set_tx_enable(nthw_mac_pcs_t *p, bool enable);
void nthw_mac_pcs_set_tx_sel_host(nthw_mac_pcs_t *p, bool enable);
void nthw_mac_pcs_set_ts_eop(nthw_mac_pcs_t *p, bool enable);
void nthw_mac_pcs_set_host_loopback(nthw_mac_pcs_t *p, bool enable);
void nthw_mac_pcs_set_line_loopback(nthw_mac_pcs_t *p, bool enable);
void nthw_mac_pcs_reset_bip_counters(nthw_mac_pcs_t *p);
bool nthw_mac_pcs_get_hi_ber(nthw_mac_pcs_t *p);

void nthw_mac_pcs_get_link_summary(nthw_mac_pcs_t *p,
	uint32_t *p_abs,
	uint32_t *p_nt_phy_link_state,
	uint32_t *p_lh_abs,
	uint32_t *p_ll_nt_phy_link_state,
	uint32_t *p_link_down_cnt,
	uint32_t *p_nim_interr,
	uint32_t *p_lh_local_fault,
	uint32_t *p_lh_remote_fault,
	uint32_t *p_local_fault,
	uint32_t *p_remote_fault);

bool nthw_mac_pcs_reset_required(nthw_mac_pcs_t *p);
void nthw_mac_pcs_set_fec(nthw_mac_pcs_t *p, bool enable);
bool nthw_mac_pcs_get_fec_bypass(nthw_mac_pcs_t *p);
bool nthw_mac_pcs_get_fec_valid(nthw_mac_pcs_t *p);
bool nthw_mac_pcs_get_fec_stat_all_am_locked(nthw_mac_pcs_t *p);
void nthw_mac_pcs_reset_fec_counters(nthw_mac_pcs_t *p);
void nthw_mac_pcs_set_gty_tx_tuning(nthw_mac_pcs_t *p,
	uint8_t lane,
	uint8_t tx_pre_csr,
	uint8_t tx_diff_ctl,
	uint8_t tx_post_csr);
void nthw_mac_pcs_swap_gty_tx_polarity(nthw_mac_pcs_t *p, uint8_t lane, bool swap);
void nthw_mac_pcs_swap_gty_rx_polarity(nthw_mac_pcs_t *p, uint8_t lane, bool swap);
void nthw_mac_pcs_set_receiver_equalization_mode(nthw_mac_pcs_t *p, uint8_t mode);
void nthw_mac_pcs_set_led_mode(nthw_mac_pcs_t *p, uint8_t mode);
void nthw_mac_pcs_set_timestamp_comp_rx(nthw_mac_pcs_t *p, uint16_t rx_dly);
#endif	/* NTHW_MAC_PCS_H_ */
