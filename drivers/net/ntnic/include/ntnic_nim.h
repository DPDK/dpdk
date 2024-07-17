/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _NTNIC_NIM_H_
#define _NTNIC_NIM_H_

#include <stdint.h>

typedef enum i2c_type {
	I2C_HWIIC,
} i2c_type_e;

enum nt_port_type_e {
	NT_PORT_TYPE_NOT_AVAILABLE = 0,	/* The NIM/port type is not available (unknown) */
	NT_PORT_TYPE_NOT_RECOGNISED,	/* The NIM/port type not recognized */
};

typedef enum nt_port_type_e nt_port_type_t, *nt_port_type_p;

typedef struct nim_i2c_ctx {
	union {
		nthw_iic_t hwiic;	/* depends on *Fpga_t, instance number, and cycle time */
		struct {
			nthw_i2cm_t *p_nt_i2cm;
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

} nim_i2c_ctx_t, *nim_i2c_ctx_p;

struct nim_sensor_group {
	struct nim_i2c_ctx *ctx;
	struct nim_sensor_group *next;
};

#endif	/* _NTNIC_NIM_H_ */
