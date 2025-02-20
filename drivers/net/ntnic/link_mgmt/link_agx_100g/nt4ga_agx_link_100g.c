/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "nt_util.h"
#include "ntlog.h"
#include "i2c_nim.h"
#include "nt4ga_adapter.h"

#include <string.h>
#include "ntnic_mod_reg.h"
#include "nim_defines.h"

static int nt4ga_agx_link_100g_ports_init(struct adapter_info_s *p_adapter_info,
	nthw_fpga_t *fpga);

/*
 * Init AGX 100G link ops variables
 */
static struct link_ops_s link_agx_100g_ops = {
	.link_init = nt4ga_agx_link_100g_ports_init,
};

void link_agx_100g_init(void)
{
	register_agx_100g_link_ops(&link_agx_100g_ops);
}

/*
 * Initialize all ports
 * The driver calls this function during initialization (of the driver).
 */
int nt4ga_agx_link_100g_ports_init(struct adapter_info_s *p_adapter_info, nthw_fpga_t *fpga)
{
	(void)fpga;
	int res = 0;

	NT_LOG(DBG, NTNIC, "%s: Initializing ports", p_adapter_info->mp_adapter_id_str);

	return res;
}
