/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _NTNIC_NIM_H_
#define _NTNIC_NIM_H_

#include <stdint.h>
#include "nthw_pcal6416a.h"
#include "nthw_i2cm.h"

typedef enum i2c_type {
	I2C_HWIIC,
	I2C_HWAGX
} i2c_type_e;

enum nt_port_type_e {
	NT_PORT_TYPE_NOT_AVAILABLE = 0,	/* The NIM/port type is not available (unknown) */
	NT_PORT_TYPE_NOT_RECOGNISED,	/* The NIM/port type not recognized */
	NT_PORT_TYPE_QSFP_PLUS_NOT_PRESENT,	/* QSFP type but slot is empty */
	NT_PORT_TYPE_QSFP_PLUS,	/* QSFP type */
	NT_PORT_TYPE_QSFP28_NOT_PRESENT,/* QSFP28 type but slot is empty */
	NT_PORT_TYPE_QSFP28,	/* QSFP28 type */
	NT_PORT_TYPE_QSFP28_SR4,/* QSFP28-SR4 type */
	NT_PORT_TYPE_QSFP28_LR4,/* QSFP28-LR4 type */
	NT_PORT_TYPE_QSFP28_CR_CA_L,	/* QSFP28-CR-CA-L type */
	NT_PORT_TYPE_QSFP28_CR_CA_S,	/* QSFP28-CR-CA-S type */
	NT_PORT_TYPE_QSFP28_CR_CA_N,	/* QSFP28-CR-CA-N type */
	/* QSFP28-FR type. Uses PAM4 modulation on one lane only */
	NT_PORT_TYPE_QSFP28_FR,
	/* QSFP28-DR type. Uses PAM4 modulation on one lane only */
	NT_PORT_TYPE_QSFP28_DR,
	/* QSFP28-LR type. Uses PAM4 modulation on one lane only */
	NT_PORT_TYPE_QSFP28_LR,
};

typedef enum nt_port_type_e nt_port_type_t, *nt_port_type_p;

typedef struct nim_i2c_ctx {
	union {
		nthw_iic_t hwiic;	/* depends on *Fpga_t, instance number, and cycle time */
		struct {
			nthw_pcal6416a_t *p_io_nim;
			nthw_i2cm_t *p_nt_i2cm;
			nthw_pca9849_t *p_ca9849;
			int mux_channel;
		} hwagx;
	};
	i2c_type_e type;/* 0 = hwiic (xilinx) - 1 =  hwagx (agilex) */
	uint8_t instance;
	uint8_t devaddr;
	uint8_t regaddr;
	uint8_t nim_id;
	nt_port_type_t port_type;

	char vendor_name[17];
	char prod_no[17];
	char serial_no[17];
	char date[9];
	char rev[5];
	bool avg_pwr;
	bool content_valid;
	uint8_t pwr_level_req;
	uint8_t pwr_level_cur;
	uint16_t len_info[5];
	uint32_t speed_mask;	/* Speeds supported by the NIM */
	int8_t lane_idx;
	uint8_t lane_count;
	uint32_t options;
	bool tx_disable;
	bool dmi_supp;

	union {
		struct {
			bool rx_only;
			bool qsfp28;
			union {
				struct {
					uint8_t rev_compliance;
					bool media_side_fec_ctrl;
					bool host_side_fec_ctrl;
					bool media_side_fec_ena;
					bool host_side_fec_ena;
				} qsfp28;
			} specific_u;
		} qsfp;

	} specific_u;
} nim_i2c_ctx_t, *nim_i2c_ctx_p;

struct nim_sensor_group {
	struct nim_i2c_ctx *ctx;
	struct nim_sensor_group *next;
};

#endif	/* _NTNIC_NIM_H_ */
