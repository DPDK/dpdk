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

#include "rte_cfgfile.h"

struct rte_cfgfile_section {
	char name[CFG_NAME_LEN];
	int num_entries;
	struct rte_cfgfile_entry *entries[0];
};

struct rte_cfgfile {
	int flags;
	int num_sections;
	struct rte_cfgfile_section *sections[0];
};

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
		int i, start = 1;
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

struct rte_cfgfile *
rte_cfgfile_load(const char *filename, int flags)
{
	int allocated_sections = CFG_ALLOC_SECTION_BATCH;
	int allocated_entries = 0;
	int curr_section = -1;
	int curr_entry = -1;
	char buffer[256] = {0};
	int lineno = 0;
	struct rte_cfgfile *cfg = NULL;

	FILE *f = fopen(filename, "r");
	if (f == NULL)
		return NULL;

	cfg = malloc(sizeof(*cfg) + sizeof(cfg->sections[0]) *
		allocated_sections);
	if (cfg == NULL)
		goto error2;

	memset(cfg->sections, 0, sizeof(cfg->sections[0]) * allocated_sections);

	while (fgets(buffer, sizeof(buffer), f) != NULL) {
		char *pos = NULL;
		size_t len = strnlen(buffer, sizeof(buffer));
		lineno++;
		if ((len >= sizeof(buffer) - 1) && (buffer[len-1] != '\n')) {
			printf("Error line %d - no \\n found on string. "
					"Check if line too long\n", lineno);
			goto error1;
		}
		pos = memchr(buffer, ';', sizeof(buffer));
		if (pos != NULL) {
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
				printf("Error line %d - no terminating '['"
					"character found\n", lineno);
				goto error1;
			}
			*end = '\0';
			_strip(&buffer[1], end - &buffer[1]);

			/* close off old section and add start new one */
			if (curr_section >= 0)
				cfg->sections[curr_section]->num_entries =
					curr_entry + 1;
			curr_section++;

			/* resize overall struct if we don't have room for more
			sections */
			if (curr_section == allocated_sections) {
				allocated_sections += CFG_ALLOC_SECTION_BATCH;
				struct rte_cfgfile *n_cfg = realloc(cfg,
					sizeof(*cfg) + sizeof(cfg->sections[0])
					* allocated_sections);
				if (n_cfg == NULL) {
					printf("Error - no more memory\n");
					goto error1;
				}
				cfg = n_cfg;
			}

			/* allocate space for new section */
			allocated_entries = CFG_ALLOC_ENTRY_BATCH;
			curr_entry = -1;
			cfg->sections[curr_section] = malloc(
				sizeof(*cfg->sections[0]) +
				sizeof(cfg->sections[0]->entries[0]) *
				allocated_entries);
			if (cfg->sections[curr_section] == NULL) {
				printf("Error - no more memory\n");
				goto error1;
			}

			snprintf(cfg->sections[curr_section]->name,
					sizeof(cfg->sections[0]->name),
					"%s", &buffer[1]);
		} else {
			/* value line */
			if (curr_section < 0) {
				printf("Error line %d - value outside of"
					"section\n", lineno);
				goto error1;
			}

			struct rte_cfgfile_section *sect =
				cfg->sections[curr_section];
			char *split[2];
			if (rte_strsplit(buffer, sizeof(buffer), split, 2, '=')
				!= 2) {
				printf("Error at line %d - cannot split "
					"string\n", lineno);
				goto error1;
			}

			curr_entry++;
			if (curr_entry == allocated_entries) {
				allocated_entries += CFG_ALLOC_ENTRY_BATCH;
				struct rte_cfgfile_section *n_sect = realloc(
					sect, sizeof(*sect) +
					sizeof(sect->entries[0]) *
					allocated_entries);
				if (n_sect == NULL) {
					printf("Error - no more memory\n");
					goto error1;
				}
				sect = cfg->sections[curr_section] = n_sect;
			}

			sect->entries[curr_entry] = malloc(
				sizeof(*sect->entries[0]));
			if (sect->entries[curr_entry] == NULL) {
				printf("Error - no more memory\n");
				goto error1;
			}

			struct rte_cfgfile_entry *entry = sect->entries[
				curr_entry];
			snprintf(entry->name, sizeof(entry->name), "%s",
				split[0]);
			snprintf(entry->value, sizeof(entry->value), "%s",
				split[1]);
			_strip(entry->name, strnlen(entry->name,
				sizeof(entry->name)));
			_strip(entry->value, strnlen(entry->value,
				sizeof(entry->value)));
		}
	}
	fclose(f);
	cfg->flags = flags;
	cfg->num_sections = curr_section + 1;
	/* curr_section will still be -1 if we have an empty file */
	if (curr_section >= 0)
		cfg->sections[curr_section]->num_entries = curr_entry + 1;
	return cfg;

error1:
	cfg->num_sections = curr_section + 1;
	rte_cfgfile_close(cfg);
error2:
	fclose(f);
	return NULL;
}


int rte_cfgfile_close(struct rte_cfgfile *cfg)
{
	int i, j;

	if (cfg == NULL)
		return -1;

	for (i = 0; i < cfg->num_sections; i++) {
		if (cfg->sections[i] != NULL) {
			if (cfg->sections[i]->num_entries) {
				for (j = 0; j < cfg->sections[i]->num_entries;
					j++) {
					if (cfg->sections[i]->entries[j] !=
						NULL)
						free(cfg->sections[i]->
							entries[j]);
				}
			}
			free(cfg->sections[i]);
		}
	}
	free(cfg);

	return 0;
}

int
rte_cfgfile_num_sections(struct rte_cfgfile *cfg, const char *sectionname,
size_t length)
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
rte_cfgfile_sections(struct rte_cfgfile *cfg, char *sections[],
	int max_sections)
{
	int i;

	for (i = 0; i < cfg->num_sections && i < max_sections; i++)
		snprintf(sections[i], CFG_NAME_LEN, "%s",
		cfg->sections[i]->name);

	return i;
}

static const struct rte_cfgfile_section *
_get_section(struct rte_cfgfile *cfg, const char *sectionname)
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
rte_cfgfile_has_section(struct rte_cfgfile *cfg, const char *sectionname)
{
	return _get_section(cfg, sectionname) != NULL;
}

int
rte_cfgfile_section_num_entries(struct rte_cfgfile *cfg,
	const char *sectionname)
{
	const struct rte_cfgfile_section *s = _get_section(cfg, sectionname);
	if (s == NULL)
		return -1;
	return s->num_entries;
}


int
rte_cfgfile_section_entries(struct rte_cfgfile *cfg, const char *sectionname,
		struct rte_cfgfile_entry *entries, int max_entries)
{
	int i;
	const struct rte_cfgfile_section *sect = _get_section(cfg, sectionname);
	if (sect == NULL)
		return -1;
	for (i = 0; i < max_entries && i < sect->num_entries; i++)
		entries[i] = *sect->entries[i];
	return i;
}

int
rte_cfgfile_section_entries_by_index(struct rte_cfgfile *cfg, int index,
		char *sectionname,
		struct rte_cfgfile_entry *entries, int max_entries)
{
	int i;
	const struct rte_cfgfile_section *sect;

	if (index < 0 || index >= cfg->num_sections)
		return -1;

	sect = cfg->sections[index];
	snprintf(sectionname, CFG_NAME_LEN, "%s", sect->name);
	for (i = 0; i < max_entries && i < sect->num_entries; i++)
		entries[i] = *sect->entries[i];
	return i;
}

const char *
rte_cfgfile_get_entry(struct rte_cfgfile *cfg, const char *sectionname,
		const char *entryname)
{
	int i;
	const struct rte_cfgfile_section *sect = _get_section(cfg, sectionname);
	if (sect == NULL)
		return NULL;
	for (i = 0; i < sect->num_entries; i++)
		if (strncmp(sect->entries[i]->name, entryname, CFG_NAME_LEN)
			== 0)
			return sect->entries[i]->value;
	return NULL;
}

int
rte_cfgfile_has_entry(struct rte_cfgfile *cfg, const char *sectionname,
		const char *entryname)
{
	return rte_cfgfile_get_entry(cfg, sectionname, entryname) != NULL;
}
