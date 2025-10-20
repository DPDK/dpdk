/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _NTNIC_DBS_H_
#define _NTNIC_DBS_H_

#include "nthw_fpga_model.h"

#define NT_DBS_RX_QUEUES_MAX (128)
#define NT_DBS_TX_QUEUES_MAX (128)

/*
 * Struct for implementation of memory bank shadows
 */

/* DBS_RX_AM_DATA */
struct nthw_dbs_rx_am_data_s {
	uint64_t guest_physical_address;
	uint32_t enable;
	uint32_t host_id;
	uint32_t packed;
	uint32_t int_enable;
};

/* DBS_TX_AM_DATA */
struct nthw_dbs_tx_am_data_s {
	uint64_t guest_physical_address;
	uint32_t enable;
	uint32_t host_id;
	uint32_t packed;
	uint32_t int_enable;
};

/* DBS_RX_UW_DATA */
struct nthw_dbs_rx_uw_data_s {
	uint64_t guest_physical_address;
	uint32_t host_id;
	uint32_t queue_size;
	uint32_t packed;
	uint32_t int_enable;
	uint32_t vec;
	uint32_t istk;
};

/* DBS_TX_UW_DATA */
struct nthw_dbs_tx_uw_data_s {
	uint64_t guest_physical_address;
	uint32_t host_id;
	uint32_t queue_size;
	uint32_t packed;
	uint32_t int_enable;
	uint32_t vec;
	uint32_t istk;
	uint32_t in_order;
};

/* DBS_RX_DR_DATA */
struct nthw_dbs_rx_dr_data_s {
	uint64_t guest_physical_address;
	uint32_t host_id;
	uint32_t queue_size;
	uint32_t header;
	uint32_t packed;
};

/* DBS_TX_DR_DATA */
struct nthw_dbs_tx_dr_data_s {
	uint64_t guest_physical_address;
	uint32_t host_id;
	uint32_t queue_size;
	uint32_t header;
	uint32_t port;
	uint32_t packed;
};

/* DBS_TX_QP_DATA */
struct nthw_dbs_tx_qp_data_s {
	uint32_t virtual_port;
};

struct nthw_dbs_s {
	nthw_fpga_t *mp_fpga;
	nthw_module_t *mp_mod_dbs;
	int mn_instance;

	int mn_param_dbs_present;

	nthw_register_t *mp_reg_rx_control;
	nthw_field_t *mp_fld_rx_control_last_queue;
	nthw_field_t *mp_fld_rx_control_avail_monitor_enable;
	nthw_field_t *mp_fld_rx_control_avail_monitor_scan_speed;
	nthw_field_t *mp_fld_rx_control_used_write_enable;
	nthw_field_t *mp_fld_rx_control_used_writer_update_speed;
	nthw_field_t *mp_fld_rx_control_rx_queues_enable;

	nthw_register_t *mp_reg_tx_control;
	nthw_field_t *mp_fld_tx_control_last_queue;
	nthw_field_t *mp_fld_tx_control_avail_monitor_enable;
	nthw_field_t *mp_fld_tx_control_avail_monitor_scan_speed;
	nthw_field_t *mp_fld_tx_control_used_write_enable;
	nthw_field_t *mp_fld_tx_control_used_writer_update_speed;
	nthw_field_t *mp_fld_tx_control_tx_queues_enable;

	nthw_register_t *mp_reg_rx_init;
	nthw_field_t *mp_fld_rx_init_init;
	nthw_field_t *mp_fld_rx_init_queue;
	nthw_field_t *mp_fld_rx_init_busy;

	nthw_register_t *mp_reg_rx_init_val;
	nthw_field_t *mp_fld_rx_init_val_idx;
	nthw_field_t *mp_fld_rx_init_val_ptr;

	nthw_register_t *mp_reg_rx_ptr;
	nthw_field_t *mp_fld_rx_ptr_ptr;
	nthw_field_t *mp_fld_rx_ptr_queue;
	nthw_field_t *mp_fld_rx_ptr_valid;

	nthw_register_t *mp_reg_tx_init;
	nthw_field_t *mp_fld_tx_init_init;
	nthw_field_t *mp_fld_tx_init_queue;
	nthw_field_t *mp_fld_tx_init_busy;

	nthw_register_t *mp_reg_tx_init_val;
	nthw_field_t *mp_fld_tx_init_val_idx;
	nthw_field_t *mp_fld_tx_init_val_ptr;

	nthw_register_t *mp_reg_tx_ptr;
	nthw_field_t *mp_fld_tx_ptr_ptr;
	nthw_field_t *mp_fld_tx_ptr_queue;
	nthw_field_t *mp_fld_tx_ptr_valid;

	nthw_register_t *mp_reg_rx_idle;
	nthw_field_t *mp_fld_rx_idle_idle;
	nthw_field_t *mp_fld_rx_idle_queue;
	nthw_field_t *mp_fld_rx_idle_busy;

	nthw_register_t *mp_reg_tx_idle;
	nthw_field_t *mp_fld_tx_idle_idle;
	nthw_field_t *mp_fld_tx_idle_queue;
	nthw_field_t *mp_fld_tx_idle_busy;

	nthw_register_t *mp_reg_rx_avail_monitor_control;
	nthw_field_t *mp_fld_rx_avail_monitor_control_adr;
	nthw_field_t *mp_fld_rx_avail_monitor_control_cnt;

	nthw_register_t *mp_reg_rx_avail_monitor_data;
	nthw_field_t *mp_fld_rx_avail_monitor_data_guest_physical_address;
	nthw_field_t *mp_fld_rx_avail_monitor_data_enable;
	nthw_field_t *mp_fld_rx_avail_monitor_data_host_id;
	nthw_field_t *mp_fld_rx_avail_monitor_data_packed;
	nthw_field_t *mp_fld_rx_avail_monitor_data_int;

	nthw_register_t *mp_reg_tx_avail_monitor_control;
	nthw_field_t *mp_fld_tx_avail_monitor_control_adr;
	nthw_field_t *mp_fld_tx_avail_monitor_control_cnt;

	nthw_register_t *mp_reg_tx_avail_monitor_data;
	nthw_field_t *mp_fld_tx_avail_monitor_data_guest_physical_address;
	nthw_field_t *mp_fld_tx_avail_monitor_data_enable;
	nthw_field_t *mp_fld_tx_avail_monitor_data_host_id;
	nthw_field_t *mp_fld_tx_avail_monitor_data_packed;
	nthw_field_t *mp_fld_tx_avail_monitor_data_int;

	nthw_register_t *mp_reg_rx_used_writer_control;
	nthw_field_t *mp_fld_rx_used_writer_control_adr;
	nthw_field_t *mp_fld_rx_used_writer_control_cnt;

	nthw_register_t *mp_reg_rx_used_writer_data;
	nthw_field_t *mp_fld_rx_used_writer_data_guest_physical_address;
	nthw_field_t *mp_fld_rx_used_writer_data_host_id;
	nthw_field_t *mp_fld_rx_used_writer_data_queue_size;
	nthw_field_t *mp_fld_rx_used_writer_data_packed;
	nthw_field_t *mp_fld_rx_used_writer_data_int;
	nthw_field_t *mp_fld_rx_used_writer_data_vec;
	nthw_field_t *mp_fld_rx_used_writer_data_istk;

	nthw_register_t *mp_reg_tx_used_writer_control;
	nthw_field_t *mp_fld_tx_used_writer_control_adr;
	nthw_field_t *mp_fld_tx_used_writer_control_cnt;

	nthw_register_t *mp_reg_tx_used_writer_data;
	nthw_field_t *mp_fld_tx_used_writer_data_guest_physical_address;
	nthw_field_t *mp_fld_tx_used_writer_data_host_id;
	nthw_field_t *mp_fld_tx_used_writer_data_queue_size;
	nthw_field_t *mp_fld_tx_used_writer_data_packed;
	nthw_field_t *mp_fld_tx_used_writer_data_int;
	nthw_field_t *mp_fld_tx_used_writer_data_vec;
	nthw_field_t *mp_fld_tx_used_writer_data_istk;
	nthw_field_t *mp_fld_tx_used_writer_data_in_order;

	nthw_register_t *mp_reg_rx_descriptor_reader_control;
	nthw_field_t *mp_fld_rx_descriptor_reader_control_adr;
	nthw_field_t *mp_fld_rx_descriptor_reader_control_cnt;

	nthw_register_t *mp_reg_rx_descriptor_reader_data;
	nthw_field_t *mp_fld_rx_descriptor_reader_data_guest_physical_address;
	nthw_field_t *mp_fld_rx_descriptor_reader_data_host_id;
	nthw_field_t *mp_fld_rx_descriptor_reader_data_queue_size;
	nthw_field_t *mp_fld_rx_descriptor_reader_data_header;
	nthw_field_t *mp_fld_rx_descriptor_reader_data_packed;

	nthw_register_t *mp_reg_tx_descriptor_reader_control;
	nthw_field_t *mp_fld_tx_descriptor_reader_control_adr;
	nthw_field_t *mp_fld_tx_descriptor_reader_control_cnt;

	nthw_register_t *mp_reg_tx_descriptor_reader_data;
	nthw_field_t *mp_fld_tx_descriptor_reader_data_guest_physical_address;
	nthw_field_t *mp_fld_tx_descriptor_reader_data_host_id;
	nthw_field_t *mp_fld_tx_descriptor_reader_data_queue_size;
	nthw_field_t *mp_fld_tx_descriptor_reader_data_port;
	nthw_field_t *mp_fld_tx_descriptor_reader_data_header;
	nthw_field_t *mp_fld_tx_descriptor_reader_data_packed;

	nthw_register_t *mp_reg_tx_queue_property_control;
	nthw_field_t *mp_fld_tx_queue_property_control_adr;
	nthw_field_t *mp_fld_tx_queue_property_control_cnt;

	nthw_register_t *mp_reg_tx_queue_property_data;
	nthw_field_t *mp_fld_tx_queue_property_data_v_port;

	struct nthw_dbs_rx_am_data_s m_rx_am_shadow[NT_DBS_RX_QUEUES_MAX];
	struct nthw_dbs_rx_uw_data_s m_rx_uw_shadow[NT_DBS_RX_QUEUES_MAX];
	struct nthw_dbs_rx_dr_data_s m_rx_dr_shadow[NT_DBS_RX_QUEUES_MAX];

	struct nthw_dbs_tx_am_data_s m_tx_am_shadow[NT_DBS_TX_QUEUES_MAX];
	struct nthw_dbs_tx_uw_data_s m_tx_uw_shadow[NT_DBS_TX_QUEUES_MAX];
	struct nthw_dbs_tx_dr_data_s m_tx_dr_shadow[NT_DBS_TX_QUEUES_MAX];
	struct nthw_dbs_tx_qp_data_s m_tx_qp_shadow[NT_DBS_TX_QUEUES_MAX];
};

typedef struct nthw_dbs_s nthw_dbs_t;

nthw_dbs_t *nthw_dbs_new(void);
int nthw_dbs_init(nthw_dbs_t *p, nthw_fpga_t *p_fpga, int n_instance);
void nthw_dbs_reset(nthw_dbs_t *p);

int nthw_set_rx_control(nthw_dbs_t *p,
	uint32_t last_queue,
	uint32_t avail_monitor_enable,
	uint32_t avail_monitor_speed,
	uint32_t used_write_enable,
	uint32_t used_write_speed,
	uint32_t rx_queue_enable);
int nthw_set_tx_control(nthw_dbs_t *p,
	uint32_t last_queue,
	uint32_t avail_monitor_enable,
	uint32_t avail_monitor_speed,
	uint32_t used_write_enable,
	uint32_t used_write_speed,
	uint32_t tx_queue_enable);
int nthw_set_rx_init(nthw_dbs_t *p, uint32_t start_idx, uint32_t start_ptr, uint32_t init,
	uint32_t queue);
int nthw_get_rx_init(nthw_dbs_t *p, uint32_t *init, uint32_t *queue, uint32_t *busy);
int nthw_set_tx_init(nthw_dbs_t *p, uint32_t start_idx, uint32_t start_ptr, uint32_t init,
	uint32_t queue);
int nthw_get_tx_init(nthw_dbs_t *p, uint32_t *init, uint32_t *queue, uint32_t *busy);
int nthw_set_rx_idle(nthw_dbs_t *p, uint32_t idle, uint32_t queue);
int nthw_get_rx_idle(nthw_dbs_t *p, uint32_t *idle, uint32_t *queue, uint32_t *busy);
int nthw_set_tx_idle(nthw_dbs_t *p, uint32_t idle, uint32_t queue);
int nthw_get_tx_idle(nthw_dbs_t *p, uint32_t *idle, uint32_t *queue, uint32_t *busy);
int nthw_set_rx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable);
int nthw_set_rx_am_data_enable(nthw_dbs_t *p, uint32_t index, uint32_t enable);
int nthw_set_tx_am_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t enable,
	uint32_t host_id,
	uint32_t packed,
	uint32_t int_enable);
int nthw_set_tx_am_data_enable(nthw_dbs_t *p, uint32_t index, uint32_t enable);
int nthw_set_rx_uw_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t host_id,
	uint32_t queue_size,
	uint32_t packed,
	uint32_t int_enable,
	uint32_t vec,
	uint32_t istk);
int nthw_set_tx_uw_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t host_id,
	uint32_t queue_size,
	uint32_t packed,
	uint32_t int_enable,
	uint32_t vec,
	uint32_t istk,
	uint32_t in_order);
int nthw_set_rx_dr_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t host_id,
	uint32_t queue_size,
	uint32_t header,
	uint32_t packed);
int nthw_set_tx_dr_data(nthw_dbs_t *p,
	uint32_t index,
	uint64_t guest_physical_address,
	uint32_t host_id,
	uint32_t queue_size,
	uint32_t port,
	uint32_t header,
	uint32_t packed);
int nthw_dbs_set_tx_qp_data(nthw_dbs_t *p, uint32_t index, uint32_t virtual_port);

#endif	/* _NTNIC_DBS_H_ */
