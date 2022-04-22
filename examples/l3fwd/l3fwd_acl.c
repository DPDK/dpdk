/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#include "l3fwd.h"
#include "l3fwd_route.h"

/*
 * Rule and trace formats definitions.
 */

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

struct acl_algorithms acl_alg[] = {
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

struct rte_acl_field_def ipv4_defs[NUM_FIELDS_IPV4] = {
	{
		.type = RTE_ACL_FIELD_TYPE_BITMASK,
		.size = sizeof(uint8_t),
		.field_index = PROTO_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PROTO,
		.offset = 0,
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_SRC,
		.offset = offsetof(struct rte_ipv4_hdr, src_addr) -
			offsetof(struct rte_ipv4_hdr, next_proto_id),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_DST,
		.offset = offsetof(struct rte_ipv4_hdr, dst_addr) -
			offsetof(struct rte_ipv4_hdr, next_proto_id),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = sizeof(struct rte_ipv4_hdr) -
			offsetof(struct rte_ipv4_hdr, next_proto_id),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV4,
		.input_index = RTE_ACL_IPV4VLAN_PORTS,
		.offset = sizeof(struct rte_ipv4_hdr) -
			offsetof(struct rte_ipv4_hdr, next_proto_id) +
			sizeof(uint16_t),
	},
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
		.offset = 0,
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC1_FIELD_IPV6,
		.input_index = SRC1_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, src_addr) -
			offsetof(struct rte_ipv6_hdr, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC2_FIELD_IPV6,
		.input_index = SRC2_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, src_addr) -
			offsetof(struct rte_ipv6_hdr, proto) + sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC3_FIELD_IPV6,
		.input_index = SRC3_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, src_addr) -
			offsetof(struct rte_ipv6_hdr, proto) +
			2 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = SRC4_FIELD_IPV6,
		.input_index = SRC4_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, src_addr) -
			offsetof(struct rte_ipv6_hdr, proto) +
			3 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST1_FIELD_IPV6,
		.input_index = DST1_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, dst_addr)
				- offsetof(struct rte_ipv6_hdr, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST2_FIELD_IPV6,
		.input_index = DST2_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, dst_addr) -
			offsetof(struct rte_ipv6_hdr, proto) + sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST3_FIELD_IPV6,
		.input_index = DST3_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, dst_addr) -
			offsetof(struct rte_ipv6_hdr, proto) +
			2 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_MASK,
		.size = sizeof(uint32_t),
		.field_index = DST4_FIELD_IPV6,
		.input_index = DST4_FIELD_IPV6,
		.offset = offsetof(struct rte_ipv6_hdr, dst_addr) -
			offsetof(struct rte_ipv6_hdr, proto) +
			3 * sizeof(uint32_t),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = SRCP_FIELD_IPV6,
		.input_index = SRCP_FIELD_IPV6,
		.offset = sizeof(struct rte_ipv6_hdr) -
			offsetof(struct rte_ipv6_hdr, proto),
	},
	{
		.type = RTE_ACL_FIELD_TYPE_RANGE,
		.size = sizeof(uint16_t),
		.field_index = DSTP_FIELD_IPV6,
		.input_index = SRCP_FIELD_IPV6,
		.offset = sizeof(struct rte_ipv6_hdr) -
			offsetof(struct rte_ipv6_hdr, proto) + sizeof(uint16_t),
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
	CB_FLD_USERDATA,
	CB_FLD_NUM,
};

RTE_ACL_RULE_DEF(acl4_rule, RTE_DIM(ipv4_defs));
RTE_ACL_RULE_DEF(acl6_rule, RTE_DIM(ipv6_defs));

struct acl_search_t {
	const uint8_t *data_ipv4[MAX_PKT_BURST];
	struct rte_mbuf *m_ipv4[MAX_PKT_BURST];
	uint32_t res_ipv4[MAX_PKT_BURST];
	int num_ipv4;

	const uint8_t *data_ipv6[MAX_PKT_BURST];
	struct rte_mbuf *m_ipv6[MAX_PKT_BURST];
	uint32_t res_ipv6[MAX_PKT_BURST];
	int num_ipv6;
};

static struct {
	struct rte_acl_ctx *acx_ipv4[NB_SOCKETS];
	struct rte_acl_ctx *acx_ipv6[NB_SOCKETS];
#ifdef L3FWDACL_DEBUG
	struct acl4_rule *rule_ipv4;
	struct acl6_rule *rule_ipv6;
#endif
} acl_config;

static const char cb_port_delim[] = ":";

static struct rte_acl_rule *acl_base_ipv4, *route_base_ipv4,
		*acl_base_ipv6, *route_base_ipv6;
static unsigned int acl_num_ipv4, route_num_ipv4,
		acl_num_ipv6, route_num_ipv6;

#include "l3fwd_acl.h"

#include "l3fwd_acl_scalar.h"

/*
 * Parse IPV6 address, expects the following format:
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
parse_cb_ipv6_rule(char *str, struct rte_acl_rule *v, int has_userdata)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";
	int dim = has_userdata ? CB_FLD_NUM : CB_FLD_USERDATA;
	s = str;

	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	rc = parse_ipv6_net(in[CB_FLD_SRC_ADDR], v->field + SRC1_FIELD_IPV6);
	if (rc != 0) {
		acl_log("failed to read source address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	rc = parse_ipv6_net(in[CB_FLD_DST_ADDR], v->field + DST1_FIELD_IPV6);
	if (rc != 0) {
		acl_log("failed to read destination address/mask: %s\n",
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

	if (v->field[SRCP_FIELD_IPV6].mask_range.u16
			< v->field[SRCP_FIELD_IPV6].value.u16
			|| v->field[DSTP_FIELD_IPV6].mask_range.u16
			< v->field[DSTP_FIELD_IPV6].value.u16)
		return -EINVAL;

	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV6].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV6].mask_range.u8,
		0, UINT8_MAX, 0);

	if (has_userdata)
		GET_CB_FIELD(in[CB_FLD_USERDATA], v->data.userdata,
			0, UINT32_MAX, 0);

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
parse_ipv4_net(char *in, uint32_t *addr, uint32_t *mask_len)
{
	char *sa, *sm, *sv;
	const char *dlm =  "/";

	sv = NULL;
	sa = strtok_r(in, dlm, &sv);
	if (sa == NULL)
		return -EINVAL;
	sm = strtok_r(NULL, dlm, &sv);
	if (sm == NULL)
		return -EINVAL;

	if (inet_pton(AF_INET, sa, addr) != 1)
		return -EINVAL;

	GET_CB_FIELD(sm, *mask_len, 0, 32, 0);
	*addr = ntohl(*addr);
	return 0;
}

static int
parse_cb_ipv4vlan_rule(char *str, struct rte_acl_rule *v, int has_userdata)
{
	int i, rc;
	char *s, *sp, *in[CB_FLD_NUM];
	static const char *dlm = " \t\n";
	int dim = has_userdata ? CB_FLD_NUM : CB_FLD_USERDATA;
	s = str;

	for (i = 0; i != dim; i++, s = NULL) {
		in[i] = strtok_r(s, dlm, &sp);
		if (in[i] == NULL)
			return -EINVAL;
	}

	rc = parse_ipv4_net(in[CB_FLD_SRC_ADDR],
			&v->field[SRC_FIELD_IPV4].value.u32,
			&v->field[SRC_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		acl_log("failed to read source address/mask: %s\n",
			in[CB_FLD_SRC_ADDR]);
		return rc;
	}

	rc = parse_ipv4_net(in[CB_FLD_DST_ADDR],
			&v->field[DST_FIELD_IPV4].value.u32,
			&v->field[DST_FIELD_IPV4].mask_range.u32);
	if (rc != 0) {
		acl_log("failed to read destination address/mask: %s\n",
			in[CB_FLD_DST_ADDR]);
		return rc;
	}

	GET_CB_FIELD(in[CB_FLD_SRC_PORT_LOW],
		v->field[SRCP_FIELD_IPV4].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_SRC_PORT_HIGH],
		v->field[SRCP_FIELD_IPV4].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_SRC_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0) {
		return -EINVAL;
	}

	GET_CB_FIELD(in[CB_FLD_DST_PORT_LOW],
		v->field[DSTP_FIELD_IPV4].value.u16,
		0, UINT16_MAX, 0);
	GET_CB_FIELD(in[CB_FLD_DST_PORT_HIGH],
		v->field[DSTP_FIELD_IPV4].mask_range.u16,
		0, UINT16_MAX, 0);

	if (strncmp(in[CB_FLD_DST_PORT_DLM], cb_port_delim,
			sizeof(cb_port_delim)) != 0) {
		return -EINVAL;
	}

	if (v->field[SRCP_FIELD_IPV4].mask_range.u16
			< v->field[SRCP_FIELD_IPV4].value.u16
			|| v->field[DSTP_FIELD_IPV4].mask_range.u16
			< v->field[DSTP_FIELD_IPV4].value.u16) {
		return -EINVAL;
	}

	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV4].value.u8,
		0, UINT8_MAX, '/');
	GET_CB_FIELD(in[CB_FLD_PROTO], v->field[PROTO_FIELD_IPV4].mask_range.u8,
		0, UINT8_MAX, 0);

	if (has_userdata)
		GET_CB_FIELD(in[CB_FLD_USERDATA], v->data.userdata, 0,
			UINT32_MAX, 0);

	return 0;
}

static int
acl_add_rules(const char *rule_path,
		struct rte_acl_rule **proute_base,
		unsigned int *proute_num,
		struct rte_acl_rule **pacl_base,
		unsigned int *pacl_num, uint32_t rule_size,
		int (*parser)(char *, struct rte_acl_rule*, int))
{
	uint8_t *acl_rules, *route_rules;
	struct rte_acl_rule *next;
	unsigned int acl_num = 0, route_num = 0, total_num = 0;
	unsigned int acl_cnt = 0, route_cnt = 0;
	char buff[LINE_MAX];
	FILE *fh = fopen(rule_path, "rb");
	unsigned int i = 0;
	int val;

	if (fh == NULL)
		rte_exit(EXIT_FAILURE, "%s: Open %s failed\n", __func__,
			rule_path);

	while ((fgets(buff, LINE_MAX, fh) != NULL)) {
		if (buff[0] == ROUTE_LEAD_CHAR)
			route_num++;
		else if (buff[0] == ACL_LEAD_CHAR)
			acl_num++;
	}

	if (route_num == 0)
		rte_exit(EXIT_FAILURE, "Not find any route entries in %s!\n",
				rule_path);

	val = fseek(fh, 0, SEEK_SET);
	if (val < 0) {
		rte_exit(EXIT_FAILURE, "%s: File seek operation failed\n",
			__func__);
	}

	acl_rules = calloc(acl_num, rule_size);

	if (acl_rules == NULL)
		rte_exit(EXIT_FAILURE, "%s: failed to malloc memory\n",
			__func__);

	route_rules = calloc(route_num, rule_size);

	if (route_rules == NULL)
		rte_exit(EXIT_FAILURE, "%s: failed to malloc memory\n",
			__func__);

	i = 0;
	while (fgets(buff, LINE_MAX, fh) != NULL) {
		i++;

		if (is_bypass_line(buff))
			continue;

		char s = buff[0];

		/* Route entry */
		if (s == ROUTE_LEAD_CHAR)
			next = (struct rte_acl_rule *)(route_rules +
				route_cnt * rule_size);

		/* ACL entry */
		else if (s == ACL_LEAD_CHAR)
			next = (struct rte_acl_rule *)(acl_rules +
				acl_cnt * rule_size);

		/* Illegal line */
		else
			rte_exit(EXIT_FAILURE,
				"%s Line %u: should start with leading "
				"char %c or %c\n",
				rule_path, i, ROUTE_LEAD_CHAR, ACL_LEAD_CHAR);

		if (parser(buff + 1, next, s == ROUTE_LEAD_CHAR) != 0)
			rte_exit(EXIT_FAILURE,
				"%s Line %u: parse rules error\n",
				rule_path, i);

		if (s == ROUTE_LEAD_CHAR) {
			/* Check the forwarding port number */
			if ((enabled_port_mask & (1 << next->data.userdata)) ==
					0)
				rte_exit(EXIT_FAILURE,
					"%s Line %u: fwd number illegal:%u\n",
					rule_path, i, next->data.userdata);
			next->data.userdata += FWD_PORT_SHIFT;
			route_cnt++;
		} else {
			next->data.userdata = ACL_DENY_SIGNATURE + acl_cnt;
			acl_cnt++;
		}

		next->data.priority = RTE_ACL_MAX_PRIORITY - total_num;
		next->data.category_mask = -1;
		total_num++;
	}

	fclose(fh);

	*pacl_base = (struct rte_acl_rule *)acl_rules;
	*pacl_num = acl_num;
	*proute_base = (struct rte_acl_rule *)route_rules;
	*proute_num = route_cnt;

	return 0;
}

enum rte_acl_classify_alg
parse_acl_alg(const char *alg)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(acl_alg); i++) {
		if (strcmp(alg, acl_alg[i].name) == 0)
			return acl_alg[i].alg;
	}

	return RTE_ACL_CLASSIFY_DEFAULT;
}

int
usage_acl_alg(char *buf, size_t sz)
{
	uint32_t i, n, rc, tn;

	n = 0;
	tn = 0;
	for (i = 0; i < RTE_DIM(acl_alg); i++) {
		rc = snprintf(buf + n, sz - n,
			i == RTE_DIM(acl_alg) - 1 ? "%s" : "%s|",
			acl_alg[i].name);
		tn += rc;
		if (rc < sz - n)
			n += rc;
	}

	return tn;
}

static const char *
str_acl_alg(enum rte_acl_classify_alg alg)
{
	uint32_t i;

	for (i = 0; i != RTE_DIM(acl_alg); i++) {
		if (alg == acl_alg[i].alg)
			return acl_alg[i].name;
	}

	return "default";
}

static void
dump_acl_config(void)
{
	printf("ACL options are:\n");
	printf("rule_ipv4: %s\n", parm_config.rule_ipv4_name);
	printf("rule_ipv6: %s\n", parm_config.rule_ipv6_name);
	printf("alg: %s\n", str_acl_alg(parm_config.alg));
}

static int
check_acl_config(void)
{
	if (parm_config.rule_ipv4_name == NULL) {
		acl_log("ACL IPv4 rule file not specified\n");
		return -1;
	} else if (parm_config.rule_ipv6_name == NULL) {
		acl_log("ACL IPv6 rule file not specified\n");
		return -1;
	}

	return 0;
}

/* Setup ACL context. 8< */
static struct rte_acl_ctx*
app_acl_init(struct rte_acl_rule *route_base,
		struct rte_acl_rule *acl_base, unsigned int route_num,
		unsigned int acl_num, int ipv6, int socketid)
{
	char name[PATH_MAX];
	struct rte_acl_param acl_param;
	struct rte_acl_config acl_build_param;
	struct rte_acl_ctx *context;
	int dim = ipv6 ? RTE_DIM(ipv6_defs) : RTE_DIM(ipv4_defs);

	/* Create ACL contexts */
	snprintf(name, sizeof(name), "%s%d",
			ipv6 ? L3FWD_ACL_IPV6_NAME : L3FWD_ACL_IPV4_NAME,
			socketid);

	acl_param.name = name;
	acl_param.socket_id = socketid;
	acl_param.rule_size = RTE_ACL_RULE_SZ(dim);
	acl_param.max_rule_num = MAX_ACL_RULE_NUM;

	context = rte_acl_create(&acl_param);
	if (context == NULL)
		rte_exit(EXIT_FAILURE, "Failed to create ACL context\n");

	if (parm_config.alg != RTE_ACL_CLASSIFY_DEFAULT &&
			rte_acl_set_ctx_classify(context, parm_config.alg) != 0)
		rte_exit(EXIT_FAILURE,
			"Failed to setup classify method for  ACL context\n");

	if (rte_acl_add_rules(context, route_base, route_num) < 0)
		rte_exit(EXIT_FAILURE, "add rules failed\n");

	if (rte_acl_add_rules(context, acl_base, acl_num) < 0)
		rte_exit(EXIT_FAILURE, "add rules failed\n");

	/* Perform builds */
	memset(&acl_build_param, 0, sizeof(acl_build_param));

	acl_build_param.num_categories = DEFAULT_MAX_CATEGORIES;
	acl_build_param.num_fields = dim;
	memcpy(&acl_build_param.defs, ipv6 ? ipv6_defs : ipv4_defs,
		ipv6 ? sizeof(ipv6_defs) : sizeof(ipv4_defs));

	if (rte_acl_build(context, &acl_build_param) != 0)
		rte_exit(EXIT_FAILURE, "Failed to build ACL trie\n");

	rte_acl_dump(context);

	return context;
}
/* >8 End of ACL context setup. */

void
acl_free_routes(void)
{
	free(route_base_ipv4);
	free(route_base_ipv6);
	route_base_ipv4 = NULL;
	route_base_ipv6 = NULL;
	route_num_ipv4 = 0;
	route_num_ipv6 = 0;
	free(acl_base_ipv4);
	free(acl_base_ipv6);
	acl_base_ipv4 = NULL;
	acl_base_ipv6 = NULL;
	acl_num_ipv4 = 0;
	acl_num_ipv6 = 0;
}

/* Load rules from the input file */
void
read_config_files_acl(void)
{
	/* ipv4 check */
	if (parm_config.rule_ipv4_name != NULL) {
		if (acl_add_rules(parm_config.rule_ipv4_name, &route_base_ipv4,
				&route_num_ipv4, &acl_base_ipv4, &acl_num_ipv4,
				sizeof(struct acl4_rule), &parse_cb_ipv4vlan_rule) < 0) {
			acl_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add IPv4 rules\n");
		}
	} else {
		RTE_LOG(ERR, L3FWD, "IPv4 rule file not specified\n");
		rte_exit(EXIT_FAILURE, "Failed to get valid route options\n");
	}

	/* ipv6 check */
	if (parm_config.rule_ipv6_name != NULL) {
		if (acl_add_rules(parm_config.rule_ipv6_name, &route_base_ipv6,
				&route_num_ipv6,
				&acl_base_ipv6, &acl_num_ipv6,
				sizeof(struct acl6_rule), &parse_cb_ipv6_rule) < 0) {
			acl_free_routes();
			rte_exit(EXIT_FAILURE, "Failed to add IPv6 rules\n");
		}
	} else {
		RTE_LOG(ERR, L3FWD, "IPv6 rule file not specified\n");
		rte_exit(EXIT_FAILURE, "Failed to get valid route options\n");
	}
}

void
print_one_ipv4_rule(struct acl4_rule *rule, int extra)
{
	char abuf[INET6_ADDRSTRLEN];
	uint32_t ipv4_addr;
	ipv4_addr = ntohl(rule->field[SRC_FIELD_IPV4].value.u32);
	printf("%s/%u ", inet_ntop(AF_INET,
			&(ipv4_addr), abuf,
			sizeof(abuf)), rule->field[SRC_FIELD_IPV4].mask_range.u32);
	ipv4_addr = ntohl(rule->field[DST_FIELD_IPV4].value.u32);
	printf("%s/%u ", inet_ntop(AF_INET,
			&(ipv4_addr), abuf,
			sizeof(abuf)), rule->field[DST_FIELD_IPV4].mask_range.u32);
	printf("%hu : %hu %hu : %hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV4].value.u16,
		rule->field[SRCP_FIELD_IPV4].mask_range.u16,
		rule->field[DSTP_FIELD_IPV4].value.u16,
		rule->field[DSTP_FIELD_IPV4].mask_range.u16,
		rule->field[PROTO_FIELD_IPV4].value.u8,
		rule->field[PROTO_FIELD_IPV4].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

void
print_one_ipv6_rule(struct acl6_rule *rule, int extra)
{
	unsigned char a, b, c, d;

	uint32_t_to_char(rule->field[SRC1_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf("%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[SRC2_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[SRC3_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[SRC4_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x/%u ", a, b, c, d,
			rule->field[SRC1_FIELD_IPV6].mask_range.u32
			+ rule->field[SRC2_FIELD_IPV6].mask_range.u32
			+ rule->field[SRC3_FIELD_IPV6].mask_range.u32
			+ rule->field[SRC4_FIELD_IPV6].mask_range.u32);

	uint32_t_to_char(rule->field[DST1_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf("%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[DST2_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[DST3_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x", a, b, c, d);
	uint32_t_to_char(rule->field[DST4_FIELD_IPV6].value.u32,
		&a, &b, &c, &d);
	printf(":%.2x%.2x:%.2x%.2x/%u ", a, b, c, d,
			rule->field[DST1_FIELD_IPV6].mask_range.u32
			+ rule->field[DST2_FIELD_IPV6].mask_range.u32
			+ rule->field[DST3_FIELD_IPV6].mask_range.u32
			+ rule->field[DST4_FIELD_IPV6].mask_range.u32);

	printf("%hu : %hu %hu : %hu 0x%hhx/0x%hhx ",
		rule->field[SRCP_FIELD_IPV6].value.u16,
		rule->field[SRCP_FIELD_IPV6].mask_range.u16,
		rule->field[DSTP_FIELD_IPV6].value.u16,
		rule->field[DSTP_FIELD_IPV6].mask_range.u16,
		rule->field[PROTO_FIELD_IPV6].value.u8,
		rule->field[PROTO_FIELD_IPV6].mask_range.u8);
	if (extra)
		printf("0x%x-0x%x-0x%x ",
			rule->data.category_mask,
			rule->data.priority,
			rule->data.userdata);
}

#ifdef L3FWDACL_DEBUG
static inline void
dump_acl4_rule(struct rte_mbuf *m, uint32_t sig)
{
	char abuf[INET6_ADDRSTRLEN];
	uint32_t offset = sig & ~ACL_DENY_SIGNATURE;
	struct rte_ipv4_hdr *ipv4_hdr =
		rte_pktmbuf_mtod_offset(m, struct rte_ipv4_hdr *,
					sizeof(struct rte_ether_hdr));

	printf("Packet Src:%s ", inet_ntop(AF_INET, ipv4_hdr->src_addr,
		abuf, sizeof(abuf)));
	printf("Dst:%s ", inet_ntop(AF_INET, ipv4_hdr->dst_addr,
		abuf, sizeof(abuf)));

	printf("Src port:%hu,Dst port:%hu ",
			rte_bswap16(*(uint16_t *)(ipv4_hdr + 1)),
			rte_bswap16(*((uint16_t *)(ipv4_hdr + 1) + 1)));
	printf("hit ACL %d - ", offset);

	print_one_ipv4_rule(acl_config.rule_ipv4 + offset, 1);

	printf("\n\n");
}

static inline void
dump_acl6_rule(struct rte_mbuf *m, uint32_t sig)
{
	char abuf[INET6_ADDRSTRLEN];
	uint32_t offset = sig & ~ACL_DENY_SIGNATURE;
	struct rte_ipv6_hdr *ipv6_hdr =
		rte_pktmbuf_mtod_offset(m, struct rte_ipv6_hdr *,
					sizeof(struct rte_ether_hdr));

	printf("Packet Src");
	printf("%s", inet_ntop(AF_INET6, ipv6_hdr->src_addr,
		abuf, sizeof(abuf)));
	printf("\nDst");
	printf("%s", inet_ntop(AF_INET6, ipv6_hdr->dst_addr,
		abuf, sizeof(abuf)));

	printf("\nSrc port:%hu,Dst port:%hu ",
			rte_bswap16(*(uint16_t *)(ipv6_hdr + 1)),
			rte_bswap16(*((uint16_t *)(ipv6_hdr + 1) + 1)));
	printf("hit ACL %d - ", offset);

	print_one_ipv6_rule(acl_config.rule_ipv6 + offset, 1);

	printf("\n\n");
}
#endif /* L3FWDACL_DEBUG */

static inline void
dump_ipv4_rules(struct acl4_rule *rule, int num, int extra)
{
	int i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ipv4_rule(rule, extra);
		printf("\n");
	}
}

static inline void
dump_ipv6_rules(struct acl6_rule *rule, int num, int extra)
{
	int i;

	for (i = 0; i < num; i++, rule++) {
		printf("\t%d:", i + 1);
		print_one_ipv6_rule(rule, extra);
		printf("\n");
	}
}

/* Function to setup acl. */
void
setup_acl(const int socket_id)
{
	if (check_acl_config() != 0)
		rte_exit(EXIT_FAILURE, "Failed to get valid ACL options\n");

	dump_acl_config();

	acl_log("IPv4 Route entries %u:\n", route_num_ipv4);
	dump_ipv4_rules((struct acl4_rule *)route_base_ipv4, route_num_ipv4, 1);

	acl_log("IPv4 ACL entries %u:\n", acl_num_ipv4);
	dump_ipv4_rules((struct acl4_rule *)acl_base_ipv4, acl_num_ipv4, 1);

	acl_log("IPv6 Route entries %u:\n", route_num_ipv6);
	dump_ipv6_rules((struct acl6_rule *)route_base_ipv6, route_num_ipv6, 1);

	acl_log("IPv6 ACL entries %u:\n", acl_num_ipv6);
	dump_ipv6_rules((struct acl6_rule *)acl_base_ipv6, acl_num_ipv6, 1);

	memset(&acl_config, 0, sizeof(acl_config));

	/* Check sockets a context should be created on */
	if (socket_id >= NB_SOCKETS) {
		acl_log("Socket %d is out "
			"of range %d\n",
			socket_id, NB_SOCKETS);
		acl_free_routes();
		return;
	}

	acl_config.acx_ipv4[socket_id] = app_acl_init(route_base_ipv4,
		acl_base_ipv4, route_num_ipv4, acl_num_ipv4,
		0, socket_id);

	acl_config.acx_ipv6[socket_id] = app_acl_init(route_base_ipv6,
		acl_base_ipv6, route_num_ipv6, acl_num_ipv6,
		1, socket_id);

#ifdef L3FWDACL_DEBUG
	acl_config.rule_ipv4 = (struct acl4_rule *)acl_base_ipv4;
	acl_config.rule_ipv6 = (struct acl6_rule *)acl_base_ipv6;
#endif

}

/* main processing loop */
int
acl_main_loop(__rte_unused void *dummy)
{
	struct rte_mbuf *pkts_burst[MAX_PKT_BURST];
	unsigned int lcore_id;
	uint64_t prev_tsc, diff_tsc, cur_tsc;
	int i, nb_rx;
	uint16_t portid;
	uint8_t queueid;
	struct lcore_conf *qconf;
	int socketid;
	const uint64_t drain_tsc = (rte_get_tsc_hz() + US_PER_S - 1)
			/ US_PER_S * BURST_TX_DRAIN_US;

	prev_tsc = 0;
	lcore_id = rte_lcore_id();
	qconf = &lcore_conf[lcore_id];
	socketid = rte_lcore_to_socket_id(lcore_id);

	if (qconf->n_rx_queue == 0) {
		RTE_LOG(INFO, L3FWD, "lcore %u has nothing to do\n", lcore_id);
		return 0;
	}

	RTE_LOG(INFO, L3FWD, "entering main loop on lcore %u\n", lcore_id);

	for (i = 0; i < qconf->n_rx_queue; i++) {

		portid = qconf->rx_queue_list[i].port_id;
		queueid = qconf->rx_queue_list[i].queue_id;
		RTE_LOG(INFO, L3FWD,
			" -- lcoreid=%u portid=%u rxqueueid=%hhu\n",
			lcore_id, portid, queueid);
	}

	while (!force_quit) {

		cur_tsc = rte_rdtsc();

		/*
		 * TX burst queue drain
		 */
		diff_tsc = cur_tsc - prev_tsc;
		if (unlikely(diff_tsc > drain_tsc)) {

			for (i = 0; i < qconf->n_tx_port; ++i) {
				portid = qconf->tx_port_id[i];
				if (qconf->tx_mbufs[portid].len == 0)
					continue;
				send_burst(qconf,
					qconf->tx_mbufs[portid].len,
					portid);
				qconf->tx_mbufs[portid].len = 0;
			}

			prev_tsc = cur_tsc;
		}

		/*
		 * Read packet from RX queues
		 */
		for (i = 0; i < qconf->n_rx_queue; ++i) {

			portid = qconf->rx_queue_list[i].port_id;
			queueid = qconf->rx_queue_list[i].queue_id;
			nb_rx = rte_eth_rx_burst(portid, queueid,
				pkts_burst, MAX_PKT_BURST);

			if (nb_rx > 0) {
				struct acl_search_t acl_search;

				l3fwd_acl_prepare_acl_parameter(pkts_burst, &acl_search,
					nb_rx);

				if (acl_search.num_ipv4) {
					rte_acl_classify(
						acl_config.acx_ipv4[socketid],
						acl_search.data_ipv4,
						acl_search.res_ipv4,
						acl_search.num_ipv4,
						DEFAULT_MAX_CATEGORIES);

					l3fwd_acl_send_packets(
						qconf,
						pkts_burst,
						acl_search.res_ipv4,
						nb_rx);
				}

				if (acl_search.num_ipv6) {
					rte_acl_classify(
						acl_config.acx_ipv6[socketid],
						acl_search.data_ipv6,
						acl_search.res_ipv6,
						acl_search.num_ipv6,
						DEFAULT_MAX_CATEGORIES);

					l3fwd_acl_send_packets(
						qconf,
						pkts_burst,
						acl_search.res_ipv6,
						nb_rx);
				}
			}
		}
	}
	return 0;
}

/* Not used by L3fwd ACL. */
void *
acl_get_ipv4_l3fwd_lookup_struct(__rte_unused const int socketid)
{
	return NULL;
}

void *
acl_get_ipv6_l3fwd_lookup_struct(__rte_unused const int socketid)
{
	return NULL;
}
