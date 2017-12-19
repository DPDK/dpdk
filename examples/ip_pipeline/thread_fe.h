/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2015 Intel Corporation
 */

#ifndef THREAD_FE_H_
#define THREAD_FE_H_

static inline struct rte_ring *
app_thread_msgq_in_get(struct app_params *app,
		uint32_t socket_id, uint32_t core_id, uint32_t ht_id)
{
	char msgq_name[32];
	ssize_t param_idx;

	snprintf(msgq_name, sizeof(msgq_name),
		"MSGQ-REQ-CORE-s%" PRIu32 "c%" PRIu32 "%s",
		socket_id,
		core_id,
		(ht_id) ? "h" : "");
	param_idx = APP_PARAM_FIND(app->msgq_params, msgq_name);

	if (param_idx < 0)
		return NULL;

	return app->msgq[param_idx];
}

static inline struct rte_ring *
app_thread_msgq_out_get(struct app_params *app,
		uint32_t socket_id, uint32_t core_id, uint32_t ht_id)
{
	char msgq_name[32];
	ssize_t param_idx;

	snprintf(msgq_name, sizeof(msgq_name),
		"MSGQ-RSP-CORE-s%" PRIu32 "c%" PRIu32 "%s",
		socket_id,
		core_id,
		(ht_id) ? "h" : "");
	param_idx = APP_PARAM_FIND(app->msgq_params, msgq_name);

	if (param_idx < 0)
		return NULL;

	return app->msgq[param_idx];

}

int
app_pipeline_thread_cmd_push(struct app_params *app);

int
app_pipeline_enable(struct app_params *app,
		uint32_t core_id,
		uint32_t socket_id,
		uint32_t hyper_th_id,
		uint32_t pipeline_id);

int
app_pipeline_disable(struct app_params *app,
		uint32_t core_id,
		uint32_t socket_id,
		uint32_t hyper_th_id,
		uint32_t pipeline_id);

int
app_thread_headroom(struct app_params *app,
		uint32_t core_id,
		uint32_t socket_id,
		uint32_t hyper_th_id);

#endif /* THREAD_FE_H_ */
