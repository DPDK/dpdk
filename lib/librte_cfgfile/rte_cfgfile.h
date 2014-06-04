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

#ifndef __INCLUDE_RTE_CFGFILE_H__
#define __INCLUDE_RTE_CFGFILE_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
* @file
* RTE Configuration File
*
* This library allows reading application defined parameters from standard
* format configuration file.
*
***/

#define CFG_NAME_LEN 32
#define CFG_VALUE_LEN 64

/** Configuration file */
struct rte_cfgfile;

/** Configuration file entry */
struct rte_cfgfile_entry {
	char name[CFG_NAME_LEN]; /**< Name */
	char value[CFG_VALUE_LEN]; /**< Value */
};

/**
* Open config file
*
* @param filename
*   Config file name
* @param flags
*   Config file flags, Reserved for future use. Must be set to 0.
* @return
*   Handle to configuration file
*/
struct rte_cfgfile *rte_cfgfile_load(const char *filename, int flags);

/**
* Get number of sections in config file
*
* @param cfg
*   Config file
* @param sec_name
*   Section name
* @param length
*   Maximum section name length
* @return
*   0 on success, error code otherwise
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
*   Config file
* @param sections
*   Array containing section names after successful invocation. Each elemen
*   of this array should be preallocated by the user with at least
*   CFG_NAME_LEN characters.
* @param max_sections
*   Maximum number of section names to be stored in sections array
* @return
*   0 on success, error code otherwise
*/
int rte_cfgfile_sections(struct rte_cfgfile *cfg, char *sections[],
	int max_sections);

/**
* Check if given section exists in config file
*
* @param cfg
*   Config file
* @param sectionname
*   Section name
* @return
*   TRUE (value different than 0) if section exists, FALSE (value 0) otherwise
*/
int rte_cfgfile_has_section(struct rte_cfgfile *cfg, const char *sectionname);

/**
* Get number of entries in given config file section
*
* @param cfg
*   Config file
* @param sectionname
*   Section name
* @return
*   Number of entries in section
*/
int rte_cfgfile_section_num_entries(struct rte_cfgfile *cfg,
	const char *sectionname);

/** Get section entries as key-value pairs
*
* @param cfg
*   Config file
* @param sectionname
*   Section name
* @param entries
*   Pre-allocated array of at least max_entries entries where the section
*   entries are stored as key-value pair after successful invocation
* @param max_entries
*   Maximum number of section entries to be stored in entries array
* @return
*   0 on success, error code otherwise
*/
int rte_cfgfile_section_entries(struct rte_cfgfile *cfg,
	const char *sectionname,
	struct rte_cfgfile_entry *entries,
	int max_entries);

/** Get value of the named entry in named config file section
*
* @param cfg
*   Config file
* @param sectionname
*   Section name
* @param entryname
*   Entry name
* @return
*   Entry value
*/
const char *rte_cfgfile_get_entry(struct rte_cfgfile *cfg,
	const char *sectionname,
	const char *entryname);

/** Check if given entry exists in named config file section
*
* @param cfg
*   Config file
* @param sectionname
*   Section name
* @param entryname
*   Entry name
* @return
*   TRUE (value different than 0) if entry exists, FALSE (value 0) otherwise
*/
int rte_cfgfile_has_entry(struct rte_cfgfile *cfg, const char *sectionname,
	const char *entryname);

/** Close config file
*
* @param cfg
*   Config file
* @return
*   0 on success, error code otherwise
*/
int rte_cfgfile_close(struct rte_cfgfile *cfg);

#ifdef __cplusplus
}
#endif

#endif
