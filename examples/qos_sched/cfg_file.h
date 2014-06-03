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

#ifndef __CFG_FILE_H__
#define __CFG_FILE_H__

#include <rte_sched.h>

#define CFG_NAME_LEN 32
#define CFG_VALUE_LEN 64

struct cfg_entry {
	char name[CFG_NAME_LEN];
	char value[CFG_VALUE_LEN];
};

struct cfg_section {
	char name[CFG_NAME_LEN];
	int num_entries;
	struct cfg_entry *entries[0];
};

struct cfg_file {
	int flags;
	int num_sections;
	struct cfg_section *sections[0];
};


int cfg_load_port(struct cfg_file *cfg, struct rte_sched_port_params *port);

int cfg_load_pipe(struct cfg_file *cfg, struct rte_sched_pipe_params *pipe);

int cfg_load_subport(struct cfg_file *cfg, struct rte_sched_subport_params *subport);


/* reads a config file from disk and returns a handle to the config
 * 'flags' is reserved for future use and must be 0
 */
struct cfg_file *cfg_load(const char *filename, int flags);

/* returns the number of sections in the config */
int cfg_num_sections(struct cfg_file *cfg, const char *sec_name, size_t length);

/* fills the array "sections" with the names of all the sections in the file
 * (up to a max of max_sections).
 * NOTE: buffers in the sections array must be at least CFG_NAME_LEN big
 */
int cfg_sections(struct cfg_file *cfg, char *sections[], int max_sections);

/* true if the named section exists, false otherwise */
int cfg_has_section(struct cfg_file *cfg, const char *sectionname);

/* returns the number of entries in a section */
int cfg_section_num_entries(struct cfg_file *cfg, const char *sectionname);

/* returns the entries in a section as key-value pairs in the "entries" array */
int cfg_section_entries(struct cfg_file *cfg, const char *sectionname,
		struct cfg_entry *entries, int max_entries);

/* returns a pointer to the value of the named entry in the named section */
const char *cfg_get_entry(struct cfg_file *cfg, const char *sectionname,
		const char *entryname);

/* true if the given entry exists in the given section, false otherwise */
int cfg_has_entry(struct cfg_file *cfg, const char *sectionname,
		const char *entryname);

/* cleans up memory allocated by cfg_load() */
int cfg_close(struct cfg_file *cfg);

#endif
