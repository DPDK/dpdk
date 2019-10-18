#include <stdio.h>
#include <rte_flow.h>
#include "base/ice_fdir.h"
#include "base/ice_flow.h"
#include "base/ice_type.h"
#include "ice_ethdev.h"
#include "ice_rxtx.h"
#include "ice_generic_flow.h"

#define ICE_FDIR_IPV6_TC_OFFSET		20
#define ICE_IPV6_TC_MASK		(0xFF << ICE_FDIR_IPV6_TC_OFFSET)

#define ICE_FDIR_MAX_QREGION_SIZE	128

#define ICE_FDIR_INSET_ETH_IPV4 (\
	ICE_INSET_DMAC | \
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | ICE_INSET_IPV4_TOS | \
	ICE_INSET_IPV4_TTL | ICE_INSET_IPV4_PROTO)

#define ICE_FDIR_INSET_ETH_IPV4_UDP (\
	ICE_FDIR_INSET_ETH_IPV4 | \
	ICE_INSET_UDP_SRC_PORT | ICE_INSET_UDP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV4_TCP (\
	ICE_FDIR_INSET_ETH_IPV4 | \
	ICE_INSET_TCP_SRC_PORT | ICE_INSET_TCP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV4_SCTP (\
	ICE_FDIR_INSET_ETH_IPV4 | \
	ICE_INSET_SCTP_SRC_PORT | ICE_INSET_SCTP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6 (\
	ICE_INSET_IPV6_SRC | ICE_INSET_IPV6_DST | ICE_INSET_IPV6_TC | \
	ICE_INSET_IPV6_HOP_LIMIT | ICE_INSET_IPV6_NEXT_HDR)

#define ICE_FDIR_INSET_ETH_IPV6_UDP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_UDP_SRC_PORT | ICE_INSET_UDP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6_TCP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_TCP_SRC_PORT | ICE_INSET_TCP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6_SCTP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_SCTP_SRC_PORT | ICE_INSET_SCTP_DST_PORT)

static struct ice_pattern_match_item ice_fdir_pattern[] = {
	{pattern_eth_ipv4,             ICE_FDIR_INSET_ETH_IPV4,              ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,         ICE_FDIR_INSET_ETH_IPV4_UDP,          ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,         ICE_FDIR_INSET_ETH_IPV4_TCP,          ICE_INSET_NONE},
	{pattern_eth_ipv4_sctp,        ICE_FDIR_INSET_ETH_IPV4_SCTP,         ICE_INSET_NONE},
	{pattern_eth_ipv6,             ICE_FDIR_INSET_ETH_IPV6,              ICE_INSET_NONE},
	{pattern_eth_ipv6_udp,         ICE_FDIR_INSET_ETH_IPV6_UDP,          ICE_INSET_NONE},
	{pattern_eth_ipv6_tcp,         ICE_FDIR_INSET_ETH_IPV6_TCP,          ICE_INSET_NONE},
	{pattern_eth_ipv6_sctp,        ICE_FDIR_INSET_ETH_IPV6_SCTP,         ICE_INSET_NONE},
};

static struct ice_flow_parser ice_fdir_parser;

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

static int
ice_fdir_counter_pool_add(__rte_unused struct ice_pf *pf,
			  struct ice_fdir_counter_pool_container *container,
			  uint32_t index_start,
			  uint32_t len)
{
	struct ice_fdir_counter_pool *pool;
	uint32_t i;
	int ret = 0;

	pool = rte_zmalloc("ice_fdir_counter_pool",
			   sizeof(*pool) +
			   sizeof(struct ice_fdir_counter) * len,
			   0);
	if (!pool) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for fdir counter pool");
		return -ENOMEM;
	}

	TAILQ_INIT(&pool->counter_list);
	TAILQ_INSERT_TAIL(&container->pool_list, pool, next);

	for (i = 0; i < len; i++) {
		struct ice_fdir_counter *counter = &pool->counters[i];

		counter->hw_index = index_start + i;
		TAILQ_INSERT_TAIL(&pool->counter_list, counter, next);
	}

	if (container->index_free == ICE_FDIR_COUNTER_MAX_POOL_SIZE) {
		PMD_INIT_LOG(ERR, "FDIR counter pool is full");
		ret = -EINVAL;
		goto free_pool;
	}

	container->pools[container->index_free++] = pool;
	return 0;

free_pool:
	rte_free(pool);
	return ret;
}

static int
ice_fdir_counter_init(struct ice_pf *pf)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_counter_pool_container *container =
				&fdir_info->counter;
	uint32_t cnt_index, len;
	int ret;

	TAILQ_INIT(&container->pool_list);

	cnt_index = ICE_FDIR_COUNTER_INDEX(hw->fd_ctr_base);
	len = ICE_FDIR_COUNTERS_PER_BLOCK;

	ret = ice_fdir_counter_pool_add(pf, container, cnt_index, len);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failed to add fdir pool to container");
		return ret;
	}

	return 0;
}

static int
ice_fdir_counter_release(struct ice_pf *pf)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_counter_pool_container *container =
				&fdir_info->counter;
	uint8_t i;

	for (i = 0; i < container->index_free; i++)
		rte_free(container->pools[i]);

	return 0;
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

	err = ice_fdir_counter_init(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to init FDIR counter.");
		return -EINVAL;
	}

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

	err = ice_fdir_counter_release(pf);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to release FDIR counter resource.");

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

static int
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

static void
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
	int ret;

	ret = ice_fdir_setup(pf);
	if (ret)
		return ret;

	return ice_register_parser(&ice_fdir_parser, ad);
}

static void
ice_fdir_uninit(struct ice_adapter *ad)
{
	struct ice_pf *pf = &ad->pf;

	ice_unregister_parser(&ice_fdir_parser, ad);

	ice_fdir_teardown(pf);
}

static int
ice_fdir_add_del_filter(struct ice_pf *pf,
			struct ice_fdir_filter_conf *filter,
			bool add)
{
	struct ice_fltr_desc desc;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	unsigned char *pkt = (unsigned char *)pf->fdir.prg_pkt;
	int ret;

	filter->input.dest_vsi = pf->main_vsi->idx;

	memset(&desc, 0, sizeof(desc));
	ice_fdir_get_prgm_desc(hw, &filter->input, &desc, add);

	memset(pkt, 0, ICE_FDIR_PKT_LEN);
	ret = ice_fdir_get_prgm_pkt(&filter->input, pkt, false);
	if (ret) {
		PMD_DRV_LOG(ERR, "Generate dummy packet failed");
		return -EINVAL;
	}

	return ice_fdir_programming(pf, &desc);
}

static int
ice_fdir_create_filter(struct ice_adapter *ad,
		       struct rte_flow *flow,
		       void *meta,
		       struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_filter_conf *filter = meta;
	struct ice_fdir_filter_conf *rule;
	int ret;

	rule = rte_zmalloc("fdir_entry", sizeof(*rule), 0);
	if (!rule) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return -rte_errno;
	}

	ret = ice_fdir_input_set_conf(pf, filter->input.flow_type,
			filter->input_set, false);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Profile configure failed.");
		goto free_entry;
	}

	ret = ice_fdir_add_del_filter(pf, filter, true);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Add filter rule failed.");
		goto free_entry;
	}

	rte_memcpy(rule, filter, sizeof(*rule));
	flow->rule = rule;
	ice_fdir_cnt_update(pf, filter->input.flow_type, false, true);
	return 0;

free_entry:
	rte_free(rule);
	return -rte_errno;
}

static int
ice_fdir_destroy_filter(struct ice_adapter *ad,
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_filter_conf *filter;
	int ret;

	filter = (struct ice_fdir_filter_conf *)flow->rule;

	ret = ice_fdir_add_del_filter(pf, filter, false);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Del filter rule failed.");
		return -rte_errno;
	}

	ice_fdir_cnt_update(pf, filter->input.flow_type, false, false);
	flow->rule = NULL;

	rte_free(filter);

	return 0;
}

static struct ice_flow_engine ice_fdir_engine = {
	.init = ice_fdir_init,
	.uninit = ice_fdir_uninit,
	.create = ice_fdir_create_filter,
	.destroy = ice_fdir_destroy_filter,
	.type = ICE_FLOW_ENGINE_FDIR,
};

static int
ice_fdir_parse_action_qregion(struct ice_pf *pf,
			      struct rte_flow_error *error,
			      const struct rte_flow_action *act,
			      struct ice_fdir_filter_conf *filter)
{
	const struct rte_flow_action_rss *rss = act->conf;
	uint32_t i;

	if (act->type != RTE_FLOW_ACTION_TYPE_RSS) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Invalid action.");
		return -rte_errno;
	}

	if (rss->queue_num <= 1) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Queue region size can't be 0 or 1.");
		return -rte_errno;
	}

	/* check if queue index for queue region is continuous */
	for (i = 0; i < rss->queue_num - 1; i++) {
		if (rss->queue[i + 1] != rss->queue[i] + 1) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ACTION, act,
					   "Discontinuous queue region");
			return -rte_errno;
		}
	}

	if (rss->queue[rss->queue_num - 1] >= pf->dev_data->nb_rx_queues) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "Invalid queue region indexes.");
		return -rte_errno;
	}

	if (!(rte_is_power_of_2(rss->queue_num) &&
	     (rss->queue_num <= ICE_FDIR_MAX_QREGION_SIZE))) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, act,
				   "The region size should be any of the following values:"
				   "1, 2, 4, 8, 16, 32, 64, 128 as long as the total number "
				   "of queues do not exceed the VSI allocation.");
		return -rte_errno;
	}

	filter->input.q_index = rss->queue[0];
	filter->input.q_region = rte_fls_u32(rss->queue_num) - 1;
	filter->input.dest_ctl = ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QGROUP;

	return 0;
}

static int
ice_fdir_parse_action(struct ice_adapter *ad,
		      const struct rte_flow_action actions[],
		      struct rte_flow_error *error,
		      struct ice_fdir_filter_conf *filter)
{
	struct ice_pf *pf = &ad->pf;
	const struct rte_flow_action_queue *act_q;
	const struct rte_flow_action_mark *mark_spec = NULL;
	uint32_t dest_num = 0;
	uint32_t mark_num = 0;
	int ret;

	for (; actions->type != RTE_FLOW_ACTION_TYPE_END; actions++) {
		switch (actions->type) {
		case RTE_FLOW_ACTION_TYPE_VOID:
			break;
		case RTE_FLOW_ACTION_TYPE_QUEUE:
			dest_num++;

			act_q = actions->conf;
			filter->input.q_index = act_q->index;
			if (filter->input.q_index >=
					pf->dev_data->nb_rx_queues) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ACTION,
						   actions,
						   "Invalid queue for FDIR.");
				return -rte_errno;
			}
			filter->input.dest_ctl =
				ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QINDEX;
			break;
		case RTE_FLOW_ACTION_TYPE_DROP:
			dest_num++;

			filter->input.dest_ctl =
				ICE_FLTR_PRGM_DESC_DEST_DROP_PKT;
			break;
		case RTE_FLOW_ACTION_TYPE_PASSTHRU:
			dest_num++;

			filter->input.dest_ctl =
				ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_QINDEX;
			filter->input.q_index = 0;
			break;
		case RTE_FLOW_ACTION_TYPE_RSS:
			dest_num++;

			ret = ice_fdir_parse_action_qregion(pf,
						error, actions, filter);
			if (ret)
				return ret;
			break;
		case RTE_FLOW_ACTION_TYPE_MARK:
			mark_num++;

			mark_spec = actions->conf;
			filter->input.fltr_id = mark_spec->id;
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Invalid action.");
			return -rte_errno;
		}
	}

	if (dest_num == 0 || dest_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Unsupported action combination");
		return -rte_errno;
	}

	if (mark_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Too many mark actions");
		return -rte_errno;
	}

	return 0;
}

static int
ice_fdir_parse_pattern(__rte_unused struct ice_adapter *ad,
		       const struct rte_flow_item pattern[],
		       struct rte_flow_error *error,
		       struct ice_fdir_filter_conf *filter)
{
	const struct rte_flow_item *item = pattern;
	enum rte_flow_item_type item_type;
	enum rte_flow_item_type l3 = RTE_FLOW_ITEM_TYPE_END;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	uint64_t input_set = ICE_INSET_NONE;
	uint8_t flow_type = ICE_FLTR_PTYPE_NONF_NONE;
	uint8_t  ipv6_addr_mask[16] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	uint32_t vtc_flow_cpu;


	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->last) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ITEM,
					item,
					"Not support range");
			return -rte_errno;
		}
		item_type = item->type;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_ETH:
			eth_spec = item->spec;
			eth_mask = item->mask;

			if (eth_spec && eth_mask) {
				if (!rte_is_zero_ether_addr(&eth_spec->src) ||
				    !rte_is_zero_ether_addr(&eth_mask->src)) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Src mac not support");
					return -rte_errno;
				}

				if (!rte_is_broadcast_ether_addr(&eth_mask->dst)) {
					rte_flow_error_set(error, EINVAL,
						RTE_FLOW_ERROR_TYPE_ITEM,
						item,
						"Invalid mac addr mask");
					return -rte_errno;
				}

				input_set |= ICE_INSET_DMAC;
				rte_memcpy(&filter->input.ext_data.dst_mac,
					   &eth_spec->dst,
					   RTE_ETHER_ADDR_LEN);
			}
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			l3 = RTE_FLOW_ITEM_TYPE_IPV4;
			ipv4_spec = item->spec;
			ipv4_mask = item->mask;

			if (ipv4_spec && ipv4_mask) {
				/* Check IPv4 mask and update input set */
				if (ipv4_mask->hdr.version_ihl ||
				    ipv4_mask->hdr.total_length ||
				    ipv4_mask->hdr.packet_id ||
				    ipv4_mask->hdr.fragment_offset ||
				    ipv4_mask->hdr.hdr_checksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 mask.");
					return -rte_errno;
				}
				if (ipv4_mask->hdr.src_addr == UINT32_MAX)
					input_set |= ICE_INSET_IPV4_SRC;
				if (ipv4_mask->hdr.dst_addr == UINT32_MAX)
					input_set |= ICE_INSET_IPV4_DST;
				if (ipv4_mask->hdr.type_of_service == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_TOS;
				if (ipv4_mask->hdr.time_to_live == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_TTL;
				if (ipv4_mask->hdr.next_proto_id == UINT8_MAX)
					input_set |= ICE_INSET_IPV4_PROTO;

				filter->input.ip.v4.dst_ip =
					ipv4_spec->hdr.src_addr;
				filter->input.ip.v4.src_ip =
					ipv4_spec->hdr.dst_addr;
				filter->input.ip.v4.tos =
					ipv4_spec->hdr.type_of_service;
				filter->input.ip.v4.ttl =
					ipv4_spec->hdr.time_to_live;
				filter->input.ip.v4.proto =
					ipv4_spec->hdr.next_proto_id;
			}

			flow_type = ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			l3 = RTE_FLOW_ITEM_TYPE_IPV6;
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;

			if (ipv6_spec && ipv6_mask) {
				/* Check IPv6 mask and update input set */
				if (ipv6_mask->hdr.payload_len) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 mask");
					return -rte_errno;
				}

				if (!memcmp(ipv6_mask->hdr.src_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.src_addr)))
					input_set |= ICE_INSET_IPV6_SRC;
				if (!memcmp(ipv6_mask->hdr.dst_addr,
					    ipv6_addr_mask,
					    RTE_DIM(ipv6_mask->hdr.dst_addr)))
					input_set |= ICE_INSET_IPV6_DST;

				if ((ipv6_mask->hdr.vtc_flow &
				     rte_cpu_to_be_32(ICE_IPV6_TC_MASK))
				    == rte_cpu_to_be_32(ICE_IPV6_TC_MASK))
					input_set |= ICE_INSET_IPV6_TC;
				if (ipv6_mask->hdr.proto == UINT8_MAX)
					input_set |= ICE_INSET_IPV6_NEXT_HDR;
				if (ipv6_mask->hdr.hop_limits == UINT8_MAX)
					input_set |= ICE_INSET_IPV6_HOP_LIMIT;

				rte_memcpy(filter->input.ip.v6.dst_ip,
					   ipv6_spec->hdr.src_addr, 16);
				rte_memcpy(filter->input.ip.v6.src_ip,
					   ipv6_spec->hdr.dst_addr, 16);

				vtc_flow_cpu =
				      rte_be_to_cpu_32(ipv6_spec->hdr.vtc_flow);
				filter->input.ip.v6.tc =
					(uint8_t)(vtc_flow_cpu >>
						  ICE_FDIR_IPV6_TC_OFFSET);
				filter->input.ip.v6.proto =
					ipv6_spec->hdr.proto;
				filter->input.ip.v6.hlim =
					ipv6_spec->hdr.hop_limits;
			}

			flow_type = ICE_FLTR_PTYPE_NONF_IPV6_OTHER;
			break;
		case RTE_FLOW_ITEM_TYPE_TCP:
			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (tcp_spec && tcp_mask) {
				/* Check TCP mask and update input set */
				if (tcp_mask->hdr.sent_seq ||
				    tcp_mask->hdr.recv_ack ||
				    tcp_mask->hdr.data_off ||
				    tcp_mask->hdr.tcp_flags ||
				    tcp_mask->hdr.rx_win ||
				    tcp_mask->hdr.cksum ||
				    tcp_mask->hdr.tcp_urp) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid TCP mask");
					return -rte_errno;
				}

				if (tcp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_TCP_SRC_PORT;
				if (tcp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_TCP_DST_PORT;

				/* Get filter info */
				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
					filter->input.ip.v4.dst_port =
						tcp_spec->hdr.src_port;
					filter->input.ip.v4.src_port =
						tcp_spec->hdr.dst_port;
					flow_type =
						ICE_FLTR_PTYPE_NONF_IPV4_TCP;
				} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
					filter->input.ip.v6.dst_port =
						tcp_spec->hdr.src_port;
					filter->input.ip.v6.src_port =
						tcp_spec->hdr.dst_port;
					flow_type =
						ICE_FLTR_PTYPE_NONF_IPV6_TCP;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			udp_spec = item->spec;
			udp_mask = item->mask;

			if (udp_spec && udp_mask) {
				/* Check UDP mask and update input set*/
				if (udp_mask->hdr.dgram_len ||
				    udp_mask->hdr.dgram_cksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
					return -rte_errno;
				}

				if (udp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_UDP_SRC_PORT;
				if (udp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_UDP_DST_PORT;

				/* Get filter info */
				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
					filter->input.ip.v4.dst_port =
						udp_spec->hdr.src_port;
					filter->input.ip.v4.src_port =
						udp_spec->hdr.dst_port;
					flow_type =
						ICE_FLTR_PTYPE_NONF_IPV4_UDP;
				} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
					filter->input.ip.v6.src_port =
						udp_spec->hdr.src_port;
					filter->input.ip.v6.dst_port =
						udp_spec->hdr.dst_port;
					flow_type =
						ICE_FLTR_PTYPE_NONF_IPV6_UDP;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (sctp_spec && sctp_mask) {
				/* Check SCTP mask and update input set */
				if (sctp_mask->hdr.cksum) {
					rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
					return -rte_errno;
				}

				if (sctp_mask->hdr.src_port == UINT16_MAX)
					input_set |= ICE_INSET_SCTP_SRC_PORT;
				if (sctp_mask->hdr.dst_port == UINT16_MAX)
					input_set |= ICE_INSET_SCTP_DST_PORT;

				/* Get filter info */
				if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
					filter->input.ip.v4.dst_port =
						sctp_spec->hdr.src_port;
					filter->input.ip.v4.src_port =
						sctp_spec->hdr.dst_port;
					flow_type =
						ICE_FLTR_PTYPE_NONF_IPV4_SCTP;
				} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
					filter->input.ip.v6.dst_port =
						sctp_spec->hdr.src_port;
					filter->input.ip.v6.src_port =
						sctp_spec->hdr.dst_port;
					flow_type =
						ICE_FLTR_PTYPE_NONF_IPV6_SCTP;
				}
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM,
				   item,
				   "Invalid pattern item.");
			return -rte_errno;
		}
	}

	filter->input.flow_type = flow_type;
	filter->input_set = input_set;

	return 0;
}

static int
ice_fdir_parse(struct ice_adapter *ad,
	       struct ice_pattern_match_item *array,
	       uint32_t array_len,
	       const struct rte_flow_item pattern[],
	       const struct rte_flow_action actions[],
	       void **meta,
	       struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_filter_conf *filter = &pf->fdir.conf;
	struct ice_pattern_match_item *item = NULL;
	uint64_t input_set;
	int ret;

	memset(filter, 0, sizeof(*filter));
	item = ice_search_pattern_match_item(pattern, array, array_len, error);
	if (!item)
		return -rte_errno;

	ret = ice_fdir_parse_pattern(ad, pattern, error, filter);
	if (ret)
		return ret;
	input_set = filter->input_set;
	if (!input_set || input_set & ~item->input_set_mask) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   pattern,
				   "Invalid input set");
		return -rte_errno;
	}

	ret = ice_fdir_parse_action(ad, actions, error, filter);
	if (ret)
		return ret;

	*meta = filter;

	return 0;
}

static struct ice_flow_parser ice_fdir_parser = {
	.engine = &ice_fdir_engine,
	.array = ice_fdir_pattern,
	.array_len = RTE_DIM(ice_fdir_pattern),
	.parse_pattern_action = ice_fdir_parse,
	.stage = ICE_FLOW_STAGE_DISTRIBUTOR,
};

RTE_INIT(ice_fdir_engine_register)
{
	ice_register_flow_engine(&ice_fdir_engine);
}
