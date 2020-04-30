/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright 2020 Intel Corporation
 */

#include <string.h>

#include "../../lib/librte_telemetry/telemetry_json.h"
#include "test.h"

static int
test_basic_array(void)
{
	const char *expected = "[\"meaning of life\",42]";
	char buf[1024];
	int used = 0;

	printf("%s: ", __func__);
	used = rte_tel_json_empty_array(buf, sizeof(buf), used);
	if (used != 2 || strcmp(buf, "[]"))
		return -1;

	used = rte_tel_json_add_array_string(buf, sizeof(buf), used,
		"meaning of life");
	used = rte_tel_json_add_array_int(buf, sizeof(buf), used, 42);

	printf("buf = '%s', expected = '%s'\n", buf, expected);
	if (used != (int)strlen(expected))
		return -1;
	return strncmp(expected, buf, sizeof(buf));
}

static int
test_basic_obj(void)
{
	const char *expected = "{\"weddings\":4,\"funerals\":1}";
	char buf[1024];
	int used = 0;

	used = rte_tel_json_add_obj_u64(buf, sizeof(buf), used,
		"weddings", 4);
	used = rte_tel_json_add_obj_u64(buf, sizeof(buf), used,
		"funerals", 1);

	printf("%s: buf = '%s', expected = '%s'\n", __func__, buf, expected);
	if (used != (int)strlen(expected))
		return -1;
	return strncmp(expected, buf, sizeof(buf));
}

static int
test_overflow_array(void)
{
	static const char * const strs[] = {"Arsenal", "Chelsea", "Liverpool",
			"Spurs"};
	const char *expected = "[\"Arsenal\",\"Chelsea\"]";
	char buf[25];
	int i, used = 0;

	for (i = 0; i < (int)RTE_DIM(strs); i++)
		used = rte_tel_json_add_array_string(buf, sizeof(buf), used,
				strs[i]);

	printf("%s: buf = '%s', expected = '%s'\n", __func__, buf, expected);
	if (buf[used - 1] != ']')
		return -1;
	if (used != (int)strlen(expected))
		return -1;
	return strncmp(expected, buf, sizeof(buf));
}

static int
test_overflow_obj(void)
{
	static const char * const names[] = {"Italy", "Wales", "Scotland",
			"Ireland", "England", "France"};
	const int vals[RTE_DIM(names)] = {20, 61, 10, 40, 55, 35};
	const char *expected = "{\"Italy\":20,\"Wales\":61}";
	char buf[25];
	int i, used = 0;

	for (i = 0; i < (int)RTE_DIM(names); i++)
		used = rte_tel_json_add_obj_u64(buf, sizeof(buf), used,
				names[i], vals[i]);

	printf("%s: buf = '%s', expected = '%s'\n", __func__, buf, expected);
	if (buf[used - 1] != '}')
		return -1;
	if (used != (int)strlen(expected))
		return -1;
	return strncmp(expected, buf, sizeof(buf));
}

static int
test_large_array_element(void)
{
	static const char str[] = "A really long string to overflow buffer";
	/* buffer should be unmodified so initial value and expected are same */
	const char *expected = "ABC";
	char buf[sizeof(str) - 5] = "ABC";
	int used = 0;

	used = rte_tel_json_add_array_string(buf, sizeof(buf), used, str);
	printf("%s: buf = '%s', expected = '%s'\n", __func__, buf, expected);

	return strlen(buf) != 0;
}

static int
test_large_obj_element(void)
{
	static const char str[] = "A really long string to overflow buffer";
	/* buffer should be unmodified so initial value and expected are same */
	const char *expected = "XYZ";
	char buf[sizeof(str) - 5] = "XYZ";
	int used = 0;

	used = rte_tel_json_add_obj_u64(buf, sizeof(buf), used, str, 0);
	printf("%s: buf = '%s', expected = '%s'\n", __func__, buf, expected);

	return strlen(buf) != 0;
}

static int
test_telemetry_json(void)
{
	if (test_basic_array() < 0 ||
			test_basic_obj() < 0 ||
			test_overflow_array() < 0 ||
			test_overflow_obj() < 0 ||
			test_large_array_element() < 0 ||
+			test_large_obj_element() < 0)
		return -1;
	return 0;
}

REGISTER_TEST_COMMAND(telemetry_json_autotest, test_telemetry_json);
