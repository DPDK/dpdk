/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Marvell International Ltd.
 * Copyright(c) 2017 Semihalf.
 * All rights reserved.
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_cfgfile.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>

#include "mrvl_qos.h"

/* Parsing tokens. Defined conveniently, so that any correction is easy. */
#define MRVL_TOK_DEFAULT "default"
#define MRVL_TOK_DEFAULT_TC "default_tc"
#define MRVL_TOK_DSCP "dscp"
#define MRVL_TOK_MAPPING_PRIORITY "mapping_priority"
#define MRVL_TOK_IP "ip"
#define MRVL_TOK_IP_VLAN "ip/vlan"
#define MRVL_TOK_PCP "pcp"
#define MRVL_TOK_PORT "port"
#define MRVL_TOK_RXQ "rxq"
#define MRVL_TOK_TC "tc"
#define MRVL_TOK_TXQ "txq"
#define MRVL_TOK_VLAN "vlan"
#define MRVL_TOK_VLAN_IP "vlan/ip"

/* egress specific configuration tokens */
#define MRVL_TOK_BURST_SIZE "burst_size"
#define MRVL_TOK_RATE_LIMIT "rate_limit"
#define MRVL_TOK_RATE_LIMIT_ENABLE "rate_limit_enable"
#define MRVL_TOK_SCHED_MODE "sched_mode"
#define MRVL_TOK_SCHED_MODE_SP "sp"
#define MRVL_TOK_SCHED_MODE_WRR "wrr"
#define MRVL_TOK_WRR_WEIGHT "wrr_weight"

/* policer specific configuration tokens */
#define MRVL_TOK_PLCR "policer"
#define MRVL_TOK_PLCR_DEFAULT "default_policer"
#define MRVL_TOK_PLCR_UNIT "token_unit"
#define MRVL_TOK_PLCR_UNIT_BYTES "bytes"
#define MRVL_TOK_PLCR_UNIT_PACKETS "packets"
#define MRVL_TOK_PLCR_COLOR "color_mode"
#define MRVL_TOK_PLCR_COLOR_BLIND "blind"
#define MRVL_TOK_PLCR_COLOR_AWARE "aware"
#define MRVL_TOK_PLCR_CIR "cir"
#define MRVL_TOK_PLCR_CBS "cbs"
#define MRVL_TOK_PLCR_EBS "ebs"
#define MRVL_TOK_PLCR_DEFAULT_COLOR "default_color"
#define MRVL_TOK_PLCR_DEFAULT_COLOR_GREEN "green"
#define MRVL_TOK_PLCR_DEFAULT_COLOR_YELLOW "yellow"
#define MRVL_TOK_PLCR_DEFAULT_COLOR_RED "red"

/** Number of tokens in range a-b = 2. */
#define MAX_RNG_TOKENS 2

/** Maximum possible value of PCP. */
#define MAX_PCP 7

/** Maximum possible value of DSCP. */
#define MAX_DSCP 63

/** Global QoS configuration. */
struct mrvl_qos_cfg *mrvl_qos_cfg;

/**
 * Convert string to uint32_t with extra checks for result correctness.
 *
 * @param string String to convert.
 * @param val Conversion result.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
get_val_securely(const char *string, uint32_t *val)
{
	char *endptr;
	size_t len = strlen(string);

	if (len == 0)
		return -1;

	errno = 0;
	*val = strtoul(string, &endptr, 0);
	if (errno != 0 || RTE_PTR_DIFF(endptr, string) != len)
		return -2;

	return 0;
}

/**
 * Read out-queue configuration from file.
 *
 * @param file Path to the configuration file.
 * @param port Port number.
 * @param outq Out queue number.
 * @param cfg Pointer to the Marvell QoS configuration structure.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
get_outq_cfg(struct rte_cfgfile *file, int port, int outq,
		struct mrvl_qos_cfg *cfg)
{
	char sec_name[32];
	const char *entry;
	uint32_t val;

	snprintf(sec_name, sizeof(sec_name), "%s %d %s %d",
		MRVL_TOK_PORT, port, MRVL_TOK_TXQ, outq);

	/* Skip non-existing */
	if (rte_cfgfile_num_sections(file, sec_name, strlen(sec_name)) <= 0)
		return 0;

	/* Read scheduling mode */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_SCHED_MODE);
	if (entry) {
		if (!strncmp(entry, MRVL_TOK_SCHED_MODE_SP,
					strlen(MRVL_TOK_SCHED_MODE_SP))) {
			cfg->port[port].outq[outq].sched_mode =
				PP2_PPIO_SCHED_M_SP;
		} else if (!strncmp(entry, MRVL_TOK_SCHED_MODE_WRR,
					strlen(MRVL_TOK_SCHED_MODE_WRR))) {
			cfg->port[port].outq[outq].sched_mode =
				PP2_PPIO_SCHED_M_WRR;
		} else {
			MRVL_LOG(ERR, "Unknown token: %s", entry);
			return -1;
		}
	}

	/* Read wrr weight */
	if (cfg->port[port].outq[outq].sched_mode == PP2_PPIO_SCHED_M_WRR) {
		entry = rte_cfgfile_get_entry(file, sec_name,
				MRVL_TOK_WRR_WEIGHT);
		if (entry) {
			if (get_val_securely(entry, &val) < 0)
				return -1;
			cfg->port[port].outq[outq].weight = val;
		}
	}

	/*
	 * There's no point in setting rate limiting for specific outq as
	 * global port rate limiting has priority.
	 */
	if (cfg->port[port].rate_limit_enable) {
		MRVL_LOG(WARNING, "Port %d rate limiting already enabled",
			port);
		return 0;
	}

	entry = rte_cfgfile_get_entry(file, sec_name,
			MRVL_TOK_RATE_LIMIT_ENABLE);
	if (entry) {
		if (get_val_securely(entry, &val) < 0)
			return -1;
		cfg->port[port].outq[outq].rate_limit_enable = val;
	}

	if (!cfg->port[port].outq[outq].rate_limit_enable)
		return 0;

	/* Read CBS (in kB) */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_BURST_SIZE);
	if (entry) {
		if (get_val_securely(entry, &val) < 0)
			return -1;
		cfg->port[port].outq[outq].rate_limit_params.cbs = val;
	}

	/* Read CIR (in kbps) */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_RATE_LIMIT);
	if (entry) {
		if (get_val_securely(entry, &val) < 0)
			return -1;
		cfg->port[port].outq[outq].rate_limit_params.cir = val;
	}

	return 0;
}

/**
 * Gets multiple-entry values and places them in table.
 *
 * Entry can be anything, e.g. "1 2-3 5 6 7-9". This needs to be converted to
 * table entries, respectively: {1, 2, 3, 5, 6, 7, 8, 9}.
 * As all result table's elements are always 1-byte long, we
 * won't overcomplicate the function, but we'll keep API generic,
 * check if someone hasn't changed element size and make it simple
 * to extend to other sizes.
 *
 * This function is purely utilitary, it does not print any error, only returns
 * different error numbers.
 *
 * @param entry[in] Values string to parse.
 * @param tab[out] Results table.
 * @param elem_sz[in] Element size (in bytes).
 * @param max_elems[in] Number of results table elements available.
 * @param max val[in] Maximum value allowed.
 * @returns Number of correctly parsed elements in case of success.
 * @retval -1 Wrong element size.
 * @retval -2 More tokens than result table allows.
 * @retval -3 Wrong range syntax.
 * @retval -4 Wrong range values.
 * @retval -5 Maximum value exceeded.
 */
static int
get_entry_values(const char *entry, uint8_t *tab,
	size_t elem_sz, uint8_t max_elems, uint8_t max_val)
{
	/* There should not be more tokens than max elements.
	 * Add 1 for error trap.
	 */
	char *tokens[max_elems + 1];

	/* Begin, End + error trap = 3. */
	char *rng_tokens[MAX_RNG_TOKENS + 1];
	long beg, end;
	uint32_t token_val;
	int nb_tokens, nb_rng_tokens;
	int i;
	int values = 0;
	char val;
	char entry_cpy[CFG_VALUE_LEN];

	if (elem_sz != 1)
		return -1;

	/* Copy the entry to safely use rte_strsplit(). */
	strlcpy(entry_cpy, entry, RTE_DIM(entry_cpy));

	/*
	 * If there are more tokens than array size, rte_strsplit will
	 * not return error, just array size.
	 */
	nb_tokens = rte_strsplit(entry_cpy, strlen(entry_cpy),
		tokens, max_elems + 1, ' ');

	/* Quick check, will be refined later. */
	if (nb_tokens > max_elems)
		return -2;

	for (i = 0; i < nb_tokens; ++i) {
		if (strchr(tokens[i], '-') != NULL) {
			/*
			 * Split to begin and end tokens.
			 * We want to catch error cases too, thus we leave
			 * option for number of tokens to be more than 2.
			 */
			nb_rng_tokens = rte_strsplit(tokens[i],
					strlen(tokens[i]), rng_tokens,
					RTE_DIM(rng_tokens), '-');
			if (nb_rng_tokens != 2)
				return -3;

			/* Range and sanity checks. */
			if (get_val_securely(rng_tokens[0], &token_val) < 0)
				return -4;
			beg = (char)token_val;
			if (get_val_securely(rng_tokens[1], &token_val) < 0)
				return -4;
			end = (char)token_val;
			if (beg < 0 || beg > UCHAR_MAX ||
				end < 0 || end > UCHAR_MAX || end < beg)
				return -4;

			for (val = beg; val <= end; ++val) {
				if (val > max_val)
					return -5;

				*tab = val;
				tab = RTE_PTR_ADD(tab, elem_sz);
				++values;
				if (values >= max_elems)
					return -2;
			}
		} else {
			/* Single values. */
			if (get_val_securely(tokens[i], &token_val) < 0)
				return -5;
			val = (char)token_val;
			if (val > max_val)
				return -5;

			*tab = val;
			tab = RTE_PTR_ADD(tab, elem_sz);
			++values;
			if (values >= max_elems)
				return -2;
		}
	}

	return values;
}

/**
 * Parse Traffic Class'es mapping configuration.
 *
 * @param file Config file handle.
 * @param port Which port to look for.
 * @param tc Which Traffic Class to look for.
 * @param cfg[out] Parsing results.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
parse_tc_cfg(struct rte_cfgfile *file, int port, int tc,
		struct mrvl_qos_cfg *cfg)
{
	char sec_name[32];
	const char *entry;
	int n;

	snprintf(sec_name, sizeof(sec_name), "%s %d %s %d",
		MRVL_TOK_PORT, port, MRVL_TOK_TC, tc);

	/* Skip non-existing */
	if (rte_cfgfile_num_sections(file, sec_name, strlen(sec_name)) <= 0)
		return 0;

	cfg->port[port].use_global_defaults = 0;
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_RXQ);
	if (entry) {
		n = get_entry_values(entry,
			cfg->port[port].tc[tc].inq,
			sizeof(cfg->port[port].tc[tc].inq[0]),
			RTE_DIM(cfg->port[port].tc[tc].inq),
			MRVL_PP2_RXQ_MAX);
		if (n < 0) {
			MRVL_LOG(ERR, "Error %d while parsing: %s",
				n, entry);
			return n;
		}
		cfg->port[port].tc[tc].inqs = n;
	}

	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_PCP);
	if (entry) {
		n = get_entry_values(entry,
			cfg->port[port].tc[tc].pcp,
			sizeof(cfg->port[port].tc[tc].pcp[0]),
			RTE_DIM(cfg->port[port].tc[tc].pcp),
			MAX_PCP);
		if (n < 0) {
			MRVL_LOG(ERR, "Error %d while parsing: %s",
				n, entry);
			return n;
		}
		cfg->port[port].tc[tc].pcps = n;
	}

	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_DSCP);
	if (entry) {
		n = get_entry_values(entry,
			cfg->port[port].tc[tc].dscp,
			sizeof(cfg->port[port].tc[tc].dscp[0]),
			RTE_DIM(cfg->port[port].tc[tc].dscp),
			MAX_DSCP);
		if (n < 0) {
			MRVL_LOG(ERR, "Error %d while parsing: %s",
				n, entry);
			return n;
		}
		cfg->port[port].tc[tc].dscps = n;
	}

	if (!cfg->port[port].setup_policer)
		return 0;

	entry = rte_cfgfile_get_entry(file, sec_name,
			MRVL_TOK_PLCR_DEFAULT_COLOR);
	if (entry) {
		if (!strncmp(entry, MRVL_TOK_PLCR_DEFAULT_COLOR_GREEN,
				sizeof(MRVL_TOK_PLCR_DEFAULT_COLOR_GREEN))) {
			cfg->port[port].tc[tc].color = PP2_PPIO_COLOR_GREEN;
		} else if (!strncmp(entry, MRVL_TOK_PLCR_DEFAULT_COLOR_YELLOW,
				sizeof(MRVL_TOK_PLCR_DEFAULT_COLOR_YELLOW))) {
			cfg->port[port].tc[tc].color = PP2_PPIO_COLOR_YELLOW;
		} else if (!strncmp(entry, MRVL_TOK_PLCR_DEFAULT_COLOR_RED,
				sizeof(MRVL_TOK_PLCR_DEFAULT_COLOR_RED))) {
			cfg->port[port].tc[tc].color = PP2_PPIO_COLOR_RED;
		} else {
			MRVL_LOG(ERR, "Error while parsing: %s", entry);
			return -1;
		}
	}

	return 0;
}

/**
 * Parse default port policer.
 *
 * @param file Config file handle.
 * @param sec_name Section name with policer configuration
 * @param port Port number.
 * @param cfg[out] Parsing results.
 * @returns 0 in case of success, negative value otherwise.
 */
static int
parse_policer(struct rte_cfgfile *file, int port, const char *sec_name,
		struct mrvl_qos_cfg *cfg)
{
	const char *entry;
	uint32_t val;

	/* Read policer token unit */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_PLCR_UNIT);
	if (entry) {
		if (!strncmp(entry, MRVL_TOK_PLCR_UNIT_BYTES,
					sizeof(MRVL_TOK_PLCR_UNIT_BYTES))) {
			cfg->port[port].policer_params.token_unit =
				PP2_CLS_PLCR_BYTES_TOKEN_UNIT;
		} else if (!strncmp(entry, MRVL_TOK_PLCR_UNIT_PACKETS,
					sizeof(MRVL_TOK_PLCR_UNIT_PACKETS))) {
			cfg->port[port].policer_params.token_unit =
				PP2_CLS_PLCR_PACKETS_TOKEN_UNIT;
		} else {
			MRVL_LOG(ERR, "Unknown token: %s", entry);
			return -1;
		}
	}

	/* Read policer color mode */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_PLCR_COLOR);
	if (entry) {
		if (!strncmp(entry, MRVL_TOK_PLCR_COLOR_BLIND,
					sizeof(MRVL_TOK_PLCR_COLOR_BLIND))) {
			cfg->port[port].policer_params.color_mode =
				PP2_CLS_PLCR_COLOR_BLIND_MODE;
		} else if (!strncmp(entry, MRVL_TOK_PLCR_COLOR_AWARE,
					sizeof(MRVL_TOK_PLCR_COLOR_AWARE))) {
			cfg->port[port].policer_params.color_mode =
				PP2_CLS_PLCR_COLOR_AWARE_MODE;
		} else {
			MRVL_LOG(ERR, "Error in parsing: %s", entry);
			return -1;
		}
	}

	/* Read policer cir */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_PLCR_CIR);
	if (entry) {
		if (get_val_securely(entry, &val) < 0)
			return -1;
		cfg->port[port].policer_params.cir = val;
	}

	/* Read policer cbs */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_PLCR_CBS);
	if (entry) {
		if (get_val_securely(entry, &val) < 0)
			return -1;
		cfg->port[port].policer_params.cbs = val;
	}

	/* Read policer ebs */
	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_PLCR_EBS);
	if (entry) {
		if (get_val_securely(entry, &val) < 0)
			return -1;
		cfg->port[port].policer_params.ebs = val;
	}

	cfg->port[port].setup_policer = 1;

	return 0;
}

/**
 * Parse QoS configuration - rte_kvargs_process handler.
 *
 * Opens configuration file and parses its content.
 *
 * @param key Unused.
 * @param path Path to config file.
 * @param extra_args Pointer to configuration structure.
 * @returns 0 in case of success, exits otherwise.
 */
int
mrvl_get_qoscfg(const char *key __rte_unused, const char *path,
		void *extra_args)
{
	struct mrvl_qos_cfg **cfg = extra_args;
	struct rte_cfgfile *file = rte_cfgfile_load(path, 0);
	uint32_t val;
	int n, i, ret;
	const char *entry;
	char sec_name[32];

	if (file == NULL)
		rte_exit(EXIT_FAILURE, "Cannot load configuration %s\n", path);

	/* Create configuration. This is never accessed on the fast path,
	 * so we can ignore socket.
	 */
	*cfg = rte_zmalloc("mrvl_qos_cfg", sizeof(struct mrvl_qos_cfg), 0);
	if (*cfg == NULL)
		rte_exit(EXIT_FAILURE, "Cannot allocate configuration %s\n",
			path);

	n = rte_cfgfile_num_sections(file, MRVL_TOK_PORT,
		sizeof(MRVL_TOK_PORT) - 1);

	if (n == 0) {
		/* This is weird, but not bad. */
		MRVL_LOG(WARNING, "Empty configuration file?");
		return 0;
	}

	/* Use the number of ports given as vdev parameters. */
	for (n = 0; n < (PP2_NUM_ETH_PPIO * PP2_NUM_PKT_PROC); ++n) {
		snprintf(sec_name, sizeof(sec_name), "%s %d %s",
			MRVL_TOK_PORT, n, MRVL_TOK_DEFAULT);

		/* Use global defaults, unless an override occurs */
		(*cfg)->port[n].use_global_defaults = 1;

		/* Skip ports non-existing in configuration. */
		if (rte_cfgfile_num_sections(file, sec_name,
				strlen(sec_name)) <= 0) {
			continue;
		}

		/*
		 * Read per-port rate limiting. Setting that will
		 * disable per-queue rate limiting.
		 */
		entry = rte_cfgfile_get_entry(file, sec_name,
				MRVL_TOK_RATE_LIMIT_ENABLE);
		if (entry) {
			if (get_val_securely(entry, &val) < 0)
				return -1;
			(*cfg)->port[n].rate_limit_enable = val;
		}

		if ((*cfg)->port[n].rate_limit_enable) {
			entry = rte_cfgfile_get_entry(file, sec_name,
					MRVL_TOK_BURST_SIZE);
			if (entry) {
				if (get_val_securely(entry, &val) < 0)
					return -1;
				(*cfg)->port[n].rate_limit_params.cbs = val;
			}

			entry = rte_cfgfile_get_entry(file, sec_name,
					MRVL_TOK_RATE_LIMIT);
			if (entry) {
				if (get_val_securely(entry, &val) < 0)
					return -1;
				(*cfg)->port[n].rate_limit_params.cir = val;
			}
		}

		entry = rte_cfgfile_get_entry(file, sec_name,
				MRVL_TOK_MAPPING_PRIORITY);
		if (entry) {
			(*cfg)->port[n].use_global_defaults = 0;
			if (!strncmp(entry, MRVL_TOK_VLAN_IP,
				sizeof(MRVL_TOK_VLAN_IP)))
				(*cfg)->port[n].mapping_priority =
					PP2_CLS_QOS_TBL_VLAN_IP_PRI;
			else if (!strncmp(entry, MRVL_TOK_IP_VLAN,
				sizeof(MRVL_TOK_IP_VLAN)))
				(*cfg)->port[n].mapping_priority =
					PP2_CLS_QOS_TBL_IP_VLAN_PRI;
			else if (!strncmp(entry, MRVL_TOK_IP,
				sizeof(MRVL_TOK_IP)))
				(*cfg)->port[n].mapping_priority =
					PP2_CLS_QOS_TBL_IP_PRI;
			else if (!strncmp(entry, MRVL_TOK_VLAN,
				sizeof(MRVL_TOK_VLAN)))
				(*cfg)->port[n].mapping_priority =
					PP2_CLS_QOS_TBL_VLAN_PRI;
			else
				rte_exit(EXIT_FAILURE,
					"Error in parsing %s value (%s)!\n",
					MRVL_TOK_MAPPING_PRIORITY, entry);
		} else {
			(*cfg)->port[n].mapping_priority =
				PP2_CLS_QOS_TBL_VLAN_IP_PRI;
		}

		/* Parse policer configuration (if any) */
		entry = rte_cfgfile_get_entry(file, sec_name,
				MRVL_TOK_PLCR_DEFAULT);
		if (entry) {
			(*cfg)->port[n].use_global_defaults = 0;
			if (get_val_securely(entry, &val) < 0)
				return -1;

			snprintf(sec_name, sizeof(sec_name), "%s %d",
					MRVL_TOK_PLCR, val);
			ret = parse_policer(file, n, sec_name, *cfg);
			if (ret)
				return -1;
		}

		for (i = 0; i < MRVL_PP2_RXQ_MAX; ++i) {
			ret = get_outq_cfg(file, n, i, *cfg);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"Error %d parsing port %d outq %d!\n",
					ret, n, i);
		}

		for (i = 0; i < MRVL_PP2_TC_MAX; ++i) {
			ret = parse_tc_cfg(file, n, i, *cfg);
			if (ret < 0)
				rte_exit(EXIT_FAILURE,
					"Error %d parsing port %d tc %d!\n",
					ret, n, i);
		}

		entry = rte_cfgfile_get_entry(file, sec_name,
					      MRVL_TOK_DEFAULT_TC);
		if (entry) {
			if (get_val_securely(entry, &val) < 0 ||
			    val > USHRT_MAX)
				return -1;
			(*cfg)->port[n].default_tc = (uint8_t)val;
		} else {
			if ((*cfg)->port[n].use_global_defaults == 0) {
				MRVL_LOG(ERR,
					 "Default Traffic Class required in custom configuration!");
				return -1;
			}
		}
	}

	return 0;
}

/**
 * Setup Traffic Class.
 *
 * Fill in TC parameters in single MUSDK TC config entry.
 * @param param TC parameters entry.
 * @param inqs Number of MUSDK in-queues in this TC.
 * @param bpool Bpool for this TC.
 * @param color Default color for this TC.
 * @returns 0 in case of success, exits otherwise.
 */
static int
setup_tc(struct pp2_ppio_tc_params *param, uint8_t inqs,
	struct pp2_bpool *bpool, enum pp2_ppio_color color)
{
	struct pp2_ppio_inq_params *inq_params;

	param->pkt_offset = MRVL_PKT_OFFS;
	param->pools[0][0] = bpool;
	param->default_color = color;

	inq_params = rte_zmalloc_socket("inq_params",
		inqs * sizeof(*inq_params),
		0, rte_socket_id());
	if (!inq_params)
		return -ENOMEM;

	param->num_in_qs = inqs;

	/* Release old config if necessary. */
	if (param->inqs_params)
		rte_free(param->inqs_params);

	param->inqs_params = inq_params;

	return 0;
}

/**
 * Setup ingress policer.
 *
 * @param priv Port's private data.
 * @param params Pointer to the policer's configuration.
 * @param plcr_id Policer id.
 * @returns 0 in case of success, negative values otherwise.
 */
static int
setup_policer(struct mrvl_priv *priv, struct pp2_cls_plcr_params *params)
{
	char match[16];
	int ret;

	/*
	 * At this point no other policers are used which means
	 * any policer can be picked up and used as a default one.
	 *
	 * Lets use 0th then.
	 */
	sprintf(match, "policer-%d:%d\n", priv->pp_id, 0);
	params->match = match;

	ret = pp2_cls_plcr_init(params, &priv->default_policer);
	if (ret) {
		MRVL_LOG(ERR, "Failed to setup %s", match);
		return -1;
	}

	priv->ppio_params.inqs_params.plcr = priv->default_policer;
	priv->used_plcrs = BIT(0);

	return 0;
}

/**
 * Configure RX Queues in a given port.
 *
 * Sets up RX queues, their Traffic Classes and DPDK rxq->(TC,inq) mapping.
 *
 * @param priv Port's private data
 * @param portid DPDK port ID
 * @param max_queues Maximum number of queues to configure.
 * @returns 0 in case of success, negative value otherwise.
 */
int
mrvl_configure_rxqs(struct mrvl_priv *priv, uint16_t portid,
	uint16_t max_queues)
{
	size_t i, tc;

	if (mrvl_qos_cfg == NULL ||
		mrvl_qos_cfg->port[portid].use_global_defaults) {
		/*
		 * No port configuration, use default: 1 TC, no QoS,
		 * TC color set to green.
		 */
		priv->ppio_params.inqs_params.num_tcs = 1;
		setup_tc(&priv->ppio_params.inqs_params.tcs_params[0],
			max_queues, priv->bpool, PP2_PPIO_COLOR_GREEN);

		/* Direct mapping of queues i.e. 0->0, 1->1 etc. */
		for (i = 0; i < max_queues; ++i) {
			priv->rxq_map[i].tc = 0;
			priv->rxq_map[i].inq = i;
		}
		return 0;
	}

	/* We need only a subset of configuration. */
	struct port_cfg *port_cfg = &mrvl_qos_cfg->port[portid];

	priv->qos_tbl_params.type = port_cfg->mapping_priority;

	/*
	 * We need to reverse mapping, from tc->pcp (better from usability
	 * point of view) to pcp->tc (configurable in MUSDK).
	 * First, set all map elements to "default".
	 */
	for (i = 0; i < RTE_DIM(priv->qos_tbl_params.pcp_cos_map); ++i)
		priv->qos_tbl_params.pcp_cos_map[i].tc = port_cfg->default_tc;

	/* Then, fill in all known values. */
	for (tc = 0; tc < RTE_DIM(port_cfg->tc); ++tc) {
		if (port_cfg->tc[tc].pcps > RTE_DIM(port_cfg->tc[0].pcp)) {
			/* Better safe than sorry. */
			MRVL_LOG(ERR,
				"Too many PCPs configured in TC %zu!", tc);
			return -1;
		}
		for (i = 0; i < port_cfg->tc[tc].pcps; ++i) {
			priv->qos_tbl_params.pcp_cos_map[
			  port_cfg->tc[tc].pcp[i]].tc = tc;
		}
	}

	/*
	 * The same logic goes with DSCP.
	 * First, set all map elements to "default".
	 */
	for (i = 0; i < RTE_DIM(priv->qos_tbl_params.dscp_cos_map); ++i)
		priv->qos_tbl_params.dscp_cos_map[i].tc =
			port_cfg->default_tc;

	/* Fill in all known values. */
	for (tc = 0; tc < RTE_DIM(port_cfg->tc); ++tc) {
		if (port_cfg->tc[tc].dscps > RTE_DIM(port_cfg->tc[0].dscp)) {
			/* Better safe than sorry. */
			MRVL_LOG(ERR,
				"Too many DSCPs configured in TC %zu!", tc);
			return -1;
		}
		for (i = 0; i < port_cfg->tc[tc].dscps; ++i) {
			priv->qos_tbl_params.dscp_cos_map[
			  port_cfg->tc[tc].dscp[i]].tc = tc;
		}
	}

	/*
	 * Surprisingly, similar logic goes with queue mapping.
	 * We need only to store qid->tc mapping,
	 * to know TC when queue is read.
	 */
	for (i = 0; i < RTE_DIM(priv->rxq_map); ++i)
		priv->rxq_map[i].tc = MRVL_UNKNOWN_TC;

	/* Set up DPDKq->(TC,inq) mapping. */
	for (tc = 0; tc < RTE_DIM(port_cfg->tc); ++tc) {
		if (port_cfg->tc[tc].inqs > RTE_DIM(port_cfg->tc[0].inq)) {
			/* Overflow. */
			MRVL_LOG(ERR,
				"Too many RX queues configured per TC %zu!",
				tc);
			return -1;
		}
		for (i = 0; i < port_cfg->tc[tc].inqs; ++i) {
			uint8_t idx = port_cfg->tc[tc].inq[i];

			if (idx > RTE_DIM(priv->rxq_map)) {
				MRVL_LOG(ERR, "Bad queue index %d!", idx);
				return -1;
			}

			priv->rxq_map[idx].tc = tc;
			priv->rxq_map[idx].inq = i;
		}
	}

	/*
	 * Set up TC configuration. TCs need to be sequenced: 0, 1, 2
	 * with no gaps. Empty TC means end of processing.
	 */
	for (i = 0; i < MRVL_PP2_TC_MAX; ++i) {
		if (port_cfg->tc[i].inqs == 0)
			break;
		setup_tc(&priv->ppio_params.inqs_params.tcs_params[i],
				port_cfg->tc[i].inqs,
				priv->bpool, port_cfg->tc[i].color);
	}

	priv->ppio_params.inqs_params.num_tcs = i;

	if (port_cfg->setup_policer)
		return setup_policer(priv, &port_cfg->policer_params);

	return 0;
}

/**
 * Configure TX Queues in a given port.
 *
 * Sets up TX queues egress scheduler and limiter.
 *
 * @param priv Port's private data
 * @param portid DPDK port ID
 * @param max_queues Maximum number of queues to configure.
 * @returns 0 in case of success, negative value otherwise.
 */
int
mrvl_configure_txqs(struct mrvl_priv *priv, uint16_t portid,
		uint16_t max_queues)
{
	/* We need only a subset of configuration. */
	struct port_cfg *port_cfg = &mrvl_qos_cfg->port[portid];
	int i;

	if (mrvl_qos_cfg == NULL)
		return 0;

	priv->ppio_params.rate_limit_enable = port_cfg->rate_limit_enable;
	if (port_cfg->rate_limit_enable)
		priv->ppio_params.rate_limit_params =
			port_cfg->rate_limit_params;

	for (i = 0; i < max_queues; i++) {
		struct pp2_ppio_outq_params *params =
			&priv->ppio_params.outqs_params.outqs_params[i];

		params->sched_mode = port_cfg->outq[i].sched_mode;
		params->weight = port_cfg->outq[i].weight;
		params->rate_limit_enable = port_cfg->outq[i].rate_limit_enable;
		params->rate_limit_params = port_cfg->outq[i].rate_limit_params;
	}

	return 0;
}

/**
 * Start QoS mapping.
 *
 * Finalize QoS table configuration and initialize it in SDK. It can be done
 * only after port is started, so we have a valid ppio reference.
 *
 * @param priv Port's private (configuration) data.
 * @returns 0 in case of success, exits otherwise.
 */
int
mrvl_start_qos_mapping(struct mrvl_priv *priv)
{
	size_t i;

	if (priv->ppio == NULL) {
		MRVL_LOG(ERR, "ppio must not be NULL here!");
		return -1;
	}

	for (i = 0; i < RTE_DIM(priv->qos_tbl_params.pcp_cos_map); ++i)
		priv->qos_tbl_params.pcp_cos_map[i].ppio = priv->ppio;

	for (i = 0; i < RTE_DIM(priv->qos_tbl_params.dscp_cos_map); ++i)
		priv->qos_tbl_params.dscp_cos_map[i].ppio = priv->ppio;

	/* Initialize Classifier QoS table. */

	return pp2_cls_qos_tbl_init(&priv->qos_tbl_params, &priv->qos_tbl);
}
