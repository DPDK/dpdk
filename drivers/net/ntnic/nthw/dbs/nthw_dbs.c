/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <errno.h>
#include "ntlog.h"

#include "nthw_drv.h"
#include "nthw_register.h"

static void set_shadow_tx_qp_data(nthw_dbs_t *p, uint32_t index, uint32_t virtual_port);
static void flush_tx_qp_data(nthw_dbs_t *p, uint32_t index);
static void set_shadow_rx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable);
static void flush_rx_am_data(nthw_dbs_t *p, uint32_t index);
static void set_shadow_tx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable);
static void flush_tx_am_data(nthw_dbs_t *p, uint32_t index);

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

	p->mp_reg_rx_avail_monitor_control =
		nthw_module_get_register(p->mp_mod_dbs, DBS_RX_AM_CTRL);
	p->mp_fld_rx_avail_monitor_control_adr =
		nthw_register_get_field(p->mp_reg_rx_avail_monitor_control, DBS_RX_AM_CTRL_ADR);
	p->mp_fld_rx_avail_monitor_control_cnt =
		nthw_register_get_field(p->mp_reg_rx_avail_monitor_control, DBS_RX_AM_CTRL_CNT);

	p->mp_reg_rx_avail_monitor_data = nthw_module_get_register(p->mp_mod_dbs, DBS_RX_AM_DATA);
	p->mp_fld_rx_avail_monitor_data_guest_physical_address =
		nthw_register_get_field(p->mp_reg_rx_avail_monitor_data, DBS_RX_AM_DATA_GPA);
	p->mp_fld_rx_avail_monitor_data_enable =
		nthw_register_get_field(p->mp_reg_rx_avail_monitor_data, DBS_RX_AM_DATA_ENABLE);
	p->mp_fld_rx_avail_monitor_data_host_id =
		nthw_register_get_field(p->mp_reg_rx_avail_monitor_data, DBS_RX_AM_DATA_HID);
	p->mp_fld_rx_avail_monitor_data_packed =
		nthw_register_query_field(p->mp_reg_rx_avail_monitor_data, DBS_RX_AM_DATA_PCKED);
	p->mp_fld_rx_avail_monitor_data_int =
		nthw_register_query_field(p->mp_reg_rx_avail_monitor_data, DBS_RX_AM_DATA_INT);

	p->mp_reg_tx_avail_monitor_control =
		nthw_module_get_register(p->mp_mod_dbs, DBS_TX_AM_CTRL);
	p->mp_fld_tx_avail_monitor_control_adr =
		nthw_register_get_field(p->mp_reg_tx_avail_monitor_control, DBS_TX_AM_CTRL_ADR);
	p->mp_fld_tx_avail_monitor_control_cnt =
		nthw_register_get_field(p->mp_reg_tx_avail_monitor_control, DBS_TX_AM_CTRL_CNT);

	p->mp_reg_tx_avail_monitor_data = nthw_module_get_register(p->mp_mod_dbs, DBS_TX_AM_DATA);
	p->mp_fld_tx_avail_monitor_data_guest_physical_address =
		nthw_register_get_field(p->mp_reg_tx_avail_monitor_data, DBS_TX_AM_DATA_GPA);
	p->mp_fld_tx_avail_monitor_data_enable =
		nthw_register_get_field(p->mp_reg_tx_avail_monitor_data, DBS_TX_AM_DATA_ENABLE);
	p->mp_fld_tx_avail_monitor_data_host_id =
		nthw_register_get_field(p->mp_reg_tx_avail_monitor_data, DBS_TX_AM_DATA_HID);
	p->mp_fld_tx_avail_monitor_data_packed =
		nthw_register_query_field(p->mp_reg_tx_avail_monitor_data, DBS_TX_AM_DATA_PCKED);
	p->mp_fld_tx_avail_monitor_data_int =
		nthw_register_query_field(p->mp_reg_tx_avail_monitor_data, DBS_TX_AM_DATA_INT);

	p->mp_reg_tx_queue_property_control =
		nthw_module_get_register(p->mp_mod_dbs, DBS_TX_QP_CTRL);
	p->mp_fld_tx_queue_property_control_adr =
		nthw_register_get_field(p->mp_reg_tx_queue_property_control, DBS_TX_QP_CTRL_ADR);
	p->mp_fld_tx_queue_property_control_cnt =
		nthw_register_get_field(p->mp_reg_tx_queue_property_control, DBS_TX_QP_CTRL_CNT);

	p->mp_reg_tx_queue_property_data = nthw_module_get_register(p->mp_mod_dbs, DBS_TX_QP_DATA);
	p->mp_fld_tx_queue_property_data_v_port =
		nthw_register_get_field(p->mp_reg_tx_queue_property_data, DBS_TX_QP_DATA_VPORT);

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
	int i;
	dbs_reset_rx_control(p);
	dbs_reset_tx_control(p);

	/* Reset RX memory banks and shado */
	for (i = 0; i < NT_DBS_RX_QUEUES_MAX; ++i) {
		set_shadow_rx_am_data(p, i, 0, 0, 0, 0, 0);
		flush_rx_am_data(p, i);
	}

	/* Reset TX memory banks and shado */
	for (i = 0; i < NT_DBS_TX_QUEUES_MAX; ++i) {
		set_shadow_tx_am_data(p, i, 0, 0, 0, 0, 0);
		flush_tx_am_data(p, i);

		set_shadow_tx_qp_data(p, i, 0);
		flush_tx_qp_data(p, i);
	}
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

static void set_rx_am_data_index(nthw_dbs_t *p, uint32_t index)
{
	nthw_field_set_val32(p->mp_fld_rx_avail_monitor_control_adr, index);
	nthw_field_set_val32(p->mp_fld_rx_avail_monitor_control_cnt, 1);
	nthw_register_flush(p->mp_reg_rx_avail_monitor_control, 1);
}

static void set_shadow_rx_am_data_guest_physical_address(nthw_dbs_t *p, uint32_t index,
	uint64_t guest_physical_address)
{
	p->m_rx_am_shadow[index].guest_physical_address = guest_physical_address;
}

static void nthw_dbs_set_shadow_rx_am_data_enable(nthw_dbs_t *p, uint32_t index, uint32_t enable)
{
	p->m_rx_am_shadow[index].enable = enable;
}

static void set_shadow_rx_am_data_host_id(nthw_dbs_t *p, uint32_t index, uint32_t host_id)
{
	p->m_rx_am_shadow[index].host_id = host_id;
}

static void set_shadow_rx_am_data_packed(nthw_dbs_t *p, uint32_t index, uint32_t packed)
{
	p->m_rx_am_shadow[index].packed = packed;
}

static void set_shadow_rx_am_data_int_enable(nthw_dbs_t *p, uint32_t index, uint32_t int_enable)
{
	p->m_rx_am_shadow[index].int_enable = int_enable;
}

static void set_shadow_rx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable)
{
	set_shadow_rx_am_data_guest_physical_address(p, index, guest_physical_address);
	nthw_dbs_set_shadow_rx_am_data_enable(p, index, enable);
	set_shadow_rx_am_data_host_id(p, index, host_id);
	set_shadow_rx_am_data_packed(p, index, packed);
	set_shadow_rx_am_data_int_enable(p, index, int_enable);
}

static void flush_rx_am_data(nthw_dbs_t *p, uint32_t index)
{
	nthw_field_set_val(p->mp_fld_rx_avail_monitor_data_guest_physical_address,
		(uint32_t *)&p->m_rx_am_shadow[index].guest_physical_address, 2);
	nthw_field_set_val32(p->mp_fld_rx_avail_monitor_data_enable,
		p->m_rx_am_shadow[index].enable);
	nthw_field_set_val32(p->mp_fld_rx_avail_monitor_data_host_id,
		p->m_rx_am_shadow[index].host_id);

	if (p->mp_fld_rx_avail_monitor_data_packed) {
		nthw_field_set_val32(p->mp_fld_rx_avail_monitor_data_packed,
			p->m_rx_am_shadow[index].packed);
	}

	if (p->mp_fld_rx_avail_monitor_data_int) {
		nthw_field_set_val32(p->mp_fld_rx_avail_monitor_data_int,
			p->m_rx_am_shadow[index].int_enable);
	}

	set_rx_am_data_index(p, index);
	nthw_register_flush(p->mp_reg_rx_avail_monitor_data, 1);
}

int set_rx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable)
{
	if (!p->mp_reg_rx_avail_monitor_data)
		return -ENOTSUP;

	set_shadow_rx_am_data(p, index, guest_physical_address, enable, host_id, packed,
		int_enable);
	flush_rx_am_data(p, index);
	return 0;
}

static void set_tx_am_data_index(nthw_dbs_t *p, uint32_t index)
{
	nthw_field_set_val32(p->mp_fld_tx_avail_monitor_control_adr, index);
	nthw_field_set_val32(p->mp_fld_tx_avail_monitor_control_cnt, 1);
	nthw_register_flush(p->mp_reg_tx_avail_monitor_control, 1);
}

static void set_shadow_tx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable)
{
	p->m_tx_am_shadow[index].guest_physical_address = guest_physical_address;
	p->m_tx_am_shadow[index].enable = enable;
	p->m_tx_am_shadow[index].host_id = host_id;
	p->m_tx_am_shadow[index].packed = packed;
	p->m_tx_am_shadow[index].int_enable = int_enable;
}

static void flush_tx_am_data(nthw_dbs_t *p, uint32_t index)
{
	nthw_field_set_val(p->mp_fld_tx_avail_monitor_data_guest_physical_address,
		(uint32_t *)&p->m_tx_am_shadow[index].guest_physical_address, 2);
	nthw_field_set_val32(p->mp_fld_tx_avail_monitor_data_enable,
		p->m_tx_am_shadow[index].enable);
	nthw_field_set_val32(p->mp_fld_tx_avail_monitor_data_host_id,
		p->m_tx_am_shadow[index].host_id);

	if (p->mp_fld_tx_avail_monitor_data_packed) {
		nthw_field_set_val32(p->mp_fld_tx_avail_monitor_data_packed,
			p->m_tx_am_shadow[index].packed);
	}

	if (p->mp_fld_tx_avail_monitor_data_int) {
		nthw_field_set_val32(p->mp_fld_tx_avail_monitor_data_int,
			p->m_tx_am_shadow[index].int_enable);
	}

	set_tx_am_data_index(p, index);
	nthw_register_flush(p->mp_reg_tx_avail_monitor_data, 1);
}

int set_tx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable)
{
	if (!p->mp_reg_tx_avail_monitor_data)
		return -ENOTSUP;

	set_shadow_tx_am_data(p, index, guest_physical_address, enable, host_id, packed,
		int_enable);
	flush_tx_am_data(p, index);
	return 0;
}

static void set_tx_qp_data_index(nthw_dbs_t *p, uint32_t index)
{
	nthw_field_set_val32(p->mp_fld_tx_queue_property_control_adr, index);
	nthw_field_set_val32(p->mp_fld_tx_queue_property_control_cnt, 1);
	nthw_register_flush(p->mp_reg_tx_queue_property_control, 1);
}

static void set_shadow_tx_qp_data_virtual_port(nthw_dbs_t *p, uint32_t index,
	uint32_t virtual_port)
{
	p->m_tx_qp_shadow[index].virtual_port = virtual_port;
}

static void set_shadow_tx_qp_data(nthw_dbs_t *p, uint32_t index, uint32_t virtual_port)
{
	set_shadow_tx_qp_data_virtual_port(p, index, virtual_port);
}

static void flush_tx_qp_data(nthw_dbs_t *p, uint32_t index)
{
	nthw_field_set_val32(p->mp_fld_tx_queue_property_data_v_port,
		p->m_tx_qp_shadow[index].virtual_port);

	set_tx_qp_data_index(p, index);
	nthw_register_flush(p->mp_reg_tx_queue_property_data, 1);
}

int nthw_dbs_set_tx_qp_data(nthw_dbs_t *p, uint32_t index, uint32_t virtual_port)
{
	if (!p->mp_reg_tx_queue_property_data)
		return -ENOTSUP;

	set_shadow_tx_qp_data(p, index, virtual_port);
	flush_tx_qp_data(p, index);
	return 0;
}
