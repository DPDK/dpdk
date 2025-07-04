/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 *
 * Implements SFF-8024 Rev 4.0 of pluggable I/O configuration and some
 * common utilities for SFF-8436/8636 and SFF-8472/8079
 */

#ifndef _SFF_COMMON_H_
#define _SFF_COMMON_H_

#include <stdio.h>

#include "rte_ethdev.h"
#include "sff_telemetry.h"

#define SFF_8024_ID_OFFSET			0x00
#define SFF_8024_ID_UNKNOWN			0x00
#define SFF_8024_ID_GBIC			0x01
#define SFF_8024_ID_SOLDERED_MODULE		0x02
#define SFF_8024_ID_SFP				0x03
#define SFF_8024_ID_300_PIN_XBI			0x04
#define SFF_8024_ID_XENPAK			0x05
#define SFF_8024_ID_XFP				0x06
#define SFF_8024_ID_XFF				0x07
#define SFF_8024_ID_XFP_E			0x08
#define SFF_8024_ID_XPAK			0x09
#define SFF_8024_ID_X2				0x0A
#define SFF_8024_ID_DWDM_SFP			0x0B
#define SFF_8024_ID_QSFP			0x0C
#define SFF_8024_ID_QSFP_PLUS			0x0D
#define SFF_8024_ID_CXP				0x0E
#define SFF_8024_ID_HD4X			0x0F
#define SFF_8024_ID_HD8X			0x10
#define SFF_8024_ID_QSFP28			0x11
#define SFF_8024_ID_CXP2			0x12
#define SFF_8024_ID_CDFP			0x13
#define SFF_8024_ID_HD4X_FANOUT			0x14
#define SFF_8024_ID_HD8X_FANOUT			0x15
#define SFF_8024_ID_CDFP_S3			0x16
#define SFF_8024_ID_MICRO_QSFP			0x17
#define SFF_8024_ID_LAST			SFF_8024_ID_MICRO_QSFP
#define SFF_8024_ID_UNALLOCATED_LAST		0x7F
#define SFF_8024_ID_VENDOR_START		0x80
#define SFF_8024_ID_VENDOR_LAST			0xFF

#define SFF_8024_CTOR_UNKNOWN			0x00
#define SFF_8024_CTOR_SC			0x01
#define SFF_8024_CTOR_FC_STYLE_1		0x02
#define SFF_8024_CTOR_FC_STYLE_2		0x03
#define SFF_8024_CTOR_BNC_TNC			0x04
#define SFF_8024_CTOR_FC_COAX			0x05
#define SFF_8024_CTOR_FIBER_JACK		0x06
#define SFF_8024_CTOR_LC			0x07
#define SFF_8024_CTOR_MT_RJ			0x08
#define SFF_8024_CTOR_MU			0x09
#define SFF_8024_CTOR_SG			0x0A
#define SFF_8024_CTOR_OPT_PT			0x0B
#define SFF_8024_CTOR_MPO			0x0C
#define SFF_8024_CTOR_MPO_2			0x0D
/* 0E-1Fh --- Reserved */
#define SFF_8024_CTOR_HSDC_II			0x20
#define SFF_8024_CTOR_COPPER_PT			0x21
#define SFF_8024_CTOR_RJ45			0x22
#define SFF_8024_CTOR_NO_SEPARABLE		0x23
#define SFF_8024_CTOR_MXC_2x16			0x24
#define SFF_8024_CTOR_LAST			SFF_8024_CTOR_MXC_2x16
#define SFF_8024_CTOR_UNALLOCATED_LAST		0x7F
#define SFF_8024_CTOR_VENDOR_START		0x80
#define SFF_8024_CTOR_VENDOR_LAST		0xFF

/* ENCODING Values */
#define SFF_8024_ENCODING_UNSPEC		0x00
#define SFF_8024_ENCODING_8B10B			0x01
#define SFF_8024_ENCODING_4B5B			0x02
#define SFF_8024_ENCODING_NRZ			0x03
/*
 * Value: 04h
 * SFF-8472      - Manchester
 * SFF-8436/8636 - SONET Scrambled
 */
#define SFF_8024_ENCODING_4h			0x04
/*
 * Value: 05h
 * SFF-8472      - SONET Scrambled
 * SFF-8436/8636 - 64B/66B
 */
#define SFF_8024_ENCODING_5h			0x05
/*
 * Value: 06h
 * SFF-8472      - 64B/66B
 * SFF-8436/8636 - Manchester
 */
#define  SFF_8024_ENCODING_6h			0x06
#define  SFF_8024_ENCODING_256B			0x07
#define  SFF_8024_ENCODING_PAM4			0x08

/* Most common case: 16-bit unsigned integer in a certain unit */
#define SFF_OFFSET_TO_U16(offset) \
		(data[offset] << 8 | data[(offset) + 1])

#define SFF_SPRINT_xX_PWR(str, var) \
		snprintf(str, sizeof(str), "%.4f mW / %.2f dBm", \
			(double)((var) / 10000.), \
			sff_convert_mw_to_dbm((double)((var) / 10000.)))

#define SFF_SPRINT_BIAS(str, bias_cur) \
		snprintf(str, sizeof(str), "%.3f mA", (double)(bias_cur / 500.))

#define SFF_SPRINT_TEMP(str, temp) \
		snprintf(str, sizeof(str), "%.2f degrees C / %.2f degrees F", \
			(double)(temp / 256.), \
			(double)(temp / 256. * 1.8 + 32.))

#define SFF_SPRINT_VCC(str, sfp_voltage) \
		snprintf(str, sizeof(str), "%.4f V", (double)(sfp_voltage / 10000.))

/* Channel Monitoring Fields */
struct sff_channel_diags {
	uint16_t bias_cur;      /* Measured bias current in 2uA units */
	uint16_t rx_power;      /* Measured RX Power */
	uint16_t tx_power;      /* Measured TX Power */
};

/* Module Monitoring Fields */
struct sff_diags {

#define SFF_MAX_CHANNEL_NUM 4
#define SFF_LWARN 0
#define SFF_HWARN 1
#define SFF_LALRM 2
#define SFF_HALRM 3
#define SFF_MCURR 4

	/* Supports DOM */
	uint8_t supports_dom;
	/* Supports alarm/warning thold */
	uint8_t supports_alarms;
	/* RX Power: 0 = OMA, 1 = Average power */
	uint8_t  rx_power_type;
	/* TX Power: 0 = Not supported, 1 = Average power */
	uint8_t  tx_power_type;

	uint8_t calibrated_ext;    /* Is externally calibrated */
	/* [5] tables are low/high warn, low/high alarm, current */
	/* SFP voltage in 0.1mV units */
	uint16_t sfp_voltage[5];
	/* SFP Temp in 16-bit signed 1/256 Celsius */
	int16_t sfp_temp[5];
	/* Measured bias current in 2uA units */
	uint16_t bias_cur[5];
	/* Measured TX Power */
	uint16_t tx_power[5];
	/* Measured RX Power */
	uint16_t rx_power[5];
	struct sff_channel_diags scd[SFF_MAX_CHANNEL_NUM];
};

double sff_convert_mw_to_dbm(double mw);
void sff_show_value_with_unit(const uint8_t *data, unsigned int reg,
			      const char *name, unsigned int mult,
			      const char *unit, struct rte_tel_data *d);
void sff_show_ascii(const uint8_t *data, unsigned int first_reg,
		    unsigned int last_reg, const char *name, struct rte_tel_data *d);
void sff_show_thresholds(struct sff_diags sd, struct rte_tel_data *d);

void sff_8024_show_oui(const uint8_t *data, int id_offset, struct rte_tel_data *d);
void sff_8024_show_identifier(const uint8_t *data, int id_offset, struct rte_tel_data *d);
void sff_8024_show_connector(const uint8_t *data, int ctor_offset, struct rte_tel_data *d);
void sff_8024_show_encoding(const uint8_t *data, int encoding_offset,
			    int sff_type, struct rte_tel_data *d);

#endif /* _SFF_COMMON_H_ */
