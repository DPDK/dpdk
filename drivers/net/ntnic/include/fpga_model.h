/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _FPGA_MODEL_H_
#define _FPGA_MODEL_H_

#include <unistd.h>
#include <stdint.h>
#include <inttypes.h>

typedef uint32_t nthw_id_t;

enum nthw_fpga_bus_type {
	NTHW_FPGA_BUS_TYPE_UNKNOWN =
		0,	/* Unknown/uninitialized - keep this as the first enum element */
	NTHW_FPGA_BUS_TYPE_BAR,
	NTHW_FPGA_BUS_TYPE_PCI,
	NTHW_FPGA_BUS_TYPE_CCIP,
	NTHW_FPGA_BUS_TYPE_RAB0,
	NTHW_FPGA_BUS_TYPE_RAB1,
	NTHW_FPGA_BUS_TYPE_RAB2,
	NTHW_FPGA_BUS_TYPE_NMB,
	NTHW_FPGA_BUS_TYPE_NDM,
	NTHW_FPGA_BUS_TYPE_SPI0,
	NTHW_FPGA_BUS_TYPE_SPI = NTHW_FPGA_BUS_TYPE_SPI0,
};

typedef enum nthw_fpga_bus_type nthw_fpga_bus_type_e;

enum nthw_fpga_register_type {
	NTHW_FPGA_REG_TYPE_UNKNOWN =
		0,	/* Unknown/uninitialized - keep this as the first enum element */
	NTHW_FPGA_REG_TYPE_RW,
	NTHW_FPGA_REG_TYPE_RO,
	NTHW_FPGA_REG_TYPE_WO,
	NTHW_FPGA_REG_TYPE_RC1,
	NTHW_FPGA_REG_TYPE_MIXED,
};

typedef enum nthw_fpga_register_type nthw_fpga_register_type_e;

struct nthw_fpga_field_init {
	nthw_id_t id;
	uint16_t bw;
	uint16_t low;
	uint64_t reset_val;
};

typedef struct nthw_fpga_field_init nthw_fpga_field_init_s;

struct nthw_fpga_register_init {
	nthw_id_t id;
	uint32_t addr_rel;
	uint16_t bw;
	nthw_fpga_register_type_e type;
	uint64_t reset_val;
	int nb_fields;
	struct nthw_fpga_field_init *fields;
};

typedef struct nthw_fpga_register_init nthw_fpga_register_init_s;

struct nthw_fpga_module_init {
	nthw_id_t id;
	int instance;
	nthw_id_t def_id;
	int major_version;
	int minor_version;
	nthw_fpga_bus_type_e bus_id;
	uint32_t addr_base;
	int nb_registers;
	struct nthw_fpga_register_init *registers;
};

typedef struct nthw_fpga_module_init nthw_fpga_module_init_s;

struct nthw_fpga_prod_param {
	const nthw_id_t id;
	const int value;
};

typedef struct nthw_fpga_prod_param nthw_fpga_prod_param_s;

struct nthw_fpga_prod_init {
	int fpga_item_id;
	int fpga_product_id;
	int fpga_version;
	int fpga_revision;
	int fpga_patch_no;
	int fpga_build_no;
	uint32_t fpga_build_time;
	int nb_prod_params;
	struct nthw_fpga_prod_param *product_params;
	int nb_modules;
	struct nthw_fpga_module_init *modules;
};

typedef struct nthw_fpga_prod_init nthw_fpga_prod_init_s;

#endif	/* _FPGA_MODEL_H_ */
