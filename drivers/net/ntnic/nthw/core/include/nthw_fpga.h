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

int nthw_fpga_get_param_info(struct fpga_info_s *p_fpga_info, nthw_fpga_t *p_fpga);

struct nt200a0x_ops {
	int (*nthw_fpga_nt200a0x_init)(struct fpga_info_s *p_fpga_info);
};

void register_nt200a0x_ops(struct nt200a0x_ops *ops);
struct nt200a0x_ops *get_nt200a0x_ops(void);
void nt200a0x_ops_init(void);

#endif	/* __NTHW_FPGA_H__ */
