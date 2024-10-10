/*
 * SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Napatech A/S
 */

#include "flow_api_engine.h"
#include "flow_api_nic_setup.h"
#include "ntnic_mod_reg.h"

#include "flow_filter.h"

const char *dbg_res_descr[] = {
	/* RES_QUEUE */ "RES_QUEUE",
	/* RES_CAT_CFN */ "RES_CAT_CFN",
	/* RES_CAT_COT */ "RES_CAT_COT",
	/* RES_CAT_EXO */ "RES_CAT_EXO",
	/* RES_CAT_LEN */ "RES_CAT_LEN",
	/* RES_KM_FLOW_TYPE */ "RES_KM_FLOW_TYPE",
	/* RES_KM_CATEGORY */ "RES_KM_CATEGORY",
	/* RES_HSH_RCP */ "RES_HSH_RCP",
	/* RES_PDB_RCP */ "RES_PDB_RCP",
	/* RES_QSL_RCP */ "RES_QSL_RCP",
	/* RES_QSL_LTX */ "RES_QSL_LTX",
	/* RES_QSL_QST */ "RES_QSL_QST",
	/* RES_SLC_LR_RCP */ "RES_SLC_LR_RCP",
	/* RES_FLM_FLOW_TYPE */ "RES_FLM_FLOW_TYPE",
	/* RES_FLM_RCP */ "RES_FLM_RCP",
	/* RES_TPE_RCP */ "RES_TPE_RCP",
	/* RES_TPE_EXT */ "RES_TPE_EXT",
	/* RES_TPE_RPL */ "RES_TPE_RPL",
	/* RES_COUNT */ "RES_COUNT",
	/* RES_INVALID */ "RES_INVALID"
};

static struct flow_nic_dev *dev_base;
static pthread_mutex_t base_mtx = PTHREAD_MUTEX_INITIALIZER;

void flow_nic_free_resource(struct flow_nic_dev *ndev, enum res_type_e res_type, int idx)
{
	flow_nic_mark_resource_unused(ndev, res_type, idx);
}

int flow_nic_deref_resource(struct flow_nic_dev *ndev, enum res_type_e res_type, int index)
{
	NT_LOG(DBG, FILTER, "De-reference resource %s idx %i (before ref cnt %i)",
		dbg_res_descr[res_type], index, ndev->res[res_type].ref[index]);
	assert(flow_nic_is_resource_used(ndev, res_type, index));
	assert(ndev->res[res_type].ref[index]);
	/* deref */
	ndev->res[res_type].ref[index]--;

	if (!ndev->res[res_type].ref[index])
		flow_nic_free_resource(ndev, res_type, index);

	return !!ndev->res[res_type].ref[index];/* if 0 resource has been freed */
}

/*
 * Device Management API
 */

static int nic_remove_eth_port_dev(struct flow_nic_dev *ndev, struct flow_eth_dev *eth_dev)
{
	struct flow_eth_dev *dev = ndev->eth_base, *prev = NULL;

	while (dev) {
		if (dev == eth_dev) {
			if (prev)
				prev->next = dev->next;

			else
				ndev->eth_base = dev->next;

			return 0;
		}

		prev = dev;
		dev = dev->next;
	}

	return -1;
}

static void flow_ndev_reset(struct flow_nic_dev *ndev)
{
	/* Delete all eth-port devices created on this NIC device */
	while (ndev->eth_base)
		flow_delete_eth_dev(ndev->eth_base);

	km_free_ndev_resource_management(&ndev->km_res_handle);
	kcc_free_ndev_resource_management(&ndev->kcc_res_handle);

	ndev->flow_unique_id_counter = 0;

#ifdef FLOW_DEBUG
	/*
	 * free all resources default allocated, initially for this NIC DEV
	 * Is not really needed since the bitmap will be freed in a sec. Therefore
	 * only in debug mode
	 */

	/* Check if all resources has been released */
	NT_LOG(DBG, FILTER, "Delete NIC DEV Adaptor %i", ndev->adapter_no);

	for (unsigned int i = 0; i < RES_COUNT; i++) {
		int err = 0;
#if defined(FLOW_DEBUG)
		NT_LOG(DBG, FILTER, "RES state for: %s", dbg_res_descr[i]);
#endif

		for (unsigned int ii = 0; ii < ndev->res[i].resource_count; ii++) {
			int ref = ndev->res[i].ref[ii];
			int used = flow_nic_is_resource_used(ndev, i, ii);

			if (ref || used) {
				NT_LOG(DBG, FILTER, "  [%i]: ref cnt %i, used %i", ii, ref,
					used);
				err = 1;
			}
		}

		if (err)
			NT_LOG(DBG, FILTER, "ERROR - some resources not freed");
	}

#endif
}

int flow_delete_eth_dev(struct flow_eth_dev *eth_dev)
{
	struct flow_nic_dev *ndev = eth_dev->ndev;

	if (!ndev) {
		/* Error invalid nic device */
		return -1;
	}

	NT_LOG(DBG, FILTER, "Delete eth-port device %p, port %i", eth_dev, eth_dev->port);

#ifdef FLOW_DEBUG
	ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_WRITE);
#endif

	/* delete all created flows from this device */
	pthread_mutex_lock(&ndev->mtx);

	/*
	 * remove unmatched queue if setup in QSL
	 * remove exception queue setting in QSL UNM
	 */
	hw_mod_qsl_unmq_set(&ndev->be, HW_QSL_UNMQ_DEST_QUEUE, eth_dev->port, 0);
	hw_mod_qsl_unmq_set(&ndev->be, HW_QSL_UNMQ_EN, eth_dev->port, 0);
	hw_mod_qsl_unmq_flush(&ndev->be, eth_dev->port, 1);

#ifdef FLOW_DEBUG
	ndev->be.iface->set_debug_mode(ndev->be.be_dev, FLOW_BACKEND_DEBUG_MODE_NONE);
#endif

#ifndef SCATTER_GATHER

	/* free rx queues */
	for (int i = 0; i < eth_dev->num_queues; i++) {
		ndev->be.iface->free_rx_queue(ndev->be.be_dev, eth_dev->rx_queue[i].hw_id);
		flow_nic_deref_resource(ndev, RES_QUEUE, eth_dev->rx_queue[i].id);
	}

#endif

	/* take eth_dev out of ndev list */
	if (nic_remove_eth_port_dev(ndev, eth_dev) != 0)
		NT_LOG(ERR, FILTER, "ERROR : eth_dev %p not found", eth_dev);

	pthread_mutex_unlock(&ndev->mtx);

	/* free eth_dev */
	free(eth_dev);

	return 0;
}

/*
 * Flow API NIC Setup
 * Flow backend creation function - register and initialize common backend API to FPA modules
 */

static int init_resource_elements(struct flow_nic_dev *ndev, enum res_type_e res_type,
	uint32_t count)
{
	assert(ndev->res[res_type].alloc_bm == NULL);
	/* allocate bitmap and ref counter */
	ndev->res[res_type].alloc_bm =
		calloc(1, BIT_CONTAINER_8_ALIGN(count) + count * sizeof(uint32_t));

	if (ndev->res[res_type].alloc_bm) {
		ndev->res[res_type].ref =
			(uint32_t *)&ndev->res[res_type].alloc_bm[BIT_CONTAINER_8_ALIGN(count)];
		ndev->res[res_type].resource_count = count;
		return 0;
	}

	return -1;
}

static void done_resource_elements(struct flow_nic_dev *ndev, enum res_type_e res_type)
{
	assert(ndev);

	if (ndev->res[res_type].alloc_bm)
		free(ndev->res[res_type].alloc_bm);
}

static void list_insert_flow_nic(struct flow_nic_dev *ndev)
{
	pthread_mutex_lock(&base_mtx);
	ndev->next = dev_base;
	dev_base = ndev;
	pthread_mutex_unlock(&base_mtx);
}

static int list_remove_flow_nic(struct flow_nic_dev *ndev)
{
	pthread_mutex_lock(&base_mtx);
	struct flow_nic_dev *nic_dev = dev_base, *prev = NULL;

	while (nic_dev) {
		if (nic_dev == ndev) {
			if (prev)
				prev->next = nic_dev->next;

			else
				dev_base = nic_dev->next;

			pthread_mutex_unlock(&base_mtx);
			return 0;
		}

		prev = nic_dev;
		nic_dev = nic_dev->next;
	}

	pthread_mutex_unlock(&base_mtx);
	return -1;
}

struct flow_nic_dev *flow_api_create(uint8_t adapter_no, const struct flow_api_backend_ops *be_if,
	void *be_dev)
{
	(void)adapter_no;

	if (!be_if || be_if->version != 1) {
		NT_LOG(DBG, FILTER, "ERR: %s", __func__);
		return NULL;
	}

	struct flow_nic_dev *ndev = calloc(1, sizeof(struct flow_nic_dev));

	if (!ndev) {
		NT_LOG(ERR, FILTER, "ERROR: calloc failed");
		return NULL;
	}

	/*
	 * To dump module initialization writes use
	 * FLOW_BACKEND_DEBUG_MODE_WRITE
	 * then remember to set it ...NONE afterwards again
	 */
	be_if->set_debug_mode(be_dev, FLOW_BACKEND_DEBUG_MODE_NONE);

	if (flow_api_backend_init(&ndev->be, be_if, be_dev) != 0)
		goto err_exit;

	ndev->adapter_no = adapter_no;

	ndev->ports = (uint16_t)((ndev->be.num_rx_ports > 256) ? 256 : ndev->be.num_rx_ports);

	/*
	 * Free resources in NIC must be managed by this module
	 * Get resource sizes and create resource manager elements
	 */
	if (init_resource_elements(ndev, RES_QUEUE, ndev->be.max_queues))
		goto err_exit;

	if (init_resource_elements(ndev, RES_CAT_CFN, ndev->be.cat.nb_cat_funcs))
		goto err_exit;

	if (init_resource_elements(ndev, RES_CAT_COT, ndev->be.max_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_CAT_EXO, ndev->be.cat.nb_pm_ext))
		goto err_exit;

	if (init_resource_elements(ndev, RES_CAT_LEN, ndev->be.cat.nb_len))
		goto err_exit;

	if (init_resource_elements(ndev, RES_KM_FLOW_TYPE, ndev->be.cat.nb_flow_types))
		goto err_exit;

	if (init_resource_elements(ndev, RES_KM_CATEGORY, ndev->be.km.nb_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_HSH_RCP, ndev->be.hsh.nb_rcp))
		goto err_exit;

	if (init_resource_elements(ndev, RES_PDB_RCP, ndev->be.pdb.nb_pdb_rcp_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_QSL_RCP, ndev->be.qsl.nb_rcp_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_QSL_QST, ndev->be.qsl.nb_qst_entries))
		goto err_exit;

	if (init_resource_elements(ndev, RES_SLC_LR_RCP, ndev->be.max_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_FLM_FLOW_TYPE, ndev->be.cat.nb_flow_types))
		goto err_exit;

	if (init_resource_elements(ndev, RES_FLM_RCP, ndev->be.flm.nb_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_TPE_RCP, ndev->be.tpe.nb_rcp_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_TPE_EXT, ndev->be.tpe.nb_rpl_ext_categories))
		goto err_exit;

	if (init_resource_elements(ndev, RES_TPE_RPL, ndev->be.tpe.nb_rpl_depth))
		goto err_exit;

	if (init_resource_elements(ndev, RES_SCRUB_RCP, ndev->be.flm.nb_scrub_profiles))
		goto err_exit;

	/* may need IPF, COR */

	/* check all defined has been initialized */
	for (int i = 0; i < RES_COUNT; i++)
		assert(ndev->res[i].alloc_bm);

	pthread_mutex_init(&ndev->mtx, NULL);
	list_insert_flow_nic(ndev);

	return ndev;

err_exit:

	if (ndev)
		flow_api_done(ndev);

	NT_LOG(DBG, FILTER, "ERR: %s", __func__);
	return NULL;
}

int flow_api_done(struct flow_nic_dev *ndev)
{
	NT_LOG(DBG, FILTER, "FLOW API DONE");

	if (ndev) {
		flow_ndev_reset(ndev);

		/* delete resource management allocations for this ndev */
		for (int i = 0; i < RES_COUNT; i++)
			done_resource_elements(ndev, i);

		flow_api_backend_done(&ndev->be);
		list_remove_flow_nic(ndev);
		free(ndev);
	}

	return 0;
}

void *flow_api_get_be_dev(struct flow_nic_dev *ndev)
{
	if (!ndev) {
		NT_LOG(DBG, FILTER, "ERR: %s", __func__);
		return NULL;
	}

	return ndev->be.be_dev;
}

static const struct flow_filter_ops ops = {
	.flow_filter_init = flow_filter_init,
	.flow_filter_done = flow_filter_done,
};

void init_flow_filter(void)
{
	register_flow_filter_ops(&ops);
}
