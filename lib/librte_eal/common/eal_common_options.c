/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   Copyright(c) 2014 6WIND S.A.
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

#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>
#include <ctype.h>
#include <limits.h>
#include <errno.h>
#include <getopt.h>

#include <rte_eal.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_version.h>
#include <rte_devargs.h>
#include <rte_memcpy.h>

#include "eal_internal_cfg.h"
#include "eal_options.h"
#include "eal_filesystem.h"

#define BITS_PER_HEX 4

const char
eal_short_options[] =
	"b:" /* pci-blacklist */
	"c:" /* coremask */
	"d:" /* driver */
	"h"  /* help */
	"l:" /* corelist */
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
	{OPT_LCORES,            1, NULL, OPT_LCORES_NUM           },
	{OPT_LOG_LEVEL,         1, NULL, OPT_LOG_LEVEL_NUM        },
	{OPT_MASTER_LCORE,      1, NULL, OPT_MASTER_LCORE_NUM     },
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
	{OPT_XEN_DOM0,          0, NULL, OPT_XEN_DOM0_NUM         },
	{0,                     0, NULL, 0                        }
};

static int lcores_parsed;
static int master_lcore_parsed;
static int mem_parsed;

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
	for (i = 0; i < MAX_HUGEPAGE_SIZES; i++)
		internal_cfg->hugepage_info[i].lock_descriptor = -1;
	internal_cfg->base_virtaddr = 0;

	internal_cfg->syslog_facility = LOG_DAEMON;
	/* default value from build option */
	internal_cfg->log_level = RTE_LOG_LEVEL;

	internal_cfg->xen_dom0_support = 0;

	/* if set to NONE, interrupt mode is determined automatically */
	internal_cfg->vfio_intr_mode = RTE_INTR_MODE_NONE;

#ifdef RTE_LIBEAL_USE_HPET
	internal_cfg->no_hpet = 0;
#else
	internal_cfg->no_hpet = 1;
#endif
	internal_cfg->vmware_tsc_map = 0;
	internal_cfg->create_uio_dev = 0;
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
	lcores_parsed = 1;
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

	lcores_parsed = 1;
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
	int i;
	unsigned count = 0;
	const char *lcore_start = NULL;
	const char *end = NULL;
	int offset;
	rte_cpuset_t cpuset;
	int lflags = 0;
	int ret = -1;

	if (lcores == NULL)
		return -1;

	/* Remove all blank characters ahead and after */
	while (isblank(*lcores))
		lcores++;
	i = strlen(lcores);
	while ((i > 0) && isblank(lcores[i - 1]))
		i--;

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
	lcores_parsed = 1;
	ret = 0;

err:

	return ret;
}

static int
eal_parse_syslog(const char *facility, struct internal_config *conf)
{
	int i;
	static struct {
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
eal_parse_log_level(const char *level, uint32_t *log_level)
{
	char *end;
	unsigned long tmp;

	errno = 0;
	tmp = strtoul(level, &end, 0);

	/* check for errors */
	if ((errno != 0) || (level[0] == '\0') ||
	    end == NULL || (*end != '\0'))
		return -1;

	/* log_level is a uint32_t */
	if (tmp >= UINT32_MAX)
		return -1;

	*log_level = tmp;
	return 0;
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
	switch (opt) {
	/* blacklist */
	case 'b':
		if (rte_eal_devargs_add(RTE_DEVTYPE_BLACKLISTED_PCI,
				optarg) < 0) {
			return -1;
		}
		break;
	/* whitelist */
	case 'w':
		if (rte_eal_devargs_add(RTE_DEVTYPE_WHITELISTED_PCI,
				optarg) < 0) {
			return -1;
		}
		break;
	/* coremask */
	case 'c':
		if (eal_parse_coremask(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid coremask\n");
			return -1;
		}
		break;
	/* corelist */
	case 'l':
		if (eal_parse_corelist(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid core list\n");
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
		if (conf->force_nchannel == 0 ||
		    conf->force_nchannel > 4) {
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
	case 'v':
		/* since message is explicitly requested by user, we
		 * write message at highest log level so it can always
		 * be seen
		 * even if info or warning messages are disabled */
		RTE_LOG(CRIT, EAL, "RTE Version: '%s'\n", rte_version());
		break;

	/* long options */
	case OPT_NO_HUGE_NUM:
		conf->no_hugetlbfs = 1;
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
		if (rte_eal_devargs_add(RTE_DEVTYPE_VIRTUAL,
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
		uint32_t log;

		if (eal_parse_log_level(optarg, &log) < 0) {
			RTE_LOG(ERR, EAL,
				"invalid parameters for --"
				OPT_LOG_LEVEL "\n");
			return -1;
		}
		conf->log_level = log;
		break;
	}
	case OPT_LCORES_NUM:
		if (eal_parse_lcores(optarg) < 0) {
			RTE_LOG(ERR, EAL, "invalid parameter for --"
				OPT_LCORES "\n");
			return -1;
		}
		break;

	/* don't know what to do, leave this to caller */
	default:
		return 1;

	}

	return 0;
}

int
eal_adjust_config(struct internal_config *internal_cfg)
{
	int i;
	struct rte_config *cfg = rte_eal_get_configuration();

	if (internal_config.process_type == RTE_PROC_AUTO)
		internal_config.process_type = eal_proc_type_detect();

	/* default master lcore is the first one */
	if (!master_lcore_parsed)
		cfg->master_lcore = rte_get_next_lcore(-1, 0, 0);

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

	if (!lcores_parsed) {
		RTE_LOG(ERR, EAL, "CPU cores must be enabled with options "
			"-c, -l or --lcores\n");
		return -1;
	}
	if (cfg->lcore_role[cfg->master_lcore] != ROLE_RTE) {
		RTE_LOG(ERR, EAL, "Master lcore is not enabled for DPDK\n");
		return -1;
	}

	if (internal_cfg->process_type == RTE_PROC_INVALID) {
		RTE_LOG(ERR, EAL, "Invalid process type specified\n");
		return -1;
	}
	if (internal_cfg->process_type == RTE_PROC_PRIMARY &&
			internal_cfg->force_nchannel == 0) {
		RTE_LOG(ERR, EAL, "Number of memory channels (-n) not "
			"specified\n");
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
	if (internal_cfg->no_hugetlbfs &&
			(mem_parsed || internal_cfg->force_sockets == 1)) {
		RTE_LOG(ERR, EAL, "Options -m or --"OPT_SOCKET_MEM" cannot "
			"be specified together with --"OPT_NO_HUGE"\n");
		return -1;
	}

	if (rte_eal_devargs_type_count(RTE_DEVTYPE_WHITELISTED_PCI) != 0 &&
		rte_eal_devargs_type_count(RTE_DEVTYPE_BLACKLISTED_PCI) != 0) {
		RTE_LOG(ERR, EAL, "Options blacklist (-b) and whitelist (-w) "
			"cannot be used at the same time\n");
		return -1;
	}

	return 0;
}

void
eal_common_usage(void)
{
	printf("-c COREMASK|-l CORELIST -n CHANNELS [options]\n\n"
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
	       "  --"OPT_MASTER_LCORE" ID   Core ID that is used as master\n"
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
	       "                      (ex: --vdev=eth_pcap0,iface=eth2).\n"
	       "  --"OPT_VMWARE_TSC_MAP"    Use VMware TSC map instead of native RDTSC\n"
	       "  --"OPT_PROC_TYPE"         Type of this process (primary|secondary|auto)\n"
	       "  --"OPT_SYSLOG"            Set syslog facility\n"
	       "  --"OPT_LOG_LEVEL"         Set default log level\n"
	       "  -v                  Display version information on startup\n"
	       "  -h, --help          This help\n"
	       "\nEAL options for DEBUG use only:\n"
	       "  --"OPT_NO_HUGE"           Use malloc instead of hugetlbfs\n"
	       "  --"OPT_NO_PCI"            Disable PCI\n"
	       "  --"OPT_NO_HPET"           Disable HPET\n"
	       "  --"OPT_NO_SHCONF"         No shared config (mmap'd files)\n"
	       "\n", RTE_MAX_LCORE);
}
