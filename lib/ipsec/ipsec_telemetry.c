/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2021 Intel Corporation
 */

#include <rte_ipsec.h>
#include <rte_telemetry.h>
#include <rte_malloc.h>
#include "sa.h"


struct ipsec_telemetry_entry {
	LIST_ENTRY(ipsec_telemetry_entry) next;
	const struct rte_ipsec_sa *sa;
};
static LIST_HEAD(ipsec_telemetry_head, ipsec_telemetry_entry)
		ipsec_telemetry_list = LIST_HEAD_INITIALIZER();

static int
handle_telemetry_cmd_ipsec_sa_list(const char *cmd __rte_unused,
		const char *params __rte_unused,
		struct rte_tel_data *data)
{
	struct ipsec_telemetry_entry *entry;
	rte_tel_data_start_array(data, RTE_TEL_U64_VAL);

	LIST_FOREACH(entry, &ipsec_telemetry_list, next) {
		const struct rte_ipsec_sa *sa = entry->sa;
		rte_tel_data_add_array_u64(data, rte_be_to_cpu_32(sa->spi));
	}

	return 0;
}

/**
 * Handle IPsec SA statistics telemetry request
 *
 * Return dict of SA's with dict of key/value counters
 *
 * {
 *     "SA_SPI_XX": {"count": 0, "bytes": 0, "errors": 0},
 *     "SA_SPI_YY": {"count": 0, "bytes": 0, "errors": 0}
 * }
 *
 */
static int
handle_telemetry_cmd_ipsec_sa_stats(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *data)
{
	struct ipsec_telemetry_entry *entry;
	const struct rte_ipsec_sa *sa;
	uint32_t sa_spi = 0;

	if (params) {
		sa_spi = rte_cpu_to_be_32((uint32_t)strtoul(params, NULL, 0));
		if (sa_spi == 0)
			return -EINVAL;
	}

	rte_tel_data_start_dict(data);

	LIST_FOREACH(entry, &ipsec_telemetry_list, next) {
		char sa_name[64];
		sa = entry->sa;
		static const char *name_pkt_cnt = "count";
		static const char *name_byte_cnt = "bytes";
		static const char *name_error_cnt = "errors";
		struct rte_tel_data *sa_data;

		/* If user provided SPI only get telemetry for that SA */
		if (sa_spi && (sa_spi != sa->spi))
			continue;

		/* allocate telemetry data struct for SA telemetry */
		sa_data = rte_tel_data_alloc();
		if (!sa_data)
			return -ENOMEM;

		rte_tel_data_start_dict(sa_data);

		/* add telemetry key/values pairs */
		rte_tel_data_add_dict_u64(sa_data, name_pkt_cnt,
					sa->statistics.count);

		rte_tel_data_add_dict_u64(sa_data, name_byte_cnt,
					sa->statistics.bytes -
					(sa->statistics.count * sa->hdr_len));

		rte_tel_data_add_dict_u64(sa_data, name_error_cnt,
					sa->statistics.errors.count);

		/* generate telemetry label */
		snprintf(sa_name, sizeof(sa_name), "SA_SPI_%i",
				rte_be_to_cpu_32(sa->spi));

		/* add SA telemetry to dictionary container */
		rte_tel_data_add_dict_container(data, sa_name, sa_data, 0);
	}

	return 0;
}

static int
handle_telemetry_cmd_ipsec_sa_details(const char *cmd __rte_unused,
		const char *params,
		struct rte_tel_data *data)
{
	struct ipsec_telemetry_entry *entry;
	const struct rte_ipsec_sa *sa;
	uint32_t sa_spi = 0;

	if (params)
		sa_spi = rte_cpu_to_be_32((uint32_t)strtoul(params, NULL, 0));
	/* valid SPI needed */
	if (sa_spi == 0)
		return -EINVAL;


	rte_tel_data_start_dict(data);

	LIST_FOREACH(entry, &ipsec_telemetry_list, next) {
		uint64_t mode;
		sa = entry->sa;
		if (sa_spi != sa->spi)
			continue;

		/* add SA configuration key/values pairs */
		rte_tel_data_add_dict_string(data, "Type",
			(sa->type & RTE_IPSEC_SATP_PROTO_MASK) ==
			RTE_IPSEC_SATP_PROTO_AH ? "AH" : "ESP");

		rte_tel_data_add_dict_string(data, "Direction",
			(sa->type & RTE_IPSEC_SATP_DIR_MASK) ==
			RTE_IPSEC_SATP_DIR_IB ?	"Inbound" : "Outbound");

		mode = sa->type & RTE_IPSEC_SATP_MODE_MASK;

		if (mode == RTE_IPSEC_SATP_MODE_TRANS) {
			rte_tel_data_add_dict_string(data, "Mode", "Transport");
		} else {
			rte_tel_data_add_dict_string(data, "Mode", "Tunnel");

			if ((sa->type & RTE_IPSEC_SATP_NATT_MASK) ==
				RTE_IPSEC_SATP_NATT_ENABLE) {
				if (sa->type & RTE_IPSEC_SATP_MODE_TUNLV4) {
					rte_tel_data_add_dict_string(data,
						"Tunnel-Type",
						"IPv4-UDP");
				} else if (sa->type &
						RTE_IPSEC_SATP_MODE_TUNLV6) {
					rte_tel_data_add_dict_string(data,
						"Tunnel-Type",
						"IPv6-UDP");
				}
			} else {
				if (sa->type & RTE_IPSEC_SATP_MODE_TUNLV4) {
					rte_tel_data_add_dict_string(data,
						"Tunnel-Type",
						"IPv4");
				} else if (sa->type &
						RTE_IPSEC_SATP_MODE_TUNLV6) {
					rte_tel_data_add_dict_string(data,
						"Tunnel-Type",
						"IPv6");
				}
			}
		}

		rte_tel_data_add_dict_string(data,
				"extended-sequence-number",
				(sa->type & RTE_IPSEC_SATP_ESN_MASK) ==
				 RTE_IPSEC_SATP_ESN_ENABLE ?
				"enabled" : "disabled");

		if ((sa->type & RTE_IPSEC_SATP_DIR_MASK) ==
			RTE_IPSEC_SATP_DIR_IB)

			if (sa->sqn.inb.rsn[sa->sqn.inb.rdidx])
				rte_tel_data_add_dict_u64(data,
				"sequence-number",
				sa->sqn.inb.rsn[sa->sqn.inb.rdidx]->sqn);
			else
				rte_tel_data_add_dict_u64(data,
					"sequence-number", 0);
		else
			rte_tel_data_add_dict_u64(data, "sequence-number",
					sa->sqn.outb);

		rte_tel_data_add_dict_string(data,
				"explicit-congestion-notification",
				(sa->type & RTE_IPSEC_SATP_ECN_MASK) ==
				RTE_IPSEC_SATP_ECN_ENABLE ?
				"enabled" : "disabled");

		rte_tel_data_add_dict_string(data,
				"copy-DSCP",
				(sa->type & RTE_IPSEC_SATP_DSCP_MASK) ==
				RTE_IPSEC_SATP_DSCP_ENABLE ?
				"enabled" : "disabled");
	}

	return 0;
}


int
rte_ipsec_telemetry_sa_add(const struct rte_ipsec_sa *sa)
{
	struct ipsec_telemetry_entry *entry = rte_zmalloc(NULL,
			sizeof(struct ipsec_telemetry_entry), 0);
	if (entry == NULL)
		return -ENOMEM;
	entry->sa = sa;
	LIST_INSERT_HEAD(&ipsec_telemetry_list, entry, next);
	return 0;
}

void
rte_ipsec_telemetry_sa_del(const struct rte_ipsec_sa *sa)
{
	struct ipsec_telemetry_entry *entry;
	LIST_FOREACH(entry, &ipsec_telemetry_list, next) {
		if (sa == entry->sa) {
			LIST_REMOVE(entry, next);
			rte_free(entry);
			return;
		}
	}
}


RTE_INIT(rte_ipsec_telemetry_init)
{
	rte_telemetry_register_cmd("/ipsec/sa/list",
		handle_telemetry_cmd_ipsec_sa_list,
		"Return list of IPsec SAs with telemetry enabled.");
	rte_telemetry_register_cmd("/ipsec/sa/stats",
		handle_telemetry_cmd_ipsec_sa_stats,
		"Returns IPsec SA statistics. Parameters: int sa_spi");
	rte_telemetry_register_cmd("/ipsec/sa/details",
		handle_telemetry_cmd_ipsec_sa_details,
		"Returns IPsec SA configuration. Parameters: int sa_spi");
}
