/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016 Intel Corporation. All rights reserved.
 *   All rights reserved.
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

#include <getopt.h>
#include <inttypes.h>
#include <limits.h>
#include <sched.h>
#include <signal.h>
#include <stdio.h>

#include <rte_common.h>
#include <rte_memcpy.h>

#include <pqos.h>

#include "cat.h"

#define BITS_PER_HEX		4
#define PQOS_MAX_SOCKETS	8
#define PQOS_MAX_SOCKET_CORES	64
#define PQOS_MAX_CORES		(PQOS_MAX_SOCKET_CORES * PQOS_MAX_SOCKETS)

static const struct pqos_cap *m_cap;
static const struct pqos_cpuinfo *m_cpu;
static const struct pqos_capability *m_cap_l3ca;
static unsigned m_sockets[PQOS_MAX_SOCKETS];
static unsigned m_sock_count;
static struct cat_config m_config[PQOS_MAX_CORES];
static unsigned m_config_count;

static unsigned
bits_count(uint64_t bitmask)
{
	unsigned count = 0;

	for (; bitmask != 0; count++)
		bitmask &= bitmask - 1;

	return count;
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
parse_set(const char *input, rte_cpuset_t *cpusetp)
{
	unsigned idx;
	const char *str = input;
	char *end = NULL;
	unsigned min, max;
	const unsigned num = PQOS_MAX_CORES;

	CPU_ZERO(cpusetp);

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

		if (*end != ',' && *end != '\0' && *end != '@')
			return -1;

		for (idx = RTE_MIN(min, max); idx <= RTE_MAX(min, max);
				idx++)
			CPU_SET(idx, cpusetp);

		return end - input;
	}

	/* process set within bracket */
	str++;
	while (isblank(*str))
		str++;
	if (*str == '\0')
		return -1;

	min = PQOS_MAX_CORES;
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
			if (min == PQOS_MAX_CORES)
				min = idx;
			else /* avoid continuous '-' */
				return -1;
		} else if ((*end == ',') || (*end == ')')) {
			max = idx;
			if (min == PQOS_MAX_CORES)
				min = idx;
			for (idx = RTE_MIN(min, max); idx <= RTE_MAX(min, max);
					idx++)
				CPU_SET(idx, cpusetp);

			min = PQOS_MAX_CORES;
		} else
			return -1;

		str = end + 1;
	} while (*end != '\0' && *end != ')');

	return str - input;
}

/* Test if bitmask is contiguous */
static int
is_contiguous(uint64_t bitmask)
{
	/* check if bitmask is contiguous */
	unsigned i = 0;
	unsigned j = 0;
	const unsigned max_idx = (sizeof(bitmask) * CHAR_BIT);

	if (bitmask == 0)
		return 0;

	for (i = 0; i < max_idx; i++) {
		if (((1ULL << i) & bitmask) != 0)
			j++;
		else if (j > 0)
			break;
	}

	if (bits_count(bitmask) != j) {
		printf("PQOS: mask 0x%llx is not contiguous.\n",
			(unsigned long long)bitmask);
		return 0;
	}

	return 1;
}

/*
 * The format pattern: --l3ca='<cbm@cpus>[,<(ccbm,dcbm)@cpus>...]'
 * cbm could be a single mask or for a CDP enabled system, a group of two masks
 * ("code cbm" and "data cbm")
 * '(' and ')' are necessary if it's a group.
 * cpus could be a single digit/range or a group.
 * '(' and ')' are necessary if it's a group.
 *
 * e.g. '0x00F00@(1,3), 0x0FF00@(4-6), 0xF0000@7'
 * - CPUs 1 and 3 share its 4 ways with CPUs 4, 5 and 6;
 * - CPUs 4,5 and 6 share half (4 out of 8 ways) of its L3 with 1 and 3;
 * - CPUs 4,5 and 6 have exclusive access to 4 out of  8 ways;
 * - CPU 7 has exclusive access to all of its 4 ways;
 *
 * e.g. '(0x00C00,0x00300)@(1,3)' for a CDP enabled system
 * - cpus 1 and 3 have access to 2 ways for code and 2 ways for data,
 *   code and data ways are not overlapping.;
 */
static int
parse_l3ca(const char *l3ca)
{
	unsigned idx = 0;
	const char *cbm_start = NULL;
	char *cbm_end = NULL;
	const char *end = NULL;
	int offset;
	rte_cpuset_t cpuset;
	uint64_t mask = 0;
	uint64_t cmask = 0;

	if (l3ca == NULL)
		goto err;

	/* Get cbm */
	do {
		CPU_ZERO(&cpuset);
		mask = 0;
		cmask = 0;

		while (isblank(*l3ca))
			l3ca++;

		if (*l3ca == '\0')
			goto err;

		/* record mask_set start point */
		cbm_start = l3ca;

		/* go across a complete bracket */
		if (*cbm_start == '(') {
			l3ca += strcspn(l3ca, ")");
			if (*l3ca++ == '\0')
				goto err;
		}

		/* scan the separator '@', ','(next) or '\0'(finish) */
		l3ca += strcspn(l3ca, "@,");

		if (*l3ca == '@') {
			/* explicit assign cpu_set */
			offset = parse_set(l3ca + 1, &cpuset);
			if (offset < 0 || CPU_COUNT(&cpuset) == 0)
				goto err;

			end = l3ca + 1 + offset;
		} else
			goto err;

		if (*end != ',' && *end != '\0')
			goto err;

		/* parse mask_set from start point */
		if (*cbm_start == '(') {
			cbm_start++;

			while (isblank(*cbm_start))
				cbm_start++;

			if (!isxdigit(*cbm_start))
				goto err;

			errno = 0;
			cmask = strtoul(cbm_start, &cbm_end, 16);
			if (errno != 0 || cbm_end == NULL || cmask == 0)
				goto err;

			while (isblank(*cbm_end))
				cbm_end++;

			if (*cbm_end != ',')
				goto err;

			cbm_end++;

			while (isblank(*cbm_end))
				cbm_end++;

			if (!isxdigit(*cbm_end))
				goto err;

			errno = 0;
			mask = strtoul(cbm_end, &cbm_end, 16);
			if (errno != 0 || cbm_end == NULL || mask == 0)
				goto err;
		} else {
			while (isblank(*cbm_start))
				cbm_start++;

			if (!isxdigit(*cbm_start))
				goto err;

			errno = 0;
			mask = strtoul(cbm_start, &cbm_end, 16);
			if (errno != 0 || cbm_end == NULL || mask == 0)
				goto err;

		}

		if (mask == 0 || is_contiguous(mask) == 0)
			goto err;

		if (cmask != 0 && is_contiguous(cmask) == 0)
			goto err;

		rte_memcpy(&m_config[idx].cpumask,
			&cpuset, sizeof(rte_cpuset_t));

		if (cmask != 0) {
			m_config[idx].cdp = 1;
			m_config[idx].code_mask = cmask;
			m_config[idx].data_mask = mask;
		} else
			m_config[idx].mask = mask;

		m_config_count++;

		l3ca = end + 1;
		idx++;
	} while (*end != '\0' && idx < PQOS_MAX_CORES);

	if (m_config_count == 0)
		goto err;

	return 0;

err:
	return -EINVAL;
}

static int
check_cpus_overlapping(void)
{
	unsigned i = 0;
	unsigned j = 0;
	rte_cpuset_t mask;

	CPU_ZERO(&mask);

	for (i = 0; i < m_config_count; i++) {
		for (j = i + 1; j < m_config_count; j++) {
			CPU_AND(&mask,
				&m_config[i].cpumask,
				&m_config[j].cpumask);

			if (CPU_COUNT(&mask) != 0) {
				printf("PQOS: Requested CPUs sets are "
					"overlapping.\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

static int
check_cpus(void)
{
	unsigned i = 0;
	unsigned cpu_id = 0;
	unsigned cos_id = 0;
	int ret = 0;

	for (i = 0; i < m_config_count; i++) {
		for (cpu_id = 0; cpu_id < PQOS_MAX_CORES; cpu_id++) {
			if (CPU_ISSET(cpu_id, &m_config[i].cpumask) != 0) {

				ret = pqos_cpu_check_core(m_cpu, cpu_id);
				if (ret != PQOS_RETVAL_OK) {
					printf("PQOS: %u is not a valid "
						"logical core id.\n", cpu_id);
					ret = -ENODEV;
					goto exit;
				}

				ret = pqos_l3ca_assoc_get(cpu_id, &cos_id);
				if (ret != PQOS_RETVAL_OK) {
					printf("PQOS: Failed to read COS "
						"associated to cpu %u.\n",
						cpu_id);
					ret = -EFAULT;
					goto exit;
				}

				/*
				 * Check if COS assigned to lcore is different
				 * then default one (#0)
				 */
				if (cos_id != 0) {
					printf("PQOS: cpu %u has already "
						"associated COS#%u. "
						"Please reset L3CA.\n",
						cpu_id, cos_id);
					ret = -EBUSY;
					goto exit;
				}
			}
		}
	}

exit:
	return ret;
}

static int
check_cdp(void)
{
	unsigned i = 0;

	for (i = 0; i < m_config_count; i++) {
		if (m_config[i].cdp == 1 && m_cap_l3ca->u.l3ca->cdp_on == 0) {
			if (m_cap_l3ca->u.l3ca->cdp == 0) {
				printf("PQOS: CDP requested but not "
					"supported.\n");
			} else {
				printf("PQOS: CDP requested but not enabled. "
					"Please enable CDP.\n");
			}
			return -ENOTSUP;
		}
	}

	return 0;
}

static int
check_cbm_len_and_contention(void)
{
	unsigned i = 0;
	uint64_t mask = 0;
	const uint64_t not_cbm = (UINT64_MAX << (m_cap_l3ca->u.l3ca->num_ways));
	const uint64_t cbm_contention_mask = m_cap_l3ca->u.l3ca->way_contention;
	int ret = 0;

	for (i = 0; i < m_config_count; i++) {
		if (m_config[i].cdp == 1)
			mask = m_config[i].code_mask | m_config[i].data_mask;
		else
			mask = m_config[i].mask;

		if ((mask & not_cbm) != 0) {
			printf("PQOS: One or more of requested CBM masks not "
				"supported by system (too long).\n");
			ret = -ENOTSUP;
			break;
		}

		/* Just a warning */
		if ((mask & cbm_contention_mask) != 0) {
			printf("PQOS: One or more of requested CBM  masks "
				"overlap CBM contention mask.\n");
			break;
		}

	}

	return ret;
}

static int
check_and_select_classes(unsigned cos_id_map[][PQOS_MAX_SOCKETS])
{
	unsigned i = 0;
	unsigned j = 0;
	unsigned phy_pkg_id = 0;
	unsigned cos_id = 0;
	unsigned cpu_id = 0;
	unsigned phy_pkg_lcores[PQOS_MAX_SOCKETS][m_config_count];
	const unsigned cos_num = m_cap_l3ca->u.l3ca->num_classes;
	unsigned used_cos_table[PQOS_MAX_SOCKETS][cos_num];
	int ret = 0;

	memset(phy_pkg_lcores, 0, sizeof(phy_pkg_lcores));
	memset(used_cos_table, 0, sizeof(used_cos_table));

	/* detect currently used COS */
	for (j = 0; j < m_cpu->num_cores; j++) {
		cpu_id = m_cpu->cores[j].lcore;

		ret = pqos_l3ca_assoc_get(cpu_id, &cos_id);
		if (ret != PQOS_RETVAL_OK) {
			printf("PQOS: Failed to read COS associated to "
				"cpu %u on phy_pkg %u.\n", cpu_id, phy_pkg_id);
			ret = -EFAULT;
			goto exit;
		}

		ret = pqos_cpu_get_socketid(m_cpu, cpu_id, &phy_pkg_id);
		if (ret != PQOS_RETVAL_OK) {
			printf("PQOS: Failed to get socket for cpu %u\n",
				cpu_id);
			ret = -EFAULT;
			goto exit;
		}

		/* Mark COS as used */
		if (used_cos_table[phy_pkg_id][cos_id] == 0)
			used_cos_table[phy_pkg_id][cos_id]++;
	}

	/* look for avail. COS to fulfill requested config */
	for (i = 0; i < m_config_count; i++) {
		for (j = 0; j < m_cpu->num_cores; j++) {
			cpu_id = m_cpu->cores[j].lcore;
			if (CPU_ISSET(cpu_id, &m_config[i].cpumask) == 0)
				continue;

			ret = pqos_cpu_get_socketid(m_cpu, cpu_id, &phy_pkg_id);
			if (ret != PQOS_RETVAL_OK) {
				printf("PQOS: Failed to get socket for "
					"cpu %u\n", cpu_id);
				ret = -EFAULT;
				goto exit;
			}

			/*
			 * Check if we already have COS selected
			 * to be used for that group on that socket
			 */
			if (phy_pkg_lcores[phy_pkg_id][i] != 0)
				continue;

			phy_pkg_lcores[phy_pkg_id][i]++;

			/* Search for avail. COS to be used on that socket */
			for (cos_id = 0; cos_id < cos_num; cos_id++) {
				if (used_cos_table[phy_pkg_id][cos_id] == 0) {
					used_cos_table[phy_pkg_id][cos_id]++;
					cos_id_map[i][phy_pkg_id] = cos_id;
					break;
				}
			}

			/* If there is no COS available ...*/
			if (cos_id == cos_num) {
				ret = -E2BIG;
				goto exit;
			}
		}
	}

exit:
	if (ret != 0)
		printf("PQOS: Not enough available COS to configure "
			"requested configuration.\n");

	return ret;
}

static int
configure_cat(unsigned cos_id_map[][PQOS_MAX_SOCKETS])
{
	unsigned phy_pkg_id = 0;
	unsigned cpu_id = 0;
	unsigned cos_id = 0;
	unsigned i = 0;
	unsigned j = 0;
	struct pqos_l3ca l3ca = {0};
	int ret = 0;

	for (i = 0; i < m_config_count; i++) {
		memset(&l3ca, 0, sizeof(l3ca));

		l3ca.cdp = m_config[i].cdp;
		if (m_config[i].cdp == 1) {
			l3ca.code_mask = m_config[i].code_mask;
			l3ca.data_mask = m_config[i].data_mask;
		} else
			l3ca.ways_mask = m_config[i].mask;

		for (j = 0; j < m_sock_count; j++) {
			phy_pkg_id = m_sockets[j];
			if (cos_id_map[i][phy_pkg_id] == 0)
				continue;

			l3ca.class_id = cos_id_map[i][phy_pkg_id];

			ret = pqos_l3ca_set(phy_pkg_id, 1, &l3ca);
			if (ret != PQOS_RETVAL_OK) {
				printf("PQOS: Failed to set COS %u on "
					"phy_pkg %u.\n", l3ca.class_id,
					phy_pkg_id);
				ret = -EFAULT;
				goto exit;
			}
		}
	}

	for (i = 0; i < m_config_count; i++) {
		for (j = 0; j < m_cpu->num_cores; j++) {
			cpu_id = m_cpu->cores[j].lcore;
			if (CPU_ISSET(cpu_id, &m_config[i].cpumask) == 0)
				continue;

			ret = pqos_cpu_get_socketid(m_cpu, cpu_id, &phy_pkg_id);
			if (ret != PQOS_RETVAL_OK) {
				printf("PQOS: Failed to get socket for "
					"cpu %u\n", cpu_id);
				ret = -EFAULT;
				goto exit;
			}

			cos_id = cos_id_map[i][phy_pkg_id];

			ret = pqos_l3ca_assoc_set(cpu_id, cos_id);
			if (ret != PQOS_RETVAL_OK) {
				printf("PQOS: Failed to associate COS %u to "
					"cpu %u\n", cos_id, cpu_id);
				ret = -EFAULT;
				goto exit;
			}
		}
	}

exit:
	return ret;
}


/* Parse the argument given in the command line of the application */
static int
parse_args(int argc, char **argv)
{
	int opt = 0;
	int retval = 0;
	int oldopterr = 0;
	char **argvopt = argv;
	char *prgname = argv[0];

	static struct option lgopts[] = {
		{ "l3ca", required_argument, 0, 0 },
		{ NULL, 0, 0, 0 }
	};

	/* Disable printing messages within getopt() */
	oldopterr = opterr;
	opterr = 0;

	opt = getopt_long(argc, argvopt, "", lgopts, NULL);
	if (opt == 0) {
		retval = parse_l3ca(optarg);
		if (retval != 0) {
			printf("PQOS: Invalid L3CA parameters!\n");
			goto exit;
		}

		argv[optind - 1] = prgname;
		retval = optind - 1;
	} else
		retval = 0;

exit:
	/* reset getopt lib */
	optind = 0;

	/* Restore opterr value */
	opterr = oldopterr;

	return retval;
}

static void
print_cmd_line_config(void)
{
	char cpustr[PQOS_MAX_CORES * 3] = {0};
	unsigned i = 0;
	unsigned j = 0;

	for (i = 0; i < m_config_count; i++) {
		unsigned len = 0;
		memset(cpustr, 0, sizeof(cpustr));

		/* Generate CPU list */
		for (j = 0; j < PQOS_MAX_CORES; j++) {
			if (CPU_ISSET(j, &m_config[i].cpumask) != 1)
				continue;

			len += snprintf(cpustr + len, sizeof(cpustr) - len - 1,
				"%u,", j);

			if (len >= sizeof(cpustr) - 1)
				break;
		}

		if (m_config[i].cdp == 1) {
			printf("PQOS: CPUs: %s cMASK: 0x%llx, dMASK: "
				"0x%llx\n", cpustr,
				(unsigned long long)m_config[i].code_mask,
				(unsigned long long)m_config[i].data_mask);
		} else {
			printf("PQOS: CPUs: %s MASK: 0x%llx\n", cpustr,
					(unsigned long long)m_config[i].mask);
		}
	}
}

/**
 * @brief Prints CAT configuration
 */
static void
print_cat_config(void)
{
	int ret = PQOS_RETVAL_OK;
	unsigned i = 0;

	for (i = 0; i < m_sock_count; i++) {
		struct pqos_l3ca tab[PQOS_MAX_L3CA_COS] = {{0} };
		unsigned num = 0;
		unsigned n = 0;

		ret = pqos_l3ca_get(m_sockets[i], PQOS_MAX_L3CA_COS, &num, tab);
		if (ret != PQOS_RETVAL_OK) {
			printf("PQOS: Error retrieving COS!\n");
			return;
		}

		printf("PQOS: COS definitions for Socket %u:\n", m_sockets[i]);
		for (n = 0; n < num; n++) {
			if (tab[n].cdp == 1) {
				printf("PQOS: COS: %u, cMASK: 0x%llx, "
					"dMASK: 0x%llx\n", tab[n].class_id,
					(unsigned long long)tab[n].code_mask,
					(unsigned long long)tab[n].data_mask);
			} else {
				printf("PQOS: COS: %u, MASK: 0x%llx\n",
					tab[n].class_id,
					(unsigned long long)tab[n].ways_mask);
			}
		}
	}

	for (i = 0; i < m_sock_count; i++) {
		unsigned lcores[PQOS_MAX_SOCKET_CORES] = {0};
		unsigned lcount = 0;
		unsigned n = 0;

		ret = pqos_cpu_get_cores(m_cpu, m_sockets[i],
				PQOS_MAX_SOCKET_CORES, &lcount, &lcores[0]);
		if (ret != PQOS_RETVAL_OK) {
			printf("PQOS: Error retrieving core information!\n");
			return;
		}

		printf("PQOS: CPU information for socket %u:\n", m_sockets[i]);
		for (n = 0; n < lcount; n++) {
			unsigned class_id = 0;

			ret = pqos_l3ca_assoc_get(lcores[n], &class_id);
			if (ret == PQOS_RETVAL_OK)
				printf("PQOS: CPU: %u, COS: %u\n", lcores[n],
					class_id);
			else
				printf("PQOS: CPU: %u, ERROR\n", lcores[n]);
		}
	}

}

static int
cat_validate(void)
{
	int ret = 0;

	ret = check_cpus();
	if (ret != 0)
		return ret;

	ret = check_cdp();
	if (ret != 0)
		return ret;

	ret = check_cbm_len_and_contention();
	if (ret != 0)
		return ret;

	ret = check_cpus_overlapping();
	if (ret != 0)
		return ret;

	return 0;
}

static int
cat_set(void)
{
	int ret = 0;
	unsigned cos_id_map[m_config_count][PQOS_MAX_SOCKETS];

	memset(cos_id_map, 0, sizeof(cos_id_map));

	ret = check_and_select_classes(cos_id_map);
	if (ret != 0)
		return ret;

	ret = configure_cat(cos_id_map);
	if (ret != 0)
		return ret;

	return 0;
}

static void
cat_fini(void)
{
	int ret = 0;

	printf("PQOS: Shutting down PQoS library...\n");

	/* deallocate all the resources */
	ret = pqos_fini();
	if (ret != PQOS_RETVAL_OK && ret != PQOS_RETVAL_INIT)
		printf("PQOS: Error shutting down PQoS library!\n");

	m_cap = NULL;
	m_cpu = NULL;
	m_cap_l3ca = NULL;
	memset(m_sockets, 0, sizeof(m_sockets));
	m_sock_count = 0;
	memset(m_config, 0, sizeof(m_config));
	m_config_count = 0;
}

void
cat_exit(void)
{
	unsigned i = 0;
	unsigned j = 0;
	unsigned cpu_id = 0;
	int ret = 0;

	/* if lib is not initialized, do nothing */
	if (m_cap == NULL && m_cpu == NULL)
		return;

	printf("PQOS: Reverting CAT configuration...\n");

	for (i = 0; i < m_config_count; i++) {
		for (j = 0; j < m_cpu->num_cores; j++) {
			cpu_id = m_cpu->cores[j].lcore;
			if (CPU_ISSET(cpu_id, &m_config[i].cpumask) == 0)
				continue;

			ret = pqos_l3ca_assoc_set(cpu_id, 0);
			if (ret != PQOS_RETVAL_OK) {
				printf("PQOS: Failed to associate COS 0 to "
					"cpu %u\n", cpu_id);
			}
		}
	}

	cat_fini();
}

static void
signal_handler(int signum)
{
	if (signum == SIGINT || signum == SIGTERM) {
		printf("\nPQOS: Signal %d received, preparing to exit...\n",
				signum);

		cat_exit();

		/* exit with the expected status */
		signal(signum, SIG_DFL);
		kill(getpid(), signum);
	}
}

int
cat_init(int argc, char **argv)
{
	int ret = 0;
	int args_num = 0;
	struct pqos_config cfg = {0};

	if (m_cap != NULL || m_cpu != NULL) {
		printf("PQOS: CAT module already initialized!\n");
		return -EEXIST;
	}

	/* Parse cmd line args */
	ret = parse_args(argc, argv);

	if (ret <= 0)
		goto err;

	args_num = ret;

	/* Print cmd line configuration */
	print_cmd_line_config();

	/* PQoS Initialization - Check and initialize CAT capability */
	cfg.fd_log = STDOUT_FILENO;
	cfg.verbose = 0;
	cfg.cdp_cfg = PQOS_REQUIRE_CDP_ANY;
	ret = pqos_init(&cfg);
	if (ret != PQOS_RETVAL_OK) {
		printf("PQOS: Error initializing PQoS library!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Get capability and CPU info pointer */
	ret = pqos_cap_get(&m_cap, &m_cpu);
	if (ret != PQOS_RETVAL_OK || m_cap == NULL || m_cpu == NULL) {
		printf("PQOS: Error retrieving PQoS capabilities!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Get L3CA capabilities */
	ret = pqos_cap_get_type(m_cap, PQOS_CAP_TYPE_L3CA, &m_cap_l3ca);
	if (ret != PQOS_RETVAL_OK || m_cap_l3ca == NULL) {
		printf("PQOS: Error retrieving PQOS_CAP_TYPE_L3CA "
			"capabilities!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Get CPU socket information */
	ret = pqos_cpu_get_sockets(m_cpu, PQOS_MAX_SOCKETS, &m_sock_count,
		m_sockets);
	if (ret != PQOS_RETVAL_OK) {
		printf("PQOS: Error retrieving CPU socket information!\n");
		ret = -EFAULT;
		goto err;
	}

	/* Validate cmd line configuration */
	ret = cat_validate();
	if (ret != 0) {
		printf("PQOS: Requested CAT configuration is not valid!\n");
		goto err;
	}

	/* configure system */
	ret = cat_set();
	if (ret != 0) {
		printf("PQOS: Failed to configure CAT!\n");
		goto err;
	}

	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	ret = atexit(cat_exit);
	if (ret != 0) {
		printf("PQOS: Cannot set exit function\n");
		goto err;
	}

	/* Print CAT configuration */
	print_cat_config();

	return args_num;

err:
	/* deallocate all the resources */
	cat_fini();
	return ret;
}
