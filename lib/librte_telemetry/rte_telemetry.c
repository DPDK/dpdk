/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <unistd.h>
#include <pthread.h>

#include <rte_eal.h>
#include <rte_ethdev.h>
#include <rte_metrics.h>
#include <rte_option.h>

#include "rte_telemetry.h"
#include "rte_telemetry_internal.h"

#define SLEEP_TIME 10

static telemetry_impl *static_telemetry;

static int32_t
rte_telemetry_run(void *userdata)
{
	struct telemetry_impl *telemetry = userdata;

	if (telemetry == NULL) {
		TELEMETRY_LOG_WARN("TELEMETRY could not be initialised");
		return -1;
	}

	return 0;
}

static void
*rte_telemetry_run_thread_func(void *userdata)
{
	int ret;
	struct telemetry_impl *telemetry = userdata;

	if (telemetry == NULL) {
		TELEMETRY_LOG_ERR("%s passed a NULL instance", __func__);
		pthread_exit(0);
	}

	while (telemetry->thread_status) {
		rte_telemetry_run(telemetry);
		ret = usleep(SLEEP_TIME);
		if (ret < 0)
			TELEMETRY_LOG_ERR("Calling thread could not be put to sleep");
	}
	pthread_exit(0);
}

int32_t __rte_experimental
rte_telemetry_init()
{
	int ret;
	pthread_attr_t attr;
	const char *telemetry_ctrl_thread = "telemetry";

	if (static_telemetry) {
		TELEMETRY_LOG_WARN("TELEMETRY structure already initialised");
		return -EALREADY;
	}

	static_telemetry = calloc(1, sizeof(struct telemetry_impl));
	if (static_telemetry == NULL) {
		TELEMETRY_LOG_ERR("Memory could not be allocated");
		return -ENOMEM;
	}

	static_telemetry->socket_id = rte_socket_id();
	rte_metrics_init(static_telemetry->socket_id);

	ret = pthread_attr_init(&attr);
	if (ret != 0) {
		TELEMETRY_LOG_ERR("Pthread attribute init failed");
		return -EPERM;
	}

	ret = rte_ctrl_thread_create(&static_telemetry->thread_id,
		telemetry_ctrl_thread, &attr, rte_telemetry_run_thread_func,
		(void *)static_telemetry);
	static_telemetry->thread_status = 1;

	if (ret < 0) {
		ret = rte_telemetry_cleanup();
		if (ret < 0)
			TELEMETRY_LOG_ERR("TELEMETRY cleanup failed");
		return -EPERM;
	}

	return 0;
}

int32_t __rte_experimental
rte_telemetry_cleanup(void)
{
	struct telemetry_impl *telemetry = static_telemetry;
	telemetry->thread_status = 0;
	pthread_join(telemetry->thread_id, NULL);
	free(telemetry);
	static_telemetry = NULL;
	return 0;
}

int telemetry_log_level;

static struct rte_option option = {
	.opt_str = "--telemetry",
	.cb = &rte_telemetry_init,
	.enabled = 0
};

RTE_INIT(rte_telemetry_register)
{
	telemetry_log_level = rte_log_register("lib.telemetry");
	if (telemetry_log_level >= 0)
		rte_log_set_level(telemetry_log_level, RTE_LOG_ERR);

	rte_option_register(&option);
}
