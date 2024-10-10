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

	p->mp_reg_rx_control = nthw_module_get_register(p->mp_mod_dbs, DBS_RX_CONTROL);
	p->mp_fld_rx_control_last_queue =
		nthw_register_get_field(p->mp_reg_rx_control, DBS_RX_CONTROL_LQ);
	p->mp_fld_rx_control_avail_monitor_enable =
		nthw_register_get_field(p->mp_reg_rx_control, DBS_RX_CONTROL_AME);
	p->mp_fld_rx_control_avail_monitor_scan_speed =
		nthw_register_get_field(p->mp_reg_rx_control, DBS_RX_CONTROL_AMS);
	p->mp_fld_rx_control_used_write_enable =
		nthw_register_get_field(p->mp_reg_rx_control, DBS_RX_CONTROL_UWE);
	p->mp_fld_rx_control_used_writer_update_speed =
		nthw_register_get_field(p->mp_reg_rx_control, DBS_RX_CONTROL_UWS);
	p->mp_fld_rx_control_rx_queues_enable =
		nthw_register_get_field(p->mp_reg_rx_control, DBS_RX_CONTROL_QE);

	p->mp_reg_tx_control = nthw_module_get_register(p->mp_mod_dbs, DBS_TX_CONTROL);
	p->mp_fld_tx_control_last_queue =
		nthw_register_get_field(p->mp_reg_tx_control, DBS_TX_CONTROL_LQ);
	p->mp_fld_tx_control_avail_monitor_enable =
		nthw_register_get_field(p->mp_reg_tx_control, DBS_TX_CONTROL_AME);
	p->mp_fld_tx_control_avail_monitor_scan_speed =
		nthw_register_get_field(p->mp_reg_tx_control, DBS_TX_CONTROL_AMS);
	p->mp_fld_tx_control_used_write_enable =
		nthw_register_get_field(p->mp_reg_tx_control, DBS_TX_CONTROL_UWE);
	p->mp_fld_tx_control_used_writer_update_speed =
		nthw_register_get_field(p->mp_reg_tx_control, DBS_TX_CONTROL_UWS);
	p->mp_fld_tx_control_tx_queues_enable =
		nthw_register_get_field(p->mp_reg_tx_control, DBS_TX_CONTROL_QE);

	p->mp_reg_rx_init = nthw_module_get_register(p->mp_mod_dbs, DBS_RX_INIT);
	p->mp_fld_rx_init_init = nthw_register_get_field(p->mp_reg_rx_init, DBS_RX_INIT_INIT);
	p->mp_fld_rx_init_queue = nthw_register_get_field(p->mp_reg_rx_init, DBS_RX_INIT_QUEUE);
	p->mp_fld_rx_init_busy = nthw_register_get_field(p->mp_reg_rx_init, DBS_RX_INIT_BUSY);

	p->mp_reg_rx_init_val = nthw_module_query_register(p->mp_mod_dbs, DBS_RX_INIT_VAL);

	if (p->mp_reg_rx_init_val) {
		p->mp_fld_rx_init_val_idx =
			nthw_register_query_field(p->mp_reg_rx_init_val, DBS_RX_INIT_VAL_IDX);
		p->mp_fld_rx_init_val_ptr =
			nthw_register_query_field(p->mp_reg_rx_init_val, DBS_RX_INIT_VAL_PTR);
	}

	p->mp_reg_rx_ptr = nthw_module_query_register(p->mp_mod_dbs, DBS_RX_PTR);

	if (p->mp_reg_rx_ptr) {
		p->mp_fld_rx_ptr_ptr = nthw_register_query_field(p->mp_reg_rx_ptr, DBS_RX_PTR_PTR);
		p->mp_fld_rx_ptr_queue =
			nthw_register_query_field(p->mp_reg_rx_ptr, DBS_RX_PTR_QUEUE);
		p->mp_fld_rx_ptr_valid =
			nthw_register_query_field(p->mp_reg_rx_ptr, DBS_RX_PTR_VALID);
	}

	p->mp_reg_tx_init = nthw_module_get_register(p->mp_mod_dbs, DBS_TX_INIT);
	p->mp_fld_tx_init_init = nthw_register_get_field(p->mp_reg_tx_init, DBS_TX_INIT_INIT);
	p->mp_fld_tx_init_queue = nthw_register_get_field(p->mp_reg_tx_init, DBS_TX_INIT_QUEUE);
	p->mp_fld_tx_init_busy = nthw_register_get_field(p->mp_reg_tx_init, DBS_TX_INIT_BUSY);

	p->mp_reg_tx_init_val = nthw_module_query_register(p->mp_mod_dbs, DBS_TX_INIT_VAL);

	if (p->mp_reg_tx_init_val) {
		p->mp_fld_tx_init_val_idx =
			nthw_register_query_field(p->mp_reg_tx_init_val, DBS_TX_INIT_VAL_IDX);
		p->mp_fld_tx_init_val_ptr =
			nthw_register_query_field(p->mp_reg_tx_init_val, DBS_TX_INIT_VAL_PTR);
	}

	p->mp_reg_tx_ptr = nthw_module_query_register(p->mp_mod_dbs, DBS_TX_PTR);

	if (p->mp_reg_tx_ptr) {
		p->mp_fld_tx_ptr_ptr = nthw_register_query_field(p->mp_reg_tx_ptr, DBS_TX_PTR_PTR);
		p->mp_fld_tx_ptr_queue =
			nthw_register_query_field(p->mp_reg_tx_ptr, DBS_TX_PTR_QUEUE);
		p->mp_fld_tx_ptr_valid =
			nthw_register_query_field(p->mp_reg_tx_ptr, DBS_TX_PTR_VALID);
	}

	p->mp_reg_rx_idle = nthw_module_query_register(p->mp_mod_dbs, DBS_RX_IDLE);

	if (p->mp_reg_rx_idle) {
		p->mp_fld_rx_idle_idle =
			nthw_register_query_field(p->mp_reg_rx_idle, DBS_RX_IDLE_IDLE);
		p->mp_fld_rx_idle_queue =
			nthw_register_query_field(p->mp_reg_rx_idle, DBS_RX_IDLE_QUEUE);
		p->mp_fld_rx_idle_busy =
			nthw_register_query_field(p->mp_reg_rx_idle, DBS_RX_IDLE_BUSY);
	}

	p->mp_reg_tx_idle = nthw_module_query_register(p->mp_mod_dbs, DBS_TX_IDLE);

	if (p->mp_reg_tx_idle) {
		p->mp_fld_tx_idle_idle =
			nthw_register_query_field(p->mp_reg_tx_idle, DBS_TX_IDLE_IDLE);
		p->mp_fld_tx_idle_queue =
			nthw_register_query_field(p->mp_reg_tx_idle, DBS_TX_IDLE_QUEUE);
		p->mp_fld_tx_idle_busy =
			nthw_register_query_field(p->mp_reg_tx_idle, DBS_TX_IDLE_BUSY);
	}

	return 0;
}

static int dbs_reset_rx_control(nthw_dbs_t *p)
{
	nthw_field_set_val32(p->mp_fld_rx_control_last_queue, 0);
	nthw_field_set_val32(p->mp_fld_rx_control_avail_monitor_enable, 0);
	nthw_field_set_val32(p->mp_fld_rx_control_avail_monitor_scan_speed, 8);
	nthw_field_set_val32(p->mp_fld_rx_control_used_write_enable, 0);
	nthw_field_set_val32(p->mp_fld_rx_control_used_writer_update_speed, 5);
	nthw_field_set_val32(p->mp_fld_rx_control_rx_queues_enable, 0);
	nthw_register_flush(p->mp_reg_rx_control, 1);
	return 0;
}

static int dbs_reset_tx_control(nthw_dbs_t *p)
{
	nthw_field_set_val32(p->mp_fld_tx_control_last_queue, 0);
	nthw_field_set_val32(p->mp_fld_tx_control_avail_monitor_enable, 0);
	nthw_field_set_val32(p->mp_fld_tx_control_avail_monitor_scan_speed, 5);
	nthw_field_set_val32(p->mp_fld_tx_control_used_write_enable, 0);
	nthw_field_set_val32(p->mp_fld_tx_control_used_writer_update_speed, 8);
	nthw_field_set_val32(p->mp_fld_tx_control_tx_queues_enable, 0);
	nthw_register_flush(p->mp_reg_tx_control, 1);
	return 0;
}

void dbs_reset(nthw_dbs_t *p)
{
	dbs_reset_rx_control(p);
	dbs_reset_tx_control(p);
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
