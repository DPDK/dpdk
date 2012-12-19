/*-
 *   BSD LICENSE
 * 
 *   Copyright(c) 2010-2012 Intel Corporation. All rights reserved.
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
 * 
 */
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/mman.h>
#include <sys/queue.h>
#include <limits.h>

#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_string_fns.h>


/* some functions used for printing out the memory segments and memory zones information */

#define PRINT_STR_FIELD(structname, field) do{ \
	count+= rte_snprintf(buf + count, len-count, " %s='%s',", \
			#field, (const char *)structname->field);\
} while(0)

#define PRINT_PTR_FIELD(structname, field) do{ \
	count+= rte_snprintf(buf + count, len-count, " %s=%p,", \
			#field, (void *)structname->field);\
} while(0)

#define PRINT_UINT_FIELD(structname, field) do{ \
	count+= rte_snprintf(buf + count, len-count, " %s=%llu,", \
			#field, (unsigned long long)structname->field);\
} while(0)

#define PRINT_INT_FIELD(structname, field) do{ \
	count+= rte_snprintf(buf + count, len-count, " %s=%lld,", \
			#field, (long long)structname->field);\
} while(0)

#define PRINT_CUSTOM_FIELD(structname, field, print_fn) do{ \
	char buf2[1024]; \
	count+= rte_snprintf(buf + count, len-count, " %s=%s,", \
			#field, print_fn(structname->field, buf2, sizeof(buf2)));\
} while(0)

static inline const char *
memseg_to_str(const struct rte_memseg *seg, char *buf, size_t len)
{
	int count = 0;
	count += rte_snprintf(buf + count, len - count, "{");
	PRINT_UINT_FIELD(seg, phys_addr);
	PRINT_PTR_FIELD(seg, addr);
	PRINT_UINT_FIELD(seg, len);
	PRINT_INT_FIELD(seg, socket_id);
	PRINT_UINT_FIELD(seg, hugepage_sz);
	PRINT_UINT_FIELD(seg, nchannel);
	PRINT_UINT_FIELD(seg, nrank);
	rte_snprintf(buf + count - 1, len - count + 1, " }");
	return buf;
}

static inline const char *
memzone_to_str(const struct rte_memzone *zone, char *buf, size_t len)
{
	int count = 0;
	count += rte_snprintf(buf + count, len - count, "{");
	PRINT_STR_FIELD(zone, name);
	PRINT_UINT_FIELD(zone, phys_addr);
	PRINT_PTR_FIELD(zone, addr);
	PRINT_UINT_FIELD(zone, len);
	PRINT_INT_FIELD(zone, socket_id);
	PRINT_UINT_FIELD(zone, flags);
	rte_snprintf(buf + count - 1, len - count + 1, " }");
	return buf;
}

static inline const char *
tailq_to_str(const struct rte_tailq_head *tailq, char *buf, size_t len)
{
	int count = 0;
	count += rte_snprintf(buf + count, len - count, "{");
	PRINT_STR_FIELD(tailq, qname);
	const struct rte_dummy_head *head = &tailq->tailq_head;
	PRINT_PTR_FIELD(head, tqh_first);
	PRINT_PTR_FIELD(head, tqh_last);
	rte_snprintf(buf + count - 1, len - count + 1, " }");
	return buf;
}

#define PREFIX    "prefix"
static const char *directory = "/var/run";
static const char *pre = "rte";

static void
usage(const char *prgname)
{
	printf("%s --prefix <prefix>\n\n"
			"dump_config option list:\n"
			"\t--"PREFIX": filename prefix\n",
			prgname);
}

static int
dmp_cfg_parse_args(int argc, char **argv)
{
	const char *prgname = argv[0];
	const char *home_dir = getenv("HOME");
	int opt;
	int option_index;
	static struct option lgopts[] = {
			{PREFIX, 1, 0, 0},
			{0, 0, 0, 0}
	};

	if (getuid() != 0 && home_dir != NULL)
		directory = home_dir;

	while ((opt = getopt_long(argc, argv, "",
			lgopts, &option_index)) != EOF) {
		switch (opt) {
		case 0:
			if (!strcmp(lgopts[option_index].name, PREFIX))
				pre = optarg;
			else{
				usage(prgname);
				return -1;
			}
			break;

		default:
			usage(prgname);
			return -1;
		}
	}
	return 0;
}

int
main(int argc, char **argv)
{
	char buffer[1024];
	char path[PATH_MAX];
	int i;
	int fd = 0;

	dmp_cfg_parse_args(argc, argv);
	rte_snprintf(path, sizeof(path), "%s/.%s_config",  directory, pre);
	printf("Path to mem_config: %s\n\n", path);

	fd = open(path, O_RDWR);
	if (fd < 0){
		printf("Error with config open\n");
		return 1;
	}
	struct rte_mem_config *cfg = mmap(NULL, sizeof(*cfg), PROT_READ, \
			MAP_SHARED, fd, 0);
	if (cfg == NULL){
		printf("Error with config mmap\n");
		close(fd);
	return 1;
	}
	close(fd);

	printf("----------- MEMORY_SEGMENTS -------------\n");
	for (i = 0; i < RTE_MAX_MEMSEG; i++){
		if (cfg->memseg[i].addr == NULL) break;
		printf("Segment %d: ", i);
		printf("%s\n", memseg_to_str(&cfg->memseg[i], buffer, sizeof(buffer)));
	}
	printf("--------- END_MEMORY_SEGMENTS -----------\n");

	printf("------------ MEMORY_ZONES ---------------\n");
	for (i = 0; i < RTE_MAX_MEMZONE; i++){
		if (cfg->memzone[i].addr == NULL) break;
		printf("Zone %d: ", i);
		printf("%s\n", memzone_to_str(&cfg->memzone[i], buffer, sizeof(buffer)));

	}
	printf("---------- END_MEMORY_ZONES -------------\n");

	printf("------------- TAIL_QUEUES ---------------\n");
	for (i = 0; i < RTE_MAX_TAILQ; i++){
		if (cfg->tailq_head[i].qname[0] == '\0') break;
		printf("Tailq %d: ", i);
		printf("%s\n", tailq_to_str(&cfg->tailq_head[i], buffer, sizeof(buffer)));

	}
	printf("----------- END_TAIL_QUEUES -------------\n");

	return 0;
}

