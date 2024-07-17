/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef __NTNIC_MOD_REG_H__
#define __NTNIC_MOD_REG_H__

#include <stdint.h>
#include "nthw_fpga_model.h"
#include "nthw_platform_drv.h"
#include "nthw_drv.h"
#include "nt4ga_adapter.h"
#include "ntos_drv.h"

struct adapter_ops {
	int (*init)(struct adapter_info_s *p_adapter_info);
	int (*deinit)(struct adapter_info_s *p_adapter_info);

	int (*show_info)(struct adapter_info_s *p_adapter_info, FILE *pfh);
};

void register_adapter_ops(const struct adapter_ops *ops);
const struct adapter_ops *get_adapter_ops(void);
void adapter_init(void);


#endif	/* __NTNIC_MOD_REG_H__ */
