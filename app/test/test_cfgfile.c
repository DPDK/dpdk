/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Wind River Systems, Inc.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <unistd.h>

#ifdef RTE_EXEC_ENV_WINDOWS
#include <io.h>
#endif

#include <rte_cfgfile.h>

#include "test.h"

#include "test_cfgfiles.h"

static int
make_tmp_file(char *filename, const char *prefix, const char *data)
{
	size_t len = strlen(data);
	size_t count;
	FILE *f;

#ifdef RTE_EXEC_ENV_WINDOWS
	char tempDirName[MAX_PATH - 14];

	if (GetTempPathA(sizeof(tempDirName), tempDirName) == 0)
		return -1;

	if (GetTempFileNameA(tempDirName, prefix, 0, filename) == 0)
		return -1;

	f = fopen(filename, "wt");
#else
	snprintf(filename, PATH_MAX, "/tmp/%s_XXXXXXX", prefix);

	int fd = mkstemp(filename);
	if (fd < 0)
		return -1;

	f = fdopen(fd, "w");
#endif
	if (f == NULL)
		return -1;

	count = fwrite(data, sizeof(char), len, f);
	fclose(f);

	return (count == len) ? 0 : -1;
}


static int
_test_cfgfile_sample(struct rte_cfgfile *cfgfile)
{
	const char *value;
	int ret;

	ret = rte_cfgfile_num_sections(cfgfile, NULL, 0);
	TEST_ASSERT(ret == 2, "Unexpected number of sections: %d", ret);

	ret = rte_cfgfile_has_section(cfgfile, "section1");
	TEST_ASSERT(ret, "section1 section missing");

	ret = rte_cfgfile_section_num_entries(cfgfile, "section1");
	TEST_ASSERT(ret == 1, "section1 unexpected number of entries: %d", ret);

	value = rte_cfgfile_get_entry(cfgfile, "section1", "key1");
	TEST_ASSERT(strcmp("value1", value) == 0,
		    "key1 unexpected value: %s", value);

	ret = rte_cfgfile_has_section(cfgfile, "section2");
	TEST_ASSERT(ret, "section2 section missing");

	ret = rte_cfgfile_section_num_entries(cfgfile, "section2");
	TEST_ASSERT(ret == 2, "section2 unexpected number of entries: %d", ret);

	value = rte_cfgfile_get_entry(cfgfile, "section2", "key2");
	TEST_ASSERT(strcmp("value2", value) == 0,
		    "key2 unexpected value: %s", value);

	value = rte_cfgfile_get_entry(cfgfile, "section2", "key3");
	TEST_ASSERT(strcmp("value3", value) == 0,
		    "key3 unexpected value: %s", value);

	return 0;
}

static int
test_cfgfile_sample1(void)
{
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "sample1", sample1_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, 0);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to load config file");

	ret = _test_cfgfile_sample(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to validate sample file: %d", ret);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_sample2(void)
{
	struct rte_cfgfile_parameters params;
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "sample2", sample2_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	/* override comment character */
	memset(&params, 0, sizeof(params));
	params.comment_character = '#';

	cfgfile = rte_cfgfile_load_with_params(filename, 0, &params);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to parse sample2");

	ret = _test_cfgfile_sample(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to validate sample file: %d", ret);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_realloc_sections(void)
{
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;
	const char *value;

	ret = make_tmp_file(filename, "realloc", realloc_sections_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, 0);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to load config file");

	ret = rte_cfgfile_num_sections(cfgfile, NULL, 0);
	TEST_ASSERT(ret == 9, "Unexpected number of sections: %d", ret);

	ret = rte_cfgfile_has_section(cfgfile, "section9");
	TEST_ASSERT(ret, "section9 missing");

	ret = rte_cfgfile_section_num_entries(cfgfile, "section3");
	TEST_ASSERT(ret == 21,
			"section3 unexpected number of entries: %d", ret);

	ret = rte_cfgfile_section_num_entries(cfgfile, "section9");
	TEST_ASSERT(ret == 8, "section9 unexpected number of entries: %d", ret);

	value = rte_cfgfile_get_entry(cfgfile, "section9", "key8");
	TEST_ASSERT(strcmp("value8_section9", value) == 0,
		    "key unexpected value: %s", value);

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	ret = make_tmp_file(filename, "save", "");
	TEST_ASSERT(ret == 0, "Failed to make empty tmp filename for save");

	ret = rte_cfgfile_save(cfgfile, filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to save to %s", filename);

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	return 0;
}

static int
test_cfgfile_invalid_section_header(void)
{
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "invalid", invalid_section_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, 0);
	TEST_ASSERT_NULL(cfgfile, "Expected failure did not occur");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_invalid_comment(void)
{
	struct rte_cfgfile_parameters params;
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;

	/* override comment character with an invalid one */
	memset(&params, 0, sizeof(params));
	params.comment_character = '$';

	ret = make_tmp_file(filename, "sample2", sample2_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load_with_params(filename, 0, &params);
	TEST_ASSERT_NULL(cfgfile, "Expected failure did not occur");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_invalid_key_value_pair(void)
{
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "empty_key", empty_key_value_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, 0);
	TEST_ASSERT_NULL(cfgfile, "Expected failure did not occur");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_empty_key_value_pair(void)
{
	struct rte_cfgfile *cfgfile;
	const char *value;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "empty_key_value", empty_key_value_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, CFG_FLAG_EMPTY_VALUES);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to parse empty_key_value");

	ret = rte_cfgfile_num_sections(cfgfile, NULL, 0);
	TEST_ASSERT(ret == 1, "Unexpected number of sections: %d", ret);

	ret = rte_cfgfile_has_section(cfgfile, "section1");
	TEST_ASSERT(ret, "section1 missing");

	ret = rte_cfgfile_section_num_entries(cfgfile, "section1");
	TEST_ASSERT(ret == 1, "section1 unexpected number of entries: %d", ret);

	value = rte_cfgfile_get_entry(cfgfile, "section1", "key");
	TEST_ASSERT(strlen(value) == 0, "key unexpected value: %s", value);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_missing_section(void)
{
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "missing_section", missing_section_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, 0);
	TEST_ASSERT_NULL(cfgfile, "Expected failure did not occur");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_global_properties(void)
{
	struct rte_cfgfile *cfgfile;
	const char *value;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "missing_section", missing_section_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, CFG_FLAG_GLOBAL_SECTION);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to load config file");

	ret = rte_cfgfile_num_sections(cfgfile, NULL, 0);
	TEST_ASSERT(ret == 1, "Unexpected number of sections: %d", ret);

	ret = rte_cfgfile_has_section(cfgfile, "GLOBAL");
	TEST_ASSERT(ret, "global section missing");

	ret = rte_cfgfile_section_num_entries(cfgfile, "GLOBAL");
	TEST_ASSERT(ret == 1, "GLOBAL unexpected number of entries: %d", ret);

	value = rte_cfgfile_get_entry(cfgfile, "GLOBAL", "key");
	TEST_ASSERT(strcmp("value", value) == 0,
		    "key unexpected value: %s", value);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static int
test_cfgfile_empty_file(void)
{
	struct rte_cfgfile *cfgfile;
	char filename[PATH_MAX];
	int ret;

	ret = make_tmp_file(filename, "empty", empty_ini);
	TEST_ASSERT_SUCCESS(ret, "Failed to setup temp file");

	cfgfile = rte_cfgfile_load(filename, 0);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to load config file");

	ret = rte_cfgfile_num_sections(cfgfile, NULL, 0);
	TEST_ASSERT(ret == 0, "Unexpected number of sections: %d", ret);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	ret = remove(filename);
	TEST_ASSERT_SUCCESS(ret, "Failed to remove file");

	return 0;
}

static struct
unit_test_suite test_cfgfile_suite  = {
	.suite_name = "Test Cfgfile Unit Test Suite",
	.unit_test_cases = {
		TEST_CASE(test_cfgfile_sample1),
		TEST_CASE(test_cfgfile_sample2),
		TEST_CASE(test_cfgfile_realloc_sections),
		TEST_CASE(test_cfgfile_invalid_section_header),
		TEST_CASE(test_cfgfile_invalid_comment),
		TEST_CASE(test_cfgfile_invalid_key_value_pair),
		TEST_CASE(test_cfgfile_empty_key_value_pair),
		TEST_CASE(test_cfgfile_missing_section),
		TEST_CASE(test_cfgfile_global_properties),
		TEST_CASE(test_cfgfile_empty_file),

		TEST_CASES_END()
	}
};

static int
test_cfgfile(void)
{
	return unit_test_suite_runner(&test_cfgfile_suite);
}

REGISTER_FAST_TEST(cfgfile_autotest, NOHUGE_OK, ASAN_OK, test_cfgfile);
