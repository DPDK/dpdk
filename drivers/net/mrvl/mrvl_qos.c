/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2017 Marvell International Ltd.
 *   Copyright(c) 2017 Semihalf.
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
 *     * Neither the name of the copyright holder nor the names of its
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_cfgfile.h>
#include <rte_log.h>
#include <rte_lcore.h>
#include <rte_malloc.h>
#include <rte_string_fns.h>

/* Unluckily, container_of is defined by both DPDK and MUSDK,
 * we'll declare only one version.
 *
 * Note that it is not used in this PMD anyway.
 */
#ifdef container_of
#undef container_of
#endif

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
#define MRVL_TOK_SP "SP"
#define MRVL_TOK_TC "tc"
#define MRVL_TOK_TXQ "txq"
#define MRVL_TOK_VLAN "vlan"
#define MRVL_TOK_VLAN_IP "vlan/ip"
#define MRVL_TOK_WEIGHT "weight"

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

	entry = rte_cfgfile_get_entry(file, sec_name,
			MRVL_TOK_WEIGHT);
	if (entry) {
		if (get_val_securely(entry, &val) < 0)
			return -1;
		cfg->port[port].outq[outq].weight = (uint8_t)val;
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
	snprintf(entry_cpy, RTE_DIM(entry_cpy), "%s", entry);

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

	entry = rte_cfgfile_get_entry(file, sec_name, MRVL_TOK_RXQ);
	if (entry) {
		n = get_entry_values(entry,
			cfg->port[port].tc[tc].inq,
			sizeof(cfg->port[port].tc[tc].inq[0]),
			RTE_DIM(cfg->port[port].tc[tc].inq),
			MRVL_PP2_RXQ_MAX);
		if (n < 0) {
			RTE_LOG(ERR, PMD, "Error %d while parsing: %s\n",
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
			RTE_LOG(ERR, PMD, "Error %d while parsing: %s\n",
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
			RTE_LOG(ERR, PMD, "Error %d while parsing: %s\n",
				n, entry);
			return n;
		}
		cfg->port[port].tc[tc].dscps = n;
	}
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
		RTE_LOG(WARNING, PMD, "Empty configuration file?\n");
		return 0;
	}

	/* Use the number of ports given as vdev parameters. */
	for (n = 0; n < (PP2_NUM_ETH_PPIO * PP2_NUM_PKT_PROC); ++n) {
		snprintf(sec_name, sizeof(sec_name), "%s %d %s",
			MRVL_TOK_PORT, n, MRVL_TOK_DEFAULT);

		/* Skip ports non-existing in configuration. */
		if (rte_cfgfile_num_sections(file, sec_name,
				strlen(sec_name)) <= 0) {
			(*cfg)->port[n].use_global_defaults = 1;
			(*cfg)->port[n].mapping_priority =
				PP2_CLS_QOS_TBL_VLAN_IP_PRI;
			continue;
		}

		entry = rte_cfgfile_get_entry(file, sec_name,
				MRVL_TOK_DEFAULT_TC);
		if (entry) {
			if (get_val_securely(entry, &val) < 0 ||
				val > USHRT_MAX)
				return -1;
			(*cfg)->port[n].default_tc = (uint8_t)val;
		} else {
			RTE_LOG(ERR, PMD,
				"Default Traffic Class required in custom configuration!\n");
			return -1;
		}

		entry = rte_cfgfile_get_entry(file, sec_name,
				MRVL_TOK_MAPPING_PRIORITY);
		if (entry) {
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
 * @returns 0 in case of success, exits otherwise.
 */
static int
setup_tc(struct pp2_ppio_tc_params *param, uint8_t inqs,
	struct pp2_bpool *bpool)
{
	struct pp2_ppio_inq_params *inq_params;

	param->pkt_offset = MRVL_PKT_OFFS;
	param->pools[0] = bpool;

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
		/* No port configuration, use default: 1 TC, no QoS. */
		priv->ppio_params.inqs_params.num_tcs = 1;
		setup_tc(&priv->ppio_params.inqs_params.tcs_params[0],
			max_queues, priv->bpool);

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
			RTE_LOG(ERR, PMD,
				"Too many PCPs configured in TC %zu!\n", tc);
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
			RTE_LOG(ERR, PMD,
				"Too many DSCPs configured in TC %zu!\n", tc);
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
			RTE_LOG(ERR, PMD,
				"Too many RX queues configured per TC %zu!\n",
				tc);
			return -1;
		}
		for (i = 0; i < port_cfg->tc[tc].inqs; ++i) {
			uint8_t idx = port_cfg->tc[tc].inq[i];

			if (idx > RTE_DIM(priv->rxq_map)) {
				RTE_LOG(ERR, PMD, "Bad queue index %d!\n", idx);
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
				priv->bpool);
	}

	priv->ppio_params.inqs_params.num_tcs = i;

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
		RTE_LOG(ERR, PMD, "ppio must not be NULL here!\n");
		return -1;
	}

	for (i = 0; i < RTE_DIM(priv->qos_tbl_params.pcp_cos_map); ++i)
		priv->qos_tbl_params.pcp_cos_map[i].ppio = priv->ppio;

	for (i = 0; i < RTE_DIM(priv->qos_tbl_params.dscp_cos_map); ++i)
		priv->qos_tbl_params.dscp_cos_map[i].ppio = priv->ppio;

	/* Initialize Classifier QoS table. */

	return pp2_cls_qos_tbl_init(&priv->qos_tbl_params, &priv->qos_tbl);
}
