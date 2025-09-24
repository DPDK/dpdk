/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_TX_CPY_H__
#define __FLOW_NTHW_TX_CPY_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct tx_cpy_writers_s {
	nthw_register_t *mp_writer_ctrl;
	nthw_field_t *mp_writer_ctrl_addr;
	nthw_field_t *mp_writer_ctrl_cnt;

	nthw_register_t *mp_writer_data;
	nthw_field_t *mp_writer_data_reader_select;
	nthw_field_t *mp_writer_data_dyn;
	nthw_field_t *mp_writer_data_ofs;
	nthw_field_t *mp_writer_data_len;
	nthw_field_t *mp_writer_data_mask_pointer;

	nthw_register_t *mp_writer_mask_ctrl;
	nthw_field_t *mp_writer_mask_ctrl_addr;
	nthw_field_t *mp_writer_mask_ctrl_cnt;

	nthw_register_t *mp_writer_mask_data;
	nthw_field_t *mp_writer_mask_data_byte_mask;
};

struct tx_cpy_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_tx_cpy;

	unsigned int m_writers_cnt;
	struct tx_cpy_writers_s *m_writers;
};

struct tx_cpy_nthw *nthw_tx_cpy_new(void);
void nthw_tx_cpy_delete(struct tx_cpy_nthw *p);
int nthw_tx_cpy_init(struct tx_cpy_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int tx_cpy_nthw_setup(struct tx_cpy_nthw *p, int n_idx, int n_idx_cnt);
void nthw_tx_cpy_set_debug_mode(struct tx_cpy_nthw *p, unsigned int n_debug_mode);

void nthw_tx_cpy_writer_select(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val);
void nthw_tx_cpy_writer_cnt(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val);
void nthw_tx_cpy_writer_reader_select(const struct tx_cpy_nthw *p, unsigned int index,
	uint32_t val);
void nthw_tx_cpy_writer_dyn(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val);
void nthw_tx_cpy_writer_ofs(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val);
void nthw_tx_cpy_writer_len(const struct tx_cpy_nthw *p, unsigned int index, uint32_t val);
void nthw_tx_cpy_writer_flush(const struct tx_cpy_nthw *p, unsigned int index);

#endif	/* __FLOW_NTHW_TX_CPY_H__ */
