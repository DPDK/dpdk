/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <errno.h>
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

nthw_dbs_t *nthw_dbs_new(void)
{
	nthw_dbs_t *p = malloc(sizeof(nthw_dbs_t));

	if (p)
		memset(p, 0, sizeof(nthw_dbs_t));

	return p;
}

int dbs_init(nthw_dbs_t *p, nthw_fpga_t *p_fpga, int n_instance)
{
	nthw_module_t *mod = nthw_fpga_query_module(p_fpga, MOD_DBS, n_instance);

	if (p == NULL)
		return mod == NULL ? -1 : 0;

	if (mod == NULL) {
		NT_LOG(ERR, NTHW, "%s: DBS %d: no such instance",
			p_fpga->p_fpga_info->mp_adapter_id_str, n_instance);
		return -1;
	}

	p->mp_fpga = p_fpga;
	p->mn_instance = n_instance;
	p->mp_mod_dbs = mod;

	p->mn_param_dbs_present = nthw_fpga_get_product_param(p_fpga, NT_DBS_PRESENT, 0);

	if (p->mn_param_dbs_present == 0) {
		NT_LOG(WRN, NTHW,
			"%s: DBS %d: logical error: module found but not flagged at present",
			p->mp_fpga->p_fpga_info->mp_adapter_id_str, p->mn_instance);
	}

	return 0;
}

void dbs_reset(nthw_dbs_t *p)
{
	(void)p;
}

int set_rx_control(nthw_dbs_t *p,
	uint32_t last_queue,
	uint32_t avail_monitor_enable,
	uint32_t avail_monitor_speed,
	uint32_t used_write_enable,
	uint32_t used_write_speed,
	uint32_t rx_queue_enable)
{
	nthw_field_set_val32(p->mp_fld_rx_control_last_queue, last_queue);
	nthw_field_set_val32(p->mp_fld_rx_control_avail_monitor_enable, avail_monitor_enable);
	nthw_field_set_val32(p->mp_fld_rx_control_avail_monitor_scan_speed, avail_monitor_speed);
	nthw_field_set_val32(p->mp_fld_rx_control_used_write_enable, used_write_enable);
	nthw_field_set_val32(p->mp_fld_rx_control_used_writer_update_speed, used_write_speed);
	nthw_field_set_val32(p->mp_fld_rx_control_rx_queues_enable, rx_queue_enable);
	nthw_register_flush(p->mp_reg_rx_control, 1);
	return 0;
}

int set_tx_control(nthw_dbs_t *p,
	uint32_t last_queue,
	uint32_t avail_monitor_enable,
	uint32_t avail_monitor_speed,
	uint32_t used_write_enable,
	uint32_t used_write_speed,
	uint32_t tx_queue_enable)
{
	nthw_field_set_val32(p->mp_fld_tx_control_last_queue, last_queue);
	nthw_field_set_val32(p->mp_fld_tx_control_avail_monitor_enable, avail_monitor_enable);
	nthw_field_set_val32(p->mp_fld_tx_control_avail_monitor_scan_speed, avail_monitor_speed);
	nthw_field_set_val32(p->mp_fld_tx_control_used_write_enable, used_write_enable);
	nthw_field_set_val32(p->mp_fld_tx_control_used_writer_update_speed, used_write_speed);
	nthw_field_set_val32(p->mp_fld_tx_control_tx_queues_enable, tx_queue_enable);
	nthw_register_flush(p->mp_reg_tx_control, 1);
	return 0;
}

int set_rx_init(nthw_dbs_t *p, uint32_t start_idx, uint32_t start_ptr, uint32_t init,
	uint32_t queue)
{
	if (p->mp_reg_rx_init_val) {
		nthw_field_set_val32(p->mp_fld_rx_init_val_idx, start_idx);
		nthw_field_set_val32(p->mp_fld_rx_init_val_ptr, start_ptr);
		nthw_register_flush(p->mp_reg_rx_init_val, 1);
	}

	nthw_field_set_val32(p->mp_fld_rx_init_init, init);
	nthw_field_set_val32(p->mp_fld_rx_init_queue, queue);
	nthw_register_flush(p->mp_reg_rx_init, 1);
	return 0;
}

int get_rx_init(nthw_dbs_t *p, uint32_t *init, uint32_t *queue, uint32_t *busy)
{
	*init = nthw_field_get_val32(p->mp_fld_rx_init_init);
	*queue = nthw_field_get_val32(p->mp_fld_rx_init_queue);
	*busy = nthw_field_get_val32(p->mp_fld_rx_init_busy);
	return 0;
}

int set_tx_init(nthw_dbs_t *p, uint32_t start_idx, uint32_t start_ptr, uint32_t init,
	uint32_t queue)
{
	if (p->mp_reg_tx_init_val) {
		nthw_field_set_val32(p->mp_fld_tx_init_val_idx, start_idx);
		nthw_field_set_val32(p->mp_fld_tx_init_val_ptr, start_ptr);
		nthw_register_flush(p->mp_reg_tx_init_val, 1);
	}

	nthw_field_set_val32(p->mp_fld_tx_init_init, init);
	nthw_field_set_val32(p->mp_fld_tx_init_queue, queue);
	nthw_register_flush(p->mp_reg_tx_init, 1);
	return 0;
}

int get_tx_init(nthw_dbs_t *p, uint32_t *init, uint32_t *queue, uint32_t *busy)
{
	*init = nthw_field_get_val32(p->mp_fld_tx_init_init);
	*queue = nthw_field_get_val32(p->mp_fld_tx_init_queue);
	*busy = nthw_field_get_val32(p->mp_fld_tx_init_busy);
	return 0;
}
