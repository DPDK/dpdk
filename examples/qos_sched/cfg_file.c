/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
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

static unsigned
_strip(char *str, unsigned len)
{
	int newlen = len;
	if (len == 0)
		return 0;

	if (isspace(str[len-1])) {
		/* strip trailing whitespace */
		while (newlen > 0 && isspace(str[newlen - 1]))
			str[--newlen] = '\0';
	}

	if (isspace(str[0])) {
		/* strip leading whitespace */
		int i,start = 1;
		while (isspace(str[start]) && start < newlen)
			start++
			; /* do nothing */
		newlen -= start;
		for (i = 0; i < newlen; i++)
			str[i] = str[i+start];
		str[i] = '\0';
	}
	return newlen;
}

struct cfg_file *
cfg_load(const char *filename, int flags)
{
	int allocated_sections = CFG_ALLOC_SECTION_BATCH;
	int allocated_entries = 0;
	int curr_section = -1;
	int curr_entry = -1;
	char buffer[256];
	int lineno = 0;
	struct cfg_file *cfg = NULL;

	FILE *f = fopen(filename, "r");
	if (f == NULL)
		return NULL;

	cfg = malloc(sizeof(*cfg) +	sizeof(cfg->sections[0]) * allocated_sections);
	if (cfg == NULL)
		goto error2;

	memset(cfg->sections, 0, sizeof(cfg->sections[0]) * allocated_sections);

	while (fgets(buffer, sizeof(buffer), f) != NULL) {
		char *pos = NULL;
		size_t len = strnlen(buffer, sizeof(buffer));
		lineno++;
		if (len >=sizeof(buffer) - 1 && buffer[len-1] != '\n'){
			printf("Error line %d - no \\n found on string. "
					"Check if line too long\n", lineno);
			goto error1;
		}
		if ((pos = memchr(buffer, ';', sizeof(buffer))) != NULL) {
			*pos = '\0';
			len = pos -  buffer;
		}

		len = _strip(buffer, len);
		if (buffer[0] != '[' && memchr(buffer, '=', len) == NULL)
			continue;

		if (buffer[0] == '[') {
			/* section heading line */
			char *end = memchr(buffer, ']', len);
			if (end == NULL) {
				printf("Error line %d - no terminating '[' found\n", lineno);
				goto error1;
			}
			*end = '\0';
			_strip(&buffer[1], end - &buffer[1]);

			/* close off old section and add start new one */
			if (curr_section >= 0)
				cfg->sections[curr_section]->num_entries = curr_entry + 1;
			curr_section++;

			/* resize overall struct if we don't have room for more sections */
			if (curr_section == allocated_sections) {
				allocated_sections += CFG_ALLOC_SECTION_BATCH;
				struct cfg_file *n_cfg = realloc(cfg, sizeof(*cfg) +
						sizeof(cfg->sections[0]) * allocated_sections);
				if (n_cfg == NULL) {
					printf("Error - no more memory\n");
					goto error1;
				}
				cfg = n_cfg;
			}

			/* allocate space for new section */
			allocated_entries = CFG_ALLOC_ENTRY_BATCH;
			curr_entry = -1;
			cfg->sections[curr_section] = malloc(sizeof(*cfg->sections[0]) +
					sizeof(cfg->sections[0]->entries[0]) * allocated_entries);
			if (cfg->sections[curr_section] == NULL) {
				printf("Error - no more memory\n");
				goto error1;
			}

			snprintf(cfg->sections[curr_section]->name,
					sizeof(cfg->sections[0]->name),
					"%s", &buffer[1]);
		}
		else {
			/* value line */
			if (curr_section < 0) {
				printf("Error line %d - value outside of section\n", lineno);
				goto error1;
			}

			struct cfg_section *sect = cfg->sections[curr_section];
			char *split[2];
			if (rte_strsplit(buffer, sizeof(buffer), split, 2, '=') != 2) {
				printf("Error at line %d - cannot split string\n", lineno);
				goto error1;
			}

			curr_entry++;
			if (curr_entry == allocated_entries) {
				allocated_entries += CFG_ALLOC_ENTRY_BATCH;
				struct cfg_section *n_sect = realloc(sect, sizeof(*sect) +
						sizeof(sect->entries[0]) * allocated_entries);
				if (n_sect == NULL) {
					printf("Error - no more memory\n");
					goto error1;
				}
				sect = cfg->sections[curr_section] = n_sect;
			}

			sect->entries[curr_entry] = malloc(sizeof(*sect->entries[0]));
			if (sect->entries[curr_entry] == NULL) {
				printf("Error - no more memory\n");
				goto error1;
			}

			struct cfg_entry *entry = sect->entries[curr_entry];
			snprintf(entry->name, sizeof(entry->name), "%s", split[0]);
			snprintf(entry->value, sizeof(entry->value), "%s", split[1]);
			_strip(entry->name, strnlen(entry->name, sizeof(entry->name)));
			_strip(entry->value, strnlen(entry->value, sizeof(entry->value)));
		}
	}
	fclose(f);
	cfg->flags = flags;
	cfg->sections[curr_section]->num_entries = curr_entry + 1;
	cfg->num_sections = curr_section + 1;
	return cfg;

error1:
	cfg_close(cfg);
error2:
	fclose(f);
	return NULL;
}


int cfg_close(struct cfg_file *cfg)
{
	int i, j;

	if (cfg == NULL)
		return -1;

	for(i = 0; i < cfg->num_sections; i++) {
		if (cfg->sections[i] != NULL) {
			if (cfg->sections[i]->num_entries) {
				for(j = 0; j < cfg->sections[i]->num_entries; j++) {
					if (cfg->sections[i]->entries[j] != NULL)
						free(cfg->sections[i]->entries[j]);
				}
			}
			free(cfg->sections[i]);
		}
	}
	free(cfg);

	return 0;
}

int
cfg_num_sections(struct cfg_file *cfg, const char *sectionname, size_t length)
{
	int i;
	int num_sections = 0;
	for (i = 0; i < cfg->num_sections; i++) {
		if (strncmp(cfg->sections[i]->name, sectionname, length) == 0)
			num_sections++;
	}
	return num_sections;
}

int
cfg_sections(struct cfg_file *cfg, char *sections[], int max_sections)
{
	int i;
	for (i = 0; i < cfg->num_sections && i < max_sections; i++) {
		snprintf(sections[i], CFG_NAME_LEN, "%s",  cfg->sections[i]->name);
	}
	return i;
}

static const struct cfg_section *
_get_section(struct cfg_file *cfg, const char *sectionname)
{
	int i;
	for (i = 0; i < cfg->num_sections; i++) {
		if (strncmp(cfg->sections[i]->name, sectionname,
				sizeof(cfg->sections[0]->name)) == 0)
			return cfg->sections[i];
	}
	return NULL;
}

int
cfg_has_section(struct cfg_file *cfg, const char *sectionname)
{
	return (_get_section(cfg, sectionname) != NULL);
}

int
cfg_section_num_entries(struct cfg_file *cfg, const char *sectionname)
{
	const struct cfg_section *s = _get_section(cfg, sectionname);
	if (s == NULL)
		return -1;
	return s->num_entries;
}


int
cfg_section_entries(struct cfg_file *cfg, const char *sectionname,
		struct cfg_entry *entries, int max_entries)
{
	int i;
	const struct cfg_section *sect = _get_section(cfg, sectionname);
	if (sect == NULL)
		return -1;
	for (i = 0; i < max_entries && i < sect->num_entries; i++)
		entries[i] = *sect->entries[i];
	return i;
}

const char *
cfg_get_entry(struct cfg_file *cfg, const char *sectionname,
		const char *entryname)
{
	int i;
	const struct cfg_section *sect = _get_section(cfg, sectionname);
	if (sect == NULL)
		return NULL;
	for (i = 0; i < sect->num_entries; i++)
		if (strncmp(sect->entries[i]->name, entryname, CFG_NAME_LEN) == 0)
			return sect->entries[i]->value;
	return NULL;
}

int
cfg_has_entry(struct cfg_file *cfg, const char *sectionname,
		const char *entryname)
{
	return (cfg_get_entry(cfg, sectionname, entryname) != NULL);
}


int
cfg_load_port(struct cfg_file *cfg, struct rte_sched_port_params *port_params)
{
	const char *entry;
	int j;

	if (!cfg || !port_params)
		return -1;

	entry = cfg_get_entry(cfg, "port", "frame overhead");
	if (entry)
		port_params->frame_overhead = (uint32_t)atoi(entry);

	entry = cfg_get_entry(cfg, "port", "number of subports per port");
	if (entry)
		port_params->n_subports_per_port = (uint32_t)atoi(entry);

	entry = cfg_get_entry(cfg, "port", "number of pipes per subport");
	if (entry)
		port_params->n_pipes_per_subport = (uint32_t)atoi(entry);

	entry = cfg_get_entry(cfg, "port", "queue sizes");
	if (entry) {
		char *next;

		for(j = 0; j < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; j++) {
			port_params->qsize[j] = (uint16_t)strtol(entry, &next, 10);
			if (next == NULL)
				break;
			entry = next;
		}
	}

#ifdef RTE_SCHED_RED
	for (j = 0; j < RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE; j++) {
		char str[32];

		/* Parse WRED min thresholds */
		snprintf(str, sizeof(str), "tc %d wred min", j);
		entry = cfg_get_entry(cfg, "red", str);
		if (entry) {
			char *next;
			int k;
			/* for each packet colour (green, yellow, red) */
			for (k = 0; k < e_RTE_METER_COLORS; k++) {
				port_params->red_params[j][k].min_th
					= (uint16_t)strtol(entry, &next, 10);
				if (next == NULL)
					break;
				entry = next;
			}
		}

		/* Parse WRED max thresholds */
		snprintf(str, sizeof(str), "tc %d wred max", j);
		entry = cfg_get_entry(cfg, "red", str);
		if (entry) {
			char *next;
			int k;
			/* for each packet colour (green, yellow, red) */
			for (k = 0; k < e_RTE_METER_COLORS; k++) {
				port_params->red_params[j][k].max_th
					= (uint16_t)strtol(entry, &next, 10);
				if (next == NULL)
					break;
				entry = next;
			}
		}

		/* Parse WRED inverse mark probabilities */
		snprintf(str, sizeof(str), "tc %d wred inv prob", j);
		entry = cfg_get_entry(cfg, "red", str);
		if (entry) {
			char *next;
			int k;
			/* for each packet colour (green, yellow, red) */
			for (k = 0; k < e_RTE_METER_COLORS; k++) {
				port_params->red_params[j][k].maxp_inv
					= (uint8_t)strtol(entry, &next, 10);

				if (next == NULL)
					break;
				entry = next;
			}
		}

		/* Parse WRED EWMA filter weights */
		snprintf(str, sizeof(str), "tc %d wred weight", j);
		entry = cfg_get_entry(cfg, "red", str);
		if (entry) {
			char *next;
			int k;
			/* for each packet colour (green, yellow, red) */
			for (k = 0; k < e_RTE_METER_COLORS; k++) {
				port_params->red_params[j][k].wq_log2
					= (uint8_t)strtol(entry, &next, 10);
				if (next == NULL)
					break;
				entry = next;
			}
		}
	}
#endif /* RTE_SCHED_RED */

	return 0;
}

int
cfg_load_pipe(struct cfg_file *cfg, struct rte_sched_pipe_params *pipe_params)
{
	int i, j;
	char *next;
	const char *entry;
	int profiles;

	if (!cfg || !pipe_params)
		return -1;

	profiles = cfg_num_sections(cfg, "pipe profile", sizeof("pipe profile") - 1);
	port_params.n_pipe_profiles = profiles;

	for (j = 0; j < profiles; j++) {
		char pipe_name[32];
		snprintf(pipe_name, sizeof(pipe_name), "pipe profile %d", j);

		entry = cfg_get_entry(cfg, pipe_name, "tb rate");
		if (entry)
			pipe_params[j].tb_rate = (uint32_t)atoi(entry);

		entry = cfg_get_entry(cfg, pipe_name, "tb size");
		if (entry)
			pipe_params[j].tb_size = (uint32_t)atoi(entry);

		entry = cfg_get_entry(cfg, pipe_name, "tc period");
		if (entry)
			pipe_params[j].tc_period = (uint32_t)atoi(entry);

		entry = cfg_get_entry(cfg, pipe_name, "tc 0 rate");
		if (entry)
			pipe_params[j].tc_rate[0] = (uint32_t)atoi(entry);

		entry = cfg_get_entry(cfg, pipe_name, "tc 1 rate");
		if (entry)
			pipe_params[j].tc_rate[1] = (uint32_t)atoi(entry);

		entry = cfg_get_entry(cfg, pipe_name, "tc 2 rate");
		if (entry)
			pipe_params[j].tc_rate[2] = (uint32_t)atoi(entry);

		entry = cfg_get_entry(cfg, pipe_name, "tc 3 rate");
		if (entry)
			pipe_params[j].tc_rate[3] = (uint32_t)atoi(entry);

#ifdef RTE_SCHED_SUBPORT_TC_OV
		entry = cfg_get_entry(cfg, pipe_name, "tc 3 oversubscription weight");
		if (entry)
			pipe_params[j].tc_ov_weight = (uint8_t)atoi(entry);
#endif

		entry = cfg_get_entry(cfg, pipe_name, "tc 0 wrr weights");
		if (entry) {
			for(i = 0; i < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; i++) {
				pipe_params[j].wrr_weights[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE*0 + i] =
					(uint8_t)strtol(entry, &next, 10);
				if (next == NULL)
					break;
				entry = next;
			}
		}
		entry = cfg_get_entry(cfg, pipe_name, "tc 1 wrr weights");
		if (entry) {
			for(i = 0; i < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; i++) {
				pipe_params[j].wrr_weights[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE*1 + i] =
					(uint8_t)strtol(entry, &next, 10);
				if (next == NULL)
					break;
				entry = next;
			}
		}
		entry = cfg_get_entry(cfg, pipe_name, "tc 2 wrr weights");
		if (entry) {
			for(i = 0; i < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; i++) {
				pipe_params[j].wrr_weights[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE*2 + i] =
					(uint8_t)strtol(entry, &next, 10);
				if (next == NULL)
					break;
				entry = next;
			}
		}
		entry = cfg_get_entry(cfg, pipe_name, "tc 3 wrr weights");
		if (entry) {
			for(i = 0; i < RTE_SCHED_QUEUES_PER_TRAFFIC_CLASS; i++) {
				pipe_params[j].wrr_weights[RTE_SCHED_TRAFFIC_CLASSES_PER_PIPE*3 + i] =
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
cfg_load_subport(struct cfg_file *cfg, struct rte_sched_subport_params *subport_params)
{
	const char *entry;
	int i, j, k;

	if (!cfg || !subport_params)
		return -1;

	memset(app_pipe_to_profile, -1, sizeof(app_pipe_to_profile));

	for (i = 0; i < MAX_SCHED_SUBPORTS; i++) {
		char sec_name[CFG_NAME_LEN];
		snprintf(sec_name, sizeof(sec_name), "subport %d", i);

		if (cfg_has_section(cfg, sec_name)) {
			entry = cfg_get_entry(cfg, sec_name, "tb rate");
			if (entry)
				subport_params[i].tb_rate = (uint32_t)atoi(entry);

			entry = cfg_get_entry(cfg, sec_name, "tb size");
			if (entry)
				subport_params[i].tb_size = (uint32_t)atoi(entry);

			entry = cfg_get_entry(cfg, sec_name, "tc period");
			if (entry)
				subport_params[i].tc_period = (uint32_t)atoi(entry);

			entry = cfg_get_entry(cfg, sec_name, "tc 0 rate");
			if (entry)
				subport_params[i].tc_rate[0] = (uint32_t)atoi(entry);

			entry = cfg_get_entry(cfg, sec_name, "tc 1 rate");
			if (entry)
				subport_params[i].tc_rate[1] = (uint32_t)atoi(entry);

			entry = cfg_get_entry(cfg, sec_name, "tc 2 rate");
			if (entry)
				subport_params[i].tc_rate[2] = (uint32_t)atoi(entry);

			entry = cfg_get_entry(cfg, sec_name, "tc 3 rate");
			if (entry)
				subport_params[i].tc_rate[3] = (uint32_t)atoi(entry);

			int n_entries = cfg_section_num_entries(cfg, sec_name);
			struct cfg_entry entries[n_entries];

			cfg_section_entries(cfg, sec_name, entries, n_entries);

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
						if (cfg_has_section(cfg, profile_name))
							app_pipe_to_profile[i][k] = profile;
						else
							rte_exit(EXIT_FAILURE, "Wrong pipe profile %s\n",
									entries[j].value);

					}
				}
			}
		}
	}

	return 0;
}


