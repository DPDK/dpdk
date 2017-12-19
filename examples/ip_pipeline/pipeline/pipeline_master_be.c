/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
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
	int post_init_done;
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

	p->post_init_done = 0;
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
	struct app_params *app = p->app;
	int status;
#ifdef RTE_LIBRTE_KNI
	uint32_t i;
#endif /* RTE_LIBRTE_KNI */

	/* Application post-init phase */
	if (p->post_init_done == 0) {
		app_post_init(app);

		p->post_init_done = 1;
	}

	/* Run startup script file */
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

	/* Command Line Interface (CLI) */
	status = cmdline_poll(p->cl);
	if (status < 0)
		rte_panic("CLI poll error (%" PRId32 ")\n", status);
	else if (status == RDLINE_EXITED) {
		cmdline_stdin_exit(p->cl);
		rte_exit(0, "Bye!\n");
	}

#ifdef RTE_LIBRTE_KNI
	/* Handle KNI requests from Linux kernel */
	for (i = 0; i < app->n_pktq_kni; i++)
		rte_kni_handle_request(app->kni[i]);
#endif /* RTE_LIBRTE_KNI */

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
};
