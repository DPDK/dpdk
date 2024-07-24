/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 Napatech A/S
 */

/*
 * Auto-generated file - do *NOT* edit
 */

#include "nthw_register.h"

static nthw_fpga_field_init_s gfg_burstsize0_fields[] = {
	{ GFG_BURSTSIZE0_VAL, 24, 0, 0 },
};

static nthw_fpga_field_init_s gfg_burstsize1_fields[] = {
	{ GFG_BURSTSIZE1_VAL, 24, 0, 0 },
};

static nthw_fpga_field_init_s gfg_ctrl0_fields[] = {
	{ GFG_CTRL0_ENABLE, 1, 0, 0 },
	{ GFG_CTRL0_MODE, 3, 1, 0 },
	{ GFG_CTRL0_PRBS_EN, 1, 4, 0 },
	{ GFG_CTRL0_SIZE, 14, 16, 64 },
};

static nthw_fpga_field_init_s gfg_ctrl1_fields[] = {
	{ GFG_CTRL1_ENABLE, 1, 0, 0 },
	{ GFG_CTRL1_MODE, 3, 1, 0 },
	{ GFG_CTRL1_PRBS_EN, 1, 4, 0 },
	{ GFG_CTRL1_SIZE, 14, 16, 64 },
};

static nthw_fpga_field_init_s gfg_run0_fields[] = {
	{ GFG_RUN0_RUN, 1, 0, 0 },
};

static nthw_fpga_field_init_s gfg_run1_fields[] = {
	{ GFG_RUN1_RUN, 1, 0, 0 },
};

static nthw_fpga_field_init_s gfg_sizemask0_fields[] = {
	{ GFG_SIZEMASK0_VAL, 14, 0, 0 },
};

static nthw_fpga_field_init_s gfg_sizemask1_fields[] = {
	{ GFG_SIZEMASK1_VAL, 14, 0, 0 },
};

static nthw_fpga_field_init_s gfg_streamid0_fields[] = {
	{ GFG_STREAMID0_VAL, 8, 0, 0 },
};

static nthw_fpga_field_init_s gfg_streamid1_fields[] = {
	{ GFG_STREAMID1_VAL, 8, 0, 1 },
};

static nthw_fpga_register_init_s gfg_registers[] = {
	{ GFG_BURSTSIZE0, 3, 24, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_burstsize0_fields },
	{ GFG_BURSTSIZE1, 8, 24, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_burstsize1_fields },
	{ GFG_CTRL0, 0, 30, NTHW_FPGA_REG_TYPE_WO, 4194304, 4, gfg_ctrl0_fields },
	{ GFG_CTRL1, 5, 30, NTHW_FPGA_REG_TYPE_WO, 4194304, 4, gfg_ctrl1_fields },
	{ GFG_RUN0, 1, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_run0_fields },
	{ GFG_RUN1, 6, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_run1_fields },
	{ GFG_SIZEMASK0, 4, 14, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_sizemask0_fields },
	{ GFG_SIZEMASK1, 9, 14, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_sizemask1_fields },
	{ GFG_STREAMID0, 2, 8, NTHW_FPGA_REG_TYPE_WO, 0, 1, gfg_streamid0_fields },
	{ GFG_STREAMID1, 7, 8, NTHW_FPGA_REG_TYPE_WO, 1, 1, gfg_streamid1_fields },
};

static nthw_fpga_field_init_s gmf_ctrl_fields[] = {
	{ GMF_CTRL_ENABLE, 1, 0, 0 },
	{ GMF_CTRL_FCS_ALWAYS, 1, 1, 0 },
	{ GMF_CTRL_IFG_AUTO_ADJUST_ENABLE, 1, 7, 0 },
	{ GMF_CTRL_IFG_ENABLE, 1, 2, 0 },
	{ GMF_CTRL_IFG_TX_NOW_ALWAYS, 1, 3, 0 },
	{ GMF_CTRL_IFG_TX_NOW_ON_TS_ENABLE, 1, 5, 0 },
	{ GMF_CTRL_IFG_TX_ON_TS_ADJUST_ON_SET_CLOCK, 1, 6, 0 },
	{ GMF_CTRL_IFG_TX_ON_TS_ALWAYS, 1, 4, 0 },
	{ GMF_CTRL_TS_INJECT_ALWAYS, 1, 8, 0 },
	{ GMF_CTRL_TS_INJECT_DUAL_STEP, 1, 9, 0 },
};

static nthw_fpga_field_init_s gmf_debug_lane_marker_fields[] = {
	{ GMF_DEBUG_LANE_MARKER_COMPENSATION, 16, 0, 16384 },
};

static nthw_fpga_field_init_s gmf_ifg_max_adjust_slack_fields[] = {
	{ GMF_IFG_MAX_ADJUST_SLACK_SLACK, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_ifg_set_clock_delta_fields[] = {
	{ GMF_IFG_SET_CLOCK_DELTA_DELTA, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_ifg_set_clock_delta_adjust_fields[] = {
	{ GMF_IFG_SET_CLOCK_DELTA_ADJUST_DELTA, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_ifg_tx_now_on_ts_fields[] = {
	{ GMF_IFG_TX_NOW_ON_TS_TS, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_speed_fields[] = {
	{ GMF_SPEED_IFG_SPEED, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_stat_data_buffer_fields[] = {
	{ GMF_STAT_DATA_BUFFER_USED, 15, 0, 0x0000 },
};

static nthw_fpga_field_init_s gmf_stat_max_delayed_pkt_fields[] = {
	{ GMF_STAT_MAX_DELAYED_PKT_NS, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_stat_next_pkt_fields[] = {
	{ GMF_STAT_NEXT_PKT_NS, 64, 0, 0 },
};

static nthw_fpga_field_init_s gmf_stat_sticky_fields[] = {
	{ GMF_STAT_STICKY_DATA_UNDERFLOWED, 1, 0, 0 },
	{ GMF_STAT_STICKY_IFG_ADJUSTED, 1, 1, 0 },
};

static nthw_fpga_field_init_s gmf_ts_inject_fields[] = {
	{ GMF_TS_INJECT_OFFSET, 14, 0, 0 },
	{ GMF_TS_INJECT_POS, 2, 14, 0 },
};

static nthw_fpga_register_init_s gmf_registers[] = {
	{ GMF_CTRL, 0, 10, NTHW_FPGA_REG_TYPE_WO, 0, 10, gmf_ctrl_fields },
	{
		GMF_DEBUG_LANE_MARKER, 7, 16, NTHW_FPGA_REG_TYPE_WO, 16384, 1,
		gmf_debug_lane_marker_fields
	},
	{
		GMF_IFG_MAX_ADJUST_SLACK, 4, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		gmf_ifg_max_adjust_slack_fields
	},
	{
		GMF_IFG_SET_CLOCK_DELTA, 2, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		gmf_ifg_set_clock_delta_fields
	},
	{
		GMF_IFG_SET_CLOCK_DELTA_ADJUST, 3, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1,
		gmf_ifg_set_clock_delta_adjust_fields
	},
	{ GMF_IFG_TX_NOW_ON_TS, 5, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1, gmf_ifg_tx_now_on_ts_fields },
	{ GMF_SPEED, 1, 64, NTHW_FPGA_REG_TYPE_WO, 0, 1, gmf_speed_fields },
	{ GMF_STAT_DATA_BUFFER, 9, 15, NTHW_FPGA_REG_TYPE_RO, 0, 1, gmf_stat_data_buffer_fields },
	{
		GMF_STAT_MAX_DELAYED_PKT, 11, 64, NTHW_FPGA_REG_TYPE_RC1, 0, 1,
		gmf_stat_max_delayed_pkt_fields
	},
	{ GMF_STAT_NEXT_PKT, 10, 64, NTHW_FPGA_REG_TYPE_RO, 0, 1, gmf_stat_next_pkt_fields },
	{ GMF_STAT_STICKY, 8, 2, NTHW_FPGA_REG_TYPE_RC1, 0, 2, gmf_stat_sticky_fields },
	{ GMF_TS_INJECT, 6, 16, NTHW_FPGA_REG_TYPE_WO, 0, 2, gmf_ts_inject_fields },
};

static nthw_fpga_field_init_s gpio_phy_cfg_fields[] = {
	{ GPIO_PHY_CFG_E_PORT0_RXLOS, 1, 8, 0 }, { GPIO_PHY_CFG_E_PORT1_RXLOS, 1, 9, 0 },
	{ GPIO_PHY_CFG_PORT0_INT_B, 1, 1, 1 }, { GPIO_PHY_CFG_PORT0_LPMODE, 1, 0, 0 },
	{ GPIO_PHY_CFG_PORT0_MODPRS_B, 1, 3, 1 }, { GPIO_PHY_CFG_PORT0_RESET_B, 1, 2, 0 },
	{ GPIO_PHY_CFG_PORT1_INT_B, 1, 5, 1 }, { GPIO_PHY_CFG_PORT1_LPMODE, 1, 4, 0 },
	{ GPIO_PHY_CFG_PORT1_MODPRS_B, 1, 7, 1 }, { GPIO_PHY_CFG_PORT1_RESET_B, 1, 6, 0 },
};

static nthw_fpga_field_init_s gpio_phy_gpio_fields[] = {
	{ GPIO_PHY_GPIO_E_PORT0_RXLOS, 1, 8, 0 }, { GPIO_PHY_GPIO_E_PORT1_RXLOS, 1, 9, 0 },
	{ GPIO_PHY_GPIO_PORT0_INT_B, 1, 1, 0x0000 }, { GPIO_PHY_GPIO_PORT0_LPMODE, 1, 0, 1 },
	{ GPIO_PHY_GPIO_PORT0_MODPRS_B, 1, 3, 0x0000 }, { GPIO_PHY_GPIO_PORT0_RESET_B, 1, 2, 0 },
	{ GPIO_PHY_GPIO_PORT1_INT_B, 1, 5, 0x0000 }, { GPIO_PHY_GPIO_PORT1_LPMODE, 1, 4, 1 },
	{ GPIO_PHY_GPIO_PORT1_MODPRS_B, 1, 7, 0x0000 }, { GPIO_PHY_GPIO_PORT1_RESET_B, 1, 6, 0 },
};

static nthw_fpga_register_init_s gpio_phy_registers[] = {
	{ GPIO_PHY_CFG, 0, 10, NTHW_FPGA_REG_TYPE_RW, 170, 10, gpio_phy_cfg_fields },
	{ GPIO_PHY_GPIO, 1, 10, NTHW_FPGA_REG_TYPE_RW, 17, 10, gpio_phy_gpio_fields },
};

static nthw_fpga_field_init_s hif_build_time_fields[] = {
	{ HIF_BUILD_TIME_TIME, 32, 0, 1713859545 },
};

static nthw_fpga_field_init_s hif_config_fields[] = {
	{ HIF_CONFIG_EXT_TAG, 1, 6, 0x0000 },
	{ HIF_CONFIG_MAX_READ, 3, 3, 0x0000 },
	{ HIF_CONFIG_MAX_TLP, 3, 0, 0x0000 },
};

static nthw_fpga_field_init_s hif_control_fields[] = {
	{ HIF_CONTROL_BLESSED, 8, 4, 0 },
	{ HIF_CONTROL_FSR, 1, 12, 1 },
	{ HIF_CONTROL_WRAW, 4, 0, 1 },
};

static nthw_fpga_field_init_s hif_prod_id_ex_fields[] = {
	{ HIF_PROD_ID_EX_LAYOUT, 1, 31, 0 },
	{ HIF_PROD_ID_EX_LAYOUT_VERSION, 8, 0, 1 },
	{ HIF_PROD_ID_EX_RESERVED, 23, 8, 0 },
};

static nthw_fpga_field_init_s hif_prod_id_lsb_fields[] = {
	{ HIF_PROD_ID_LSB_GROUP_ID, 16, 16, 9563 },
	{ HIF_PROD_ID_LSB_REV_ID, 8, 0, 39 },
	{ HIF_PROD_ID_LSB_VER_ID, 8, 8, 55 },
};

static nthw_fpga_field_init_s hif_prod_id_msb_fields[] = {
	{ HIF_PROD_ID_MSB_BUILD_NO, 10, 12, 0 },
	{ HIF_PROD_ID_MSB_TYPE_ID, 12, 0, 200 },
};

static nthw_fpga_field_init_s hif_sample_time_fields[] = {
	{ HIF_SAMPLE_TIME_SAMPLE_TIME, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s hif_status_fields[] = {
	{ HIF_STATUS_RD_ERR, 1, 9, 0 },
	{ HIF_STATUS_TAGS_IN_USE, 8, 0, 0 },
	{ HIF_STATUS_WR_ERR, 1, 8, 0 },
};

static nthw_fpga_field_init_s hif_stat_ctrl_fields[] = {
	{ HIF_STAT_CTRL_STAT_ENA, 1, 1, 0 },
	{ HIF_STAT_CTRL_STAT_REQ, 1, 0, 0 },
};

static nthw_fpga_field_init_s hif_stat_refclk_fields[] = {
	{ HIF_STAT_REFCLK_REFCLK250, 32, 0, 0 },
};

static nthw_fpga_field_init_s hif_stat_rx_fields[] = {
	{ HIF_STAT_RX_COUNTER, 32, 0, 0 },
};

static nthw_fpga_field_init_s hif_stat_tx_fields[] = {
	{ HIF_STAT_TX_COUNTER, 32, 0, 0 },
};

static nthw_fpga_field_init_s hif_test0_fields[] = {
	{ HIF_TEST0_DATA, 32, 0, 287454020 },
};

static nthw_fpga_field_init_s hif_test1_fields[] = {
	{ HIF_TEST1_DATA, 32, 0, 2864434397 },
};

static nthw_fpga_field_init_s hif_uuid0_fields[] = {
	{ HIF_UUID0_UUID0, 32, 0, 1237800326 },
};

static nthw_fpga_field_init_s hif_uuid1_fields[] = {
	{ HIF_UUID1_UUID1, 32, 0, 3057550372 },
};

static nthw_fpga_field_init_s hif_uuid2_fields[] = {
	{ HIF_UUID2_UUID2, 32, 0, 2445752330 },
};

static nthw_fpga_field_init_s hif_uuid3_fields[] = {
	{ HIF_UUID3_UUID3, 32, 0, 1864147557 },
};

static nthw_fpga_register_init_s hif_registers[] = {
	{ HIF_BUILD_TIME, 16, 32, NTHW_FPGA_REG_TYPE_RO, 1713859545, 1, hif_build_time_fields },
	{ HIF_CONFIG, 24, 7, NTHW_FPGA_REG_TYPE_RW, 0, 3, hif_config_fields },
	{ HIF_CONTROL, 40, 13, NTHW_FPGA_REG_TYPE_MIXED, 4097, 3, hif_control_fields },
	{ HIF_PROD_ID_EX, 112, 32, NTHW_FPGA_REG_TYPE_RO, 1, 3, hif_prod_id_ex_fields },
	{ HIF_PROD_ID_LSB, 0, 32, NTHW_FPGA_REG_TYPE_RO, 626734887, 3, hif_prod_id_lsb_fields },
	{ HIF_PROD_ID_MSB, 8, 22, NTHW_FPGA_REG_TYPE_RO, 200, 2, hif_prod_id_msb_fields },
	{ HIF_SAMPLE_TIME, 96, 1, NTHW_FPGA_REG_TYPE_WO, 0, 1, hif_sample_time_fields },
	{ HIF_STATUS, 32, 10, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, hif_status_fields },
	{ HIF_STAT_CTRL, 64, 2, NTHW_FPGA_REG_TYPE_WO, 0, 2, hif_stat_ctrl_fields },
	{ HIF_STAT_REFCLK, 72, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, hif_stat_refclk_fields },
	{ HIF_STAT_RX, 88, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, hif_stat_rx_fields },
	{ HIF_STAT_TX, 80, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, hif_stat_tx_fields },
	{ HIF_TEST0, 48, 32, NTHW_FPGA_REG_TYPE_RW, 287454020, 1, hif_test0_fields },
	{ HIF_TEST1, 56, 32, NTHW_FPGA_REG_TYPE_RW, 2864434397, 1, hif_test1_fields },
	{ HIF_UUID0, 128, 32, NTHW_FPGA_REG_TYPE_RO, 1237800326, 1, hif_uuid0_fields },
	{ HIF_UUID1, 144, 32, NTHW_FPGA_REG_TYPE_RO, 3057550372, 1, hif_uuid1_fields },
	{ HIF_UUID2, 160, 32, NTHW_FPGA_REG_TYPE_RO, 2445752330, 1, hif_uuid2_fields },
	{ HIF_UUID3, 176, 32, NTHW_FPGA_REG_TYPE_RO, 1864147557, 1, hif_uuid3_fields },
};

static nthw_fpga_field_init_s iic_adr_fields[] = {
	{ IIC_ADR_SLV_ADR, 7, 1, 0 },
};

static nthw_fpga_field_init_s iic_cr_fields[] = {
	{ IIC_CR_EN, 1, 0, 0 }, { IIC_CR_GC_EN, 1, 6, 0 }, { IIC_CR_MSMS, 1, 2, 0 },
	{ IIC_CR_RST, 1, 7, 0 }, { IIC_CR_RSTA, 1, 5, 0 }, { IIC_CR_TX, 1, 3, 0 },
	{ IIC_CR_TXAK, 1, 4, 0 }, { IIC_CR_TXFIFO_RESET, 1, 1, 0 },
};

static nthw_fpga_field_init_s iic_dgie_fields[] = {
	{ IIC_DGIE_GIE, 1, 31, 0 },
};

static nthw_fpga_field_init_s iic_gpo_fields[] = {
	{ IIC_GPO_GPO_VAL, 1, 0, 0 },
};

static nthw_fpga_field_init_s iic_ier_fields[] = {
	{ IIC_IER_INT0, 1, 0, 0 }, { IIC_IER_INT1, 1, 1, 0 }, { IIC_IER_INT2, 1, 2, 0 },
	{ IIC_IER_INT3, 1, 3, 0 }, { IIC_IER_INT4, 1, 4, 0 }, { IIC_IER_INT5, 1, 5, 0 },
	{ IIC_IER_INT6, 1, 6, 0 }, { IIC_IER_INT7, 1, 7, 0 },
};

static nthw_fpga_field_init_s iic_isr_fields[] = {
	{ IIC_ISR_INT0, 1, 0, 0 }, { IIC_ISR_INT1, 1, 1, 0 }, { IIC_ISR_INT2, 1, 2, 0 },
	{ IIC_ISR_INT3, 1, 3, 0 }, { IIC_ISR_INT4, 1, 4, 0 }, { IIC_ISR_INT5, 1, 5, 0 },
	{ IIC_ISR_INT6, 1, 6, 0 }, { IIC_ISR_INT7, 1, 7, 0 },
};

static nthw_fpga_field_init_s iic_rx_fifo_fields[] = {
	{ IIC_RX_FIFO_RXDATA, 8, 0, 0 },
};

static nthw_fpga_field_init_s iic_rx_fifo_ocy_fields[] = {
	{ IIC_RX_FIFO_OCY_OCY_VAL, 4, 0, 0 },
};

static nthw_fpga_field_init_s iic_rx_fifo_pirq_fields[] = {
	{ IIC_RX_FIFO_PIRQ_CMP_VAL, 4, 0, 0 },
};

static nthw_fpga_field_init_s iic_softr_fields[] = {
	{ IIC_SOFTR_RKEY, 4, 0, 0x0000 },
};

static nthw_fpga_field_init_s iic_sr_fields[] = {
	{ IIC_SR_AAS, 1, 1, 0 }, { IIC_SR_ABGC, 1, 0, 0 }, { IIC_SR_BB, 1, 2, 0 },
	{ IIC_SR_RXFIFO_EMPTY, 1, 6, 1 }, { IIC_SR_RXFIFO_FULL, 1, 5, 0 }, { IIC_SR_SRW, 1, 3, 0 },
	{ IIC_SR_TXFIFO_EMPTY, 1, 7, 1 }, { IIC_SR_TXFIFO_FULL, 1, 4, 0 },
};

static nthw_fpga_field_init_s iic_tbuf_fields[] = {
	{ IIC_TBUF_TBUF_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_ten_adr_fields[] = {
	{ IIC_TEN_ADR_MSB_SLV_ADR, 3, 0, 0 },
};

static nthw_fpga_field_init_s iic_thddat_fields[] = {
	{ IIC_THDDAT_THDDAT_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_thdsta_fields[] = {
	{ IIC_THDSTA_THDSTA_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_thigh_fields[] = {
	{ IIC_THIGH_THIGH_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_tlow_fields[] = {
	{ IIC_TLOW_TLOW_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_tsudat_fields[] = {
	{ IIC_TSUDAT_TSUDAT_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_tsusta_fields[] = {
	{ IIC_TSUSTA_TSUSTA_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_tsusto_fields[] = {
	{ IIC_TSUSTO_TSUSTO_VAL, 32, 0, 0 },
};

static nthw_fpga_field_init_s iic_tx_fifo_fields[] = {
	{ IIC_TX_FIFO_START, 1, 8, 0 },
	{ IIC_TX_FIFO_STOP, 1, 9, 0 },
	{ IIC_TX_FIFO_TXDATA, 8, 0, 0 },
};

static nthw_fpga_field_init_s iic_tx_fifo_ocy_fields[] = {
	{ IIC_TX_FIFO_OCY_OCY_VAL, 4, 0, 0 },
};

static nthw_fpga_register_init_s iic_registers[] = {
	{ IIC_ADR, 68, 8, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_adr_fields },
	{ IIC_CR, 64, 8, NTHW_FPGA_REG_TYPE_RW, 0, 8, iic_cr_fields },
	{ IIC_DGIE, 7, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_dgie_fields },
	{ IIC_GPO, 73, 1, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_gpo_fields },
	{ IIC_IER, 10, 8, NTHW_FPGA_REG_TYPE_RW, 0, 8, iic_ier_fields },
	{ IIC_ISR, 8, 8, NTHW_FPGA_REG_TYPE_RW, 0, 8, iic_isr_fields },
	{ IIC_RX_FIFO, 67, 8, NTHW_FPGA_REG_TYPE_RO, 0, 1, iic_rx_fifo_fields },
	{ IIC_RX_FIFO_OCY, 70, 4, NTHW_FPGA_REG_TYPE_RO, 0, 1, iic_rx_fifo_ocy_fields },
	{ IIC_RX_FIFO_PIRQ, 72, 4, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_rx_fifo_pirq_fields },
	{ IIC_SOFTR, 16, 4, NTHW_FPGA_REG_TYPE_WO, 0, 1, iic_softr_fields },
	{ IIC_SR, 65, 8, NTHW_FPGA_REG_TYPE_RO, 192, 8, iic_sr_fields },
	{ IIC_TBUF, 78, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_tbuf_fields },
	{ IIC_TEN_ADR, 71, 3, NTHW_FPGA_REG_TYPE_RO, 0, 1, iic_ten_adr_fields },
	{ IIC_THDDAT, 81, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_thddat_fields },
	{ IIC_THDSTA, 76, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_thdsta_fields },
	{ IIC_THIGH, 79, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_thigh_fields },
	{ IIC_TLOW, 80, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_tlow_fields },
	{ IIC_TSUDAT, 77, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_tsudat_fields },
	{ IIC_TSUSTA, 74, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_tsusta_fields },
	{ IIC_TSUSTO, 75, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, iic_tsusto_fields },
	{ IIC_TX_FIFO, 66, 10, NTHW_FPGA_REG_TYPE_WO, 0, 3, iic_tx_fifo_fields },
	{ IIC_TX_FIFO_OCY, 69, 4, NTHW_FPGA_REG_TYPE_RO, 0, 1, iic_tx_fifo_ocy_fields },
};

static nthw_fpga_field_init_s mac_pcs_bad_code_fields[] = {
	{ MAC_PCS_BAD_CODE_CODE_ERR, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_bip_err_fields[] = {
	{ MAC_PCS_BIP_ERR_BIP_ERR, 640, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_block_lock_fields[] = {
	{ MAC_PCS_BLOCK_LOCK_LOCK, 20, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_block_lock_chg_fields[] = {
	{ MAC_PCS_BLOCK_LOCK_CHG_LOCK_CHG, 20, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_debounce_ctrl_fields[] = {
	{ MAC_PCS_DEBOUNCE_CTRL_NT_DEBOUNCE_LATENCY, 8, 8, 10 },
	{ MAC_PCS_DEBOUNCE_CTRL_NT_FORCE_LINK_DOWN, 1, 16, 0 },
	{ MAC_PCS_DEBOUNCE_CTRL_NT_LINKUP_LATENCY, 8, 0, 10 },
	{ MAC_PCS_DEBOUNCE_CTRL_NT_PORT_CTRL, 2, 17, 2 },
};

static nthw_fpga_field_init_s mac_pcs_drp_ctrl_fields[] = {
	{ MAC_PCS_DRP_CTRL_ADR, 10, 16, 0 }, { MAC_PCS_DRP_CTRL_DATA, 16, 0, 0 },
	{ MAC_PCS_DRP_CTRL_DBG_BUSY, 1, 30, 0x0000 }, { MAC_PCS_DRP_CTRL_DONE, 1, 31, 0x0000 },
	{ MAC_PCS_DRP_CTRL_MOD_ADR, 3, 26, 0 }, { MAC_PCS_DRP_CTRL_WREN, 1, 29, 0 },
};

static nthw_fpga_field_init_s mac_pcs_fec_ctrl_fields[] = {
	{ MAC_PCS_FEC_CTRL_RS_FEC_CTRL_IN, 5, 0, 0 },
};

static nthw_fpga_field_init_s mac_pcs_fec_cw_cnt_fields[] = {
	{ MAC_PCS_FEC_CW_CNT_CW_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_err_cnt_0_fields[] = {
	{ MAC_PCS_FEC_ERR_CNT_0_ERR_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_err_cnt_1_fields[] = {
	{ MAC_PCS_FEC_ERR_CNT_1_ERR_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_err_cnt_2_fields[] = {
	{ MAC_PCS_FEC_ERR_CNT_2_ERR_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_err_cnt_3_fields[] = {
	{ MAC_PCS_FEC_ERR_CNT_3_ERR_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_lane_dly_0_fields[] = {
	{ MAC_PCS_FEC_LANE_DLY_0_DLY, 14, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_lane_dly_1_fields[] = {
	{ MAC_PCS_FEC_LANE_DLY_1_DLY, 14, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_lane_dly_2_fields[] = {
	{ MAC_PCS_FEC_LANE_DLY_2_DLY, 14, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_lane_dly_3_fields[] = {
	{ MAC_PCS_FEC_LANE_DLY_3_DLY, 14, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_lane_map_fields[] = {
	{ MAC_PCS_FEC_LANE_MAP_MAPPING, 8, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_stat_fields[] = {
	{ MAC_PCS_FEC_STAT_AM_LOCK, 1, 10, 0x0000 },
	{ MAC_PCS_FEC_STAT_AM_LOCK_0, 1, 3, 0x0000 },
	{ MAC_PCS_FEC_STAT_AM_LOCK_1, 1, 4, 0x0000 },
	{ MAC_PCS_FEC_STAT_AM_LOCK_2, 1, 5, 0x0000 },
	{ MAC_PCS_FEC_STAT_AM_LOCK_3, 1, 6, 0x0000 },
	{ MAC_PCS_FEC_STAT_BLOCK_LOCK, 1, 9, 0x0000 },
	{ MAC_PCS_FEC_STAT_BYPASS, 1, 0, 0x0000 },
	{ MAC_PCS_FEC_STAT_FEC_LANE_ALGN, 1, 7, 0x0000 },
	{ MAC_PCS_FEC_STAT_HI_SER, 1, 2, 0x0000 },
	{ MAC_PCS_FEC_STAT_PCS_LANE_ALGN, 1, 8, 0x0000 },
	{ MAC_PCS_FEC_STAT_VALID, 1, 1, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_fec_ucw_cnt_fields[] = {
	{ MAC_PCS_FEC_UCW_CNT_UCW_CNT, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_gty_ctl_rx_fields[] = {
	{ MAC_PCS_GTY_CTL_RX_CDR_HOLD_0, 1, 24, 0 }, { MAC_PCS_GTY_CTL_RX_CDR_HOLD_1, 1, 25, 0 },
	{ MAC_PCS_GTY_CTL_RX_CDR_HOLD_2, 1, 26, 0 }, { MAC_PCS_GTY_CTL_RX_CDR_HOLD_3, 1, 27, 0 },
	{ MAC_PCS_GTY_CTL_RX_EQUA_RST_0, 1, 20, 0 }, { MAC_PCS_GTY_CTL_RX_EQUA_RST_1, 1, 21, 0 },
	{ MAC_PCS_GTY_CTL_RX_EQUA_RST_2, 1, 22, 0 }, { MAC_PCS_GTY_CTL_RX_EQUA_RST_3, 1, 23, 0 },
	{ MAC_PCS_GTY_CTL_RX_LPM_EN_0, 1, 16, 0 }, { MAC_PCS_GTY_CTL_RX_LPM_EN_1, 1, 17, 0 },
	{ MAC_PCS_GTY_CTL_RX_LPM_EN_2, 1, 18, 0 }, { MAC_PCS_GTY_CTL_RX_LPM_EN_3, 1, 19, 0 },
	{ MAC_PCS_GTY_CTL_RX_POLARITY_0, 1, 0, 0 }, { MAC_PCS_GTY_CTL_RX_POLARITY_1, 1, 1, 0 },
	{ MAC_PCS_GTY_CTL_RX_POLARITY_2, 1, 2, 0 }, { MAC_PCS_GTY_CTL_RX_POLARITY_3, 1, 3, 0 },
	{ MAC_PCS_GTY_CTL_RX_RATE_0, 3, 4, 0 }, { MAC_PCS_GTY_CTL_RX_RATE_1, 3, 7, 0 },
	{ MAC_PCS_GTY_CTL_RX_RATE_2, 3, 10, 0 }, { MAC_PCS_GTY_CTL_RX_RATE_3, 3, 13, 0 },
};

static nthw_fpga_field_init_s mac_pcs_gty_ctl_tx_fields[] = {
	{ MAC_PCS_GTY_CTL_TX_INHIBIT_0, 1, 4, 0 }, { MAC_PCS_GTY_CTL_TX_INHIBIT_1, 1, 5, 0 },
	{ MAC_PCS_GTY_CTL_TX_INHIBIT_2, 1, 6, 0 }, { MAC_PCS_GTY_CTL_TX_INHIBIT_3, 1, 7, 0 },
	{ MAC_PCS_GTY_CTL_TX_POLARITY_0, 1, 0, 0 }, { MAC_PCS_GTY_CTL_TX_POLARITY_1, 1, 1, 0 },
	{ MAC_PCS_GTY_CTL_TX_POLARITY_2, 1, 2, 0 }, { MAC_PCS_GTY_CTL_TX_POLARITY_3, 1, 3, 0 },
};

static nthw_fpga_field_init_s mac_pcs_gty_diff_ctl_fields[] = {
	{ MAC_PCS_GTY_DIFF_CTL_TX_DIFF_CTL_0, 5, 0, 24 },
	{ MAC_PCS_GTY_DIFF_CTL_TX_DIFF_CTL_1, 5, 5, 24 },
	{ MAC_PCS_GTY_DIFF_CTL_TX_DIFF_CTL_2, 5, 10, 24 },
	{ MAC_PCS_GTY_DIFF_CTL_TX_DIFF_CTL_3, 5, 15, 24 },
};

static nthw_fpga_field_init_s mac_pcs_gty_loop_fields[] = {
	{ MAC_PCS_GTY_LOOP_GT_LOOP_0, 3, 0, 0 },
	{ MAC_PCS_GTY_LOOP_GT_LOOP_1, 3, 3, 0 },
	{ MAC_PCS_GTY_LOOP_GT_LOOP_2, 3, 6, 0 },
	{ MAC_PCS_GTY_LOOP_GT_LOOP_3, 3, 9, 0 },
};

static nthw_fpga_field_init_s mac_pcs_gty_post_cursor_fields[] = {
	{ MAC_PCS_GTY_POST_CURSOR_TX_POST_CSR_0, 5, 0, 20 },
	{ MAC_PCS_GTY_POST_CURSOR_TX_POST_CSR_1, 5, 5, 20 },
	{ MAC_PCS_GTY_POST_CURSOR_TX_POST_CSR_2, 5, 10, 20 },
	{ MAC_PCS_GTY_POST_CURSOR_TX_POST_CSR_3, 5, 15, 20 },
};

static nthw_fpga_field_init_s mac_pcs_gty_prbs_sel_fields[] = {
	{ MAC_PCS_GTY_PRBS_SEL_RX_PRBS_SEL_0, 4, 16, 0 },
	{ MAC_PCS_GTY_PRBS_SEL_RX_PRBS_SEL_1, 4, 20, 0 },
	{ MAC_PCS_GTY_PRBS_SEL_RX_PRBS_SEL_2, 4, 24, 0 },
	{ MAC_PCS_GTY_PRBS_SEL_RX_PRBS_SEL_3, 4, 28, 0 },
	{ MAC_PCS_GTY_PRBS_SEL_TX_PRBS_SEL_0, 4, 0, 0 },
	{ MAC_PCS_GTY_PRBS_SEL_TX_PRBS_SEL_1, 4, 4, 0 },
	{ MAC_PCS_GTY_PRBS_SEL_TX_PRBS_SEL_2, 4, 8, 0 },
	{ MAC_PCS_GTY_PRBS_SEL_TX_PRBS_SEL_3, 4, 12, 0 },
};

static nthw_fpga_field_init_s mac_pcs_gty_pre_cursor_fields[] = {
	{ MAC_PCS_GTY_PRE_CURSOR_TX_PRE_CSR_0, 5, 0, 0 },
	{ MAC_PCS_GTY_PRE_CURSOR_TX_PRE_CSR_1, 5, 5, 0 },
	{ MAC_PCS_GTY_PRE_CURSOR_TX_PRE_CSR_2, 5, 10, 0 },
	{ MAC_PCS_GTY_PRE_CURSOR_TX_PRE_CSR_3, 5, 15, 0 },
};

static nthw_fpga_field_init_s mac_pcs_gty_rx_buf_stat_fields[] = {
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_0, 3, 0, 0x0000 },
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_1, 3, 3, 0x0000 },
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_2, 3, 6, 0x0000 },
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_3, 3, 9, 0x0000 },
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_CHANGED_0, 3, 12, 0x0000 },
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_CHANGED_1, 3, 15, 0x0000 },
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_CHANGED_2, 3, 18, 0x0000 },
	{ MAC_PCS_GTY_RX_BUF_STAT_RX_BUF_STAT_CHANGED_3, 3, 21, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_gty_scan_ctl_fields[] = {
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_RST_0, 1, 0, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_RST_1, 1, 1, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_RST_2, 1, 2, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_RST_3, 1, 3, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_TRG_0, 1, 4, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_TRG_1, 1, 5, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_TRG_2, 1, 6, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_EYE_SCAN_TRG_3, 1, 7, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_ERR_INS_0, 1, 12, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_ERR_INS_1, 1, 13, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_ERR_INS_2, 1, 14, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_ERR_INS_3, 1, 15, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_RST_0, 1, 8, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_RST_1, 1, 9, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_RST_2, 1, 10, 0 },
	{ MAC_PCS_GTY_SCAN_CTL_PRBS_RST_3, 1, 11, 0 },
};

static nthw_fpga_field_init_s mac_pcs_gty_scan_stat_fields[] = {
	{ MAC_PCS_GTY_SCAN_STAT_EYE_SCAN_ERR_0, 1, 0, 0x0000 },
	{ MAC_PCS_GTY_SCAN_STAT_EYE_SCAN_ERR_1, 1, 1, 0x0000 },
	{ MAC_PCS_GTY_SCAN_STAT_EYE_SCAN_ERR_2, 1, 2, 0x0000 },
	{ MAC_PCS_GTY_SCAN_STAT_EYE_SCAN_ERR_3, 1, 3, 0x0000 },
	{ MAC_PCS_GTY_SCAN_STAT_PRBS_ERR_0, 1, 4, 0x0000 },
	{ MAC_PCS_GTY_SCAN_STAT_PRBS_ERR_1, 1, 5, 0x0000 },
	{ MAC_PCS_GTY_SCAN_STAT_PRBS_ERR_2, 1, 6, 0x0000 },
	{ MAC_PCS_GTY_SCAN_STAT_PRBS_ERR_3, 1, 7, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_gty_stat_fields[] = {
	{ MAC_PCS_GTY_STAT_RX_RST_DONE_0, 1, 4, 0x0000 },
	{ MAC_PCS_GTY_STAT_RX_RST_DONE_1, 1, 5, 0x0000 },
	{ MAC_PCS_GTY_STAT_RX_RST_DONE_2, 1, 6, 0x0000 },
	{ MAC_PCS_GTY_STAT_RX_RST_DONE_3, 1, 7, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_BUF_STAT_0, 2, 8, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_BUF_STAT_1, 2, 10, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_BUF_STAT_2, 2, 12, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_BUF_STAT_3, 2, 14, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_RST_DONE_0, 1, 0, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_RST_DONE_1, 1, 1, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_RST_DONE_2, 1, 2, 0x0000 },
	{ MAC_PCS_GTY_STAT_TX_RST_DONE_3, 1, 3, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_link_summary_fields[] = {
	{ MAC_PCS_LINK_SUMMARY_ABS, 1, 0, 0x0000 },
	{ MAC_PCS_LINK_SUMMARY_LH_ABS, 1, 2, 0x0000 },
	{ MAC_PCS_LINK_SUMMARY_LH_LOCAL_FAULT, 1, 13, 0 },
	{ MAC_PCS_LINK_SUMMARY_LH_REMOTE_FAULT, 1, 14, 0 },
	{ MAC_PCS_LINK_SUMMARY_LINK_DOWN_CNT, 8, 4, 0 },
	{ MAC_PCS_LINK_SUMMARY_LL_PHY_LINK_STATE, 1, 3, 0x0000 },
	{ MAC_PCS_LINK_SUMMARY_LOCAL_FAULT, 1, 17, 0x0000 },
	{ MAC_PCS_LINK_SUMMARY_NIM_INTERR, 1, 12, 0x0000 },
	{ MAC_PCS_LINK_SUMMARY_NT_PHY_LINK_STATE, 1, 1, 0x0000 },
	{ MAC_PCS_LINK_SUMMARY_REMOTE_FAULT, 1, 18, 0x0000 },
	{ MAC_PCS_LINK_SUMMARY_RESERVED, 2, 15, 0 },
};

static nthw_fpga_field_init_s mac_pcs_mac_pcs_config_fields[] = {
	{ MAC_PCS_MAC_PCS_CONFIG_RX_CORE_RST, 1, 3, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_RX_ENABLE, 1, 5, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_RX_FORCE_RESYNC, 1, 6, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_RX_PATH_RST, 1, 1, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_RX_TEST_PATTERN, 1, 7, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_TX_CORE_RST, 1, 2, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_TX_ENABLE, 1, 8, 1 },
	{ MAC_PCS_MAC_PCS_CONFIG_TX_FCS_REMOVE, 1, 4, 1 },
	{ MAC_PCS_MAC_PCS_CONFIG_TX_PATH_RST, 1, 0, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_TX_SEND_IDLE, 1, 9, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_TX_SEND_RFI, 1, 10, 0 },
	{ MAC_PCS_MAC_PCS_CONFIG_TX_TEST_PATTERN, 1, 11, 0 },
};

static nthw_fpga_field_init_s mac_pcs_max_pkt_len_fields[] = {
	{ MAC_PCS_MAX_PKT_LEN_MAX_LEN, 14, 0, 10000 },
};

static nthw_fpga_field_init_s mac_pcs_phymac_misc_fields[] = {
	{ MAC_PCS_PHYMAC_MISC_TS_EOP, 1, 3, 1 },
	{ MAC_PCS_PHYMAC_MISC_TX_MUX_STATE, 4, 4, 0x0000 },
	{ MAC_PCS_PHYMAC_MISC_TX_SEL_HOST, 1, 0, 1 },
	{ MAC_PCS_PHYMAC_MISC_TX_SEL_RX_LOOP, 1, 2, 0 },
	{ MAC_PCS_PHYMAC_MISC_TX_SEL_TFG, 1, 1, 0 },
};

static nthw_fpga_field_init_s mac_pcs_phy_stat_fields[] = {
	{ MAC_PCS_PHY_STAT_ALARM, 1, 2, 0x0000 },
	{ MAC_PCS_PHY_STAT_MOD_PRS, 1, 1, 0x0000 },
	{ MAC_PCS_PHY_STAT_RX_LOS, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_stat_pcs_rx_fields[] = {
	{ MAC_PCS_STAT_PCS_RX_ALIGNED, 1, 1, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_ALIGNED_ERR, 1, 2, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_GOT_SIGNAL_OS, 1, 9, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_HI_BER, 1, 8, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_INTERNAL_LOCAL_FAULT, 1, 4, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LOCAL_FAULT, 1, 6, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_MISALIGNED, 1, 3, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_RECEIVED_LOCAL_FAULT, 1, 5, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_REMOTE_FAULT, 1, 7, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_STATUS, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_stat_pcs_rx_latch_fields[] = {
	{ MAC_PCS_STAT_PCS_RX_LATCH_ALIGNED, 1, 1, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_ALIGNED_ERR, 1, 2, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_GOT_SIGNAL_OS, 1, 9, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_HI_BER, 1, 8, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_INTERNAL_LOCAL_FAULT, 1, 4, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_LOCAL_FAULT, 1, 6, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_MISALIGNED, 1, 3, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_RECEIVED_LOCAL_FAULT, 1, 5, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_REMOTE_FAULT, 1, 7, 0x0000 },
	{ MAC_PCS_STAT_PCS_RX_LATCH_STATUS, 1, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_stat_pcs_tx_fields[] = {
	{ MAC_PCS_STAT_PCS_TX_LOCAL_FAULT, 1, 0, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_LOCAL_FAULT_CHANGED, 1, 5, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_PTP_FIFO_READ_ERROR, 1, 4, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_PTP_FIFO_READ_ERROR_CHANGED, 1, 9, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_PTP_FIFO_WRITE_ERROR, 1, 3, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_PTP_FIFO_WRITE_ERROR_CHANGED, 1, 8, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_TX_OVFOUT, 1, 2, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_TX_OVFOUT_CHANGED, 1, 7, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_TX_UNFOUT, 1, 1, 0x0000 },
	{ MAC_PCS_STAT_PCS_TX_TX_UNFOUT_CHANGED, 1, 6, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_synced_fields[] = {
	{ MAC_PCS_SYNCED_SYNC, 20, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_synced_err_fields[] = {
	{ MAC_PCS_SYNCED_ERR_SYNC_ERROR, 20, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_test_err_fields[] = {
	{ MAC_PCS_TEST_ERR_CODE_ERR, 16, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_timestamp_comp_fields[] = {
	{ MAC_PCS_TIMESTAMP_COMP_RX_DLY, 16, 0, 1451 },
	{ MAC_PCS_TIMESTAMP_COMP_TX_DLY, 16, 16, 1440 },
};

static nthw_fpga_field_init_s mac_pcs_vl_demuxed_fields[] = {
	{ MAC_PCS_VL_DEMUXED_LOCK, 20, 0, 0x0000 },
};

static nthw_fpga_field_init_s mac_pcs_vl_demuxed_chg_fields[] = {
	{ MAC_PCS_VL_DEMUXED_CHG_LOCK_CHG, 20, 0, 0x0000 },
};

static nthw_fpga_register_init_s mac_pcs_registers[] = {
	{ MAC_PCS_BAD_CODE, 26, 16, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_bad_code_fields },
	{ MAC_PCS_BIP_ERR, 31, 640, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_bip_err_fields },
	{ MAC_PCS_BLOCK_LOCK, 27, 20, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_block_lock_fields },
	{
		MAC_PCS_BLOCK_LOCK_CHG, 28, 20, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_block_lock_chg_fields
	},
	{
		MAC_PCS_DEBOUNCE_CTRL, 1, 19, NTHW_FPGA_REG_TYPE_RW, 264714, 4,
		mac_pcs_debounce_ctrl_fields
	},
	{ MAC_PCS_DRP_CTRL, 43, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 6, mac_pcs_drp_ctrl_fields },
	{ MAC_PCS_FEC_CTRL, 2, 5, NTHW_FPGA_REG_TYPE_RW, 0, 1, mac_pcs_fec_ctrl_fields },
	{ MAC_PCS_FEC_CW_CNT, 9, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_fec_cw_cnt_fields },
	{
		MAC_PCS_FEC_ERR_CNT_0, 11, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_err_cnt_0_fields
	},
	{
		MAC_PCS_FEC_ERR_CNT_1, 12, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_err_cnt_1_fields
	},
	{
		MAC_PCS_FEC_ERR_CNT_2, 13, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_err_cnt_2_fields
	},
	{
		MAC_PCS_FEC_ERR_CNT_3, 14, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_err_cnt_3_fields
	},
	{
		MAC_PCS_FEC_LANE_DLY_0, 5, 14, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_lane_dly_0_fields
	},
	{
		MAC_PCS_FEC_LANE_DLY_1, 6, 14, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_lane_dly_1_fields
	},
	{
		MAC_PCS_FEC_LANE_DLY_2, 7, 14, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_lane_dly_2_fields
	},
	{
		MAC_PCS_FEC_LANE_DLY_3, 8, 14, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_fec_lane_dly_3_fields
	},
	{ MAC_PCS_FEC_LANE_MAP, 4, 8, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_fec_lane_map_fields },
	{ MAC_PCS_FEC_STAT, 3, 11, NTHW_FPGA_REG_TYPE_RO, 0, 11, mac_pcs_fec_stat_fields },
	{ MAC_PCS_FEC_UCW_CNT, 10, 32, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_fec_ucw_cnt_fields },
	{ MAC_PCS_GTY_CTL_RX, 38, 28, NTHW_FPGA_REG_TYPE_RW, 0, 20, mac_pcs_gty_ctl_rx_fields },
	{ MAC_PCS_GTY_CTL_TX, 39, 8, NTHW_FPGA_REG_TYPE_RW, 0, 8, mac_pcs_gty_ctl_tx_fields },
	{
		MAC_PCS_GTY_DIFF_CTL, 35, 20, NTHW_FPGA_REG_TYPE_RW, 811800, 4,
		mac_pcs_gty_diff_ctl_fields
	},
	{ MAC_PCS_GTY_LOOP, 20, 12, NTHW_FPGA_REG_TYPE_RW, 0, 4, mac_pcs_gty_loop_fields },
	{
		MAC_PCS_GTY_POST_CURSOR, 36, 20, NTHW_FPGA_REG_TYPE_RW, 676500, 4,
		mac_pcs_gty_post_cursor_fields
	},
	{ MAC_PCS_GTY_PRBS_SEL, 40, 32, NTHW_FPGA_REG_TYPE_RW, 0, 8, mac_pcs_gty_prbs_sel_fields },
	{
		MAC_PCS_GTY_PRE_CURSOR, 37, 20, NTHW_FPGA_REG_TYPE_RW, 0, 4,
		mac_pcs_gty_pre_cursor_fields
	},
	{
		MAC_PCS_GTY_RX_BUF_STAT, 34, 24, NTHW_FPGA_REG_TYPE_RO, 0, 8,
		mac_pcs_gty_rx_buf_stat_fields
	},
	{
		MAC_PCS_GTY_SCAN_CTL, 41, 16, NTHW_FPGA_REG_TYPE_RW, 0, 16,
		mac_pcs_gty_scan_ctl_fields
	},
	{
		MAC_PCS_GTY_SCAN_STAT, 42, 8, NTHW_FPGA_REG_TYPE_RO, 0, 8,
		mac_pcs_gty_scan_stat_fields
	},
	{ MAC_PCS_GTY_STAT, 33, 16, NTHW_FPGA_REG_TYPE_RO, 0, 12, mac_pcs_gty_stat_fields },
	{ MAC_PCS_LINK_SUMMARY, 0, 19, NTHW_FPGA_REG_TYPE_RO, 0, 11, mac_pcs_link_summary_fields },
	{
		MAC_PCS_MAC_PCS_CONFIG, 19, 12, NTHW_FPGA_REG_TYPE_RW, 272, 12,
		mac_pcs_mac_pcs_config_fields
	},
	{
		MAC_PCS_MAX_PKT_LEN, 17, 14, NTHW_FPGA_REG_TYPE_RW, 10000, 1,
		mac_pcs_max_pkt_len_fields
	},
	{ MAC_PCS_PHYMAC_MISC, 16, 8, NTHW_FPGA_REG_TYPE_MIXED, 9, 5, mac_pcs_phymac_misc_fields },
	{ MAC_PCS_PHY_STAT, 15, 3, NTHW_FPGA_REG_TYPE_RO, 0, 3, mac_pcs_phy_stat_fields },
	{ MAC_PCS_STAT_PCS_RX, 21, 10, NTHW_FPGA_REG_TYPE_RO, 0, 10, mac_pcs_stat_pcs_rx_fields },
	{
		MAC_PCS_STAT_PCS_RX_LATCH, 22, 10, NTHW_FPGA_REG_TYPE_RO, 0, 10,
		mac_pcs_stat_pcs_rx_latch_fields
	},
	{ MAC_PCS_STAT_PCS_TX, 23, 10, NTHW_FPGA_REG_TYPE_RO, 0, 10, mac_pcs_stat_pcs_tx_fields },
	{ MAC_PCS_SYNCED, 24, 20, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_synced_fields },
	{ MAC_PCS_SYNCED_ERR, 25, 20, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_synced_err_fields },
	{ MAC_PCS_TEST_ERR, 32, 16, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_test_err_fields },
	{
		MAC_PCS_TIMESTAMP_COMP, 18, 32, NTHW_FPGA_REG_TYPE_RW, 94373291, 2,
		mac_pcs_timestamp_comp_fields
	},
	{ MAC_PCS_VL_DEMUXED, 29, 20, NTHW_FPGA_REG_TYPE_RO, 0, 1, mac_pcs_vl_demuxed_fields },
	{
		MAC_PCS_VL_DEMUXED_CHG, 30, 20, NTHW_FPGA_REG_TYPE_RO, 0, 1,
		mac_pcs_vl_demuxed_chg_fields
	},
};

static nthw_fpga_field_init_s pci_rd_tg_tg_ctrl_fields[] = {
	{ PCI_RD_TG_TG_CTRL_TG_RD_RDY, 1, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rdaddr_fields[] = {
	{ PCI_RD_TG_TG_RDADDR_RAM_ADDR, 9, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rddata0_fields[] = {
	{ PCI_RD_TG_TG_RDDATA0_PHYS_ADDR_LOW, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rddata1_fields[] = {
	{ PCI_RD_TG_TG_RDDATA1_PHYS_ADDR_HIGH, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rddata2_fields[] = {
	{ PCI_RD_TG_TG_RDDATA2_REQ_HID, 6, 22, 0 },
	{ PCI_RD_TG_TG_RDDATA2_REQ_SIZE, 22, 0, 0 },
	{ PCI_RD_TG_TG_RDDATA2_WAIT, 1, 30, 0 },
	{ PCI_RD_TG_TG_RDDATA2_WRAP, 1, 31, 0 },
};

static nthw_fpga_field_init_s pci_rd_tg_tg_rd_run_fields[] = {
	{ PCI_RD_TG_TG_RD_RUN_RD_ITERATION, 16, 0, 0 },
};

static nthw_fpga_register_init_s pci_rd_tg_registers[] = {
	{ PCI_RD_TG_TG_CTRL, 5, 1, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_rd_tg_tg_ctrl_fields },
	{ PCI_RD_TG_TG_RDADDR, 3, 9, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rdaddr_fields },
	{ PCI_RD_TG_TG_RDDATA0, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rddata0_fields },
	{ PCI_RD_TG_TG_RDDATA1, 1, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rddata1_fields },
	{ PCI_RD_TG_TG_RDDATA2, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 4, pci_rd_tg_tg_rddata2_fields },
	{ PCI_RD_TG_TG_RD_RUN, 4, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_rd_tg_tg_rd_run_fields },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_ctrl_fields[] = {
	{ PCI_WR_TG_TG_CTRL_TG_WR_RDY, 1, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_seq_fields[] = {
	{ PCI_WR_TG_TG_SEQ_SEQUENCE, 16, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wraddr_fields[] = {
	{ PCI_WR_TG_TG_WRADDR_RAM_ADDR, 9, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wrdata0_fields[] = {
	{ PCI_WR_TG_TG_WRDATA0_PHYS_ADDR_LOW, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wrdata1_fields[] = {
	{ PCI_WR_TG_TG_WRDATA1_PHYS_ADDR_HIGH, 32, 0, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wrdata2_fields[] = {
	{ PCI_WR_TG_TG_WRDATA2_INC_MODE, 1, 29, 0 }, { PCI_WR_TG_TG_WRDATA2_REQ_HID, 6, 22, 0 },
	{ PCI_WR_TG_TG_WRDATA2_REQ_SIZE, 22, 0, 0 }, { PCI_WR_TG_TG_WRDATA2_WAIT, 1, 30, 0 },
	{ PCI_WR_TG_TG_WRDATA2_WRAP, 1, 31, 0 },
};

static nthw_fpga_field_init_s pci_wr_tg_tg_wr_run_fields[] = {
	{ PCI_WR_TG_TG_WR_RUN_WR_ITERATION, 16, 0, 0 },
};

static nthw_fpga_register_init_s pci_wr_tg_registers[] = {
	{ PCI_WR_TG_TG_CTRL, 5, 1, NTHW_FPGA_REG_TYPE_RO, 0, 1, pci_wr_tg_tg_ctrl_fields },
	{ PCI_WR_TG_TG_SEQ, 6, 16, NTHW_FPGA_REG_TYPE_RW, 0, 1, pci_wr_tg_tg_seq_fields },
	{ PCI_WR_TG_TG_WRADDR, 3, 9, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wraddr_fields },
	{ PCI_WR_TG_TG_WRDATA0, 0, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wrdata0_fields },
	{ PCI_WR_TG_TG_WRDATA1, 1, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wrdata1_fields },
	{ PCI_WR_TG_TG_WRDATA2, 2, 32, NTHW_FPGA_REG_TYPE_WO, 0, 5, pci_wr_tg_tg_wrdata2_fields },
	{ PCI_WR_TG_TG_WR_RUN, 4, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, pci_wr_tg_tg_wr_run_fields },
};

static nthw_fpga_field_init_s rac_dbg_ctrl_fields[] = {
	{ RAC_DBG_CTRL_C, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s rac_dbg_data_fields[] = {
	{ RAC_DBG_DATA_D, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s rac_rab_buf_free_fields[] = {
	{ RAC_RAB_BUF_FREE_IB_FREE, 9, 0, 511 }, { RAC_RAB_BUF_FREE_IB_OVF, 1, 12, 0 },
	{ RAC_RAB_BUF_FREE_OB_FREE, 9, 16, 511 }, { RAC_RAB_BUF_FREE_OB_OVF, 1, 28, 0 },
	{ RAC_RAB_BUF_FREE_TIMEOUT, 1, 31, 0 },
};

static nthw_fpga_field_init_s rac_rab_buf_used_fields[] = {
	{ RAC_RAB_BUF_USED_FLUSH, 1, 31, 0 },
	{ RAC_RAB_BUF_USED_IB_USED, 9, 0, 0 },
	{ RAC_RAB_BUF_USED_OB_USED, 9, 16, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_hi_fields[] = {
	{ RAC_RAB_DMA_IB_HI_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_lo_fields[] = {
	{ RAC_RAB_DMA_IB_LO_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_rd_fields[] = {
	{ RAC_RAB_DMA_IB_RD_PTR, 16, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ib_wr_fields[] = {
	{ RAC_RAB_DMA_IB_WR_PTR, 16, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ob_hi_fields[] = {
	{ RAC_RAB_DMA_OB_HI_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ob_lo_fields[] = {
	{ RAC_RAB_DMA_OB_LO_PHYADDR, 32, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_dma_ob_wr_fields[] = {
	{ RAC_RAB_DMA_OB_WR_PTR, 16, 0, 0 },
};

static nthw_fpga_field_init_s rac_rab_ib_data_fields[] = {
	{ RAC_RAB_IB_DATA_D, 32, 0, 0x0000 },
};

static nthw_fpga_field_init_s rac_rab_init_fields[] = {
	{ RAC_RAB_INIT_RAB, 3, 0, 7 },
};

static nthw_fpga_field_init_s rac_rab_ob_data_fields[] = {
	{ RAC_RAB_OB_DATA_D, 32, 0, 0x0000 },
};

static nthw_fpga_register_init_s rac_registers[] = {
	{ RAC_DBG_CTRL, 4200, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, rac_dbg_ctrl_fields },
	{ RAC_DBG_DATA, 4208, 32, NTHW_FPGA_REG_TYPE_RW, 0, 1, rac_dbg_data_fields },
	{
		RAC_RAB_BUF_FREE, 4176, 32, NTHW_FPGA_REG_TYPE_MIXED, 33489407, 5,
		rac_rab_buf_free_fields
	},
	{ RAC_RAB_BUF_USED, 4184, 32, NTHW_FPGA_REG_TYPE_MIXED, 0, 3, rac_rab_buf_used_fields },
	{ RAC_RAB_DMA_IB_HI, 4360, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ib_hi_fields },
	{ RAC_RAB_DMA_IB_LO, 4352, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ib_lo_fields },
	{ RAC_RAB_DMA_IB_RD, 4424, 16, NTHW_FPGA_REG_TYPE_RO, 0, 1, rac_rab_dma_ib_rd_fields },
	{ RAC_RAB_DMA_IB_WR, 4416, 16, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ib_wr_fields },
	{ RAC_RAB_DMA_OB_HI, 4376, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ob_hi_fields },
	{ RAC_RAB_DMA_OB_LO, 4368, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_dma_ob_lo_fields },
	{ RAC_RAB_DMA_OB_WR, 4480, 16, NTHW_FPGA_REG_TYPE_RO, 0, 1, rac_rab_dma_ob_wr_fields },
	{ RAC_RAB_IB_DATA, 4160, 32, NTHW_FPGA_REG_TYPE_WO, 0, 1, rac_rab_ib_data_fields },
	{ RAC_RAB_INIT, 4192, 3, NTHW_FPGA_REG_TYPE_RW, 7, 1, rac_rab_init_fields },
	{ RAC_RAB_OB_DATA, 4168, 32, NTHW_FPGA_REG_TYPE_RC1, 0, 1, rac_rab_ob_data_fields },
};

static nthw_fpga_field_init_s rst9563_ctrl_fields[] = {
	{ RST9563_CTRL_PTP_MMCM_CLKSEL, 1, 2, 1 },
	{ RST9563_CTRL_TS_CLKSEL, 1, 1, 1 },
	{ RST9563_CTRL_TS_CLKSEL_OVERRIDE, 1, 0, 1 },
};

static nthw_fpga_field_init_s rst9563_power_fields[] = {
	{ RST9563_POWER_PU_NSEB, 1, 1, 0 },
	{ RST9563_POWER_PU_PHY, 1, 0, 0 },
};

static nthw_fpga_field_init_s rst9563_rst_fields[] = {
	{ RST9563_RST_CORE_MMCM, 1, 15, 0 }, { RST9563_RST_DDR4, 3, 3, 7 },
	{ RST9563_RST_MAC_RX, 2, 9, 3 }, { RST9563_RST_PERIPH, 1, 13, 0 },
	{ RST9563_RST_PHY, 2, 7, 3 }, { RST9563_RST_PTP, 1, 11, 1 },
	{ RST9563_RST_PTP_MMCM, 1, 16, 0 }, { RST9563_RST_RPP, 1, 2, 1 },
	{ RST9563_RST_SDC, 1, 6, 1 }, { RST9563_RST_SYS, 1, 0, 1 },
	{ RST9563_RST_SYS_MMCM, 1, 14, 0 }, { RST9563_RST_TMC, 1, 1, 1 },
	{ RST9563_RST_TS, 1, 12, 1 }, { RST9563_RST_TS_MMCM, 1, 17, 0 },
};

static nthw_fpga_field_init_s rst9563_stat_fields[] = {
	{ RST9563_STAT_CORE_MMCM_LOCKED, 1, 5, 0x0000 },
	{ RST9563_STAT_DDR4_MMCM_LOCKED, 1, 2, 0x0000 },
	{ RST9563_STAT_DDR4_PLL_LOCKED, 1, 3, 0x0000 },
	{ RST9563_STAT_PTP_MMCM_LOCKED, 1, 0, 0x0000 },
	{ RST9563_STAT_SYS_MMCM_LOCKED, 1, 4, 0x0000 },
	{ RST9563_STAT_TS_MMCM_LOCKED, 1, 1, 0x0000 },
};

static nthw_fpga_field_init_s rst9563_sticky_fields[] = {
	{ RST9563_STICKY_CORE_MMCM_UNLOCKED, 1, 5, 0x0000 },
	{ RST9563_STICKY_DDR4_MMCM_UNLOCKED, 1, 2, 0x0000 },
	{ RST9563_STICKY_DDR4_PLL_UNLOCKED, 1, 3, 0x0000 },
	{ RST9563_STICKY_PTP_MMCM_UNLOCKED, 1, 0, 0x0000 },
	{ RST9563_STICKY_SYS_MMCM_UNLOCKED, 1, 4, 0x0000 },
	{ RST9563_STICKY_TS_MMCM_UNLOCKED, 1, 1, 0x0000 },
};

static nthw_fpga_register_init_s rst9563_registers[] = {
	{ RST9563_CTRL, 1, 3, NTHW_FPGA_REG_TYPE_RW, 7, 3, rst9563_ctrl_fields },
	{ RST9563_POWER, 4, 2, NTHW_FPGA_REG_TYPE_RW, 0, 2, rst9563_power_fields },
	{ RST9563_RST, 0, 18, NTHW_FPGA_REG_TYPE_RW, 8191, 14, rst9563_rst_fields },
	{ RST9563_STAT, 2, 6, NTHW_FPGA_REG_TYPE_RO, 0, 6, rst9563_stat_fields },
	{ RST9563_STICKY, 3, 6, NTHW_FPGA_REG_TYPE_RC1, 0, 6, rst9563_sticky_fields },
};

static nthw_fpga_module_init_s fpga_modules[] = {
	{ MOD_GFG, 0, MOD_GFG, 1, 1, NTHW_FPGA_BUS_TYPE_RAB2, 8704, 10, gfg_registers },
	{ MOD_GMF, 0, MOD_GMF, 2, 5, NTHW_FPGA_BUS_TYPE_RAB2, 9216, 12, gmf_registers },
	{ MOD_GMF, 1, MOD_GMF, 2, 5, NTHW_FPGA_BUS_TYPE_RAB2, 9728, 12, gmf_registers },
	{
		MOD_GPIO_PHY, 0, MOD_GPIO_PHY, 1, 0, NTHW_FPGA_BUS_TYPE_RAB0, 16386, 2,
		gpio_phy_registers
	},
	{ MOD_HIF, 0, MOD_HIF, 0, 0, NTHW_FPGA_BUS_TYPE_PCI, 0, 18, hif_registers },
	{ MOD_IIC, 0, MOD_IIC, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 768, 22, iic_registers },
	{ MOD_IIC, 1, MOD_IIC, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 896, 22, iic_registers },
	{ MOD_IIC, 2, MOD_IIC, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 24832, 22, iic_registers },
	{ MOD_IIC, 3, MOD_IIC, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 24960, 22, iic_registers },
	{
		MOD_MAC_PCS, 0, MOD_MAC_PCS, 0, 2, NTHW_FPGA_BUS_TYPE_RAB2, 10240, 44,
		mac_pcs_registers
	},
	{
		MOD_MAC_PCS, 1, MOD_MAC_PCS, 0, 2, NTHW_FPGA_BUS_TYPE_RAB2, 11776, 44,
		mac_pcs_registers
	},
	{
		MOD_PCI_RD_TG, 0, MOD_PCI_RD_TG, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 2320, 6,
		pci_rd_tg_registers
	},
	{
		MOD_PCI_WR_TG, 0, MOD_PCI_WR_TG, 0, 1, NTHW_FPGA_BUS_TYPE_RAB0, 2304, 7,
		pci_wr_tg_registers
	},
	{ MOD_RAC, 0, MOD_RAC, 3, 0, NTHW_FPGA_BUS_TYPE_PCI, 8192, 14, rac_registers },
	{ MOD_RST9563, 0, MOD_RST9563, 0, 5, NTHW_FPGA_BUS_TYPE_RAB0, 1024, 5, rst9563_registers },
};

static nthw_fpga_prod_param_s product_parameters[] = {
	{ NT_BUILD_NUMBER, 0 },
	{ NT_BUILD_TIME, 1713859545 },
	{ NT_CATEGORIES, 64 },
	{ NT_CAT_DCT_PRESENT, 0 },
	{ NT_CAT_END_OFS_SUPPORT, 0 },
	{ NT_CAT_FUNCS, 64 },
	{ NT_CAT_KCC_BANKS, 3 },
	{ NT_CAT_KCC_PRESENT, 0 },
	{ NT_CAT_KCC_SIZE, 1536 },
	{ NT_CAT_KM_IF_CNT, 2 },
	{ NT_CAT_KM_IF_M0, 0 },
	{ NT_CAT_KM_IF_M1, 1 },
	{ NT_CAT_N_CMP, 8 },
	{ NT_CAT_N_EXT, 4 },
	{ NT_CAT_N_LEN, 8 },
	{ NT_CB_DEBUG, 0 },
	{ NT_COR_CATEGORIES, 16 },
	{ NT_COR_PRESENT, 0 },
	{ NT_CSU_PRESENT, 1 },
	{ NT_DBS_PRESENT, 1 },
	{ NT_DBS_RX_QUEUES, 128 },
	{ NT_DBS_TX_PORTS, 2 },
	{ NT_DBS_TX_QUEUES, 128 },
	{ NT_DDP_PRESENT, 0 },
	{ NT_DDP_TBL_DEPTH, 4096 },
	{ NT_EMI_SPLIT_STEPS, 16 },
	{ NT_EOF_TIMESTAMP_ONLY, 1 },
	{ NT_EPP_CATEGORIES, 32 },
	{ NT_FLM_CACHE, 1 },
	{ NT_FLM_CATEGORIES, 32 },
	{ NT_FLM_ENTRY_SIZE, 64 },
	{ NT_FLM_LOAD_APS_MAX, 260000000 },
	{ NT_FLM_LOAD_LPS_MAX, 300000000 },
	{ NT_FLM_PRESENT, 1 },
	{ NT_FLM_PRIOS, 4 },
	{ NT_FLM_PST_PROFILES, 16 },
	{ NT_FLM_SCRUB_PROFILES, 16 },
	{ NT_FLM_SIZE_MB, 12288 },
	{ NT_FLM_STATEFUL, 1 },
	{ NT_FLM_VARIANT, 2 },
	{ NT_GFG_PRESENT, 1 },
	{ NT_GFG_TX_LIVE_RECONFIG_SUPPORT, 1 },
	{ NT_GMF_FCS_PRESENT, 0 },
	{ NT_GMF_IFG_SPEED_DIV, 33 },
	{ NT_GMF_IFG_SPEED_DIV100G, 33 },
	{ NT_GMF_IFG_SPEED_MUL, 20 },
	{ NT_GMF_IFG_SPEED_MUL100G, 20 },
	{ NT_GROUP_ID, 9563 },
	{ NT_HFU_PRESENT, 1 },
	{ NT_HIF_MSIX_BAR, 1 },
	{ NT_HIF_MSIX_PBA_OFS, 8192 },
	{ NT_HIF_MSIX_PRESENT, 1 },
	{ NT_HIF_MSIX_TBL_OFS, 0 },
	{ NT_HIF_MSIX_TBL_SIZE, 8 },
	{ NT_HIF_PER_PS, 4000 },
	{ NT_HIF_SRIOV_PRESENT, 1 },
	{ NT_HIF_VF_OFFSET, 4 },
	{ NT_HSH_CATEGORIES, 16 },
	{ NT_HSH_TOEPLITZ, 1 },
	{ NT_HST_CATEGORIES, 32 },
	{ NT_HST_PRESENT, 0 },
	{ NT_IOA_CATEGORIES, 1024 },
	{ NT_IOA_PRESENT, 0 },
	{ NT_IPF_PRESENT, 0 },
	{ NT_KM_CAM_BANKS, 3 },
	{ NT_KM_CAM_RECORDS, 2048 },
	{ NT_KM_CAM_REC_WORDS, 6 },
	{ NT_KM_CATEGORIES, 32 },
	{ NT_KM_END_OFS_SUPPORT, 0 },
	{ NT_KM_EXT_EXTRACTORS, 1 },
	{ NT_KM_FLOW_TYPES, 16 },
	{ NT_KM_PRESENT, 1 },
	{ NT_KM_SWX_PRESENT, 0 },
	{ NT_KM_SYNERGY_MATCH, 0 },
	{ NT_KM_TCAM_BANKS, 12 },
	{ NT_KM_TCAM_BANK_WIDTH, 72 },
	{ NT_KM_TCAM_HIT_QUAL, 0 },
	{ NT_KM_TCAM_KEYWAY, 1 },
	{ NT_KM_WIDE, 1 },
	{ NT_LR_PRESENT, 1 },
	{ NT_MCU_PRESENT, 0 },
	{ NT_MDG_DEBUG_FLOW_CONTROL, 0 },
	{ NT_MDG_DEBUG_REG_READ_BACK, 0 },
	{ NT_MSK_CATEGORIES, 32 },
	{ NT_MSK_PRESENT, 0 },
	{ NT_NFV_OVS_PRODUCT, 0 },
	{ NT_NIMS, 2 },
	{ NT_PCI_DEVICE_ID, 453 },
	{ NT_PCI_TA_TG_PRESENT, 1 },
	{ NT_PCI_VENDOR_ID, 6388 },
	{ NT_PDB_CATEGORIES, 16 },
	{ NT_PHY_ANEG_PRESENT, 0 },
	{ NT_PHY_KRFEC_PRESENT, 0 },
	{ NT_PHY_PORTS, 2 },
	{ NT_PHY_PORTS_PER_QUAD, 1 },
	{ NT_PHY_QUADS, 2 },
	{ NT_PHY_RSFEC_PRESENT, 1 },
	{ NT_QM_CELLS, 2097152 },
	{ NT_QM_CELL_SIZE, 6144 },
	{ NT_QM_PRESENT, 0 },
	{ NT_QSL_CATEGORIES, 32 },
	{ NT_QSL_COLOR_SEL_BW, 7 },
	{ NT_QSL_QST_SIZE, 4096 },
	{ NT_QUEUES, 128 },
	{ NT_RAC_RAB_INTERFACES, 3 },
	{ NT_RAC_RAB_OB_UPDATE, 0 },
	{ NT_REVISION_ID, 39 },
	{ NT_RMC_LAG_GROUPS, 1 },
	{ NT_RMC_PRESENT, 1 },
	{ NT_ROA_CATEGORIES, 1024 },
	{ NT_ROA_PRESENT, 0 },
	{ NT_RPF_MATURING_DEL_DEFAULT, -150 },
	{ NT_RPF_PRESENT, 0 },
	{ NT_RPP_PER_PS, 3333 },
	{ NT_RTX_PRESENT, 0 },
	{ NT_RX_HOST_BUFFERS, 128 },
	{ NT_RX_PORTS, 2 },
	{ NT_RX_PORT_REPLICATE, 0 },
	{ NT_SLB_PRESENT, 0 },
	{ NT_SLC_LR_PRESENT, 1 },
	{ NT_SLC_PRESENT, 0 },
	{ NT_STA_COLORS, 64 },
	{ NT_STA_LOAD_AVG_RX, 1 },
	{ NT_STA_LOAD_AVG_TX, 1 },
	{ NT_STA_RX_PORTS, 2 },
	{ NT_TBH_DEBUG_DLN, 1 },
	{ NT_TBH_PRESENT, 0 },
	{ NT_TFD_PRESENT, 1 },
	{ NT_TPE_CATEGORIES, 16 },
	{ NT_TSM_OST_ONLY, 0 },
	{ NT_TS_APPEND, 0 },
	{ NT_TS_INJECT_PRESENT, 0 },
	{ NT_TX_CPY_PACKET_READERS, 0 },
	{ NT_TX_CPY_PRESENT, 1 },
	{ NT_TX_CPY_SIDEBAND_READERS, 7 },
	{ NT_TX_CPY_VARIANT, 0 },
	{ NT_TX_CPY_WRITERS, 6 },
	{ NT_TX_HOST_BUFFERS, 128 },
	{ NT_TX_INS_OFS_ZERO, 1 },
	{ NT_TX_INS_PRESENT, 1 },
	{ NT_TX_MTU_PROFILE_IFR, 16 },
	{ NT_TX_ON_TIMESTAMP, 1 },
	{ NT_TX_PORTS, 2 },
	{ NT_TX_PORT_REPLICATE, 1 },
	{ NT_TX_RPL_DEPTH, 4096 },
	{ NT_TX_RPL_EXT_CATEGORIES, 1024 },
	{ NT_TX_RPL_OFS_ZERO, 1 },
	{ NT_TX_RPL_PRESENT, 1 },
	{ NT_TYPE_ID, 200 },
	{ NT_USE_TRIPLE_SPEED, 0 },
	{ NT_VERSION_ID, 55 },
	{ NT_VLI_PRESENT, 0 },
	{ 0, -1 },	/* END */
};

nthw_fpga_prod_init_s nthw_fpga_9563_055_039_0000 = {
	200, 9563, 55, 39, 0, 0, 1713859545, 152, product_parameters, 15, fpga_modules,
};
