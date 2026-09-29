/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2026 Roman Khromenok
 */

#include <errno.h>
#include <stdalign.h>
#include <stdio.h>
#include <string.h>

#include <rte_common.h>
#include <rte_ethdev.h>
#include <rte_string_fns.h>

#include "test.h"

#define MAX_FIELDS 128
#define MAX_NAME_LEN 64
#define MAX_VALUE_LEN 128

struct decoded_fields {
	unsigned int count;
	struct {
		char name[MAX_NAME_LEN];
		char value[MAX_VALUE_LEN];
	} field[MAX_FIELDS];
};

static struct decoded_fields fields;

static void
collect_field(const char *name, const char *value, void *arg)
{
	struct decoded_fields *f = arg;

	if (f->count >= MAX_FIELDS)
		return;
	strlcpy(f->field[f->count].name, name, MAX_NAME_LEN);
	strlcpy(f->field[f->count].value, value, MAX_VALUE_LEN);
	f->count++;
}

static const char *
find_field(const char *name)
{
	unsigned int i;

	for (i = 0; i < fields.count; i++)
		if (strcmp(fields.field[i].name, name) == 0)
			return fields.field[i].value;
	return NULL;
}

static int
parse(uint32_t type, const uint8_t *data, uint32_t length)
{
	memset(&fields, 0, sizeof(fields));
	return rte_eth_module_eeprom_parse(type, data, length, collect_field, &fields);
}

#define CHECK_FIELD(name, expected) do { \
	const char *value = find_field(name); \
	TEST_ASSERT_NOT_NULL(value, "Field \"%s\" not found", name); \
	TEST_ASSERT(strcmp(value, expected) == 0, \
		"Field \"%s\": expected \"%s\", got \"%s\"", name, expected, value); \
} while (0)

#define CHECK_NO_FIELD(name) \
	TEST_ASSERT_NULL(find_field(name), "Unexpected field \"%s\"", name)

static void
put_u16(uint8_t *data, unsigned int offset, uint16_t value)
{
	data[offset] = value >> 8;
	data[offset + 1] = value & 0xff;
}

/* SFP+ 10GBASE-LR module, SFF-8472 layout: page A0h then page A2h */
static void
fill_sfp(uint8_t *data)
{
	uint8_t *a2 = data + RTE_ETH_MODULE_SFF_8079_LEN;

	memset(data, 0, RTE_ETH_MODULE_SFF_8472_LEN);
	data[0] = 0x03;		/* identifier: SFP */
	data[1] = 0x04;		/* extended identifier */
	data[2] = 0x07;		/* connector: LC */
	data[3] = 0x20;		/* 10G Base-LR */
	data[11] = 0x06;	/* encoding: 64B/66B */
	data[12] = 103;		/* nominal bit rate, units of 100 MBd */
	memcpy(&data[20], "STARRY          ", 16);
	data[37] = 0x00;
	data[38] = 0x1b;
	data[39] = 0x21;
	memcpy(&data[40], "SFP-10G-LR-20   ", 16);
	memcpy(&data[56], "A   ", 4);
	memcpy(&data[68], "2024082600001   ", 16);
	memcpy(&data[84], "240902  ", 8);
	/* diagnostics implemented, internally calibrated, average RX power */
	data[92] = 0x68;

	put_u16(a2, 96, 0x2940);	/* temperature: 41.25 C */
	put_u16(a2, 98, 33205);		/* voltage: 3.3205 V */
	put_u16(a2, 100, 19790);	/* TX bias: 39.580 mA */
	put_u16(a2, 102, 7754);		/* TX power: 0.7754 mW */
	put_u16(a2, 104, 6724);		/* RX power: 0.6724 mW */
}

/* QSFP28 module, SFF-8636 layout: lower page then upper page 00h */
static void
fill_qsfp(uint8_t *data)
{
	memset(data, 0, RTE_ETH_MODULE_SFF_8636_LEN);
	data[0] = 0x11;		/* identifier: QSFP28 */
	put_u16(data, 22, 0x3040);	/* temperature: 48.25 C */
	put_u16(data, 26, 32638);	/* voltage: 3.2638 V */
	put_u16(data, 34, 1);		/* channel 1 RX power: 0.0001 mW */
	put_u16(data, 42, 21525);	/* channel 1 TX bias: 43.050 mA */
	put_u16(data, 50, 12763);	/* channel 1 TX power: 1.2763 mW */

	data[128] = 0x11;	/* identifier: QSFP28 */
	data[130] = 0x07;	/* connector: LC */
	memcpy(&data[148], "FINISAR CORP.   ", 16);
	data[165] = 0x00;
	data[166] = 0x90;
	data[167] = 0x65;
	memcpy(&data[168], "FTLC1157RGPL6-FB", 16);
	memcpy(&data[184], "A0", 2);
	memcpy(&data[196], "X24A15P         ", 16);
	memcpy(&data[212], "190826  ", 8);
	data[220] = 0x08;	/* average RX power */
}

static int
check_sfp_identity(void)
{
	CHECK_FIELD("Identifier", "0x03 (SFP)");
	CHECK_FIELD("Connector", "0x07 (LC)");
	CHECK_FIELD("Vendor name", "STARRY");
	CHECK_FIELD("Vendor OUI", "00:1b:21");
	CHECK_FIELD("Vendor PN", "SFP-10G-LR-20");
	CHECK_FIELD("Vendor rev", "A");
	CHECK_FIELD("Vendor SN", "2024082600001");
	CHECK_FIELD("Date code", "240902");
	return TEST_SUCCESS;
}

static int
test_module_eeprom_sfp_8079(void)
{
	uint8_t data[RTE_ETH_MODULE_SFF_8472_LEN];

	fill_sfp(data);
	TEST_ASSERT_SUCCESS(parse(RTE_ETH_MODULE_SFF_8079, data, RTE_ETH_MODULE_SFF_8079_LEN),
		"Failed to parse SFF-8079 data");
	TEST_ASSERT_SUCCESS(check_sfp_identity(), "Wrong SFF-8079 base information");
	CHECK_NO_FIELD("Optical diagnostics support");
	return TEST_SUCCESS;
}

static int
test_module_eeprom_sfp_8472(void)
{
	uint8_t data[RTE_ETH_MODULE_SFF_8472_LEN];

	fill_sfp(data);
	TEST_ASSERT_SUCCESS(parse(RTE_ETH_MODULE_SFF_8472, data, sizeof(data)),
		"Failed to parse SFF-8472 data");
	TEST_ASSERT_SUCCESS(check_sfp_identity(), "Wrong SFF-8472 base information");
	CHECK_FIELD("Optical diagnostics support", "Yes");
	CHECK_FIELD("Laser bias current", "39.580 mA");
	CHECK_FIELD("Laser output power", "0.7754 mW / -1.10 dBm");
	CHECK_FIELD("Receiver signal average optical power", "0.6724 mW / -1.72 dBm");
	CHECK_FIELD("Module temperature", "41.25 degrees C / 106.25 degrees F");
	CHECK_FIELD("Module voltage", "3.3205 V");
	return TEST_SUCCESS;
}

static int
test_module_eeprom_sfp_8472_short(void)
{
	uint8_t data[RTE_ETH_MODULE_SFF_8472_LEN];
	const uint32_t lengths[] = {
		RTE_ETH_MODULE_SFF_8079_LEN,
		RTE_ETH_MODULE_SFF_8472_LEN - 1,
	};
	unsigned int i;

	fill_sfp(data);
	for (i = 0; i < RTE_DIM(lengths); i++) {
		TEST_ASSERT_SUCCESS(parse(RTE_ETH_MODULE_SFF_8472, data, lengths[i]),
			"Failed to parse SFF-8472 data of %u bytes", lengths[i]);
		TEST_ASSERT_SUCCESS(check_sfp_identity(),
			"Wrong SFF-8472 base information from %u bytes", lengths[i]);
		/* page A2h is not complete, diagnostics must be skipped */
		CHECK_NO_FIELD("Optical diagnostics support");
		CHECK_NO_FIELD("Laser bias current");
	}
	return TEST_SUCCESS;
}

static int
test_module_eeprom_qsfp_8636(void)
{
	uint8_t data[RTE_ETH_MODULE_SFF_8636_LEN];

	fill_qsfp(data);
	TEST_ASSERT_SUCCESS(parse(RTE_ETH_MODULE_SFF_8636, data, sizeof(data)),
		"Failed to parse SFF-8636 data");
	CHECK_FIELD("Identifier", "0x11 (QSFP28)");
	CHECK_FIELD("Connector", "0x07 (LC)");
	CHECK_FIELD("Vendor name", "FINISAR CORP.");
	CHECK_FIELD("Vendor OUI", "00:90:65");
	CHECK_FIELD("Vendor PN", "FTLC1157RGPL6-FB");
	CHECK_FIELD("Vendor rev", "A0");
	CHECK_FIELD("Vendor SN", "X24A15P");
	CHECK_FIELD("Module temperature", "48.25 degrees C / 118.85 degrees F");
	CHECK_FIELD("Module voltage", "3.2638 V");
	/* page 03h is not provided */
	CHECK_FIELD("Alarm/warning flags implemented", "No");
	CHECK_FIELD("Laser tx bias current (Channel 1)", "43.050 mA");
	CHECK_FIELD("Transmit avg optical power (Channel 1)", "1.2763 mW / 1.06 dBm");
	CHECK_FIELD("Rcvr signal avg optical power(Channel 1)", "0.0001 mW / -40.00 dBm");
	CHECK_NO_FIELD("Module temperature high alarm threshold");
	return TEST_SUCCESS;
}

static int
test_module_eeprom_qsfp_8636_thresholds(void)
{
	uint8_t data[RTE_ETH_MODULE_SFF_8636_MAX_LEN];

	memset(data, 0, sizeof(data));
	/* paging is supported (flat memory bit is clear), page 03h is at 0x200 */
	fill_qsfp(data);
	put_u16(data, 0x200, 0x5000);	/* temperature high alarm: 80 C */

	TEST_ASSERT_SUCCESS(parse(RTE_ETH_MODULE_SFF_8636, data, sizeof(data)),
		"Failed to parse SFF-8636 data with page 03h");
	CHECK_FIELD("Vendor name", "FINISAR CORP.");
	CHECK_FIELD("Alarm/warning flags implemented", "Yes");
	CHECK_FIELD("Module temperature high alarm threshold",
		"80.00 degrees C / 176.00 degrees F");
	return TEST_SUCCESS;
}

static int
test_module_eeprom_sfp_8472_ext_cal_unaligned(void)
{
	/* the SFF-8472 calibration floats must be readable from any address */
	uint8_t alignas(8) buf[RTE_ETH_MODULE_SFF_8472_LEN + 1];
	uint8_t *data = buf + 1;
	uint8_t *a2 = data + RTE_ETH_MODULE_SFF_8079_LEN;
	const uint8_t two[4] = { 0x40, 0x00, 0x00, 0x00 };	/* 2.0f big endian */

	fill_sfp(data);
	/* diagnostics implemented, externally calibrated, average RX power */
	data[92] = 0x58;
	/* unit slopes, zero offsets, RX power multiplied by RX_PWR(1) */
	a2[76] = 1;	/* TX bias slope */
	a2[80] = 1;	/* TX power slope */
	a2[84] = 1;	/* temperature slope */
	a2[88] = 1;	/* voltage slope */
	memcpy(&a2[68], two, sizeof(two));

	TEST_ASSERT_SUCCESS(parse(RTE_ETH_MODULE_SFF_8472, data, RTE_ETH_MODULE_SFF_8472_LEN),
		"Failed to parse externally calibrated SFF-8472 data");
	CHECK_FIELD("Laser bias current", "39.580 mA");
	CHECK_FIELD("Receiver signal average optical power", "1.3448 mW / 1.29 dBm");
	return TEST_SUCCESS;
}

static int
test_module_eeprom_invalid(void)
{
	uint8_t data[RTE_ETH_MODULE_SFF_8636_MAX_LEN] = {0};
	const uint32_t types[] = {
		RTE_ETH_MODULE_SFF_8079,
		RTE_ETH_MODULE_SFF_8472,
		RTE_ETH_MODULE_SFF_8436,
		RTE_ETH_MODULE_SFF_8636,
	};
	unsigned int i;
	int ret;

	/* all supported types require at least one full page */
	for (i = 0; i < RTE_DIM(types); i++) {
		ret = parse(types[i], data, 255);
		TEST_ASSERT_EQUAL(ret, -EINVAL,
			"Type %u with 255 bytes: expected %d, got %d", types[i], -EINVAL, ret);
		TEST_ASSERT_EQUAL(fields.count, 0,
			"Type %u with 255 bytes: callback called %u times",
			types[i], fields.count);
	}

	ret = parse(0, data, sizeof(data));
	TEST_ASSERT_EQUAL(ret, -ENOTSUP, "Type 0: expected %d, got %d", -ENOTSUP, ret);
	ret = parse(RTE_ETH_MODULE_SFF_8436 + 1, data, sizeof(data));
	TEST_ASSERT_EQUAL(ret, -ENOTSUP, "Unknown type: expected %d, got %d", -ENOTSUP, ret);
	TEST_ASSERT_EQUAL(fields.count, 0, "Unknown type: callback called %u times",
		fields.count);

	ret = rte_eth_module_eeprom_parse(RTE_ETH_MODULE_SFF_8079, NULL,
		RTE_ETH_MODULE_SFF_8079_LEN, collect_field, &fields);
	TEST_ASSERT_EQUAL(ret, -EINVAL, "NULL data: expected %d, got %d", -EINVAL, ret);
	ret = rte_eth_module_eeprom_parse(RTE_ETH_MODULE_SFF_8079, data,
		RTE_ETH_MODULE_SFF_8079_LEN, NULL, NULL);
	TEST_ASSERT_EQUAL(ret, -EINVAL, "NULL callback: expected %d, got %d", -EINVAL, ret);

	return TEST_SUCCESS;
}

static struct unit_test_suite module_eeprom_testsuite = {
	.suite_name = "ethdev module EEPROM decoding",
	.setup = NULL,
	.teardown = NULL,
	.unit_test_cases = {
		TEST_CASE(test_module_eeprom_sfp_8079),
		TEST_CASE(test_module_eeprom_sfp_8472),
		TEST_CASE(test_module_eeprom_sfp_8472_short),
		TEST_CASE(test_module_eeprom_sfp_8472_ext_cal_unaligned),
		TEST_CASE(test_module_eeprom_qsfp_8636),
		TEST_CASE(test_module_eeprom_qsfp_8636_thresholds),
		TEST_CASE(test_module_eeprom_invalid),
		TEST_CASES_END()
	}
};

static int
test_module_eeprom(void)
{
	return unit_test_suite_runner(&module_eeprom_testsuite);
}

REGISTER_FAST_TEST(ethdev_module_eeprom_autotest, NOHUGE_OK, ASAN_OK, test_module_eeprom);
