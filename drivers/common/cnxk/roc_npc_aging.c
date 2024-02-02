/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2023 Marvell.
 */

#include "roc_api.h"
#include "roc_priv.h"

int
npc_aged_flows_bitmap_alloc(struct roc_npc *roc_npc)
{
	struct roc_npc_flow_age *flow_age;
	uint8_t *age_mem = NULL;
	uint32_t bmap_sz;
	int rc = 0;

	bmap_sz = plt_bitmap_get_memory_footprint(MCAM_ARR_ELEM_SZ *
						  MCAM_ARR_SIZE);
	age_mem = plt_zmalloc(bmap_sz, 0);
	if (age_mem == NULL) {
		plt_err("Bmap alloc failed");
		rc = NPC_ERR_NO_MEM;
		goto done;
	}

	flow_age = &roc_npc->flow_age;
	flow_age->age_mem = age_mem;
	flow_age->aged_flows = plt_bitmap_init(MCAM_ARR_ELEM_SZ * MCAM_ARR_SIZE,
					       age_mem, bmap_sz);
	if (!flow_age->aged_flows) {
		plt_err("Bitmap init failed");
		plt_free(age_mem);
		rc = NPC_ERR_NO_MEM;
		goto done;
	}

done:
	return rc;
}

void
npc_aged_flows_bitmap_free(struct roc_npc *roc_npc)
{
	struct roc_npc_flow_age *flow_age;

	flow_age = &roc_npc->flow_age;
	plt_bitmap_free(flow_age->aged_flows);
	if (flow_age->age_mem)
		plt_free(roc_npc->flow_age.age_mem);
}

static void
check_timeout_cycles(struct roc_npc *roc_npc, uint32_t mcam_id)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_age_flow_list_head *list;
	struct npc_age_flow_entry *fl_iter;
	struct roc_npc_flow_age *flow_age;

	flow_age = &roc_npc->flow_age;
	list = &npc->age_flow_list;
	TAILQ_FOREACH(fl_iter, list, next) {
		if (fl_iter->flow->mcam_id == mcam_id &&
		    fl_iter->flow->timeout_cycles < plt_tsc_cycles()) {
			/* update bitmap */
			plt_bitmap_set(flow_age->aged_flows, mcam_id);
			if (flow_age->aged_flows_cnt == 0) {
				flow_age->start_id = mcam_id;
				flow_age->end_id = mcam_id;
			}
			if (flow_age->start_id > mcam_id)
				flow_age->start_id = mcam_id;
			else if (flow_age->end_id < mcam_id)
				flow_age->end_id = mcam_id;
			flow_age->aged_flows_cnt += 1;
			break;
		}
	}
}

static void
update_timeout_cycles(struct roc_npc *roc_npc, uint32_t mcam_id)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_age_flow_list_head *list;
	struct npc_age_flow_entry *fl_iter;

	list = &npc->age_flow_list;
	TAILQ_FOREACH(fl_iter, list, next) {
		if (fl_iter->flow->mcam_id == mcam_id) {
			fl_iter->flow->timeout_cycles = plt_tsc_cycles() +
				fl_iter->flow->timeout * plt_tsc_hz();
			break;
		}
	}
}

static int
npc_mcam_get_hit_status(struct npc *npc, uint64_t *mcam_ids, uint16_t start_id,
			uint16_t end_id, uint64_t *hit_status, bool clear)
{
	struct npc_mcam_get_hit_status_req *req;
	struct npc_mcam_get_hit_status_rsp *rsp;
	struct mbox *mbox = mbox_get(npc->mbox);
	uint8_t idx_start;
	uint8_t idx_end;
	int rc;
	int i;

	req = mbox_alloc_msg_npc_mcam_get_hit_status(mbox);
	if (req == NULL)
		return -ENOSPC;

	idx_start = start_id / MCAM_ARR_ELEM_SZ;
	idx_end = end_id / MCAM_ARR_ELEM_SZ;

	for (i = idx_start; i <= idx_end; i++)
		req->mcam_ids[i] = mcam_ids[i];

	req->range_valid_mcam_ids_start = start_id;
	req->range_valid_mcam_ids_end = end_id;
	req->clear = clear;

	rc = mbox_process_msg(mbox, (void *)&rsp);
	if (rc)
		goto exit;

	for (i = idx_start; i <= idx_end; i++)
		hit_status[i] = rsp->mcam_hit_status[i];

	rc = 0;
exit:
	mbox_put(mbox);
	return rc;
}

uint32_t
npc_aged_flows_get(void *args)
{
	uint64_t hit_status[MCAM_ARR_SIZE] = {0};
	uint64_t mcam_ids[MCAM_ARR_SIZE] = {0};
	struct npc_age_flow_list_head *list;
	struct npc_age_flow_entry *fl_iter;
	struct roc_npc *roc_npc = args;
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct roc_npc_flow_age *flow_age;
	bool aging_enabled;
	uint32_t start_id;
	uint32_t end_id;
	uint32_t mcam_id;
	uint32_t idx;
	uint32_t i;
	int rc;

	flow_age = &roc_npc->flow_age;
	list = &npc->age_flow_list;
	while (!flow_age->aged_flows_get_thread_exit) {
		start_id = 0;
		end_id = 0;
		aging_enabled = false;
		memset(mcam_ids, 0, sizeof(mcam_ids));
		TAILQ_FOREACH(fl_iter, list, next) {
			mcam_id = fl_iter->flow->mcam_id;
			idx = mcam_id / MCAM_ARR_ELEM_SZ;
			mcam_ids[idx] |= BIT_ULL(mcam_id % MCAM_ARR_ELEM_SZ);

			if (!aging_enabled) {
				start_id = mcam_id;
				end_id = mcam_id;
				aging_enabled = true;
			}

			if (mcam_id < start_id)
				start_id = mcam_id;
			else if (mcam_id > end_id)
				end_id = mcam_id;
		}

		if (!aging_enabled)
			goto lbl_sleep;

		rc = npc_mcam_get_hit_status(npc, mcam_ids, start_id, end_id,
					     hit_status, true);
		if (rc)
			return 0;

		plt_seqcount_write_begin(&flow_age->seq_cnt);
		flow_age->aged_flows_cnt = 0;
		for (i = start_id; i <= end_id; i++) {
			idx = i / MCAM_ARR_ELEM_SZ;
			if (mcam_ids[idx] & BIT_ULL(i % MCAM_ARR_ELEM_SZ)) {
				if (!(hit_status[idx] & BIT_ULL(i % MCAM_ARR_ELEM_SZ)))
					check_timeout_cycles(roc_npc, i);
				else
					update_timeout_cycles(roc_npc, i);
			}
		}
		plt_seqcount_write_end(&flow_age->seq_cnt);

lbl_sleep:
		sleep(flow_age->aging_poll_freq);
	}

	return 0;
}

void
npc_age_flow_list_entry_add(struct roc_npc *roc_npc, struct roc_npc_flow *flow)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_age_flow_entry *age_fl_iter;
	struct npc_age_flow_entry *new_entry;

	new_entry = plt_zmalloc(sizeof(*new_entry), 0);
	if (new_entry == NULL) {
		plt_err("flow entry alloc failed");
		return;
	}

	new_entry->flow = flow;
	roc_npc->flow_age.age_flow_refcnt++;
	/* List in ascending order of mcam entries */
	TAILQ_FOREACH(age_fl_iter, &npc->age_flow_list, next) {
		if (age_fl_iter->flow->mcam_id > flow->mcam_id) {
			TAILQ_INSERT_BEFORE(age_fl_iter, new_entry, next);
			return;
		}
	}
	TAILQ_INSERT_TAIL(&npc->age_flow_list, new_entry, next);
}

void
npc_age_flow_list_entry_delete(struct roc_npc *roc_npc,
			       struct roc_npc_flow *flow)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_age_flow_list_head *list;
	struct roc_npc_flow_age *flow_age;
	struct npc_age_flow_entry *curr;

	flow_age = &roc_npc->flow_age;

	list = &npc->age_flow_list;
	curr = TAILQ_FIRST(list);

	if (!curr)
		return;

	while (curr) {
		if (flow->mcam_id == curr->flow->mcam_id) {
			plt_bitmap_clear(flow_age->aged_flows, flow->mcam_id);
			TAILQ_REMOVE(list, curr, next);
			plt_free(curr);
			break;
		}
		curr = TAILQ_NEXT(curr, next);
	}
	roc_npc->flow_age.age_flow_refcnt--;
}

int
npc_aging_ctrl_thread_create(struct roc_npc *roc_npc,
			     const struct roc_npc_action_age *age,
			     struct roc_npc_flow *flow)
{
	struct roc_npc_flow_age *flow_age;
	int errcode = 0;

	flow_age = &roc_npc->flow_age;
	if (age->timeout < flow_age->aging_poll_freq) {
		plt_err("Age timeout should be greater or equal to %u seconds",
			flow_age->aging_poll_freq);
		errcode = NPC_ERR_ACTION_NOTSUP;
		goto done;
	}

	flow->age_context = age->context == NULL ? flow : age->context;
	flow->timeout = age->timeout;
	flow->timeout_cycles = plt_tsc_cycles() + age->timeout * plt_tsc_hz();
	flow->has_age_action = true;

	if (flow_age->age_flow_refcnt == 0) {
		errcode = npc_aged_flows_bitmap_alloc(roc_npc);
		if (errcode != 0)
			goto done;

		flow_age->aged_flows_get_thread_exit = false;
		if (plt_thread_create_control(&flow_age->aged_flows_poll_thread,
					   "Aged Flows Get Ctrl Thread",
					   npc_aged_flows_get, roc_npc) != 0) {
			plt_err("Failed to create thread for age flows");
			npc_aged_flows_bitmap_free(roc_npc);
			errcode = NPC_ERR_ACTION_NOTSUP;
			goto done;
		}
	}
done:
	return errcode;
}

void
npc_aging_ctrl_thread_destroy(struct roc_npc *roc_npc)
{
	struct roc_npc_flow_age *flow_age;

	flow_age = &roc_npc->flow_age;
	flow_age->aged_flows_get_thread_exit = true;
	plt_thread_join(flow_age->aged_flows_poll_thread, NULL);
	npc_aged_flows_bitmap_free(roc_npc);
}

void *
roc_npc_aged_flow_ctx_get(struct roc_npc *roc_npc, uint32_t mcam_id)
{
	struct npc *npc = roc_npc_to_npc_priv(roc_npc);
	struct npc_age_flow_list_head *list;
	struct npc_age_flow_entry *fl_iter;

	list = &npc->age_flow_list;

	TAILQ_FOREACH(fl_iter, list, next) {
		if (fl_iter->flow->mcam_id == mcam_id)
			return fl_iter->flow->age_context;
	}

	return NULL;
}
