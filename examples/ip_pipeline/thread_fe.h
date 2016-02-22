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
