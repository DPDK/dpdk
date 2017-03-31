/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Wind River Systems Inc. All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <stdio.h>
#include <string.h>
#include <stdint.h>
#include <sys/queue.h>

#include <rte_cfgfile.h>

#include "test.h"
#include "resource.h"


#define CFG_FILES_ETC "test_cfgfiles/etc"

REGISTER_LINKED_RESOURCE(test_cfgfiles);

static int
test_cfgfile_setup(void)
{
	const struct resource *r;
	int ret;

	r = resource_find("test_cfgfiles");
	TEST_ASSERT_NOT_NULL(r, "missing resource test_cfgfiles");

	ret = resource_untar(r);
	TEST_ASSERT_SUCCESS(ret, "failed to untar %s", r->name);

	return 0;
}

static int
test_cfgfile_cleanup(void)
{
	const struct resource *r;
	int ret;

	r = resource_find("test_cfgfiles");
	TEST_ASSERT_NOT_NULL(r, "missing resource test_cfgfiles");

	ret = resource_rm_by_tar(r);
	TEST_ASSERT_SUCCESS(ret, "Failed to delete resource %s", r->name);

	return 0;
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
	int ret;

	cfgfile = rte_cfgfile_load(CFG_FILES_ETC "/sample1.ini", 0);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to load config file");

	ret = _test_cfgfile_sample(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to validate sample file: %d", ret);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	return 0;
}

static int
test_cfgfile_sample2(void)
{
	struct rte_cfgfile_parameters params;
	struct rte_cfgfile *cfgfile;
	int ret;

	/* override comment character */
	memset(&params, 0, sizeof(params));
	params.comment_character = '#';

	cfgfile = rte_cfgfile_load_with_params(CFG_FILES_ETC "/sample2.ini", 0,
					       &params);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to parse sample2.ini");

	ret = _test_cfgfile_sample(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to validate sample file: %d", ret);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	return 0;
}

static int
test_cfgfile_invalid_section_header(void)
{
	struct rte_cfgfile *cfgfile;

	cfgfile = rte_cfgfile_load(CFG_FILES_ETC "/invalid_section.ini", 0);
	TEST_ASSERT_NULL(cfgfile, "Expected failured did not occur");

	return 0;
}

static int
test_cfgfile_invalid_comment(void)
{
	struct rte_cfgfile_parameters params;
	struct rte_cfgfile *cfgfile;

	/* override comment character with an invalid one */
	memset(&params, 0, sizeof(params));
	params.comment_character = '$';

	cfgfile = rte_cfgfile_load_with_params(CFG_FILES_ETC "/sample2.ini", 0,
					       &params);
	TEST_ASSERT_NULL(cfgfile, "Expected failured did not occur");

	return 0;
}

static int
test_cfgfile_invalid_key_value_pair(void)
{
	struct rte_cfgfile *cfgfile;

	cfgfile = rte_cfgfile_load(CFG_FILES_ETC "/empty_key_value.ini", 0);
	TEST_ASSERT_NULL(cfgfile, "Expected failured did not occur");

	return 0;
}

static int
test_cfgfile_empty_key_value_pair(void)
{
	struct rte_cfgfile *cfgfile;
	const char *value;
	int ret;

	cfgfile = rte_cfgfile_load(CFG_FILES_ETC "/empty_key_value.ini",
				   CFG_FLAG_EMPTY_VALUES);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to parse empty_key_value.ini");

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

	return 0;
}

static int
test_cfgfile_missing_section(void)
{
	struct rte_cfgfile *cfgfile;

	cfgfile = rte_cfgfile_load(CFG_FILES_ETC "/missing_section.ini", 0);
	TEST_ASSERT_NULL(cfgfile, "Expected failured did not occur");

	return 0;
}

static int
test_cfgfile_global_properties(void)
{
	struct rte_cfgfile *cfgfile;
	const char *value;
	int ret;

	cfgfile = rte_cfgfile_load(CFG_FILES_ETC "/missing_section.ini",
				   CFG_FLAG_GLOBAL_SECTION);
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

	return 0;
}

static int
test_cfgfile_empty_file(void)
{
	struct rte_cfgfile *cfgfile;
	int ret;

	cfgfile = rte_cfgfile_load(CFG_FILES_ETC "/empty.ini", 0);
	TEST_ASSERT_NOT_NULL(cfgfile, "Failed to load config file");

	ret = rte_cfgfile_num_sections(cfgfile, NULL, 0);
	TEST_ASSERT(ret == 0, "Unexpected number of sections: %d", ret);

	ret = rte_cfgfile_close(cfgfile);
	TEST_ASSERT_SUCCESS(ret, "Failed to close cfgfile");

	return 0;
}

static int
test_cfgfile(void)
{
	if (test_cfgfile_setup())
		return -1;

	if (test_cfgfile_sample1())
		return -1;

	if (test_cfgfile_sample2())
		return -1;

	if (test_cfgfile_invalid_section_header())
		return -1;

	if (test_cfgfile_invalid_comment())
		return -1;

	if (test_cfgfile_invalid_key_value_pair())
		return -1;

	if (test_cfgfile_empty_key_value_pair())
		return -1;

	if (test_cfgfile_missing_section())
		return -1;

	if (test_cfgfile_global_properties())
		return -1;

	if (test_cfgfile_empty_file())
		return -1;

	if (test_cfgfile_cleanup())
		return -1;

	return 0;
}

REGISTER_TEST_COMMAND(cfgfile_autotest, test_cfgfile);
