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

#include <rte_lcore.h>
#include <rte_debug.h>
#include <rte_string_fns.h>

#include "process.h"

#define DEFAULT_MEM_SIZE "18"
#define mp_flag "--proc-type=secondary"
#define no_hpet "--no-hpet"
#define no_huge "--no-huge"
#define no_shconf "--no-shconf"
#define allow "--allow"
#define vdev "--vdev"
#define memtest "memtest"
#define memtest1 "memtest1"
#define memtest2 "memtest2"
#define SOCKET_MEM_STRLEN (RTE_MAX_NUMA_NODES * 20)
#define launch_proc(ARGV) process_dup(ARGV, RTE_DIM(ARGV), __func__)

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
					closedir(hugepage_dir);
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
 * Test that the app doesn't run with invalid allow option.
 * Final tests ensures it does run with valid options as sanity check (one
 * test for with Domain+BDF, second for just with BDF)
 */
static int
test_allow_flag(void)
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

	const char *wlinval[][7] = {
		{prgname, prefix, mp_flag,
				allow, "error", "", ""},
		{prgname, prefix, mp_flag,
				allow, "0:0:0", "", ""},
		{prgname, prefix, mp_flag,
				allow, "0:error:0.1", "", ""},
		{prgname, prefix, mp_flag,
				allow, "0:0:0.1error", "", ""},
		{prgname, prefix, mp_flag,
				allow, "error0:0:0.1", "", ""},
		{prgname, prefix, mp_flag,
				allow, "0:0:0.1.2", "", ""},
	};
	/* Test with valid allow option */
	const char *wlval1[] = {prgname, prefix, mp_flag,
			allow, "00FF:09:0B.3"};
	const char *wlval2[] = {prgname, prefix, mp_flag,
			allow, "09:0B.3", allow, "0a:0b.1"};
	const char *wlval3[] = {prgname, prefix, mp_flag,
			allow, "09:0B.3,type=test",
			allow, "08:00.1,type=normal",
	};

	for (i = 0; i < RTE_DIM(wlinval); i++) {
		if (launch_proc(wlinval[i]) == 0) {
			printf("Error - process did run ok with invalid "
			    "allow parameter\n");
			return -1;
		}
	}
	if (launch_proc(wlval1) != 0 ) {
		printf("Error - process did not run ok with valid allow\n");
		return -1;
	}
	if (launch_proc(wlval2) != 0 ) {
		printf("Error - process did not run ok with valid allow value set\n");
		return -1;
	}
	if (launch_proc(wlval3) != 0 ) {
		printf("Error - process did not run ok with valid allow + args\n");
		return -1;
	}

	return 0;
}

/*
 * Test that the app doesn't run with invalid blocklist option.
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

	const char *blinval[][5] = {
		{prgname, prefix, mp_flag, "-b", "error"},
		{prgname, prefix, mp_flag, "-b", "0:0:0"},
		{prgname, prefix, mp_flag, "-b", "0:error:0.1"},
		{prgname, prefix, mp_flag, "-b", "0:0:0.1error"},
		{prgname, prefix, mp_flag, "-b", "error0:0:0.1"},
		{prgname, prefix, mp_flag, "-b", "0:0:0.1.2"},
	};
	/* Test with valid blocklist option */
	const char *blval[] = {prgname, prefix, mp_flag,
			       "-b", "FF:09:0B.3"};

	int i;

	for (i = 0; i != RTE_DIM(blinval); i++) {
		if (launch_proc(blinval[i]) == 0) {
			printf("Error - process did run ok with invalid "
			    "blocklist parameter\n");
			return -1;
		}
	}
	if (launch_proc(blval) != 0) {
		printf("Error - process did not run ok with valid blocklist value\n");
		return -1;
	}
	return 0;
}

/*
 *  Test that the app doesn't run with invalid vdev option.
 *  Final test ensures it does run with valid options as sanity check
 */
static int
test_invalid_vdev_flag(void)
{
#ifdef RTE_NET_RING
#ifdef RTE_EXEC_ENV_FREEBSD
	/* BSD target doesn't support prefixes at this point, and we also need to
	 * run another primary process here */
	const char * prefix = no_shconf;
#else
	const char * prefix = "--file-prefix=vdev";
#endif

	/* Test with invalid vdev option */
	const char *vdevinval[] = {prgname, prefix, no_huge,
				vdev, "eth_dummy"};

	/* Test with valid vdev option */
	const char *vdevval1[] = {prgname, prefix, no_huge,
	vdev, "net_ring0"};

	const char *vdevval2[] = {prgname, prefix, no_huge,
	vdev, "net_ring0,args=test"};

	const char *vdevval3[] = {prgname, prefix, no_huge,
	vdev, "net_ring0,nodeaction=r1:0:CREATE"};

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
#else
	return TEST_SKIPPED;
#endif
}

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

	const char *rinval[][5] = {
			{prgname, prefix, mp_flag, "-r", "error"},
			{prgname, prefix, mp_flag, "-r", "0"},
			{prgname, prefix, mp_flag, "-r", "-1"},
			{prgname, prefix, mp_flag, "-r", "17"},
	};
	/* Test with valid blocklist option */
	const char *rval[] = {prgname, prefix, mp_flag, "-r", "16"};

	int i;

	for (i = 0; i != RTE_DIM(rinval); i++) {
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
	const char *argv1[] = { prgname, prefix, mp_flag, "-c"};
	/* No -c, -l or --lcores flag at all */
	const char *argv2[] = { prgname, prefix, mp_flag};
	/* bad coremask value */
	const char *argv3[] = { prgname, prefix, mp_flag,
				"-c", "error" };
	/* sanity check of tests - valid coremask value */
	const char *argv4[] = { prgname, prefix, mp_flag,
				"-c", "1" };
	/* -l flag but no corelist value */
	const char *argv5[] = { prgname, prefix, mp_flag,
				"-l"};
	const char *argv6[] = { prgname, prefix, mp_flag,
				"-l", " " };
	/* bad corelist values */
	const char *argv7[] = { prgname, prefix, mp_flag,
				"-l", "error" };
	const char *argv8[] = { prgname, prefix, mp_flag,
				"-l", "1-" };
	const char *argv9[] = { prgname, prefix, mp_flag,
				"-l", "1," };
	const char *argv10[] = { prgname, prefix, mp_flag,
				 "-l", "1#2" };
	/* core number is negative value */
	const char * const argv11[] = { prgname, prefix, mp_flag,
				"-l", "-5" };
	const char * const argv12[] = { prgname, prefix, mp_flag,
				"-l", "-5-7" };
	/* core number is maximum value */
	const char * const argv13[] = { prgname, prefix, mp_flag,
				"-l", RTE_STR(RTE_MAX_LCORE) };
	const char * const argv14[] = { prgname, prefix, mp_flag,
				"-l", "1-"RTE_STR(RTE_MAX_LCORE) };
	/* sanity check test - valid corelist value */
	const char * const argv15[] = { prgname, prefix, mp_flag,
				 "-l", "1-2,3" };

	/* --lcores flag but no lcores value */
	const char * const argv16[] = { prgname, prefix, mp_flag,
				 "--lcores" };
	const char * const argv17[] = { prgname, prefix, mp_flag,
				 "--lcores", " " };
	/* bad lcores value */
	const char * const argv18[] = { prgname, prefix, mp_flag,
				 "--lcores", "1-3-5" };
	const char * const argv19[] = { prgname, prefix, mp_flag,
				 "--lcores", "0-1,,2" };
	const char * const argv20[] = { prgname, prefix, mp_flag,
				 "--lcores", "0-,1" };
	const char * const argv21[] = { prgname, prefix, mp_flag,
				 "--lcores", "(0-,2-4)" };
	const char * const argv22[] = { prgname, prefix, mp_flag,
				 "--lcores", "(-1,2)" };
	const char * const argv23[] = { prgname, prefix, mp_flag,
				 "--lcores", "(2-4)@(2-4-6)" };
	const char * const argv24[] = { prgname, prefix, mp_flag,
				 "--lcores", "(a,2)" };
	const char * const argv25[] = { prgname, prefix, mp_flag,
				 "--lcores", "1-3@(1,3)" };
	const char * const argv26[] = { prgname, prefix, mp_flag,
				 "--lcores", "3@((1,3)" };
	const char * const argv27[] = { prgname, prefix, mp_flag,
				 "--lcores", "(4-7)=(1,3)" };
	const char * const argv28[] = { prgname, prefix, mp_flag,
				 "--lcores", "[4-7]@(1,3)" };
	/* sanity check of tests - valid lcores value */
	const char * const argv29[] = { prgname, prefix, mp_flag,
				 "--lcores",
				 "0-1,2@(5-7),(3-5)@(0,2),(0,6),7"};
	/* check an invalid cpu value >= CPU_SETSIZE */
	const char * const argv30[] = { prgname, prefix, mp_flag,
				 "--lcores", "3@" RTE_STR(CPU_SETSIZE) };

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
	if (rte_lcore_is_enabled(0) && rte_lcore_is_enabled(1) &&
	    rte_lcore_is_enabled(2) && rte_lcore_is_enabled(3) &&
	    launch_proc(argv15) != 0) {
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
	    launch_proc(argv28) == 0 || launch_proc(argv30) == 0) {
		printf("Error - "
		       "process ran without error with invalid --lcore flag\n");
		return -1;
	}

	if (rte_lcore_is_enabled(0) && rte_lcore_is_enabled(1) &&
	    rte_lcore_is_enabled(2) && rte_lcore_is_enabled(3) &&
	    rte_lcore_is_enabled(3) && rte_lcore_is_enabled(5) &&
	    rte_lcore_is_enabled(4) && rte_lcore_is_enabled(7) &&
	    launch_proc(argv29) != 0) {
		printf("Error - "
		       "process did not run ok with valid corelist value\n");
		return -1;
	}

	return 0;
}

/*
 * Test --main-lcore option with matching coremask
 */
static int
test_main_lcore_flag(void)
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

	if (!rte_lcore_is_enabled(0) || !rte_lcore_is_enabled(1))
		return TEST_SKIPPED;

	/* --main-lcore flag but no value */
	const char *argv1[] = { prgname, prefix, mp_flag,
				"-c", "3", "--main-lcore"};
	/* --main-lcore flag with invalid value */
	const char *argv2[] = { prgname, prefix, mp_flag,
				"-c", "3", "--main-lcore", "-1"};
	const char *argv3[] = { prgname, prefix, mp_flag,
				"-c", "3", "--main-lcore", "X"};
	/* main lcore not in coremask */
	const char *argv4[] = { prgname, prefix, mp_flag,
				"-c", "3", "--main-lcore", "2"};
	/* valid value */
	const char *argv5[] = { prgname, prefix, mp_flag,
				"-c", "3", "--main-lcore", "1"};
	/* valid value set before coremask */
	const char *argv6[] = { prgname, prefix, mp_flag,
				"--main-lcore", "1", "-c", "3"};

	if (launch_proc(argv1) == 0
			|| launch_proc(argv2) == 0
			|| launch_proc(argv3) == 0
			|| launch_proc(argv4) == 0) {
		printf("Error - process ran without error with wrong --main-lcore\n");
		return -1;
	}
	if (launch_proc(argv5) != 0
			|| launch_proc(argv6) != 0) {
		printf("Error - process did not run ok with valid --main-lcore\n");
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
	const char *argv1[] = { prgname, prefix, no_huge, no_shconf,
				"-n"};
	/* bad numeric value */
	const char *argv2[] = { prgname, prefix, no_huge, no_shconf,
				"-n", "e" };
	/* zero is invalid */
	const char *argv3[] = { prgname, prefix, no_huge, no_shconf,
				"-n", "0" };
	/* sanity test - check with good value */
	const char *argv4[] = { prgname, prefix, no_huge, no_shconf,
				"-n", "2" };
	/* sanity test - check with no -n flag */
	const char *argv5[] = { prgname, prefix, no_huge, no_shconf};

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
	const char *argv1[] = {prgname, prefix, mp_flag, no_hpet};
	/* Without --no-hpet */
	const char *argv2[] = {prgname, prefix, mp_flag};

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
	const char *argv1[] = {prgname, prefix, no_huge};
	/* With --no-huge and -m */
	const char *argv2[] = {prgname, prefix, no_huge,
			"-m", DEFAULT_MEM_SIZE};

	/* With --no-huge and --socket-mem */
	const char *argv3[] = {prgname, prefix, no_huge,
			"--socket-mem=" DEFAULT_MEM_SIZE};
	/* With --no-huge, -m and --socket-mem */
	const char *argv4[] = {prgname, prefix, no_huge,
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
	const char *argv0[] = {prgname, prefix, mp_flag, "--invalid-opt"};

	/* With --no-pci */
	const char *argv1[] = {prgname, prefix, mp_flag, "--no-pci"};
	/* With -v */
	const char *argv2[] = {prgname, prefix, mp_flag, "-v"};
	/* With valid --syslog */
	const char *argv3[] = {prgname, prefix, mp_flag,
			"--syslog", "syslog"};
	/* With empty --syslog (should fail) */
	const char *argv4[] = {prgname, prefix, mp_flag, "--syslog"};
	/* With invalid --syslog */
	const char *argv5[] = {prgname, prefix, mp_flag, "--syslog", "error"};
	/* With no-sh-conf, also use no-huge to ensure this test runs on BSD */
	const char *argv6[] = {prgname, "-m", DEFAULT_MEM_SIZE,
			no_shconf, nosh_prefix, no_huge};

	/* With --huge-dir */
	const char *argv7[] = {prgname, "-m", DEFAULT_MEM_SIZE,
			"--file-prefix=hugedir", "--huge-dir", hugepath};
	/* With empty --huge-dir (should fail) */
	const char *argv8[] = {prgname, "-m", DEFAULT_MEM_SIZE,
			"--file-prefix=hugedir", "--huge-dir"};
	/* With invalid --huge-dir */
	const char *argv9[] = {prgname, "-m", DEFAULT_MEM_SIZE,
			"--file-prefix=hugedir", "--huge-dir", "invalid"};
	/* Secondary process with invalid --huge-dir (should run as flag has no
	 * effect on secondary processes) */
	const char *argv10[] = {prgname, prefix, mp_flag,
			"--huge-dir", "invalid"};

	/* try running with base-virtaddr param */
	const char *argv11[] = {prgname, "--file-prefix=virtaddr",
			"--base-virtaddr=0x12345678"};

	/* try running with --vfio-intr INTx flag */
	const char *argv12[] = {prgname, "--file-prefix=intr",
			"--vfio-intr=legacy"};

	/* try running with --vfio-intr MSI flag */
	const char *argv13[] = {prgname, "--file-prefix=intr",
			"--vfio-intr=msi"};

	/* try running with --vfio-intr MSI-X flag */
	const char *argv14[] = {prgname, "--file-prefix=intr",
			"--vfio-intr=msix"};

	/* try running with --vfio-intr invalid flag */
	const char *argv15[] = {prgname, "--file-prefix=intr",
			"--vfio-intr=invalid"};

	/* With process type as auto-detect */
	const char * const argv16[] = {prgname, "--file-prefix=auto",
			"--proc-type=auto"};

	/* With process type as auto-detect with no-shconf */
	const char * const argv17[] = {prgname, "--proc-type=auto",
			no_shconf, nosh_prefix, no_huge};

	/* With process type as --create-uio-dev flag */
	const char * const argv18[] = {prgname, "--file-prefix=uiodev",
			"--create-uio-dev"};

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
	if (launch_proc(argv16) != 0) {
		printf("Error - process did not run ok with "
				"--proc-type as auto parameter\n");
		return -1;
	}
	if (launch_proc(argv17) != 0) {
		printf("Error - process did not run ok with "
				"--proc-type and --no-shconf parameter\n");
		return -1;
	}
	if (launch_proc(argv18) != 0) {
		printf("Error - process did not run ok with "
				"--create-uio-dev parameter\n");
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
	const char *argv0[] = {prgname, mp_flag, "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest };

	/* primary process with memtest1 and default mem mode */
	const char *argv1[] = {prgname, "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest1 };

	/* primary process with memtest1 and legacy mem mode */
	const char *argv2[] = {prgname, "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest1,
			"--legacy-mem" };

	/* primary process with memtest2 and legacy mem mode */
	const char *argv3[] = {prgname, "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest2,
			"--legacy-mem" };

	/* primary process with memtest2 and default mem mode */
	const char *argv4[] = {prgname, "-m",
			DEFAULT_MEM_SIZE, "--file-prefix=" memtest2 };

	/* primary process with --in-memory mode */
	const char * const argv5[] = {prgname, "-m",
		DEFAULT_MEM_SIZE, "--in-memory" };

	/* primary process with memtest1 and --in-memory mode */
	const char * const argv6[] = {prgname, "-m",
		DEFAULT_MEM_SIZE, "--in-memory",
		"--file-prefix=" memtest1 };

	/* primary process with parent file-prefix and --in-memory mode */
	const char * const argv7[] = {prgname, "-m",
		DEFAULT_MEM_SIZE, "--in-memory", "--file-prefix", prefix };

	/* primary process with memtest1 and --single-file-segments mode */
	const char * const argv8[] = {prgname, "-m",
		DEFAULT_MEM_SIZE, "--single-file-segments",
		"--file-prefix=" memtest1 };

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

	/* this process will run in --in-memory mode, so it should not leave any
	 * hugepage files behind.
	 */

	/* test case to check eal-options with --in-memory mode */
	if (launch_proc(argv5) != 0) {
		printf("Error - failed to run with --in-memory mode\n");
		return -1;
	}

	/*test case to check eal-options with --in-memory mode with
	 * custom file-prefix.
	 */
	if (launch_proc(argv6) != 0) {
		printf("Error - failed to run with --in-memory mode\n");
		return -1;
	}

	/* check if hugefiles for memtest1 are present */
	if (process_hugefiles(memtest1, HUGEPAGE_CHECK_EXISTS) != 0) {
		printf("Error - hugepage files for %s were created and not deleted!\n",
				memtest1);
		return -1;
	}

	/* test case to check eal-options with --in-memory mode with
	 * parent file-prefix.
	 */
	if (launch_proc(argv7) != 0) {
		printf("Error - failed to run with --file-prefix=%s\n", prefix);
		return -1;
	}

	/* this process will run in --single-file-segments mode,
	 * so it should not leave any hugepage files behind.
	 */
	if (launch_proc(argv8) != 0) {
		printf("Error - failed to run with --single-file-segments mode\n");
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

/* This function writes in passed buf pointer a valid --socket-mem= option
 * for num_sockets then concatenates the provided suffix string.
 *
 * Example for num_sockets 4, mem "2", suffix "plop"
 * --socket-mem=2,2,2,2plop
 */
static void
populate_socket_mem_param(int num_sockets, const char *mem,
		const char *suffix, char *buf, size_t buf_size)
{
	unsigned int offset = 0;
	int written;
	int i;

	written = snprintf(&buf[offset], buf_size - offset, "--socket-mem=");
	if (written < 0 || written + offset >= buf_size)
		return;
	offset += written;

	for (i = 0; i < num_sockets - 1; i++) {
		written = snprintf(&buf[offset], buf_size - offset,
			"%s,", mem);
		if (written < 0 || written + offset >= buf_size)
			return;
		offset += written;
	}

	written = snprintf(&buf[offset], buf_size - offset, "%s%s", mem,
		suffix);
	if (written < 0 || written + offset >= buf_size)
		return;
	offset += written;
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
	const char *argv0[] = {prgname, prefix, mp_flag,
			"-m", DEFAULT_MEM_SIZE};

	/* valid -m flag */
	const char *argv1[] = {prgname,
			"--file-prefix=" memtest, "-m", DEFAULT_MEM_SIZE};

	/* valid (zero) --socket-mem flag */
	char arg2_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv2[] = {prgname,
			"--file-prefix=" memtest, arg2_socket_mem};

	/* invalid (incomplete) --socket-mem flag */
	char arg3_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv3[] = {prgname,
			"--file-prefix=" memtest, arg3_socket_mem};

	/* invalid (mixed with invalid data) --socket-mem flag */
	char arg4_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv4[] = {prgname,
			"--file-prefix=" memtest, arg4_socket_mem};

	/* invalid (with numeric value as last character) --socket-mem flag */
	char arg5_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv5[] = {prgname,
			"--file-prefix=" memtest, arg5_socket_mem};

	/* invalid (with empty socket) --socket-mem flag */
	char arg6_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv6[] = {prgname,
			"--file-prefix=" memtest, arg6_socket_mem};

	/* invalid (null) --socket-mem flag */
	const char *argv7[] = {prgname,
			"--file-prefix=" memtest, "--socket-mem="};

	/* valid --socket-mem specified together with -m flag */
	char arg8_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv8[] = {prgname,
			"--file-prefix=" memtest, "-m", DEFAULT_MEM_SIZE,
			arg8_socket_mem};

#ifdef RTE_EXEC_ENV_FREEBSD
	int num_sockets = 1;
#else
	int num_sockets = RTE_MIN(get_number_of_sockets(),
			RTE_MAX_NUMA_NODES);
#endif

	if (num_sockets <= 0) {
		printf("Error - cannot get number of sockets!\n");
		return -1;
	}

	/* invalid --socket-mem flag (with extra socket) */
	char invalid_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv9[] = {prgname,
			"--file-prefix=" memtest, invalid_socket_mem};

	/* valid --socket-mem flag */
	char valid_socket_mem[SOCKET_MEM_STRLEN];
	const char *argv10[] = {prgname,
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

	populate_socket_mem_param(num_sockets, "0", "",
		arg2_socket_mem, sizeof(arg2_socket_mem));
	if (launch_proc(argv2) != 0) {
		printf("Error - process failed with valid (zero) --socket-mem!\n");
		return -1;
	}

	if (num_sockets > 1) {
		populate_socket_mem_param(num_sockets - 1, "2", ",",
			arg3_socket_mem, sizeof(arg3_socket_mem));
		if (launch_proc(argv3) == 0) {
			printf("Error - process run ok with invalid "
				"(incomplete) --socket-mem!\n");
			return -1;
		}

		populate_socket_mem_param(num_sockets - 1, "2", ",Fred",
			arg4_socket_mem, sizeof(arg4_socket_mem));
		if (launch_proc(argv4) == 0) {
			printf("Error - process run ok with invalid "
				"(mixed with invalid input) --socket-mem!\n");
			return -1;
		}

		populate_socket_mem_param(num_sockets - 1, "2", ",Fred0",
			arg5_socket_mem, sizeof(arg5_socket_mem));
		if (launch_proc(argv5) == 0) {
			printf("Error - process run ok with invalid "
				"(mixed with invalid input with a numeric value as "
				"last character) --socket-mem!\n");
			return -1;
		}
	}

	if (num_sockets > 2) {
		populate_socket_mem_param(num_sockets - 2, "2", ",,2",
			arg6_socket_mem, sizeof(arg6_socket_mem));
		if (launch_proc(argv6) == 0) {
			printf("Error - process run ok with invalid "
				"(with empty socket) --socket-mem!\n");
			return -1;
		}
	}

	if (launch_proc(argv7) == 0) {
		printf("Error - process run ok with invalid (null) --socket-mem!\n");
		return -1;
	}

	populate_socket_mem_param(num_sockets, "2", "",
		arg8_socket_mem, sizeof(arg8_socket_mem));
	if (launch_proc(argv8) == 0) {
		printf("Error - process run ok with --socket-mem and -m specified!\n");
		return -1;
	}

	populate_socket_mem_param(num_sockets + 1, "2", "",
		invalid_socket_mem, sizeof(invalid_socket_mem));
	if (launch_proc(argv9) == 0) {
		printf("Error - process run ok with extra socket in --socket-mem!\n");
		return -1;
	}

	populate_socket_mem_param(num_sockets, "2", "",
		valid_socket_mem, sizeof(valid_socket_mem));
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

	ret = test_main_lcore_flag();
	if (ret < 0) {
		printf("Error in test_main_lcore_flag()\n");
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

	ret = test_allow_flag();
	if (ret < 0) {
		printf("Error in test_allow_flag()\n");
		return ret;
	}

	ret = test_invalid_b_flag();
	if (ret < 0) {
		printf("Error in test_invalid_b_flag()\n");
		return ret;
	}

#ifdef RTE_NET_RING
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

/* subtests used in meson for CI */
REGISTER_TEST_COMMAND(eal_flags_c_opt_autotest, test_missing_c_flag);
REGISTER_TEST_COMMAND(eal_flags_main_opt_autotest, test_main_lcore_flag);
REGISTER_TEST_COMMAND(eal_flags_n_opt_autotest, test_invalid_n_flag);
REGISTER_TEST_COMMAND(eal_flags_hpet_autotest, test_no_hpet_flag);
REGISTER_TEST_COMMAND(eal_flags_no_huge_autotest, test_no_huge_flag);
REGISTER_TEST_COMMAND(eal_flags_a_opt_autotest, test_allow_flag);
REGISTER_TEST_COMMAND(eal_flags_b_opt_autotest, test_invalid_b_flag);
REGISTER_TEST_COMMAND(eal_flags_vdev_opt_autotest, test_invalid_vdev_flag);
REGISTER_TEST_COMMAND(eal_flags_r_opt_autotest, test_invalid_r_flag);
REGISTER_TEST_COMMAND(eal_flags_mem_autotest, test_memory_flags);
REGISTER_TEST_COMMAND(eal_flags_file_prefix_autotest, test_file_prefix);
REGISTER_TEST_COMMAND(eal_flags_misc_autotest, test_misc_flags);
