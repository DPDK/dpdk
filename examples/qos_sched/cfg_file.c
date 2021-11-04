/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <rte_string_fns.h>
#include <rte_sched.h>

#include "cfg_file.h"
#include "main.h"


/** when we resize a file structure, how many extra entries
 * for new sections do we add in */
#define CFG_ALLOC_SECTION_BATCH 8
/** when we resize a section structure, how many extra entries
 * for new entries do we add in */
#define CFG_ALLOC_ENTRY_BATCH 16

uint32_t active_queues[RTE_SCHED_QUEUES_PER_PIPE];
uint32_t n_active_queues;

int
cfg_load_port(struct rte_cfgfile *cfg, struct rte_sched_port_params *port_params)
{
	const char *entry;

	if (!cfg || !port_params)
		return -1;

	entry = rte_cfgfile_get_entry(cfg, "port", "frame overhead");
	if (entry)
		port_params->frame_overhead = (uint32_t)atoi(entry);

	entry = rte_cfgfile_get_entry(cfg, "port", "number of subports per port");
	if (entry)
		port_params->n_subports_per_port = (uint32_t)atoi(entry);

	return 0;
}

int
cfg_load_pipe(struct rte_cfgfile *cfg, struct rte_sched_pipe_params *pipe_params)
{
	int i, j;
	char *next;
	const char *entry;
	int profiles;

	if (!cfg || !pipe_params)
		return -1;

	profiles = rte_cfgfile_num_sections(cfg, "pipe profile", sizeof("pipe profile") - 1);
	subport_params[0].n_pipe_profiles = profiles;

	for (j = 0; j < profiles; j++) {
		char pipe_name[32];
		snprintf(pipe_name, sizeof(pipe_name), "pipe profile %d", j);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tb rate");
		if (entry)
			pipe_params[j].tb_rate = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tb size");
		if (entry)
			pipe_params[j].tb_size = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc period");
		if (entry)
			pipe_params[j].tc_period = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 0 rate");
		if (entry)
			pipe_params[j].tc_rate[0] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 1 rate");
		if (entry)
			pipe_params[j].tc_rate[1] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 2 rate");
		if (entry)
			pipe_params[j].tc_rate[2] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 3 rate");
		if (entry)
			pipe_params[j].tc_rate[3] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 4 rate");
		if (entry)
			pipe_params[j].tc_rate[4] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 5 rate");
		if (entry)
			pipe_params[j].tc_rate[5] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 6 rate");
		if (entry)
			pipe_params[j].tc_rate[6] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 7 rate");
		if (entry)
			pipe_params[j].tc_rate[7] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 8 rate");
		if (entry)
			pipe_params[j].tc_rate[8] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 9 rate");
		if (entry)
			pipe_params[j].tc_rate[9] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 10 rate");
		if (entry)
			pipe_params[j].tc_rate[10] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 11 rate");
		if (entry)
			pipe_params[j].tc_rate[11] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 12 rate");
		if (entry)
			pipe_params[j].tc_rate[12] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 12 oversubscription weight");
		if (entry)
			pipe_params[j].tc_ov_weight = (uint8_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, pipe_name, "tc 12 wrr weights");
		if (entry) {
			for (i = 0; i < RTE_SCHED_BE_QUEUES_PER_PIPE; i++) {
				pipe_params[j].wrr_weights[i] =
					(uint8_t)strtol(entry, &next, 10);
				if (next == NULL)
					break;
				entry = next;
			}
		}
	}
	return 0;
}

int
cfg_load_subport_profile(struct rte_cfgfile *cfg,
	struct rte_sched_subport_profile_params *subport_profile)
{
	int i;
	const char *entry;
	int profiles;

	if (!cfg || !subport_profile)
		return -1;

	profiles = rte_cfgfile_num_sections(cfg, "subport profile",
					   sizeof("subport profile") - 1);
	subport_params[0].n_pipe_profiles = profiles;

	for (i = 0; i < profiles; i++) {
		char sec_name[32];
		snprintf(sec_name, sizeof(sec_name), "subport profile %d", i);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tb rate");
		if (entry)
			subport_profile[i].tb_rate = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tb size");
		if (entry)
			subport_profile[i].tb_size = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc period");
		if (entry)
			subport_profile[i].tc_period = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 0 rate");
		if (entry)
			subport_profile[i].tc_rate[0] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 1 rate");
		if (entry)
			subport_profile[i].tc_rate[1] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 2 rate");
		if (entry)
			subport_profile[i].tc_rate[2] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 3 rate");
		if (entry)
			subport_profile[i].tc_rate[3] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 4 rate");
		if (entry)
			subport_profile[i].tc_rate[4] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 5 rate");
		if (entry)
			subport_profile[i].tc_rate[5] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 6 rate");
		if (entry)
			subport_profile[i].tc_rate[6] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 7 rate");
		if (entry)
			subport_profile[i].tc_rate[7] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 8 rate");
		if (entry)
			subport_profile[i].tc_rate[8] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 9 rate");
		if (entry)
			subport_profile[i].tc_rate[9] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 10 rate");
		if (entry)
			subport_profile[i].tc_rate[10] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 11 rate");
		if (entry)
			subport_profile[i].tc_rate[11] = (uint64_t)atoi(entry);

		entry = rte_cfgfile_get_entry(cfg, sec_name, "tc 12 rate");
		if (entry)
			subport_profile[i].tc_rate[12] = (uint64_t)atoi(entry);
	}

	return 0;
}

#ifdef RTE_SCHED_CMAN
void set_subport_cman_params(struct rte_sched_subport_params *subport_p,
					struct rte_sched_cman_params cman_p)
{
	int j, k;
	subport_p->cman_params->cman_mode = cman_p.cman_mode;

	for (j = 0; j < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; j++) {
		if (subport_p->cman_params->cman_mode ==
					RTE_SCHED_CMAN_RED) {
			for (k = 0; k < RTE_COLORS; k++) {
				subport_p->cman_params->red_params[j][k].min_th =
					cman_p.red_params[j][k].min_th;
				subport_p->cman_params->red_params[j][k].max_th =
					cman_p.red_params[j][k].max_th;
				subport_p->cman_params->red_params[j][k].maxp_inv =
					cman_p.red_params[j][k].maxp_inv;
				subport_p->cman_params->red_params[j][k].wq_log2 =
					cman_p.red_params[j][k].wq_log2;
			}
		} else {
			subport_p->cman_params->pie_params[j].qdelay_ref =
				cman_p.pie_params[j].qdelay_ref;
			subport_p->cman_params->pie_params[j].dp_update_interval =
				cman_p.pie_params[j].dp_update_interval;
			subport_p->cman_params->pie_params[j].max_burst =
				cman_p.pie_params[j].max_burst;
			subport_p->cman_params->pie_params[j].tailq_th =
				cman_p.pie_params[j].tailq_th;
		}
	}
}
#endif

int
cfg_load_subport(struct rte_cfgfile *cfg, struct rte_sched_subport_params *subport_params)
{
	const char *entry;
	int i, j, k;

	if (!cfg || !subport_params)
		return -1;

	memset(app_pipe_to_profile, -1, sizeof(app_pipe_to_profile));
	memset(active_queues, 0, sizeof(active_queues));
	n_active_queues = 0;

#ifdef RTE_SCHED_CMAN
	struct rte_sched_cman_params cman_params = {
		.cman_mode = RTE_SCHED_CMAN_RED,
		.red_params = { },
	};

	if (rte_cfgfile_has_section(cfg, "red")) {
		cman_params.cman_mode = RTE_SCHED_CMAN_RED;

		for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
			char str[32];

			/* Parse RED min thresholds */
			snprintf(str, sizeof(str), "tc %d red min", i);
			entry = rte_cfgfile_get_entry(cfg, "red", str);
			if (entry) {
				char *next;
				/* for each packet colour (green, yellow, red) */
				for (j = 0; j < RTE_COLORS; j++) {
					cman_params.red_params[i][j].min_th
						= (uint16_t)strtol(entry, &next, 10);
					if (next == NULL)
						break;
					entry = next;
				}
			}

			/* Parse RED max thresholds */
			snprintf(str, sizeof(str), "tc %d red max", i);
			entry = rte_cfgfile_get_entry(cfg, "red", str);
			if (entry) {
				char *next;
				/* for each packet colour (green, yellow, red) */
				for (j = 0; j < RTE_COLORS; j++) {
					cman_params.red_params[i][j].max_th
						= (uint16_t)strtol(entry, &next, 10);
					if (next == NULL)
						break;
					entry = next;
				}
			}

			/* Parse RED inverse mark probabilities */
			snprintf(str, sizeof(str), "tc %d red inv prob", i);
			entry = rte_cfgfile_get_entry(cfg, "red", str);
			if (entry) {
				char *next;
				/* for each packet colour (green, yellow, red) */
				for (j = 0; j < RTE_COLORS; j++) {
					cman_params.red_params[i][j].maxp_inv
						= (uint8_t)strtol(entry, &next, 10);

					if (next == NULL)
						break;
					entry = next;
				}
			}

			/* Parse RED EWMA filter weights */
			snprintf(str, sizeof(str), "tc %d red weight", i);
			entry = rte_cfgfile_get_entry(cfg, "red", str);
			if (entry) {
				char *next;
				/* for each packet colour (green, yellow, red) */
				for (j = 0; j < RTE_COLORS; j++) {
					cman_params.red_params[i][j].wq_log2
						= (uint8_t)strtol(entry, &next, 10);
					if (next == NULL)
						break;
					entry = next;
				}
			}
		}
	}

	if (rte_cfgfile_has_section(cfg, "pie")) {
		cman_params.cman_mode = RTE_SCHED_CMAN_PIE;

		for (i = 0; i < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; i++) {
			char str[32];

			/* Parse Queue Delay Ref value */
			snprintf(str, sizeof(str), "tc %d qdelay ref", i);
			entry = rte_cfgfile_get_entry(cfg, "pie", str);
			if (entry)
				cman_params.pie_params[i].qdelay_ref =
					(uint16_t) atoi(entry);

			/* Parse Max Burst value */
			snprintf(str, sizeof(str), "tc %d max burst", i);
			entry = rte_cfgfile_get_entry(cfg, "pie", str);
			if (entry)
				cman_params.pie_params[i].max_burst =
					(uint16_t) atoi(entry);

			/* Parse Update Interval Value */
			snprintf(str, sizeof(str), "tc %d update interval", i);
			entry = rte_cfgfile_get_entry(cfg, "pie", str);
			if (entry)
				cman_params.pie_params[i].dp_update_interval =
					(uint16_t) atoi(entry);

			/* Parse Tailq Threshold Value */
			snprintf(str, sizeof(str), "tc %d tailq th", i);
			entry = rte_cfgfile_get_entry(cfg, "pie", str);
			if (entry)
				cman_params.pie_params[i].tailq_th =
					(uint16_t) atoi(entry);

		}
	}
#endif /* RTE_SCHED_CMAN */

	for (i = 0; i < MAX_SCHED_SUBPORTS; i++) {
		char sec_name[CFG_NAME_LEN];
		snprintf(sec_name, sizeof(sec_name), "subport %d", i);

		if (rte_cfgfile_has_section(cfg, sec_name)) {
			entry = rte_cfgfile_get_entry(cfg, sec_name,
				"number of pipes per subport");
			if (entry)
				subport_params[i].n_pipes_per_subport_enabled =
					(uint32_t)atoi(entry);

			entry = rte_cfgfile_get_entry(cfg, sec_name, "queue sizes");
			if (entry) {
				char *next;

				for (j = 0; j < RTE_SCHED_TRAFFIC_CLASS_BE; j++) {
					subport_params[i].qsize[j] =
						(uint16_t)strtol(entry, &next, 10);
					if (subport_params[i].qsize[j] != 0) {
						active_queues[n_active_queues] = j;
						n_active_queues++;
					}
					if (next == NULL)
						break;
					entry = next;
				}

				subport_params[i].qsize[RTE_SCHED_TRAFFIC_CLASS_BE] =
					(uint16_t)strtol(entry, &next, 10);

				for (j = 0; j < RTE_SCHED_BE_QUEUES_PER_PIPE; j++) {
					active_queues[n_active_queues] =
						RTE_SCHED_TRAFFIC_CLASS_BE + j;
					n_active_queues++;
				}
			}

			int n_entries = rte_cfgfile_section_num_entries(cfg, sec_name);
			struct rte_cfgfile_entry entries[n_entries];

			rte_cfgfile_section_entries(cfg, sec_name, entries, n_entries);

			for (j = 0; j < n_entries; j++) {
				if (strncmp("pipe", entries[j].name, sizeof("pipe") - 1) == 0) {
					int profile;
					char *tokens[2] = {NULL, NULL};
					int n_tokens;
					int begin, end;

					profile = atoi(entries[j].value);
					n_tokens = rte_strsplit(&entries[j].name[sizeof("pipe")],
							strnlen(entries[j].name, CFG_NAME_LEN), tokens, 2, '-');

					begin =  atoi(tokens[0]);
					if (n_tokens == 2)
						end = atoi(tokens[1]);
					else
						end = begin;

					if (end >= MAX_SCHED_PIPES || begin > end)
						return -1;

					for (k = begin; k <= end; k++) {
						char profile_name[CFG_NAME_LEN];

						snprintf(profile_name, sizeof(profile_name),
								"pipe profile %d", profile);
						if (rte_cfgfile_has_section(cfg, profile_name))
							app_pipe_to_profile[i][k] = profile;
						else
							rte_exit(EXIT_FAILURE, "Wrong pipe profile %s\n",
									entries[j].value);

					}
				}
			}
#ifdef RTE_SCHED_CMAN
			set_subport_cman_params(subport_params+i, cman_params);
#endif
		}
	}

	return 0;
}
