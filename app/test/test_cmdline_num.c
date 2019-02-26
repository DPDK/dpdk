/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <string.h>
#include <inttypes.h>

#include <rte_string_fns.h>

#include <cmdline_parse.h>
#include <cmdline_parse_num.h>

#include "test_cmdline.h"

struct num_unsigned_str {
	const char * str;
	uint64_t result;
};

struct num_signed_str {
	const char * str;
	int64_t result;
};

const struct num_unsigned_str num_valid_positive_strs[] = {
		/* decimal positive */
		{"0", 0 },
		{"127", INT8_MAX },
		{"128", INT8_MAX + 1 },
		{"255", UINT8_MAX },
		{"256", UINT8_MAX + 1 },
		{"32767", INT16_MAX },
		{"32768", INT16_MAX + 1 },
		{"65535", UINT16_MAX },
		{"65536", UINT16_MAX + 1 },
		{"2147483647", INT32_MAX },
		{"2147483648", INT32_MAX + 1U },
		{"4294967295", UINT32_MAX },
		{"4294967296", UINT32_MAX + 1ULL },
		{"9223372036854775807", INT64_MAX },
		{"9223372036854775808", INT64_MAX + 1ULL},
		{"18446744073709551615", UINT64_MAX },
		/* hexadecimal (no leading zeroes) */
		{"0x0", 0 },
		{"0x7F", INT8_MAX },
		{"0x80", INT8_MAX + 1 },
		{"0xFF", UINT8_MAX },
		{"0x100", UINT8_MAX + 1 },
		{"0x7FFF", INT16_MAX },
		{"0x8000", INT16_MAX + 1 },
		{"0xFFFF", UINT16_MAX },
		{"0x10000", UINT16_MAX + 1 },
		{"0x7FFFFFFF", INT32_MAX },
		{"0x80000000", INT32_MAX + 1U },
		{"0xFFFFFFFF", UINT32_MAX },
		{"0x100000000", UINT32_MAX + 1ULL },
		{"0x7FFFFFFFFFFFFFFF", INT64_MAX },
		{"0x8000000000000000", INT64_MAX + 1ULL},
		{"0xFFFFFFFFFFFFFFFF", UINT64_MAX },
		/* hexadecimal (with leading zeroes) */
		{"0x00", 0 },
		{"0x7F", INT8_MAX },
		{"0x80", INT8_MAX + 1 },
		{"0xFF", UINT8_MAX },
		{"0x0100", UINT8_MAX + 1 },
		{"0x7FFF", INT16_MAX },
		{"0x8000", INT16_MAX + 1 },
		{"0xFFFF", UINT16_MAX },
		{"0x00010000", UINT16_MAX + 1 },
		{"0x7FFFFFFF", INT32_MAX },
		{"0x80000000", INT32_MAX + 1U },
		{"0xFFFFFFFF", UINT32_MAX },
		{"0x0000000100000000", UINT32_MAX + 1ULL },
		{"0x7FFFFFFFFFFFFFFF", INT64_MAX },
		{"0x8000000000000000", INT64_MAX + 1ULL},
		{"0xFFFFFFFFFFFFFFFF", UINT64_MAX },
		/* check all characters */
		{"0x1234567890ABCDEF", 0x1234567890ABCDEFULL },
		{"0x1234567890abcdef", 0x1234567890ABCDEFULL },
		/* binary (no leading zeroes) */
		{"0b0", 0 },
		{"0b1111111", INT8_MAX },
		{"0b10000000", INT8_MAX + 1 },
		{"0b11111111", UINT8_MAX },
		{"0b100000000", UINT8_MAX + 1 },
		{"0b111111111111111", INT16_MAX },
		{"0b1000000000000000", INT16_MAX + 1 },
		{"0b1111111111111111", UINT16_MAX },
		{"0b10000000000000000", UINT16_MAX + 1 },
		{"0b1111111111111111111111111111111", INT32_MAX },
		{"0b10000000000000000000000000000000", INT32_MAX + 1U },
		{"0b11111111111111111111111111111111", UINT32_MAX },
		{"0b100000000000000000000000000000000", UINT32_MAX + 1ULL },
		{"0b111111111111111111111111111111111111111111111111111111111111111",
				INT64_MAX },
		{"0b1000000000000000000000000000000000000000000000000000000000000000",
				INT64_MAX + 1ULL},
		{"0b1111111111111111111111111111111111111111111111111111111111111111",
				UINT64_MAX },
		/* binary (with leading zeroes) */
		{"0b01111111", INT8_MAX },
		{"0b0000000100000000", UINT8_MAX + 1 },
		{"0b0111111111111111", INT16_MAX },
		{"0b00000000000000010000000000000000", UINT16_MAX + 1 },
		{"0b01111111111111111111111111111111", INT32_MAX },
		{"0b0000000000000000000000000000000100000000000000000000000000000000",
				UINT32_MAX + 1ULL },
		{"0b0111111111111111111111111111111111111111111111111111111111111111",
				INT64_MAX },
		/* octal */
		{"00", 0 },
		{"0177", INT8_MAX },
		{"0200", INT8_MAX + 1 },
		{"0377", UINT8_MAX },
		{"0400", UINT8_MAX + 1 },
		{"077777", INT16_MAX },
		{"0100000", INT16_MAX + 1 },
		{"0177777", UINT16_MAX },
		{"0200000", UINT16_MAX + 1 },
		{"017777777777", INT32_MAX },
		{"020000000000", INT32_MAX + 1U },
		{"037777777777", UINT32_MAX },
		{"040000000000", UINT32_MAX + 1ULL },
		{"0777777777777777777777", INT64_MAX },
		{"01000000000000000000000", INT64_MAX + 1ULL},
		{"01777777777777777777777", UINT64_MAX },
		/* check all numbers */
		{"012345670", 012345670 },
		{"076543210", 076543210 },
};

const struct num_signed_str num_valid_negative_strs[] = {
		/* deciman negative */
		{"-128", INT8_MIN },
		{"-129", INT8_MIN - 1 },
		{"-32768", INT16_MIN },
		{"-32769", INT16_MIN - 1 },
		{"-2147483648", INT32_MIN },
		{"-2147483649", INT32_MIN - 1LL },
		{"-9223372036854775808", INT64_MIN },
};

const struct num_unsigned_str num_garbage_positive_strs[] = {
		/* valid strings with garbage on the end, should still be valid */
		/* decimal */
		{"9223372036854775807\0garbage", INT64_MAX },
		{"9223372036854775807\tgarbage", INT64_MAX },
		{"9223372036854775807\rgarbage", INT64_MAX },
		{"9223372036854775807\ngarbage", INT64_MAX },
		{"9223372036854775807#garbage", INT64_MAX },
		{"9223372036854775807 garbage", INT64_MAX },
		/* hex */
		{"0x7FFFFFFFFFFFFFFF\0garbage", INT64_MAX },
		{"0x7FFFFFFFFFFFFFFF\tgarbage", INT64_MAX },
		{"0x7FFFFFFFFFFFFFFF\rgarbage", INT64_MAX },
		{"0x7FFFFFFFFFFFFFFF\ngarbage", INT64_MAX },
		{"0x7FFFFFFFFFFFFFFF#garbage", INT64_MAX },
		{"0x7FFFFFFFFFFFFFFF garbage", INT64_MAX },
		/* binary */
		{"0b1111111111111111111111111111111\0garbage", INT32_MAX },
		{"0b1111111111111111111111111111111\rgarbage", INT32_MAX },
		{"0b1111111111111111111111111111111\tgarbage", INT32_MAX },
		{"0b1111111111111111111111111111111\ngarbage", INT32_MAX },
		{"0b1111111111111111111111111111111#garbage", INT32_MAX },
		{"0b1111111111111111111111111111111 garbage", INT32_MAX },
		/* octal */
		{"01777777777777777777777\0garbage", UINT64_MAX },
		{"01777777777777777777777\rgarbage", UINT64_MAX },
		{"01777777777777777777777\tgarbage", UINT64_MAX },
		{"01777777777777777777777\ngarbage", UINT64_MAX },
		{"01777777777777777777777#garbage", UINT64_MAX },
		{"01777777777777777777777 garbage", UINT64_MAX },
};

const struct num_signed_str num_garbage_negative_strs[] = {
		/* valid strings with garbage on the end, should still be valid */
		{"-9223372036854775808\0garbage", INT64_MIN },
		{"-9223372036854775808\rgarbage", INT64_MIN },
		{"-9223372036854775808\tgarbage", INT64_MIN },
		{"-9223372036854775808\ngarbage", INT64_MIN },
		{"-9223372036854775808#garbage", INT64_MIN },
		{"-9223372036854775808 garbage", INT64_MIN },
};

const char * num_invalid_strs[] = {
		"18446744073709551616", /* out of range unsigned */
		"-9223372036854775809", /* out of range negative signed */
		"0x10000000000000000", /* out of range hex */
		/* out of range binary */
		"0b10000000000000000000000000000000000000000000000000000000000000000",
		"020000000000000000000000", /* out of range octal */
		/* wrong chars */
		"0123456239",
		"0x1234580AGE",
		"0b0111010101g001",
		"0b01110101017001",
		/* false negative numbers */
		"-12345F623",
		"-0x1234580A",
		"-0b0111010101",
		/* too long (128+ chars) */
		"0b1111000011110000111100001111000011110000111100001111000011110000"
		  "1111000011110000111100001111000011110000111100001111000011110000",
		"1E3",
		"0A",
		"-B",
		"+4",
		"1.23G",
		"",
		" ",
		"#",
		"\r",
		"\t",
		"\n",
		"\0",
};

#define NUM_POSITIVE_STRS_SIZE \
	(sizeof(num_valid_positive_strs) / sizeof(num_valid_positive_strs[0]))
#define NUM_NEGATIVE_STRS_SIZE \
	(sizeof(num_valid_negative_strs) / sizeof(num_valid_negative_strs[0]))
#define NUM_POSITIVE_GARBAGE_STRS_SIZE \
	(sizeof(num_garbage_positive_strs) / sizeof(num_garbage_positive_strs[0]))
#define NUM_NEGATIVE_GARBAGE_STRS_SIZE \
	(sizeof(num_garbage_negative_strs) / sizeof(num_garbage_negative_strs[0]))
#define NUM_INVALID_STRS_SIZE \
	(sizeof(num_invalid_strs) / sizeof(num_invalid_strs[0]))



static int
can_parse_unsigned(uint64_t expected_result, enum cmdline_numtype type)
{
	switch (type) {
	case UINT8:
		if (expected_result > UINT8_MAX)
			return 0;
		break;
	case UINT16:
		if (expected_result > UINT16_MAX)
			return 0;
		break;
	case UINT32:
		if (expected_result > UINT32_MAX)
			return 0;
		break;
	case INT8:
		if (expected_result > INT8_MAX)
			return 0;
		break;
	case INT16:
		if (expected_result > INT16_MAX)
			return 0;
		break;
	case INT32:
		if (expected_result > INT32_MAX)
			return 0;
		break;
	case INT64:
		if (expected_result > INT64_MAX)
			return 0;
		break;
	default:
		return 1;
	}
	return 1;
}

static int
can_parse_signed(int64_t expected_result, enum cmdline_numtype type)
{
	switch (type) {
	case UINT8:
		if (expected_result > UINT8_MAX || expected_result < 0)
			return 0;
		break;
	case UINT16:
		if (expected_result > UINT16_MAX || expected_result < 0)
			return 0;
		break;
	case UINT32:
		if (expected_result > UINT32_MAX || expected_result < 0)
			return 0;
		break;
	case UINT64:
		if (expected_result < 0)
			return 0;
		break;
	case INT8:
		if (expected_result > INT8_MAX || expected_result < INT8_MIN)
			return 0;
		break;
	case INT16:
		if (expected_result > INT16_MAX || expected_result < INT16_MIN)
			return 0;
		break;
	case INT32:
		if (expected_result > INT32_MAX || expected_result < INT32_MIN)
			return 0;
		break;
	default:
		return 1;
	}
	return 1;
}

/* test invalid parameters */
int
test_parse_num_invalid_param(void)
{
	char buf[CMDLINE_TEST_BUFSIZE];
	uint32_t result;
	cmdline_parse_token_num_t token;
	int ret = 0;

	/* set up a token */
	token.num_data.type = UINT32;

	/* copy string to buffer */
	snprintf(buf, sizeof(buf), "%s",
			num_valid_positive_strs[0].str);

	/* try all null */
	ret = cmdline_parse_num(NULL, NULL, NULL, 0);
	if (ret != -1) {
		printf("Error: parser accepted null parameters!\n");
		return -1;
	}

	/* try null token */
	ret = cmdline_parse_num(NULL, buf, (void*)&result, sizeof(result));
	if (ret != -1) {
		printf("Error: parser accepted null token!\n");
		return -1;
	}

	/* try null buf */
	ret = cmdline_parse_num((cmdline_parse_token_hdr_t*)&token, NULL,
		(void*)&result, sizeof(result));
	if (ret != -1) {
		printf("Error: parser accepted null string!\n");
		return -1;
	}

	/* try null result */
	ret = cmdline_parse_num((cmdline_parse_token_hdr_t*)&token, buf,
		NULL, 0);
	if (ret == -1) {
		printf("Error: parser rejected null result!\n");
		return -1;
	}

	/* test help function */
	memset(&buf, 0, sizeof(buf));

	/* try all null */
	ret = cmdline_get_help_num(NULL, NULL, 0);
	if (ret != -1) {
		printf("Error: help function accepted null parameters!\n");
		return -1;
	}

	/* try null token */
	ret = cmdline_get_help_num(NULL, buf, sizeof(buf));
	if (ret != -1) {
		printf("Error: help function accepted null token!\n");
		return -1;
	}

	/* coverage! */
	ret = cmdline_get_help_num((cmdline_parse_token_hdr_t*)&token, buf, sizeof(buf));
	if (ret < 0) {
		printf("Error: help function failed with valid parameters!\n");
		return -1;
	}

	return 0;
}
/* test valid parameters but invalid data */
int
test_parse_num_invalid_data(void)
{
	enum cmdline_numtype type;
	int ret = 0;
	unsigned i;
	char buf[CMDLINE_TEST_BUFSIZE];
	uint64_t result; /* pick largest buffer */
	cmdline_parse_token_num_t token;

	/* cycle through all possible parsed types */
	for (type = UINT8; type <= INT64; type++) {
		token.num_data.type = type;

		/* test full strings */
		for (i = 0; i < NUM_INVALID_STRS_SIZE; i++) {

			memset(&result, 0, sizeof(uint64_t));
			memset(&buf, 0, sizeof(buf));

			ret = cmdline_parse_num((cmdline_parse_token_hdr_t*)&token,
				num_invalid_strs[i], (void*)&result, sizeof(result));
			if (ret != -1) {
				/* get some info about what we are trying to parse */
				cmdline_get_help_num((cmdline_parse_token_hdr_t*)&token,
						buf, sizeof(buf));

				printf("Error: parsing %s as %s succeeded!\n",
						num_invalid_strs[i], buf);
				return -1;
			}
		}
	}
	return 0;
}

/* test valid parameters and data */
int
test_parse_num_valid(void)
{
	int ret = 0;
	enum cmdline_numtype type;
	unsigned i;
	char buf[CMDLINE_TEST_BUFSIZE];
	uint64_t result;
	cmdline_parse_token_num_t token;

	/** valid strings **/

	/* cycle through all possible parsed types */
	for (type = UINT8; type <= INT64; type++) {
		token.num_data.type = type;

		/* test positive strings */
		for (i = 0; i < NUM_POSITIVE_STRS_SIZE; i++) {
			result = 0;
			memset(&buf, 0, sizeof(buf));

			cmdline_get_help_num((cmdline_parse_token_hdr_t*)&token,
					buf, sizeof(buf));

			ret = cmdline_parse_num((cmdline_parse_token_hdr_t*) &token,
				num_valid_positive_strs[i].str,
				(void*)&result, sizeof(result));

			/* if it should have passed but didn't, or if it should have failed but didn't */
			if ((ret < 0) == (can_parse_unsigned(num_valid_positive_strs[i].result, type) > 0)) {
				printf("Error: parser behaves unexpectedly when parsing %s as %s!\n",
						num_valid_positive_strs[i].str, buf);
				return -1;
			}
			/* check if result matches what it should have matched
			 * since unsigned numbers don't care about number of bits, we can just convert
			 * everything to uint64_t without any worries. */
			if (ret > 0 && num_valid_positive_strs[i].result != result) {
				printf("Error: parsing %s as %s failed: result mismatch!\n",
						num_valid_positive_strs[i].str, buf);
				return -1;
			}
		}

		/* test negative strings */
		for (i = 0; i < NUM_NEGATIVE_STRS_SIZE; i++) {
			result = 0;
			memset(&buf, 0, sizeof(buf));

			cmdline_get_help_num((cmdline_parse_token_hdr_t*)&token,
					buf, sizeof(buf));

			ret = cmdline_parse_num((cmdline_parse_token_hdr_t*) &token,
				num_valid_negative_strs[i].str,
				(void*)&result, sizeof(result));

			/* if it should have passed but didn't, or if it should have failed but didn't */
			if ((ret < 0) == (can_parse_signed(num_valid_negative_strs[i].result, type) > 0)) {
				printf("Error: parser behaves unexpectedly when parsing %s as %s!\n",
						num_valid_negative_strs[i].str, buf);
				return -1;
			}
			/* check if result matches what it should have matched
			 * the result is signed in this case, so we have to account for that */
			if (ret > 0) {
				/* detect negative */
				switch (type) {
				case INT8:
					result = (int8_t) result;
					break;
				case INT16:
					result = (int16_t) result;
					break;
				case INT32:
					result = (int32_t) result;
					break;
				default:
					break;
				}
				if (num_valid_negative_strs[i].result == (int64_t) result)
					continue;
				printf("Error: parsing %s as %s failed: result mismatch!\n",
						num_valid_negative_strs[i].str, buf);
				return -1;
			}
		}
	}

	/** garbage strings **/

	/* cycle through all possible parsed types */
	for (type = UINT8; type <= INT64; type++) {
		token.num_data.type = type;

		/* test positive garbage strings */
		for (i = 0; i < NUM_POSITIVE_GARBAGE_STRS_SIZE; i++) {
			result = 0;
			memset(&buf, 0, sizeof(buf));

			cmdline_get_help_num((cmdline_parse_token_hdr_t*)&token,
					buf, sizeof(buf));

			ret = cmdline_parse_num((cmdline_parse_token_hdr_t*) &token,
				num_garbage_positive_strs[i].str,
				(void*)&result, sizeof(result));

			/* if it should have passed but didn't, or if it should have failed but didn't */
			if ((ret < 0) == (can_parse_unsigned(num_garbage_positive_strs[i].result, type) > 0)) {
				printf("Error: parser behaves unexpectedly when parsing %s as %s!\n",
						num_garbage_positive_strs[i].str, buf);
				return -1;
			}
			/* check if result matches what it should have matched
			 * since unsigned numbers don't care about number of bits, we can just convert
			 * everything to uint64_t without any worries. */
			if (ret > 0 && num_garbage_positive_strs[i].result != result) {
				printf("Error: parsing %s as %s failed: result mismatch!\n",
						num_garbage_positive_strs[i].str, buf);
				return -1;
			}
		}

		/* test negative strings */
		for (i = 0; i < NUM_NEGATIVE_GARBAGE_STRS_SIZE; i++) {
			result = 0;
			memset(&buf, 0, sizeof(buf));

			cmdline_get_help_num((cmdline_parse_token_hdr_t*)&token,
					buf, sizeof(buf));

			ret = cmdline_parse_num((cmdline_parse_token_hdr_t*) &token,
				num_garbage_negative_strs[i].str,
				(void*)&result, sizeof(result));

			/* if it should have passed but didn't, or if it should have failed but didn't */
			if ((ret < 0) == (can_parse_signed(num_garbage_negative_strs[i].result, type) > 0)) {
				printf("Error: parser behaves unexpectedly when parsing %s as %s!\n",
						num_garbage_negative_strs[i].str, buf);
				return -1;
			}
			/* check if result matches what it should have matched
			 * the result is signed in this case, so we have to account for that */
			if (ret > 0) {
				/* detect negative */
				switch (type) {
				case INT8:
					if (result & (INT8_MAX + 1))
						result |= 0xFFFFFFFFFFFFFF00ULL;
					break;
				case INT16:
					if (result & (INT16_MAX + 1))
						result |= 0xFFFFFFFFFFFF0000ULL;
					break;
				case INT32:
					if (result & (INT32_MAX + 1ULL))
						result |= 0xFFFFFFFF00000000ULL;
					break;
				default:
					break;
				}
				if (num_garbage_negative_strs[i].result == (int64_t) result)
					continue;
				printf("Error: parsing %s as %s failed: result mismatch!\n",
						num_garbage_negative_strs[i].str, buf);
				return -1;
			}
		}
	}

	memset(&buf, 0, sizeof(buf));

	/* coverage! */
	cmdline_get_help_num((cmdline_parse_token_hdr_t*)&token,
			buf, sizeof(buf));

	return 0;
}
