/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <getopt.h>
#include <string.h>
#include <arpa/inet.h>
#include <sys/socket.h>

#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ip.h>
#include <rte_random.h>
#include <rte_malloc.h>
#include <rte_lpm.h>
#include <rte_lpm6.h>
#include <rte_fib.h>
#include <rte_fib6.h>

#define	PRINT_USAGE_START	"%s [EAL options] --\n"

#define GET_CB_FIELD(in, fd, base, lim, dlm)	do {		\
	unsigned long val;					\
	char *end_fld;						\
	errno = 0;						\
	val = strtoul((in), &end_fld, (base));			\
	if (errno != 0 || end_fld[0] != (dlm) || val > (lim))	\
		return -EINVAL;					\
	(fd) = (typeof(fd))val;					\
	(in) = end_fld + 1;					\
} while (0)

#define	DEF_ROUTES_NUM		0x10000
#define	DEF_LOOKUP_IPS_NUM	0x100000
#define BURST_SZ		64
#define DEFAULT_LPM_TBL8	100000U

#define CMP_FLAG		(1 << 0)
#define CMP_ALL_FLAG		(1 << 1)
#define IPV6_FLAG		(1 << 2)
#define FIB_RIB_TYPE		(1 << 3)
#define FIB_V4_DIR_TYPE		(1 << 4)
#define FIB_V6_TRIE_TYPE	(1 << 4)
#define FIB_TYPE_MASK		(FIB_RIB_TYPE|FIB_V4_DIR_TYPE|FIB_V6_TRIE_TYPE)
#define SHUFFLE_FLAG		(1 << 7)
#define DRY_RUN_FLAG		(1 << 8)

static char *distrib_string;
static char line[LINE_MAX];

enum {
	RT_PREFIX,
	RT_NEXTHOP,
	RT_NUM
};

#ifndef NIPQUAD
#define NIPQUAD_FMT "%u.%u.%u.%u"
#define NIPQUAD(addr)				\
	(unsigned)((unsigned char *)&addr)[3],	\
	(unsigned)((unsigned char *)&addr)[2],	\
	(unsigned)((unsigned char *)&addr)[1],	\
	(unsigned)((unsigned char *)&addr)[0]

#define NIPQUAD6_FMT "%04x:%04x:%04x:%04x:%04x:%04x:%04x:%04x"
#define NIPQUAD6(addr)				\
	((uint8_t *)addr)[0] << 8 |	\
	((uint8_t *)addr)[1],		\
	((uint8_t *)addr)[2] << 8 |	\
	((uint8_t *)addr)[3],		\
	((uint8_t *)addr)[4] << 8 |	\
	((uint8_t *)addr)[5],		\
	((uint8_t *)addr)[6] << 8 |	\
	((uint8_t *)addr)[7],		\
	((uint8_t *)addr)[8] << 8 |	\
	((uint8_t *)addr)[9],		\
	((uint8_t *)addr)[10] << 8 |	\
	((uint8_t *)addr)[11],		\
	((uint8_t *)addr)[12] << 8 |	\
	((uint8_t *)addr)[13],		\
	((uint8_t *)addr)[14] << 8 |	\
	((uint8_t *)addr)[15]
#endif

static struct {
	const char	*prgname;
	const char	*routes_file;
	const char	*lookup_ips_file;
	const char	*routes_file_s;
	const char	*lookup_ips_file_s;
	void		*rt;
	void		*lookup_tbl;
	uint32_t	nb_routes;
	uint32_t	nb_lookup_ips;
	uint32_t	nb_lookup_ips_rnd;
	uint32_t	nb_routes_per_depth[128 + 1];
	uint32_t	flags;
	uint32_t	tbl8;
	uint8_t		ent_sz;
	uint8_t		rnd_lookup_ips_ratio;
	uint8_t		print_fract;
	uint8_t		lookup_fn;
} config = {
	.routes_file = NULL,
	.lookup_ips_file = NULL,
	.nb_routes = DEF_ROUTES_NUM,
	.nb_lookup_ips = DEF_LOOKUP_IPS_NUM,
	.nb_lookup_ips_rnd = 0,
	.nb_routes_per_depth = {0},
	.flags = FIB_V4_DIR_TYPE,
	.tbl8 = DEFAULT_LPM_TBL8,
	.ent_sz = 4,
	.rnd_lookup_ips_ratio = 0,
	.print_fract = 10,
	.lookup_fn = 0
};

struct rt_rule_4 {
	uint32_t	addr;
	uint8_t		depth;
	uint64_t	nh;
};

struct rt_rule_6 {
	uint8_t		addr[16];
	uint8_t		depth;
	uint64_t	nh;
};

static uint64_t
get_rnd_rng(uint64_t l, uint64_t u)
{
	if (l == u)
		return l;
	else
		return (rte_rand() % (u - l) + l);
}

static __rte_always_inline __attribute__((pure)) uint8_t
bits_in_nh(uint8_t nh_sz)
{
	return 8 * (1 << nh_sz);
}

static  __rte_always_inline __attribute__((pure)) uint64_t
get_max_nh(uint8_t nh_sz)
{
	/* min between fib and lpm6 which is 21 bits */
	return RTE_MIN(((1ULL << (bits_in_nh(nh_sz) - 1)) - 1),
			(1ULL << 21) - 1);
}

static int
get_fib_type(void)
{
	if (config.flags & IPV6_FLAG) {
		if ((config.flags & FIB_TYPE_MASK) == FIB_V6_TRIE_TYPE)
			return RTE_FIB6_TRIE;
		else
			return RTE_FIB6_DUMMY;
	} else {
		if ((config.flags & FIB_TYPE_MASK) == FIB_V4_DIR_TYPE)
			return RTE_FIB_DIR24_8;
		if ((config.flags & FIB_TYPE_MASK) == FIB_RIB_TYPE)
			return RTE_FIB_DUMMY;
	}
	return -1;
}

static int
complete_distrib(uint8_t depth_lim, const uint32_t n, uint8_t rpd[],
	uint32_t nrpd[])
{
	uint8_t depth;
	uint32_t nr = 0;
	uint8_t m = 0;

	/*
	 * complete number of routes for every depth
	 * that was configured with ratio
	 */
	for (depth = 0; depth <= depth_lim; depth++) {
		if (rpd[depth] != 0) {
			if (rpd[depth] == UINT8_MAX)
				config.nb_routes_per_depth[depth] =
					nrpd[depth];
			else
				config.nb_routes_per_depth[depth] =
					(n * rpd[depth]) / 100;

			nr += config.nb_routes_per_depth[depth];
			m++;
		}
	}

	if (nr > n) {
		printf("Too much configured routes\n");
		return -1;
	}

	/*complete number of routes for every unspecified depths*/
	for (depth = 0; depth <= depth_lim; depth++) {
		if (rpd[depth] == 0) {
			/*we don't need more than two /1 routes*/
			uint64_t max_routes_per_depth =
				1ULL << RTE_MIN(depth, 63);
			uint32_t avg_routes_left = (n - nr) /
				(depth_lim + 1 - m++);
			config.nb_routes_per_depth[depth] =
				RTE_MIN(max_routes_per_depth, avg_routes_left);
			nr += config.nb_routes_per_depth[depth];
		}
	}

	return 0;
}

static int
parse_distrib(uint8_t depth_lim, const uint32_t n)
{
	uint8_t	rpd[128 + 1] = {0}; /*routes ratios per depth including /0 */
	uint32_t nrpd[128 + 1] = {0}; /* number of routes per depth */
	uint32_t n_routes;
	uint8_t depth, ratio, ratio_acc = 0;
	char *in;

	in = strtok(distrib_string, ",");

	/*parse configures routes percentage ratios*/
	while (in != NULL) {
		GET_CB_FIELD(in, depth, 0, UINT8_MAX, ':');
		if (in[strlen(in) - 1] == '%') {
			in[strlen(in) - 1] = 0;
			GET_CB_FIELD(in, ratio, 0, UINT8_MAX, '\0');
			if (depth > depth_lim) {
				printf("Depth /%d is bigger than maximum "
					"allowed depth /%d for this AF\n",
					depth, depth_lim);
				return -EINVAL;
			}
			if (ratio > 100) {
				printf("Ratio for depth /%d is bigger "
					"than 100%%\n", depth);
				return -EINVAL;
			}
			if ((depth < 64) && ((n * ratio) / 100) >
					(1ULL << depth)) {
				printf("Configured ratio %d%% for depth /%d "
					"has %d different routes, but maximum "
					"is %lu\n", ratio, depth,
					((n * ratio) / 100), (1UL << depth));
				return -EINVAL;
			}
			rpd[depth] = ratio;
			/*configured zero routes for a given depth*/
			if (ratio == 0)
				rpd[depth] = UINT8_MAX;
			/*sum of all percentage ratios*/
			ratio_acc += ratio;
		} else {
			GET_CB_FIELD(in, n_routes, 0, UINT32_MAX, '\0');
			rpd[depth] = UINT8_MAX;
			nrpd[depth] = n_routes;
		}

		/*number of configured depths in*/
		in = strtok(NULL, ",");
	}

	if (ratio_acc > 100) {
		printf("Total ratio's sum is bigger than 100%%\n");
		return -EINVAL;
	}

	return complete_distrib(depth_lim, n, rpd, nrpd);
}

static void
shuffle_rt_4(struct rt_rule_4 *rt, int n)
{
	struct rt_rule_4 tmp;
	int i, j;

	for (i = 0; i < n; i++) {
		j = rte_rand() % n;
		tmp.addr = rt[i].addr;
		tmp.depth = rt[i].depth;
		tmp.nh = rt[i].nh;

		rt[i].addr = rt[j].addr;
		rt[i].depth = rt[j].depth;
		rt[i].nh = rt[j].nh;

		rt[j].addr = tmp.addr;
		rt[j].depth = tmp.depth;
		rt[j].nh = tmp.nh;
	}
}

static void
shuffle_rt_6(struct rt_rule_6 *rt, int n)
{
	struct rt_rule_6 tmp;
	int i, j;

	for (i = 0; i < n; i++) {
		j = rte_rand() % n;
		memcpy(tmp.addr, rt[i].addr, 16);
		tmp.depth = rt[i].depth;
		tmp.nh = rt[i].nh;

		memcpy(rt[i].addr, rt[j].addr, 16);
		rt[i].depth = rt[j].depth;
		rt[i].nh = rt[j].nh;

		memcpy(rt[j].addr, tmp.addr, 16);
		rt[j].depth = tmp.depth;
		rt[j].nh = tmp.nh;
	}
}

static void
gen_random_rt_4(struct rt_rule_4 *rt, int nh_sz)
{
	uint32_t i, j, k = 0;

	if (config.nb_routes_per_depth[0] != 0) {
		rt[k].addr = 0;
		rt[k].depth = 0;
		rt[k++].nh = rte_rand() & get_max_nh(nh_sz);
	}

	for (i = 1; i <= 32; i++) {
		double edge = 0;
		double step;
		step = (double)(1ULL << i) / config.nb_routes_per_depth[i];
		for (j = 0; j < config.nb_routes_per_depth[i];
				j++, k++, edge += step) {
			uint64_t rnd_val = get_rnd_rng((uint64_t)edge,
				(uint64_t)(edge + step));
			rt[k].addr = rnd_val << (32 - i);
			rt[k].depth = i;
			rt[k].nh = rte_rand() & get_max_nh(nh_sz);
		}
	}
}

static void
complete_v6_addr(uint32_t *addr, uint32_t rnd, int n)
{
	int i;

	for (i = 0; i < n; i++)
		addr[i] = rte_rand();
	addr[i++] = rnd;
	for (; i < 4; i++)
		addr[i] = 0;
}

static void
gen_random_rt_6(struct rt_rule_6 *rt, int nh_sz)
{
	uint32_t a, i, j, k = 0;

	if (config.nb_routes_per_depth[0] != 0) {
		memset(rt[k].addr, 0, 16);
		rt[k].depth = 0;
		rt[k++].nh = rte_rand() & get_max_nh(nh_sz);
	}

	for (a = 0; a < 4; a++) {
		for (i = 1; i <= 32; i++) {
			uint32_t rnd;
			double edge = 0;
			double step = (double)(1ULL << i) /
				config.nb_routes_per_depth[(a * 32) + i];
			for (j = 0; j < config.nb_routes_per_depth[a * 32 + i];
					j++, k++, edge += step) {
				uint64_t rnd_val = get_rnd_rng((uint64_t)edge,
					(uint64_t)(edge + step));
				rnd = rte_cpu_to_be_32(rnd_val << (32 - i));
				complete_v6_addr((uint32_t *)rt[k].addr,
					rnd, a);
				rt[k].depth = (a * 32) + i;
				rt[k].nh = rte_rand() & get_max_nh(nh_sz);
			}
		}
	}
}

static inline void
set_rnd_ipv6(uint8_t *addr, uint8_t *route, int depth)
{
	int i;

	for (i = 0; i < 16; i++)
		addr[i] = rte_rand();

	for (i = 0; i < 16; i++) {
		if (depth >= 8)
			addr[i] = route[i];
		else if (depth > 0) {
			addr[i] &= (uint16_t)UINT8_MAX >> depth;
			addr[i] |= route[i] & UINT8_MAX << (8 - depth);
		} else
			return;
		depth -= 8;
	}
}

static void
gen_rnd_lookup_tbl(int af)
{
	uint32_t *tbl4 = config.lookup_tbl;
	uint8_t *tbl6 = config.lookup_tbl;
	struct rt_rule_4 *rt4 = (struct rt_rule_4 *)config.rt;
	struct rt_rule_6 *rt6 = (struct rt_rule_6 *)config.rt;
	uint32_t i, j;

	if (af == AF_INET) {
		for (i = 0, j = 0; i < config.nb_lookup_ips;
				i++, j = (j + 1) % config.nb_routes) {
			if ((rte_rand() % 100) < config.rnd_lookup_ips_ratio) {
				tbl4[i] = rte_rand();
				config.nb_lookup_ips_rnd++;
			} else
				tbl4[i] = rt4[j].addr | (rte_rand() &
					((1ULL << (32 - rt4[j].depth)) - 1));
		}
	} else {
		for (i = 0, j = 0; i < config.nb_lookup_ips;
				i++, j = (j + 1) % config.nb_routes) {
			if ((rte_rand() % 100) < config.rnd_lookup_ips_ratio) {
				set_rnd_ipv6(&tbl6[i * 16], rt6[j].addr, 0);
				config.nb_lookup_ips_rnd++;
			} else {
				set_rnd_ipv6(&tbl6[i * 16], rt6[j].addr,
					rt6[j].depth);
			}
		}
	}
}

static int
_inet_net_pton(int af, char *prefix, void *addr)
{
	const char *dlm = "/";
	char *s, *sp;
	int ret, depth;
	unsigned int max_depth;

	if ((prefix == NULL) || (addr == NULL))
		return -EINVAL;

	s = strtok_r(prefix, dlm, &sp);
	if (s == NULL)
		return -EINVAL;

	ret = inet_pton(af, s, addr);
	if (ret != 1)
		return -errno;

	s = strtok_r(NULL, dlm, &sp);
	max_depth = (af == AF_INET) ? 32 : 128;
	GET_CB_FIELD(s, depth, 0, max_depth, 0);

	return depth;
}

static int
parse_rt_4(FILE *f)
{
	int ret, i, j = 0;
	char *s, *sp, *in[RT_NUM];
	static const char *dlm = " \t\n";
	int string_tok_nb = RTE_DIM(in);
	struct rt_rule_4 *rt;

	rt = (struct rt_rule_4 *)config.rt;

	while (fgets(line, sizeof(line), f) != NULL) {
		s = line;
		for (i = 0; i != string_tok_nb; i++) {
			in[i] = strtok_r(s, dlm, &sp);
			if (in[i] == NULL)
				return -EINVAL;
			s = NULL;
		}

		ret = _inet_net_pton(AF_INET, in[RT_PREFIX], &rt[j].addr);
		if (ret == -1)
			return -errno;

		rt[j].addr = rte_be_to_cpu_32(rt[j].addr);
		rt[j].depth = ret;
		config.nb_routes_per_depth[ret]++;
		GET_CB_FIELD(in[RT_NEXTHOP], rt[j].nh, 0,
				UINT32_MAX, 0);
		j++;
	}
	return 0;
}

static int
parse_rt_6(FILE *f)
{
	int ret, i, j = 0;
	char *s, *sp, *in[RT_NUM];
	static const char *dlm = " \t\n";
	int string_tok_nb = RTE_DIM(in);
	struct rt_rule_6 *rt;

	rt = (struct rt_rule_6 *)config.rt;

	while (fgets(line, sizeof(line), f) != NULL) {
		s = line;
		for (i = 0; i != string_tok_nb; i++) {
			in[i] = strtok_r(s, dlm, &sp);
			if (in[i] == NULL)
				return -EINVAL;
			s = NULL;
		}

		ret = _inet_net_pton(AF_INET6, in[RT_PREFIX], rt[j].addr);
		if (ret < 0)
			return ret;

		rt[j].depth = ret;
		config.nb_routes_per_depth[ret]++;
		GET_CB_FIELD(in[RT_NEXTHOP], rt[j].nh, 0,
				UINT32_MAX, 0);
		j++;
	}

	return 0;
}

static int
parse_lookup(FILE *f, int af)
{
	int ret, i = 0;
	uint8_t *tbl = (uint8_t *)config.lookup_tbl;
	int step = (af == AF_INET) ? 4 : 16;
	char *s;

	while (fgets(line, sizeof(line), f) != NULL) {
		s = strtok(line, " \t\n");
		if (s == NULL)
			return -EINVAL;
		ret = inet_pton(af, s, &tbl[i]);
		if (ret != 1)
			return -EINVAL;
		i += step;
	}
	return 0;
}

static int
dump_lookup(int af)
{
	FILE *f;
	uint32_t *tbl4 = config.lookup_tbl;
	uint8_t *tbl6 = config.lookup_tbl;
	uint32_t i;

	f = fopen(config.lookup_ips_file_s, "w");
	if (f == NULL) {
		printf("Can not open file %s\n", config.lookup_ips_file_s);
		return -1;
	}

	if (af == AF_INET) {
		for (i = 0; i < config.nb_lookup_ips; i++)
			fprintf(f, NIPQUAD_FMT"\n", NIPQUAD(tbl4[i]));
	} else {
		for (i = 0; i < config.nb_lookup_ips; i++)
			fprintf(f, NIPQUAD6_FMT"\n", NIPQUAD6(&tbl6[i * 16]));
	}
	fclose(f);
	return 0;
}

static void
print_config(void)
{
	uint8_t depth_lim;
	char dlm;
	int i;

	depth_lim = ((config.flags & IPV6_FLAG) == IPV6_FLAG) ? 128 : 32;

	fprintf(stdout,
		"Routes total: %u\n"
		"Routes distribution:\n", config.nb_routes);

	for (i = 1; i <= depth_lim; i++) {
		fprintf(stdout,
			"depth /%d:%u", i, config.nb_routes_per_depth[i]);
		if (i % 4 == 0)
			dlm = '\n';
		else
			dlm = '\t';
		fprintf(stdout, "%c", dlm);
	}

	fprintf(stdout,
		"Lookup tuples: %u\n"
		"Configured ratios of random ips for lookup: %u\n"
		"Random lookup ips: %u\n",
		config.nb_lookup_ips, config.rnd_lookup_ips_ratio,
		config.nb_lookup_ips_rnd);
}

static void
print_usage(void)
{
	fprintf(stdout,
		PRINT_USAGE_START
		"[-f <routes file>]\n"
		"[-t <ip's file for lookup>]\n"
		"[-n <number of routes (if -f is not specified)>]\n"
		"[-l <number of ip's for lookup (if -t is not specified)>]\n"
		"[-d <\",\" separated \"depth:n%%\"routes depth distribution"
		"(if -f is not specified)>]\n"
		"[-r <percentage ratio of random ip's to lookup"
		"(if -t is not specified)>]\n"
		"[-c <do comparison with LPM library>]\n"
		"[-6 <do tests with ipv6 (default ipv4)>]\n"
		"[-s <shuffle randomly generated routes>]\n"
		"[-a <check nexthops for all ipv4 address space"
		"(only valid with -c)>]\n"
		"[-b <fib algorithm>]\n\tavailable options for ipv4\n"
		"\t\trib - RIB based FIB\n"
		"\t\tdir - DIR24_8 based FIB\n"
		"\tavailable options for ipv6:\n"
		"\t\trib - RIB based FIB\n"
		"\t\ttrie - TRIE based FIB\n"
		"defaults are: dir for ipv4 and trie for ipv6\n"
		"[-e <entry size (valid only for dir and trie fib types): "
		"1/2/4/8 (default 4)>]\n"
		"[-g <number of tbl8's for dir24_8 or trie FIBs>]\n"
		"[-w <path to the file to dump routing table>]\n"
		"[-u <path to the file to dump ip's for lookup>]\n"
		"[-v <type of lookup function:"
		"\ts1, s2, s3 (3 types of scalar), v (vector) -"
		" for DIR24_8 based FIB\n"
		"\ts, v - for TRIE based ipv6 FIB>]\n",
		config.prgname);
}

static int
check_config(void)
{
	if ((config.routes_file == NULL) && (config.lookup_ips_file != NULL)) {
		printf("-t option only valid with -f option\n");
		return -1;
	}

	if ((config.flags & CMP_ALL_FLAG) && (config.flags & IPV6_FLAG)) {
		printf("-a flag is only valid for ipv4\n");
		return -1;
	}

	if ((config.flags & CMP_ALL_FLAG) &&
			((config.flags & CMP_FLAG) != CMP_FLAG)) {
		printf("-a flag is valid only with -c flag\n");
		return -1;
	}

	if (!((config.ent_sz == 1) || (config.ent_sz == 2) ||
			(config.ent_sz == 4) || (config.ent_sz == 8))) {
		printf("wrong -e option %d, can be 1 or 2 or 4 or 8\n",
			config.ent_sz);
		return -1;
	}

	if ((config.ent_sz == 1) && (config.flags & IPV6_FLAG)) {
		printf("-e 1 is valid only for ipv4\n");
		return -1;
	}
	return 0;
}

static void
parse_opts(int argc, char **argv)
{
	int opt;
	char *endptr;

	while ((opt = getopt(argc, argv, "f:t:n:d:l:r:c6ab:e:g:w:u:sv:")) !=
			-1) {
		switch (opt) {
		case 'f':
			config.routes_file = optarg;
			break;
		case 't':
			config.lookup_ips_file = optarg;
			break;
		case 'w':
			config.routes_file_s = optarg;
			config.flags |= DRY_RUN_FLAG;
			break;
		case 'u':
			config.lookup_ips_file_s = optarg;
			config.flags |= DRY_RUN_FLAG;
			break;
		case 'n':
			errno = 0;
			config.nb_routes = strtoul(optarg, &endptr, 10);
			if ((errno != 0) || (config.nb_routes == 0)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -n\n");
			}

			if (config.nb_routes < config.print_fract)
				config.print_fract = config.nb_routes;

			break;
		case 'd':
			distrib_string = optarg;
			break;
		case 'l':
			errno = 0;
			config.nb_lookup_ips = strtoul(optarg, &endptr, 10);
			if ((errno != 0) || (config.nb_lookup_ips == 0)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -l\n");
			}
			break;
		case 'r':
			errno = 0;
			config.rnd_lookup_ips_ratio =
				strtoul(optarg, &endptr, 10);
			if ((errno != 0) ||
					(config.rnd_lookup_ips_ratio == 0) ||
					(config.rnd_lookup_ips_ratio >= 100)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -r\n");
			}
			break;
		case 's':
			config.flags |= SHUFFLE_FLAG;
			break;
		case 'c':
			config.flags |= CMP_FLAG;
			break;
		case '6':
			config.flags |= IPV6_FLAG;
			break;
		case 'a':
			config.flags |= CMP_ALL_FLAG;
			break;
		case 'b':
			if (strcmp(optarg, "rib") == 0) {
				config.flags &= ~FIB_TYPE_MASK;
				config.flags |= FIB_RIB_TYPE;
			} else if (strcmp(optarg, "dir") == 0) {
				config.flags &= ~FIB_TYPE_MASK;
				config.flags |= FIB_V4_DIR_TYPE;
			} else if (strcmp(optarg, "trie") == 0) {
				config.flags &= ~FIB_TYPE_MASK;
				config.flags |= FIB_V6_TRIE_TYPE;
			} else
				rte_exit(-EINVAL, "Invalid option -b\n");
			break;
		case 'e':
			errno = 0;
			config.ent_sz = strtoul(optarg, &endptr, 10);
			if (errno != 0) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -e\n");
			}
			break;
		case 'g':
			errno = 0;
			config.tbl8 = strtoul(optarg, &endptr, 10);
			if ((errno != 0) || (config.tbl8 == 0)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -g\n");
			}
			break;
		case 'v':
			if ((strcmp(optarg, "s1") == 0) ||
					(strcmp(optarg, "s") == 0)) {
				config.lookup_fn = 1;
				break;
			} else if (strcmp(optarg, "v") == 0) {
				config.lookup_fn = 2;
				break;
			} else if (strcmp(optarg, "s2") == 0) {
				config.lookup_fn = 3;
				break;
			} else if (strcmp(optarg, "s3") == 0) {
				config.lookup_fn = 4;
				break;
			}
			print_usage();
			rte_exit(-EINVAL, "Invalid option -v %s\n", optarg);
		default:
			print_usage();
			rte_exit(-EINVAL, "Invalid options\n");
		}
	}
}

static int
dump_rt_4(struct rt_rule_4 *rt)
{
	FILE *f;
	uint32_t i;

	f = fopen(config.routes_file_s, "w");
	if (f == NULL) {
		printf("Can not open file %s\n", config.routes_file_s);
		return -1;
	}

	for (i = 0; i < config.nb_routes; i++)
		fprintf(f, NIPQUAD_FMT"/%d %"PRIu64"\n", NIPQUAD(rt[i].addr),
			rt[i].depth, rt[i].nh);

	fclose(f);
	return 0;
}

static inline void
print_depth_err(void)
{
	printf("LPM does not support /0 prefix length (default route), use "
		"-d 0:0 option or remove /0 prefix from routes file\n");
}

static int
run_v4(void)
{
	uint64_t start, acc;
	uint64_t def_nh = 0;
	struct rte_fib *fib;
	struct rte_fib_conf conf = {0};
	struct rt_rule_4 *rt;
	uint32_t i, j, k;
	int ret = 0;
	struct rte_lpm	*lpm = NULL;
	struct rte_lpm_config lpm_conf;
	uint32_t *tbl4 = config.lookup_tbl;
	uint64_t fib_nh[BURST_SZ];
	uint32_t lpm_nh[BURST_SZ];

	rt = (struct rt_rule_4 *)config.rt;

	if (config.flags & DRY_RUN_FLAG) {
		if (config.routes_file_s != NULL)
			ret = dump_rt_4(rt);
		if (ret != 0)
			return ret;
		if (config.lookup_ips_file_s != NULL)
			ret = dump_lookup(AF_INET);
		return ret;
	}

	conf.type = get_fib_type();
	conf.default_nh = def_nh;
	conf.max_routes = config.nb_routes * 2;
	conf.rib_ext_sz = 0;
	if (conf.type == RTE_FIB_DIR24_8) {
		conf.dir24_8.nh_sz = __builtin_ctz(config.ent_sz);
		conf.dir24_8.num_tbl8 = RTE_MIN(config.tbl8,
			get_max_nh(conf.dir24_8.nh_sz));
	}

	fib = rte_fib_create("test", -1, &conf);
	if (fib == NULL) {
		printf("Can not alloc FIB, err %d\n", rte_errno);
		return -rte_errno;
	}

	if (config.lookup_fn != 0) {
		if (config.lookup_fn == 1)
			ret = rte_fib_select_lookup(fib,
				RTE_FIB_LOOKUP_DIR24_8_SCALAR_MACRO);
		else if (config.lookup_fn == 2)
			ret = rte_fib_select_lookup(fib,
				RTE_FIB_LOOKUP_DIR24_8_VECTOR_AVX512);
		else if (config.lookup_fn == 3)
			ret = rte_fib_select_lookup(fib,
				RTE_FIB_LOOKUP_DIR24_8_SCALAR_INLINE);
		else if (config.lookup_fn == 4)
			ret = rte_fib_select_lookup(fib,
				RTE_FIB_LOOKUP_DIR24_8_SCALAR_UNI);
		else
			ret = -EINVAL;
		if (ret != 0) {
			printf("Can not init lookup function\n");
			return ret;
		}
	}

	for (k = config.print_fract, i = 0; k > 0; k--) {
		start = rte_rdtsc_precise();
		for (j = 0; j < (config.nb_routes - i) / k; j++) {
			ret = rte_fib_add(fib, rt[i + j].addr, rt[i + j].depth,
				rt[i + j].nh);
			if (unlikely(ret != 0)) {
				printf("Can not add a route to FIB, err %d\n",
					ret);
				return -ret;
			}
		}
		printf("AVG FIB add %"PRIu64"\n",
			(rte_rdtsc_precise() - start) / j);
		i += j;
	}

	if (config.flags & CMP_FLAG) {
		lpm_conf.max_rules = config.nb_routes * 2;
		lpm_conf.number_tbl8s = RTE_MAX(conf.dir24_8.num_tbl8,
			config.tbl8);

		lpm = rte_lpm_create("test_lpm", -1, &lpm_conf);
		if (lpm == NULL) {
			printf("Can not alloc LPM, err %d\n", rte_errno);
			return -rte_errno;
		}
		for (k = config.print_fract, i = 0; k > 0; k--) {
			start = rte_rdtsc_precise();
			for (j = 0; j < (config.nb_routes - i) / k; j++) {
				ret = rte_lpm_add(lpm, rt[i + j].addr,
					rt[i + j].depth, rt[i + j].nh);
				if (ret != 0) {
					if (rt[i + j].depth == 0)
						print_depth_err();
					printf("Can not add a route to LPM, "
						"err %d\n", ret);
					return -ret;
				}
			}
			printf("AVG LPM add %"PRIu64"\n",
				(rte_rdtsc_precise() - start) / j);
			i += j;
		}
	}

	acc = 0;
	for (i = 0; i < config.nb_lookup_ips; i += BURST_SZ) {
		start = rte_rdtsc_precise();
		ret = rte_fib_lookup_bulk(fib, tbl4 + i, fib_nh, BURST_SZ);
		acc += rte_rdtsc_precise() - start;
		if (ret != 0) {
			printf("FIB lookup fails, err %d\n", ret);
			return -ret;
		}
	}
	printf("AVG FIB lookup %.1f\n", (double)acc / (double)i);

	if (config.flags & CMP_FLAG) {
		acc = 0;
		for (i = 0; i < config.nb_lookup_ips; i += BURST_SZ) {
			start = rte_rdtsc_precise();
			ret = rte_lpm_lookup_bulk(lpm, tbl4 + i, lpm_nh,
				BURST_SZ);
			acc += rte_rdtsc_precise() - start;
			if (ret != 0) {
				printf("LPM lookup fails, err %d\n", ret);
				return -ret;
			}
		}
		printf("AVG LPM lookup %.1f\n", (double)acc / (double)i);

		for (i = 0; i < config.nb_lookup_ips; i += BURST_SZ) {
			rte_fib_lookup_bulk(fib, tbl4 + i, fib_nh, BURST_SZ);
			rte_lpm_lookup_bulk(lpm, tbl4 + i, lpm_nh, BURST_SZ);
			for (j = 0; j < BURST_SZ; j++) {
				struct rte_lpm_tbl_entry *tbl;
				tbl = (struct rte_lpm_tbl_entry *)&lpm_nh[j];
				if ((fib_nh[j] != tbl->next_hop) &&
						!((tbl->valid == 0) &&
						(fib_nh[j] == def_nh))) {
					printf("FAIL\n");
					return -1;
				}
			}
		}
		printf("FIB and LPM lookup returns same values\n");
	}

	for (k = config.print_fract, i = 0; k > 0; k--) {
		start = rte_rdtsc_precise();
		for (j = 0; j < (config.nb_routes - i) / k; j++)
			rte_fib_delete(fib, rt[i + j].addr, rt[i + j].depth);

		printf("AVG FIB delete %"PRIu64"\n",
			(rte_rdtsc_precise() - start) / j);
		i += j;
	}

	if (config.flags & CMP_FLAG) {
		for (k = config.print_fract, i = 0; k > 0; k--) {
			start = rte_rdtsc_precise();
			for (j = 0; j < (config.nb_routes - i) / k; j++)
				rte_lpm_delete(lpm, rt[i + j].addr,
					rt[i + j].depth);

			printf("AVG LPM delete %"PRIu64"\n",
				(rte_rdtsc_precise() - start) / j);
			i += j;
		}
	}

	return 0;
}

static int
dump_rt_6(struct rt_rule_6 *rt)
{
	FILE *f;
	uint32_t i;

	f = fopen(config.routes_file_s, "w");
	if (f == NULL) {
		printf("Can not open file %s\n", config.routes_file_s);
		return -1;
	}

	for (i = 0; i < config.nb_routes; i++) {
		fprintf(f, NIPQUAD6_FMT"/%d %"PRIu64"\n", NIPQUAD6(rt[i].addr),
			rt[i].depth, rt[i].nh);

	}
	fclose(f);
	return 0;
}

static int
run_v6(void)
{
	uint64_t start, acc;
	uint64_t def_nh = 0;
	struct rte_fib6 *fib;
	struct rte_fib6_conf conf = {0};
	struct rt_rule_6 *rt;
	uint32_t i, j, k;
	int ret = 0;
	struct rte_lpm6	*lpm = NULL;
	struct rte_lpm6_config lpm_conf;
	uint8_t *tbl6;
	uint64_t fib_nh[BURST_SZ];
	int32_t lpm_nh[BURST_SZ];

	rt = (struct rt_rule_6 *)config.rt;
	tbl6 = config.lookup_tbl;

	if (config.flags & DRY_RUN_FLAG) {
		if (config.routes_file_s != NULL)
			ret =  dump_rt_6(rt);
		if (ret != 0)
			return ret;
		if (config.lookup_ips_file_s != NULL)
			ret = dump_lookup(AF_INET6);
		return ret;
	}

	conf.type = get_fib_type();
	conf.default_nh = def_nh;
	conf.max_routes = config.nb_routes * 2;
	conf.rib_ext_sz = 0;
	if (conf.type == RTE_FIB6_TRIE) {
		conf.trie.nh_sz = __builtin_ctz(config.ent_sz);
		conf.trie.num_tbl8 = RTE_MIN(config.tbl8,
			get_max_nh(conf.trie.nh_sz));
	}

	fib = rte_fib6_create("test", -1, &conf);
	if (fib == NULL) {
		printf("Can not alloc FIB, err %d\n", rte_errno);
		return -rte_errno;
	}

	if (config.lookup_fn != 0) {
		if (config.lookup_fn == 1)
			ret = rte_fib6_select_lookup(fib,
				RTE_FIB6_LOOKUP_TRIE_SCALAR);
		else if (config.lookup_fn == 2)
			ret = rte_fib6_select_lookup(fib,
				RTE_FIB6_LOOKUP_TRIE_VECTOR_AVX512);
		else
			ret = -EINVAL;
		if (ret != 0) {
			printf("Can not init lookup function\n");
			return ret;
		}
	}

	for (k = config.print_fract, i = 0; k > 0; k--) {
		start = rte_rdtsc_precise();
		for (j = 0; j < (config.nb_routes - i) / k; j++) {
			ret = rte_fib6_add(fib, rt[i + j].addr,
				rt[i + j].depth, rt[i + j].nh);
			if (unlikely(ret != 0)) {
				printf("Can not add a route to FIB, err %d\n",
					ret);
				return -ret;
			}
		}
		printf("AVG FIB add %"PRIu64"\n",
			(rte_rdtsc_precise() - start) / j);
		i += j;
	}

	if (config.flags & CMP_FLAG) {
		lpm_conf.max_rules = config.nb_routes * 2;
		lpm_conf.number_tbl8s = RTE_MAX(conf.trie.num_tbl8,
			config.tbl8);

		lpm = rte_lpm6_create("test_lpm", -1, &lpm_conf);
		if (lpm == NULL) {
			printf("Can not alloc LPM, err %d\n", rte_errno);
			return -rte_errno;
		}
		for (k = config.print_fract, i = 0; k > 0; k--) {
			start = rte_rdtsc_precise();
			for (j = 0; j < (config.nb_routes - i) / k; j++) {
				ret = rte_lpm6_add(lpm, rt[i + j].addr,
					rt[i + j].depth, rt[i + j].nh);
				if (ret != 0) {
					if (rt[i + j].depth == 0)
						print_depth_err();
					printf("Can not add a route to LPM, "
						"err %d\n", ret);
					return -ret;
				}
			}
			printf("AVG LPM add %"PRIu64"\n",
				(rte_rdtsc_precise() - start) / j);
			i += j;
		}
	}

	acc = 0;
	for (i = 0; i < config.nb_lookup_ips; i += BURST_SZ) {
		start = rte_rdtsc_precise();
		ret = rte_fib6_lookup_bulk(fib, (uint8_t (*)[16])(tbl6 + i*16),
			fib_nh, BURST_SZ);
		acc += rte_rdtsc_precise() - start;
		if (ret != 0) {
			printf("FIB lookup fails, err %d\n", ret);
			return -ret;
		}
	}
	printf("AVG FIB lookup %.1f\n", (double)acc / (double)i);

	if (config.flags & CMP_FLAG) {
		acc = 0;
		for (i = 0; i < config.nb_lookup_ips; i += BURST_SZ) {
			start = rte_rdtsc_precise();
			ret = rte_lpm6_lookup_bulk_func(lpm,
				(uint8_t (*)[16])(tbl6 + i*16),
				lpm_nh, BURST_SZ);
			acc += rte_rdtsc_precise() - start;
			if (ret != 0) {
				printf("LPM lookup fails, err %d\n", ret);
				return -ret;
			}
		}
		printf("AVG LPM lookup %.1f\n", (double)acc / (double)i);

		for (i = 0; i < config.nb_lookup_ips; i += BURST_SZ) {
			rte_fib6_lookup_bulk(fib,
				(uint8_t (*)[16])(tbl6 + i*16),
				fib_nh, BURST_SZ);
			rte_lpm6_lookup_bulk_func(lpm,
				(uint8_t (*)[16])(tbl6 + i*16),
				lpm_nh, BURST_SZ);
			for (j = 0; j < BURST_SZ; j++) {
				if ((fib_nh[j] != (uint32_t)lpm_nh[j]) &&
						!((lpm_nh[j] == -1) &&
						(fib_nh[j] == def_nh))) {
					printf("FAIL\n");
					return -1;
				}
			}
		}
		printf("FIB and LPM lookup returns same values\n");
	}

	for (k = config.print_fract, i = 0; k > 0; k--) {
		start = rte_rdtsc_precise();
		for (j = 0; j < (config.nb_routes - i) / k; j++)
			rte_fib6_delete(fib, rt[i + j].addr, rt[i + j].depth);

		printf("AVG FIB delete %"PRIu64"\n",
			(rte_rdtsc_precise() - start) / j);
		i += j;
	}

	if (config.flags & CMP_FLAG) {
		for (k = config.print_fract, i = 0; k > 0; k--) {
			start = rte_rdtsc_precise();
			for (j = 0; j < (config.nb_routes - i) / k; j++)
				rte_lpm6_delete(lpm, rt[i + j].addr,
					rt[i + j].depth);

			printf("AVG LPM delete %"PRIu64"\n",
				(rte_rdtsc_precise() - start) / j);
			i += j;
		}
	}
	return 0;
}

int
main(int argc, char **argv)
{
	int ret, af, rt_ent_sz, lookup_ent_sz;
	FILE *fr = NULL;
	FILE *fl = NULL;
	uint8_t depth_lim;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	argc -= ret;
	argv += ret;

	config.prgname = argv[0];

	parse_opts(argc, argv);

	ret = check_config();
	if (ret != 0)
		rte_exit(-ret, "Bad configuration\n");

	af = ((config.flags & IPV6_FLAG) == 0) ? AF_INET : AF_INET6;
	depth_lim = (af == AF_INET) ? 32 : 128;
	rt_ent_sz = (af == AF_INET) ? sizeof(struct rt_rule_4) :
		sizeof(struct rt_rule_6);
	lookup_ent_sz = (af == AF_INET) ? 4 : 16;

	/* Count number of rules in file*/
	if (config.routes_file != NULL) {
		fr = fopen(config.routes_file, "r");
		if (fr == NULL)
			rte_exit(-errno, "Can not open file with routes %s\n",
				config.routes_file);

		config.nb_routes = 0;
		while (fgets(line, sizeof(line), fr) != NULL)
			config.nb_routes++;

		if (config.nb_routes < config.print_fract)
			config.print_fract = config.nb_routes;

		rewind(fr);
	}

	/* Count number of ip's in file*/
	if (config.lookup_ips_file != NULL) {
		fl = fopen(config.lookup_ips_file, "r");
		if (fl == NULL)
			rte_exit(-errno, "Can not open file with ip's %s\n",
				config.lookup_ips_file);

		config.nb_lookup_ips = 0;
		while (fgets(line, sizeof(line), fl) != NULL)
			config.nb_lookup_ips++;
		rewind(fl);
	}

	/* Alloc routes table*/
	config.rt  = rte_malloc(NULL, rt_ent_sz * config.nb_routes, 0);
	if (config.rt == NULL)
		rte_exit(-ENOMEM, "Can not alloc rt\n");

	/* Alloc table with ip's for lookup*/
	config.lookup_tbl  = rte_malloc(NULL, lookup_ent_sz *
		config.nb_lookup_ips, 0);
	if (config.lookup_tbl == NULL)
		rte_exit(-ENOMEM, "Can not alloc lookup table\n");

	/* Fill routes table */
	if (fr == NULL) {
		if (distrib_string != NULL)
			ret = parse_distrib(depth_lim, config.nb_routes);
		else {
			uint8_t rpd[129] = {0};
			uint32_t nrpd[129] = {0};
			ret = complete_distrib(depth_lim, config.nb_routes,
				rpd, nrpd);
		}
		if (ret != 0)
			rte_exit(-ret,
				"Bad routes distribution configuration\n");
		if (af == AF_INET) {
			gen_random_rt_4(config.rt,
				__builtin_ctz(config.ent_sz));
			if (config.flags & SHUFFLE_FLAG)
				shuffle_rt_4(config.rt, config.nb_routes);
		} else {
			gen_random_rt_6(config.rt,
				__builtin_ctz(config.ent_sz));
			if (config.flags & SHUFFLE_FLAG)
				shuffle_rt_6(config.rt, config.nb_routes);
		}
	} else {
		if (af == AF_INET)
			ret = parse_rt_4(fr);
		else
			ret = parse_rt_6(fr);

		if (ret != 0) {
			rte_exit(-ret, "failed to parse routes file %s\n",
				config.routes_file);
		}
	}

	/* Fill lookup table with ip's*/
	if (fl == NULL)
		gen_rnd_lookup_tbl(af);
	else {
		ret = parse_lookup(fl, af);
		if (ret != 0)
			rte_exit(-ret, "failed to parse lookup file\n");
	}

	print_config();

	if (af == AF_INET)
		ret = run_v4();
	else
		ret = run_v6();

	return ret;
}
