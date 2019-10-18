#include <stdio.h>
#include <rte_flow.h>
#include "base/ice_fdir.h"
#include "base/ice_flow.h"
#include "base/ice_type.h"
#include "ice_ethdev.h"
#include "ice_rxtx.h"
#include "ice_generic_flow.h"

static const struct rte_memzone *
ice_memzone_reserve(const char *name, uint32_t len, int socket_id)
{
	return rte_memzone_reserve_aligned(name, len, socket_id,
					   RTE_MEMZONE_IOVA_CONTIG,
					   ICE_RING_BASE_ALIGN);
}

#define ICE_FDIR_MZ_NAME	"FDIR_MEMZONE"

static int
ice_fdir_prof_alloc(struct ice_hw *hw)
{
	enum ice_fltr_ptype ptype, fltr_ptype;

	if (!hw->fdir_prof) {
		hw->fdir_prof = (struct ice_fd_hw_prof **)
			ice_malloc(hw, ICE_FLTR_PTYPE_MAX *
				   sizeof(*hw->fdir_prof));
		if (!hw->fdir_prof)
			return -ENOMEM;
	}
	for (ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
	     ptype < ICE_FLTR_PTYPE_MAX;
	     ptype++) {
		if (!hw->fdir_prof[ptype]) {
			hw->fdir_prof[ptype] = (struct ice_fd_hw_prof *)
				ice_malloc(hw, sizeof(**hw->fdir_prof));
			if (!hw->fdir_prof[ptype])
				goto fail_mem;
		}
	}
	return 0;

fail_mem:
	for (fltr_ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
	     fltr_ptype < ptype;
	     fltr_ptype++)
		rte_free(hw->fdir_prof[fltr_ptype]);
	rte_free(hw->fdir_prof);
	return -ENOMEM;
}

/*
 * ice_fdir_setup - reserve and initialize the Flow Director resources
 * @pf: board private structure
 */
static int
ice_fdir_setup(struct ice_pf *pf)
{
	struct rte_eth_dev *eth_dev = pf->adapter->eth_dev;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	const struct rte_memzone *mz = NULL;
	char z_name[RTE_MEMZONE_NAMESIZE];
	struct ice_vsi *vsi;
	int err = ICE_SUCCESS;

	if ((pf->flags & ICE_FLAG_FDIR) == 0) {
		PMD_INIT_LOG(ERR, "HW doesn't support FDIR");
		return -ENOTSUP;
	}

	PMD_DRV_LOG(INFO, "FDIR HW Capabilities: fd_fltr_guar = %u,"
		    " fd_fltr_best_effort = %u.",
		    hw->func_caps.fd_fltr_guar,
		    hw->func_caps.fd_fltr_best_effort);

	if (pf->fdir.fdir_vsi) {
		PMD_DRV_LOG(INFO, "FDIR initialization has been done.");
		return ICE_SUCCESS;
	}

	/* make new FDIR VSI */
	vsi = ice_setup_vsi(pf, ICE_VSI_CTRL);
	if (!vsi) {
		PMD_DRV_LOG(ERR, "Couldn't create FDIR VSI.");
		return -EINVAL;
	}
	pf->fdir.fdir_vsi = vsi;

	/*Fdir tx queue setup*/
	err = ice_fdir_setup_tx_resources(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to setup FDIR TX resources.");
		goto fail_setup_tx;
	}

	/*Fdir rx queue setup*/
	err = ice_fdir_setup_rx_resources(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to setup FDIR RX resources.");
		goto fail_setup_rx;
	}

	err = ice_fdir_tx_queue_start(eth_dev, pf->fdir.txq->queue_id);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to start FDIR TX queue.");
		goto fail_mem;
	}

	err = ice_fdir_rx_queue_start(eth_dev, pf->fdir.rxq->queue_id);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to start FDIR RX queue.");
		goto fail_mem;
	}

	/* reserve memory for the fdir programming packet */
	snprintf(z_name, sizeof(z_name), "ICE_%s_%d",
		 ICE_FDIR_MZ_NAME,
		 eth_dev->data->port_id);
	mz = ice_memzone_reserve(z_name, ICE_FDIR_PKT_LEN, SOCKET_ID_ANY);
	if (!mz) {
		PMD_DRV_LOG(ERR, "Cannot init memzone for "
			    "flow director program packet.");
		err = -ENOMEM;
		goto fail_mem;
	}
	pf->fdir.prg_pkt = mz->addr;
	pf->fdir.dma_addr = mz->iova;

	err = ice_fdir_prof_alloc(hw);
	if (err) {
		PMD_DRV_LOG(ERR, "Cannot allocate memory for "
			    "flow director profile.");
		err = -ENOMEM;
		goto fail_mem;
	}

	PMD_DRV_LOG(INFO, "FDIR setup successfully, with programming queue %u.",
		    vsi->base_queue);
	return ICE_SUCCESS;

fail_mem:
	ice_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
fail_setup_rx:
	ice_tx_queue_release(pf->fdir.txq);
	pf->fdir.txq = NULL;
fail_setup_tx:
	ice_release_vsi(vsi);
	pf->fdir.fdir_vsi = NULL;
	return err;
}

static void
ice_fdir_prof_free(struct ice_hw *hw)
{
	enum ice_fltr_ptype ptype;

	for (ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
	     ptype < ICE_FLTR_PTYPE_MAX;
	     ptype++)
		rte_free(hw->fdir_prof[ptype]);

	rte_free(hw->fdir_prof);
}

/*
 * ice_fdir_teardown - release the Flow Director resources
 * @pf: board private structure
 */
static void
ice_fdir_teardown(struct ice_pf *pf)
{
	struct rte_eth_dev *eth_dev = pf->adapter->eth_dev;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi;
	int err;

	vsi = pf->fdir.fdir_vsi;
	if (!vsi)
		return;

	err = ice_fdir_tx_queue_stop(eth_dev, pf->fdir.txq->queue_id);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to stop TX queue.");

	err = ice_fdir_rx_queue_stop(eth_dev, pf->fdir.rxq->queue_id);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to stop RX queue.");

	ice_tx_queue_release(pf->fdir.txq);
	pf->fdir.txq = NULL;
	ice_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
	ice_release_vsi(vsi);
	pf->fdir.fdir_vsi = NULL;
	ice_fdir_prof_free(hw);
}

static int
ice_fdir_init(struct ice_adapter *ad)
{
	struct ice_pf *pf = &ad->pf;

	return ice_fdir_setup(pf);
}

static void
ice_fdir_uninit(struct ice_adapter *ad)
{
	struct ice_pf *pf = &ad->pf;

	ice_fdir_teardown(pf);
}

static struct ice_flow_engine ice_fdir_engine = {
	.init = ice_fdir_init,
	.uninit = ice_fdir_uninit,
	.type = ICE_FLOW_ENGINE_FDIR,
};

RTE_INIT(ice_fdir_engine_register)
{
	ice_register_flow_engine(&ice_fdir_engine);
}
