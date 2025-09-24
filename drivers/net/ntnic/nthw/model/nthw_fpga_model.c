/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include <time.h>

#include "nthw_drv.h"
#include "nthw_register.h"
#include "nthw_fpga_model.h"
#include "nthw_rac.h"
#include "ntlog.h"

/*
 * NOTE: this needs to be (manually) synced with enum nthw_fpga_bus_type
 */
static const char *const sa_nthw_fpga_bus_type_str[] = {
	"ERR",	/* NTHW_FPGA_BUS_TYPE_UNKNOWN, */
	"BAR",	/* NTHW_FPGA_BUS_TYPE_BAR, */
	"PCI",	/* NTHW_FPGA_BUS_TYPE_PCI, */
	"CCIP",	/* NTHW_FPGA_BUS_TYPE_CCIP, */
	"RAB0",	/* NTHW_FPGA_BUS_TYPE_RAB0, */
	"RAB1",	/* NTHW_FPGA_BUS_TYPE_RAB1, */
	"RAB2",	/* NTHW_FPGA_BUS_TYPE_RAB2, */
	"NMB",	/* NTHW_FPGA_BUS_TYPE_NMB, */
	"NDM",	/* NTHW_FPGA_BUS_TYPE_NDM, */
};

static const char *get_bus_name(int n_bus_type_id)
{
	if (n_bus_type_id >= 0 && n_bus_type_id < (int)ARRAY_SIZE(sa_nthw_fpga_bus_type_str))
		return sa_nthw_fpga_bus_type_str[n_bus_type_id];

	return "ERR";
}

/*
 * module name lookup by id from array
 * Uses naive linear search as performance is not an issue here...
 */

static const struct {
	const nthw_id_t a;
	const char *b;
} *sa_nthw_fpga_mod_str_map;

static const char *nthw_fpga_mod_id_to_str(nthw_id_t n_fpga_mod_id)
{
	int i;

	if (sa_nthw_fpga_mod_str_map) {
		for (i = 0; sa_nthw_fpga_mod_str_map[i].a && sa_nthw_fpga_mod_str_map[i].b; i++)
			if ((nthw_id_t)sa_nthw_fpga_mod_str_map[i].a == n_fpga_mod_id)
				return sa_nthw_fpga_mod_str_map[i].b;
	}

	return "unknown";
}

static int nthw_read_data(struct fpga_info_s *p_fpga_info, bool trc, int n_bus_type_id,
	uint32_t addr, uint32_t len, uint32_t *p_data)
{
	int rc = -1;

	RTE_ASSERT(p_fpga_info);
	RTE_ASSERT(p_data);
	RTE_ASSERT(len >= 1);

	switch (n_bus_type_id) {
	case NTHW_FPGA_BUS_TYPE_BAR:
	case NTHW_FPGA_BUS_TYPE_PCI:
		nthw_rac_bar0_read32(p_fpga_info, addr, len, p_data);
		rc = 0;
		break;

	case NTHW_FPGA_BUS_TYPE_RAB0:
		RTE_ASSERT(p_fpga_info->mp_nthw_rac);
		rc = nthw_rac_rab_read32(p_fpga_info->mp_nthw_rac, trc, 0, addr, len, p_data);
		break;

	case NTHW_FPGA_BUS_TYPE_RAB1:
		RTE_ASSERT(p_fpga_info->mp_nthw_rac);
		rc = nthw_rac_rab_read32(p_fpga_info->mp_nthw_rac, trc, 1, addr, len, p_data);
		break;

	case NTHW_FPGA_BUS_TYPE_RAB2:
		RTE_ASSERT(p_fpga_info->mp_nthw_rac);
		rc = nthw_rac_rab_read32(p_fpga_info->mp_nthw_rac, trc, 2, addr, len, p_data);
		break;

	default:
		RTE_ASSERT(false);
		return -1;
	}

	return rc;
}

static int nthw_write_data(struct fpga_info_s *p_fpga_info, bool trc, int n_bus_type_id,
	uint32_t addr, uint32_t len, const uint32_t *p_data)
{
	int rc = -1;

	RTE_ASSERT(p_fpga_info);
	RTE_ASSERT(p_data);
	RTE_ASSERT(len >= 1);

	switch (n_bus_type_id) {
	case NTHW_FPGA_BUS_TYPE_BAR:
	case NTHW_FPGA_BUS_TYPE_PCI:
		nthw_rac_bar0_write32(p_fpga_info, addr, len, p_data);
		rc = 0;
		break;

	case NTHW_FPGA_BUS_TYPE_RAB0:
		RTE_ASSERT(p_fpga_info->mp_nthw_rac);
		rc = nthw_rac_rab_write32(p_fpga_info->mp_nthw_rac, trc, 0, addr, len, p_data);
		break;

	case NTHW_FPGA_BUS_TYPE_RAB1:
		RTE_ASSERT(p_fpga_info->mp_nthw_rac);
		rc = nthw_rac_rab_write32(p_fpga_info->mp_nthw_rac, trc, 1, addr, len, p_data);
		break;

	case NTHW_FPGA_BUS_TYPE_RAB2:
		RTE_ASSERT(p_fpga_info->mp_nthw_rac);
		rc = nthw_rac_rab_write32(p_fpga_info->mp_nthw_rac, trc, 2, addr, len, p_data);
		break;

	default:
		RTE_ASSERT(false);
		return -1;
	}

	return rc;
}

uint64_t nthw_fpga_read_ident(struct fpga_info_s *p_fpga_info)
{
	uint64_t n_fpga_ident;
	uint32_t n_fpga_ident_lo, n_fpga_ident_hi;
	nthw_rac_bar0_read32(p_fpga_info, 0x0, 1, &n_fpga_ident_lo);
	nthw_rac_bar0_read32(p_fpga_info, 0x8, 1, &n_fpga_ident_hi);
	n_fpga_ident = (((uint64_t)n_fpga_ident_hi << 32) | n_fpga_ident_lo);
	return n_fpga_ident;
}

uint32_t nthw_fpga_read_buildtime(struct fpga_info_s *p_fpga_info)
{
	uint32_t n_fpga_build_time;
	nthw_rac_bar0_read32(p_fpga_info, 0x10, 1, &n_fpga_build_time);
	return n_fpga_build_time;
}

int nthw_fpga_extract_type_id(const uint64_t n_fpga_ident)
{
	return (uint16_t)(n_fpga_ident >> 32) & 0xFF;
}

int nthw_fpga_extract_prod_id(const uint64_t n_fpga_ident)
{
	return (uint16_t)(n_fpga_ident >> 16) & 0xFFFF;
}

int nthw_fpga_extract_ver_id(const uint64_t n_fpga_ident)
{
	return (uint16_t)((n_fpga_ident >> 8) & 0xFF);
}

int nthw_fpga_extract_rev_id(const uint64_t n_fpga_ident)
{
	return (uint16_t)(n_fpga_ident & 0xFF);
}
/*
 * Field
 */
nthw_field_t *nthw_field_new(void)
{
	nthw_field_t *p = malloc(sizeof(nthw_field_t));

	return p;
}

static void nthw_field_init(nthw_field_t *p, nthw_register_t *p_reg,
	const nthw_fpga_field_init_s *p_init)
{
	p->mp_owner = p_reg;

	p->mn_debug_mode = p_reg->mn_debug_mode;

	p->mn_id = p_init->id;
	p->mn_bit_width = p_init->bw;
	p->mn_bit_pos_low = p_init->low;
	p->mn_reset_val = (uint32_t)p_init->reset_val;
	p->mn_first_word = p_init->low / 32;
	p->mn_first_bit = p_init->low % 32;
	p->mn_front_mask = 0;
	p->mn_body_length = 0;
	p->mn_words = (p_init->bw + 0x1f) / 0x20;
	p->mn_tail_mask = 0;

	{
		int bits_remaining = p_init->bw;
		int front_mask_length = 32 - p->mn_first_bit;

		if (front_mask_length > bits_remaining)
			front_mask_length = bits_remaining;

		bits_remaining -= front_mask_length;

		p->mn_front_mask =
			(uint32_t)(((1ULL << front_mask_length) - 1) << p->mn_first_bit);

		p->mn_body_length = bits_remaining / 32;
		bits_remaining -= p->mn_body_length * 32;
		p->mn_tail_mask = (1 << bits_remaining) - 1;

		if (p->mn_debug_mode >= 0x100) {
			NT_LOG_DBGX(DBG, NTHW,
				"fldid=%08" PRIu32 ": [%08" PRIu16 ":%08" PRIu16 "] %08" PRIu16 "/%08"
				PRIu16 ": (%08i ,%08" PRIu32 ") (0x%08" PRIx32 ",%08"
				PRIu32 ",0x%08" PRIx32 ")",
				p_init->id, p_init->low, (p_init->low + p_init->bw),
				p_init->bw, ((p_init->bw + 31) / 32), p->mn_first_word,
				p->mn_first_bit, p->mn_front_mask, p->mn_body_length,
				p->mn_tail_mask);
		}
	}
}

/*
 * Register
 */
nthw_register_t *nthw_register_new(void)
{
	nthw_register_t *p = malloc(sizeof(nthw_register_t));

	return p;
}

static void nthw_register_init(nthw_register_t *p, nthw_module_t *p_module,
	nthw_fpga_register_init_s *p_init)
{
	int i;

	p->mp_owner = p_module;

	p->mn_id = p_init->id;
	p->mn_bit_width = p_init->bw;
	p->mn_addr_rel = p_init->addr_rel;
	p->mn_addr = p_module->mn_addr_base + p_init->addr_rel;
	p->mn_type = p_init->type;
	/* Old P200 registers have no bw at register level - default to BW=-1 */
	p->mn_len = ((p_init->bw != (uint16_t)-1) ? ((p_init->bw + 31) >> 5) : 1);
	p->mn_debug_mode = p_module->mn_debug_mode;

	p->mn_fields = p_init->nb_fields;

	if (p->mn_fields) {
		p->mpa_fields = malloc(p->mn_fields * sizeof(nthw_field_t *));

		if (p->mpa_fields) {
			memset(p->mpa_fields, 0, (p->mn_fields * sizeof(nthw_field_t *)));

			for (i = 0; i < p->mn_fields; i++) {
				nthw_field_t *p_field = nthw_field_new();

				nthw_field_init(p_field, p, &p_init->fields[i]);
				p->mpa_fields[i] = p_field;
			}

			p->mp_shadow = malloc(p->mn_len * sizeof(uint32_t));

			if (p->mp_shadow)
				memset(p->mp_shadow, 0x00, (p->mn_len * sizeof(uint32_t)));

			p->mp_dirty = malloc(p->mn_len * sizeof(bool));

			if (p->mp_dirty)
				memset(p->mp_dirty, 0x00, (p->mn_len * sizeof(bool)));
		}
	}
}

/*
 * Param
 */
nthw_param_t *nthw_param_new(void)
{
	nthw_param_t *p = malloc(sizeof(nthw_param_t));

	return p;
}

static void nthw_param_init(nthw_param_t *p, nthw_fpga_t *p_fpga, nthw_fpga_prod_param_s *p_init)
{
	p->mp_owner = p_fpga;
	p->mp_init = p_init;

	p->mn_param_id = p_init->id;
	p->mn_param_value = p_init->value;
}

/*
 * Module
 */
nthw_module_t *nthw_module_new(void)
{
	nthw_module_t *p = malloc(sizeof(nthw_module_t));

	return p;
}

static void nthw_module_init(nthw_module_t *p, nthw_fpga_t *p_fpga, nthw_fpga_module_init_s *p_init)
{
	int i;

	p->mp_owner = p_fpga;
	p->mp_init = p_init;

	p->mn_mod_id = p_init->id;
	p->mn_instance = p_init->instance;

	/* Copy debug mode from owner */
	if (p->mp_owner)
		p->mn_debug_mode = p->mp_owner->m_debug_mode;

	else
		p->mn_debug_mode = 0;

	p->mn_mod_def_id = p_init->def_id;
	p->mn_major_version = p_init->major_version;
	p->mn_minor_version = p_init->minor_version;
	p->mn_bus = p_init->bus_id;
	p->mn_addr_base = p_init->addr_base;

	p->mn_registers = p_init->nb_registers;

	if (p->mn_registers) {
		p->mpa_registers = malloc(p->mn_registers * sizeof(nthw_register_t *));

		if (p->mpa_registers) {
			memset(p->mpa_registers, 0, (p->mn_registers * sizeof(nthw_register_t *)));

			for (i = 0; i < p->mn_registers; i++) {
				nthw_register_t *p_reg = nthw_register_new();

				nthw_register_init(p_reg, p, &p_init->registers[i]);
				p->mpa_registers[i] = p_reg;
			}
		}
	}
}

/*
 * FpgaMgr
 */
nthw_fpga_mgr_t *nthw_fpga_mgr_new(void)
{
	nthw_fpga_mgr_t *p = malloc(sizeof(nthw_fpga_mgr_t));
	return p;
}

void nthw_fpga_mgr_delete(nthw_fpga_mgr_t *p)
{
	free(p);
}

void nthw_fpga_mgr_init(nthw_fpga_mgr_t *p, struct nthw_fpga_prod_init **pa_nthw_fpga_instances,
	const void *pa_mod_str_map)
{
	size_t i = 0;

	p->mpa_fpga_prod_init = pa_nthw_fpga_instances;
	sa_nthw_fpga_mod_str_map = pa_mod_str_map;

	/* Count fpga instance in array */
	if (pa_nthw_fpga_instances) {
		for (i = 0;; i++)
			if (p->mpa_fpga_prod_init[i] == NULL)
				break;
	}

	p->mn_fpgas = (int)i;
}

void nthw_fpga_mgr_log_dump(nthw_fpga_mgr_t *p)
{
	int i;

	NT_LOG_DBGX(DBG, NTHW, "fpgas=%d", p->mn_fpgas);

	for (i = 0; i < p->mn_fpgas; i++) {
		nthw_fpga_prod_init_s *p_init = p->mpa_fpga_prod_init[i];
		(void)p_init;
		NT_LOG_DBGX(DBG, NTHW, "fpga=%d/%d: %04d-%02d-%02d", i, p->mn_fpgas,
			p_init->fpga_product_id, p_init->fpga_version, p_init->fpga_revision);
	}
}

/*
 * Fpga
 */
nthw_fpga_t *nthw_fpga_model_new(void)
{
	nthw_fpga_t *p = malloc(sizeof(nthw_fpga_t));

	if (p)
		memset(p, 0, sizeof(nthw_fpga_t));

	return p;
}

static void nthw_fpga_model_init(nthw_fpga_t *p, nthw_fpga_prod_init_s *p_init,
	struct fpga_info_s *p_fpga_info)
{
	int i;

	p->p_fpga_info = p_fpga_info;
	p->mp_init = p_init;

	p->mn_item_id = p_init->fpga_item_id;
	p->mn_product_id = p_init->fpga_product_id;
	p->mn_fpga_version = p_init->fpga_version;
	p->mn_fpga_revision = p_init->fpga_revision;
	p->mn_fpga_patch_no = p_init->fpga_patch_no;
	p->mn_fpga_build_no = p_init->fpga_build_no;
	p->mn_fpga_build_time = p_init->fpga_build_time;

	p->mn_params = p_init->nb_prod_params;

	if (p->mn_params) {
		p->mpa_params = malloc(p->mn_params * sizeof(nthw_param_t *));

		if (p->mpa_params) {
			memset(p->mpa_params, 0, (p->mn_params * sizeof(nthw_param_t *)));

			for (i = 0; i < p->mn_params; i++) {
				nthw_param_t *p_param = nthw_param_new();

				nthw_param_init(p_param, p, &p_init->product_params[i]);
				p->mpa_params[i] = p_param;
			}
		}
	}

	p->mn_modules = p_init->nb_modules;

	if (p->mn_modules) {
		p->mpa_modules = malloc(p_init->nb_modules * sizeof(nthw_module_t *));

		if (p->mpa_modules) {
			memset(p->mpa_modules, 0, (p->mn_modules * sizeof(nthw_module_t *)));

			for (i = 0; i < p->mn_modules; i++) {
				nthw_module_t *p_mod = nthw_module_new();

				nthw_module_init(p_mod, p, &p_init->modules[i]);
				p->mpa_modules[i] = p_mod;
			}
		}
	}
}

static nthw_module_t *nthw_fpga_lookup_module(const nthw_fpga_t *p, nthw_id_t id, int instance)
{
	int i;

	for (i = 0; i < p->mn_modules; i++) {
		nthw_module_t *p_mod = p->mpa_modules[i];

		if (p_mod->mn_mod_id == id && p_mod->mn_instance == instance)
			return p_mod;
	}

	return NULL;
}

nthw_module_t *nthw_fpga_query_module(const nthw_fpga_t *p, nthw_id_t id, int instance)
{
	return nthw_fpga_lookup_module(p, id, instance);
}

int nthw_fpga_get_product_param(const nthw_fpga_t *p, const nthw_id_t n_param_id,
	const int n_default_value)
{
	int i;

	for (i = 0; i < p->mn_params; i++) {
		nthw_param_t *p_param = p->mpa_params[i];

		if (p_param->mn_param_id == n_param_id)
			return p_param->mn_param_value;
	}

	return n_default_value;
}

int nthw_fpga_get_product_id(const nthw_fpga_t *p)
{
	return p->mn_product_id;
}

int nthw_module_get_major_version(const nthw_module_t *p)
{
	return p->mn_major_version;
}

int nthw_module_get_minor_version(const nthw_module_t *p)
{
	return p->mn_minor_version;
}

uint64_t nthw_module_get_version_packed64(const nthw_module_t *p)
{
	return (((uint64_t)p->mn_major_version & 0xFFFFFFFF) << 32) |
		(p->mn_minor_version & 0xFFFFFFFF);
}

bool nthw_module_is_version_newer(const nthw_module_t *p, int major_version, int minor_version)
{
	if (major_version == p->mn_major_version)
		return p->mn_minor_version >= minor_version;

	return p->mn_major_version >= major_version;
}

static nthw_register_t *nthw_module_lookup_register(nthw_module_t *p, nthw_id_t id)
{
	int i;
	nthw_register_t *p_register = NULL;

	for (i = 0; i < p->mn_registers; i++) {
		if (p->mpa_registers[i]->mn_id == id) {
			p_register = p->mpa_registers[i];
			break;
		}
	}

	return p_register;
}

nthw_register_t *nthw_module_query_register(nthw_module_t *p, nthw_id_t id)
{
	return nthw_module_lookup_register(p, id);
}

nthw_register_t *nthw_module_get_register(nthw_module_t *p, nthw_id_t id)
{
	nthw_register_t *p_register;

	if (p == NULL) {
		NT_LOG(ERR, NTHW, "Illegal module context for register %u", id);
		return NULL;
	}

	p_register = nthw_module_lookup_register(p, id);

	if (!p_register) {
		NT_LOG(ERR, NTHW, "Register %u not found in module: %s (%u)", id,
			nthw_fpga_mod_id_to_str(p->mn_mod_id), p->mn_mod_id);
	}

	return p_register;
}

int nthw_module_get_debug_mode(const nthw_module_t *p)
{
	return p->mn_debug_mode;
}

void nthw_module_set_debug_mode(nthw_module_t *p, unsigned int debug_mode)
{
	int i;

	p->mn_debug_mode = debug_mode;

	for (i = 0; i < p->mn_registers; i++) {
		nthw_register_t *p_register = p->mpa_registers[i];

		if (p_register)
			nthw_register_set_debug_mode(p_register, debug_mode);
	}
}

static int nthw_module_get_bus(const nthw_module_t *p)
{
	return p->mn_bus;
}

uint32_t nthw_register_get_address(const nthw_register_t *p)
{
	return p->mn_addr;
}

static nthw_field_t *nthw_register_lookup_field(const nthw_register_t *p, nthw_id_t id)
{
	int i;
	nthw_field_t *p_field = NULL;

	if (!p)
		return NULL;

	for (i = 0; i < p->mn_fields; i++) {
		if (p->mpa_fields[i]->mn_id == id) {
			p_field = p->mpa_fields[i];
			break;
		}
	}

	return p_field;
}

nthw_field_t *nthw_register_query_field(const nthw_register_t *p, nthw_id_t id)
{
	return nthw_register_lookup_field(p, id);
}

nthw_field_t *nthw_register_get_field(const nthw_register_t *p, nthw_id_t id)
{
	nthw_field_t *p_field;

	if (p == NULL) {
		NT_LOG(ERR, NTHW, "Illegal register context for field %u", id);
		return NULL;
	}

	p_field = nthw_register_lookup_field(p, id);

	if (!p_field) {
		NT_LOG(ERR, NTHW, "Field %u not found in module: %s (%u)", id,
			nthw_fpga_mod_id_to_str(p->mp_owner->mn_mod_id), p->mp_owner->mn_mod_id);
	}

	return p_field;
}

/*
 * NOTE: do not set debug on fields - as register operation dumps typically are enough
 */
static void nthw_field_set_debug_mode(nthw_field_t *p, unsigned int debug_mode)
{
	p->mn_debug_mode = debug_mode;
}

void nthw_register_set_debug_mode(nthw_register_t *p, unsigned int debug_mode)
{
	int i;

	p->mn_debug_mode = debug_mode;

	for (i = 0; i < p->mn_fields; i++) {
		nthw_field_t *p_field = p->mpa_fields[i];

		if (p_field)
			nthw_field_set_debug_mode(p_field, debug_mode);
	}
}

static int nthw_register_read_data(const nthw_register_t *p)
{
	int rc = -1;
	if (p) {
		if (p->mp_owner) {
			const int n_bus_type_id = nthw_module_get_bus(p->mp_owner);
			const uint32_t addr = p->mn_addr;
			const uint32_t len = p->mn_len;
			uint32_t *const p_data = p->mp_shadow;
			const bool trc = (p->mn_debug_mode & NTHW_REG_TRACE_ON_READ);

			struct fpga_info_s *p_fpga_info = NULL;

			if (p->mp_owner->mp_owner)
				p_fpga_info = p->mp_owner->mp_owner->p_fpga_info;

			RTE_ASSERT(p_fpga_info);
			RTE_ASSERT(p_data);

			rc = nthw_read_data(p_fpga_info, trc, n_bus_type_id, addr, len, p_data);
		}
	}
	return rc;
}

static int nthw_register_write_data(const nthw_register_t *p, uint32_t cnt)
{
	int rc = -1;
	if (p) {
		if (p->mp_owner) {
			const int n_bus_type_id = nthw_module_get_bus(p->mp_owner);
			const uint32_t addr = p->mn_addr;
			const uint32_t len = p->mn_len;
			uint32_t *const p_data = p->mp_shadow;
			const bool trc = (p->mn_debug_mode & NTHW_REG_TRACE_ON_WRITE);

			struct fpga_info_s *p_fpga_info = NULL;

			if (p->mp_owner->mp_owner)
				p_fpga_info = p->mp_owner->mp_owner->p_fpga_info;

			RTE_ASSERT(p_fpga_info);
			RTE_ASSERT(p_data);

			rc = nthw_write_data(p_fpga_info, trc, n_bus_type_id, addr, (len * cnt),
				p_data);
		}
	}
	return rc;
}

void nthw_register_update(const nthw_register_t *p)
{
	if (p && p->mn_type != NTHW_FPGA_REG_TYPE_WO) {
		const char *const p_dev_name = "NA";
		(void)p_dev_name;
		const int n_bus_type_id = nthw_module_get_bus(p->mp_owner);
		const char *const p_bus_name = get_bus_name(n_bus_type_id);
		(void)p_bus_name;
		const uint32_t addr = p->mn_addr;
		(void)addr;
		const uint32_t len = p->mn_len;
		uint32_t *const p_data = p->mp_shadow;

		nthw_register_read_data(p);

		if (p->mn_debug_mode & NTHW_REG_DEBUG_ON_READ) {
			uint32_t i = len;
			uint32_t *ptr = p_data;
			(void)ptr;
			char *tmp_string = ntlog_helper_str_alloc("Register::read");
			ntlog_helper_str_add(tmp_string,
				"(Dev: %s, Bus: %s, Addr: 0x%08X, Cnt: %d, Data:",
				p_dev_name, p_bus_name, addr, len);

			while (i--)
				ntlog_helper_str_add(tmp_string, " 0x%08X", *ptr++);

			ntlog_helper_str_add(tmp_string, ")");
			NT_LOG(DBG, NTHW, "%s", tmp_string);
			ntlog_helper_str_free(tmp_string);
		}
	}
}

void nthw_register_make_dirty(nthw_register_t *p)
{
	uint32_t i;

	for (i = 0; i < p->mn_len; i++)
		p->mp_dirty[i] = true;
}

void nthw_register_set_val(nthw_register_t *p, const uint32_t *p_data, uint32_t len)
{
	RTE_ASSERT(len <= p->mn_len);
	RTE_ASSERT(p_data);

	if (len == (uint32_t)-1 || len > p->mn_len)
		len = p->mn_len;

	if (p->mp_shadow != p_data)
		memcpy(p->mp_shadow, p_data, (len * sizeof(uint32_t)));
}

void nthw_register_flush(const nthw_register_t *p, uint32_t cnt)
{
	int rc;

	if (p->mn_type != NTHW_FPGA_REG_TYPE_RO) {
		const char *const p_dev_name = "NA";
		const int n_bus_type_id = nthw_module_get_bus(p->mp_owner);
		const char *p_bus_name = get_bus_name(n_bus_type_id);
		const uint32_t addr = p->mn_addr;
		const uint32_t len = p->mn_len;
		uint32_t *const p_data = p->mp_shadow;
		uint32_t i;

		RTE_ASSERT(len * cnt <= 256);

		if (p->mn_debug_mode & NTHW_REG_DEBUG_ON_WRITE) {
			uint32_t i = len * cnt;
			uint32_t *ptr = p_data;
			char *tmp_string = ntlog_helper_str_alloc("Register::write");

			ntlog_helper_str_add(tmp_string,
				"(Dev: %s, Bus: %s, Addr: 0x%08X, Cnt: %d, Data:",
				p_dev_name, p_bus_name, addr, i);

			while (i--)
				ntlog_helper_str_add(tmp_string, " 0x%08X", *ptr++);

			ntlog_helper_str_add(tmp_string, ")");
			NT_LOG(DBG, NTHW, "%s", tmp_string);
			ntlog_helper_str_free(tmp_string);
		}

		rc = nthw_register_write_data(p, cnt);

		if (rc)
			NT_LOG(ERR, NTHW, "Register write error %d", rc);

		for (i = 0; i < cnt; i++)
			p->mp_dirty[i] = false;
	}
}

int nthw_field_get_bit_width(const nthw_field_t *p)
{
	return p->mn_bit_width;
}

int nthw_field_get_bit_pos_low(const nthw_field_t *p)
{
	return p->mn_bit_pos_low;
}

static int nthw_field_get_bit_pos_high(const nthw_field_t *p)
{
	return p->mn_bit_pos_low + p->mn_bit_width - 1;
}

uint32_t nthw_field_get_mask(const nthw_field_t *p)
{
	return p->mn_front_mask;
}

void nthw_field_get_val(const nthw_field_t *p, uint32_t *p_data, uint32_t len)
{
	uint32_t i;
	uint32_t data_index = 0;
	uint32_t shadow_index = p->mn_first_word;

	union {
		uint32_t w32[2];
		uint64_t w64;
	} buf;

	(void)len;
	RTE_ASSERT(len <= p->mn_words);

	/* handle front */
	buf.w32[0] = p->mp_owner->mp_shadow[shadow_index++] & p->mn_front_mask;

	/* handle body */
	for (i = 0; i < p->mn_body_length; i++) {
		buf.w32[1] = p->mp_owner->mp_shadow[shadow_index++];
		buf.w64 = buf.w64 >> (p->mn_first_bit);
		RTE_ASSERT(data_index < len);
		p_data[data_index++] = buf.w32[0];
		buf.w64 = buf.w64 >> (32 - p->mn_first_bit);
	}

	/* handle tail */
	if (p->mn_tail_mask)
		buf.w32[1] = p->mp_owner->mp_shadow[shadow_index++] & p->mn_tail_mask;

	else
		buf.w32[1] = 0;

	buf.w64 = buf.w64 >> (p->mn_first_bit);
	p_data[data_index++] = buf.w32[0];

	if (data_index < p->mn_words)
		p_data[data_index++] = buf.w32[1];
}

void nthw_field_set_val(const nthw_field_t *p, const uint32_t *p_data, uint32_t len)
{
	uint32_t i;
	uint32_t data_index = 0;
	uint32_t shadow_index = p->mn_first_word;

	union {
		uint32_t w32[2];
		uint64_t w64;
	} buf;

	(void)len;
	RTE_ASSERT(len == p->mn_words);

	/* handle front */
	buf.w32[0] = 0;
	buf.w32[1] = p_data[data_index++];
	buf.w64 = buf.w64 >> (32 - p->mn_first_bit);
	p->mp_owner->mp_shadow[shadow_index] =
		(p->mp_owner->mp_shadow[shadow_index] & ~p->mn_front_mask) |
		(buf.w32[0] & p->mn_front_mask);
	shadow_index++;

	/* handle body */
	for (i = 0; i < p->mn_body_length; i++) {
		buf.w64 = buf.w64 >> (p->mn_first_bit);
		RTE_ASSERT(data_index < len);
		buf.w32[1] = p_data[data_index++];
		buf.w64 = buf.w64 >> (32 - p->mn_first_bit);
		p->mp_owner->mp_shadow[shadow_index++] = buf.w32[0];
	}

	/* handle tail */
	if (p->mn_tail_mask) {
		buf.w64 = buf.w64 >> (p->mn_first_bit);

		if (data_index < len)
			buf.w32[1] = p_data[data_index];

		buf.w64 = buf.w64 >> (32 - p->mn_first_bit);
		p->mp_owner->mp_shadow[shadow_index] =
			(p->mp_owner->mp_shadow[shadow_index] & ~p->mn_tail_mask) |
			(buf.w32[0] & p->mn_tail_mask);
	}

	nthw_register_make_dirty(p->mp_owner);
}

void nthw_field_set_val_flush(const nthw_field_t *p, const uint32_t *p_data, uint32_t len)
{
	nthw_field_set_val(p, p_data, len);
	nthw_field_flush_register(p);
}

uint32_t nthw_field_get_val32(const nthw_field_t *p)
{
	uint32_t val;

	nthw_field_get_val(p, &val, 1);
	return val;
}

int32_t nthw_field_get_signed(const nthw_field_t *p)
{
	uint32_t val;

	nthw_field_get_val(p, &val, 1);

	if (val & (1U << nthw_field_get_bit_pos_high(p)))	/* check sign */
		val = val | ~nthw_field_get_mask(p);	/* sign extension */

	return (int32_t)val;	/* cast to signed value */
}

uint32_t nthw_field_get_updated(const nthw_field_t *p)
{
	uint32_t val;

	nthw_register_update(p->mp_owner);
	nthw_field_get_val(p, &val, 1);

	return val;
}

void nthw_field_update_register(const nthw_field_t *p)
{
	nthw_register_update(p->mp_owner);
}

void nthw_field_flush_register(const nthw_field_t *p)
{
	nthw_register_flush(p->mp_owner, 1);
}

void nthw_field_set_val32(const nthw_field_t *p, uint32_t val)
{
	nthw_field_set_val(p, &val, 1);
}

void nthw_field_set_val_flush32(const nthw_field_t *p, uint32_t val)
{
	nthw_field_set_val(p, &val, 1);
	nthw_register_flush(p->mp_owner, 1);
}

void nthw_field_clr_all(const nthw_field_t *p)
{
	RTE_ASSERT(p->mn_body_length == 0);
	nthw_field_set_val32(p, 0);
}

void nthw_field_clr_flush(const nthw_field_t *p)
{
	nthw_field_clr_all(p);
	nthw_register_flush(p->mp_owner, 1);
}

void nthw_field_set_all(const nthw_field_t *p)
{
	RTE_ASSERT(p->mn_body_length == 0);
	nthw_field_set_val32(p, ~0);
}

void nthw_field_set_flush(const nthw_field_t *p)
{
	nthw_field_set_all(p);
	nthw_register_flush(p->mp_owner, 1);
}

enum nthw_field_match {
	NTHW_FIELD_MATCH_CLR_ALL,
	NTHW_FIELD_MATCH_SET_ALL,
	NTHW_FIELD_MATCH_CLR_ANY,
	NTHW_FIELD_MATCH_SET_ANY,
};

static int nthw_field_wait_cond32(const nthw_field_t *p, enum nthw_field_match e_match,
	int n_poll_iterations, int n_poll_interval)
{
	const uint32_t n_mask = (1 << p->mn_bit_width) - 1;

	if (n_poll_iterations == -1)
		n_poll_iterations = 10000;

	if (n_poll_interval == -1)
		n_poll_interval = 100;	/* usec */

	if (p->mn_debug_mode) {
		const char *const p_cond_name =
			((e_match == NTHW_FIELD_MATCH_SET_ALL)
				? "SetAll"
				: ((e_match == NTHW_FIELD_MATCH_CLR_ALL)
					? "ClrAll"
					: ((e_match == NTHW_FIELD_MATCH_CLR_ANY) ? "ClrAny"
						: "SetAny")));
		(void)p_cond_name;
		const char *const p_dev_name = "NA";
		(void)p_dev_name;
		const char *const p_bus_name =
			get_bus_name(nthw_module_get_bus(p->mp_owner->mp_owner));
		(void)p_bus_name;
		uint32_t n_reg_addr = nthw_register_get_address(p->mp_owner);
		(void)n_reg_addr;
		uint32_t n_reg_mask = (((1 << p->mn_bit_width) - 1) << p->mn_bit_pos_low);
		(void)n_reg_mask;

		NT_LOG(DBG, NTHW,
			"Register::Field::wait%s32(Dev: %s, Bus: %s, Addr: 0x%08X, Mask: 0x%08X, Iterations: %d, Interval: %d)",
			p_cond_name, p_dev_name, p_bus_name, n_reg_addr, n_reg_mask,
			n_poll_iterations, n_poll_interval);
	}

	while (true) {
		uint32_t val = nthw_field_get_updated(p);

		if (e_match == NTHW_FIELD_MATCH_SET_ANY && val != 0)
			return 0;

		if (e_match == NTHW_FIELD_MATCH_SET_ALL && val == n_mask)
			return 0;

		if (e_match == NTHW_FIELD_MATCH_CLR_ALL && val == 0)
			return 0;

		if (e_match == NTHW_FIELD_MATCH_CLR_ANY) {
			uint32_t mask = nthw_field_get_mask(p);

			if (val != mask)
				return 0;
		}

		n_poll_iterations--;

		if (n_poll_iterations <= 0)
			return -1;

		nt_os_wait_usec(n_poll_interval);
	}

	return 0;
}

int nthw_field_wait_set_all32(const nthw_field_t *p, int n_poll_iterations, int n_poll_interval)
{
	return nthw_field_wait_cond32(p, NTHW_FIELD_MATCH_SET_ALL, n_poll_iterations,
			n_poll_interval);
}

int nthw_field_wait_clr_all32(const nthw_field_t *p, int n_poll_iterations, int n_poll_interval)
{
	return nthw_field_wait_cond32(p, NTHW_FIELD_MATCH_CLR_ALL, n_poll_iterations,
			n_poll_interval);
}

int nthw_field_wait_set_any32(const nthw_field_t *p, int n_poll_iterations, int n_poll_interval)
{
	return nthw_field_wait_cond32(p, NTHW_FIELD_MATCH_SET_ANY, n_poll_iterations,
			n_poll_interval);
}

static nthw_fpga_t *nthw_fpga_mgr_lookup_fpga(nthw_fpga_mgr_t *p, uint64_t n_fpga_id,
	struct fpga_info_s *p_fpga_info)
{
	const int n_fpga_prod_id = nthw_fpga_extract_prod_id(n_fpga_id);
	const int n_fpga_ver = nthw_fpga_extract_ver_id(n_fpga_id);
	const int n_fpga_rev = nthw_fpga_extract_rev_id(n_fpga_id);
	int i;

	for (i = 0; i < p->mn_fpgas; i++) {
		nthw_fpga_prod_init_s *p_init = p->mpa_fpga_prod_init[i];

		if (p_init->fpga_product_id == n_fpga_prod_id &&
			p_init->fpga_version == n_fpga_ver && p_init->fpga_revision == n_fpga_rev) {
			nthw_fpga_t *p_fpga = nthw_fpga_model_new();
			nthw_fpga_model_init(p_fpga, p_init, p_fpga_info);
			return p_fpga;
		}
	}

	return NULL;
}

nthw_fpga_t *nthw_fpga_mgr_query_fpga(nthw_fpga_mgr_t *p_fpga_mgr, uint64_t n_fpga_id,
	struct fpga_info_s *p_fpga_info)
{
	const int n_fpga_prod_id = nthw_fpga_extract_prod_id(n_fpga_id);
	const int n_fpga_ver = nthw_fpga_extract_ver_id(n_fpga_id);
	const int n_fpga_rev = nthw_fpga_extract_rev_id(n_fpga_id);

	nthw_fpga_t *p_fpga = nthw_fpga_mgr_lookup_fpga(p_fpga_mgr, n_fpga_id, p_fpga_info);

	if (p_fpga) {
	} else {
		NT_LOG(ERR, NTHW, "FPGA Id 0x%" PRIX64 ": %04d: %d.%d: no match found",
			n_fpga_id, n_fpga_prod_id, n_fpga_ver, n_fpga_rev);
	}

	return p_fpga;
}
