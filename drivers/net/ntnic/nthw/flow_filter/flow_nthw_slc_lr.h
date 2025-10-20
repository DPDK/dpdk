/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __FLOW_NTHW_SLC_LR_H__
#define __FLOW_NTHW_SLC_LR_H__

#include <stdint.h>

#include "nthw_fpga_model.h"

struct slc_lr_nthw {
	uint8_t m_physical_adapter_no;
	nthw_fpga_t *mp_fpga;

	nthw_module_t *m_slc_lr;

	nthw_register_t *mp_rcp_ctrl;
	nthw_field_t *mp_rcp_addr;
	nthw_field_t *mp_rcp_cnt;
	nthw_register_t *mp_rcp_data;

	nthw_field_t *mp_rcp_data_head_slc_en;
	nthw_field_t *mp_rcp_data_head_dyn;
	nthw_field_t *mp_rcp_data_head_ofs;
	nthw_field_t *mp_rcp_data_tail_slc_en;
	nthw_field_t *mp_rcp_data_tail_dyn;
	nthw_field_t *mp_rcp_data_tail_ofs;
	nthw_field_t *mp_rcp_data_pcap;
};

typedef struct slc_lr_nthw slc_lr_nthw_t;

struct slc_lr_nthw *nthw_slc_lr_new(void);
void nthw_slc_lr_delete(struct slc_lr_nthw *p);
int nthw_slc_lr_init(struct slc_lr_nthw *p, nthw_fpga_t *p_fpga, int n_instance);

int slc_lr_nthw_setup(struct slc_lr_nthw *p, int n_idx, int n_idx_cnt);
void nthw_slc_lr_set_debug_mode(struct slc_lr_nthw *p, unsigned int n_debug_mode);

/* RCP */
void nthw_slc_lr_rcp_select(const struct slc_lr_nthw *p, uint32_t val);
void nthw_slc_lr_rcp_cnt(const struct slc_lr_nthw *p, uint32_t val);
void nthw_slc_lr_rcp_head_slc_en(const struct slc_lr_nthw *p, uint32_t val);
void nthw_slc_lr_rcp_head_dyn(const struct slc_lr_nthw *p, uint32_t val);
void nthw_slc_lr_rcp_head_ofs(const struct slc_lr_nthw *p, int32_t val);
void nthw_slc_lr_rcp_tail_slc_en(const struct slc_lr_nthw *p, uint32_t val);
void nthw_slc_lr_rcp_tail_dyn(const struct slc_lr_nthw *p, uint32_t val);
void nthw_slc_lr_rcp_tail_ofs(const struct slc_lr_nthw *p, int32_t val);
void nthw_slc_lr_rcp_pcap(const struct slc_lr_nthw *p, uint32_t val);
void nthw_slc_lr_rcp_flush(const struct slc_lr_nthw *p);

#endif	/* __FLOW_NTHW_SLC_LR_H__ */
