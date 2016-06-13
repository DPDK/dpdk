#include <rte_common.h>
#include <rte_ring.h>
#include <rte_malloc.h>
#include <cmdline_rdline.h>
#include <cmdline_parse.h>
#include <cmdline_parse_num.h>
#include <cmdline_parse_string.h>

#include "thread.h"
#include "thread_fe.h"
#include "pipeline.h"
#include "pipeline_common_fe.h"
#include "app.h"

static inline void *
thread_msg_send_recv(struct app_params *app,
	uint32_t socket_id, uint32_t core_id, uint32_t ht_id,
	void *msg,
	uint32_t timeout_ms)
{
	struct rte_ring *r_req = app_thread_msgq_in_get(app,
		socket_id, core_id, ht_id);
	struct rte_ring *r_rsp = app_thread_msgq_out_get(app,
		socket_id, core_id, ht_id);
	uint64_t hz = rte_get_tsc_hz();
	void *msg_recv;
	uint64_t deadline;
	int status;

	/* send */
	do {
		status = rte_ring_sp_enqueue(r_req, (void *) msg);
	} while (status == -ENOBUFS);

	/* recv */
	deadline = (timeout_ms) ?
		(rte_rdtsc() + ((hz * timeout_ms) / 1000)) :
		UINT64_MAX;

	do {
		if (rte_rdtsc() > deadline)
			return NULL;

		status = rte_ring_sc_dequeue(r_rsp, &msg_recv);
	} while (status != 0);

	return msg_recv;
}

int
app_pipeline_enable(struct app_params *app,
		uint32_t socket_id,
		uint32_t core_id,
		uint32_t hyper_th_id,
		uint32_t pipeline_id)
{
	struct thread_pipeline_enable_msg_req *req;
	struct thread_pipeline_enable_msg_rsp *rsp;
	int thread_id;
	struct app_pipeline_data *p;
	struct app_pipeline_params *p_params;
	struct pipeline_type *p_type;
	int status;

	if (app == NULL)
		return -1;

	thread_id = cpu_core_map_get_lcore_id(app->core_map,
			socket_id,
			core_id,
			hyper_th_id);

	if ((thread_id < 0) ||
		((app->core_mask & (1LLU << thread_id)) == 0))
		return -1;

	if (app_pipeline_data(app, pipeline_id) == NULL)
		return -1;

	p = &app->pipeline_data[pipeline_id];
	p_params = &app->pipeline_params[pipeline_id];
	p_type = app_pipeline_type_find(app, p_params->type);

	if (p_type == NULL)
		return -1;

	if (p->enabled == 1)
		return -1;

	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = THREAD_MSG_REQ_PIPELINE_ENABLE;
	req->pipeline_id = pipeline_id;
	req->be = p->be;
	req->f_run = p_type->be_ops->f_run;
	req->f_timer = p_type->be_ops->f_timer;
	req->timer_period = p->timer_period;

	rsp = thread_msg_send_recv(app,
		socket_id, core_id, hyper_th_id, req, MSG_TIMEOUT_DEFAULT);
	if (rsp == NULL)
		return -1;

	status = rsp->status;
	app_msg_free(app, rsp);

	if (status != 0)
		return -1;

	p->enabled = 1;
	return 0;
}

int
app_pipeline_disable(struct app_params *app,
		uint32_t socket_id,
		uint32_t core_id,
		uint32_t hyper_th_id,
		uint32_t pipeline_id)
{
	struct thread_pipeline_disable_msg_req *req;
	struct thread_pipeline_disable_msg_rsp *rsp;
	int thread_id;
	struct app_pipeline_data *p;
	int status;

	if (app == NULL)
		return -1;

	thread_id = cpu_core_map_get_lcore_id(app->core_map,
			socket_id,
			core_id,
			hyper_th_id);

	if ((thread_id < 0) ||
		((app->core_mask & (1LLU << thread_id)) == 0))
		return -1;

	if (app_pipeline_data(app, pipeline_id) == NULL)
		return -1;

	p = &app->pipeline_data[pipeline_id];

	if (p->enabled == 0)
		return -1;

	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = THREAD_MSG_REQ_PIPELINE_DISABLE;
	req->pipeline_id = pipeline_id;

	rsp = thread_msg_send_recv(app,
		socket_id, core_id, hyper_th_id, req, MSG_TIMEOUT_DEFAULT);

	if (rsp == NULL)
		return -1;

	status = rsp->status;
	app_msg_free(app, rsp);

	if (status != 0)
		return -1;

	p->enabled = 0;
	return 0;
}

int
app_thread_headroom(struct app_params *app,
		uint32_t socket_id,
		uint32_t core_id,
		uint32_t hyper_th_id)
{
	struct thread_headroom_read_msg_req *req;
	struct thread_headroom_read_msg_rsp *rsp;
	int thread_id;
	int status;

	if (app == NULL)
		return -1;

	thread_id = cpu_core_map_get_lcore_id(app->core_map,
			socket_id,
			core_id,
			hyper_th_id);

	if ((thread_id < 0) ||
		((app->core_mask & (1LLU << thread_id)) == 0))
		return -1;

	req = app_msg_alloc(app);
	if (req == NULL)
		return -1;

	req->type = THREAD_MSG_REQ_HEADROOM_READ;

	rsp = thread_msg_send_recv(app,
		socket_id, core_id, hyper_th_id, req, MSG_TIMEOUT_DEFAULT);

	if (rsp == NULL)
		return -1;

	status = rsp->status;

	if (status != 0)
		return -1;

	printf("%.3f%%\n", rsp->headroom_ratio * 100);


	app_msg_free(app, rsp);

	return 0;
}

/*
 * pipeline enable
 */

struct cmd_pipeline_enable_result {
	cmdline_fixed_string_t t_string;
	cmdline_fixed_string_t t_id_string;
	cmdline_fixed_string_t pipeline_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t enable_string;
};

static void
cmd_pipeline_enable_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	 void *data)
{
	struct cmd_pipeline_enable_result *params = parsed_result;
	struct app_params *app = data;
	int status;
	uint32_t core_id, socket_id, hyper_th_id;

	if (parse_pipeline_core(&socket_id,
			&core_id,
			&hyper_th_id,
			params->t_id_string) != 0) {
		printf("Command failed\n");
		return;
	}

	status = app_pipeline_enable(app,
			socket_id,
			core_id,
			hyper_th_id,
			params->pipeline_id);

	if (status != 0)
		printf("Command failed\n");
}

static cmdline_parse_token_string_t cmd_pipeline_enable_t_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_enable_result, t_string, "t");

static cmdline_parse_token_string_t cmd_pipeline_enable_t_id_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_enable_result, t_id_string,
		NULL);

static cmdline_parse_token_string_t cmd_pipeline_enable_pipeline_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_enable_result, pipeline_string,
		"pipeline");

static cmdline_parse_token_num_t cmd_pipeline_enable_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pipeline_enable_result, pipeline_id,
		UINT32);

static cmdline_parse_token_string_t cmd_pipeline_enable_enable_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_enable_result, enable_string,
		"enable");

static cmdline_parse_inst_t cmd_pipeline_enable = {
	.f = cmd_pipeline_enable_parsed,
	.data = NULL,
	.help_str = "Enable pipeline on specified core",
	.tokens = {
		(void *)&cmd_pipeline_enable_t_string,
		(void *)&cmd_pipeline_enable_t_id_string,
		(void *)&cmd_pipeline_enable_pipeline_string,
		(void *)&cmd_pipeline_enable_pipeline_id,
		(void *)&cmd_pipeline_enable_enable_string,
		NULL,
	},
};

/*
 * pipeline disable
 */

struct cmd_pipeline_disable_result {
	cmdline_fixed_string_t t_string;
	cmdline_fixed_string_t t_id_string;
	cmdline_fixed_string_t pipeline_string;
	uint32_t pipeline_id;
	cmdline_fixed_string_t disable_string;
};

static void
cmd_pipeline_disable_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	 void *data)
{
	struct cmd_pipeline_disable_result *params = parsed_result;
	struct app_params *app = data;
	int status;
	uint32_t core_id, socket_id, hyper_th_id;

	if (parse_pipeline_core(&socket_id,
			&core_id,
			&hyper_th_id,
			params->t_id_string) != 0) {
		printf("Command failed\n");
		return;
	}

	status = app_pipeline_disable(app,
			socket_id,
			core_id,
			hyper_th_id,
			params->pipeline_id);

	if (status != 0)
		printf("Command failed\n");
}

static cmdline_parse_token_string_t cmd_pipeline_disable_t_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_disable_result, t_string, "t");

static cmdline_parse_token_string_t cmd_pipeline_disable_t_id_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_disable_result, t_id_string,
		NULL);

static cmdline_parse_token_string_t cmd_pipeline_disable_pipeline_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_disable_result,
		pipeline_string, "pipeline");

static cmdline_parse_token_num_t cmd_pipeline_disable_pipeline_id =
	TOKEN_NUM_INITIALIZER(struct cmd_pipeline_disable_result, pipeline_id,
		UINT32);

static cmdline_parse_token_string_t cmd_pipeline_disable_disable_string =
	TOKEN_STRING_INITIALIZER(struct cmd_pipeline_disable_result, disable_string,
		"disable");

static cmdline_parse_inst_t cmd_pipeline_disable = {
	.f = cmd_pipeline_disable_parsed,
	.data = NULL,
	.help_str = "Disable pipeline on specified core",
	.tokens = {
		(void *)&cmd_pipeline_disable_t_string,
		(void *)&cmd_pipeline_disable_t_id_string,
		(void *)&cmd_pipeline_disable_pipeline_string,
		(void *)&cmd_pipeline_disable_pipeline_id,
		(void *)&cmd_pipeline_disable_disable_string,
		NULL,
	},
};


/*
 * thread headroom
 */

struct cmd_thread_headroom_result {
	cmdline_fixed_string_t t_string;
	cmdline_fixed_string_t t_id_string;
	cmdline_fixed_string_t headroom_string;
};

static void
cmd_thread_headroom_parsed(
	void *parsed_result,
	__rte_unused struct cmdline *cl,
	 void *data)
{
	struct cmd_thread_headroom_result *params = parsed_result;
	struct app_params *app = data;
	int status;
	uint32_t core_id, socket_id, hyper_th_id;

	if (parse_pipeline_core(&socket_id,
			&core_id,
			&hyper_th_id,
			params->t_id_string) != 0) {
		printf("Command failed\n");
		return;
	}

	status = app_thread_headroom(app,
			socket_id,
			core_id,
			hyper_th_id);

	if (status != 0)
		printf("Command failed\n");
}

static cmdline_parse_token_string_t cmd_thread_headroom_t_string =
	TOKEN_STRING_INITIALIZER(struct cmd_thread_headroom_result,
	t_string, "t");

static cmdline_parse_token_string_t cmd_thread_headroom_t_id_string =
	TOKEN_STRING_INITIALIZER(struct cmd_thread_headroom_result,
	t_id_string, NULL);

static cmdline_parse_token_string_t cmd_thread_headroom_headroom_string =
	TOKEN_STRING_INITIALIZER(struct cmd_thread_headroom_result,
		headroom_string, "headroom");

static cmdline_parse_inst_t cmd_thread_headroom = {
	.f = cmd_thread_headroom_parsed,
	.data = NULL,
	.help_str = "Display thread headroom",
	.tokens = {
		(void *)&cmd_thread_headroom_t_string,
		(void *)&cmd_thread_headroom_t_id_string,
		(void *)&cmd_thread_headroom_headroom_string,
		NULL,
	},
};


static cmdline_parse_ctx_t thread_cmds[] = {
	(cmdline_parse_inst_t *) &cmd_pipeline_enable,
	(cmdline_parse_inst_t *) &cmd_pipeline_disable,
	(cmdline_parse_inst_t *) &cmd_thread_headroom,
	NULL,
};

int
app_pipeline_thread_cmd_push(struct app_params *app)
{
	uint32_t n_cmds, i;

	/* Check for available slots in the application commands array */
	n_cmds = RTE_DIM(thread_cmds) - 1;
	if (n_cmds > APP_MAX_CMDS - app->n_cmds)
		return -ENOMEM;

	/* Push thread commands into the application */
	memcpy(&app->cmds[app->n_cmds], thread_cmds,
		n_cmds * sizeof(cmdline_parse_ctx_t));

	for (i = 0; i < n_cmds; i++)
		app->cmds[app->n_cmds + i]->data = app;

	app->n_cmds += n_cmds;
	app->cmds[app->n_cmds] = NULL;

	return 0;
}
