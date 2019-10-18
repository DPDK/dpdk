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

/* Remove a profile for some filter type */
static void
ice_fdir_prof_rm(struct ice_pf *pf, enum ice_fltr_ptype ptype, bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fd_hw_prof *hw_prof;
	uint64_t prof_id;
	uint16_t vsi_num;
	int i;

	if (!hw->fdir_prof || !hw->fdir_prof[ptype])
		return;

	hw_prof = hw->fdir_prof[ptype];

	prof_id = ptype + is_tunnel * ICE_FLTR_PTYPE_MAX;
	for (i = 0; i < pf->hw_prof_cnt[ptype][is_tunnel]; i++) {
		if (hw_prof->entry_h[i][is_tunnel]) {
			vsi_num = ice_get_hw_vsi_num(hw,
						     hw_prof->vsi_h[i]);
			ice_rem_prof_id_flow(hw, ICE_BLK_FD,
					     vsi_num, ptype);
			ice_flow_rem_entry(hw,
					   hw_prof->entry_h[i][is_tunnel]);
			hw_prof->entry_h[i][is_tunnel] = 0;
		}
	}
	ice_flow_rem_prof(hw, ICE_BLK_FD, prof_id);
	rte_free(hw_prof->fdir_seg[is_tunnel]);
	hw_prof->fdir_seg[is_tunnel] = NULL;

	for (i = 0; i < hw_prof->cnt; i++)
		hw_prof->vsi_h[i] = 0;
	pf->hw_prof_cnt[ptype][is_tunnel] = 0;
}

/* Remove all created profiles */
static void
ice_fdir_prof_rm_all(struct ice_pf *pf)
{
	enum ice_fltr_ptype ptype;

	for (ptype = ICE_FLTR_PTYPE_NONF_NONE;
	     ptype < ICE_FLTR_PTYPE_MAX;
	     ptype++) {
		ice_fdir_prof_rm(pf, ptype, false);
		ice_fdir_prof_rm(pf, ptype, true);
	}
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
	ice_fdir_prof_rm_all(pf);
	ice_fdir_prof_free(hw);
	ice_release_vsi(vsi);
	pf->fdir.fdir_vsi = NULL;
}

static int
ice_fdir_hw_tbl_conf(struct ice_pf *pf, struct ice_vsi *vsi,
		     struct ice_vsi *ctrl_vsi,
		     struct ice_flow_seg_info *seg,
		     enum ice_fltr_ptype ptype,
		     bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	enum ice_flow_dir dir = ICE_FLOW_RX;
	struct ice_flow_seg_info *ori_seg;
	struct ice_fd_hw_prof *hw_prof;
	struct ice_flow_prof *prof;
	uint64_t entry_1 = 0;
	uint64_t entry_2 = 0;
	uint16_t vsi_num;
	int ret;
	uint64_t prof_id;

	hw_prof = hw->fdir_prof[ptype];
	ori_seg = hw_prof->fdir_seg[is_tunnel];
	if (ori_seg) {
		if (!is_tunnel) {
			if (!memcmp(ori_seg, seg, sizeof(*seg)))
				return -EAGAIN;
		} else {
			if (!memcmp(ori_seg, &seg[1], sizeof(*seg)))
				return -EAGAIN;
		}

		if (pf->fdir_fltr_cnt[ptype][is_tunnel])
			return -EINVAL;

		ice_fdir_prof_rm(pf, ptype, is_tunnel);
	}

	prof_id = ptype + is_tunnel * ICE_FLTR_PTYPE_MAX;
	ret = ice_flow_add_prof(hw, ICE_BLK_FD, dir, prof_id, seg,
				(is_tunnel) ? 2 : 1, NULL, 0, &prof);
	if (ret)
		return ret;
	ret = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id, vsi->idx,
				 vsi->idx, ICE_FLOW_PRIO_NORMAL,
				 seg, NULL, 0, &entry_1);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to add main VSI flow entry for %d.",
			    ptype);
		goto err_add_prof;
	}
	ret = ice_flow_add_entry(hw, ICE_BLK_FD, prof_id, vsi->idx,
				 ctrl_vsi->idx, ICE_FLOW_PRIO_NORMAL,
				 seg, NULL, 0, &entry_2);
	if (ret) {
		PMD_DRV_LOG(ERR, "Failed to add control VSI flow entry for %d.",
			    ptype);
		goto err_add_entry;
	}

	pf->hw_prof_cnt[ptype][is_tunnel] = 0;
	hw_prof->cnt = 0;
	hw_prof->fdir_seg[is_tunnel] = seg;
	hw_prof->vsi_h[hw_prof->cnt] = vsi->idx;
	hw_prof->entry_h[hw_prof->cnt++][is_tunnel] = entry_1;
	pf->hw_prof_cnt[ptype][is_tunnel]++;
	hw_prof->vsi_h[hw_prof->cnt] = ctrl_vsi->idx;
	hw_prof->entry_h[hw_prof->cnt++][is_tunnel] = entry_2;
	pf->hw_prof_cnt[ptype][is_tunnel]++;

	return ret;

err_add_entry:
	vsi_num = ice_get_hw_vsi_num(hw, vsi->idx);
	ice_rem_prof_id_flow(hw, ICE_BLK_FD, vsi_num, prof_id);
	ice_flow_rem_entry(hw, entry_1);
err_add_prof:
	ice_flow_rem_prof(hw, ICE_BLK_FD, prof_id);

	return ret;
}

static void
ice_fdir_input_set_parse(uint64_t inset, enum ice_flow_field *field)
{
	uint32_t i, j;

	struct ice_inset_map {
		uint64_t inset;
		enum ice_flow_field fld;
	};
	static const struct ice_inset_map ice_inset_map[] = {
		{ICE_INSET_DMAC, ICE_FLOW_FIELD_IDX_ETH_DA},
		{ICE_INSET_IPV4_SRC, ICE_FLOW_FIELD_IDX_IPV4_SA},
		{ICE_INSET_IPV4_DST, ICE_FLOW_FIELD_IDX_IPV4_DA},
		{ICE_INSET_IPV4_TOS, ICE_FLOW_FIELD_IDX_IPV4_DSCP},
		{ICE_INSET_IPV4_TTL, ICE_FLOW_FIELD_IDX_IPV4_TTL},
		{ICE_INSET_IPV4_PROTO, ICE_FLOW_FIELD_IDX_IPV4_PROT},
		{ICE_INSET_IPV6_SRC, ICE_FLOW_FIELD_IDX_IPV6_SA},
		{ICE_INSET_IPV6_DST, ICE_FLOW_FIELD_IDX_IPV6_DA},
		{ICE_INSET_IPV6_TC, ICE_FLOW_FIELD_IDX_IPV6_DSCP},
		{ICE_INSET_IPV6_NEXT_HDR, ICE_FLOW_FIELD_IDX_IPV6_PROT},
		{ICE_INSET_IPV6_HOP_LIMIT, ICE_FLOW_FIELD_IDX_IPV6_TTL},
		{ICE_INSET_TCP_SRC_PORT, ICE_FLOW_FIELD_IDX_TCP_SRC_PORT},
		{ICE_INSET_TCP_DST_PORT, ICE_FLOW_FIELD_IDX_TCP_DST_PORT},
		{ICE_INSET_UDP_SRC_PORT, ICE_FLOW_FIELD_IDX_UDP_SRC_PORT},
		{ICE_INSET_UDP_DST_PORT, ICE_FLOW_FIELD_IDX_UDP_DST_PORT},
		{ICE_INSET_SCTP_SRC_PORT, ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT},
		{ICE_INSET_SCTP_DST_PORT, ICE_FLOW_FIELD_IDX_SCTP_DST_PORT},
	};

	for (i = 0, j = 0; i < RTE_DIM(ice_inset_map); i++) {
		if ((inset & ice_inset_map[i].inset) ==
		    ice_inset_map[i].inset)
			field[j++] = ice_inset_map[i].fld;
	}
}

static int __rte_unused
ice_fdir_input_set_conf(struct ice_pf *pf, enum ice_fltr_ptype flow,
			uint64_t input_set, bool is_tunnel)
{
	struct ice_flow_seg_info *seg;
	struct ice_flow_seg_info *seg_tun = NULL;
	enum ice_flow_field field[ICE_FLOW_FIELD_IDX_MAX];
	int i, ret;

	if (!input_set)
		return -EINVAL;

	seg = (struct ice_flow_seg_info *)
		ice_malloc(hw, sizeof(*seg));
	if (!seg) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}

	for (i = 0; i < ICE_FLOW_FIELD_IDX_MAX; i++)
		field[i] = ICE_FLOW_FIELD_IDX_MAX;
	ice_fdir_input_set_parse(input_set, field);

	switch (flow) {
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_UDP |
				  ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_TCP |
				  ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_SCTP |
				  ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV4);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_UDP |
				  ICE_FLOW_SEG_HDR_IPV6);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_TCP |
				  ICE_FLOW_SEG_HDR_IPV6);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_SCTP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_SCTP |
				  ICE_FLOW_SEG_HDR_IPV6);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_OTHER:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV6);
		break;
	default:
		PMD_DRV_LOG(ERR, "not supported filter type.");
		break;
	}

	for (i = 0; field[i] != ICE_FLOW_FIELD_IDX_MAX; i++) {
		ice_flow_set_fld(seg, field[i],
				 ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL,
				 ICE_FLOW_FLD_OFF_INVAL, false);
	}

	if (!is_tunnel) {
		ret = ice_fdir_hw_tbl_conf(pf, pf->main_vsi, pf->fdir.fdir_vsi,
					   seg, flow, false);
	} else {
		seg_tun = (struct ice_flow_seg_info *)
			ice_malloc(hw, sizeof(*seg) * ICE_FD_HW_SEG_MAX);
		if (!seg_tun) {
			PMD_DRV_LOG(ERR, "No memory can be allocated");
			rte_free(seg);
			return -ENOMEM;
		}
		rte_memcpy(&seg_tun[1], seg, sizeof(*seg));
		ret = ice_fdir_hw_tbl_conf(pf, pf->main_vsi, pf->fdir.fdir_vsi,
					   seg_tun, flow, true);
	}

	if (!ret) {
		return ret;
	} else if (ret < 0) {
		rte_free(seg);
		if (is_tunnel)
			rte_free(seg_tun);
		return (ret == -EAGAIN) ? 0 : ret;
	} else {
		return ret;
	}
}

static void __rte_unused
ice_fdir_cnt_update(struct ice_pf *pf, enum ice_fltr_ptype ptype,
		    bool is_tunnel, bool add)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	int cnt;

	cnt = (add) ? 1 : -1;
	hw->fdir_active_fltr += cnt;
	if (ptype == ICE_FLTR_PTYPE_NONF_NONE || ptype >= ICE_FLTR_PTYPE_MAX)
		PMD_DRV_LOG(ERR, "Unknown filter type %d", ptype);
	else
		pf->fdir_fltr_cnt[ptype][is_tunnel] += cnt;
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
