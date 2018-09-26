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

	return 0;
}

const struct rte_mtr_ops pmd_mtr_ops = {
	.capabilities_get = NULL,

	.meter_profile_add = pmd_mtr_meter_profile_add,
	.meter_profile_delete = pmd_mtr_meter_profile_delete,

	.create = pmd_mtr_create,
	.destroy = NULL,
	.meter_enable = NULL,
	.meter_disable = NULL,

	.meter_profile_update = NULL,
	.meter_dscp_table_update = NULL,
	.policer_actions_update = NULL,
	.stats_update = NULL,

	.stats_read = NULL,
};
