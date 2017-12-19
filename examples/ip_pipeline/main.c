/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#include "app.h"

static struct app_params app;

int
main(int argc, char **argv)
{
	rte_openlog_stream(stderr);

	/* Config */
	app_config_init(&app);

	app_config_args(&app, argc, argv);

	app_config_preproc(&app);

	app_config_parse(&app, app.parser_file);

	app_config_check(&app);

	/* Init */
	app_init(&app);

	/* Run-time */
	rte_eal_mp_remote_launch(
		app_thread,
		(void *) &app,
		CALL_MASTER);

	return 0;
}
