/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2015 Intel Corporation. All rights reserved.
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

#include <fcntl.h>
#include <unistd.h>

#include <rte_common.h>
#include <rte_malloc.h>

#include <cmdline_parse.h>
#include <cmdline_parse_string.h>
#include <cmdline_socket.h>
#include <cmdline.h>

#include "app.h"
#include "pipeline_master_be.h"

struct pipeline_master {
	struct app_params *app;
	struct cmdline *cl;
	int script_file_done;
} __rte_cache_aligned;

static void*
pipeline_init(__rte_unused struct pipeline_params *params, void *arg)
{
	struct app_params *app = (struct app_params *) arg;
	struct pipeline_master *p;
	uint32_t size;

	/* Check input arguments */
	if (app == NULL)
		return NULL;

	/* Memory allocation */
	size = RTE_CACHE_LINE_ROUNDUP(sizeof(struct pipeline_master));
	p = rte_zmalloc(NULL, size, RTE_CACHE_LINE_SIZE);
	if (p == NULL)
		return NULL;

	/* Initialization */
	p->app = app;

	p->cl = cmdline_stdin_new(app->cmds, "pipeline> ");
	if (p->cl == NULL) {
		rte_free(p);
		return NULL;
	}

	p->script_file_done = 0;
	if (app->script_file == NULL)
		p->script_file_done = 1;

	return (void *) p;
}

static int
pipeline_free(void *pipeline)
{
	struct pipeline_master *p = (struct pipeline_master *) pipeline;

	if (p == NULL)
		return -EINVAL;

	cmdline_stdin_exit(p->cl);
	rte_free(p);

	return 0;
}

static int
pipeline_run(void *pipeline)
{
	struct pipeline_master *p = (struct pipeline_master *) pipeline;
	int status;

	if (p->script_file_done == 0) {
		struct app_params *app = p->app;
		int fd = open(app->script_file, O_RDONLY);

		if (fd < 0)
			printf("Cannot open CLI script file \"%s\"\n",
				app->script_file);
		else {
			struct cmdline *file_cl;

			printf("Running CLI script file \"%s\" ...\n",
				app->script_file);
			file_cl = cmdline_new(p->cl->ctx, "", fd, 1);
			cmdline_interact(file_cl);
			close(fd);
		}

		p->script_file_done = 1;
	}

	status = cmdline_poll(p->cl);
	if (status < 0)
		rte_panic("CLI poll error (%" PRId32 ")\n", status);
	else if (status == RDLINE_EXITED) {
		cmdline_stdin_exit(p->cl);
		rte_exit(0, "Bye!\n");
	}

	return 0;
}

static int
pipeline_timer(__rte_unused void *pipeline)
{
	return 0;
}

struct pipeline_be_ops pipeline_master_be_ops = {
		.f_init = pipeline_init,
		.f_free = pipeline_free,
		.f_run = pipeline_run,
		.f_timer = pipeline_timer,
		.f_track = NULL,
};
