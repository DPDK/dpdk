/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <rte_mtr.h>
#include <rte_mtr_driver.h>

#include "rte_eth_softnic_internals.h"

int
softnic_mtr_init(struct pmd_internals *p)
{
	/* Initialize meter profiles list */
	TAILQ_INIT(&p->mtr.meter_profiles);

	/* Initialize meter policies list */
	TAILQ_INIT(&p->mtr.meter_policies);

	/* Initialize MTR objects list */
	TAILQ_INIT(&p->mtr.mtrs);

	return 0;
}

void
softnic_mtr_free(struct pmd_internals *p)
{
	/* Remove MTR objects */
	for ( ; ; ) {
		struct softnic_mtr *m;

		m = TAILQ_FIRST(&p->mtr.mtrs);
		if (m == NULL)
			break;

		TAILQ_REMOVE(&p->mtr.mtrs, m, node);
		free(m);
	}

	/* Remove meter profiles */
	for ( ; ; ) {
		struct softnic_mtr_meter_profile *mp;

		mp = TAILQ_FIRST(&p->mtr.meter_profiles);
		if (mp == NULL)
			break;

		TAILQ_REMOVE(&p->mtr.meter_profiles, mp, node);
		free(mp);
	}

	/* Remove meter policies */
	for ( ; ; ) {
		struct softnic_mtr_meter_policy *mp;

		mp = TAILQ_FIRST(&p->mtr.meter_policies);
		if (mp == NULL)
			break;

		TAILQ_REMOVE(&p->mtr.meter_policies, mp, node);
		free(mp);
	}
}

struct softnic_mtr_meter_profile *
softnic_mtr_meter_profile_find(struct pmd_internals *p,
	uint32_t meter_profile_id)
{
	struct softnic_mtr_meter_profile_list *mpl = &p->mtr.meter_profiles;
	struct softnic_mtr_meter_profile *mp;

	TAILQ_FOREACH(mp, mpl, node)
		if (meter_profile_id == mp->meter_profile_id)
			return mp;

	return NULL;
}

static int
meter_profile_check(struct rte_eth_dev *dev,
	uint32_t meter_profile_id,
	struct rte_mtr_meter_profile *profile,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_meter_profile *mp;

	/* Meter profile ID must be valid. */
	if (meter_profile_id == UINT32_MAX)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL,
			"Meter profile id not valid");

	/* Meter profile must not exist. */
	mp = softnic_mtr_meter_profile_find(p, meter_profile_id);
	if (mp)
		return -rte_mtr_error_set(error,
			EEXIST,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL,
			"Meter prfile already exists");

	/* Profile must not be NULL. */
	if (profile == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE,
			NULL,
			"profile null");

	/* Traffic metering algorithm : TRTCM_RFC2698 */
	if (profile->alg != RTE_MTR_TRTCM_RFC2698)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE,
			NULL,
			"Metering alg not supported");

	/* Not support packet mode, just support byte mode. */
	if (profile->packet_mode)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_PACKET_MODE,
			NULL,
			"Meter packet mode not supported");

	return 0;
}

/* MTR meter profile add */
static int
pmd_mtr_meter_profile_add(struct rte_eth_dev *dev,
	uint32_t meter_profile_id,
	struct rte_mtr_meter_profile *profile,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_meter_profile_list *mpl = &p->mtr.meter_profiles;
	struct softnic_mtr_meter_profile *mp;
	int status;

	/* Check input params */
	status = meter_profile_check(dev, meter_profile_id, profile, error);
	if (status)
		return status;

	/* Memory allocation */
	mp = calloc(1, sizeof(struct softnic_mtr_meter_profile));
	if (mp == NULL)
		return -rte_mtr_error_set(error,
			ENOMEM,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			"Memory alloc failed");

	/* Fill in */
	mp->meter_profile_id = meter_profile_id;
	memcpy(&mp->params, profile, sizeof(mp->params));

	/* Add to list */
	TAILQ_INSERT_TAIL(mpl, mp, node);

	return 0;
}

/* MTR meter profile delete */
static int
pmd_mtr_meter_profile_delete(struct rte_eth_dev *dev,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_meter_profile *mp;

	/* Meter profile must exist */
	mp = softnic_mtr_meter_profile_find(p, meter_profile_id);
	if (mp == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL,
			"Meter profile id invalid");

	/* Check unused */
	if (mp->n_users)
		return -rte_mtr_error_set(error,
			EBUSY,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL,
			"Meter profile in use");

	/* Remove from list */
	TAILQ_REMOVE(&p->mtr.meter_profiles, mp, node);
	free(mp);

	return 0;
}

struct softnic_mtr_meter_policy *
softnic_mtr_meter_policy_find(struct pmd_internals *p,
	uint32_t meter_policy_id)
{
	struct softnic_mtr_meter_policy_list *mpl = &p->mtr.meter_policies;
	struct softnic_mtr_meter_policy *mp;

	TAILQ_FOREACH(mp, mpl, node)
		if (meter_policy_id == mp->meter_policy_id)
			return mp;

	return NULL;
}

/* MTR meter policy add */
static int
pmd_mtr_meter_policy_add(struct rte_eth_dev *dev,
	uint32_t meter_policy_id,
	struct rte_mtr_meter_policy_params *policy,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_meter_policy_list *mpl = &p->mtr.meter_policies;
	struct softnic_mtr_meter_policy *mp;
	const struct rte_flow_action *act;
	const struct rte_flow_action_meter_color *recolor;
	uint32_t i;
	bool valid_act_found;

	if (policy == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_POLICY,
			NULL,
			"Null meter policy invalid");

	/* Meter policy must not exist. */
	mp = softnic_mtr_meter_policy_find(p, meter_policy_id);
	if (mp != NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL,
			"Meter policy already exists");

	for (i = 0; i < RTE_COLORS; i++) {
		if (policy->actions[i] == NULL)
			return -rte_mtr_error_set(error,
				EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL,
				"Null action list");
		for (act = policy->actions[i], valid_act_found = false;
		     act->type != RTE_FLOW_ACTION_TYPE_END; act++) {
			if (act->type == RTE_FLOW_ACTION_TYPE_VOID)
				continue;
			/*
			 * Support one (and one only) of
			 * METER_COLOR or DROP action.
			 */
			if ((act->type != RTE_FLOW_ACTION_TYPE_METER_COLOR &&
				act->type != RTE_FLOW_ACTION_TYPE_DROP) ||
				valid_act_found)
				return -rte_mtr_error_set(error,
					EINVAL,
					RTE_MTR_ERROR_TYPE_METER_POLICY,
					NULL,
					"Action invalid");
			valid_act_found = true;
		}
		if (!valid_act_found)
			return -rte_mtr_error_set(error,
				EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY,
				NULL,
				"No valid action found");
	}

	/* Memory allocation */
	mp = calloc(1, sizeof(struct softnic_mtr_meter_policy));
	if (mp == NULL)
		return -rte_mtr_error_set(error,
			ENOMEM,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			"Memory alloc failed");

	/* Fill in */
	mp->meter_policy_id = meter_policy_id;
	for (i = 0; i < RTE_COLORS; i++) {
		mp->policer[i] = RTE_TABLE_ACTION_POLICER_DROP;
		act = policy->actions[i];
		if (!act)
			continue;
		if (act->type == RTE_FLOW_ACTION_TYPE_METER_COLOR) {
			recolor = act->conf;
			switch (recolor->color) {
			case RTE_COLOR_GREEN:
				mp->policer[i] =
				RTE_TABLE_ACTION_POLICER_COLOR_GREEN;
				break;
			case RTE_COLOR_YELLOW:
				mp->policer[i] =
				RTE_TABLE_ACTION_POLICER_COLOR_YELLOW;
				break;
			case RTE_COLOR_RED:
				mp->policer[i] =
				RTE_TABLE_ACTION_POLICER_COLOR_RED;
				break;
			default:
				break;
			}
		}
	}

	/* Add to list */
	TAILQ_INSERT_TAIL(mpl, mp, node);

	return 0;
}

/* MTR meter policy delete */
static int
pmd_mtr_meter_policy_delete(struct rte_eth_dev *dev,
	uint32_t meter_policy_id,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_meter_policy *mp;

	/* Meter policy must exist */
	mp = softnic_mtr_meter_policy_find(p, meter_policy_id);
	if (mp == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL,
			"Meter policy id invalid");

	/* Check unused */
	if (mp->n_users)
		return -rte_mtr_error_set(error,
			EBUSY,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL,
			"Meter policy in use");

	/* Remove from list */
	TAILQ_REMOVE(&p->mtr.meter_policies, mp, node);
	free(mp);

	return 0;
}

struct softnic_mtr *
softnic_mtr_find(struct pmd_internals *p, uint32_t mtr_id)
{
	struct softnic_mtr_list *ml = &p->mtr.mtrs;
	struct softnic_mtr *m;

	TAILQ_FOREACH(m, ml, node)
		if (m->mtr_id == mtr_id)
			return m;

	return NULL;
}


static int
mtr_check(struct pmd_internals *p,
	uint32_t mtr_id,
	struct rte_mtr_params *params,
	int shared,
	struct rte_mtr_error *error)
{
	/* MTR id valid  */
	if (softnic_mtr_find(p, mtr_id))
		return -rte_mtr_error_set(error,
			EEXIST,
			RTE_MTR_ERROR_TYPE_MTR_ID,
			NULL,
			"MTR object already exists");

	/* MTR params must not be NULL */
	if (params == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_MTR_PARAMS,
			NULL,
			"MTR object params null");

	/* Previous meter color not supported */
	if (params->use_prev_mtr_color)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_MTR_PARAMS,
			NULL,
			"Previous meter color not supported");

	/* Shared MTR object not supported */
	if (shared)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_SHARED,
			NULL,
			"Shared MTR object not supported");

	return 0;
}

/* MTR object create */
static int
pmd_mtr_create(struct rte_eth_dev *dev,
	uint32_t mtr_id,
	struct rte_mtr_params *params,
	int shared,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_list *ml = &p->mtr.mtrs;
	struct softnic_mtr_meter_profile *mp;
	struct softnic_mtr_meter_policy *policy;
	struct softnic_mtr *m;
	int status;

	/* Check parameters */
	status = mtr_check(p, mtr_id, params, shared, error);
	if (status)
		return status;

	/* Meter profile must exist */
	mp = softnic_mtr_meter_profile_find(p, params->meter_profile_id);
	if (mp == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL,
			"Meter profile id not valid");

	/* Meter policy must exist */
	policy = softnic_mtr_meter_policy_find(p, params->meter_policy_id);
	if (policy == NULL) {
		return -rte_mtr_error_set(error,
				EINVAL,
				RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
				NULL,
				"Meter policy id invalid");
	}

	/* Memory allocation */
	m = calloc(1, sizeof(struct softnic_mtr));
	if (m == NULL)
		return -rte_mtr_error_set(error,
			ENOMEM,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			"Memory alloc failed");

	/* Fill in */
	m->mtr_id = mtr_id;
	memcpy(&m->params, params, sizeof(m->params));

	/* Add to list */
	TAILQ_INSERT_TAIL(ml, m, node);

	/* Update dependencies */
	mp->n_users++;
	policy->n_users++;

	return 0;
}

/* MTR object destroy */
static int
pmd_mtr_destroy(struct rte_eth_dev *dev,
	uint32_t mtr_id,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_list *ml = &p->mtr.mtrs;
	struct softnic_mtr_meter_profile *mp;
	struct softnic_mtr *m;
	struct softnic_mtr_meter_policy *policy;

	/* MTR object must exist */
	m = softnic_mtr_find(p, mtr_id);
	if (m == NULL)
		return -rte_mtr_error_set(error,
			EEXIST,
			RTE_MTR_ERROR_TYPE_MTR_ID,
			NULL,
			"MTR object id not valid");

	/* MTR object must not have any owner */
	if (m->flow != NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			"MTR object is being used");

	/* Get meter profile */
	mp = softnic_mtr_meter_profile_find(p, m->params.meter_profile_id);
	if (mp == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL,
			"MTR object meter profile invalid");

	/* Meter policy must exist */
	policy = softnic_mtr_meter_policy_find(p, m->params.meter_policy_id);
	if (policy == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL,
			"MTR object meter policy invalid");

	/* Update dependencies */
	mp->n_users--;
	policy->n_users--;

	/* Remove from list */
	TAILQ_REMOVE(ml, m, node);
	free(m);

	return 0;
}

/* MTR object meter profile update */
static int
pmd_mtr_meter_profile_update(struct rte_eth_dev *dev,
	uint32_t mtr_id,
	uint32_t meter_profile_id,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr_meter_profile *mp_new, *mp_old;
	struct softnic_mtr *m;
	int status;

	/* MTR object id must be valid */
	m = softnic_mtr_find(p, mtr_id);
	if (m == NULL)
		return -rte_mtr_error_set(error,
			EEXIST,
			RTE_MTR_ERROR_TYPE_MTR_ID,
			NULL,
			"MTR object id not valid");

	/* Meter profile id must be valid */
	mp_new = softnic_mtr_meter_profile_find(p, meter_profile_id);
	if (mp_new == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_PROFILE_ID,
			NULL,
			"Meter profile not valid");

	/* MTR object already set to meter profile id */
	if (m->params.meter_profile_id == meter_profile_id)
		return 0;

	/*  MTR object owner table update */
	if (m->flow) {
		uint32_t table_id = m->flow->table_id;
		struct softnic_table *table = &m->flow->pipeline->table[table_id];
		struct softnic_table_rule_action action;

		if (!softnic_pipeline_table_meter_profile_find(table,
			meter_profile_id)) {
			struct rte_table_action_meter_profile profile;

			memset(&profile, 0, sizeof(profile));

			profile.alg = RTE_TABLE_ACTION_METER_TRTCM;
			profile.trtcm.cir = mp_new->params.trtcm_rfc2698.cir;
			profile.trtcm.pir = mp_new->params.trtcm_rfc2698.pir;
			profile.trtcm.cbs = mp_new->params.trtcm_rfc2698.cbs;
			profile.trtcm.pbs = mp_new->params.trtcm_rfc2698.pbs;

			/* Add meter profile to pipeline table */
			status = softnic_pipeline_table_mtr_profile_add(p,
					m->flow->pipeline->name,
					table_id,
					meter_profile_id,
					&profile);
			if (status)
				return -rte_mtr_error_set(error,
					EINVAL,
					RTE_MTR_ERROR_TYPE_UNSPECIFIED,
					NULL,
					"Table meter profile add failed");
		}

		/* Set meter action */
		memcpy(&action, &m->flow->action, sizeof(action));

		action.mtr.mtr[0].meter_profile_id = meter_profile_id;

		/* Re-add rule */
		status = softnic_pipeline_table_rule_add(p,
			m->flow->pipeline->name,
			table_id,
			&m->flow->match,
			&action,
			&m->flow->data);
		if (status)
			return -rte_mtr_error_set(error,
				EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Pipeline table rule add failed");

		/* Flow: update meter action */
		memcpy(&m->flow->action, &action, sizeof(m->flow->action));
	}

	mp_old = softnic_mtr_meter_profile_find(p, m->params.meter_profile_id);

	/* Meter: Set meter profile */
	m->params.meter_profile_id = meter_profile_id;

	/* Update dependencies*/
	mp_old->n_users--;
	mp_new->n_users++;

	return 0;
}

/* MTR object meter DSCP table update */
static int
pmd_mtr_meter_dscp_table_update(struct rte_eth_dev *dev,
	uint32_t mtr_id,
	enum rte_color *dscp_table,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct rte_table_action_dscp_table dt;
	struct pipeline *pipeline;
	struct softnic_table *table;
	struct softnic_mtr *m;
	uint32_t table_id, i;
	int status;

	/* MTR object id must be valid */
	m = softnic_mtr_find(p, mtr_id);
	if (m == NULL)
		return -rte_mtr_error_set(error,
			EEXIST,
			RTE_MTR_ERROR_TYPE_MTR_ID,
			NULL,
			"MTR object id not valid");

	/* MTR object owner valid? */
	if (m->flow == NULL)
		return 0;

	pipeline = m->flow->pipeline;
	table_id = m->flow->table_id;
	table = &pipeline->table[table_id];

	memcpy(&dt, &table->dscp_table, sizeof(dt));
	for (i = 0; i < RTE_DIM(dt.entry); i++)
		dt.entry[i].color = (enum rte_color)dscp_table[i];

	/* Update table */
	status = softnic_pipeline_table_dscp_table_update(p,
			pipeline->name,
			table_id,
			UINT64_MAX,
			&dt);
	if (status)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			"Table action dscp table update failed");

	return 0;
}

/* MTR object policy update */
static int
pmd_mtr_meter_policy_update(struct rte_eth_dev *dev,
	uint32_t mtr_id,
	uint32_t meter_policy_id,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct softnic_mtr *m;
	uint32_t i;
	int status;
	struct softnic_mtr_meter_policy *mp_new, *mp_old;

	/* MTR object id must be valid */
	m = softnic_mtr_find(p, mtr_id);
	if (m == NULL)
		return -rte_mtr_error_set(error,
			EEXIST,
			RTE_MTR_ERROR_TYPE_MTR_ID,
			NULL,
			"MTR object id not valid");

	if (m->params.meter_policy_id == meter_policy_id)
		return 0;

	/* Meter policy must exist */
	mp_new = softnic_mtr_meter_policy_find(p, meter_policy_id);
	if (mp_new == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL,
			"Meter policy id invalid");

	/* MTR object owner valid? */
	if (m->flow) {
		struct pipeline *pipeline = m->flow->pipeline;
		struct softnic_table *table = &pipeline->table[m->flow->table_id];
		struct softnic_table_rule_action action;

		memcpy(&action, &m->flow->action, sizeof(action));

		/* Set action */
		for (i = 0; i < RTE_COLORS; i++)
			action.mtr.mtr[0].policer[i] = mp_new->policer[i];

		/* Re-add the rule */
		status = softnic_pipeline_table_rule_add(p,
			pipeline->name,
			m->flow->table_id,
			&m->flow->match,
			&action,
			&m->flow->data);
		if (status)
			return -rte_mtr_error_set(error,
				EINVAL,
				RTE_MTR_ERROR_TYPE_UNSPECIFIED,
				NULL,
				"Pipeline table rule re-add failed");

		/* Flow: Update meter action */
		memcpy(&m->flow->action, &action, sizeof(m->flow->action));

		/* Reset the meter stats */
		rte_table_action_meter_read(table->a, m->flow->data,
			1, NULL, 1);
	}

	mp_old = softnic_mtr_meter_policy_find(p, m->params.meter_policy_id);
	if (mp_old == NULL)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_METER_POLICY_ID,
			NULL,
			"Old meter policy id invalid");

	/* Meter: Set meter profile */
	m->params.meter_policy_id = meter_policy_id;

	/* Update dependencies*/
	mp_old->n_users--;
	mp_new->n_users++;

	return 0;
}

#define MTR_STATS_PKTS_DEFAULT (RTE_MTR_STATS_N_PKTS_GREEN | \
				RTE_MTR_STATS_N_PKTS_YELLOW | \
				RTE_MTR_STATS_N_PKTS_RED | \
				RTE_MTR_STATS_N_PKTS_DROPPED)

#define MTR_STATS_BYTES_DEFAULT (RTE_MTR_STATS_N_BYTES_GREEN | \
				RTE_MTR_STATS_N_BYTES_YELLOW | \
				RTE_MTR_STATS_N_BYTES_RED | \
				RTE_MTR_STATS_N_BYTES_DROPPED)

/* MTR object stats read */
static void
mtr_stats_convert(struct pmd_internals *p,
	struct softnic_mtr *m,
	struct rte_table_action_mtr_counters_tc *in,
	struct rte_mtr_stats *out,
	uint64_t *out_mask)
{
	struct softnic_mtr_meter_policy *mp;

	memset(&out, 0, sizeof(out));
	*out_mask = 0;

	/* Meter policy must exist */
	mp = softnic_mtr_meter_policy_find(p, m->params.meter_policy_id);
	if (mp == NULL)
		return;

	if (in->n_packets_valid) {
		uint32_t i;

		for (i = 0; i < RTE_COLORS; i++) {
			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_COLOR_GREEN)
				out->n_pkts[RTE_COLOR_GREEN] += in->n_packets[i];

			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_COLOR_YELLOW)
				out->n_pkts[RTE_COLOR_YELLOW] += in->n_packets[i];

			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_COLOR_RED)
				out->n_pkts[RTE_COLOR_RED] += in->n_packets[i];

			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_DROP)
				out->n_pkts_dropped += in->n_packets[i];
		}

		*out_mask |= MTR_STATS_PKTS_DEFAULT;
	}

	if (in->n_bytes_valid) {
		uint32_t i;

		for (i = 0; i < RTE_COLORS; i++) {
			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_COLOR_GREEN)
				out->n_bytes[RTE_COLOR_GREEN] += in->n_bytes[i];

			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_COLOR_YELLOW)
				out->n_bytes[RTE_COLOR_YELLOW] += in->n_bytes[i];

			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_COLOR_RED)
				out->n_bytes[RTE_COLOR_RED] += in->n_bytes[i];

			if (mp->policer[i] ==
				RTE_TABLE_ACTION_POLICER_DROP)
				out->n_bytes_dropped += in->n_bytes[i];
		}

		*out_mask |= MTR_STATS_BYTES_DEFAULT;
	}
}

/* MTR object stats read */
static int
pmd_mtr_stats_read(struct rte_eth_dev *dev,
	uint32_t mtr_id,
	struct rte_mtr_stats *stats,
	uint64_t *stats_mask,
	int clear,
	struct rte_mtr_error *error)
{
	struct pmd_internals *p = dev->data->dev_private;
	struct rte_table_action_mtr_counters counters;
	struct pipeline *pipeline;
	struct softnic_table *table;
	struct softnic_mtr *m;
	int status;

	/* MTR object id must be valid */
	m = softnic_mtr_find(p, mtr_id);
	if (m == NULL)
		return -rte_mtr_error_set(error,
			EEXIST,
			RTE_MTR_ERROR_TYPE_MTR_ID,
			NULL,
			"MTR object id not valid");

	/* MTR meter object owner valid? */
	if (m->flow == NULL) {
		if (stats != NULL)
			memset(stats, 0, sizeof(*stats));

		if (stats_mask)
			*stats_mask = MTR_STATS_PKTS_DEFAULT |
				MTR_STATS_BYTES_DEFAULT;

		return 0;
	}

	pipeline = m->flow->pipeline;
	table = &pipeline->table[m->flow->table_id];

	/* Meter stats read. */
	status = rte_table_action_meter_read(table->a,
		m->flow->data,
		1,
		&counters,
		clear);
	if (status)
		return -rte_mtr_error_set(error,
			EINVAL,
			RTE_MTR_ERROR_TYPE_UNSPECIFIED,
			NULL,
			"Meter stats read failed");

	/* Stats format conversion. */
	if (stats || stats_mask) {
		struct rte_mtr_stats s;
		uint64_t s_mask = 0;

		mtr_stats_convert(p,
			m,
			&counters.stats[0],
			&s,
			&s_mask);

		if (stats)
			memcpy(stats, &s, sizeof(*stats));

		if (stats_mask)
			*stats_mask = s_mask;
	}

	return 0;
}

const struct rte_mtr_ops pmd_mtr_ops = {
	.capabilities_get = NULL,

	.meter_profile_add = pmd_mtr_meter_profile_add,
	.meter_profile_delete = pmd_mtr_meter_profile_delete,

	.meter_policy_add = pmd_mtr_meter_policy_add,
	.meter_policy_delete = pmd_mtr_meter_policy_delete,

	.create = pmd_mtr_create,
	.destroy = pmd_mtr_destroy,
	.meter_enable = NULL,
	.meter_disable = NULL,

	.meter_profile_update = pmd_mtr_meter_profile_update,
	.meter_dscp_table_update = pmd_mtr_meter_dscp_table_update,
	.meter_policy_update = pmd_mtr_meter_policy_update,
	.stats_update = NULL,

	.stats_read = pmd_mtr_stats_read,
};
