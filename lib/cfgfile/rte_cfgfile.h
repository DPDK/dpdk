/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_CFGFILE_H__
#define __INCLUDE_RTE_CFGFILE_H__

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * Configuration File management.
 *
 * This library allows reading application defined parameters
 * from standard format configuration file.
 */

#ifndef CFG_NAME_LEN
#define CFG_NAME_LEN 64
#endif

#ifndef CFG_VALUE_LEN
#define CFG_VALUE_LEN 256
#endif

/** Configuration file */
struct rte_cfgfile;

/** Configuration file entry */
struct rte_cfgfile_entry {
	char name[CFG_NAME_LEN]; /**< Name */
	char value[CFG_VALUE_LEN]; /**< Value */
};

/** Configuration file operation optional arguments */
struct rte_cfgfile_parameters {
	/** Config file comment character; one of '!', '#', '%', ';', '@' */
	char comment_character;
};

/**@{ cfgfile load operation flags */
enum {
	/**
	 * Indicates that the file supports key value entries before the first
	 * defined section.  These entries can be accessed in the "GLOBAL"
	 * section.
	 */
	CFG_FLAG_GLOBAL_SECTION = 1,

	/**
	 * Indicates that file supports key value entries where the value can
	 * be zero length (e.g., "key=").
	 */
	CFG_FLAG_EMPTY_VALUES = 2,
};
/**@} */

/** Defines the default comment character used for parsing config files. */
#define CFG_DEFAULT_COMMENT_CHARACTER ';'

/**
 * Open config file.
 *
 * @param filename
 *   Config file name.
 * @param flags
 *   Config file flags.
 * @return
 *   Handle to configuration file on success, NULL otherwise.
 */
struct rte_cfgfile *rte_cfgfile_load(const char *filename, int flags);

/**
 * Open config file with specified optional parameters.
 *
 * @param filename
 *   Config file name
 * @param flags
 *   Config file flags
 * @param params
 *   Additional configuration attributes.  Must be configured with desired
 *   values prior to invoking this API.
 * @return
 *   Handle to configuration file on success, NULL otherwise
 */
struct rte_cfgfile *rte_cfgfile_load_with_params(const char *filename,
	int flags, const struct rte_cfgfile_parameters *params);

/**
 * Create new cfgfile instance with empty sections and entries
 *
 * @param flags
 *   - CFG_FLAG_GLOBAL_SECTION
 *     Indicates that the file supports key value entries before the first
 *     defined section.  These entries can be accessed in the "GLOBAL"
 *     section.
 *   - CFG_FLAG_EMPTY_VALUES
 *     Indicates that file supports key value entries where the value can
 *     be zero length (e.g., "key=").
 * @return
 *   Handle to cfgfile instance on success, NULL otherwise
 */
struct rte_cfgfile *rte_cfgfile_create(int flags);

/**
 * Add section in cfgfile instance.
 *
 * @param cfg
 *   Pointer to the cfgfile structure.
 * @param sectionname
 *   Section name which will be add to cfgfile.
 * @return
 *   0 on success, -ENOMEM if can't add section
 */
int
rte_cfgfile_add_section(struct rte_cfgfile *cfg, const char *sectionname);

/**
 * Add entry to specified section in cfgfile instance.
 *
 * @param cfg
 *   Pointer to the cfgfile structure.
 * @param sectionname
 *   Given section name to add an entry.
 * @param entryname
 *   Entry name to add.
 * @param entryvalue
 *   Entry value to add.
 * @return
 *   0 on success, -EEXIST if entry already exist, -EINVAL if bad argument
 */
int rte_cfgfile_add_entry(struct rte_cfgfile *cfg,
		const char *sectionname, const char *entryname,
		const char *entryvalue);

/**
 * Update value of specified entry name in given section in config file
 *
 * @param cfg
 *   Config file
 * @param sectionname
 *   Section name
 * @param entryname
 *   Entry name to look for the value change
 * @param entryvalue
 *   New entry value. Can be also an empty string if CFG_FLAG_EMPTY_VALUES = 1
 * @return
 *   0 on success, -EINVAL if bad argument
 */
int rte_cfgfile_set_entry(struct rte_cfgfile *cfg, const char *sectionname,
		const char *entryname, const char *entryvalue);

/**
 * Save object cfgfile to file on disc
 *
 * @param cfg
 *   Config file structure
 * @param filename
 *   File name to save data
 * @return
 *   0 on success, errno otherwise
 */
int rte_cfgfile_save(struct rte_cfgfile *cfg, const char *filename);

/**
 * Get number of sections in config file.
 *
 * @param cfg
 *   Config file.
 * @param sec_name
 *   Section name.
 * @param length
 *   Maximum section name length.
 * @return
 *   Number of sections.
 */
int rte_cfgfile_num_sections(struct rte_cfgfile *cfg, const char *sec_name,
	size_t length);

/**
 * Get name of all config file sections.
 *
 * Fills in the array sections with the name of all the sections in the file
 * (up to the number of max_sections sections).
 *
 * @param cfg
 *   Config file.
 * @param sections
 *   Array containing section names after successful invocation.
 *   Each element of this array should be preallocated by the user
 *   with at least CFG_NAME_LEN characters.
 * @param max_sections
 *   Maximum number of section names to be stored in sections array.
 * @return
 *   Number of populated sections names.
 */
int rte_cfgfile_sections(struct rte_cfgfile *cfg, char *sections[],
	int max_sections);

/**
 * Check if given section exists in config file.
 *
 * @param cfg
 *   Config file.
 * @param sectionname
 *   Section name.
 * @return
 *   TRUE (value different than 0) if section exists, FALSE (value 0) otherwise.
 */
int rte_cfgfile_has_section(struct rte_cfgfile *cfg, const char *sectionname);

/**
 * Get number of entries in given config file section.
 *
 * If multiple sections have the given name,
 * this function operates on the first one.
 *
 * @param cfg
 *   Config file.
 * @param sectionname
 *   Section name.
 * @return
 *   Number of entries in section on success, -1 otherwise.
 */
int rte_cfgfile_section_num_entries(struct rte_cfgfile *cfg,
	const char *sectionname);

/**
 * Get number of entries in given config file section.
 *
 * The index of a section is the same as the index of its name
 * in the result of rte_cfgfile_sections.
 * This API can be used when there are multiple sections with the same name.
 *
 * @param cfg
 *   Config file.
 * @param sectionname
 *   Section name.
 * @param index
 *   Section index.
 * @return
 *   Number of entries in section on success, -1 otherwise.
 */
int rte_cfgfile_section_num_entries_by_index(struct rte_cfgfile *cfg,
	char *sectionname,
	int index);

/**
 * Get section entries as key-value pairs.
 *
 * If multiple sections have the given name,
 * this function operates on the first one.
 *
 * @param cfg
 *   Config file.
 * @param sectionname
 *   Section name.
 * @param entries
 *   Pre-allocated array of at least max_entries entries where the section
 *   entries are stored as key-value pair after successful invocation.
 * @param max_entries
 *   Maximum number of section entries to be stored in entries array.
 * @return
 *   Number of entries populated on success, -1 otherwise.
 */
int rte_cfgfile_section_entries(struct rte_cfgfile *cfg,
	const char *sectionname,
	struct rte_cfgfile_entry *entries,
	int max_entries);

/**
 * Get section entries as key-value pairs.
 *
 * The index of a section is the same as the index of its name
 * in the result of rte_cfgfile_sections.
 * This API can be used when there are multiple sections with the same name.
 *
 * @param cfg
 *   Config file.
 * @param index
 *   Section index.
 * @param sectionname
 *   Pre-allocated string of at least CFG_NAME_LEN characters
 *   where the section name is stored after successful invocation.
 * @param entries
 *   Pre-allocated array of at least max_entries entries where the section
 *   entries are stored as key-value pair after successful invocation.
 * @param max_entries
 *   Maximum number of section entries to be stored in entries array.
 * @return
 *   Number of entries populated on success, -1 otherwise.
 */
int rte_cfgfile_section_entries_by_index(struct rte_cfgfile *cfg,
	int index,
	char *sectionname,
	struct rte_cfgfile_entry *entries,
	int max_entries);

/**
 * Get value of the named entry in named config file section.
 *
 * If multiple sections have the given name,
 * this function operates on the first one.
 *
 * @param cfg
 *   Config file.
 * @param sectionname
 *   Section name.
 * @param entryname
 *   Entry name.
 * @return
 *   Entry value on success, NULL otherwise.
 */
const char *rte_cfgfile_get_entry(struct rte_cfgfile *cfg,
	const char *sectionname,
	const char *entryname);

/**
 * Check if given entry exists in named config file section.
 *
 * If multiple sections have the given name,
 * this function operates on the first one.
 *
 * @param cfg
 *   Config file.
 * @param sectionname
 *   Section name.
 * @param entryname
 *   Entry name.
 * @return
 *   TRUE (value different than 0) if entry exists, FALSE (value 0) otherwise.
 */
int rte_cfgfile_has_entry(struct rte_cfgfile *cfg, const char *sectionname,
	const char *entryname);

/**
 * Close config file.
 *
 * @param cfg
 *   Config file.
 * @return
 *   0 on success, -1 otherwise.
 */
int rte_cfgfile_close(struct rte_cfgfile *cfg);

#ifdef __cplusplus
}
#endif

#endif
