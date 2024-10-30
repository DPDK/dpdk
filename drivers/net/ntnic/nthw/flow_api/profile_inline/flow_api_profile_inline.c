/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "ntlog.h"
#include "flow_api_engine.h"
#include "flow_api_hw_db_inline.h"
#include "flow_id_table.h"

#include "flow_api_profile_inline.h"
#include "ntnic_mod_reg.h"

/*
 * Public functions
 */

int initialize_flow_management_of_ndev_profile_inline(struct flow_nic_dev *ndev)
{
	if (!ndev->flow_mgnt_prepared) {
		/* Check static arrays are big enough */
		assert(ndev->be.tpe.nb_cpy_writers <= MAX_CPY_WRITERS_SUPPORTED);

		ndev->id_table_handle = ntnic_id_table_create();

		if (ndev->id_table_handle == NULL)
			goto err_exit0;

		if (flow_group_handle_create(&ndev->group_handle, ndev->be.flm.nb_categories))
			goto err_exit0;

		if (hw_db_inline_create(ndev, &ndev->hw_db_handle))
			goto err_exit0;

		ndev->flow_mgnt_prepared = 1;
	}

	return 0;

err_exit0:
	done_flow_management_of_ndev_profile_inline(ndev);
	return -1;
}

int done_flow_management_of_ndev_profile_inline(struct flow_nic_dev *ndev)
{
#ifdef FLOW_DEBUG
	ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_WRITE);
#endif

	if (ndev->flow_mgnt_prepared) {
		flow_nic_free_resource(ndev, RES_KM_FLOW_TYPE, 0);
		flow_nic_free_resource(ndev, RES_KM_CATEGORY, 0);

		flow_group_handle_destroy(&ndev->group_handle);
		ntnic_id_table_destroy(ndev->id_table_handle);

		flow_nic_free_resource(ndev, RES_CAT_CFN, 0);

		hw_mod_tpe_reset(&ndev->be);
		flow_nic_free_resource(ndev, RES_TPE_RCP, 0);
		flow_nic_free_resource(ndev, RES_TPE_EXT, 0);
		flow_nic_free_resource(ndev, RES_TPE_RPL, 0);

		hw_db_inline_destroy(ndev->hw_db_handle);

#ifdef FLOW_DEBUG
		ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_NONE);
#endif

		ndev->flow_mgnt_prepared = 0;
	}

	return 0;
}

struct flow_handle *flow_create_profile_inline(struct flow_eth_dev *dev __rte_unused,
	const struct rte_flow_attr *attr __rte_unused,
	uint16_t forced_vlan_vid __rte_unused,
	uint16_t caller_id __rte_unused,
	const struct rte_flow_item elem[] __rte_unused,
	const struct rte_flow_action action[] __rte_unused,
	struct rte_flow_error *error __rte_unused)
{
	return NULL;
}

int flow_destroy_locked_profile_inline(struct flow_eth_dev *dev,
	struct flow_handle *fh,
	struct rte_flow_error *error)
{
	assert(dev);
	assert(fh);

	int err = 0;

	flow_nic_set_error(ERR_SUCCESS, error);

	return err;
}

int flow_destroy_profile_inline(struct flow_eth_dev *dev, struct flow_handle *flow,
	struct rte_flow_error *error)
{
	int err = 0;

	flow_nic_set_error(ERR_SUCCESS, error);

	if (flow) {
		/* Delete this flow */
		pthread_mutex_lock(&dev->ndev->mtx);
		err = flow_destroy_locked_profile_inline(dev, flow, error);
		pthread_mutex_unlock(&dev->ndev->mtx);
	}

	return err;
}

static const struct profile_inline_ops ops = {
	/*
	 * Management
	 */
	.done_flow_management_of_ndev_profile_inline = done_flow_management_of_ndev_profile_inline,
	.initialize_flow_management_of_ndev_profile_inline =
		initialize_flow_management_of_ndev_profile_inline,
	/*
	 * Flow functionality
	 */
	.flow_destroy_locked_profile_inline = flow_destroy_locked_profile_inline,
	.flow_create_profile_inline = flow_create_profile_inline,
	.flow_destroy_profile_inline = flow_destroy_profile_inline,
};

void profile_inline_init(void)
{
	register_profile_inline_ops(&ops);
}
