/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include "test.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

/* eal_filesystem.h is not a public header file, so use relative path */
#include "../../lib/librte_eal/common/eal_filesystem.h"

static int
test_parse_sysfs_value(void)
{
	char filename[PATH_MAX] = "";
	char proc_path[PATH_MAX];
	char file_template[] = "/tmp/eal_test_XXXXXX";
	int tmp_file_handle = -1;
	FILE *fd = NULL;
	unsigned valid_number;
	unsigned long retval = 0;

#ifdef RTE_EXEC_ENV_BSDAPP
	/* BSD doesn't have /proc/pid/fd */
	return 0;
#endif

	printf("Testing function eal_parse_sysfs_value()\n");

	/* get a temporary filename to use for all tests - create temp file handle and then
	 * use /proc to get the actual file that we can open */
	tmp_file_handle = mkstemp(file_template);
	if (tmp_file_handle == -1) {
		perror("mkstemp() failure");
		goto error;
	}
	snprintf(proc_path, sizeof(proc_path), "/proc/self/fd/%d", tmp_file_handle);
	if (readlink(proc_path, filename, sizeof(filename)) < 0) {
		perror("readlink() failure");
		goto error;
	}
	printf("Temporary file is: %s\n", filename);

	/* test we get an error value if we use file before it's created */
	printf("Test reading a missing file ...\n");
	if (eal_parse_sysfs_value("/dev/not-quite-null", &retval) == 0) {
		printf("Error with eal_parse_sysfs_value() - returned success on reading empty file\n");
		goto error;
	}
	printf("Confirmed return error when reading empty file\n");

	/* test reading a valid number value with "\n" on the end */
	printf("Test reading valid values ...\n");
	valid_number = 15;
	fd = fopen(filename,"w");
	if (fd == NULL) {
		printf("line %d, Error opening %s: %s\n", __LINE__, filename, strerror(errno));
		goto error;
	}
	fprintf(fd,"%u\n", valid_number);
	fclose(fd);
	fd = NULL;
	if (eal_parse_sysfs_value(filename, &retval) < 0) {
		printf("eal_parse_sysfs_value() returned error - test failed\n");
		goto error;
	}
	if (retval != valid_number) {
		printf("Invalid value read by eal_parse_sysfs_value() - test failed\n");
		goto error;
	}
	printf("Read '%u\\n' ok\n", valid_number);

	/* test reading a valid hex number value with "\n" on the end */
	valid_number = 25;
	fd = fopen(filename,"w");
	if (fd == NULL) {
		printf("line %d, Error opening %s: %s\n", __LINE__, filename, strerror(errno));
		goto error;
	}
	fprintf(fd,"0x%x\n", valid_number);
	fclose(fd);
	fd = NULL;
	if (eal_parse_sysfs_value(filename, &retval) < 0) {
		printf("eal_parse_sysfs_value() returned error - test failed\n");
		goto error;
	}
	if (retval != valid_number) {
		printf("Invalid value read by eal_parse_sysfs_value() - test failed\n");
		goto error;
	}
	printf("Read '0x%x\\n' ok\n", valid_number);

	printf("Test reading invalid values ...\n");

	/* test reading an empty file - expect failure!*/
	fd = fopen(filename,"w");
	if (fd == NULL) {
		printf("line %d, Error opening %s: %s\n", __LINE__, filename, strerror(errno));
		goto error;
	}
	fclose(fd);
	fd = NULL;
	if (eal_parse_sysfs_value(filename, &retval) == 0) {
		printf("eal_parse_sysfs_value() read invalid value  - test failed\n");
		goto error;
	}

	/* test reading a valid number value *without* "\n" on the end - expect failure!*/
	valid_number = 3;
	fd = fopen(filename,"w");
	if (fd == NULL) {
		printf("line %d, Error opening %s: %s\n", __LINE__, filename, strerror(errno));
		goto error;
	}
	fprintf(fd,"%u", valid_number);
	fclose(fd);
	fd = NULL;
	if (eal_parse_sysfs_value(filename, &retval) == 0) {
		printf("eal_parse_sysfs_value() read invalid value  - test failed\n");
		goto error;
	}

	/* test reading a valid number value followed by string - expect failure!*/
	valid_number = 3;
	fd = fopen(filename,"w");
	if (fd == NULL) {
		printf("line %d, Error opening %s: %s\n", __LINE__, filename, strerror(errno));
		goto error;
	}
	fprintf(fd,"%uJ\n", valid_number);
	fclose(fd);
	fd = NULL;
	if (eal_parse_sysfs_value(filename, &retval) == 0) {
		printf("eal_parse_sysfs_value() read invalid value  - test failed\n");
		goto error;
	}

	/* test reading a non-numeric value - expect failure!*/
	fd = fopen(filename,"w");
	if (fd == NULL) {
		printf("line %d, Error opening %s: %s\n", __LINE__, filename, strerror(errno));
		goto error;
	}
	fprintf(fd,"error\n");
	fclose(fd);
	fd = NULL;
	if (eal_parse_sysfs_value(filename, &retval) == 0) {
		printf("eal_parse_sysfs_value() read invalid value  - test failed\n");
		goto error;
	}

	close(tmp_file_handle);
	unlink(filename);
	printf("eal_parse_sysfs_value() - OK\n");
	return 0;

error:
	if (fd)
		fclose(fd);
	if (tmp_file_handle > 0)
		close(tmp_file_handle);
	if (filename[0] != '\0')
		unlink(filename);
	return -1;
}

static int
test_eal_fs(void)
{
	if (test_parse_sysfs_value() < 0)
		return -1;
	return 0;
}

REGISTER_TEST_COMMAND(eal_fs_autotest, test_eal_fs);
