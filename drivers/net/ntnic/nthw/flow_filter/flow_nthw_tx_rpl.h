/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_TX_RPL_H__
#define __FLOW_NTHW_TX_RPL_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct tx_rpl_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_tx_rpl;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_ctrl_addr;
	nthw_field_t *mp_rcp_ctrl_cnt;

	nthw_register_t *mp_rcp_data;
	nthw_field_t *mp_rcp_data_dyn;
	nthw_field_t *mp_rcp_data_ofs;
	nthw_field_t *mp_rcp_data_len;
	nthw_field_t *mp_rcp_data_rpl_ptr;
	nthw_field_t *mp_rcp_data_ext_prio;
	nthw_field_t *mp_rcp_data_eth_type_wr;

	nthw_register_t *mp_ext_ctrl;
	nthw_field_t *mp_ext_ctrl_addr;
	nthw_field_t *mp_ext_ctrl_cnt;

	nthw_register_t *mp_ext_data;
	nthw_field_t *mp_ext_data_rpl_ptr;

	nthw_register_t *mp_rpl_ctrl;
	nthw_field_t *mp_rpl_ctrl_addr;
	nthw_field_t *mp_rpl_ctrl_cnt;

	nthw_register_t *mp_rpl_data;
	nthw_field_t *mp_rpl_data_value;
};

struct tx_rpl_nthw *tx_rpl_nthw_new(void);
void tx_rpl_nthw_delete(struct tx_rpl_nthw *p);
int tx_rpl_nthw_init(struct tx_rpl_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int tx_rpl_nthw_setup(struct tx_rpl_nthw *p, int n_idx, int n_idx_cnt);

#endif	/* __FLOW_NTHW_TX_RPL_H__ */
