/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 * Implements SFF-8079 optics diagnostics.
 */

#include <stdio.h>

#include "sff_common.h"

static void sff_8079_show_identifier(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_identifier(data, 0, d);
}

static void sff_8079_show_ext_identifier(const uint8_t *data, struct rte_tel_data *d)
{
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];

	snprintf(val_string, sizeof(val_string), "0x%02x", data[1]);
	if (data[1] == 0x00)
		strlcat(val_string, " (GBIC not specified / not MOD_DEF compliant)",
			sizeof(val_string));
	else if (data[1] == 0x04)
		strlcat(val_string, " (GBIC/SFP defined by 2-wire interface ID)",
			sizeof(val_string));
	else if (data[1] <= 0x07) {
		char tmp[SFF_ITEM_VAL_COMPOSE_SIZE];
		snprintf(tmp, sizeof(tmp), " (GBIC compliant with MOD_DEF %u)", data[1]);
		strlcat(val_string, tmp, sizeof(val_string));
	} else
		strlcat(val_string, " (unknown)", sizeof(val_string));
	ssf_add_dict_string(d, "Extended identifier", val_string);
}

static void sff_8079_show_connector(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_connector(data, 2, d);
}

static void sff_8079_show_transceiver(const uint8_t *data, struct rte_tel_data *d)
{
	static const char *name = "Transceiver type";
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];

	snprintf(val_string, sizeof(val_string),
		"0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
		data[3], data[4], data[5], data[6], data[7], data[8], data[9], data[10], data[36]);
	ssf_add_dict_string(d, "Transceiver codes", val_string);

	/* 10G Ethernet Compliance Codes */
	if (data[3] & (1 << 7))
		ssf_add_dict_string(d, "10G Ethernet transceiver type",
		"10G Ethernet: 10G Base-ER [SFF-8472 rev10.4 onwards]");
	if (data[3] & (1 << 6))
		ssf_add_dict_string(d, name, "10G Ethernet: 10G Base-LRM");
	if (data[3] & (1 << 5))
		ssf_add_dict_string(d, name, "10G Ethernet: 10G Base-LR");
	if (data[3] & (1 << 4))
		ssf_add_dict_string(d, name, "10G Ethernet: 10G Base-SR");

	/* Infiniband Compliance Codes */
	if (data[3] & (1 << 3))
		ssf_add_dict_string(d, name, "Infiniband: 1X SX");
	if (data[3] & (1 << 2))
		ssf_add_dict_string(d, name, "Infiniband: 1X LX");
	if (data[3] & (1 << 1))
		ssf_add_dict_string(d, name, "Infiniband: 1X Copper Active");
	if (data[3] & (1 << 0))
		ssf_add_dict_string(d, name, "Infiniband: 1X Copper Passive");

	/* ESCON Compliance Codes */
	if (data[4] & (1 << 7))
		ssf_add_dict_string(d, name, "ESCON: ESCON MMF, 1310nm LED");
	if (data[4] & (1 << 6))
		ssf_add_dict_string(d, name, "ESCON: ESCON SMF, 1310nm Laser");

	/* SONET Compliance Codes */
	if (data[4] & (1 << 5))
		ssf_add_dict_string(d, name, "SONET: OC-192, short reach");
	if (data[4] & (1 << 4))
		ssf_add_dict_string(d, name, "SONET: SONET reach specifier bit 1");
	if (data[4] & (1 << 3))
		ssf_add_dict_string(d, name, "SONET: SONET reach specifier bit 2");
	if (data[4] & (1 << 2))
		ssf_add_dict_string(d, name, "SONET: OC-48, long reach");
	if (data[4] & (1 << 1))
		ssf_add_dict_string(d, name, "SONET: OC-48, intermediate reach");
	if (data[4] & (1 << 0))
		ssf_add_dict_string(d, name, "SONET: OC-48, short reach");
	if (data[5] & (1 << 6))
		ssf_add_dict_string(d, name, "SONET: OC-12, single mode, long reach");
	if (data[5] & (1 << 5))
		ssf_add_dict_string(d, name, "SONET: OC-12, single mode, inter. reach");
	if (data[5] & (1 << 4))
		ssf_add_dict_string(d, name, "SONET: OC-12, short reach");
	if (data[5] & (1 << 2))
		ssf_add_dict_string(d, name, "SONET: OC-3, single mode, long reach");
	if (data[5] & (1 << 1))
		ssf_add_dict_string(d, name, "SONET: OC-3, single mode, inter. reach");
	if (data[5] & (1 << 0))
		ssf_add_dict_string(d, name, "SONET: OC-3, short reach");

	/* Ethernet Compliance Codes */
	if (data[6] & (1 << 7))
		ssf_add_dict_string(d, name, "Ethernet: BASE-PX");
	if (data[6] & (1 << 6))
		ssf_add_dict_string(d, name, "Ethernet: BASE-BX10");
	if (data[6] & (1 << 5))
		ssf_add_dict_string(d, name, "Ethernet: 100BASE-FX");
	if (data[6] & (1 << 4))
		ssf_add_dict_string(d, name, "Ethernet: 100BASE-LX/LX10");
	if (data[6] & (1 << 3))
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-T");
	if (data[6] & (1 << 2))
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-CX");
	if (data[6] & (1 << 1))
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-LX");
	if (data[6] & (1 << 0))
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-SX");

	/* Fibre Channel link length */
	if (data[7] & (1 << 7))
		ssf_add_dict_string(d, name, "FC: very long distance (V)");
	if (data[7] & (1 << 6))
		ssf_add_dict_string(d, name, "FC: short distance (S)");
	if (data[7] & (1 << 5))
		ssf_add_dict_string(d, name, "FC: intermediate distance (I)");
	if (data[7] & (1 << 4))
		ssf_add_dict_string(d, name, "FC: long distance (L)");
	if (data[7] & (1 << 3))
		ssf_add_dict_string(d, name, "FC: medium distance (M)");

	/* Fibre Channel transmitter technology */
	if (data[7] & (1 << 2))
		ssf_add_dict_string(d, name, "FC: Shortwave laser, linear Rx (SA)");
	if (data[7] & (1 << 1))
		ssf_add_dict_string(d, name, "FC: Longwave laser (LC)");
	if (data[7] & (1 << 0))
		ssf_add_dict_string(d, name, "FC: Electrical inter-enclosure (EL)");
	if (data[8] & (1 << 7))
		ssf_add_dict_string(d, name, "FC: Electrical intra-enclosure (EL)");
	if (data[8] & (1 << 6))
		ssf_add_dict_string(d, name, "FC: Shortwave laser w/o OFC (SN)");
	if (data[8] & (1 << 5))
		ssf_add_dict_string(d, name, "FC: Shortwave laser with OFC (SL)");
	if (data[8] & (1 << 4))
		ssf_add_dict_string(d, name, "FC: Longwave laser (LL)");
	if (data[8] & (1 << 3))
		ssf_add_dict_string(d, name, "Active Cable");
	if (data[8] & (1 << 2))
		ssf_add_dict_string(d, name, "Passive Cable");
	if (data[8] & (1 << 1))
		ssf_add_dict_string(d, name, "FC: Copper FC-BaseT");

	/* Fibre Channel transmission media */
	if (data[9] & (1 << 7))
		ssf_add_dict_string(d, name, "FC: Twin Axial Pair (TW)");
	if (data[9] & (1 << 6))
		ssf_add_dict_string(d, name, "FC: Twisted Pair (TP)");
	if (data[9] & (1 << 5))
		ssf_add_dict_string(d, name, "FC: Miniature Coax (MI)");
	if (data[9] & (1 << 4))
		ssf_add_dict_string(d, name, "FC: Video Coax (TV)");
	if (data[9] & (1 << 3))
		ssf_add_dict_string(d, name, "FC: Multimode, 62.5um (M6)");
	if (data[9] & (1 << 2))
		ssf_add_dict_string(d, name, "FC: Multimode, 50um (M5)");
	if (data[9] & (1 << 0))
		ssf_add_dict_string(d, name, "FC: Single Mode (SM)");

	/* Fibre Channel speed */
	if (data[10] & (1 << 7))
		ssf_add_dict_string(d, name, "FC: 1200 MBytes/sec");
	if (data[10] & (1 << 6))
		ssf_add_dict_string(d, name, "FC: 800 MBytes/sec");
	if (data[10] & (1 << 4))
		ssf_add_dict_string(d, name, "FC: 400 MBytes/sec");
	if (data[10] & (1 << 2))
		ssf_add_dict_string(d, name, "FC: 200 MBytes/sec");
	if (data[10] & (1 << 0))
		ssf_add_dict_string(d, name, "FC: 100 MBytes/sec");

	/* Extended Specification Compliance Codes from SFF-8024 */
	switch (data[36]) {
	case 0x1:
		ssf_add_dict_string(d, name,
			"Extended: 100G AOC or 25GAUI C2M AOC with worst BER of 5x10^(-5)");
		break;
	case 0x2:
		ssf_add_dict_string(d, name, "Extended: 100G Base-SR4 or 25GBase-SR");
		break;
	case 0x3:
		ssf_add_dict_string(d, name, "Extended: 100G Base-LR4 or 25GBase-LR");
		break;
	case 0x4:
		ssf_add_dict_string(d, name, "Extended: 100G Base-ER4 or 25GBase-ER");
		break;
	case 0x8:
		ssf_add_dict_string(d, name,
			"Extended: 100G ACC or 25GAUI C2M ACC with worst BER of 5x10^(-5)");
		break;
	case 0xb:
		ssf_add_dict_string(d, name, "Extended: 100G Base-CR4 or 25G Base-CR CA-L");
		break;
	case 0xc:
		ssf_add_dict_string(d, name, "Extended: 25G Base-CR CA-S");
		break;
	case 0xd:
		ssf_add_dict_string(d, name, "Extended: 25G Base-CR CA-N");
		break;
	case 0x16:
		ssf_add_dict_string(d, name, "Extended: 10Gbase-T with SFI electrical interface");
		break;
	case 0x18:
		ssf_add_dict_string(d, name,
			"Extended: 100G AOC or 25GAUI C2M AOC with worst BER of 10^(-12)");
		break;
	case 0x19:
		ssf_add_dict_string(d, name,
			"Extended: 100G ACC or 25GAUI C2M ACC with worst BER of 10^(-12)");
		break;
	case 0x1c:
		ssf_add_dict_string(d, name, "Extended: 10Gbase-T Short Reach");
		break;
	default:
		break;
	}
}

static void sff_8079_show_encoding(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_encoding(data, 11, RTE_ETH_MODULE_SFF_8472, d);
}

static void sff_8079_show_rate_identifier(const uint8_t *data, struct rte_tel_data *d)
{
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];

	snprintf(val_string, sizeof(val_string), "0x%02x", data[13]);

	switch (data[13]) {
	case 0x00:
		strlcat(val_string, " (unspecified)", sizeof(val_string));
		break;
	case 0x01:
		strlcat(val_string, " (4/2/1G Rate_Select & AS0/AS1)", sizeof(val_string));
		break;
	case 0x02:
		strlcat(val_string, " (8/4/2G Rx Rate_Select only)", sizeof(val_string));
		break;
	case 0x03:
		strlcat(val_string, " (8/4/2G Independent Rx & Tx Rate_Select)",
			sizeof(val_string));
		break;
	case 0x04:
		strlcat(val_string, " (8/4/2G Tx Rate_Select only)", sizeof(val_string));
		break;
	default:
		strlcat(val_string, " (reserved or unknown)", sizeof(val_string));
		break;
	}
	ssf_add_dict_string(d, "Rate identifier", val_string);
}

static void sff_8079_show_oui(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_oui(data, 37, d);
}

static void
sff_8079_show_wavelength_or_copper_compliance(const uint8_t *data,
					      struct rte_tel_data *d)
{
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];

	if (data[8] & (1 << 2)) {
		snprintf(val_string, sizeof(val_string), "0x%02x", data[60]);
		switch (data[60]) {
		case 0x00:
			strlcat(val_string, " (unspecified)", sizeof(val_string));
			break;
		case 0x01:
			strlcat(val_string, " (SFF-8431 appendix E)", sizeof(val_string));
			break;
		default:
			strlcat(val_string, " (unknown)", sizeof(val_string));
			break;
		}
		strlcat(val_string, " [SFF-8472 rev10.4 only]", sizeof(val_string));
		ssf_add_dict_string(d, "Passive Cu cmplnce.", val_string);
	} else if (data[8] & (1 << 3)) {
		snprintf(val_string, sizeof(val_string), "0x%02x", data[60]);
		switch (data[60]) {
		case 0x00:
			strlcat(val_string, " (unspecified)", sizeof(val_string));
			break;
		case 0x01:
			strlcat(val_string, " (SFF-8431 appendix E)", sizeof(val_string));
			break;
		case 0x04:
			strlcat(val_string, " (SFF-8431 limiting)", sizeof(val_string));
			break;
		default:
			strlcat(val_string, " (unknown)", sizeof(val_string));
			break;
		}
		strlcat(val_string, " [SFF-8472 rev10.4 only]", sizeof(val_string));
		ssf_add_dict_string(d, "Active Cu cmplnce.", val_string);
	} else {
		snprintf(val_string, sizeof(val_string), "%unm", (data[60] << 8) | data[61]);
		ssf_add_dict_string(d, "Laser wavelength", val_string);
	}
}

static void sff_8079_show_options(const uint8_t *data, struct rte_tel_data *d)
{
	static const char *name = "Option";
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];

	snprintf(val_string, sizeof(val_string), "0x%02x 0x%02x", data[64], data[65]);
	ssf_add_dict_string(d, "Option values", val_string);

	if (data[65] & (1 << 1))
		ssf_add_dict_string(d, name, "RX_LOS implemented");
	if (data[65] & (1 << 2))
		ssf_add_dict_string(d, name, "RX_LOS implemented, inverted");
	if (data[65] & (1 << 3))
		ssf_add_dict_string(d, name, "TX_FAULT implemented");
	if (data[65] & (1 << 4))
		ssf_add_dict_string(d, name, "TX_DISABLE implemented");
	if (data[65] & (1 << 5))
		ssf_add_dict_string(d, name, "RATE_SELECT implemented");
	if (data[65] & (1 << 6))
		ssf_add_dict_string(d, name, "Tunable transmitter technology");
	if (data[65] & (1 << 7))
		ssf_add_dict_string(d, name, "Receiver decision threshold implemented");
	if (data[64] & (1 << 0))
		ssf_add_dict_string(d, name, "Linear receiver output implemented");
	if (data[64] & (1 << 1))
		ssf_add_dict_string(d, name, "Power level 2 requirement");
	if (data[64] & (1 << 2))
		ssf_add_dict_string(d, name, "Cooled transceiver implemented");
	if (data[64] & (1 << 3))
		ssf_add_dict_string(d, name, "Retimer or CDR implemented");
	if (data[64] & (1 << 4))
		ssf_add_dict_string(d, name, "Paging implemented");
	if (data[64] & (1 << 5))
		ssf_add_dict_string(d, name, "Power level 3 requirement");
}

void sff_8079_show_all(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8079_show_identifier(data, d);
	if (((data[0] == 0x02) || (data[0] == 0x03)) && (data[1] == 0x04)) {
		unsigned int br_nom, br_min, br_max;
		char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];

		if (data[12] == 0) {
			br_nom = br_min = br_max = 0;
		} else if (data[12] == 255) {
			br_nom = data[66] * 250;
			br_max = data[67];
			br_min = data[67];
		} else {
			br_nom = data[12] * 100;
			br_max = data[66];
			br_min = data[67];
		}
		sff_8079_show_ext_identifier(data, d);
		sff_8079_show_connector(data, d);
		sff_8079_show_transceiver(data, d);
		sff_8079_show_encoding(data, d);

		snprintf(val_string, sizeof(val_string), "%uMBd", br_nom);
		ssf_add_dict_string(d, "BR, Nominal", val_string);

		sff_8079_show_rate_identifier(data, d);
		sff_show_value_with_unit(data, 14,
					 "Length (SMF,km)", 1, "km", d);
		sff_show_value_with_unit(data, 15, "Length (SMF)", 100, "m", d);
		sff_show_value_with_unit(data, 16, "Length (50um)", 10, "m", d);
		sff_show_value_with_unit(data, 17,
					 "Length (62.5um)", 10, "m", d);
		sff_show_value_with_unit(data, 18, "Length (Copper)", 1, "m", d);
		sff_show_value_with_unit(data, 19, "Length (OM3)", 10, "m", d);
		sff_8079_show_wavelength_or_copper_compliance(data, d);
		sff_show_ascii(data, 20, 35, "Vendor name", d);
		sff_8079_show_oui(data, d);
		sff_show_ascii(data, 40, 55, "Vendor PN", d);
		sff_show_ascii(data, 56, 59, "Vendor rev", d);
		sff_8079_show_options(data, d);

		snprintf(val_string, sizeof(val_string), "%u%%", br_max);
		ssf_add_dict_string(d, "BR margin, max", val_string);
		snprintf(val_string, sizeof(val_string), "%u%%", br_min);
		ssf_add_dict_string(d, "BR margin, min", val_string);

		sff_show_ascii(data, 68, 83, "Vendor SN", d);
		sff_show_ascii(data, 84, 91, "Date code", d);
	}
}
