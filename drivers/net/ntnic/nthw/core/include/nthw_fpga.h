/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTHW_FPGA_H__
#define __NTHW_FPGA_H__

#include "nthw_drv.h"

#include "nthw_fpga_model.h"

#include "nthw_rac.h"
#include "nthw_iic.h"

int nthw_fpga_init(struct fpga_info_s *p_fpga_info);
int nthw_fpga_shutdown(struct fpga_info_s *p_fpga_info);

int nthw_fpga_avr_probe(nthw_fpga_t *p_fpga, const int n_instance_no);

int nthw_fpga_iic_scan(nthw_fpga_t *p_fpga, const int n_instance_no_begin,
	const int n_instance_no_end);

int nthw_fpga_silabs_detect(nthw_fpga_t *p_fpga, const int n_instance_no, const int n_dev_addr,
	const int n_page_reg_addr);

int nthw_fpga_si5340_clock_synth_init_fmt2(nthw_fpga_t *p_fpga, const uint8_t n_iic_addr,
	const clk_profile_data_fmt2_t *p_clk_profile,
	const int n_clk_profile_rec_cnt);

struct nt200a0x_ops {
	int (*nthw_fpga_nt200a0x_init)(struct fpga_info_s *p_fpga_info);
};

void nthw_reg_nt200a0x_ops(struct nt200a0x_ops *ops);
struct nt200a0x_ops *nthw_get_nt200a0x_ops(void);
void nthw_nt200a0x_ops_init(void);

struct nt400dxx_ops {
	int (*nthw_fpga_nt400dxx_init)(struct fpga_info_s *p_fpga_info);
};

void nthw_reg_nt400dxx_ops(struct nt400dxx_ops *ops);
struct nt400dxx_ops *nthw_get_nt400dxx_ops(void);
void nthw_nt400dxx_ops_init(void);

#endif	/* __NTHW_FPGA_H__ */
