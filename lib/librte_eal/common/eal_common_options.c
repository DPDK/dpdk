/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation.
 * Copyright(c) 2014 6WIND S.A.
 */

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <dirent.h>

#include <rte_eal.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_tailq.h>
#include <rte_version.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>

#include "eal_internal_cfg.h"
#include "eal_options.h"
#include "eal_filesystem.h"
#include "eal_private.h"

#define BITS_PER_HEX 4
#define LCORE_OPT_LST 1
#define LCORE_OPT_MSK 2
#define LCORE_OPT_MAP 3

const char
eal_short_options[] =
	"b:" /* pci-blacklist */
	"c:" /* coremask */
	"s:" /* service coremask */
	"d:" /* driver */
	"h"  /* help */
	"l:" /* corelist */
	"S:" /* service corelist */
	"m:" /* memory size */
	"n:" /* memory channels */
	"r:" /* memory ranks */
	"v"  /* version */
	"w:" /* pci-whitelist */
	;

const struct option
eal_long_options[] = {
	{OPT_BASE_VIRTADDR,     1, NULL, OPT_BASE_VIRTADDR_NUM    },
	{OPT_CREATE_UIO_DEV,    0, NULL, OPT_CREATE_UIO_DEV_NUM   },
	{OPT_FILE_PREFIX,       1, NULL, OPT_FILE_PREFIX_NUM      },
	{OPT_HELP,              0, NULL, OPT_HELP_NUM             },
	{OPT_HUGE_DIR,          1, NULL, OPT_HUGE_DIR_NUM         },
	{OPT_HUGE_UNLINK,       0, NULL, OPT_HUGE_UNLINK_NUM      },
	{OPT_LCORES,            1, NULL, OPT_LCORES_NUM           },
	{OPT_LOG_LEVEL,         1, NULL, OPT_LOG_LEVEL_NUM        },
	{OPT_MASTER_LCORE,      1, NULL, OPT_MASTER_LCORE_NUM     },
	{OPT_MBUF_POOL_OPS_NAME, 1, NULL, OPT_MBUF_POOL_OPS_NAME_NUM},
	{OPT_NO_HPET,           0, NULL, OPT_NO_HPET_NUM          },
	{OPT_NO_HUGE,           0, NULL, OPT_NO_HUGE_NUM          },
	{OPT_NO_PCI,            0, NULL, OPT_NO_PCI_NUM           },
	{OPT_NO_SHCONF,         0, NULL, OPT_NO_SHCONF_NUM        },
	{OPT_PCI_BLACKLIST,     1, NULL, OPT_PCI_BLACKLIST_NUM    },
	{OPT_PCI_WHITELIST,     1, NULL, OPT_PCI_WHITELIST_NUM    },
	{OPT_PROC_TYPE,         1, NULL, OPT_PROC_TYPE_NUM        },
	{OPT_SOCKET_MEM,        1, NULL, OPT_SOCKET_MEM_NUM       },
	{OPT_SYSLOG,            1, NULL, OPT_SYSLOG_NUM           },
	{OPT_VDEV,              1, NULL, OPT_VDEV_NUM             },
	{OPT_VFIO_INTR,         1, NULL, OPT_VFIO_INTR_NUM        },
	{OPT_VMWARE_TSC_MAP,    0, NULL, OPT_VMWARE_TSC_MAP_NUM   },
	{OPT_LEGACY_MEM,        0, NULL, OPT_LEGACY_MEM_NUM       },
	{OPT_SINGLE_FILE_SEGMENTS, 0, NULL, OPT_SINGLE_FILE_SEGMENTS_NUM},
	{0,                     0, NULL, 0                        }
};

TAILQ_HEAD(shared_driver_list, shared_driver);

/* Definition for shared object drivers. */
struct shared_driver {
	TAILQ_ENTRY(shared_driver) next;

	char    name[PATH_MAX];
	void*   lib_handle;
};

/* List of external loadable drivers */
static struct shared_driver_list solib_list =
TAILQ_HEAD_INITIALIZER(solib_list);

/* Default path of external loadable drivers */
static const char *default_solib_dir = RTE_EAL_PMD_PATH;

/*
 * Stringified version of solib path used by dpdk-pmdinfo.py
 * Note: PLEASE DO NOT ALTER THIS without making a corresponding
 * change to usertools/dpdk-pmdinfo.py
 */
static const char dpdk_solib_path[] __attribute__((used)) =
"DPDK_PLUGIN_PATH=" RTE_EAL_PMD_PATH;

TAILQ_HEAD(device_option_list, device_option);

struct device_option {
	TAILQ_ENTRY(device_option) next;

	enum rte_devtype type;
	char arg[];
};

static struct device_option_list devopt_list =
TAILQ_HEAD_INITIALIZER(devopt_list);

static int master_lcore_parsed;
static int mem_parsed;
static int core_parsed;

static int
eal_option_device_add(enum rte_devtype type, const char *optarg)
{
	struct device_option *devopt;
	size_t optlen;
	int ret;

	optlen = strlen(optarg) + 1;
	devopt = calloc(1, sizeof(*devopt) + optlen);
	if (devopt == NULL) {
		RTE_LOG(ERR, EAL, "Unable to allocate device option\n");
		return -ENOMEM;
	}

	devopt->type = type;
	ret = snprintf(devopt->arg, optlen, "%s", optarg);
	if (ret < 0) {
		RTE_LOG(ERR, EAL, "Unable to copy device option\n");
		free(devopt);
		return -EINVAL;
	}
	TAILQ_INSERT_TAIL(&devopt_list, devopt, next);
	return 0;
}

int
eal_option_device_parse(void)
{
	struct device_option *devopt;
	void *tmp;
	int ret = 0;

	TAILQ_FOREACH_SAFE(devopt, &devopt_list, next, tmp) {
		if (ret == 0) {
			ret = rte_devargs_add(devopt->type, devopt->arg);
			if (ret)
				RTE_LOG(ERR, EAL, "Unable to parse device '%s'\n",
					devopt->arg);
		}
		TAILQ_REMOVE(&devopt_list, devopt, next);
		free(devopt);
	}
	return ret;
}

void
eal_reset_internal_config(struct internal_config *internal_cfg)
{
	int i;

	internal_cfg->memory = 0;
	internal_cfg->force_nrank = 0;
	internal_cfg->force_nchannel = 0;
	internal_cfg->hugefile_prefix = HUGEFILE_PREFIX_DEFAULT;
	internal_cfg->hugepage_dir = NULL;
	internal_cfg->force_sockets = 0;
	/* zero out the NUMA config */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
		internal_cfg->socket_mem[i] = 0;
	/* zero out hugedir descriptors */
	for (i = 0; i < MAX_HUGEPAGE_SIZES; i++) {
		memset(&internal_cfg->hugepage_info[i], 0,
				sizeof(internal_cfg->hugepage_info[0]));
		internal_cfg->hugepage_info[i].lock_descriptor = -1;
	}
	internal_cfg->base_virtaddr = 0;

	internal_cfg->syslog_facility = LOG_DAEMON;

	/* if set to NONE, interrupt mode is determined automatically */
	internal_cfg->vfio_intr_mode = RTE_INTR_MODE_NONE;

#ifdef RTE_LIBEAL_USE_HPET
	internal_cfg->no_hpet = 0;
#else
	internal_cfg->no_hpet = 1;
#endif
	internal_cfg->vmware_tsc_map = 0;
	internal_cfg->create_uio_dev = 0;
	internal_cfg->user_mbuf_pool_ops_name = NULL;
	internal_cfg->init_complete = 0;
}

static int
eal_plugin_add(const char *path)
{
	struct shared_driver *solib;

	solib = malloc(sizeof(*solib));
	if (solib == NULL) {
		RTE_LOG(ERR, EAL, "malloc(solib) failed\n");
		return -1;
	}
	memset(solib, 0, sizeof(*solib));
	strncpy(solib->name, path, PATH_MAX-1);
	solib->name[PATH_MAX-1] = 0;
	TAILQ_INSERT_TAIL(&solib_list, solib, next);

	return 0;
}

static int
eal_plugindir_init(const char *path)
{
	DIR *d = NULL;
	struct dirent *dent = NULL;
	char sopath[PATH_MAX];

	if (path == NULL || *path == '\0')
		return 0;

	d = opendir(path);
	if (d == NULL) {
		RTE_LOG(ERR, EAL, "failed to open directory %s: %s\n",
			path, strerror(errno));
		return -1;
	}

	while ((dent = readdir(d)) != NULL) {
		struct stat sb;

		snprintf(sopath, PATH_MAX-1, "%s/%s", path, dent->d_name);
		sopath[PATH_MAX-1] = 0;

		if (!(stat(sopath, &sb) == 0 && S_ISREG(sb.st_mode)))
			continue;

		if (eal_plugin_add(sopath) == -1)
			break;
	}

	closedir(d);
	/* XXX this ignores failures from readdir() itself */
	return (dent == NULL) ? 0 : -1;
}

int
eal_plugins_init(void)
{
	struct shared_driver *solib = NULL;
	struct stat sb;

	if (*default_solib_dir != '\0' && stat(default_solib_dir, &sb) == 0 &&
				S_ISDIR(sb.st_mode))
		eal_plugin_add(default_solib_dir);

	TAILQ_FOREACH(solib, &solib_list, next) {

		if (stat(solib->name, &sb) == 0 && S_ISDIR(sb.st_mode)) {
			if (eal_plugindir_init(solib->name) == -1) {
				RTE_LOG(ERR, EAL,
					"Cannot init plugin directory %s\n",
					solib->name);
				return -1;
			}
		} else {
			RTE_LOG(DEBUG, EAL, "open shared lib %s\n",
				solib->name);
			solib->lib_handle = dlopen(solib->name, RTLD_NOW);
			if (solib->lib_handle == NULL) {
				RTE_LOG(ERR, EAL, "%s\n", dlerror());
				return -1;
			}
		}

	}
	return 0;
}

/*
 * Parse the coremask given as argument (hexadecimal string) and fill
 * the global configuration (core role and core count) with the parsed
 * value.
 */
static int xdigit2val(unsigned char c)
{
	int val;

	if (isdigit(c))
		val = c - '0';
	else if (isupper(c))
		val = c - 'A' + 10;
	else
		val = c - 'a' + 10;
	return val;
}

static int
eal_parse_service_coremask(const char *coremask)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	int i, j, idx = 0;
	unsigned int count = 0;
	char c;
	int val;

	if (coremask == NULL)
		return -1;
	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*coremask))
		coremask++;
	if (coremask[0] == '0' && ((coremask[1] == 'x')
		|| (coremask[1] == 'X')))
		coremask += 2;
	i = strlen(coremask);
	while ((i > 0) && isblank(coremask[i - 1]))
		i--;

	if (i == 0)
		return -1;

	for (i = i - 1; i >= 0 && idx < RTE_MAX_LCORE; i--) {
		c = coremask[i];
		if (isxdigit(c) == 0) {
			/* invalid characters */
			return -1;
		}
		val = xdigit2val(c);
		for (j = 0; j < BITS_PER_HEX && idx < RTE_MAX_LCORE;
				j++, idx++) {
			if ((1 << j) & val) {
				/* handle master lcore already parsed */
				uint32_t lcore = idx;
				if (master_lcore_parsed &&
						cfg->master_lcore == lcore) {
					RTE_LOG(ERR, EAL,
						"Error: lcore %u is master lcore, cannot use as service core\n",
						idx);
					return -1;
				}

				if (!lcore_config[idx].detected) {
					RTE_LOG(ERR, EAL,
						"lcore %u unavailable\n", idx);
					return -1;
				}
				lcore_config[idx].core_role = ROLE_SERVICE;
				count++;
			}
		}
	}

	for (; i >= 0; i--)
		if (coremask[i] != '0')
			return -1;

	for (; idx < RTE_MAX_LCORE; idx++)
		lcore_config[idx].core_index = -1;

	if (count == 0)
		return -1;

	cfg->service_lcore_count = count;
	return 0;
}

static int
eal_parse_coremask(const char *coremask)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	int i, j, idx = 0;
	unsigned count = 0;
	char c;
	int val;

	if (coremask == NULL)
		return -1;
	/* Remove all blank characters ahead and after .
	 * Remove 0x/0X if exists.
	 */
	while (isblank(*coremask))
		coremask++;
	if (coremask[0] == '0' && ((coremask[1] == 'x')
		|| (coremask[1] == 'X')))
		coremask += 2;
	i = strlen(coremask);
	while ((i > 0) && isblank(coremask[i - 1]))
		i--;
	if (i == 0)
		return -1;

	for (i = i - 1; i >= 0 && idx < RTE_MAX_LCORE; i--) {
		c = coremask[i];
		if (isxdigit(c) == 0) {
			/* invalid characters */
			return -1;
		}
		val = xdigit2val(c);
		for (j = 0; j < BITS_PER_HEX && idx < RTE_MAX_LCORE; j++, idx++)
		{
			if ((1 << j) & val) {
				if (!lcore_config[idx].detected) {
					RTE_LOG(ERR, EAL, "lcore %u "
					        "unavailable\n", idx);
					return -1;
				}
				cfg->lcore_role[idx] = ROLE_RTE;
				lcore_config[idx].core_index = count;
				count++;
			} else {
				cfg->lcore_role[idx] = ROLE_OFF;
				lcore_config[idx].core_index = -1;
			}
		}
	}
	for (; i >= 0; i--)
		if (coremask[i] != '0')
			return -1;
	for (; idx < RTE_MAX_LCORE; idx++) {
		cfg->lcore_role[idx] = ROLE_OFF;
		lcore_config[idx].core_index = -1;
	}
	if (count == 0)
		return -1;
	/* Update the count of enabled logical cores of the EAL configuration */
	cfg->lcore_count = count;
	return 0;
}

static int
eal_parse_service_corelist(const char *corelist)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	int i, idx = 0;
	unsigned count = 0;
	char *end = NULL;
	int min, max;

	if (corelist == NULL)
		return -1;

	/* Remove all blank characters ahead and after */
	while (isblank(*corelist))
		corelist++;
	i = strlen(corelist);
	while ((i > 0) && isblank(corelist[i - 1]))
		i--;

	/* Get list of cores */
	min = RTE_MAX_LCORE;
	do {
		while (isblank(*corelist))
			corelist++;
		if (*corelist == '\0')
			return -1;
		errno = 0;
		idx = strtoul(corelist, &end, 10);
		if (errno || end == NULL)
			return -1;
		while (isblank(*end))
			end++;
		if (*end == '-') {
			min = idx;
		} else if ((*end == ',') || (*end == '\0')) {
			max = idx;
			if (min == RTE_MAX_LCORE)
				min = idx;
			for (idx = min; idx <= max; idx++) {
				if (cfg->lcore_role[idx] != ROLE_SERVICE) {
					/* handle master lcore already parsed */
					uint32_t lcore = idx;
					if (cfg->master_lcore == lcore &&
							master_lcore_parsed) {
						RTE_LOG(ERR, EAL,
							"Error: lcore %u is master lcore, cannot use as service core\n",
							idx);
						return -1;
					}
					lcore_config[idx].core_role =
							ROLE_SERVICE;
					count++;
				}
			}
			min = RTE_MAX_LCORE;
		} else
			return -1;
		corelist = end + 1;
	} while (*end != '\0');

	if (count == 0)
		return -1;

	return 0;
}

static int
eal_parse_corelist(const char *corelist)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	int i, idx = 0;
	unsigned count = 0;
	char *end = NULL;
	int min, max;

	if (corelist == NULL)
		return -1;

	/* Remove all blank characters ahead and after */
	while (isblank(*corelist))
		corelist++;
	i = strlen(corelist);
	while ((i > 0) && isblank(corelist[i - 1]))
		i--;

	/* Reset config */
	for (idx = 0; idx < RTE_MAX_LCORE; idx++) {
		cfg->lcore_role[idx] = ROLE_OFF;
		lcore_config[idx].core_index = -1;
	}

	/* Get list of cores */
	min = RTE_MAX_LCORE;
	do {
		while (isblank(*corelist))
			corelist++;
		if (*corelist == '\0')
			return -1;
		errno = 0;
		idx = strtoul(corelist, &end, 10);
		if (errno || end == NULL)
			return -1;
		while (isblank(*end))
			end++;
		if (*end == '-') {
			min = idx;
		} else if ((*end == ',') || (*end == '\0')) {
			max = idx;
			if (min == RTE_MAX_LCORE)
				min = idx;
			for (idx = min; idx <= max; idx++) {
				if (cfg->lcore_role[idx] != ROLE_RTE) {
					cfg->lcore_role[idx] = ROLE_RTE;
					lcore_config[idx].core_index = count;
					count++;
				}
			}
			min = RTE_MAX_LCORE;
		} else
			return -1;
		corelist = end + 1;
	} while (*end != '\0');

	if (count == 0)
		return -1;

	/* Update the count of enabled logical cores of the EAL configuration */
	cfg->lcore_count = count;

	return 0;
}

/* Changes the lcore id of the master thread */
static int
eal_parse_master_lcore(const char *arg)
{
	char *parsing_end;
	struct rte_config *cfg = rte_eal_get_configuration();

	errno = 0;
	cfg->master_lcore = (uint32_t) strtol(arg, &parsing_end, 0);
	if (errno || parsing_end[0] != 0)
		return -1;
	if (cfg->master_lcore >= RTE_MAX_LCORE)
		return -1;
	master_lcore_parsed = 1;

	/* ensure master core is not used as service core */
	if (lcore_config[cfg->master_lcore].core_role == ROLE_SERVICE) {
		RTE_LOG(ERR, EAL, "Error: Master lcore is used as a service core.\n");
		return -1;
	}

	return 0;
}

/*
 * Parse elem, the elem could be single number/range or '(' ')' group
 * 1) A single number elem, it's just a simple digit. e.g. 9
 * 2) A single range elem, two digits with a '-' between. e.g. 2-6
 * 3) A group elem, combines multiple 1) or 2) with '( )'. e.g (0,2-4,6)
 *    Within group elem, '-' used for a range separator;
 *                       ',' used for a single number.
 */
static int
eal_parse_set(const char *input, uint16_t set[], unsigned num)
{
	unsigned idx;
	const char *str = input;
	char *end = NULL;
	unsigned min, max;

	memset(set, 0, num * sizeof(uint16_t));

	while (isblank(*str))
		str++;

	/* only digit or left bracket is qualify for start point */
	if ((!isdigit(*str) && *str != '(') || *str == '\0')
		return -1;

	/* process single number or single range of number */
	if (*str != '(') {
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || end == NULL || idx >= num)
			return -1;
		else {
			while (isblank(*end))
				end++;

			min = idx;
			max = idx;
			if (*end == '-') {
				/* process single <number>-<number> */
				end++;
				while (isblank(*end))
					end++;
				if (!isdigit(*end))
					return -1;

				errno = 0;
				idx = strtoul(end, &end, 10);
				if (errno || end == NULL || idx >= num)
					return -1;
				max = idx;
				while (isblank(*end))
					end++;
				if (*end != ',' && *end != '\0')
					return -1;
			}

			if (*end != ',' && *end != '\0' &&
			    *end != '@')
				return -1;

			for (idx = RTE_MIN(min, max);
			     idx <= RTE_MAX(min, max); idx++)
				set[idx] = 1;

			return end - input;
		}
	}

	/* process set within bracket */
	str++;
	while (isblank(*str))
		str++;
	if (*str == '\0')
		return -1;

	min = RTE_MAX_LCORE;
	do {

		/* go ahead to the first digit */
		while (isblank(*str))
			str++;
		if (!isdigit(*str))
			return -1;

		/* get the digit value */
		errno = 0;
		idx = strtoul(str, &end, 10);
		if (errno || end == NULL || idx >= num)
			return -1;

		/* go ahead to separator '-',',' and ')' */
		while (isblank(*end))
			end++;
		if (*end == '-') {
			if (min == RTE_MAX_LCORE)
				min = idx;
			else /* avoid continuous '-' */
				return -1;
		} else if ((*end == ',') || (*end == ')')) {
			max = idx;
			if (min == RTE_MAX_LCORE)
				min = idx;
			for (idx = RTE_MIN(min, max);
			     idx <= RTE_MAX(min, max); idx++)
				set[idx] = 1;

			min = RTE_MAX_LCORE;
		} else
			return -1;

		str = end + 1;
	} while (*end != '\0' && *end != ')');

	/*
	 * to avoid failure that tail blank makes end character check fail
	 * in eal_parse_lcores( )
	 */
	while (isblank(*str))
		str++;

	return str - input;
}

/* convert from set array to cpuset bitmap */
static int
convert_to_cpuset(rte_cpuset_t *cpusetp,
	      uint16_t *set, unsigned num)
{
	unsigned idx;

	CPU_ZERO(cpusetp);

	for (idx = 0; idx < num; idx++) {
		if (!set[idx])
			continue;

		if (!lcore_config[idx].detected) {
			RTE_LOG(ERR, EAL, "core %u "
				"unavailable\n", idx);
			return -1;
		}

		CPU_SET(idx, cpusetp);
	}

	return 0;
}

/*
 * The format pattern: --lcores='<lcores[@cpus]>[<,lcores[@cpus]>...]'
 * lcores, cpus could be a single digit/range or a group.
 * '(' and ')' are necessary if it's a group.
 * If not supply '@cpus', the value of cpus uses the same as lcores.
 * e.g. '1,2@(5-7),(3-5)@(0,2),(0,6),7-8' means start 9 EAL thread as below
 *   lcore 0 runs on cpuset 0x41 (cpu 0,6)
 *   lcore 1 runs on cpuset 0x2 (cpu 1)
 *   lcore 2 runs on cpuset 0xe0 (cpu 5,6,7)
 *   lcore 3,4,5 runs on cpuset 0x5 (cpu 0,2)
 *   lcore 6 runs on cpuset 0x41 (cpu 0,6)
 *   lcore 7 runs on cpuset 0x80 (cpu 7)
 *   lcore 8 runs on cpuset 0x100 (cpu 8)
 */
static int
eal_parse_lcores(const char *lcores)
{
	struct rte_config *cfg = rte_eal_get_configuration();
	static uint16_t set[RTE_MAX_LCORE];
	unsigned idx = 0;
	unsigned count = 0;
	const char *lcore_start = NULL;
	const char *end = NULL;
	int offset;
	rte_cpuset_t cpuset;
	int lflags;
	int ret = -1;

	if (lcores == NULL)
		return -1;

	/* Remove all blank characters ahead and after */
	while (isblank(*lcores))
		lcores++;

	CPU_ZERO(&cpuset);

	/* Reset lcore config */
	for (idx = 0; idx < RTE_MAX_LCORE; idx++) {
		cfg->lcore_role[idx] = ROLE_OFF;
		lcore_config[idx].core_index = -1;
		CPU_ZERO(&lcore_config[idx].cpuset);
	}

	/* Get list of cores */
	do {
		while (isblank(*lcores))
			lcores++;
		if (*lcores == '\0')
			goto err;

		lflags = 0;

		/* record lcore_set start point */
		lcore_start = lcores;

		/* go across a complete bracket */
		if (*lcore_start == '(') {
			lcores += strcspn(lcores, ")");
			if (*lcores++ == '\0')
				goto err;
		}

		/* scan the separator '@', ','(next) or '\0'(finish) */
		lcores += strcspn(lcores, "@,");

		if (*lcores == '@') {
			/* explicit assign cpu_set */
			offset = eal_parse_set(lcores + 1, set, RTE_DIM(set));
			if (offset < 0)
				goto err;

			/* prepare cpu_set and update the end cursor */
			if (0 > convert_to_cpuset(&cpuset,
						  set, RTE_DIM(set)))
				goto err;
			end = lcores + 1 + offset;
		} else { /* ',' or '\0' */
			/* haven't given cpu_set, current loop done */
			end = lcores;

			/* go back to check <number>-<number> */
			offset = strcspn(lcore_start, "(-");
			if (offset < (end - lcore_start) &&
			    *(lcore_start + offset) != '(')
				lflags = 1;
		}

		if (*end != ',' && *end != '\0')
			goto err;

		/* parse lcore_set from start point */
		if (0 > eal_parse_set(lcore_start, set, RTE_DIM(set)))
			goto err;

		/* without '@', by default using lcore_set as cpu_set */
		if (*lcores != '@' &&
		    0 > convert_to_cpuset(&cpuset, set, RTE_DIM(set)))
			goto err;

		/* start to update lcore_set */
		for (idx = 0; idx < RTE_MAX_LCORE; idx++) {
			if (!set[idx])
				continue;

			if (cfg->lcore_role[idx] != ROLE_RTE) {
				lcore_config[idx].core_index = count;
				cfg->lcore_role[idx] = ROLE_RTE;
				count++;
			}

			if (lflags) {
				CPU_ZERO(&cpuset);
				CPU_SET(idx, &cpuset);
			}
			rte_memcpy(&lcore_config[idx].cpuset, &cpuset,
				   sizeof(rte_cpuset_t));
		}

		lcores = end + 1;
	} while (*end != '\0');

	if (count == 0)
		goto err;

	cfg->lcore_count = count;
	ret = 0;

err:

	return ret;
}

static int
eal_parse_syslog(const char *facility, struct internal_config *conf)
{
	int i;
	static const struct {
		const char *name;
		int value;
	} map[] = {
		{ "auth", LOG_AUTH },
		{ "cron", LOG_CRON },
		{ "daemon", LOG_DAEMON },
		{ "ftp", LOG_FTP },
		{ "kern", LOG_KERN },
		{ "lpr", LOG_LPR },
		{ "mail", LOG_MAIL },
		{ "news", LOG_NEWS },
		{ "syslog", LOG_SYSLOG },
		{ "user", LOG_USER },
		{ "uucp", LOG_UUCP },
		{ "local0", LOG_LOCAL0 },
		{ "local1", LOG_LOCAL1 },
		{ "local2", LOG_LOCAL2 },
		{ "local3", LOG_LOCAL3 },
		{ "local4", LOG_LOCAL4 },
		{ "local5", LOG_LOCAL5 },
		{ "local6", LOG_LOCAL6 },
		{ "local7", LOG_LOCAL7 },
		{ NULL, 0 }
	};

	for (i = 0; map[i].name; i++) {
		if (!strcmp(facility, map[i].name)) {
			conf->syslog_facility = map[i].value;
			return 0;
		}
	}
	return -1;
}

static int
eal_parse_log_priority(const char *level)
{
	static const char * const levels[] = {
		[RTE_LOG_EMERG]   = "emergency",
		[RTE_LOG_ALERT]   = "alert",
		[RTE_LOG_CRIT]    = "critical",
		[RTE_LOG_ERR]     = "error",
		[RTE_LOG_WARNING] = "warning",
		[RTE_LOG_NOTICE]  = "notice",
		[RTE_LOG_INFO]    = "info",
		[RTE_LOG_DEBUG]   = "debug",
	};
	size_t len = strlen(level);
	unsigned long tmp;
	char *end;
	unsigned int i;

	if (len == 0)
		return -1;

	/* look for named values, skip 0 which is not a valid level */
	for (i = 1; i < RTE_DIM(levels); i++) {
		if (strncmp(levels[i], level, len) == 0)
			return i;
	}

	/* not a string, maybe it is numeric */
	errno = 0;
	tmp = strtoul(level, &end, 0);

	/* check for errors */
	if (errno != 0 || end == NULL || *end != '\0' ||
	    tmp >= UINT32_MAX)
		return -1;

	return tmp;
}

static int
eal_parse_log_level(const char *arg)
{
	const char *pattern = NULL;
	const char *regex = NULL;
	char *str, *level;
	int priority;

	str = strdup(arg);
	if (str == NULL)
		return -1;

	if ((level = strchr(str, ','))) {
		regex = str;
		*level++ = '\0';
	} else if ((level = strchr(str, ':'))) {
		pattern = str;
		*level++ = '\0';
	} else {
		level = str;
	}

	priority = eal_parse_log_priority(level);
	if (priority < 0) {
		fprintf(stderr, "invalid log priority: %s\n", level);
		goto fail;
	}

	if (regex) {
		if (rte_log_set_level_regexp(regex, priority) < 0) {
			fprintf(stderr, "cannot set log level %s,%d\n",
				pattern, priority);
			goto fail;
		}
		if (rte_log_save_regexp(regex, priority) < 0)
			goto fail;
	} else if (pattern) {
		if (rte_log_set_level_pattern(pattern, priority) < 0) {
			fprintf(stderr, "cannot set log level %s:%d\n",
				pattern, priority);
			goto fail;
		}
		if (rte_log_save_pattern(pattern, priority) < 0)
			goto fail;
	} else {
		rte_log_set_global_level(priority);
	}

	free(str);
	return 0;

fail:
	free(str);
	return -1;
}

static enum rte_proc_type_t
eal_parse_proc_type(const char *arg)
{
	if (strncasecmp(arg, "primary", sizeof("primary")) == 0)
		return RTE_PROC_PRIMARY;
	if (strncasecmp(arg, "secondary", sizeof("secondary")) == 0)
		return RTE_PROC_SECONDARY;
	if (strncasecmp(arg, "auto", sizeof("auto")) == 0)
		return RTE_PROC_AUTO;

	return RTE_PROC_INVALID;
}

int
eal_parse_common_option(int opt, const char *optarg,
			struct internal_config *conf)
{
	static int b_used;
	static int w_used;

	switch (opt) {
	/* blacklist */
	case 'b':
		if (w_used)
			goto bw_used;
		if (eal_option_device_add(RTE_DEVTYPE_BLACKLISTED_PCI,
				optarg) < 0) {
			return -1;
		}
		b_used = 1;
		break;
	/* whitelist */
	case 'w':
		if (b_used)
			goto bw_used;
		if (eal_option_device_add(RTE_DEVTYPE_WHITELISTED_PCI,
				optarg) < 0) {
			return -1;
		}
		w_used = 1;
		break;
	/* coremask */
	case 'c':
		if (eal_parse_coremask(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid coremask\n");
			return -1;
		}

		if (core_parsed) {
			RTE_LOG(ERR, EAL, "Option -c is ignored, because (%s) is set!\n",
				(core_parsed == LCORE_OPT_LST) ? "-l" :
				(core_parsed == LCORE_OPT_MAP) ? "--lcore" :
				"-c");
			return -1;
		}

		core_parsed = LCORE_OPT_MSK;
		break;
	/* corelist */
	case 'l':
		if (eal_parse_corelist(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid core list\n");
			return -1;
		}

		if (core_parsed) {
			RTE_LOG(ERR, EAL, "Option -l is ignored, because (%s) is set!\n",
				(core_parsed == LCORE_OPT_MSK) ? "-c" :
				(core_parsed == LCORE_OPT_MAP) ? "--lcore" :
				"-l");
			return -1;
		}

		core_parsed = LCORE_OPT_LST;
		break;
	/* service coremask */
	case 's':
		if (eal_parse_service_coremask(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid service coremask\n");
			return -1;
		}
		break;
	/* service corelist */
	case 'S':
		if (eal_parse_service_corelist(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid service core list\n");
			return -1;
		}
		break;
	/* size of memory */
	case 'm':
		conf->memory = atoi(optarg);
		conf->memory *= 1024ULL;
		conf->memory *= 1024ULL;
		mem_parsed = 1;
		break;
	/* force number of channels */
	case 'n':
		conf->force_nchannel = atoi(optarg);
		if (conf->force_nchannel == 0) {
			RTE_LOG(ERR, EAL, "invalid channel number\n");
			return -1;
		}
		break;
	/* force number of ranks */
	case 'r':
		conf->force_nrank = atoi(optarg);
		if (conf->force_nrank == 0 ||
		    conf->force_nrank > 16) {
			RTE_LOG(ERR, EAL, "invalid rank number\n");
			return -1;
		}
		break;
	/* force loading of external driver */
	case 'd':
		if (eal_plugin_add(optarg) == -1)
			return -1;
		break;
	case 'v':
		/* since message is explicitly requested by user, we
		 * write message at highest log level so it can always
		 * be seen
		 * even if info or warning messages are disabled */
		RTE_LOG(CRIT, EAL, "RTE Version: '%s'\n", rte_version());
		break;

	/* long options */
	case OPT_HUGE_UNLINK_NUM:
		conf->hugepage_unlink = 1;
		break;

	case OPT_NO_HUGE_NUM:
		conf->no_hugetlbfs = 1;
		/* no-huge is legacy mem */
		conf->legacy_mem = 1;
		break;

	case OPT_NO_PCI_NUM:
		conf->no_pci = 1;
		break;

	case OPT_NO_HPET_NUM:
		conf->no_hpet = 1;
		break;

	case OPT_VMWARE_TSC_MAP_NUM:
		conf->vmware_tsc_map = 1;
		break;

	case OPT_NO_SHCONF_NUM:
		conf->no_shconf = 1;
		break;

	case OPT_PROC_TYPE_NUM:
		conf->process_type = eal_parse_proc_type(optarg);
		break;

	case OPT_MASTER_LCORE_NUM:
		if (eal_parse_master_lcore(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid parameter for --"
					OPT_MASTER_LCORE "\n");
			return -1;
		}
		break;

	case OPT_VDEV_NUM:
		if (eal_option_device_add(RTE_DEVTYPE_VIRTUAL,
				optarg) < 0) {
			return -1;
		}
		break;

	case OPT_SYSLOG_NUM:
		if (eal_parse_syslog(optarg, conf) < 0) {
			RTE_LOG(ERR, EAL, "invalid parameters for --"
					OPT_SYSLOG "\n");
			return -1;
		}
		break;

	case OPT_LOG_LEVEL_NUM: {
		if (eal_parse_log_level(optarg) < 0) {
			RTE_LOG(ERR, EAL,
				"invalid parameters for --"
				OPT_LOG_LEVEL "\n");
			return -1;
		}
		break;
	}
	case OPT_LCORES_NUM:
		if (eal_parse_lcores(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid parameter for --"
				OPT_LCORES "\n");
			return -1;
		}

		if (core_parsed) {
			RTE_LOG(ERR, EAL, "Option --lcore is ignored, because (%s) is set!\n",
				(core_parsed == LCORE_OPT_LST) ? "-l" :
				(core_parsed == LCORE_OPT_MSK) ? "-c" :
				"--lcore");
			return -1;
		}

		core_parsed = LCORE_OPT_MAP;
		break;
	case OPT_LEGACY_MEM_NUM:
		conf->legacy_mem = 1;
		break;
	case OPT_SINGLE_FILE_SEGMENTS_NUM:
		conf->single_file_segments = 1;
		break;

	/* don't know what to do, leave this to caller */
	default:
		return 1;

	}

	return 0;
bw_used:
	RTE_LOG(ERR, EAL, "Options blacklist (-b) and whitelist (-w) "
		"cannot be used at the same time\n");
	return -1;
}

static void
eal_auto_detect_cores(struct rte_config *cfg)
{
	unsigned int lcore_id;
	unsigned int removed = 0;
	rte_cpuset_t affinity_set;
	pthread_t tid = pthread_self();

	if (pthread_getaffinity_np(tid, sizeof(rte_cpuset_t),
				&affinity_set) < 0)
		CPU_ZERO(&affinity_set);

	for (lcore_id = 0; lcore_id < RTE_MAX_LCORE; lcore_id++) {
		if (cfg->lcore_role[lcore_id] == ROLE_RTE &&
		    !CPU_ISSET(lcore_id, &affinity_set)) {
			cfg->lcore_role[lcore_id] = ROLE_OFF;
			removed++;
		}
	}

	cfg->lcore_count -= removed;
}

int
eal_adjust_config(struct internal_config *internal_cfg)
{
	int i;
	struct rte_config *cfg = rte_eal_get_configuration();

	if (!core_parsed)
		eal_auto_detect_cores(cfg);

	if (internal_config.process_type == RTE_PROC_AUTO)
		internal_config.process_type = eal_proc_type_detect();

	/* default master lcore is the first one */
	if (!master_lcore_parsed) {
		cfg->master_lcore = rte_get_next_lcore(-1, 0, 0);
		lcore_config[cfg->master_lcore].core_role = ROLE_RTE;
	}

	/* if no memory amounts were requested, this will result in 0 and
	 * will be overridden later, right after eal_hugepage_info_init() */
	for (i = 0; i < RTE_MAX_NUMA_NODES; i++)
		internal_cfg->memory += internal_cfg->socket_mem[i];

	return 0;
}

int
eal_check_common_options(struct internal_config *internal_cfg)
{
	struct rte_config *cfg = rte_eal_get_configuration();

	if (cfg->lcore_role[cfg->master_lcore] != ROLE_RTE) {
		RTE_LOG(ERR, EAL, "Master lcore is not enabled for DPDK\n");
		return -1;
	}

	if (internal_cfg->process_type == RTE_PROC_INVALID) {
		RTE_LOG(ERR, EAL, "Invalid process type specified\n");
		return -1;
	}
	if (index(internal_cfg->hugefile_prefix, '%') != NULL) {
		RTE_LOG(ERR, EAL, "Invalid char, '%%', in --"OPT_FILE_PREFIX" "
			"option\n");
		return -1;
	}
	if (mem_parsed && internal_cfg->force_sockets == 1) {
		RTE_LOG(ERR, EAL, "Options -m and --"OPT_SOCKET_MEM" cannot "
			"be specified at the same time\n");
		return -1;
	}
	if (internal_cfg->no_hugetlbfs && internal_cfg->force_sockets == 1) {
		RTE_LOG(ERR, EAL, "Option --"OPT_SOCKET_MEM" cannot "
			"be specified together with --"OPT_NO_HUGE"\n");
		return -1;
	}

	if (internal_cfg->no_hugetlbfs && internal_cfg->hugepage_unlink) {
		RTE_LOG(ERR, EAL, "Option --"OPT_HUGE_UNLINK" cannot "
			"be specified together with --"OPT_NO_HUGE"\n");
		return -1;
	}

	return 0;
}

void
eal_common_usage(void)
{
	printf("[options]\n\n"
	       "EAL common options:\n"
	       "  -c COREMASK         Hexadecimal bitmask of cores to run on\n"
	       "  -l CORELIST         List of cores to run on\n"
	       "                      The argument format is <c1>[-c2][,c3[-c4],...]\n"
	       "                      where c1, c2, etc are core indexes between 0 and %d\n"
	       "  --"OPT_LCORES" COREMAP    Map lcore set to physical cpu set\n"
	       "                      The argument format is\n"
	       "                            '<lcores[@cpus]>[<,lcores[@cpus]>...]'\n"
	       "                      lcores and cpus list are grouped by '(' and ')'\n"
	       "                      Within the group, '-' is used for range separator,\n"
	       "                      ',' is used for single number separator.\n"
	       "                      '( )' can be omitted for single element group,\n"
	       "                      '@' can be omitted if cpus and lcores have the same value\n"
	       "  -s SERVICE COREMASK Hexadecimal bitmask of cores to be used as service cores\n"
	       "  --"OPT_MASTER_LCORE" ID   Core ID that is used as master\n"
	       "  --"OPT_MBUF_POOL_OPS_NAME" Pool ops name for mbuf to use\n"
	       "  -n CHANNELS         Number of memory channels\n"
	       "  -m MB               Memory to allocate (see also --"OPT_SOCKET_MEM")\n"
	       "  -r RANKS            Force number of memory ranks (don't detect)\n"
	       "  -b, --"OPT_PCI_BLACKLIST" Add a PCI device in black list.\n"
	       "                      Prevent EAL from using this PCI device. The argument\n"
	       "                      format is <domain:bus:devid.func>.\n"
	       "  -w, --"OPT_PCI_WHITELIST" Add a PCI device in white list.\n"
	       "                      Only use the specified PCI devices. The argument format\n"
	       "                      is <[domain:]bus:devid.func>. This option can be present\n"
	       "                      several times (once per device).\n"
	       "                      [NOTE: PCI whitelist cannot be used with -b option]\n"
	       "  --"OPT_VDEV"              Add a virtual device.\n"
	       "                      The argument format is <driver><id>[,key=val,...]\n"
	       "                      (ex: --vdev=net_pcap0,iface=eth2).\n"
	       "  -d LIB.so|DIR       Add a driver or driver directory\n"
	       "                      (can be used multiple times)\n"
	       "  --"OPT_VMWARE_TSC_MAP"    Use VMware TSC map instead of native RDTSC\n"
	       "  --"OPT_PROC_TYPE"         Type of this process (primary|secondary|auto)\n"
	       "  --"OPT_SYSLOG"            Set syslog facility\n"
	       "  --"OPT_LOG_LEVEL"=<int>   Set global log level\n"
	       "  --"OPT_LOG_LEVEL"=<type-match>:<int>\n"
	       "                      Set specific log level\n"
	       "  -v                  Display version information on startup\n"
	       "  -h, --help          This help\n"
	       "\nEAL options for DEBUG use only:\n"
	       "  --"OPT_HUGE_UNLINK"       Unlink hugepage files after init\n"
	       "  --"OPT_NO_HUGE"           Use malloc instead of hugetlbfs\n"
	       "  --"OPT_NO_PCI"            Disable PCI\n"
	       "  --"OPT_NO_HPET"           Disable HPET\n"
	       "  --"OPT_NO_SHCONF"         No shared config (mmap'd files)\n"
	       "\n", RTE_MAX_LCORE);
}
