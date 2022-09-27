/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <stdlib.h>
#include <string.h>

#include <rte_common.h>
#include <rte_string_fns.h>

#include "rte_eth_softnic_internals.h"

int
softnic_pipeline_init(struct pmd_internals *p)
{
	TAILQ_INIT(&p->pipeline_list);

	return 0;
}

void
softnic_pipeline_free(struct pmd_internals *p)
{
	for ( ; ; ) {
		struct pipeline *pipeline;

		pipeline = TAILQ_FIRST(&p->pipeline_list);
		if (pipeline == NULL)
			break;

		TAILQ_REMOVE(&p->pipeline_list, pipeline, node);
		rte_swx_ctl_pipeline_free(pipeline->ctl);
		rte_swx_pipeline_free(pipeline->p);
		free(pipeline);
	}
}

void
softnic_pipeline_disable_all(struct pmd_internals *p)
{
	struct pipeline *pipeline;

	TAILQ_FOREACH(pipeline, &p->pipeline_list, node)
		if (pipeline->enabled)
			softnic_thread_pipeline_disable(p,
				pipeline->thread_id,
				pipeline);
}

uint32_t
softnic_pipeline_thread_count(struct pmd_internals *p, uint32_t thread_id)
{
	struct pipeline *pipeline;
	uint32_t count = 0;

	TAILQ_FOREACH(pipeline, &p->pipeline_list, node)
		if ((pipeline->enabled) && (pipeline->thread_id == thread_id))
			count++;

	return count;
}

struct pipeline *
softnic_pipeline_find(struct pmd_internals *p,
	const char *name)
{
	struct pipeline *pipeline;

	if (name == NULL)
		return NULL;

	TAILQ_FOREACH(pipeline, &p->pipeline_list, node)
		if (strcmp(name, pipeline->name) == 0)
			return pipeline;

	return NULL;
}

#ifndef MAX_LINE_LENGTH
#define MAX_LINE_LENGTH 2048
#endif

/* The Soft NIC device internal resources such as mempools, rings or pipelines are globally visible,
 * hence they need to have globally unique names. In order to apply the same configuration scripts
 * unmodified to all the Soft NIC devices that instantiate the same program, the pipeline I/O
 * configuration files are silently translated internally to prefix the name of the above resources
 * with the Soft NIC device name, thus making the resource names globally unique.
 */
static int
iospec_translate(struct pmd_internals *softnic __rte_unused,
		 const char *file_in_name,
		 const char *file_out_name)
{
	FILE *fi = NULL, *fo = NULL;
	char *line = NULL;
	int status = 0;

	/* File open. */
	fi = fopen(file_in_name, "r");
	fo = fopen(file_out_name, "w");
	if (!fi || !fo) {
		status = -EIO;
		goto free;
	}

	/* Memory allocation. */
	line = malloc(MAX_LINE_LENGTH);
	if (!line) {
		status = -ENOMEM;
		goto free;
	}

	/* Read from the input file and write to the output file. */
	for ( ; ; ) {
		char *ptr = line;
		uint32_t n_tokens;
		int flag = 0;

		/* Read next line. */
		if (!fgets(line, MAX_LINE_LENGTH, fi))
			break;

		/* Parse the line into tokens. */
		for (n_tokens = 0; ; n_tokens++) {
			char *token;

			/* Read token. */
			token = strtok_r(ptr, " \f\n\r\t\v", &ptr);
			if (!token)
				break;

			/* Handle comments. */
			if (!n_tokens &&
			    ((token[0] == '#') ||
			     (token[0] == ';') ||
			     ((token[0] == '/') && (token[1] == '/'))))
				break;

			/* Process token. */
			if (flag) {
				fprintf(fo, "%s_%s ", softnic->params.name, token);
				flag = 0;
				continue;
			}

			if (!strcmp(token, "mempool") ||
			    !strcmp(token, "ring")) {
				flag = 1;
				fprintf(fo, "%s ", token);
				continue;
			}

			/* Default action: write token. */
			fprintf(fo, "%s ", token);
		}

		/* Handle empty or comment lines. */
		if (!n_tokens)
			continue;

		/* Write newline. */
		fprintf(fo, "\n");
	}

free:
	/* Memory free. */
	free(line);

	/* File close. */
	if (fi)
		fclose(fi);
	if (fo)
		fclose(fo);
	return status;
}

struct pipeline *
softnic_pipeline_create(struct pmd_internals *softnic,
	const char *name,
	const char *lib_file_name,
	const char *iospec_file_name,
	int numa_node)
{
	char global_name[NAME_MAX];
	FILE *iospec_file = NULL;
	struct pipeline *pipeline = NULL;
	struct rte_swx_pipeline *p = NULL;
	struct rte_swx_ctl_pipeline *ctl = NULL;
	int status = 0;

	/* Check input params */
	if (!name || !name[0] || softnic_pipeline_find(softnic, name))
		goto error;

	/* Resource create */
	snprintf(global_name, sizeof(global_name), "/tmp/%s_%s.io", softnic->params.name, name);

	status = iospec_translate(softnic, iospec_file_name, global_name);
	if (status)
		goto error;

	iospec_file = fopen(global_name, "r");
	if (!iospec_file)
		goto error;

	snprintf(global_name, sizeof(global_name), "%s_%s", softnic->params.name, name);

	status = rte_swx_pipeline_build_from_lib(&p,
						 global_name,
						 lib_file_name,
						 iospec_file,
						 numa_node);
	if (status)
		goto error;

	fclose(iospec_file);
	iospec_file = NULL;

	ctl = rte_swx_ctl_pipeline_create(p);
	if (!ctl)
		goto error;

	/* Node allocation */
	pipeline = calloc(1, sizeof(struct pipeline));
	if (!pipeline)
		goto error;

	/* Node fill in */
	strlcpy(pipeline->name, name, sizeof(pipeline->name));
	pipeline->p = p;
	pipeline->ctl = ctl;

	/* Node add to list */
	TAILQ_INSERT_TAIL(&softnic->pipeline_list, pipeline, node);

	return pipeline;

error:
	free(pipeline);
	rte_swx_ctl_pipeline_free(ctl);
	rte_swx_pipeline_free(p);
	if (iospec_file)
		fclose(iospec_file);
	return NULL;
}
