/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef NIM_DEFINES_H_
#define NIM_DEFINES_H_

#define NIM_IDENTIFIER_ADDR 0	/* 1 byte */

/* I2C addresses */
#define NIM_I2C_0XA0 0xA0	/* Basic I2C address */
#define NIM_I2C_0XA2 0xA2	/* Diagnostic monitoring */

typedef enum {
	NIM_OPTION_TX_DISABLE,
	/* Indicates that the module should be checked for the two next FEC types */
	NIM_OPTION_FEC,
	NIM_OPTION_MEDIA_SIDE_FEC,
	NIM_OPTION_HOST_SIDE_FEC,
	NIM_OPTION_RX_ONLY
} nim_option_t;

enum nt_nim_identifier_e {
	NT_NIM_UNKNOWN = 0x00,	/* Nim type is unknown */
};
typedef enum nt_nim_identifier_e nt_nim_identifier_t;

#endif	/* NIM_DEFINES_H_ */
