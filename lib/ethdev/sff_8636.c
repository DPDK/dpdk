/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 * Implements SFF-8636 based QSFP+/QSFP28 Diagnostics Memory map.
 */

#include <stdio.h>
#include <math.h>

#include "sff_common.h"
#include "sff_8636.h"

#define SFF_MAX_DESC_SIZE	42

static const uint8_t sff_8636_rx_power_offset[SFF_MAX_CHANNEL_NUM] = {
	SFF_8636_RX_PWR_1_OFFSET,
	SFF_8636_RX_PWR_2_OFFSET,
	SFF_8636_RX_PWR_3_OFFSET,
	SFF_8636_RX_PWR_4_OFFSET,
};
static const uint8_t sff_8636_tx_power_offset[SFF_MAX_CHANNEL_NUM] = {
	SFF_8636_TX_PWR_1_OFFSET,
	SFF_8636_TX_PWR_2_OFFSET,
	SFF_8636_TX_PWR_3_OFFSET,
	SFF_8636_TX_PWR_4_OFFSET,
};
static const uint8_t sff_8636_tx_bias_offset[SFF_MAX_CHANNEL_NUM] = {
	SFF_8636_TX_BIAS_1_OFFSET,
	SFF_8636_TX_BIAS_2_OFFSET,
	SFF_8636_TX_BIAS_3_OFFSET,
	SFF_8636_TX_BIAS_4_OFFSET,
};

static struct sff_8636_aw_flags {
	const char *str;        /* Human-readable string, null at the end */
	int offset;             /* A2-relative address offset */
	uint8_t value;             /* Alarm is on if (offset & value) != 0. */
} sff_8636_aw_flags[] = {
	{ "Laser bias current high alarm   (Chan 1)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_1_HALARM) },
	{ "Laser bias current low alarm    (Chan 1)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_1_LALARM) },
	{ "Laser bias current high warning (Chan 1)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_1_HWARN) },
	{ "Laser bias current low warning  (Chan 1)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_1_LWARN) },

	{ "Laser bias current high alarm   (Chan 2)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_2_HALARM) },
	{ "Laser bias current low alarm    (Chan 2)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_2_LALARM) },
	{ "Laser bias current high warning (Chan 2)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_2_HWARN) },
	{ "Laser bias current low warning  (Chan 2)",
		SFF_8636_TX_BIAS_12_AW_OFFSET, (SFF_8636_TX_BIAS_2_LWARN) },

	{ "Laser bias current high alarm   (Chan 3)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_3_HALARM) },
	{ "Laser bias current low alarm    (Chan 3)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_3_LALARM) },
	{ "Laser bias current high warning (Chan 3)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_3_HWARN) },
	{ "Laser bias current low warning  (Chan 3)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_3_LWARN) },

	{ "Laser bias current high alarm   (Chan 4)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_4_HALARM) },
	{ "Laser bias current low alarm    (Chan 4)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_4_LALARM) },
	{ "Laser bias current high warning (Chan 4)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_4_HWARN) },
	{ "Laser bias current low warning  (Chan 4)",
		SFF_8636_TX_BIAS_34_AW_OFFSET, (SFF_8636_TX_BIAS_4_LWARN) },

	{ "Module temperature high alarm",
		SFF_8636_TEMP_AW_OFFSET, (SFF_8636_TEMP_HALARM_STATUS) },
	{ "Module temperature low alarm",
		SFF_8636_TEMP_AW_OFFSET, (SFF_8636_TEMP_LALARM_STATUS) },
	{ "Module temperature high warning",
		SFF_8636_TEMP_AW_OFFSET, (SFF_8636_TEMP_HWARN_STATUS) },
	{ "Module temperature low warning",
		SFF_8636_TEMP_AW_OFFSET, (SFF_8636_TEMP_LWARN_STATUS) },

	{ "Module voltage high alarm",
		SFF_8636_VCC_AW_OFFSET, (SFF_8636_VCC_HALARM_STATUS) },
	{ "Module voltage low alarm",
		SFF_8636_VCC_AW_OFFSET, (SFF_8636_VCC_LALARM_STATUS) },
	{ "Module voltage high warning",
		SFF_8636_VCC_AW_OFFSET, (SFF_8636_VCC_HWARN_STATUS) },
	{ "Module voltage low warning",
		SFF_8636_VCC_AW_OFFSET, (SFF_8636_VCC_LWARN_STATUS) },

	{ "Laser tx power high alarm   (Channel 1)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_1_HALARM) },
	{ "Laser tx power low alarm    (Channel 1)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_1_LALARM) },
	{ "Laser tx power high warning (Channel 1)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_1_HWARN) },
	{ "Laser tx power low warning  (Channel 1)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_1_LWARN) },

	{ "Laser tx power high alarm   (Channel 2)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_2_HALARM) },
	{ "Laser tx power low alarm    (Channel 2)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_2_LALARM) },
	{ "Laser tx power high warning (Channel 2)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_2_HWARN) },
	{ "Laser tx power low warning  (Channel 2)",
		SFF_8636_TX_PWR_12_AW_OFFSET, (SFF_8636_TX_PWR_2_LWARN) },

	{ "Laser tx power high alarm   (Channel 3)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_3_HALARM) },
	{ "Laser tx power low alarm    (Channel 3)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_3_LALARM) },
	{ "Laser tx power high warning (Channel 3)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_3_HWARN) },
	{ "Laser tx power low warning  (Channel 3)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_3_LWARN) },

	{ "Laser tx power high alarm   (Channel 4)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_4_HALARM) },
	{ "Laser tx power low alarm    (Channel 4)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_4_LALARM) },
	{ "Laser tx power high warning (Channel 4)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_4_HWARN) },
	{ "Laser tx power low warning  (Channel 4)",
		SFF_8636_TX_PWR_34_AW_OFFSET, (SFF_8636_TX_PWR_4_LWARN) },

	{ "Laser rx power high alarm   (Channel 1)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_1_HALARM) },
	{ "Laser rx power low alarm    (Channel 1)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_1_LALARM) },
	{ "Laser rx power high warning (Channel 1)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_1_HWARN) },
	{ "Laser rx power low warning  (Channel 1)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_1_LWARN) },

	{ "Laser rx power high alarm   (Channel 2)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_2_HALARM) },
	{ "Laser rx power low alarm    (Channel 2)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_2_LALARM) },
	{ "Laser rx power high warning (Channel 2)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_2_HWARN) },
	{ "Laser rx power low warning  (Channel 2)",
		SFF_8636_RX_PWR_12_AW_OFFSET, (SFF_8636_RX_PWR_2_LWARN) },

	{ "Laser rx power high alarm   (Channel 3)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_3_HALARM) },
	{ "Laser rx power low alarm    (Channel 3)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_3_LALARM) },
	{ "Laser rx power high warning (Channel 3)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_3_HWARN) },
	{ "Laser rx power low warning  (Channel 3)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_3_LWARN) },

	{ "Laser rx power high alarm   (Channel 4)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_4_HALARM) },
	{ "Laser rx power low alarm    (Channel 4)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_4_LALARM) },
	{ "Laser rx power high warning (Channel 4)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_4_HWARN) },
	{ "Laser rx power low warning  (Channel 4)",
		SFF_8636_RX_PWR_34_AW_OFFSET, (SFF_8636_RX_PWR_4_LWARN) },

	{ NULL, 0, 0 },
};

static void sff_8636_show_identifier(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_identifier(data, SFF_8636_ID_OFFSET, d);
}

static void sff_8636_show_ext_identifier(const uint8_t *data, struct rte_tel_data *d)
{
	static const char *name = "Extended identifier description";
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];
	snprintf(val_string, sizeof(val_string), "0x%02x", data[SFF_8636_EXT_ID_OFFSET]);
	ssf_add_dict_string(d, "Extended identifier", val_string);

	switch (data[SFF_8636_EXT_ID_OFFSET] & SFF_8636_EXT_ID_PWR_CLASS_MASK) {
	case SFF_8636_EXT_ID_PWR_CLASS_1:
		ssf_add_dict_string(d, name, "1.5W max. Power consumption");
		break;
	case SFF_8636_EXT_ID_PWR_CLASS_2:
		ssf_add_dict_string(d, name, "2.0W max. Power consumption");
		break;
	case SFF_8636_EXT_ID_PWR_CLASS_3:
		ssf_add_dict_string(d, name, "2.5W max. Power consumption");
		break;
	case SFF_8636_EXT_ID_PWR_CLASS_4:
		ssf_add_dict_string(d, name, "3.5W max. Power consumption");
		break;
	}

	if (data[SFF_8636_EXT_ID_OFFSET] & SFF_8636_EXT_ID_CDR_TX_MASK)
		ssf_add_dict_string(d, name, "CDR present in TX");
	else
		ssf_add_dict_string(d, name, "No CDR in TX");

	if (data[SFF_8636_EXT_ID_OFFSET] & SFF_8636_EXT_ID_CDR_RX_MASK)
		ssf_add_dict_string(d, name, "CDR present in RX");
	else
		ssf_add_dict_string(d, name, "No CDR in RX");

	switch (data[SFF_8636_EXT_ID_OFFSET] & SFF_8636_EXT_ID_EPWR_CLASS_MASK) {
	case SFF_8636_EXT_ID_PWR_CLASS_LEGACY:
		snprintf(val_string, sizeof(val_string), "%s", "");
		break;
	case SFF_8636_EXT_ID_PWR_CLASS_5:
		snprintf(val_string, sizeof(val_string), "%s", "4.0W max. Power consumption, ");
		break;
	case SFF_8636_EXT_ID_PWR_CLASS_6:
		snprintf(val_string, sizeof(val_string), "%s", "4.5W max. Power consumption, ");
		break;
	case SFF_8636_EXT_ID_PWR_CLASS_7:
		snprintf(val_string, sizeof(val_string), "%s", "5.0W max. Power consumption, ");
		break;
	}

	if (data[SFF_8636_PWR_MODE_OFFSET] & SFF_8636_HIGH_PWR_ENABLE)
		strlcat(val_string, "High Power Class (> 3.5 W) enabled", sizeof(val_string));
	else
		strlcat(val_string, "High Power Class (> 3.5 W) not enabled", sizeof(val_string));

	ssf_add_dict_string(d, name, val_string);
}

static void sff_8636_show_connector(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_connector(data, SFF_8636_CTOR_OFFSET, d);
}

static void sff_8636_show_transceiver(const uint8_t *data, struct rte_tel_data *d)
{
	static const char *name = "Transceiver type";
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];

	snprintf(val_string, sizeof(val_string), "0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x 0x%02x",
		data[SFF_8636_ETHERNET_COMP_OFFSET],
		data[SFF_8636_SONET_COMP_OFFSET],
		data[SFF_8636_SAS_COMP_OFFSET],
		data[SFF_8636_GIGE_COMP_OFFSET],
		data[SFF_8636_FC_LEN_OFFSET],
		data[SFF_8636_FC_TECH_OFFSET],
		data[SFF_8636_FC_TRANS_MEDIA_OFFSET],
		data[SFF_8636_FC_SPEED_OFFSET]);
	ssf_add_dict_string(d, "Transceiver codes", val_string);

	/* 10G/40G Ethernet Compliance Codes */
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_10G_LRM)
		ssf_add_dict_string(d, name, "10G Ethernet: 10G Base-LRM");
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_10G_LR)
		ssf_add_dict_string(d, name, "10G Ethernet: 10G Base-LR");
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_10G_SR)
		ssf_add_dict_string(d, name, "10G Ethernet: 10G Base-SR");
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_40G_CR4)
		ssf_add_dict_string(d, name, "40G Ethernet: 40G Base-CR4");
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_40G_SR4)
		ssf_add_dict_string(d, name, "40G Ethernet: 40G Base-SR4");
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_40G_LR4)
		ssf_add_dict_string(d, name, "40G Ethernet: 40G Base-LR4");
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_40G_ACTIVE)
		ssf_add_dict_string(d, name, "40G Ethernet: 40G Active Cable (XLPPI)");

	/* Extended Specification Compliance Codes from SFF-8024 */
	if (data[SFF_8636_ETHERNET_COMP_OFFSET] & SFF_8636_ETHERNET_RSRVD) {
		switch (data[SFF_8636_OPTION_1_OFFSET]) {
		case SFF_8636_ETHERNET_UNSPECIFIED:
			ssf_add_dict_string(d, name, "(reserved or unknown)");
			break;
		case SFF_8636_ETHERNET_100G_AOC:
			ssf_add_dict_string(d, name,
			"100G Ethernet: 100G AOC or 25GAUI C2M AOC with worst BER of 5x10^(-5)");
			break;
		case SFF_8636_ETHERNET_100G_SR4:
			ssf_add_dict_string(d, name,
					"100G Ethernet: 100G Base-SR4 or 25GBase-SR");
			break;
		case SFF_8636_ETHERNET_100G_LR4:
			ssf_add_dict_string(d, name, "100G Ethernet: 100G Base-LR4");
			break;
		case SFF_8636_ETHERNET_100G_ER4:
			ssf_add_dict_string(d, name, "100G Ethernet: 100G Base-ER4");
			break;
		case SFF_8636_ETHERNET_100G_SR10:
			ssf_add_dict_string(d, name, "100G Ethernet: 100G Base-SR10");
			break;
		case SFF_8636_ETHERNET_100G_CWDM4_FEC:
			ssf_add_dict_string(d, name, "100G Ethernet: 100G CWDM4 MSA with FEC");
			break;
		case SFF_8636_ETHERNET_100G_PSM4:
			ssf_add_dict_string(d, name, "100G Ethernet: 100G PSM4 Parallel SMF");
			break;
		case SFF_8636_ETHERNET_100G_ACC:
			ssf_add_dict_string(d, name,
			"100G Ethernet: 100G ACC or 25GAUI C2M ACC with worst BER of 5x10^(-5)");
			break;
		case SFF_8636_ETHERNET_100G_CWDM4_NO_FEC:
			ssf_add_dict_string(d, name,
					"100G Ethernet: 100G CWDM4 MSA without FEC");
			break;
		case SFF_8636_ETHERNET_100G_RSVD1:
			ssf_add_dict_string(d, name, "(reserved or unknown)");
			break;
		case SFF_8636_ETHERNET_100G_CR4:
			ssf_add_dict_string(d, name,
					"100G Ethernet: 100G Base-CR4 or 25G Base-CR CA-L");
			break;
		case SFF_8636_ETHERNET_25G_CR_CA_S:
			ssf_add_dict_string(d, name, "25G Ethernet: 25G Base-CR CA-S");
			break;
		case SFF_8636_ETHERNET_25G_CR_CA_N:
			ssf_add_dict_string(d, name, "25G Ethernet: 25G Base-CR CA-N");
			break;
		case SFF_8636_ETHERNET_40G_ER4:
			ssf_add_dict_string(d, name, "40G Ethernet: 40G Base-ER4");
			break;
		case SFF_8636_ETHERNET_4X10_SR:
			ssf_add_dict_string(d, name, "4x10G Ethernet: 10G Base-SR");
			break;
		case SFF_8636_ETHERNET_40G_PSM4:
			ssf_add_dict_string(d, name, "40G Ethernet: 40G PSM4 Parallel SMF");
			break;
		case SFF_8636_ETHERNET_G959_P1I1_2D1:
			ssf_add_dict_string(d, name,
				"Ethernet: G959.1 profile P1I1-2D1 (10709 MBd, 2km, 1310nm SM)");
			break;
		case SFF_8636_ETHERNET_G959_P1S1_2D2:
			ssf_add_dict_string(d, name,
				"Ethernet: G959.1 profile P1S1-2D2 (10709 MBd, 40km, 1550nm SM)");
			break;
		case SFF_8636_ETHERNET_G959_P1L1_2D2:
			ssf_add_dict_string(d, name,
				"Ethernet: G959.1 profile P1L1-2D2 (10709 MBd, 80km, 1550nm SM)");
			break;
		case SFF_8636_ETHERNET_10GT_SFI:
			ssf_add_dict_string(d, name,
				"10G Ethernet: 10G Base-T with SFI electrical interface");
			break;
		case SFF_8636_ETHERNET_100G_CLR4:
			ssf_add_dict_string(d, name, "100G Ethernet: 100G CLR4");
			break;
		case SFF_8636_ETHERNET_100G_AOC2:
			ssf_add_dict_string(d, name,
			"100G Ethernet: 100G AOC or 25GAUI C2M AOC with worst BER of 10^(-12)");
			break;
		case SFF_8636_ETHERNET_100G_ACC2:
			ssf_add_dict_string(d, name,
			"100G Ethernet: 100G ACC or 25GAUI C2M ACC with worst BER of 10^(-12)");
			break;
		default:
			ssf_add_dict_string(d, name, "(reserved or unknown)");
			break;
		}
	}

	/* SONET Compliance Codes */
	if (data[SFF_8636_SONET_COMP_OFFSET] & SFF_8636_SONET_40G_OTN)
		ssf_add_dict_string(d, name, "40G OTN (OTU3B/OTU3C)");
	if (data[SFF_8636_SONET_COMP_OFFSET] & SFF_8636_SONET_OC48_LR)
		ssf_add_dict_string(d, name, "SONET: OC-48, long reach");
	if (data[SFF_8636_SONET_COMP_OFFSET] & SFF_8636_SONET_OC48_IR)
		ssf_add_dict_string(d, name, "SONET: OC-48, intermediate reach");
	if (data[SFF_8636_SONET_COMP_OFFSET] & SFF_8636_SONET_OC48_SR)
		ssf_add_dict_string(d, name, "SONET: OC-48, short reach");

	/* SAS/SATA Compliance Codes */
	if (data[SFF_8636_SAS_COMP_OFFSET] & SFF_8636_SAS_6G)
		ssf_add_dict_string(d, name, "SAS 6.0G");
	if (data[SFF_8636_SAS_COMP_OFFSET] & SFF_8636_SAS_3G)
		ssf_add_dict_string(d, name, "SAS 3.0G");

	/* Ethernet Compliance Codes */
	if (data[SFF_8636_GIGE_COMP_OFFSET] & SFF_8636_GIGE_1000_BASE_T)
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-T");
	if (data[SFF_8636_GIGE_COMP_OFFSET] & SFF_8636_GIGE_1000_BASE_CX)
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-CX");
	if (data[SFF_8636_GIGE_COMP_OFFSET] & SFF_8636_GIGE_1000_BASE_LX)
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-LX");
	if (data[SFF_8636_GIGE_COMP_OFFSET] & SFF_8636_GIGE_1000_BASE_SX)
		ssf_add_dict_string(d, name, "Ethernet: 1000BASE-SX");

	/* Fibre Channel link length */
	if (data[SFF_8636_FC_LEN_OFFSET] & SFF_8636_FC_LEN_VERY_LONG)
		ssf_add_dict_string(d, name, "FC: very long distance (V)");
	if (data[SFF_8636_FC_LEN_OFFSET] & SFF_8636_FC_LEN_SHORT)
		ssf_add_dict_string(d, name, "FC: short distance (S)");
	if (data[SFF_8636_FC_LEN_OFFSET] & SFF_8636_FC_LEN_INT)
		ssf_add_dict_string(d, name, "FC: intermediate distance (I)");
	if (data[SFF_8636_FC_LEN_OFFSET] & SFF_8636_FC_LEN_LONG)
		ssf_add_dict_string(d, name, "FC: long distance (L)");
	if (data[SFF_8636_FC_LEN_OFFSET] & SFF_8636_FC_LEN_MED)
		ssf_add_dict_string(d, name, "FC: medium distance (M)");

	/* Fibre Channel transmitter technology */
	if (data[SFF_8636_FC_LEN_OFFSET] & SFF_8636_FC_TECH_LONG_LC)
		ssf_add_dict_string(d, name, "FC: Longwave laser (LC)");
	if (data[SFF_8636_FC_LEN_OFFSET] & SFF_8636_FC_TECH_ELEC_INTER)
		ssf_add_dict_string(d, name, "FC: Electrical inter-enclosure (EL)");
	if (data[SFF_8636_FC_TECH_OFFSET] & SFF_8636_FC_TECH_ELEC_INTRA)
		ssf_add_dict_string(d, name, "FC: Electrical intra-enclosure (EL)");
	if (data[SFF_8636_FC_TECH_OFFSET] & SFF_8636_FC_TECH_SHORT_WO_OFC)
		ssf_add_dict_string(d, name, "FC: Shortwave laser w/o OFC (SN)");
	if (data[SFF_8636_FC_TECH_OFFSET] & SFF_8636_FC_TECH_SHORT_W_OFC)
		ssf_add_dict_string(d, name, "FC: Shortwave laser with OFC (SL)");
	if (data[SFF_8636_FC_TECH_OFFSET] & SFF_8636_FC_TECH_LONG_LL)
		ssf_add_dict_string(d, name, "FC: Longwave laser (LL)");

	/* Fibre Channel transmission media */
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_TW)
		ssf_add_dict_string(d, name, "FC: Twin Axial Pair (TW)");
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_TP)
		ssf_add_dict_string(d, name, "FC: Twisted Pair (TP)");
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_MI)
		ssf_add_dict_string(d, name, "FC: Miniature Coax (MI)");
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_TV)
		ssf_add_dict_string(d, name, "FC: Video Coax (TV)");
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_M6)
		ssf_add_dict_string(d, name, "FC: Multimode, 62.5m (M6)");
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_M5)
		ssf_add_dict_string(d, name, "FC: Multimode, 50m (M5)");
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_OM3)
		ssf_add_dict_string(d, name, "FC: Multimode, 50um (OM3)");
	if (data[SFF_8636_FC_TRANS_MEDIA_OFFSET] & SFF_8636_FC_TRANS_MEDIA_SM)
		ssf_add_dict_string(d, name, "FC: Single Mode (SM)");

	/* Fibre Channel speed */
	if (data[SFF_8636_FC_SPEED_OFFSET] & SFF_8636_FC_SPEED_1200_MBPS)
		ssf_add_dict_string(d, name, "FC: 1200 MBytes/sec");
	if (data[SFF_8636_FC_SPEED_OFFSET] & SFF_8636_FC_SPEED_800_MBPS)
		ssf_add_dict_string(d, name, "FC: 800 MBytes/sec");
	if (data[SFF_8636_FC_SPEED_OFFSET] & SFF_8636_FC_SPEED_1600_MBPS)
		ssf_add_dict_string(d, name, "FC: 1600 MBytes/sec");
	if (data[SFF_8636_FC_SPEED_OFFSET] & SFF_8636_FC_SPEED_400_MBPS)
		ssf_add_dict_string(d, name, "FC: 400 MBytes/sec");
	if (data[SFF_8636_FC_SPEED_OFFSET] & SFF_8636_FC_SPEED_200_MBPS)
		ssf_add_dict_string(d, name, "FC: 200 MBytes/sec");
	if (data[SFF_8636_FC_SPEED_OFFSET] & SFF_8636_FC_SPEED_100_MBPS)
		ssf_add_dict_string(d, name, "FC: 100 MBytes/sec");
}

static void sff_8636_show_encoding(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_encoding(data, SFF_8636_ENCODING_OFFSET,
			       RTE_ETH_MODULE_SFF_8636, d);
}

static void sff_8636_show_rate_identifier(const uint8_t *data, struct rte_tel_data *d)
{
	char val_string[20];

	snprintf(val_string, sizeof(val_string), "0x%02x", data[SFF_8636_EXT_RS_OFFSET]);
	ssf_add_dict_string(d, "Rate identifier", val_string);
}

static void sff_8636_show_oui(const uint8_t *data, struct rte_tel_data *d)
{
	sff_8024_show_oui(data, SFF_8636_VENDOR_OUI_OFFSET, d);
}

static void sff_8636_show_wavelength_or_copper_compliance(const uint8_t *data,
							  struct rte_tel_data *d)
{
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];
	snprintf(val_string, sizeof(val_string), "0x%02x",
		(data[SFF_8636_DEVICE_TECH_OFFSET] & SFF_8636_TRANS_TECH_MASK));

	switch (data[SFF_8636_DEVICE_TECH_OFFSET] & SFF_8636_TRANS_TECH_MASK) {
	case SFF_8636_TRANS_850_VCSEL:
		strlcat(val_string, " (850 nm VCSEL)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1310_VCSEL:
		strlcat(val_string, " (1310 nm VCSEL)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1550_VCSEL:
		strlcat(val_string, " (1550 nm VCSEL)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1310_FP:
		strlcat(val_string, " (1310 nm FP)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1310_DFB:
		strlcat(val_string, " (1310 nm DFB)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1550_DFB:
		strlcat(val_string, " (1550 nm DFB)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1310_EML:
		strlcat(val_string, " (1310 nm EML)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1550_EML:
		strlcat(val_string, " (1550 nm EML)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_OTHERS:
		strlcat(val_string, " (Others/Undefined)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_1490_DFB:
		strlcat(val_string, " (1490 nm DFB)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_COPPER_PAS_UNEQUAL:
		strlcat(val_string, " (Copper cable unequalized)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_COPPER_PAS_EQUAL:
		strlcat(val_string, " (Copper cable passive equalized)", sizeof(val_string));
		break;
	case SFF_8636_TRANS_COPPER_LNR_FAR_EQUAL:
		strlcat(val_string,
		       " (Copper cable, near and far end limiting active equalizers)",
		       sizeof(val_string));
		break;
	case SFF_8636_TRANS_COPPER_FAR_EQUAL:
		strlcat(val_string,
			" (Copper cable, far end limiting active equalizers)",
			sizeof(val_string));
		break;
	case SFF_8636_TRANS_COPPER_NEAR_EQUAL:
		strlcat(val_string,
			" (Copper cable, near end limiting active equalizers)",
			sizeof(val_string));
		break;
	case SFF_8636_TRANS_COPPER_LNR_EQUAL:
		strlcat(val_string,
			" (Copper cable, linear active equalizers)",
			sizeof(val_string));
		break;
	}
	ssf_add_dict_string(d, "Transmitter technology", val_string);

	if ((data[SFF_8636_DEVICE_TECH_OFFSET] & SFF_8636_TRANS_TECH_MASK)
			>= SFF_8636_TRANS_COPPER_PAS_UNEQUAL) {
		snprintf(val_string, sizeof(val_string), "%udb",
			 data[SFF_8636_WAVELEN_HIGH_BYTE_OFFSET]);
		ssf_add_dict_string(d, "Attenuation at 2.5GHz", val_string);

		snprintf(val_string, sizeof(val_string), "%udb",
			 data[SFF_8636_WAVELEN_HIGH_BYTE_OFFSET]);
		ssf_add_dict_string(d, "Attenuation at 5.0GHz", val_string);

		snprintf(val_string, sizeof(val_string), "%udb",
			 data[SFF_8636_WAVELEN_HIGH_BYTE_OFFSET]);
		ssf_add_dict_string(d, "Attenuation at 7.0GHz", val_string);

		snprintf(val_string, sizeof(val_string), "%udb",
			 data[SFF_8636_WAVELEN_HIGH_BYTE_OFFSET]);
		ssf_add_dict_string(d, "Attenuation at 12.9GHz", val_string);
	} else {
		snprintf(val_string, sizeof(val_string), "%.3lfnm",
			(((data[SFF_8636_WAVELEN_HIGH_BYTE_OFFSET] << 8) |
			data[SFF_8636_WAVELEN_LOW_BYTE_OFFSET])*0.05));
		ssf_add_dict_string(d, "Laser wavelength", val_string);

		snprintf(val_string, sizeof(val_string), "%.3lfnm",
			(((data[SFF_8636_WAVE_TOL_HIGH_BYTE_OFFSET] << 8) |
			data[SFF_8636_WAVE_TOL_LOW_BYTE_OFFSET])*0.005));
		ssf_add_dict_string(d, "Laser wavelength tolerance", val_string);
	}
}

static void sff_8636_show_revision_compliance(const uint8_t *data, struct rte_tel_data *d)
{
	static const char *name = "Revision Compliance";

	switch (data[SFF_8636_REV_COMPLIANCE_OFFSET]) {
	case SFF_8636_REV_UNSPECIFIED:
		ssf_add_dict_string(d, name, "Revision not specified");
		break;
	case SFF_8636_REV_8436_48:
		ssf_add_dict_string(d, name, "SFF-8436 Rev 4.8 or earlier");
		break;
	case SFF_8636_REV_8436_8636:
		ssf_add_dict_string(d, name, "SFF-8436 Rev 4.8 or earlier");
		break;
	case SFF_8636_REV_8636_13:
		ssf_add_dict_string(d, name, "SFF-8636 Rev 1.3 or earlier");
		break;
	case SFF_8636_REV_8636_14:
		ssf_add_dict_string(d, name, "SFF-8636 Rev 1.4");
		break;
	case SFF_8636_REV_8636_15:
		ssf_add_dict_string(d, name, "SFF-8636 Rev 1.5");
		break;
	case SFF_8636_REV_8636_20:
		ssf_add_dict_string(d, name, "SFF-8636 Rev 2.0");
		break;
	case SFF_8636_REV_8636_27:
		ssf_add_dict_string(d, name, "SFF-8636 Rev 2.5/2.6/2.7");
		break;
	default:
		ssf_add_dict_string(d, name, "Unallocated");
		break;
	}
}

/*
 * 2-byte internal temperature conversions:
 * First byte is a signed 8-bit integer, which is the temp decimal part
 * Second byte are 1/256th of degree, which are added to the dec part.
 */
#define SFF_8636_OFFSET_TO_TEMP(offset) ((int16_t)SFF_OFFSET_TO_U16(offset))

static void sff_8636_dom_parse(const uint8_t *data, struct sff_diags *sd)
{
	int i = 0;

	/* Monitoring Thresholds for Alarms and Warnings */
	sd->sfp_voltage[SFF_MCURR] = SFF_OFFSET_TO_U16(SFF_8636_VCC_CURR);
	sd->sfp_voltage[SFF_HALRM] = SFF_OFFSET_TO_U16(SFF_8636_VCC_HALRM);
	sd->sfp_voltage[SFF_LALRM] = SFF_OFFSET_TO_U16(SFF_8636_VCC_LALRM);
	sd->sfp_voltage[SFF_HWARN] = SFF_OFFSET_TO_U16(SFF_8636_VCC_HWARN);
	sd->sfp_voltage[SFF_LWARN] = SFF_OFFSET_TO_U16(SFF_8636_VCC_LWARN);

	sd->sfp_temp[SFF_MCURR] = SFF_8636_OFFSET_TO_TEMP(SFF_8636_TEMP_CURR);
	sd->sfp_temp[SFF_HALRM] = SFF_8636_OFFSET_TO_TEMP(SFF_8636_TEMP_HALRM);
	sd->sfp_temp[SFF_LALRM] = SFF_8636_OFFSET_TO_TEMP(SFF_8636_TEMP_LALRM);
	sd->sfp_temp[SFF_HWARN] = SFF_8636_OFFSET_TO_TEMP(SFF_8636_TEMP_HWARN);
	sd->sfp_temp[SFF_LWARN] = SFF_8636_OFFSET_TO_TEMP(SFF_8636_TEMP_LWARN);

	sd->bias_cur[SFF_HALRM] = SFF_OFFSET_TO_U16(SFF_8636_TX_BIAS_HALRM);
	sd->bias_cur[SFF_LALRM] = SFF_OFFSET_TO_U16(SFF_8636_TX_BIAS_LALRM);
	sd->bias_cur[SFF_HWARN] = SFF_OFFSET_TO_U16(SFF_8636_TX_BIAS_HWARN);
	sd->bias_cur[SFF_LWARN] = SFF_OFFSET_TO_U16(SFF_8636_TX_BIAS_LWARN);

	sd->tx_power[SFF_HALRM] = SFF_OFFSET_TO_U16(SFF_8636_TX_PWR_HALRM);
	sd->tx_power[SFF_LALRM] = SFF_OFFSET_TO_U16(SFF_8636_TX_PWR_LALRM);
	sd->tx_power[SFF_HWARN] = SFF_OFFSET_TO_U16(SFF_8636_TX_PWR_HWARN);
	sd->tx_power[SFF_LWARN] = SFF_OFFSET_TO_U16(SFF_8636_TX_PWR_LWARN);

	sd->rx_power[SFF_HALRM] = SFF_OFFSET_TO_U16(SFF_8636_RX_PWR_HALRM);
	sd->rx_power[SFF_LALRM] = SFF_OFFSET_TO_U16(SFF_8636_RX_PWR_LALRM);
	sd->rx_power[SFF_HWARN] = SFF_OFFSET_TO_U16(SFF_8636_RX_PWR_HWARN);
	sd->rx_power[SFF_LWARN] = SFF_OFFSET_TO_U16(SFF_8636_RX_PWR_LWARN);


	/* Channel Specific Data */
	for (i = 0; i < SFF_MAX_CHANNEL_NUM; i++) {
		sd->scd[i].bias_cur = SFF_OFFSET_TO_U16(sff_8636_tx_bias_offset[i]);
		sd->scd[i].rx_power = SFF_OFFSET_TO_U16(sff_8636_rx_power_offset[i]);
		sd->scd[i].tx_power = SFF_OFFSET_TO_U16(sff_8636_tx_power_offset[i]);
	}

}

static void sff_8636_show_dom(const uint8_t *data, uint32_t eeprom_len, struct rte_tel_data *d)
{
	struct sff_diags sd = {0};
	const char *rx_power_string = NULL;
	char power_string[SFF_MAX_DESC_SIZE];
	char val_string[SFF_ITEM_VAL_COMPOSE_SIZE];
	int i;

	/*
	 * There is no clear identifier to signify the existence of
	 * optical diagnostics similar to SFF-8472. So checking existence
	 * of page 3, will provide the guarantee for existence of alarms
	 * and thresholds
	 * If pagging support exists, then supports_alarms is marked as 1
	 */

	if (eeprom_len == RTE_ETH_MODULE_SFF_8636_MAX_LEN) {
		if (!(data[SFF_8636_STATUS_2_OFFSET] &
					SFF_8636_STATUS_PAGE_3_PRESENT)) {
			sd.supports_alarms = 1;
		}
	}

	sd.rx_power_type = data[SFF_8636_DIAG_TYPE_OFFSET] &
						SFF_8636_RX_PWR_TYPE_MASK;
	sd.tx_power_type = data[SFF_8636_DIAG_TYPE_OFFSET] &
						SFF_8636_RX_PWR_TYPE_MASK;

	sff_8636_dom_parse(data, &sd);

	SFF_SPRINT_TEMP(val_string, sd.sfp_temp[SFF_MCURR]);
	ssf_add_dict_string(d, "Module temperature", val_string);

	SFF_SPRINT_VCC(val_string, sd.sfp_voltage[SFF_MCURR]);
	ssf_add_dict_string(d, "Module voltage", val_string);

	/*
	 * SFF-8636/8436 spec is not clear whether RX power/ TX bias
	 * current fields are supported or not. A valid temperature
	 * reading is used as existence for TX/RX power.
	 */
	if ((sd.sfp_temp[SFF_MCURR] == 0x0) ||
	    (sd.sfp_temp[SFF_MCURR] == (int16_t)0xFFFF))
		return;

	ssf_add_dict_string(d, "Alarm/warning flags implemented",
			(sd.supports_alarms ? "Yes" : "No"));

	for (i = 0; i < SFF_MAX_CHANNEL_NUM; i++) {
		snprintf(power_string, SFF_MAX_DESC_SIZE, "%s (Channel %d)",
					"Laser tx bias current", i+1);
		SFF_SPRINT_BIAS(val_string, sd.scd[i].bias_cur);
		ssf_add_dict_string(d, power_string, val_string);
	}

	for (i = 0; i < SFF_MAX_CHANNEL_NUM; i++) {
		snprintf(power_string, SFF_MAX_DESC_SIZE, "%s (Channel %d)",
					"Transmit avg optical power", i+1);
		SFF_SPRINT_xX_PWR(val_string, sd.scd[i].tx_power);
		ssf_add_dict_string(d, power_string, val_string);
	}

	if (!sd.rx_power_type)
		rx_power_string = "Receiver signal OMA";
	else
		rx_power_string = "Rcvr signal avg optical power";

	for (i = 0; i < SFF_MAX_CHANNEL_NUM; i++) {
		snprintf(power_string, SFF_MAX_DESC_SIZE, "%s(Channel %d)",
					rx_power_string, i+1);
		SFF_SPRINT_xX_PWR(val_string, sd.scd[i].rx_power);
		ssf_add_dict_string(d, power_string, val_string);
	}

	if (sd.supports_alarms) {
		for (i = 0; sff_8636_aw_flags[i].str; ++i) {
			ssf_add_dict_string(d, sff_8636_aw_flags[i].str,
					data[sff_8636_aw_flags[i].offset]
					& sff_8636_aw_flags[i].value ? "On" : "Off");
		}

		sff_show_thresholds(sd, d);
	}

}
void sff_8636_show_all(const uint8_t *data, uint32_t eeprom_len, struct rte_tel_data *d)
{
	sff_8636_show_identifier(data, d);
	if ((data[SFF_8636_ID_OFFSET] == SFF_8024_ID_QSFP) ||
		(data[SFF_8636_ID_OFFSET] == SFF_8024_ID_QSFP_PLUS) ||
		(data[SFF_8636_ID_OFFSET] == SFF_8024_ID_QSFP28)) {
		sff_8636_show_ext_identifier(data, d);
		sff_8636_show_connector(data, d);
		sff_8636_show_transceiver(data, d);
		sff_8636_show_encoding(data, d);
		sff_show_value_with_unit(data, SFF_8636_BR_NOMINAL_OFFSET,
				"BR, Nominal", 100, "Mbps", d);
		sff_8636_show_rate_identifier(data, d);
		sff_show_value_with_unit(data, SFF_8636_SM_LEN_OFFSET,
			     "Length (SMF,km)", 1, "km", d);
		sff_show_value_with_unit(data, SFF_8636_OM3_LEN_OFFSET,
				"Length (OM3 50um)", 2, "m", d);
		sff_show_value_with_unit(data, SFF_8636_OM2_LEN_OFFSET,
				"Length (OM2 50um)", 1, "m", d);
		sff_show_value_with_unit(data, SFF_8636_OM1_LEN_OFFSET,
			     "Length (OM1 62.5um)", 1, "m", d);
		sff_show_value_with_unit(data, SFF_8636_CBL_LEN_OFFSET,
			     "Length (Copper or Active cable)", 1, "m", d);
		sff_8636_show_wavelength_or_copper_compliance(data, d);
		sff_show_ascii(data, SFF_8636_VENDOR_NAME_START_OFFSET,
			     SFF_8636_VENDOR_NAME_END_OFFSET, "Vendor name", d);
		sff_8636_show_oui(data, d);
		sff_show_ascii(data, SFF_8636_VENDOR_PN_START_OFFSET,
			     SFF_8636_VENDOR_PN_END_OFFSET, "Vendor PN", d);
		sff_show_ascii(data, SFF_8636_VENDOR_REV_START_OFFSET,
			     SFF_8636_VENDOR_REV_END_OFFSET, "Vendor rev", d);
		sff_show_ascii(data, SFF_8636_VENDOR_SN_START_OFFSET,
			     SFF_8636_VENDOR_SN_END_OFFSET, "Vendor SN", d);
		sff_show_ascii(data, SFF_8636_DATE_YEAR_OFFSET,
			     SFF_8636_DATE_VENDOR_LOT_OFFSET + 1, "Date code", d);
		sff_8636_show_revision_compliance(data, d);
		sff_8636_show_dom(data, eeprom_len, d);
	}
}
