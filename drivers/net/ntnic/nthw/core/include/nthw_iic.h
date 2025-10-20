/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_IIC_H__
#define __NTHW_IIC_H__

#include "nthw_fpga_model.h"

struct nthw_iic {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_iic;
	int mn_iic_instance;

	uint32_t mn_iic_cycle_time;
	int mn_poll_delay;
	int mn_bus_ready_retry;
	int mn_data_ready_retry;
	int mn_read_data_retry;
	int mn_write_data_retry;

	nthw_register_t *mp_reg_tsusta;
	nthw_field_t *mp_fld_tsusta;

	nthw_register_t *mp_reg_tsusto;
	nthw_field_t *mp_fld_tsusto;

	nthw_register_t *mp_reg_thdsta;
	nthw_field_t *mp_fld_thdsta;

	nthw_register_t *mp_reg_tsudat;
	nthw_field_t *mp_fld_tsudat;

	nthw_register_t *mp_reg_tbuf;
	nthw_field_t *mp_fld_tbuf;

	nthw_register_t *mp_reg_thigh;
	nthw_field_t *mp_fld_thigh;

	nthw_register_t *mp_reg_tlow;
	nthw_field_t *mp_fld_tlow;

	nthw_register_t *mp_reg_thddat;
	nthw_field_t *mp_fld_thddat;

	nthw_register_t *mp_reg_cr;
	nthw_field_t *mp_fld_cr_en;
	nthw_field_t *mp_fld_cr_msms;
	nthw_field_t *mp_fld_cr_txfifo_reset;
	nthw_field_t *mp_fld_cr_txak;

	nthw_register_t *mp_reg_sr;
	nthw_field_t *mp_fld_sr_bb;
	nthw_field_t *mp_fld_sr_rxfifo_full;
	nthw_field_t *mp_fld_sr_rxfifo_empty;
	nthw_field_t *mp_fld_sr_txfifo_full;
	nthw_field_t *mp_fld_sr_txfifo_empty;

	nthw_register_t *mp_reg_tx_fifo;
	nthw_field_t *mp_fld_tx_fifo_txdata;
	nthw_field_t *mp_fld_tx_fifo_start;
	nthw_field_t *mp_fld_tx_fifo_stop;

	nthw_register_t *mp_reg_rx_fifo_pirq;
	nthw_field_t *mp_fld_rx_fifo_pirq_cmp_val;

	nthw_register_t *mp_reg_rx_fifo;
	nthw_field_t *mp_fld_rx_fifo_rxdata;

	nthw_register_t *mp_reg_softr;
	nthw_field_t *mp_fld_softr_rkey;
};

typedef struct nthw_iic nthw_iic_t;

nthw_iic_t *nthw_iic_new(void);
int nthw_iic_init(nthw_iic_t *p, nthw_fpga_t *p_fpga, int n_iic_instance,
	uint32_t n_iic_cycle_time);
void nthw_iic_delete(nthw_iic_t *p);

int nthw_iic_set_retry_params(nthw_iic_t *p, const int n_poll_delay, const int n_bus_ready_retry,
	const int n_data_ready_retry, const int n_read_data_retry,
	const int n_write_data_retry);

int nthw_iic_read_data(nthw_iic_t *p, uint8_t dev_addr, uint8_t a_reg_addr, uint8_t data_len,
	void *p_void);
int nthw_iic_write_data(nthw_iic_t *p, uint8_t dev_addr, uint8_t a_reg_addr, uint8_t data_len,
	void *p_void);
int nthw_iic_scan(nthw_iic_t *p);
#endif	/* __NTHW_IIC_H__ */
