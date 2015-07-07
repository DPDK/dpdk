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

#include <rte_common.h>
#include <rte_cycles.h>
#include <rte_pipeline.h>

#include "pipeline_common_be.h"
#include "app.h"

int app_thread(void *arg)
{
	struct app_params *app = (struct app_params *) arg;
	uint32_t core_id = rte_lcore_id(), i, j;
	struct app_thread_data *t = &app->thread_data[core_id];
	uint32_t n_regular = RTE_MIN(t->n_regular, RTE_DIM(t->regular));
	uint32_t n_custom = RTE_MIN(t->n_custom, RTE_DIM(t->custom));

	for (i = 0; ; i++) {
		/* Run regular pipelines */
		for (j = 0; j < n_regular; j++) {
			struct app_thread_pipeline_data *data = &t->regular[j];
			struct pipeline *p = data->be;

			rte_pipeline_run(p->p);
		}

		/* Run custom pipelines */
		for (j = 0; j < n_custom; j++) {
			struct app_thread_pipeline_data *data = &t->custom[j];

			data->f_run(data->be);
		}

		/* Timer */
		if ((i & 0xF) == 0) {
			uint64_t time = rte_get_tsc_cycles();
			uint64_t t_deadline = UINT64_MAX;

			if (time < t->deadline)
				continue;

			/* Timer for regular pipelines */
			for (j = 0; j < n_regular; j++) {
				struct app_thread_pipeline_data *data =
					&t->regular[j];
				uint64_t p_deadline = data->deadline;

				if (p_deadline <= time) {
					data->f_timer(data->be);
					p_deadline = time + data->timer_period;
					data->deadline = p_deadline;
				}

				if (p_deadline < t_deadline)
					t_deadline = p_deadline;
			}

			/* Timer for custom pipelines */
			for (j = 0; j < n_custom; j++) {
				struct app_thread_pipeline_data *data =
					&t->custom[j];
				uint64_t p_deadline = data->deadline;

				if (p_deadline <= time) {
					data->f_timer(data->be);
					p_deadline = time + data->timer_period;
					data->deadline = p_deadline;
				}

				if (p_deadline < t_deadline)
					t_deadline = p_deadline;
			}

			t->deadline = t_deadline;
		}
	}

	return 0;
}
