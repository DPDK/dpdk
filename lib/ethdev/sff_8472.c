/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 * Implements SFF-8472 optics diagnostics.
 */

#include <stdio.h>

#include "sff_common.h"

/* Offsets in decimal, for direct comparison with the SFF specs */

/* A0-based EEPROM offsets for DOM support checks */
#define SFF_A0_DOM                        92
#define SFF_A0_OPTIONS                    93
#define SFF_A0_COMP                       94

/* EEPROM bit values for various registers */
#define SFF_A0_DOM_EXTCAL                 RTE_BIT32(4)
#define SFF_A0_DOM_INTCAL                 RTE_BIT32(5)
#define SFF_A0_DOM_IMPL                   RTE_BIT32(6)
#define SFF_A0_DOM_PWRT                   RTE_BIT32(3)

#define SFF_A0_OPTIONS_AW                 RTE_BIT32(7)

/*
 * This is the offset at which the A2 page is in the EEPROM
 * blob returned by the kernel.
 */
#define SFF_A2_BASE                       0x100

/* A2-based offsets for DOM */
#define SFF_A2_TEMP                       96
#define SFF_A2_TEMP_HALRM                 0
#define SFF_A2_TEMP_LALRM                 2
#define SFF_A2_TEMP_HWARN                 4
#define SFF_A2_TEMP_LWARN                 6

#define SFF_A2_VCC                        98
#define SFF_A2_VCC_HALRM                  8
#define SFF_A2_VCC_LALRM                  10
#define SFF_A2_VCC_HWARN                  12
#define SFF_A2_VCC_LWARN                  14

#define SFF_A2_BIAS                       100
#define SFF_A2_BIAS_HALRM                 16
#define SFF_A2_BIAS_LALRM                 18
#define SFF_A2_BIAS_HWARN                 20
#define SFF_A2_BIAS_LWARN                 22

#define SFF_A2_TX_PWR                     102
#define SFF_A2_TX_PWR_HALRM               24
#define SFF_A2_TX_PWR_LALRM               26
#define SFF_A2_TX_PWR_HWARN               28
#define SFF_A2_TX_PWR_LWARN               30

#define SFF_A2_RX_PWR                     104
#define SFF_A2_RX_PWR_HALRM               32
#define SFF_A2_RX_PWR_LALRM               34
#define SFF_A2_RX_PWR_HWARN               36
#define SFF_A2_RX_PWR_LWARN               38

#define SFF_A2_ALRM_FLG                   112
#define SFF_A2_WARN_FLG                   116

/* 32-bit little-endian calibration constants */
#define SFF_A2_CAL_RXPWR4                 56
#define SFF_A2_CAL_RXPWR3                 60
#define SFF_A2_CAL_RXPWR2                 64
#define SFF_A2_CAL_RXPWR1                 68
#define SFF_A2_CAL_RXPWR0                 72

/* 16-bit little endian calibration constants */
#define SFF_A2_CAL_TXI_SLP                76
#define SFF_A2_CAL_TXI_OFF                78
#define SFF_A2_CAL_TXPWR_SLP              80
#define SFF_A2_CAL_TXPWR_OFF              82
#define SFF_A2_CAL_T_SLP                  84
#define SFF_A2_CAL_T_OFF                  86
#define SFF_A2_CAL_V_SLP                  88
#define SFF_A2_CAL_V_OFF                  90

static struct sff_8472_aw_flags {
	const char *str;        /* Human-readable string, null at the end */
	int offset;             /* A2-relative address offset */
	uint8_t value;          /* Alarm is on if (offset & value) != 0. */
} sff_8472_aw_flags[] = {
	{ "Laser bias current high alarm",   SFF_A2_ALRM_FLG, RTE_BIT32(3) },
	{ "Laser bias current low alarm",    SFF_A2_ALRM_FLG, RTE_BIT32(2) },
	{ "Laser bias current high warning", SFF_A2_WARN_FLG, RTE_BIT32(3) },
	{ "Laser bias current low warning",  SFF_A2_WARN_FLG, RTE_BIT32(2) },

	{ "Laser output power high alarm",   SFF_A2_ALRM_FLG, RTE_BIT32(1) },
	{ "Laser output power low alarm",    SFF_A2_ALRM_FLG, RTE_BIT32(0) },
	{ "Laser output power high warning", SFF_A2_WARN_FLG, RTE_BIT32(1) },
	{ "Laser output power low warning",  SFF_A2_WARN_FLG, RTE_BIT32(0) },

	{ "Module temperature high alarm",   SFF_A2_ALRM_FLG, RTE_BIT32(7) },
	{ "Module temperature low alarm",    SFF_A2_ALRM_FLG, RTE_BIT32(6) },
	{ "Module temperature high warning", SFF_A2_WARN_FLG, RTE_BIT32(7) },
	{ "Module temperature low warning",  SFF_A2_WARN_FLG, RTE_BIT32(6) },

	{ "Module voltage high alarm",   SFF_A2_ALRM_FLG, RTE_BIT32(5) },
	{ "Module voltage low alarm",    SFF_A2_ALRM_FLG, RTE_BIT32(4) },
	{ "Module voltage high warning", SFF_A2_WARN_FLG, RTE_BIT32(5) },
	{ "Module voltage low warning",  SFF_A2_WARN_FLG, RTE_BIT32(4) },

	{ "Laser rx power high alarm",   SFF_A2_ALRM_FLG + 1, RTE_BIT32(7) },
	{ "Laser rx power low alarm",    SFF_A2_ALRM_FLG + 1, RTE_BIT32(6) },
	{ "Laser rx power high warning", SFF_A2_WARN_FLG + 1, RTE_BIT32(7) },
	{ "Laser rx power low warning",  SFF_A2_WARN_FLG + 1, RTE_BIT32(6) },

	{ NULL, 0, 0 },
};

/* Most common case: 16-bit unsigned integer in a certain unit */
#define A2_OFFSET_TO_U16(offset) \
	(data[SFF_A2_BASE + (offset)] << 8 | data[SFF_A2_BASE + (offset) + 1])

/* Calibration slope is a number between 0.0 included and 256.0 excluded. */
#define A2_OFFSET_TO_SLP(offset) \
	(data[SFF_A2_BASE + (offset)] + data[SFF_A2_BASE + (offset) + 1] / 256.)

/* Calibration offset is an integer from -32768 to 32767 */
#define A2_OFFSET_TO_OFF(offset) \
	((int16_t)A2_OFFSET_TO_U16(offset))

/* RXPWR(x) are IEEE-754 floating point numbers in big-endian format */
#define A2_OFFSET_TO_RXPWRx(offset) \
	(befloattoh((const uint32_t *)(data + SFF_A2_BASE + (offset))))

/*
 * 2-byte internal temperature conversions:
 * First byte is a signed 8-bit integer, which is the temp decimal part
 * Second byte are 1/256th of degree, which are added to the dec part.
 */
#define A2_OFFSET_TO_TEMP(offset) ((int16_t)A2_OFFSET_TO_U16(offset))

static void sff_8472_dom_parse(const uint8_t *data, struct sff_diags *sd)
{
	sd->bias_cur[SFF_MCURR] = A2_OFFSET_TO_U16(SFF_A2_BIAS);
	sd->bias_cur[SFF_HALRM] = A2_OFFSET_TO_U16(SFF_A2_BIAS_HALRM);
	sd->bias_cur[SFF_LALRM] = A2_OFFSET_TO_U16(SFF_A2_BIAS_LALRM);
	sd->bias_cur[SFF_HWARN] = A2_OFFSET_TO_U16(SFF_A2_BIAS_HWARN);
	sd->bias_cur[SFF_LWARN] = A2_OFFSET_TO_U16(SFF_A2_BIAS_LWARN);

	sd->sfp_voltage[SFF_MCURR] = A2_OFFSET_TO_U16(SFF_A2_VCC);
	sd->sfp_voltage[SFF_HALRM] = A2_OFFSET_TO_U16(SFF_A2_VCC_HALRM);
	sd->sfp_voltage[SFF_LALRM] = A2_OFFSET_TO_U16(SFF_A2_VCC_LALRM);
	sd->sfp_voltage[SFF_HWARN] = A2_OFFSET_TO_U16(SFF_A2_VCC_HWARN);
	sd->sfp_voltage[SFF_LWARN] = A2_OFFSET_TO_U16(SFF_A2_VCC_LWARN);

	sd->tx_power[SFF_MCURR] = A2_OFFSET_TO_U16(SFF_A2_TX_PWR);
	sd->tx_power[SFF_HALRM] = A2_OFFSET_TO_U16(SFF_A2_TX_PWR_HALRM);
	sd->tx_power[SFF_LALRM] = A2_OFFSET_TO_U16(SFF_A2_TX_PWR_LALRM);
	sd->tx_power[SFF_HWARN] = A2_OFFSET_TO_U16(SFF_A2_TX_PWR_HWARN);
	sd->tx_power[SFF_LWARN] = A2_OFFSET_TO_U16(SFF_A2_TX_PWR_LWARN);

	sd->rx_power[SFF_MCURR] = A2_OFFSET_TO_U16(SFF_A2_RX_PWR);
	sd->rx_power[SFF_HALRM] = A2_OFFSET_TO_U16(SFF_A2_RX_PWR_HALRM);
	sd->rx_power[SFF_LALRM] = A2_OFFSET_TO_U16(SFF_A2_RX_PWR_LALRM);
	sd->rx_power[SFF_HWARN] = A2_OFFSET_TO_U16(SFF_A2_RX_PWR_HWARN);
	sd->rx_power[SFF_LWARN] = A2_OFFSET_TO_U16(SFF_A2_RX_PWR_LWARN);

	sd->sfp_temp[SFF_MCURR] = A2_OFFSET_TO_TEMP(SFF_A2_TEMP);
	sd->sfp_temp[SFF_HALRM] = A2_OFFSET_TO_TEMP(SFF_A2_TEMP_HALRM);
	sd->sfp_temp[SFF_LALRM] = A2_OFFSET_TO_TEMP(SFF_A2_TEMP_LALRM);
	sd->sfp_temp[SFF_HWARN] = A2_OFFSET_TO_TEMP(SFF_A2_TEMP_HWARN);
	sd->sfp_temp[SFF_LWARN] = A2_OFFSET_TO_TEMP(SFF_A2_TEMP_LWARN);
}

/* Converts to a float from a big-endian 4-byte source buffer. */
static float befloattoh(const uint32_t *source)
{
	union {
		uint32_t src;
		float dst;
	} converter;

	converter.src = ntohl(*source);
	return converter.dst;
}

static void sff_8472_calibration(const uint8_t *data, struct sff_diags *sd)
{
	unsigned long i;
	uint16_t rx_reading;

	/* Calibration should occur for all values (threshold and current) */
	for (i = 0; i < RTE_DIM(sd->bias_cur); ++i) {
		/*
		 * Apply calibration formula 1 (Temp., Voltage, Bias, Tx Power)
		 */
		sd->bias_cur[i]    *= A2_OFFSET_TO_SLP(SFF_A2_CAL_TXI_SLP);
		sd->tx_power[i]    *= A2_OFFSET_TO_SLP(SFF_A2_CAL_TXPWR_SLP);
		sd->sfp_voltage[i] *= A2_OFFSET_TO_SLP(SFF_A2_CAL_V_SLP);
		sd->sfp_temp[i]    *= A2_OFFSET_TO_SLP(SFF_A2_CAL_T_SLP);

		sd->bias_cur[i]    += A2_OFFSET_TO_OFF(SFF_A2_CAL_TXI_OFF);
		sd->tx_power[i]    += A2_OFFSET_TO_OFF(SFF_A2_CAL_TXPWR_OFF);
		sd->sfp_voltage[i] += A2_OFFSET_TO_OFF(SFF_A2_CAL_V_OFF);
		sd->sfp_temp[i]    += A2_OFFSET_TO_OFF(SFF_A2_CAL_T_OFF);

		/*
		 * Apply calibration formula 2 (Rx Power only)
		 */
		rx_reading = sd->rx_power[i];
		sd->rx_power[i]    = A2_OFFSET_TO_RXPWRx(SFF_A2_CAL_RXPWR0);
		sd->rx_power[i]    += rx_reading *
			A2_OFFSET_TO_RXPWRx(SFF_A2_CAL_RXPWR1);
		sd->rx_power[i]    += rx_reading *
			A2_OFFSET_TO_RXPWRx(SFF_A2_CAL_RXPWR2);
		sd->rx_power[i]    += rx_reading *
			A2_OFFSET_TO_RXPWRx(SFF_A2_CAL_RXPWR3);
	}
}

static void sff_8472_parse_eeprom(const uint8_t *data, struct sff_diags *sd)
{
	sd->supports_dom = data[SFF_A0_DOM] & SFF_A0_DOM_IMPL;
	sd->supports_alarms = data[SFF_A0_OPTIONS] & SFF_A0_OPTIONS_AW;
	sd->calibrated_ext = data[SFF_A0_DOM] & SFF_A0_DOM_EXTCAL;
	sd->rx_power_type = data[SFF_A0_DOM] & SFF_A0_DOM_PWRT;

	sff_8472_dom_parse(data, sd);

	/*
	 * If the SFP is externally calibrated, we need to read calibration data
	 * and compensate the already stored readings.
	 */
	if (sd->calibrated_ext)
		sff_8472_calibration(data, sd);
}

void sff_8472_show_all(const uint8_t *data, struct rte_tel_data *d)
{
	struct sff_diags sd = {0};
	const char *rx_power_string = NULL;
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];
	int i;

	sff_8472_parse_eeprom(data, &sd);

	if (!sd.supports_dom) {
		ssf_add_dict_string(d, "Optical diagnostics support", "No");
		return;
	}
	ssf_add_dict_string(d, "Optical diagnostics support", "Yes");

	SFF_SPRINT_BIAS(val_string, sd.bias_cur[SFF_MCURR]);
	ssf_add_dict_string(d, "Laser bias current", val_string);

	SFF_SPRINT_xX_PWR(val_string, sd.tx_power[SFF_MCURR]);
	ssf_add_dict_string(d, "Laser output power", val_string);

	if (!sd.rx_power_type)
		rx_power_string = "Receiver signal OMA";
	else
		rx_power_string = "Receiver signal average optical power";

	SFF_SPRINT_xX_PWR(val_string, sd.rx_power[SFF_MCURR]);
	ssf_add_dict_string(d, rx_power_string, val_string);

	SFF_SPRINT_TEMP(val_string, sd.sfp_temp[SFF_MCURR]);
	ssf_add_dict_string(d, "Module temperature", val_string);

	SFF_SPRINT_VCC(val_string, sd.sfp_voltage[SFF_MCURR]);
	ssf_add_dict_string(d, "Module voltage", val_string);

	ssf_add_dict_string(d, "Alarm/warning flags implemented",
			(sd.supports_alarms ? "Yes" : "No"));

	if (sd.supports_alarms) {
		for (i = 0; sff_8472_aw_flags[i].str; ++i) {
			ssf_add_dict_string(d, sff_8472_aw_flags[i].str,
					data[SFF_A2_BASE + sff_8472_aw_flags[i].offset]
					& sff_8472_aw_flags[i].value ? "On" : "Off");
		}
		sff_show_thresholds(sd, d);
	}
}
