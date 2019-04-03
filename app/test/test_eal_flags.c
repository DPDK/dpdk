/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <stdio.h>

#include "test.h"

#include <string.h>
#include <stdarg.h>
#include <libgen.h>
#include <stdlib.h>
#include <errno.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/wait.h>
#include <sys/file.h>
#include <limits.h>
#include <fcntl.h>

#include <rte_per_lcore.h>
#include <rte_debug.h>
#include <rte_string_fns.h>

#include "process.h"

#define DEFAULT_MEM_SIZE "18"
#define mp_flag "--proc-type=secondary"
#define no_hpet "--no-hpet"
#define no_huge "--no-huge"
#define no_shconf "--no-shconf"
#define pci_whitelist "--pci-whitelist"
#define vdev "--vdev"
#define memtest "memtest"
#define memtest1 "memtest1"
#define memtest2 "memtest2"
#define SOCKET_MEM_STRLEN (RTE_MAX_NUMA_NODES * 20)
#define launch_proc(ARGV) process_dup(ARGV, \
		sizeof(ARGV)/(sizeof(ARGV[0])), __func__)

enum hugepage_action {
	HUGEPAGE_CHECK_EXISTS = 0,
	HUGEPAGE_CHECK_LOCKED,
	HUGEPAGE_DELETE,
	HUGEPAGE_INVALID
};

/* if string contains a hugepage path */
static int
get_hugepage_path(char * src, int src_len, char * dst, int dst_len)
{
#define NUM_TOKENS 4
	char *tokens[NUM_TOKENS];

	/* if we couldn't properly split the string */
	if (rte_strsplit(src, src_len, tokens, NUM_TOKENS, ' ') < NUM_TOKENS)
		return 0;

	if (strncmp(tokens[2], "hugetlbfs", sizeof("hugetlbfs")) == 0) {
		strlcpy(dst, tokens[1], dst_len);
		return 1;
	}
	return 0;
}

/*
 * Cycles through hugepage directories and looks for hugepage
 * files associated with a given prefix. Depending on value of
 * action, the hugepages are checked if they exist, checked if
 * they can be locked, or are simply deleted.
 *
 * Returns 1 if it finds at least one hugepage matching the action
 * Returns 0 if no matching hugepages were found
 * Returns -1 if it encounters an error
 */
static int
process_hugefiles(const char * prefix, enum hugepage_action action)
{
	FILE * hugedir_handle = NULL;
	DIR * hugepage_dir = NULL;
	struct dirent *dirent = NULL;

	char hugefile_prefix[PATH_MAX] = {0};
	char hugedir[PATH_MAX] = {0};
	char line[PATH_MAX] = {0};

	int fd, lck_result, result = 0;

	const int prefix_len = snprintf(hugefile_prefix,
			sizeof(hugefile_prefix), "%smap_", prefix);
	if (prefix_len <= 0 || prefix_len >= (int)sizeof(hugefile_prefix)
			|| prefix_len >= (int)sizeof(dirent->d_name)) {
		printf("Error creating hugefile filename prefix\n");
		return -1;
	}

	/* get hugetlbfs mountpoints from /proc/mounts */
	hugedir_handle = fopen("/proc/mounts", "r");

	if (hugedir_handle == NULL) {
		printf("Error parsing /proc/mounts!\n");
		return -1;
	}

	/* read and parse script output */
	while (fgets(line, sizeof(line), hugedir_handle) != NULL) {

		/* check if we have a hugepage filesystem path */
		if (!get_hugepage_path(line, sizeof(line), hugedir, sizeof(hugedir)))
			continue;

		/* check if directory exists */
		if ((hugepage_dir = opendir(hugedir)) == NULL) {
			fclose(hugedir_handle);
			printf("Error reading %s: %s\n", hugedir, strerror(errno));
			return -1;
		}

		while ((dirent = readdir(hugepage_dir)) != NULL) {
			if (memcmp(dirent->d_name, hugefile_prefix, prefix_len) != 0)
				continue;

			switch (action) {
			case HUGEPAGE_CHECK_EXISTS:
				{
					/* file exists, return */
					result = 1;
					goto end;
				}
				break;
			case HUGEPAGE_DELETE:
				{
					char file_path[PATH_MAX] = {0};

					snprintf(file_path, sizeof(file_path),
						"%s/%s", hugedir, dirent->d_name);

					/* remove file */
					if (remove(file_path) < 0) {
						printf("Error deleting %s - %s!\n",
								dirent->d_name, strerror(errno));
						closedir(hugepage_dir);
						result = -1;
						goto end;
					}
					result = 1;
				}
				break;
			case HUGEPAGE_CHECK_LOCKED:
				{
					/* try and lock the file */
					fd = openat(dirfd(hugepage_dir), dirent->d_name, O_RDONLY);

					/* this shouldn't happen */
					if (fd == -1) {
						printf("Error opening %s - %s!\n",
								dirent->d_name, strerror(errno));
						closedir(hugepage_dir);
						result = -1;
						goto end;
					}

					/* non-blocking lock */
					lck_result = flock(fd, LOCK_EX | LOCK_NB);

					/* if lock succeeds, there's something wrong */
					if (lck_result != -1) {
						result = 0;

						/* unlock the resulting lock */
						flock(fd, LOCK_UN);
						close(fd);
						closedir(hugepage_dir);
						goto end;
					}
					result = 1;
					close(fd);
				}
				break;
				/* shouldn't happen */
			default:
				goto end;
			} /* switch */

		} /* read hugepage directory */
		closedir(hugepage_dir);
	} /* read /proc/mounts */
end:
	fclose(hugedir_handle);
	return result;
}

#ifdef RTE_EXEC_ENV_LINUX
/*
 * count the number of "node*" files in /sys/devices/system/node/
 */
static int
get_number_of_sockets(void)
{
	struct dirent *dirent = NULL;
	const char * nodedir = "/sys/devices/system/node/";
	DIR * dir = NULL;
	int result = 0;

	/* check if directory exists */
	if ((dir = opendir(nodedir)) == NULL) {
		/* if errno==ENOENT this means we don't have NUMA support */
		if (errno == ENOENT) {
			printf("No NUMA nodes detected: assuming 1 available socket\n");
			return 1;
		}
		printf("Error opening %s: %s\n", nodedir, strerror(errno));
		return -1;
	}

	while ((dirent = readdir(dir)) != NULL)
		if (strncmp(dirent->d_name, "node", sizeof("node") - 1) == 0)
			result++;

	closedir(dir);
	return result;
}
#endif

/*
 * Test that the app doesn't run with invalid whitelist option.
 * Final tests ensures it does run with valid options as sanity check (one
 * test for with Domain+BDF, second for just with BDF)
 */
static int
test_whitelist_flag(void)
{
	unsigned i;
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char * prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	const char *wlinval[][11] = {
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1",
				pci_whitelist, "error", "", ""},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1",
				pci_whitelist, "0:0:0", "", ""},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1",
				pci_whitelist, "0:error:0.1", "", ""},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1",
				pci_whitelist, "0:0:0.1error", "", ""},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1",
				pci_whitelist, "error0:0:0.1", "", ""},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1",
				pci_whitelist, "0:0:0.1.2", "", ""},
	};
	/* Test with valid whitelist option */
	const char *wlval1[] = {prgname, prefix, mp_flag, "-n", "1", "-c", "1",
			pci_whitelist, "00FF:09:0B.3"};
	const char *wlval2[] = {prgname, prefix, mp_flag, "-n", "1", "-c", "1",
			pci_whitelist, "09:0B.3", pci_whitelist, "0a:0b.1"};
	const char *wlval3[] = {prgname, prefix, mp_flag, "-n", "1", "-c", "1",
			pci_whitelist, "09:0B.3,type=test",
			pci_whitelist, "08:00.1,type=normal",
	};

	for (i = 0; i < sizeof(wlinval) / sizeof(wlinval[0]); i++) {
		if (launch_proc(wlinval[i]) == 0) {
			printf("Error - process did run ok with invalid "
			    "whitelist parameter\n");
			return -1;
		}
	}
	if (launch_proc(wlval1) != 0 ) {
		printf("Error - process did not run ok with valid whitelist\n");
		return -1;
	}
	if (launch_proc(wlval2) != 0 ) {
		printf("Error - process did not run ok with valid whitelist value set\n");
		return -1;
	}
	if (launch_proc(wlval3) != 0 ) {
		printf("Error - process did not run ok with valid whitelist + args\n");
		return -1;
	}

	return 0;
}

/*
 * Test that the app doesn't run with invalid blacklist option.
 * Final test ensures it does run with valid options as sanity check
 */
static int
test_invalid_b_flag(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char * prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	const char *blinval[][9] = {
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-b", "error"},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-b", "0:0:0"},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-b", "0:error:0.1"},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-b", "0:0:0.1error"},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-b", "error0:0:0.1"},
		{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-b", "0:0:0.1.2"},
	};
	/* Test with valid blacklist option */
	const char *blval[] = {prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-b", "FF:09:0B.3"};

	int i;

	for (i = 0; i != sizeof (blinval) / sizeof (blinval[0]); i++) {
		if (launch_proc(blinval[i]) == 0) {
			printf("Error - process did run ok with invalid "
			    "blacklist parameter\n");
			return -1;
		}
	}
	if (launch_proc(blval) != 0) {
		printf("Error - process did not run ok with valid blacklist value\n");
		return -1;
	}
	return 0;
}

/*
 *  Test that the app doesn't run with invalid vdev option.
 *  Final test ensures it does run with valid options as sanity check
 */
#ifdef RTE_LIBRTE_PMD_RING
static int
test_invalid_vdev_flag(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point, and we also need to
	 * run another primary process here */
	const char * prefix = no_shconf;
#else
	const char * prefix = "--file-prefix=vdev";
#endif

	/* Test with invalid vdev option */
	const char *vdevinval[] = {prgname, prefix, no_huge, "-n", "1",
				"-c", "1", vdev, "eth_dummy"};

	/* Test with valid vdev option */
	const char *vdevval1[] = {prgname, prefix, no_huge, "-n", "1",
	"-c", "1", vdev, "net_ring0"};

	const char *vdevval2[] = {prgname, prefix, no_huge, "-n", "1",
	"-c", "1", vdev, "net_ring0,args=test"};

	const char *vdevval3[] = {prgname, prefix, no_huge, "-n", "1",
	"-c", "1", vdev, "net_ring0,nodeaction=r1:0:CREATE"};

	if (launch_proc(vdevinval) == 0) {
		printf("Error - process did run ok with invalid "
			"vdev parameter\n");
		return -1;
	}

	if (launch_proc(vdevval1) != 0) {
		printf("Error - process did not run ok with valid vdev value\n");
		return -1;
	}

	if (launch_proc(vdevval2) != 0) {
		printf("Error - process did not run ok with valid vdev value,"
			"with dummy args\n");
		return -1;
	}

	if (launch_proc(vdevval3) != 0) {
		printf("Error - process did not run ok with valid vdev value,"
			"with valid args\n");
		return -1;
	}
	return 0;
}
#endif

/*
 * Test that the app doesn't run with invalid -r option.
 */
static int
test_invalid_r_flag(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char * prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	const char *rinval[][9] = {
			{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-r", "error"},
			{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-r", "0"},
			{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-r", "-1"},
			{prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-r", "17"},
	};
	/* Test with valid blacklist option */
	const char *rval[] = {prgname, prefix, mp_flag, "-n", "1", "-c", "1", "-r", "16"};

	int i;

	for (i = 0; i != sizeof (rinval) / sizeof (rinval[0]); i++) {
		if (launch_proc(rinval[i]) == 0) {
			printf("Error - process did run ok with invalid "
			    "-r (rank) parameter\n");
			return -1;
		}
	}
	if (launch_proc(rval) != 0) {
		printf("Error - process did not run ok with valid -r (rank) value\n");
		return -1;
	}
	return 0;
}

/*
 * Test that the app doesn't run without the coremask/corelist flags. In all cases
 * should give an error and fail to run
 */
static int
test_missing_c_flag(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char * prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	/* -c flag but no coremask value */
	const char *argv1[] = { prgname, prefix, mp_flag, "-n", "3", "-c"};
	/* No -c, -l or --lcores flag at all */
	const char *argv2[] = { prgname, prefix, mp_flag, "-n", "3"};
	/* bad coremask value */
	const char *argv3[] = { prgname, prefix, mp_flag,
				"-n", "3", "-c", "error" };
	/* sanity check of tests - valid coremask value */
	const char *argv4[] = { prgname, prefix, mp_flag,
				"-n", "3", "-c", "1" };
	/* -l flag but no corelist value */
	const char *argv5[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l"};
	const char *argv6[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", " " };
	/* bad corelist values */
	const char *argv7[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", "error" };
	const char *argv8[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", "1-" };
	const char *argv9[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", "1," };
	const char *argv10[] = { prgname, prefix, mp_flag,
				 "-n", "3", "-l", "1#2" };
	/* core number is negative value */
	const char * const argv11[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", "-5" };
	const char * const argv12[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", "-5-7" };
	/* core number is maximum value */
	const char * const argv13[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", RTE_STR(RTE_MAX_LCORE) };
	const char * const argv14[] = { prgname, prefix, mp_flag,
				"-n", "3", "-l", "1-"RTE_STR(RTE_MAX_LCORE) };
	/* sanity check test - valid corelist value */
	const char * const argv15[] = { prgname, prefix, mp_flag,
				 "-n", "3", "-l", "1-2,3" };

	/* --lcores flag but no lcores value */
	const char * const argv16[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores" };
	const char * const argv17[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", " " };
	/* bad lcores value */
	const char * const argv18[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "1-3-5" };
	const char * const argv19[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "0-1,,2" };
	const char * const argv20[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "0-,1" };
	const char * const argv21[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "(0-,2-4)" };
	const char * const argv22[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "(-1,2)" };
	const char * const argv23[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "(2-4)@(2-4-6)" };
	const char * const argv24[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "(a,2)" };
	const char * const argv25[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "1-3@(1,3)" };
	const char * const argv26[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "3@((1,3)" };
	const char * const argv27[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "(4-7)=(1,3)" };
	const char * const argv28[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores", "[4-7]@(1,3)" };
	/* sanity check of tests - valid lcores value */
	const char * const argv29[] = { prgname, prefix, mp_flag,
				 "-n", "3", "--lcores",
				 "0-1,2@(5-7),(3-5)@(0,2),(0,6),7"};

	if (launch_proc(argv2) != 0) {
		printf("Error - "
		       "process did not run ok when missing -c flag\n");
		return -1;
	}

	if (launch_proc(argv1) == 0
			|| launch_proc(argv3) == 0) {
		printf("Error - "
		       "process ran without error with invalid -c flag\n");
		return -1;
	}
	if (launch_proc(argv4) != 0) {
		printf("Error - "
		       "process did not run ok with valid coremask value\n");
		return -1;
	}

	/* start -l test */
	if (launch_proc(argv5) == 0
			|| launch_proc(argv6) == 0
			|| launch_proc(argv7) == 0
			|| launch_proc(argv8) == 0
			|| launch_proc(argv9) == 0
			|| launch_proc(argv10) == 0
			|| launch_proc(argv11) == 0
			|| launch_proc(argv12) == 0
			|| launch_proc(argv13) == 0
			|| launch_proc(argv14) == 0) {
		printf("Error - "
		       "process ran without error with invalid -l flag\n");
		return -1;
	}
	if (launch_proc(argv15) != 0) {
		printf("Error - "
		       "process did not run ok with valid corelist value\n");
		return -1;
	}

	/* start --lcores tests */
	if (launch_proc(argv16) == 0 || launch_proc(argv17) == 0 ||
	    launch_proc(argv18) == 0 || launch_proc(argv19) == 0 ||
	    launch_proc(argv20) == 0 || launch_proc(argv21) == 0 ||
	    launch_proc(argv22) == 0 || launch_proc(argv23) == 0 ||
	    launch_proc(argv24) == 0 || launch_proc(argv25) == 0 ||
	    launch_proc(argv26) == 0 || launch_proc(argv27) == 0 ||
	    launch_proc(argv28) == 0) {
		printf("Error - "
		       "process ran without error with invalid --lcore flag\n");
		return -1;
	}

	if (launch_proc(argv29) != 0) {
		printf("Error - "
		       "process did not run ok with valid corelist value\n");
		return -1;
	}

	return 0;
}

/*
 * Test --master-lcore option with matching coremask
 */
static int
test_master_lcore_flag(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char *prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	/* --master-lcore flag but no value */
	const char *argv1[] = { prgname, prefix, mp_flag, "-n", "1", "-c", "3", "--master-lcore"};
	/* --master-lcore flag with invalid value */
	const char *argv2[] = { prgname, prefix, mp_flag, "-n", "1", "-c", "3", "--master-lcore", "-1"};
	const char *argv3[] = { prgname, prefix, mp_flag, "-n", "1", "-c", "3", "--master-lcore", "X"};
	/* master lcore not in coremask */
	const char *argv4[] = { prgname, prefix, mp_flag, "-n", "1", "-c", "3", "--master-lcore", "2"};
	/* valid value */
	const char *argv5[] = { prgname, prefix, mp_flag, "-n", "1", "-c", "3", "--master-lcore", "1"};
	/* valid value set before coremask */
	const char *argv6[] = { prgname, prefix, mp_flag, "-n", "1", "--master-lcore", "1", "-c", "3"};

	if (launch_proc(argv1) == 0
			|| launch_proc(argv2) == 0
			|| launch_proc(argv3) == 0
			|| launch_proc(argv4) == 0) {
		printf("Error - process ran without error with wrong --master-lcore\n");
		return -1;
	}
	if (launch_proc(argv5) != 0
			|| launch_proc(argv6) != 0) {
		printf("Error - process did not run ok with valid --master-lcore\n");
		return -1;
	}
	return 0;
}

/*
 * Test that the app doesn't run with invalid -n flag option.
 * Final test ensures it does run with valid options as sanity check
 * Since -n is not compulsory for MP, we instead use --no-huge and --no-shconf
 * flags.
 */
static int
test_invalid_n_flag(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char * prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	/* -n flag but no value */
	const char *argv1[] = { prgname, prefix, no_huge, no_shconf, "-c", "1", "-n"};
	/* bad numeric value */
	const char *argv2[] = { prgname, prefix, no_huge, no_shconf, "-c", "1", "-n", "e" };
	/* zero is invalid */
	const char *argv3[] = { prgname, prefix, no_huge, no_shconf, "-c", "1", "-n", "0" };
	/* sanity test - check with good value */
	const char *argv4[] = { prgname, prefix, no_huge, no_shconf, "-c", "1", "-n", "2" };
	/* sanity test - check with no -n flag */
	const char *argv5[] = { prgname, prefix, no_huge, no_shconf, "-c", "1"};

	if (launch_proc(argv1) == 0
			|| launch_proc(argv2) == 0
			|| launch_proc(argv3) == 0) {
		printf("Error - process ran without error when"
		       "invalid -n flag\n");
		return -1;
	}
	if (launch_proc(argv4) != 0) {
		printf("Error - process did not run ok with valid num-channel value\n");
		return -1;
	}
	if (launch_proc(argv5) != 0) {
		printf("Error - process did not run ok without -n flag\n");
		return -1;
	}

	return 0;
}

/*
 * Test that the app runs with HPET, and without HPET
 */
static int
test_no_hpet_flag(void)
{
	char prefix[PATH_MAX] = "";

#ifdef RTE_EXEC_ENV_FREEBSD
	return 0;
#else
	char tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	/* With --no-hpet */
	const char *argv1[] = {prgname, prefix, mp_flag, no_hpet, "-c", "1", "-n", "2"};
	/* Without --no-hpet */
	const char *argv2[] = {prgname, prefix, mp_flag, "-c", "1", "-n", "2"};

	if (launch_proc(argv1) != 0) {
		printf("Error - process did not run ok with --no-hpet flag\n");
		return -1;
	}
	if (launch_proc(argv2) != 0) {
		printf("Error - process did not run ok without --no-hpet flag\n");
		return -1;
	}
	return 0;
}

/*
 * Test that the app runs with --no-huge and doesn't run when --socket-mem are
 * specified with --no-huge.
 */
static int
test_no_huge_flag(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point, and we also need to
	 * run another primary process here */
	const char * prefix = no_shconf;
#else
	const char * prefix = "--file-prefix=nohuge";
#endif

	/* With --no-huge */
	const char *argv1[] = {prgname, prefix, no_huge, "-c", "1", "-n", "2"};
	/* With --no-huge and -m */
	const char *argv2[] = {prgname, prefix, no_huge, "-c", "1", "-n", "2",
			"-m", DEFAULT_MEM_SIZE};

	/* With --no-huge and --socket-mem */
	const char *argv3[] = {prgname, prefix, no_huge, "-c", "1", "-n", "2",
			"--socket-mem=" DEFAULT_MEM_SIZE};
	/* With --no-huge, -m and --socket-mem */
	const char *argv4[] = {prgname, prefix, no_huge, "-c", "1", "-n", "2",
			"-m", DEFAULT_MEM_SIZE, "--socket-mem=" DEFAULT_MEM_SIZE};
	if (launch_proc(argv1) != 0) {
		printf("Error - process did not run ok with --no-huge flag\n");
		return -1;
	}
	if (launch_proc(argv2) != 0) {
		printf("Error - process did not run ok with --no-huge and -m flags\n");
		return -1;
	}
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target does not support NUMA, hence no --socket-mem tests */
	return 0;
#endif

	if (launch_proc(argv3) == 0) {
		printf("Error - process run ok with --no-huge and --socket-mem "
				"flags\n");
		return -1;
	}
	if (launch_proc(argv4) == 0) {
		printf("Error - process run ok with --no-huge, -m and "
				"--socket-mem flags\n");
		return -1;
	}
	return 0;
}

static int
test_misc_flags(void)
{
	char hugepath[PATH_MAX] = {0};
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char * prefix = "";
	const char * nosh_prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	const char * nosh_prefix = "--file-prefix=noshconf";
	FILE * hugedir_handle = NULL;
	char line[PATH_MAX] = {0};
	unsigned i, isempty = 1;
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);

	/*
	 * get first valid hugepage path
	 */

	/* get hugetlbfs mountpoints from /proc/mounts */
	hugedir_handle = fopen("/proc/mounts", "r");

	if (hugedir_handle == NULL) {
		printf("Error opening /proc/mounts!\n");
		return -1;
	}

	/* read /proc/mounts */
	while (fgets(line, sizeof(line), hugedir_handle) != NULL) {

		/* find first valid hugepath */
		if (get_hugepage_path(line, sizeof(line), hugepath, sizeof(hugepath)))
			break;
	}

	fclose(hugedir_handle);

	/* check if path is not empty */
	for (i = 0; i < sizeof(hugepath); i++)
		if (hugepath[i] != '\0')
			isempty = 0;

	if (isempty) {
		printf("No mounted hugepage dir found!\n");
		return -1;
	}
#endif


	/* check that some general flags don't prevent things from working.
	 * All cases, apart from the first, app should run.
	 * No further testing of output done.
	 */
	/* sanity check - failure with invalid option */
	const char *argv0[] = {prgname, prefix, mp_flag, "-c", "1", "--invalid-opt"};

	/* With --no-pci */
	const char *argv1[] = {prgname, prefix, mp_flag, "-c", "1", "--no-pci"};
	/* With -v */
	const char *argv2[] = {prgname, prefix, mp_flag, "-c", "1", "-v"};
	/* With valid --syslog */
	const char *argv3[] = {prgname, prefix, mp_flag, "-c", "1",
			"--syslog", "syslog"};
	/* With empty --syslog (should fail) */
	const char *argv4[] = {prgname, prefix, mp_flag, "-c", "1", "--syslog"};
	/* With invalid --syslog */
	const char *argv5[] = {prgname, prefix, mp_flag, "-c", "1", "--syslog", "error"};
	/* With no-sh-conf, also use no-huge to ensure this test runs on BSD */
	const char *argv6[] = {prgname, "-c", "1", "-n", "2", "-m", DEFAULT_MEM_SIZE,
			no_shconf, nosh_prefix, no_huge};

	/* With --huge-dir */
	const char *argv7[] = {prgname, "-c", "1", "-n", "2", "-m", DEFAULT_MEM_SIZE,
			"--file-prefix=hugedir", "--huge-dir", hugepath};
	/* With empty --huge-dir (should fail) */
	const char *argv8[] = {prgname, "-c", "1", "-n", "2", "-m", DEFAULT_MEM_SIZE,
			"--file-prefix=hugedir", "--huge-dir"};
	/* With invalid --huge-dir */
	const char *argv9[] = {prgname, "-c", "1", "-n", "2", "-m", DEFAULT_MEM_SIZE,
			"--file-prefix=hugedir", "--huge-dir", "invalid"};
	/* Secondary process with invalid --huge-dir (should run as flag has no
	 * effect on secondary processes) */
	const char *argv10[] = {prgname, prefix, mp_flag, "-c", "1", "--huge-dir", "invalid"};

	/* try running with base-virtaddr param */
	const char *argv11[] = {prgname, "--file-prefix=virtaddr",
			"-c", "1", "-n", "2", "--base-virtaddr=0x12345678"};

	/* try running with --vfio-intr INTx flag */
	const char *argv12[] = {prgname, "--file-prefix=intr",
			"-c", "1", "-n", "2", "--vfio-intr=legacy"};

	/* try running with --vfio-intr MSI flag */
	const char *argv13[] = {prgname, "--file-prefix=intr",
			"-c", "1", "-n", "2", "--vfio-intr=msi"};

	/* try running with --vfio-intr MSI-X flag */
	const char *argv14[] = {prgname, "--file-prefix=intr",
			"-c", "1", "-n", "2", "--vfio-intr=msix"};

	/* try running with --vfio-intr invalid flag */
	const char *argv15[] = {prgname, "--file-prefix=intr",
			"-c", "1", "-n", "2", "--vfio-intr=invalid"};

	/* run all tests also applicable to FreeBSD first */

	if (launch_proc(argv0) == 0) {
		printf("Error - process ran ok with invalid flag\n");
		return -1;
	}
	if (launch_proc(argv1) != 0) {
		printf("Error - process did not run ok with --no-pci flag\n");
		return -1;
	}
	if (launch_proc(argv2) != 0) {
		printf("Error - process did not run ok with -v flag\n");
		return -1;
	}
	if (launch_proc(argv6) != 0) {
		printf("Error - process did not run ok with --no-shconf flag\n");
		return -1;
	}

#ifdef RTE_EXEC_ENV_FREEBSD
	/* no more tests to be done on FreeBSD */
	return 0;
#endif

	if (launch_proc(argv3) != 0) {
		printf("Error - process did not run ok with --syslog flag\n");
		return -1;
	}
	if (launch_proc(argv4) == 0) {
		printf("Error - process run ok with empty --syslog flag\n");
		return -1;
	}
	if (launch_proc(argv5) == 0) {
		printf("Error - process run ok with invalid --syslog flag\n");
		return -1;
	}
	if (launch_proc(argv7) != 0) {
		printf("Error - process did not run ok with --huge-dir flag\n");
		return -1;
	}
	if (launch_proc(argv8) == 0) {
		printf("Error - process run ok with empty --huge-dir flag\n");
		return -1;
	}
	if (launch_proc(argv9) == 0) {
		printf("Error - process run ok with invalid --huge-dir flag\n");
		return -1;
	}
	if (launch_proc(argv10) != 0) {
		printf("Error - secondary process did not run ok with invalid --huge-dir flag\n");
		return -1;
	}
	if (launch_proc(argv11) != 0) {
		printf("Error - process did not run ok with --base-virtaddr parameter\n");
		return -1;
	}
	if (launch_proc(argv12) != 0) {
		printf("Error - process did not run ok with "
				"--vfio-intr INTx parameter\n");
		return -1;
	}
	if (launch_proc(argv13) != 0) {
		printf("Error - process did not run ok with "
				"--vfio-intr MSI parameter\n");
		return -1;
	}
	if (launch_proc(argv14) != 0) {
		printf("Error - process did not run ok with "
				"--vfio-intr MSI-X parameter\n");
		return -1;
	}
	if (launch_proc(argv15) == 0) {
		printf("Error - process run ok with "
				"--vfio-intr invalid parameter\n");
		return -1;
	}
	return 0;
}

static int
test_file_prefix(void)
{
	/*
	 * 1. check if current process hugefiles are locked
	 * 2. try to run secondary process without a corresponding primary process
	 * (while failing to run, it will also remove any unused hugepage files)
	 * 3. check if current process hugefiles are still in place and are locked
	 * 4. run a primary process with memtest1 prefix in default and legacy
	 *    mem mode
	 * 5. check if memtest1 hugefiles are created in case of legacy mem
	 *    mode, and deleted in case of default mem mode
	 * 6. run a primary process with memtest2 prefix in default and legacy
	 *    mem modes
	 * 7. check that memtest2 hugefiles are present in the hugedir after a
	 *    run in legacy mode, and not present at all after run in default
	 *    mem mode
	 */
	char prefix[PATH_MAX] = "";

#ifdef RTE_EXEC_ENV_FREEBSD
	return 0;
#else
	if (get_current_prefix(prefix, sizeof(prefix)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
#endif

	/* this should fail unless the test itself is run with "memtest" prefix */
	const char *argv0[] = {prgname, mp_flag, "-c", "1", "-n", "2", "-m", DEFAULT_MEM_SIZE,
			"--file-prefix=" memtest };

	/* primary process with memtest1 and default mem mode */
	const char *argv1[] = {prgname, "-c", "1", "-n", "2", "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest1 };

	/* primary process with memtest1 and legacy mem mode */
	const char *argv2[] = {prgname, "-c", "1", "-n", "2", "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest1,
			"--legacy-mem" };

	/* primary process with memtest2 and legacy mem mode */
	const char *argv3[] = {prgname, "-c", "1", "-n", "2", "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest2,
			"--legacy-mem" };

	/* primary process with memtest2 and default mem mode */
	const char *argv4[] = {prgname, "-c", "1", "-n", "2", "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest2 };

	/* check if files for current prefix are present */
	if (process_hugefiles(prefix, HUGEPAGE_CHECK_EXISTS) != 1) {
		printf("Error - hugepage files for %s were not created!\n", prefix);
		return -1;
	}

	/* checks if files for current prefix are locked */
	if (process_hugefiles(prefix, HUGEPAGE_CHECK_LOCKED) != 1) {
		printf("Error - hugepages for current process aren't locked!\n");
		return -1;
	}

	/* check if files for secondary process are present */
	if (process_hugefiles(memtest, HUGEPAGE_CHECK_EXISTS) == 1) {
		/* check if they are not locked */
		if (process_hugefiles(memtest, HUGEPAGE_CHECK_LOCKED) == 1) {
			printf("Error - hugepages for current process are locked!\n");
			return -1;
		}
		/* they aren't locked, delete them */
		else {
			if (process_hugefiles(memtest, HUGEPAGE_DELETE) != 1) {
				printf("Error - deleting hugepages failed!\n");
				return -1;
			}
		}
	}

	if (launch_proc(argv0) == 0) {
		printf("Error - secondary process ran ok without primary process\n");
		return -1;
	}

	/* check if files for current prefix are present */
	if (process_hugefiles(prefix, HUGEPAGE_CHECK_EXISTS) != 1) {
		printf("Error - hugepage files for %s were not created!\n", prefix);
		return -1;
	}

	/* checks if files for current prefix are locked */
	if (process_hugefiles(prefix, HUGEPAGE_CHECK_LOCKED) != 1) {
		printf("Error - hugepages for current process aren't locked!\n");
		return -1;
	}

	/* we're running this process in default memory mode, which means it
	 * should clean up after itself on exit and leave no hugepages behind.
	 */
	if (launch_proc(argv1) != 0) {
		printf("Error - failed to run with --file-prefix=%s\n",
				memtest1);
		return -1;
	}

	/* check if memtest1_map0 is present */
	if (process_hugefiles(memtest1, HUGEPAGE_CHECK_EXISTS) != 0) {
		printf("Error - hugepage files for %s were not deleted!\n",
				memtest1);
		return -1;
	}

	/* now, we're running a process under the same prefix, but with legacy
	 * mem mode - this should leave behind hugepage files.
	 */
	if (launch_proc(argv2) != 0) {
		printf("Error - failed to run with --file-prefix=%s\n",
				memtest1);
		return -1;
	}

	/* check if memtest1_map0 is present */
	if (process_hugefiles(memtest1, HUGEPAGE_CHECK_EXISTS) != 1) {
		printf("Error - hugepage files for %s were not created!\n",
				memtest1);
		return -1;
	}

	if (launch_proc(argv3) != 0) {
		printf("Error - failed to run with --file-prefix=%s\n",
				memtest2);
		return -1;
	}

	/* check if hugefiles for memtest2 are present */
	if (process_hugefiles(memtest2, HUGEPAGE_CHECK_EXISTS) != 1) {
		printf("Error - hugepage files for %s were not created!\n",
				memtest2);
		return -1;
	}

	/* check if hugefiles for memtest1 are present */
	if (process_hugefiles(memtest1, HUGEPAGE_CHECK_EXISTS) != 0) {
		printf("Error - hugepage files for %s were not deleted!\n",
				memtest1);
		return -1;
	}

	/* this process will run in default mem mode, so it should not leave any
	 * hugepage files behind.
	 */
	if (launch_proc(argv4) != 0) {
		printf("Error - failed to run with --file-prefix=%s\n",
				memtest2);
		return -1;
	}

	/* check if hugefiles for memtest2 are present */
	if (process_hugefiles(memtest2, HUGEPAGE_CHECK_EXISTS) != 0) {
		printf("Error - hugepage files for %s were not deleted!\n",
				memtest2);
		return -1;
	}

	/* check if hugefiles for memtest1 are present */
	if (process_hugefiles(memtest1, HUGEPAGE_CHECK_EXISTS) != 0) {
		printf("Error - hugepage files for %s were not deleted!\n",
				memtest1);
		return -1;
	}

	return 0;
}

/*
 * Tests for correct handling of -m and --socket-mem flags
 */
static int
test_memory_flags(void)
{
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point */
	const char * prefix = "";
#else
	char prefix[PATH_MAX], tmp[PATH_MAX];
	if (get_current_prefix(tmp, sizeof(tmp)) == NULL) {
		printf("Error - unable to get current prefix!\n");
		return -1;
	}
	snprintf(prefix, sizeof(prefix), "--file-prefix=%s", tmp);
#endif

	/* valid -m flag and mp flag */
	const char *argv0[] = {prgname, prefix, mp_flag, "-c", "10",
			"-n", "2", "-m", DEFAULT_MEM_SIZE};

	/* valid -m flag */
	const char *argv1[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "-m", DEFAULT_MEM_SIZE};

	/* valid (zero) --socket-mem flag */
	const char *argv2[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "--socket-mem=0,0,0,0"};

	/* invalid (incomplete) --socket-mem flag */
	const char *argv3[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "--socket-mem=2,2,"};

	/* invalid (mixed with invalid data) --socket-mem flag */
	const char *argv4[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "--socket-mem=2,2,Fred"};

	/* invalid (with numeric value as last character) --socket-mem flag */
	const char *argv5[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "--socket-mem=2,2,Fred0"};

	/* invalid (with empty socket) --socket-mem flag */
	const char *argv6[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "--socket-mem=2,,2"};

	/* invalid (null) --socket-mem flag */
	const char *argv7[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "--socket-mem="};

	/* valid --socket-mem specified together with -m flag */
	const char *argv8[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, "-m", DEFAULT_MEM_SIZE, "--socket-mem=2,2"};

	/* construct an invalid socket mask with 2 megs on each socket plus
	 * extra 2 megs on socket that doesn't exist on current system */
	char invalid_socket_mem[SOCKET_MEM_STRLEN];
	char buf[SOCKET_MEM_STRLEN];	/* to avoid copying string onto itself */

#ifdef RTE_EXEC_ENV_FREEBSD
	int i, num_sockets = 1;
#else
	int i, num_sockets = RTE_MIN(get_number_of_sockets(),
			RTE_MAX_NUMA_NODES);
#endif

	if (num_sockets <= 0) {
		printf("Error - cannot get number of sockets!\n");
		return -1;
	}

	snprintf(invalid_socket_mem, sizeof(invalid_socket_mem), "--socket-mem=");

	/* add one extra socket */
	for (i = 0; i < num_sockets + 1; i++) {
		snprintf(buf, sizeof(buf), "%s%s", invalid_socket_mem, DEFAULT_MEM_SIZE);
		strlcpy(invalid_socket_mem, buf, sizeof(invalid_socket_mem));

		if (num_sockets + 1 - i > 1) {
			snprintf(buf, sizeof(buf), "%s,", invalid_socket_mem);
			strlcpy(invalid_socket_mem, buf,
				sizeof(invalid_socket_mem));
		}
	}

	/* construct a valid socket mask with 2 megs on each existing socket */
	char valid_socket_mem[SOCKET_MEM_STRLEN];

	snprintf(valid_socket_mem, sizeof(valid_socket_mem), "--socket-mem=");

	/* add one extra socket */
	for (i = 0; i < num_sockets; i++) {
		snprintf(buf, sizeof(buf), "%s%s", valid_socket_mem, DEFAULT_MEM_SIZE);
		strlcpy(valid_socket_mem, buf, sizeof(valid_socket_mem));

		if (num_sockets - i > 1) {
			snprintf(buf, sizeof(buf), "%s,", valid_socket_mem);
			strlcpy(valid_socket_mem, buf,
				sizeof(valid_socket_mem));
		}
	}

	/* invalid --socket-mem flag (with extra socket) */
	const char *argv9[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, invalid_socket_mem};

	/* valid --socket-mem flag */
	const char *argv10[] = {prgname, "-c", "10", "-n", "2",
			"--file-prefix=" memtest, valid_socket_mem};

	if (launch_proc(argv0) != 0) {
		printf("Error - secondary process failed with valid -m flag !\n");
		return -1;
	}

#ifdef RTE_EXEC_ENV_FREEBSD
	/* no other tests are applicable to BSD */
	return 0;
#endif

	if (launch_proc(argv1) != 0) {
		printf("Error - process failed with valid -m flag!\n");
		return -1;
	}
	if (launch_proc(argv2) != 0) {
		printf("Error - process failed with valid (zero) --socket-mem!\n");
		return -1;
	}

	if (launch_proc(argv3) == 0) {
		printf("Error - process run ok with invalid "
				"(incomplete) --socket-mem!\n");
		return -1;
	}

	if (launch_proc(argv4) == 0) {
		printf("Error - process run ok with invalid "
				"(mixed with invalid input) --socket-mem!\n");
		return -1;
	}

	if (launch_proc(argv5) == 0) {
		printf("Error - process run ok with invalid "
				"(mixed with invalid input with a numeric value as "
				"last character) --socket-mem!\n");
		return -1;
	}

	if (launch_proc(argv6) == 0) {
		printf("Error - process run ok with invalid "
				"(with empty socket) --socket-mem!\n");
		return -1;
	}

	if (launch_proc(argv7) == 0) {
		printf("Error - process run ok with invalid (null) --socket-mem!\n");
		return -1;
	}

	if (launch_proc(argv8) == 0) {
		printf("Error - process run ok with --socket-mem and -m specified!\n");
		return -1;
	}

	if (launch_proc(argv9) == 0) {
		printf("Error - process run ok with extra socket in --socket-mem!\n");
		return -1;
	}

	if (launch_proc(argv10) != 0) {
		printf("Error - process failed with valid --socket-mem!\n");
		return -1;
	}

	return 0;
}

static int
test_eal_flags(void)
{
	int ret = 0;

	ret = test_missing_c_flag();
	if (ret < 0) {
		printf("Error in test_missing_c_flag()\n");
		return ret;
	}

	ret = test_master_lcore_flag();
	if (ret < 0) {
		printf("Error in test_master_lcore_flag()\n");
		return ret;
	}

	ret = test_invalid_n_flag();
	if (ret < 0) {
		printf("Error in test_invalid_n_flag()\n");
		return ret;
	}

	ret = test_no_hpet_flag();
	if (ret < 0) {
		printf("Error in test_no_hpet_flag()\n");
		return ret;
	}

	ret = test_no_huge_flag();
	if (ret < 0) {
		printf("Error in test_no_huge_flag()\n");
		return ret;
	}

	ret = test_whitelist_flag();
	if (ret < 0) {
		printf("Error in test_invalid_whitelist_flag()\n");
		return ret;
	}

	ret = test_invalid_b_flag();
	if (ret < 0) {
		printf("Error in test_invalid_b_flag()\n");
		return ret;
	}

#ifdef RTE_LIBRTE_PMD_RING
	ret = test_invalid_vdev_flag();
	if (ret < 0) {
		printf("Error in test_invalid_vdev_flag()\n");
		return ret;
	}
#endif
	ret = test_invalid_r_flag();
	if (ret < 0) {
		printf("Error in test_invalid_r_flag()\n");
		return ret;
	}

	ret = test_memory_flags();
	if (ret < 0) {
		printf("Error in test_memory_flags()\n");
		return ret;
	}

	ret = test_file_prefix();
	if (ret < 0) {
		printf("Error in test_file_prefix()\n");
		return ret;
	}

	ret = test_misc_flags();
	if (ret < 0) {
		printf("Error in test_misc_flags()");
		return ret;
	}

	return ret;
}

REGISTER_TEST_COMMAND(eal_flags_autotest, test_eal_flags);
