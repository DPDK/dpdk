/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_ipsec_sad.h>
#include <getopt.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <rte_cycles.h>
#include <rte_errno.h>
#include <rte_ip.h>
#include <rte_random.h>
#include <rte_malloc.h>

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

#define	DEF_RULE_NUM		0x10000
#define	DEF_TUPLES_NUM	0x100000
#define BURST_SZ_MAX	64

static struct {
	const char	*prgname;
	const char	*rules_file;
	const char	*tuples_file;
	uint32_t	nb_rules;
	uint32_t	nb_tuples;
	uint32_t	nb_rules_32;
	uint32_t	nb_rules_64;
	uint32_t	nb_rules_96;
	uint32_t	nb_tuples_rnd;
	uint32_t	burst_sz;
	uint8_t		fract_32;
	uint8_t		fract_64;
	uint8_t		fract_96;
	uint8_t		fract_rnd_tuples;
	int		ipv6;
	int		verbose;
	int		parallel_lookup;
	int		concurrent_rw;
} config = {
	.rules_file = NULL,
	.tuples_file = NULL,
	.nb_rules = DEF_RULE_NUM,
	.nb_tuples = DEF_TUPLES_NUM,
	.nb_rules_32 = 0,
	.nb_rules_64 = 0,
	.nb_rules_96 = 0,
	.nb_tuples_rnd = 0,
	.burst_sz = BURST_SZ_MAX,
	.fract_32 = 90,
	.fract_64 = 9,
	.fract_96 = 1,
	.fract_rnd_tuples = 0,
	.ipv6 = 0,
	.verbose = 0,
	.parallel_lookup = 0,
	.concurrent_rw = 0
};

enum {
	CB_RULE_SPI,
	CB_RULE_DIP,
	CB_RULE_SIP,
	CB_RULE_LEN,
	CB_RULE_NUM,
};

static char line[LINE_MAX];
struct rule {
	union rte_ipsec_sad_key	tuple;
	int rule_type;
};

static struct rule *rules_tbl;
static struct rule *tuples_tbl;

static int
parse_distrib(const char *in)
{
	int a, b, c;

	GET_CB_FIELD(in, a, 0, UINT8_MAX, '/');
	GET_CB_FIELD(in, b, 0, UINT8_MAX, '/');
	GET_CB_FIELD(in, c, 0, UINT8_MAX, 0);

	if ((a + b + c) != 100)
		return -EINVAL;

	config.fract_32 = a;
	config.fract_64 = b;
	config.fract_96 = c;

	return 0;
}

static void
print_config(void)
{
	fprintf(stdout,
		"Rules total: %u\n"
		"Configured rules distribution SPI/SPI_DIP/SIP_DIP_SIP:"
		"%u/%u/%u\n"
		"SPI only rules: %u\n"
		"SPI_DIP  rules: %u\n"
		"SPI_DIP_SIP rules: %u\n"
		"Lookup tuples: %u\n"
		"Lookup burst size %u\n"
		"Configured fraction of random tuples: %u\n"
		"Random lookup tuples: %u\n",
		config.nb_rules, config.fract_32, config.fract_64,
		config.fract_96, config.nb_rules_32, config.nb_rules_64,
		config.nb_rules_96, config.nb_tuples, config.burst_sz,
		config.fract_rnd_tuples, config.nb_tuples_rnd);
}

static void
print_usage(void)
{
	fprintf(stdout,
		PRINT_USAGE_START
		"[-f <rules file>]\n"
		"[-t <tuples file for lookup>]\n"
		"[-n <rules number (if -f is not specified)>]\n"
		"[-l <lookup tuples number (if -t is not specified)>]\n"
		"[-6 <ipv6 tests>]\n"
		"[-d <\"/\" separated rules length distribution"
		"(if -f is not specified)>]\n"
		"[-r <random tuples fraction to lookup"
		"(if -t is not specified)>]\n"
		"[-b <lookup burst size: 1-64 >]\n"
		"[-v <verbose, print results on lookup>]\n"
		"[-p <parallel lookup on all available cores>]\n"
		"[-c <init sad supporting read/write concurrency>]\n",
		config.prgname);

}

static int
get_str_num(FILE *f, int num)
{
	int n_lines = 0;

	if (f != NULL) {
		while (fgets(line, sizeof(line), f) != NULL)
			n_lines++;
		rewind(f);
	} else {
		n_lines = num;
	}
	return n_lines;
}

static int
parse_file(FILE *f, struct rule *tbl, int rule_tbl)
{
	int ret, i, j = 0;
	char *s, *sp, *in[CB_RULE_NUM];
	static const char *dlm = " \t\n";
	int string_tok_nb = RTE_DIM(in);

	string_tok_nb -= (rule_tbl == 0) ? 1 : 0;
	while (fgets(line, sizeof(line), f) != NULL) {
		s = line;
		for (i = 0; i != string_tok_nb; i++) {
			in[i] = strtok_r(s, dlm, &sp);
			if (in[i] == NULL)
				return -EINVAL;
			s = NULL;
		}
		GET_CB_FIELD(in[CB_RULE_SPI], tbl[j].tuple.v4.spi, 0,
				UINT32_MAX, 0);

		if (config.ipv6)
			ret = inet_pton(AF_INET6, in[CB_RULE_DIP],
				&tbl[j].tuple.v6.dip);
		else
			ret = inet_pton(AF_INET, in[CB_RULE_DIP],
				&tbl[j].tuple.v4.dip);
		if (ret != 1)
			return -EINVAL;
		if (config.ipv6)
			ret = inet_pton(AF_INET6, in[CB_RULE_SIP],
				&tbl[j].tuple.v6.sip);
		else
			ret = inet_pton(AF_INET, in[CB_RULE_SIP],
				&tbl[j].tuple.v4.sip);
		if (ret != 1)
			return -EINVAL;
		if ((rule_tbl) && (in[CB_RULE_LEN] != NULL)) {
			if (strcmp(in[CB_RULE_LEN], "SPI_DIP_SIP") == 0) {
				tbl[j].rule_type = RTE_IPSEC_SAD_SPI_DIP_SIP;
				config.nb_rules_96++;
			} else if (strcmp(in[CB_RULE_LEN], "SPI_DIP") == 0) {
				tbl[j].rule_type = RTE_IPSEC_SAD_SPI_DIP;
				config.nb_rules_64++;
			} else if (strcmp(in[CB_RULE_LEN], "SPI") == 0) {
				tbl[j].rule_type = RTE_IPSEC_SAD_SPI_ONLY;
				config.nb_rules_32++;
			} else {
				return -EINVAL;
			}
		}
		j++;
	}
	return 0;
}

static uint64_t
get_rnd_rng(uint64_t l, uint64_t u)
{
	if (l == u)
		return l;
	else
		return (rte_rand() % (u - l) + l);
}

static void
get_random_rules(struct rule *tbl, uint32_t nb_rules, int rule_tbl)
{
	unsigned int i, j, rnd;
	int rule_type;
	double edge = 0;
	double step;

	step = (double)UINT32_MAX / nb_rules;
	for (i = 0; i < nb_rules; i++, edge += step) {
		rnd = rte_rand() % 100;
		if (rule_tbl) {
			tbl[i].tuple.v4.spi = get_rnd_rng((uint64_t)edge,
						(uint64_t)(edge + step));
			if (config.ipv6) {
				for (j = 0; j < 16; j++) {
					tbl[i].tuple.v6.dip[j] = rte_rand();
					tbl[i].tuple.v6.sip[j] = rte_rand();
				}
			} else {
				tbl[i].tuple.v4.dip = rte_rand();
				tbl[i].tuple.v4.sip = rte_rand();
			}
			if (rnd >= (100UL - config.fract_32)) {
				rule_type = RTE_IPSEC_SAD_SPI_ONLY;
				config.nb_rules_32++;
			} else if (rnd >= (100UL - (config.fract_32 +
					config.fract_64))) {
				rule_type = RTE_IPSEC_SAD_SPI_DIP;
				config.nb_rules_64++;
			} else {
				rule_type = RTE_IPSEC_SAD_SPI_DIP_SIP;
				config.nb_rules_96++;
			}
			tbl[i].rule_type = rule_type;
		} else {
			if (rnd >= 100UL - config.fract_rnd_tuples) {
				tbl[i].tuple.v4.spi =
					get_rnd_rng((uint64_t)edge,
					(uint64_t)(edge + step));
				if (config.ipv6) {
					for (j = 0; j < 16; j++) {
						tbl[i].tuple.v6.dip[j] =
								rte_rand();
						tbl[i].tuple.v6.sip[j] =
								rte_rand();
					}
				} else {
					tbl[i].tuple.v4.dip = rte_rand();
					tbl[i].tuple.v4.sip = rte_rand();
				}
				config.nb_tuples_rnd++;
			} else {
				tbl[i].tuple.v4.spi = rules_tbl[i %
					config.nb_rules].tuple.v4.spi;
				if (config.ipv6) {
					int r_idx = i % config.nb_rules;
					memcpy(tbl[i].tuple.v6.dip,
						rules_tbl[r_idx].tuple.v6.dip,
						sizeof(tbl[i].tuple.v6.dip));
					memcpy(tbl[i].tuple.v6.sip,
						rules_tbl[r_idx].tuple.v6.sip,
						sizeof(tbl[i].tuple.v6.sip));
				} else {
					tbl[i].tuple.v4.dip = rules_tbl[i %
						config.nb_rules].tuple.v4.dip;
					tbl[i].tuple.v4.sip = rules_tbl[i %
						config.nb_rules].tuple.v4.sip;
				}
			}
		}
	}
}

static void
tbl_init(struct rule **tbl, uint32_t *n_entries,
	const char *file_name, int rule_tbl)
{
	FILE *f = NULL;
	int ret;
	const char *rules = "rules";
	const char *tuples = "tuples";

	if (file_name != NULL) {
		f = fopen(file_name, "r");
		if (f == NULL)
			rte_exit(-EINVAL, "failed to open file: %s\n",
				file_name);
	}

	printf("init %s table...", (rule_tbl) ? rules : tuples);
	*n_entries = get_str_num(f, *n_entries);
	printf("%d entries\n", *n_entries);
	*tbl = rte_zmalloc(NULL, sizeof(struct rule) * *n_entries,
		RTE_CACHE_LINE_SIZE);
	if (*tbl == NULL)
		rte_exit(-ENOMEM, "failed to allocate tbl\n");

	if (f != NULL) {
		printf("parse file %s\n", file_name);
		ret = parse_file(f, *tbl, rule_tbl);
		if (ret != 0)
			rte_exit(-EINVAL, "failed to parse file %s\n"
				"rules file must be: "
				"<uint32_t: spi> <space> "
				"<ip_addr: dip> <space> "
				"<ip_addr: sip> <space> "
				"<string: SPI|SPI_DIP|SIP_DIP_SIP>\n"
				"tuples file must be: "
				"<uint32_t: spi> <space> "
				"<ip_addr: dip> <space> "
				"<ip_addr: sip>\n",
				file_name);
	} else {
		printf("generate random values in %s table\n",
			(rule_tbl) ? rules : tuples);
		get_random_rules(*tbl, *n_entries, rule_tbl);
	}
	if (f != NULL)
		fclose(f);
}

static void
parse_opts(int argc, char **argv)
{
	int opt, ret;
	char *endptr;

	while ((opt = getopt(argc, argv, "f:t:n:d:l:r:6b:vpc")) != -1) {
		switch (opt) {
		case 'f':
			config.rules_file = optarg;
			break;
		case 't':
			config.tuples_file = optarg;
			break;
		case 'n':
			errno = 0;
			config.nb_rules = strtoul(optarg, &endptr, 10);
			if ((errno != 0) || (config.nb_rules == 0) ||
					(endptr[0] != 0)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -n\n");
			}
			break;
		case 'd':
			ret = parse_distrib(optarg);
			if (ret != 0) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -d\n");
			}
			break;
		case 'b':
			errno = 0;
			config.burst_sz = strtoul(optarg, &endptr, 10);
			if ((errno != 0) || (config.burst_sz == 0) ||
					(config.burst_sz > BURST_SZ_MAX) ||
					(endptr[0] != 0)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -b\n");
			}
			break;
		case 'l':
			errno = 0;
			config.nb_tuples = strtoul(optarg, &endptr, 10);
			if ((errno != 0) || (config.nb_tuples == 0) ||
					(endptr[0] != 0)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -l\n");
			}
			break;
		case 'r':
			errno = 0;
			config.fract_rnd_tuples = strtoul(optarg, &endptr, 10);
			if ((errno != 0) || (config.fract_rnd_tuples == 0) ||
					(config.fract_rnd_tuples >= 100) ||
					(endptr[0] != 0)) {
				print_usage();
				rte_exit(-EINVAL, "Invalid option -r\n");
			}
			break;
		case '6':
			config.ipv6 = 1;
			break;
		case 'v':
			config.verbose = 1;
			break;
		case 'p':
			config.parallel_lookup = 1;
			break;
		case 'c':
			config.concurrent_rw = 1;
			break;
		default:
			print_usage();
			rte_exit(-EINVAL, "Invalid options\n");
		}
	}
}

static void
print_addr(int af, const void *addr)
{
	char str[INET6_ADDRSTRLEN];
	const char *ret;

	ret = inet_ntop(af, addr, str, sizeof(str));
	if (ret != NULL)
		printf("%s", str);
}

static void
print_tuple(int af, uint32_t spi, const void *dip, const void *sip)
{

	printf("<SPI: %u DIP: ", spi);
	print_addr(af, dip);
	printf(" SIP: ");
	print_addr(af, sip);
	printf(">");
}

static void
print_result(const union rte_ipsec_sad_key *key, void *res)
{
	struct rule *rule = res;
	const struct rte_ipsec_sadv4_key *v4;
	const struct rte_ipsec_sadv6_key *v6;
	const char *spi_only = "SPI_ONLY";
	const char *spi_dip = "SPI_DIP";
	const char *spi_dip_sip = "SPI_DIP_SIP";
	const char *rule_type;
	const void *dip, *sip;
	uint32_t spi;
	int af;

	af = (config.ipv6) ? AF_INET6 : AF_INET;
	v4 = &key->v4;
	v6 = &key->v6;
	spi = (config.ipv6 == 0) ? v4->spi : v6->spi;
	dip = (config.ipv6 == 0) ? &v4->dip : (const void *)v6->dip;
	sip = (config.ipv6 == 0) ? &v4->sip : (const void *)v6->sip;

	if (res == NULL) {
		printf("TUPLE: ");
		print_tuple(af, spi, dip, sip);
		printf(" not found\n");
		return;
	}

	switch (rule->rule_type) {
	case RTE_IPSEC_SAD_SPI_ONLY:
		rule_type = spi_only;
		break;
	case RTE_IPSEC_SAD_SPI_DIP:
		rule_type = spi_dip;
		break;
	case RTE_IPSEC_SAD_SPI_DIP_SIP:
		rule_type = spi_dip_sip;
		break;
	default:
		return;
	}

	print_tuple(af, spi, dip, sip);
	v4 = &rule->tuple.v4;
	v6 = &rule->tuple.v6;
	spi = (config.ipv6 == 0) ? v4->spi : v6->spi;
	dip = (config.ipv6 == 0) ? &v4->dip : (const void *)v6->dip;
	sip = (config.ipv6 == 0) ? &v4->sip : (const void *)v6->sip;
	printf("\n\tpoints to RULE ID %zu ",
		RTE_PTR_DIFF(res, rules_tbl)/sizeof(struct rule));
	print_tuple(af, spi, dip, sip);
	printf(" %s\n", rule_type);
}

static int
lookup(void *arg)
{
	int ret;
	unsigned int i, j;
	const union rte_ipsec_sad_key *keys[BURST_SZ_MAX];
	void *vals[BURST_SZ_MAX];
	uint64_t start, acc = 0;
	uint32_t burst_sz;
	struct rte_ipsec_sad *sad = arg;

	if (config.nb_tuples == 0)
		return 0;

	burst_sz = RTE_MIN(config.burst_sz, config.nb_tuples);
	for (i = 0; i < config.nb_tuples; i += burst_sz) {
		for (j = 0; j < burst_sz; j++)
			keys[j] = (union rte_ipsec_sad_key *)
				(&tuples_tbl[i + j].tuple);
		start = rte_rdtsc_precise();
		ret = rte_ipsec_sad_lookup(sad, keys, vals, burst_sz);
		acc += rte_rdtsc_precise() - start;
		if (ret < 0)
			rte_exit(-EINVAL, "Lookup failed\n");
		if (config.verbose) {
			for (j = 0; j < burst_sz; j++)
				print_result(keys[j], vals[j]);
		}
	}
	acc = (acc == 0) ? UINT64_MAX : acc;
	printf("Average lookup cycles %.2Lf, lookups/sec: %.2Lf\n",
		(long double)acc / config.nb_tuples,
		(long double)config.nb_tuples * rte_get_tsc_hz() / acc);

	return 0;
}

static void
add_rules(struct rte_ipsec_sad *sad, uint32_t fract)
{
	int32_t ret;
	uint32_t i, j, f, fn, n;
	uint64_t start, tm[fract + 1];
	uint32_t nm[fract + 1];

	f = (config.nb_rules > fract) ? config.nb_rules / fract : 1;

	for (n = 0, j = 0; n != config.nb_rules; n = fn, j++) {

		fn = n + f;
		fn = fn > config.nb_rules ? config.nb_rules : fn;

		start = rte_rdtsc_precise();
		for (i = n; i != fn; i++) {
			ret = rte_ipsec_sad_add(sad,
				&rules_tbl[i].tuple,
				rules_tbl[i].rule_type, &rules_tbl[i]);
			if (ret != 0)
				rte_exit(ret, "%s failed @ %u-th rule\n",
					__func__, i);
		}
		tm[j] = rte_rdtsc_precise() - start;
		nm[j] = fn - n;
	}

	for (i = 0; i != j; i++)
		printf("ADD %u rules, %.2Lf cycles/rule, %.2Lf ADD/sec\n",
			nm[i], (long double)tm[i] / nm[i],
			(long double)nm[i] * rte_get_tsc_hz() / tm[i]);
}

static void
del_rules(struct rte_ipsec_sad *sad, uint32_t fract)
{
	int32_t ret;
	uint32_t i, j, f, fn, n;
	uint64_t start, tm[fract + 1];
	uint32_t nm[fract + 1];

	f = (config.nb_rules > fract) ? config.nb_rules / fract : 1;

	for (n = 0, j = 0; n != config.nb_rules; n = fn, j++) {

		fn = n + f;
		fn = fn > config.nb_rules ? config.nb_rules : fn;

		start = rte_rdtsc_precise();
		for (i = n; i != fn; i++) {
			ret = rte_ipsec_sad_del(sad,
				&rules_tbl[i].tuple,
				rules_tbl[i].rule_type);
			if (ret != 0 && ret != -ENOENT)
				rte_exit(ret, "%s failed @ %u-th rule\n",
					__func__, i);
		}
		tm[j] = rte_rdtsc_precise() - start;
		nm[j] = fn - n;
	}

	for (i = 0; i != j; i++)
		printf("DEL %u rules, %.2Lf cycles/rule, %.2Lf DEL/sec\n",
			nm[i], (long double)tm[i] / nm[i],
			(long double)nm[i] * rte_get_tsc_hz() / tm[i]);
}

int
main(int argc, char **argv)
{
	int ret;
	struct rte_ipsec_sad *sad;
	struct rte_ipsec_sad_conf conf = {0};
	unsigned int lcore_id;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	argc -= ret;
	argv += ret;

	config.prgname = argv[0];

	parse_opts(argc, argv);
	tbl_init(&rules_tbl, &config.nb_rules, config.rules_file, 1);
	tbl_init(&tuples_tbl, &config.nb_tuples, config.tuples_file, 0);
	if (config.rules_file != NULL) {
		config.fract_32 = (100 * config.nb_rules_32) / config.nb_rules;
		config.fract_64 = (100 * config.nb_rules_64) / config.nb_rules;
		config.fract_96 = (100 * config.nb_rules_96) / config.nb_rules;
	}
	if (config.tuples_file != NULL) {
		config.fract_rnd_tuples = 0;
		config.nb_tuples_rnd = 0;
	}
	conf.socket_id = -1;
	conf.max_sa[RTE_IPSEC_SAD_SPI_ONLY] = config.nb_rules_32 * 5 / 4;
	conf.max_sa[RTE_IPSEC_SAD_SPI_DIP] = config.nb_rules_64 * 5 / 4;
	conf.max_sa[RTE_IPSEC_SAD_SPI_DIP_SIP] = config.nb_rules_96 * 5 / 4;
	if (config.ipv6)
		conf.flags |= RTE_IPSEC_SAD_FLAG_IPV6;
	if (config.concurrent_rw)
		conf.flags |= RTE_IPSEC_SAD_FLAG_RW_CONCURRENCY;
	sad = rte_ipsec_sad_create("test", &conf);
	if (sad == NULL)
		rte_exit(-rte_errno, "can not allocate SAD table\n");

	print_config();

	add_rules(sad, 10);
	if (config.parallel_lookup)
		rte_eal_mp_remote_launch(lookup, sad, SKIP_MAIN);

	lookup(sad);
	if (config.parallel_lookup)
		RTE_LCORE_FOREACH_WORKER(lcore_id)
			if (rte_eal_wait_lcore(lcore_id) < 0)
				return -1;

	del_rules(sad, 10);

	return 0;
}
