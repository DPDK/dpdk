/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#ifndef _QSFP_REGISTERS_H
#define _QSFP_REGISTERS_H

/*
 * QSFP Registers
 */
#define QSFP_INT_STATUS_RX_LOS_ADDR 3
#define QSFP_TEMP_LIN_ADDR 22
#define QSFP_VOLT_LIN_ADDR 26
#define QSFP_RX_PWR_LIN_ADDR 34	/* uint16_t [0..3] */
#define QSFP_TX_BIAS_LIN_ADDR 42/* uint16_t [0..3] */
#define QSFP_TX_PWR_LIN_ADDR 50	/* uint16_t [0..3] */

#define QSFP_CONTROL_STATUS_LIN_ADDR 86
#define QSFP_SOFT_TX_ALL_DISABLE_BITS 0x0F

#define QSFP_POWER_CLASS_BITS_1_4 0xC0
#define QSFP_POWER_CLASS_BITS_5_7 0x03

#define QSFP_SUP_LEN_INFO_LIN_ADDR 142	/* 5bytes */
#define QSFP_TRANSMITTER_TYPE_LIN_ADDR 147	/* 1byte */
#define QSFP_VENDOR_NAME_LIN_ADDR 148	/* 16bytes */
#define QSFP_VENDOR_PN_LIN_ADDR 168	/* 16bytes */
#define QSFP_VENDOR_SN_LIN_ADDR 196	/* 16bytes */
#define QSFP_VENDOR_DATE_LIN_ADDR 212	/* 8bytes */
#define QSFP_VENDOR_REV_LIN_ADDR 184	/* 2bytes */

#define QSFP_SPEC_COMPLIANCE_CODES_ADDR 131	/* 8 bytes */
#define QSFP_EXT_SPEC_COMPLIANCE_CODES_ADDR 192	/* 1 byte */

#define QSFP_OPTION3_LIN_ADDR 195
#define QSFP_OPTION3_TX_DISABLE_BIT (1 << 4)

#define QSFP_DMI_OPTION_LIN_ADDR 220
#define QSFP_DMI_AVG_PWR_BIT (1 << 3)


#endif	/* _QSFP_REGISTERS_H */
