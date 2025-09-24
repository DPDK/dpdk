/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _NTHW_FPGA_MODEL_H_
#define _NTHW_FPGA_MODEL_H_

#include <stdbool.h>
#include "fpga_model.h"

#define VERSION_PACKED64(_major_, _minor_)                                                        \
	((((uint64_t)(_major_) & (0xFFFFFFFF)) << 32) | ((_minor_) & (0xFFFFFFFF)))

enum nthw_reg_debug_mode {
	NTHW_REG_DEBUG_NONE = 0,
	NTHW_REG_DEBUG_ON_READ = 1,
	NTHW_REG_DEBUG_ON_WRITE = 2,
	NTHW_REG_TRACE_ON_READ = 4,
	NTHW_REG_TRACE_ON_WRITE = 8,
};

struct nthw_fpga_s;

struct nthw_param_s;

struct nthw_module_s;

struct nthw_register_s;

struct nthw_field_s;

struct nthw_fpga_mgr_s {
	int mn_fpgas;
	struct nthw_fpga_prod_init **mpa_fpga_prod_init;
};

typedef struct nthw_fpga_mgr_s nthw_fpga_mgr_t;

struct nthw_fpga_s {
	struct fpga_info_s *p_fpga_info;

	int mn_item_id;
	int mn_product_id;
	int mn_fpga_version;
	int mn_fpga_revision;
	int mn_fpga_patch_no;
	int mn_fpga_build_no;
	uint32_t mn_fpga_build_time;

	int mn_params;
	struct nthw_param_s **mpa_params;

	int mn_modules;
	struct nthw_module_s **mpa_modules;

	nthw_fpga_prod_init_s *mp_init;

	int m_debug_mode;
};

typedef struct nthw_fpga_s nthw_fpga_t;

struct nthw_param_s {
	nthw_fpga_t *mp_owner;

	nthw_id_t mn_param_id;
	int mn_param_value;

	nthw_fpga_prod_param_s *mp_init;
};

typedef struct nthw_param_s nthw_param_t;

struct nthw_module_s {
	nthw_fpga_t *mp_owner;

	nthw_id_t mn_mod_id;

	int mn_instance;

	int mn_mod_def_id;
	int mn_major_version;
	int mn_minor_version;

	int mn_bus;
	uint32_t mn_addr_base;

	int mn_debug_mode;

	int mn_registers;
	struct nthw_register_s **mpa_registers;

	nthw_fpga_module_init_s *mp_init;
};

typedef struct nthw_module_s nthw_module_t;

struct nthw_register_s {
	nthw_module_t *mp_owner;

	nthw_id_t mn_id;

	uint32_t mn_bit_width;
	uint32_t mn_addr_rel;
	uint32_t mn_addr;
	uint32_t mn_type;
	uint32_t mn_len;

	int mn_debug_mode;

	int mn_fields;
	struct nthw_field_s **mpa_fields;

	uint32_t *mp_shadow;
	bool *mp_dirty;

	nthw_fpga_register_init_s *mp_init;
};

typedef struct nthw_register_s nthw_register_t;

struct nthw_field_s {
	nthw_register_t *mp_owner;

	nthw_id_t mn_id;

	uint32_t mn_bit_width;
	uint32_t mn_bit_pos_low;
	uint32_t mn_reset_val;
	uint32_t mn_first_word;
	uint32_t mn_first_bit;
	uint32_t mn_front_mask;
	uint32_t mn_body_length;
	uint32_t mn_words;
	uint32_t mn_tail_mask;

	int mn_debug_mode;

	nthw_fpga_field_init_s *mp_init;
};

typedef struct nthw_field_s nthw_field_t;

int nthw_fpga_extract_type_id(const uint64_t n_fpga_ident);
int nthw_fpga_extract_prod_id(const uint64_t n_fpga_ident);
int nthw_fpga_extract_ver_id(const uint64_t n_fpga_ident);
int nthw_fpga_extract_rev_id(const uint64_t n_fpga_ident);

uint64_t nthw_fpga_read_ident(struct fpga_info_s *p_fpga_info);
uint32_t nthw_fpga_read_buildtime(struct fpga_info_s *p_fpga_info);

nthw_fpga_mgr_t *nthw_fpga_mgr_new(void);
void nthw_fpga_mgr_init(nthw_fpga_mgr_t *p, struct nthw_fpga_prod_init **pa_nthw_fpga_instances,
	const void *pa_mod_str_map);
void nthw_fpga_mgr_delete(nthw_fpga_mgr_t *p);
nthw_fpga_t *nthw_fpga_mgr_query_fpga(nthw_fpga_mgr_t *p, uint64_t n_fpga_id,
	struct fpga_info_s *p_fpga_info);
void nthw_fpga_mgr_log_dump(nthw_fpga_mgr_t *p);

nthw_fpga_t *nthw_fpga_model_new(void);
int nthw_fpga_get_product_param(const nthw_fpga_t *p, const nthw_id_t n_param_id,
	const int default_value);
int nthw_fpga_get_product_id(const nthw_fpga_t *p);

nthw_module_t *nthw_fpga_query_module(const nthw_fpga_t *p, nthw_id_t id, int instance);
nthw_param_t *nthw_param_new(void);
nthw_module_t *nthw_module_new(void);
void nthw_module_init2(nthw_module_t *p, nthw_fpga_t *p_fpga, nthw_id_t mod_id, int instance,
	int debug_mode);

int nthw_module_get_major_version(const nthw_module_t *p);
int nthw_module_get_minor_version(const nthw_module_t *p);
uint64_t nthw_module_get_version_packed64(const nthw_module_t *p);
bool nthw_module_is_version_newer(const nthw_module_t *p, int major_version, int minor_version);

nthw_register_t *nthw_module_query_register(nthw_module_t *p, nthw_id_t id);
nthw_register_t *nthw_module_get_register(nthw_module_t *p, nthw_id_t id);
int nthw_module_get_debug_mode(const nthw_module_t *p);
void nthw_module_set_debug_mode(nthw_module_t *p, unsigned int debug_mode);

nthw_register_t *nthw_register_new(void);
nthw_field_t *nthw_register_query_field(const nthw_register_t *p, nthw_id_t id);
nthw_field_t *nthw_register_get_field(const nthw_register_t *p, nthw_id_t id);

uint32_t nthw_register_get_address(const nthw_register_t *p);
void nthw_register_set_debug_mode(nthw_register_t *p, unsigned int debug_mode);

void nthw_register_set_val(nthw_register_t *p, const uint32_t *p_data, uint32_t len);

void nthw_register_make_dirty(nthw_register_t *p);
void nthw_register_update(const nthw_register_t *p);
void nthw_register_flush(const nthw_register_t *p, uint32_t cnt);
nthw_field_t *nthw_field_new(void);
int nthw_field_get_bit_width(const nthw_field_t *p);
int nthw_field_get_bit_pos_low(const nthw_field_t *p);
uint32_t nthw_field_get_mask(const nthw_field_t *p);
void nthw_field_get_val(const nthw_field_t *p, uint32_t *p_data, uint32_t len);
void nthw_field_set_val(const nthw_field_t *p, const uint32_t *p_data, uint32_t len);
void nthw_field_set_val_flush(const nthw_field_t *p, const uint32_t *p_data, uint32_t len);
uint32_t nthw_field_get_val32(const nthw_field_t *p);
int32_t nthw_field_get_signed(const nthw_field_t *p);
uint32_t nthw_field_get_updated(const nthw_field_t *p);
void nthw_field_update_register(const nthw_field_t *p);
void nthw_field_flush_register(const nthw_field_t *p);
void nthw_field_set_val32(const nthw_field_t *p, uint32_t val);
void nthw_field_set_val_flush32(const nthw_field_t *p, uint32_t val);
void nthw_field_clr_all(const nthw_field_t *p);
void nthw_field_clr_flush(const nthw_field_t *p);
void nthw_field_set_all(const nthw_field_t *p);
void nthw_field_set_flush(const nthw_field_t *p);

int nthw_field_wait_clr_all32(const nthw_field_t *p, int n_poll_iterations, int n_poll_interval);
int nthw_field_wait_set_all32(const nthw_field_t *p, int n_poll_iterations, int n_poll_interval);

int nthw_field_wait_set_any32(const nthw_field_t *p, int n_poll_iterations, int n_poll_interval);

#endif	/* _NTHW_FPGA_MODEL_H_ */
