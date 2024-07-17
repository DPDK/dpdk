/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <string.h>	/* memcmp, memset */

#include "nt_util.h"
#include "ntlog.h"
#include "ntnic_mod_reg.h"

/*
 * Initialize all ports
 * The driver calls this function during initialization (of the driver).
 */
static int nt4ga_link_100g_ports_init(struct adapter_info_s *p_adapter_info, nthw_fpga_t *fpga)
{
	(void)fpga;
	const int adapter_no = p_adapter_info->adapter_no;
	int res = 0;

	NT_LOG(DBG, NTNIC, "%s: Initializing ports\n", p_adapter_info->mp_adapter_id_str);

	/*
	 * Initialize global variables
	 */
	assert(adapter_no >= 0 && adapter_no < NUM_ADAPTER_MAX);

	if (res == 0 && !p_adapter_info->nt4ga_link.variables_initialized) {
		if (res == 0) {
			p_adapter_info->nt4ga_link.speed_capa = NT_LINK_SPEED_100G;
			p_adapter_info->nt4ga_link.variables_initialized = true;
		}
	}

	return res;
}

/*
 * Init 100G link ops variables
 */
static struct link_ops_s link_100g_ops = {
	.link_init = nt4ga_link_100g_ports_init,
};

void link_100g_init(void)
{
	register_100g_link_ops(&link_100g_ops);
}
