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


#endif	/* __NTHW_FPGA_H__ */
