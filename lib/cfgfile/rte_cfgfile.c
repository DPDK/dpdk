/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <errno.h>
#include <rte_string_fns.h>
#include <rte_common.h>
#include <rte_log.h>

#include "rte_cfgfile.h"

struct rte_cfgfile_section {
	char name[CFG_NAME_LEN];
	int num_entries;
	int allocated_entries;
	struct rte_cfgfile_entry *entries;
};

struct rte_cfgfile {
	int flags;
	int num_sections;
	int allocated_sections;
	struct rte_cfgfile_section *sections;
};

/* Setting up dynamic logging 8< */
RTE_LOG_REGISTER_DEFAULT(cfgfile_logtype, INFO);

#define CFG_LOG(level, fmt, args...)					\
	rte_log(RTE_LOG_ ## level, cfgfile_logtype, "%s(): " fmt "\n",	\
		__func__, ## args)
/* >8 End of setting up dynamic logging */

/** when we resize a file structure, how many extra entries
 * for new sections do we add in */
#define CFG_ALLOC_SECTION_BATCH 8
/** when we resize a section structure, how many extra entries
 * for new entries do we add in */
#define CFG_ALLOC_ENTRY_BATCH 16

/**
 * Default cfgfile load parameters.
 */
static const struct rte_cfgfile_parameters default_cfgfile_params = {
	.comment_character = CFG_DEFAULT_COMMENT_CHARACTER,
};

/**
 * Defines the list of acceptable comment characters supported by this
 * library.
 */
static const char valid_comment_chars[] = {
	'!',
	'#',
	'%',
	';',
	'@'
};

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

static struct rte_cfgfile_section *
_get_section(struct rte_cfgfile *cfg, const char *sectionname)
{
	int i;

	for (i = 0; i < cfg->num_sections; i++) {
		if (strncmp(cfg->sections[i].name, sectionname,
				sizeof(cfg->sections[0].name)) == 0)
			return &cfg->sections[i];
	}
	return NULL;
}

static int
_add_entry(struct rte_cfgfile_section *section, const char *entryname,
		const char *entryvalue)
{
	/* resize entry structure if we don't have room for more entries */
	if (section->num_entries == section->allocated_entries) {
		struct rte_cfgfile_entry *n_entries = realloc(
				section->entries,
				sizeof(struct rte_cfgfile_entry) *
				((section->allocated_entries) +
						CFG_ALLOC_ENTRY_BATCH));

		if (n_entries == NULL)
			return -ENOMEM;

		section->entries = n_entries;
		section->allocated_entries += CFG_ALLOC_ENTRY_BATCH;
	}
	/* fill up entry fields with key name and value */
	struct rte_cfgfile_entry *curr_entry =
					&section->entries[section->num_entries];

	strlcpy(curr_entry->name, entryname, sizeof(curr_entry->name));
	strlcpy(curr_entry->value, entryvalue, sizeof(curr_entry->value));
	section->num_entries++;

	return 0;
}

static int
rte_cfgfile_check_params(const struct rte_cfgfile_parameters *params)
{
	unsigned int valid_comment;
	unsigned int i;

	if (!params) {
		CFG_LOG(ERR, "missing cfgfile parameters\n");
		return -EINVAL;
	}

	valid_comment = 0;
	for (i = 0; i < RTE_DIM(valid_comment_chars); i++) {
		if (params->comment_character == valid_comment_chars[i]) {
			valid_comment = 1;
			break;
		}
	}

	if (valid_comment == 0)	{
		CFG_LOG(ERR, "invalid comment characters %c\n",
		       params->comment_character);
		return -ENOTSUP;
	}

	return 0;
}

struct rte_cfgfile *
rte_cfgfile_load(const char *filename, int flags)
{
	return rte_cfgfile_load_with_params(filename, flags,
					    &default_cfgfile_params);
}

struct rte_cfgfile *
rte_cfgfile_load_with_params(const char *filename, int flags,
			     const struct rte_cfgfile_parameters *params)
{
	char buffer[CFG_NAME_LEN + CFG_VALUE_LEN + 4];
	int lineno = 0;
	struct rte_cfgfile *cfg;

	if (rte_cfgfile_check_params(params))
		return NULL;

	FILE *f = fopen(filename, "r");
	if (f == NULL)
		return NULL;

	cfg = rte_cfgfile_create(flags);

	while (fgets(buffer, sizeof(buffer), f) != NULL) {
		char *pos;
		size_t len = strnlen(buffer, sizeof(buffer));
		lineno++;
		if ((len >= sizeof(buffer) - 1) && (buffer[len-1] != '\n')) {
			CFG_LOG(ERR, " line %d - no \\n found on string. "
					"Check if line too long\n", lineno);
			goto error1;
		}
		/* skip parsing if comment character found */
		pos = memchr(buffer, params->comment_character, len);
		if (pos != NULL &&
		    (pos == buffer || *(pos-1) != '\\')) {
			*pos = '\0';
			len = pos -  buffer;
		}

		len = _strip(buffer, len);
		/* skip lines without useful content */
		if (buffer[0] != '[' && memchr(buffer, '=', len) == NULL)
			continue;

		if (buffer[0] == '[') {
			/* section heading line */
			char *end = memchr(buffer, ']', len);
			if (end == NULL) {
				CFG_LOG(ERR,
					"line %d - no terminating ']' character found\n",
					lineno);
				goto error1;
			}
			*end = '\0';
			_strip(&buffer[1], end - &buffer[1]);

			rte_cfgfile_add_section(cfg, &buffer[1]);
		} else {
			/* key and value line */
			char *split[2] = {NULL};

			split[0] = buffer;
			split[1] = memchr(buffer, '=', len);
			if (split[1] == NULL) {
				CFG_LOG(ERR,
					"line %d - no '=' character found\n",
					lineno);
				goto error1;
			}
			*split[1] = '\0';
			split[1]++;

			_strip(split[0], strlen(split[0]));
			_strip(split[1], strlen(split[1]));
			char *end = memchr(split[1], '\\', strlen(split[1]));

			size_t split_len = strlen(split[1]) + 1;
			while (end != NULL) {
				if (*(end+1) == params->comment_character) {
					*end = '\0';
					strlcat(split[1], end+1, split_len);
				} else
					end++;
				end = memchr(end, '\\', strlen(end));
			}

			if (!(flags & CFG_FLAG_EMPTY_VALUES) &&
					(*split[1] == '\0')) {
				CFG_LOG(ERR,
					"line %d - cannot use empty values\n",
					lineno);
				goto error1;
			}

			if (cfg->num_sections == 0)
				goto error1;

			_add_entry(&cfg->sections[cfg->num_sections - 1],
					split[0], split[1]);
		}
	}
	fclose(f);
	return cfg;
error1:
	rte_cfgfile_close(cfg);
	fclose(f);
	return NULL;
}

struct rte_cfgfile *
rte_cfgfile_create(int flags)
{
	int i;
	struct rte_cfgfile *cfg;

	/* future proof flags usage */
	if (flags & ~(CFG_FLAG_GLOBAL_SECTION | CFG_FLAG_EMPTY_VALUES))
		return NULL;

	cfg = malloc(sizeof(*cfg));

	if (cfg == NULL)
		return NULL;

	cfg->flags = flags;
	cfg->num_sections = 0;

	/* allocate first batch of sections and entries */
	cfg->sections = calloc(CFG_ALLOC_SECTION_BATCH,
			       sizeof(struct rte_cfgfile_section));
	if (cfg->sections == NULL)
		goto error1;

	cfg->allocated_sections = CFG_ALLOC_SECTION_BATCH;

	for (i = 0; i < CFG_ALLOC_SECTION_BATCH; i++) {
		cfg->sections[i].entries = calloc(CFG_ALLOC_ENTRY_BATCH,
					  sizeof(struct rte_cfgfile_entry));

		if (cfg->sections[i].entries == NULL)
			goto error1;

		cfg->sections[i].num_entries = 0;
		cfg->sections[i].allocated_entries = CFG_ALLOC_ENTRY_BATCH;
	}

	if (flags & CFG_FLAG_GLOBAL_SECTION)
		rte_cfgfile_add_section(cfg, "GLOBAL");

	return cfg;
error1:
	if (cfg->sections != NULL) {
		for (i = 0; i < cfg->allocated_sections; i++) {
			if (cfg->sections[i].entries != NULL) {
				free(cfg->sections[i].entries);
				cfg->sections[i].entries = NULL;
			}
		}
		free(cfg->sections);
		cfg->sections = NULL;
	}
	free(cfg);
	return NULL;
}

int
rte_cfgfile_add_section(struct rte_cfgfile *cfg, const char *sectionname)
{
	int i;

	if (cfg == NULL)
		return -EINVAL;

	if (sectionname == NULL)
		return -EINVAL;

	/* resize overall struct if we don't have room for more	sections */
	if (cfg->num_sections == cfg->allocated_sections) {

		struct rte_cfgfile_section *n_sections =
				realloc(cfg->sections,
				sizeof(struct rte_cfgfile_section) *
				((cfg->allocated_sections) +
				CFG_ALLOC_SECTION_BATCH));

		if (n_sections == NULL)
			return -ENOMEM;

		for (i = 0; i < CFG_ALLOC_SECTION_BATCH; i++) {
			n_sections[i + cfg->allocated_sections].num_entries = 0;
			n_sections[i +
				 cfg->allocated_sections].allocated_entries = 0;
			n_sections[i + cfg->allocated_sections].entries = NULL;
		}
		cfg->sections = n_sections;
		cfg->allocated_sections += CFG_ALLOC_SECTION_BATCH;
	}

	strlcpy(cfg->sections[cfg->num_sections].name, sectionname,
		sizeof(cfg->sections[0].name));
	cfg->sections[cfg->num_sections].num_entries = 0;
	cfg->num_sections++;

	return 0;
}

int rte_cfgfile_add_entry(struct rte_cfgfile *cfg,
		const char *sectionname, const char *entryname,
		const char *entryvalue)
{
	int ret;

	if ((cfg == NULL) || (sectionname == NULL) || (entryname == NULL)
			|| (entryvalue == NULL))
		return -EINVAL;

	if (rte_cfgfile_has_entry(cfg, sectionname, entryname) != 0)
		return -EEXIST;

	/* search for section pointer by sectionname */
	struct rte_cfgfile_section *curr_section = _get_section(cfg,
								sectionname);
	if (curr_section == NULL)
		return -EINVAL;

	ret = _add_entry(curr_section, entryname, entryvalue);

	return ret;
}

int rte_cfgfile_set_entry(struct rte_cfgfile *cfg, const char *sectionname,
		const char *entryname, const char *entryvalue)
{
	int i;

	if ((cfg == NULL) || (sectionname == NULL) || (entryname == NULL))
		return -EINVAL;

	/* search for section pointer by sectionname */
	struct rte_cfgfile_section *curr_section = _get_section(cfg,
								sectionname);
	if (curr_section == NULL)
		return -EINVAL;

	if (entryvalue == NULL)
		entryvalue = "";

	for (i = 0; i < curr_section->num_entries; i++)
		if (!strcmp(curr_section->entries[i].name, entryname)) {
			strlcpy(curr_section->entries[i].value, entryvalue,
				sizeof(curr_section->entries[i].value));
			return 0;
		}

	CFG_LOG(ERR, "entry name doesn't exist\n");
	return -EINVAL;
}

int rte_cfgfile_save(struct rte_cfgfile *cfg, const char *filename)
{
	int i, j;

	if ((cfg == NULL) || (filename == NULL))
		return -EINVAL;

	FILE *f = fopen(filename, "w");

	if (f == NULL)
		return -EINVAL;

	for (i = 0; i < cfg->num_sections; i++) {
		fprintf(f, "[%s]\n", cfg->sections[i].name);

		for (j = 0; j < cfg->sections[i].num_entries; j++) {
			fprintf(f, "%s=%s\n",
					cfg->sections[i].entries[j].name,
					cfg->sections[i].entries[j].value);
		}
	}
	return fclose(f);
}

int rte_cfgfile_close(struct rte_cfgfile *cfg)
{
	int i;

	if (cfg == NULL)
		return -1;

	if (cfg->sections != NULL) {
		for (i = 0; i < cfg->allocated_sections; i++) {
			if (cfg->sections[i].entries != NULL) {
				free(cfg->sections[i].entries);
				cfg->sections[i].entries = NULL;
			}
		}
		free(cfg->sections);
		cfg->sections = NULL;
	}
	free(cfg);
	cfg = NULL;

	return 0;
}

int
rte_cfgfile_num_sections(struct rte_cfgfile *cfg, const char *sectionname,
size_t length)
{
	int i;
	int num_sections = 0;
	for (i = 0; i < cfg->num_sections; i++) {
		if (strncmp(cfg->sections[i].name, sectionname, length) == 0)
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
		strlcpy(sections[i], cfg->sections[i].name, CFG_NAME_LEN);

	return i;
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
rte_cfgfile_section_num_entries_by_index(struct rte_cfgfile *cfg,
	char *sectionname, int index)
{
	if (index < 0 || index >= cfg->num_sections)
		return -1;

	const struct rte_cfgfile_section *sect = &(cfg->sections[index]);

	strlcpy(sectionname, sect->name, CFG_NAME_LEN);
	return sect->num_entries;
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
		entries[i] = sect->entries[i];
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
	sect = &cfg->sections[index];
	strlcpy(sectionname, sect->name, CFG_NAME_LEN);
	for (i = 0; i < max_entries && i < sect->num_entries; i++)
		entries[i] = sect->entries[i];
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
		if (strncmp(sect->entries[i].name, entryname, CFG_NAME_LEN)
									== 0)
			return sect->entries[i].value;
	return NULL;
}

int
rte_cfgfile_has_entry(struct rte_cfgfile *cfg, const char *sectionname,
		const char *entryname)
{
	return rte_cfgfile_get_entry(cfg, sectionname, entryname) != NULL;
}
