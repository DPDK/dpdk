/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <rte_string_fns.h>
#include <rte_acl.h>
#include <getopt.h>
#include <string.h>

#include <rte_cycles.h>
#include <rte_per_lcore.h>
#include <rte_lcore.h>
#include <rte_ip.h>

#define	PRINT_USAGE_START	"%s [EAL options] --\n"

#define	RTE_LOGTYPE_TESTACL	RTE_LOGTYPE_USER1

#define	APP_NAME	"TESTACL"

#define GET_CB_FIELD(in, fd, base, lim, dlm)	do {            \
	unsigned long val;                                      \
	char *end_fld;                                          \
	errno = 0;                                              \
	val = strtoul((in), &end_fld, (base));                  \
	if (errno != 0 || end_fld[0] != (dlm) || val > (lim))   \
		return -EINVAL;                               \
	(fd) = (typeof(fd))val;                                 \
	(in) = end_fld + 1;                                     \
} while (0)

#define	OPT_RULE_FILE		"rulesf"
#define	OPT_TRACE_FILE		"tracef"
#define	OPT_RULE_NUM		"rulenum"
#define	OPT_TRACE_NUM		"tracenum"
#define	OPT_TRACE_STEP		"tracestep"
#define	OPT_SEARCH_ALG		"alg"
#define	OPT_BLD_CATEGORIES	"bldcat"
#define	OPT_RUN_CATEGORIES	"runcat"
#define	OPT_MAX_SIZE		"maxsize"
#define	OPT_ITER_NUM		"iter"
#define	OPT_VERBOSE		"verbose"
#define	OPT_IPV6		"ipv6"

#define	TRACE_DEFAULT_NUM	0x10000
#define	TRACE_STEP_MAX		0x1000
#define	TRACE_STEP_DEF		0x100

#define	RULE_NUM		0x10000

#define COMMENT_LEAD_CHAR	'#'

enum {
	DUMP_NONE,
	DUMP_SEARCH,
	DUMP_PKT,
	DUMP_MAX
};

struct acl_alg {
	const char *name;
	enum rte_acl_classify_alg alg;
};

static const struct acl_alg acl_alg[] = {
	{
		.name = "scalar",
		.alg = RTE_ACL_CLASSIFY_SCALAR,
	},
	{
		.name = "sse",
		.alg = RTE_ACL_CLASSIFY_SSE,
	},
	{
		.name = "avx2",
		.alg = RTE_ACL_CLASSIFY_AVX2,
	},
	{
		.name = "neon",
		.alg = RTE_ACL_CLASSIFY_NEON,
	},
	{
		.name = "altivec",
		.alg = RTE_ACL_CLASSIFY_ALTIVEC,
	},
	{
		.name = "avx512x16",
		.alg = RTE_ACL_CLASSIFY_AVX512X16,
	},
	{
		.name = "avx512x32",
		.alg = RTE_ACL_CLASSIFY_AVX512X32,
	},
};

static struct {
	const char         *prgname;
	const char         *rule_file;
	const char         *trace_file;
	size_t              max_size;
	uint32_t            bld_categories;
	uint32_t            run_categories;
	uint32_t            nb_rules;
	uint32_t            nb_traces;
	uint32_t            trace_step;
	uint32_t            trace_sz;
	uint32_t            iter_num;
	uint32_t            verbose;
	uint32_t            ipv6;
	struct acl_alg      alg;
	uint32_t            used_traces;
	void               *traces;
	struct rte_acl_ctx *acx;
} config = {
	.bld_categories = 3,
	.run_categories = 1,
	.nb_rules = RULE_NUM,
	.nb_traces = TRACE_DEFAULT_NUM,
	.trace_step = TRACE_STEP_DEF,
	.iter_num = 1,
	.verbose = DUMP_MAX,
	.alg = {
		.name = "default",
		.alg = RTE_ACL_CLASSIFY_DEFAULT,
	},
	.ipv6 = 0
};

static struct rte_acl_param prm = {
	.name = APP_NAME,
	.socket_id = SOCKET_ID_ANY,
};

/*
 * Rule and trace formats definitions.
 */

struct ipv4_5tuple {
	uint8_t  proto;
	uint32_t ip_src;
	uint32_t ip_dst;
	uint16_t port_src;
	uint16_t port_dst;
};

enum {
	PROTO_FIELD_IPV4,
	SRC_FIELD_IPV4,
	DST_FIELD_IPV4,
	SRCP_FIELD_IPV4,
	DSTP_FIELD_IPV4,
	NUM_FIELDS_IPV4
};

/*
 * That effectively defines order of IPV4VLAN classifications:
 *  - PROTO
 *  - VLAN (TAG and DOMAIN)
 *  - SRC IP ADDRESS
 *  - DST IP ADDRESS
 *  - PORTS (SRC and DST)
 */
enum {
	RTE_ACL_IPV4VLAN_PROTO,
	RTE_ACL_IPV4VLAN_VLAN,
	RTE_ACL_IPV4VLAN_SRC,
	RTE_ACL_IPV4VLAN_DST,
	RTE_ACL_IPV4VLAN_PORTS,
	RTE_ACL_IPV4VLAN_NUM
};

struct rte_acl_field_def ipv4_defs[NUM_FIELDS_IPV4] = {
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PROTO,
		.offset = offsetof(struct ipv4_5tuple, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_SRC,
		.offset = offsetof(struct ipv4_5tuple, ip_src),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_DST,
		.offset = offsetof(struct ipv4_5tuple, ip_dst),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = offsetof(struct ipv4_5tuple, port_src),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = offsetof(struct ipv4_5tuple, port_dst),
	},
};

#define	IPV6_ADDR_LEN	16
#define	IPV6_ADDR_U16	(IPV6_ADDR_LEN / sizeof(uint16_t))
#define	IPV6_ADDR_U32	(IPV6_ADDR_LEN / sizeof(uint32_t))

struct ipv6_5tuple {
	uint8_t  proto;
	uint32_t ip_src[IPV6_ADDR_U32];
	uint32_t ip_dst[IPV6_ADDR_U32];
	uint16_t port_src;
	uint16_t port_dst;
};

enum {
	PROTO_FIELD_IPV6,
	SRC1_FIELD_IPV6,
	SRC2_FIELD_IPV6,
	SRC3_FIELD_IPV6,
	SRC4_FIELD_IPV6,
	DST1_FIELD_IPV6,
	DST2_FIELD_IPV6,
	DST3_FIELD_IPV6,
	DST4_FIELD_IPV6,
	SRCP_FIELD_IPV6,
	DSTP_FIELD_IPV6,
	NUM_FIELDS_IPV6
};

struct rte_acl_field_def ipv6_defs[NUM_FIELDS_IPV6] = {
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV6,
		.input_index = PROTO_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC1_FIELD_IPV6,
		.input_index = SRC1_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_src[0]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC2_FIELD_IPV6,
		.input_index = SRC2_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_src[1]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC3_FIELD_IPV6,
		.input_index = SRC3_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_src[2]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC4_FIELD_IPV6,
		.input_index = SRC4_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_src[3]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST1_FIELD_IPV6,
		.input_index = DST1_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_dst[0]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST2_FIELD_IPV6,
		.input_index = DST2_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_dst[1]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST3_FIELD_IPV6,
		.input_index = DST3_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_dst[2]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST4_FIELD_IPV6,
		.input_index = DST4_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, ip_dst[3]),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV6,
		.input_index = SRCP_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, port_src),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV6,
		.input_index = SRCP_FIELD_IPV6,
		.offset = offsetof(struct ipv6_5tuple, port_dst),
	},
};


enum {
	CB_FLD_SRC_ADDR,
	CB_FLD_DST_ADDR,
	CB_FLD_SRC_PORT_LOW,
	CB_FLD_SRC_PORT_DLM,
	CB_FLD_SRC_PORT_HIGH,
	CB_FLD_DST_PORT_LOW,
	CB_FLD_DST_PORT_DLM,
	CB_FLD_DST_PORT_HIGH,
	CB_FLD_PROTO,
	CB_FLD_NUM,
};

enum {
	CB_TRC_SRC_ADDR,
	CB_TRC_DST_ADDR,
	CB_TRC_SRC_PORT,
	CB_TRC_DST_PORT,
	CB_TRC_PROTO,
	CB_TRC_NUM,
};

RTE_ACL_RULE_DEF(acl_rule, RTE_ACL_MAX_FIELDS);

static const char cb_port_delim[] = ":";

static char line[LINE_MAX];

#define	dump_verbose(lvl, fh, fmt, args...)	do { \
	if ((lvl) <= (int32_t)config.verbose)        \
		fprintf(fh, fmt, ##args);            \
} while (0)


/*
 * Parse ClassBench input trace (test vectors and expected results) file.
 * Expected format:
 * <src_ipv4_addr> <space> <dst_ipv4_addr> <space> \
 * <src_port> <space> <dst_port> <space> <proto>
 */
static int
parse_cb_ipv4_trace(char *str, struct ipv4_5tuple *v)
{
	int i;
	char *s, *sp, *in[CB_TRC_NUM];
	static const char *dlm = " \t\n";

	s = str;
	for (i = 0; i != RTE_DIM(in); i++) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
		s = NULL;
	}

	GET_CB_FIELD(in[CB_TRC_SRC_ADDR], v->ip_src, 0, UINT32_MAX, 0);
	GET_CB_FIELD(in[CB_TRC_DST_ADDR], v->ip_dst, 0, UINT32_MAX, 0);
	GET_CB_FIELD(in[CB_TRC_SRC_PORT], v->port_src, 0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_TRC_DST_PORT], v->port_dst, 0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_TRC_PROTO], v->proto, 0, UINT8_MAX, 0);

	/* convert to network byte order. */
	v->ip_src = rte_cpu_to_be_32(v->ip_src);
	v->ip_dst = rte_cpu_to_be_32(v->ip_dst);
	v->port_src = rte_cpu_to_be_16(v->port_src);
	v->port_dst = rte_cpu_to_be_16(v->port_dst);

	return 0;
}

/*
 * Parse IPv6 address, expects the following format:
 * XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX:XXXX (where X is a hexadecimal digit).
 */
static int
parse_ipv6_addr(const char *in, const char **end, uint32_t v[IPV6_ADDR_U32],
	char dlm)
{
	uint32_t addr[IPV6_ADDR_U16];

	GET_CB_FIELD(in, addr[0], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[1], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[2], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[3], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[4], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[5], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[6], 16, UINT16_MAX, ':');
	GET_CB_FIELD(in, addr[7], 16, UINT16_MAX, dlm);

	*end = in;

	v[0] = (addr[0] << 16) + addr[1];
	v[1] = (addr[2] << 16) + addr[3];
	v[2] = (addr[4] << 16) + addr[5];
	v[3] = (addr[6] << 16) + addr[7];

	return 0;
}

static int
parse_cb_ipv6_addr_trace(const char *in, uint32_t v[IPV6_ADDR_U32])
{
	int32_t rc;
	const char *end;

	rc = parse_ipv6_addr(in, &end, v, 0);
	if (rc != 0)
		return rc;

	v[0] = rte_cpu_to_be_32(v[0]);
	v[1] = rte_cpu_to_be_32(v[1]);
	v[2] = rte_cpu_to_be_32(v[2]);
	v[3] = rte_cpu_to_be_32(v[3]);

	return 0;
}

/*
 * Parse ClassBench input trace (test vectors and expected results) file.
 * Expected format:
 * <src_ipv6_addr> <space> <dst_ipv6_addr> <space> \
 * <src_port> <space> <dst_port> <space> <proto>
 */
static int
parse_cb_ipv6_trace(char *str, struct ipv6_5tuple *v)
{
	int32_t i, rc;
	char *s, *sp, *in[CB_TRC_NUM];
	static const char *dlm = " \t\n";

	s = str;
	for (i = 0; i != RTE_DIM(in); i++) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
		s = NULL;
	}

	/* get ip6 src address. */
	rc = parse_cb_ipv6_addr_trace(in[CB_TRC_SRC_ADDR], v->ip_src);
	if (rc != 0)
		return rc;

	/* get ip6 dst address. */
	rc = parse_cb_ipv6_addr_trace(in[CB_TRC_DST_ADDR], v->ip_dst);
	if (rc != 0)
		return rc;

	GET_CB_FIELD(in[CB_TRC_SRC_PORT], v->port_src, 0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_TRC_DST_PORT], v->port_dst, 0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_TRC_PROTO], v->proto, 0, UINT8_MAX, 0);

	/* convert to network byte order. */
	v->port_src = rte_cpu_to_be_16(v->port_src);
	v->port_dst = rte_cpu_to_be_16(v->port_dst);

	return 0;
}

/* Bypass comment and empty lines */
static int
skip_line(const char *buf)
{
	uint32_t i;

	for (i = 0; isspace(buf[i]) != 0; i++)
		;

	if (buf[i] == 0 || buf[i] == COMMENT_LEAD_CHAR)
		return 1;

	return 0;
}

static void
tracef_init(void)
{
	static const char name[] = APP_NAME;
	FILE *f;
	size_t sz;
	uint32_t i, k, n;
	struct ipv4_5tuple *v;
	struct ipv6_5tuple *w;

	sz = config.nb_traces * (config.ipv6 ? sizeof(*w) : sizeof(*v));
	config.traces = rte_zmalloc_socket(name, sz, RTE_CACHE_LINE_SIZE,
			SOCKET_ID_ANY);
	if (config.traces == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate %zu bytes for "
			"requested %u number of trace records\n",
			sz, config.nb_traces);

	f = fopen(config.trace_file, "r");
	if (f == NULL)
		rte_exit(-EINVAL, "failed to open file: %s\n",
			config.trace_file);

	v = config.traces;
	w = config.traces;
	k = 0;
	n = 0;
	for (i = 0; n != config.nb_traces; i++) {

		if (fgets(line, sizeof(line), f) == NULL)
			break;

		if (skip_line(line) != 0) {
			k++;
			continue;
		}

		n = i - k;

		if (config.ipv6) {
			if (parse_cb_ipv6_trace(line, w + n) != 0)
				rte_exit(EXIT_FAILURE,
					"%s: failed to parse ipv6 trace "
					"record at line %u\n",
					config.trace_file, i + 1);
		} else {
			if (parse_cb_ipv4_trace(line, v + n) != 0)
				rte_exit(EXIT_FAILURE,
					"%s: failed to parse ipv4 trace "
					"record at line %u\n",
					config.trace_file, i + 1);
		}
	}

	config.used_traces = i - k;
	fclose(f);
}

static int
parse_ipv6_net(const char *in, struct rte_acl_field field[4])
{
	int32_t rc;
	const char *mp;
	uint32_t i, m, v[4];
	const uint32_t nbu32 = sizeof(uint32_t) * CHAR_BIT;

	/* get address. */
	rc = parse_ipv6_addr(in, &mp, v, '/');
	if (rc != 0)
		return rc;

	/* get mask. */
	GET_CB_FIELD(mp, m, 0, CHAR_BIT * sizeof(v), 0);

	/* put all together. */
	for (i = 0; i != RTE_DIM(v); i++) {
		if (m >= (i + 1) * nbu32)
			field[i].mask_range.u32 = nbu32;
		else
			field[i].mask_range.u32 = m > (i * nbu32) ?
				m - (i * 32) : 0;

		field[i].value.u32 = v[i];
	}

	return 0;
}


static int
parse_cb_ipv6_rule(char *str, struct acl_rule *v)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";

	/*
	 * Skip leading '@'
	 */
	if (strchr(str, '@') != str)
		return -EINVAL;

	s = str + 1;

	for (i = 0; i != RTE_DIM(in); i++) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
		s = NULL;
	}

	rc = parse_ipv6_net(in[CB_FLD_SRC_ADDR], v->field + SRC1_FIELD_IPV6);
	if (rc != 0) {
		RTE_LOG(ERR, TESTACL,
			"failed to read source address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	rc = parse_ipv6_net(in[CB_FLD_DST_ADDR], v->field + DST1_FIELD_IPV6);
	if (rc != 0) {
		RTE_LOG(ERR, TESTACL,
			"failed to read destination address/mask: %s\n",
			in[CB_FLD_DST_ADDR]);
		return rc;
	}

	/* source port. */
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_LOW],
		v->field[SRCP_FIELD_IPV6].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_HIGH],
		v->field[SRCP_FIELD_IPV6].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	/* destination port. */
	GET_CB_FIELD(in[CB_FLD_DST_PORT_LOW],
		v->field[DSTP_FIELD_IPV6].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_DST_PORT_HIGH],
		v->field[DSTP_FIELD_IPV6].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_DST_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV6].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV6].mask_range.u8,
		0, UINT8_MAX, 0);

	return 0;
}

static int
parse_ipv4_net(const char *in, uint32_t *addr, uint32_t *mask_len)
{
	uint8_t a, b, c, d, m;

	GET_CB_FIELD(in, a, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, b, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, c, 0, UINT8_MAX, '.');
	GET_CB_FIELD(in, d, 0, UINT8_MAX, '/');
	GET_CB_FIELD(in, m, 0, sizeof(uint32_t) * CHAR_BIT, 0);

	addr[0] = RTE_IPV4(a, b, c, d);
	mask_len[0] = m;

	return 0;
}
/*
 * Parse ClassBench rules file.
 * Expected format:
 * '@'<src_ipv4_addr>'/'<masklen> <space> \
 * <dst_ipv4_addr>'/'<masklen> <space> \
 * <src_port_low> <space> ":" <src_port_high> <space> \
 * <dst_port_low> <space> ":" <dst_port_high> <space> \
 * <proto>'/'<mask>
 */
static int
parse_cb_ipv4_rule(char *str, struct acl_rule *v)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";

	/*
	 * Skip leading '@'
	 */
	if (strchr(str, '@') != str)
		return -EINVAL;

	s = str + 1;

	for (i = 0; i != RTE_DIM(in); i++) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
		s = NULL;
	}

	rc = parse_ipv4_net(in[CB_FLD_SRC_ADDR],
			&v->field[SRC_FIELD_IPV4].value.u32,
			&v->field[SRC_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		RTE_LOG(ERR, TESTACL,
			"failed to read source address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	rc = parse_ipv4_net(in[CB_FLD_DST_ADDR],
			&v->field[DST_FIELD_IPV4].value.u32,
			&v->field[DST_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		RTE_LOG(ERR, TESTACL,
			"failed to read destination address/mask: %s\n",
			in[CB_FLD_DST_ADDR]);
		return rc;
	}

	/* source port. */
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_LOW],
		v->field[SRCP_FIELD_IPV4].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_HIGH],
		v->field[SRCP_FIELD_IPV4].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	/* destination port. */
	GET_CB_FIELD(in[CB_FLD_DST_PORT_LOW],
		v->field[DSTP_FIELD_IPV4].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_DST_PORT_HIGH],
		v->field[DSTP_FIELD_IPV4].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_DST_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0)
		return -EINVAL;

	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV4].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV4].mask_range.u8,
		0, UINT8_MAX, 0);

	return 0;
}

typedef int (*parse_5tuple)(char *text, struct acl_rule *rule);

static int
add_cb_rules(FILE *f, struct rte_acl_ctx *ctx)
{
	int rc;
	uint32_t i, k, n;
	struct acl_rule v;
	parse_5tuple parser;

	memset(&v, 0, sizeof(v));
	parser = (config.ipv6 != 0) ? parse_cb_ipv6_rule : parse_cb_ipv4_rule;

	k = 0;
	for (i = 1; fgets(line, sizeof(line), f) != NULL; i++) {

		if (skip_line(line) != 0) {
			k++;
			continue;
		}

		n = i - k;
		rc = parser(line, &v);
		if (rc != 0) {
			RTE_LOG(ERR, TESTACL, "line %u: parse_cb_ipv4vlan_rule"
				" failed, error code: %d (%s)\n",
				i, rc, strerror(-rc));
			return rc;
		}

		v.data.category_mask = RTE_LEN2MASK(RTE_ACL_MAX_CATEGORIES,
			typeof(v.data.category_mask));
		v.data.priority = RTE_ACL_MAX_PRIORITY - n;
		v.data.userdata = n;

		rc = rte_acl_add_rules(ctx, (struct rte_acl_rule *)&v, 1);
		if (rc != 0) {
			RTE_LOG(ERR, TESTACL, "line %u: failed to add rules "
				"into ACL context, error code: %d (%s)\n",
				i, rc, strerror(-rc));
			return rc;
		}
	}

	return 0;
}

static void
acx_init(void)
{
	int ret;
	FILE *f;
	struct rte_acl_config cfg;

	memset(&cfg, 0, sizeof(cfg));

	/* setup ACL build config. */
	if (config.ipv6) {
		cfg.num_fields = RTE_DIM(ipv6_defs);
		memcpy(&cfg.defs, ipv6_defs, sizeof(ipv6_defs));
	} else {
		cfg.num_fields = RTE_DIM(ipv4_defs);
		memcpy(&cfg.defs, ipv4_defs, sizeof(ipv4_defs));
	}
	cfg.num_categories = config.bld_categories;
	cfg.max_size = config.max_size;

	/* setup ACL creation parameters. */
	prm.rule_size = RTE_ACL_RULE_SZ(cfg.num_fields);
	prm.max_rule_num = config.nb_rules;

	config.acx = rte_acl_create(&prm);
	if (config.acx == NULL)
		rte_exit(rte_errno, "failed to create ACL context\n");

	/* set default classify method for this context. */
	if (config.alg.alg != RTE_ACL_CLASSIFY_DEFAULT) {
		ret = rte_acl_set_ctx_classify(config.acx, config.alg.alg);
		if (ret != 0)
			rte_exit(ret, "failed to setup %s method "
				"for ACL context\n", config.alg.name);
	}

	/* add ACL rules. */
	f = fopen(config.rule_file, "r");
	if (f == NULL)
		rte_exit(-EINVAL, "failed to open file %s\n",
			config.rule_file);

	ret = add_cb_rules(f, config.acx);
	if (ret != 0)
		rte_exit(ret, "failed to add rules into ACL context\n");

	fclose(f);

	/* perform build. */
	ret = rte_acl_build(config.acx, &cfg);

	dump_verbose(DUMP_NONE, stdout,
		"rte_acl_build(%u) finished with %d\n",
		config.bld_categories, ret);

	rte_acl_dump(config.acx);

	if (ret != 0)
		rte_exit(ret, "failed to build search context\n");
}

static uint32_t
search_ip5tuples_once(uint32_t categories, uint32_t step, const char *alg)
{
	int ret;
	uint32_t i, j, k, n, r;
	const uint8_t *data[step], *v;
	uint32_t results[step * categories];

	v = config.traces;
	for (i = 0; i != config.used_traces; i += n) {

		n = RTE_MIN(step, config.used_traces - i);

		for (j = 0; j != n; j++) {
			data[j] = v;
			v += config.trace_sz;
		}

		ret = rte_acl_classify(config.acx, data, results,
			n, categories);

		if (ret != 0)
			rte_exit(ret, "classify for ipv%c_5tuples returns %d\n",
				config.ipv6 ? '6' : '4', ret);

		for (r = 0, j = 0; j != n; j++) {
			for (k = 0; k != categories; k++, r++) {
				dump_verbose(DUMP_PKT, stdout,
					"ipv%c_5tuple: %u, category: %u, "
					"result: %u\n",
					config.ipv6 ? '6' : '4',
					i + j + 1, k, results[r] - 1);
			}

		}
	}

	dump_verbose(DUMP_SEARCH, stdout,
		"%s(%u, %u, %s) returns %u\n", __func__,
		categories, step, alg, i);
	return i;
}

static int
search_ip5tuples(__rte_unused void *arg)
{
	uint64_t pkt, start, tm;
	uint32_t i, lcore;
	long double st;

	lcore = rte_lcore_id();
	start = rte_rdtsc_precise();
	pkt = 0;

	for (i = 0; i != config.iter_num; i++) {
		pkt += search_ip5tuples_once(config.run_categories,
			config.trace_step, config.alg.name);
	}

	tm = rte_rdtsc_precise() - start;

	st = (long double)tm / rte_get_timer_hz();
	dump_verbose(DUMP_NONE, stdout,
		"%s  @lcore %u: %" PRIu32 " iterations, %" PRIu64 " pkts, %"
		PRIu32 " categories, %" PRIu64 " cycles (%.2Lf sec), "
		"%.2Lf cycles/pkt, %.2Lf pkt/sec\n",
		__func__, lcore, i, pkt,
		config.run_categories, tm, st,
		(pkt == 0) ? 0 : (long double)tm / pkt, pkt / st);

	return 0;
}

static unsigned long
get_ulong_opt(const char *opt, const char *name, size_t min, size_t max)
{
	unsigned long val;
	char *end;

	errno = 0;
	val = strtoul(opt, &end, 0);
	if (errno != 0 || end[0] != 0 || val > max || val < min)
		rte_exit(-EINVAL, "invalid value: \"%s\" for option: %s\n",
			opt, name);
	return val;
}

static void
get_alg_opt(const char *opt, const char *name)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(acl_alg); i++) {
		if (strcmp(opt, acl_alg[i].name) == 0) {
			config.alg = acl_alg[i];
			return;
		}
	}

	rte_exit(-EINVAL, "invalid value: \"%s\" for option: %s\n",
		opt, name);
}

static void
print_usage(const char *prgname)
{
	uint32_t i, n, rc;
	char buf[PATH_MAX];

	n = 0;
	buf[0] = 0;

	for (i = 0; i < RTE_DIM(acl_alg) - 1; i++) {
		rc = snprintf(buf + n, sizeof(buf) - n, "%s|",
			acl_alg[i].name);
		if (rc > sizeof(buf) - n)
			break;
		n += rc;
	}

	strlcpy(buf + n, acl_alg[i].name, sizeof(buf) - n);

	fprintf(stdout,
		PRINT_USAGE_START
		"--" OPT_RULE_FILE "=<rules set file>\n"
		"[--" OPT_TRACE_FILE "=<input traces file>]\n"
		"[--" OPT_RULE_NUM
			"=<maximum number of rules for ACL context>]\n"
		"[--" OPT_TRACE_NUM
			"=<number of traces to read binary file in>]\n"
		"[--" OPT_TRACE_STEP
			"=<number of traces to classify per one call>]\n"
		"[--" OPT_BLD_CATEGORIES
			"=<number of categories to build with>]\n"
		"[--" OPT_RUN_CATEGORIES
			"=<number of categories to run with> "
			"should be either 1 or multiple of %zu, "
			"but not greater then %u]\n"
		"[--" OPT_MAX_SIZE
			"=<size limit (in bytes) for runtime ACL structures> "
			"leave 0 for default behaviour]\n"
		"[--" OPT_ITER_NUM "=<number of iterations to perform>]\n"
		"[--" OPT_VERBOSE "=<verbose level>]\n"
		"[--" OPT_SEARCH_ALG "=%s]\n"
		"[--" OPT_IPV6 "=<IPv6 rules and trace files>]\n",
		prgname, RTE_ACL_RESULTS_MULTIPLIER,
		(uint32_t)RTE_ACL_MAX_CATEGORIES,
		buf);
}

static void
dump_config(FILE *f)
{
	fprintf(f, "%s:\n", __func__);
	fprintf(f, "%s:%s\n", OPT_RULE_FILE, config.rule_file);
	fprintf(f, "%s:%s\n", OPT_TRACE_FILE, config.trace_file);
	fprintf(f, "%s:%u\n", OPT_RULE_NUM, config.nb_rules);
	fprintf(f, "%s:%u\n", OPT_TRACE_NUM, config.nb_traces);
	fprintf(f, "%s:%u\n", OPT_TRACE_STEP, config.trace_step);
	fprintf(f, "%s:%u\n", OPT_BLD_CATEGORIES, config.bld_categories);
	fprintf(f, "%s:%u\n", OPT_RUN_CATEGORIES, config.run_categories);
	fprintf(f, "%s:%zu\n", OPT_MAX_SIZE, config.max_size);
	fprintf(f, "%s:%u\n", OPT_ITER_NUM, config.iter_num);
	fprintf(f, "%s:%u\n", OPT_VERBOSE, config.verbose);
	fprintf(f, "%s:%u(%s)\n", OPT_SEARCH_ALG, config.alg.alg,
		config.alg.name);
	fprintf(f, "%s:%u\n", OPT_IPV6, config.ipv6);
}

static void
check_config(void)
{
	if (config.rule_file == NULL) {
		print_usage(config.prgname);
		rte_exit(-EINVAL, "mandatory option %s is not specified\n",
			OPT_RULE_FILE);
	}
}


static void
get_input_opts(int argc, char **argv)
{
	static struct option lgopts[] = {
		{OPT_RULE_FILE, 1, 0, 0},
		{OPT_TRACE_FILE, 1, 0, 0},
		{OPT_TRACE_NUM, 1, 0, 0},
		{OPT_RULE_NUM, 1, 0, 0},
		{OPT_MAX_SIZE, 1, 0, 0},
		{OPT_TRACE_STEP, 1, 0, 0},
		{OPT_BLD_CATEGORIES, 1, 0, 0},
		{OPT_RUN_CATEGORIES, 1, 0, 0},
		{OPT_ITER_NUM, 1, 0, 0},
		{OPT_VERBOSE, 1, 0, 0},
		{OPT_SEARCH_ALG, 1, 0, 0},
		{OPT_IPV6, 0, 0, 0},
		{NULL, 0, 0, 0}
	};

	int opt, opt_idx;

	while ((opt = getopt_long(argc, argv, "", lgopts,  &opt_idx)) != EOF) {

		if (opt != 0) {
			print_usage(config.prgname);
			rte_exit(-EINVAL, "unknown option: %c", opt);
		}

		if (strcmp(lgopts[opt_idx].name, OPT_RULE_FILE) == 0) {
			config.rule_file = optarg;
		} else if (strcmp(lgopts[opt_idx].name, OPT_TRACE_FILE) == 0) {
			config.trace_file = optarg;
		} else if (strcmp(lgopts[opt_idx].name, OPT_RULE_NUM) == 0) {
			config.nb_rules = get_ulong_opt(optarg,
				lgopts[opt_idx].name, 1, RTE_ACL_MAX_INDEX + 1);
		} else if (strcmp(lgopts[opt_idx].name, OPT_MAX_SIZE) == 0) {
			config.max_size = get_ulong_opt(optarg,
				lgopts[opt_idx].name, 0, SIZE_MAX);
		} else if (strcmp(lgopts[opt_idx].name, OPT_TRACE_NUM) == 0) {
			config.nb_traces = get_ulong_opt(optarg,
				lgopts[opt_idx].name, 1, UINT32_MAX);
		} else if (strcmp(lgopts[opt_idx].name, OPT_TRACE_STEP) == 0) {
			config.trace_step = get_ulong_opt(optarg,
				lgopts[opt_idx].name, 1, TRACE_STEP_MAX);
		} else if (strcmp(lgopts[opt_idx].name,
				OPT_BLD_CATEGORIES) == 0) {
			config.bld_categories = get_ulong_opt(optarg,
				lgopts[opt_idx].name, 1,
				RTE_ACL_MAX_CATEGORIES);
		} else if (strcmp(lgopts[opt_idx].name,
				OPT_RUN_CATEGORIES) == 0) {
			config.run_categories = get_ulong_opt(optarg,
				lgopts[opt_idx].name, 1,
				RTE_ACL_MAX_CATEGORIES);
		} else if (strcmp(lgopts[opt_idx].name, OPT_ITER_NUM) == 0) {
			config.iter_num = get_ulong_opt(optarg,
				lgopts[opt_idx].name, 1, INT32_MAX);
		} else if (strcmp(lgopts[opt_idx].name, OPT_VERBOSE) == 0) {
			config.verbose = get_ulong_opt(optarg,
				lgopts[opt_idx].name, DUMP_NONE, DUMP_MAX);
		} else if (strcmp(lgopts[opt_idx].name,
				OPT_SEARCH_ALG) == 0) {
			get_alg_opt(optarg, lgopts[opt_idx].name);
		} else if (strcmp(lgopts[opt_idx].name, OPT_IPV6) == 0) {
			config.ipv6 = 1;
		}
	}
	config.trace_sz = config.ipv6 ? sizeof(struct ipv6_5tuple) :
						sizeof(struct ipv4_5tuple);

}

int
main(int argc, char **argv)
{
	int ret;
	uint32_t lcore;

	ret = rte_eal_init(argc, argv);
	if (ret < 0)
		rte_panic("Cannot init EAL\n");

	argc -= ret;
	argv += ret;

	config.prgname = argv[0];

	get_input_opts(argc, argv);
	dump_config(stdout);
	check_config();

	acx_init();

	if (config.trace_file != NULL)
		tracef_init();

	RTE_LCORE_FOREACH_WORKER(lcore)
		 rte_eal_remote_launch(search_ip5tuples, NULL, lcore);

	search_ip5tuples(NULL);

	rte_eal_mp_wait_lcore();

	rte_acl_free(config.acx);
	return 0;
}
