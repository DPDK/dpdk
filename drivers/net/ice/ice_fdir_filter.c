/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#include <stdio.h>
#include <rte_flow.h>
#include <rte_hash.h>
#include <rte_hash_crc.h>
#include "base/ice_fdir.h"
#include "base/ice_flow.h"
#include "base/ice_type.h"
#include "ice_ethdev.h"
#include "ice_rxtx.h"
#include "ice_generic_flow.h"

#define ICE_FDIR_IPV6_TC_OFFSET		20
#define ICE_IPV6_TC_MASK		(0xFF << ICE_FDIR_IPV6_TC_OFFSET)

#define ICE_FDIR_MAX_QREGION_SIZE	128

#define ICE_FDIR_INSET_ETH (\
	ICE_INSET_DMAC | ICE_INSET_SMAC | ICE_INSET_ETHERTYPE)

#define ICE_FDIR_INSET_ETH_IPV4 (\
	ICE_FDIR_INSET_ETH | \
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | ICE_INSET_IPV4_TOS | \
	ICE_INSET_IPV4_TTL | ICE_INSET_IPV4_PROTO | ICE_INSET_IPV4_PKID)

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
	ICE_INSET_DMAC | \
	ICE_INSET_IPV6_SRC | ICE_INSET_IPV6_DST | ICE_INSET_IPV6_TC | \
	ICE_INSET_IPV6_HOP_LIMIT | ICE_INSET_IPV6_NEXT_HDR | \
	ICE_INSET_IPV6_PKID)

#define ICE_FDIR_INSET_ETH_IPV6_UDP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_UDP_SRC_PORT | ICE_INSET_UDP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6_TCP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_TCP_SRC_PORT | ICE_INSET_TCP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV6_SCTP (\
	ICE_FDIR_INSET_ETH_IPV6 | \
	ICE_INSET_SCTP_SRC_PORT | ICE_INSET_SCTP_DST_PORT)

#define ICE_FDIR_INSET_IPV4 (\
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | \
	ICE_INSET_IPV4_PKID)

#define ICE_FDIR_INSET_IPV4_TCP (\
	ICE_FDIR_INSET_IPV4 | \
	ICE_INSET_TCP_SRC_PORT | ICE_INSET_TCP_DST_PORT)

#define ICE_FDIR_INSET_IPV4_UDP (\
	ICE_FDIR_INSET_IPV4 | \
	ICE_INSET_UDP_SRC_PORT | ICE_INSET_UDP_DST_PORT)

#define ICE_FDIR_INSET_IPV4_SCTP (\
	ICE_FDIR_INSET_IPV4 | \
	ICE_INSET_SCTP_SRC_PORT | ICE_INSET_SCTP_DST_PORT)

#define ICE_FDIR_INSET_ETH_IPV4_VXLAN (\
	ICE_FDIR_INSET_ETH | ICE_FDIR_INSET_ETH_IPV4 | \
	ICE_INSET_VXLAN_VNI)

#define ICE_FDIR_INSET_IPV4_GTPU (\
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | ICE_INSET_GTPU_TEID)

#define ICE_FDIR_INSET_IPV4_GTPU_EH (\
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | \
	ICE_INSET_GTPU_TEID | ICE_INSET_GTPU_QFI)

#define ICE_FDIR_INSET_IPV6_GTPU (\
	ICE_INSET_IPV6_SRC | ICE_INSET_IPV6_DST | ICE_INSET_GTPU_TEID)

#define ICE_FDIR_INSET_IPV6_GTPU_EH (\
	ICE_INSET_IPV6_SRC | ICE_INSET_IPV6_DST | \
	ICE_INSET_GTPU_TEID | ICE_INSET_GTPU_QFI)

#define ICE_FDIR_INSET_IPV4_ESP (\
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | \
	ICE_INSET_ESP_SPI)

#define ICE_FDIR_INSET_IPV6_ESP (\
	ICE_INSET_IPV6_SRC | ICE_INSET_IPV6_DST | \
	ICE_INSET_ESP_SPI)

#define ICE_FDIR_INSET_IPV4_NATT_ESP (\
	ICE_INSET_IPV4_SRC | ICE_INSET_IPV4_DST | \
	ICE_INSET_NAT_T_ESP_SPI)

#define ICE_FDIR_INSET_IPV6_NATT_ESP (\
	ICE_INSET_IPV6_SRC | ICE_INSET_IPV6_DST | \
	ICE_INSET_NAT_T_ESP_SPI)

static struct ice_pattern_match_item ice_fdir_pattern_list[] = {
	{pattern_raw,					ICE_INSET_NONE,			ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_ethertype,				ICE_FDIR_INSET_ETH,		ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv4,				ICE_FDIR_INSET_ETH_IPV4,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv4_udp,				ICE_FDIR_INSET_ETH_IPV4_UDP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv4_tcp,				ICE_FDIR_INSET_ETH_IPV4_TCP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv4_sctp,				ICE_FDIR_INSET_ETH_IPV4_SCTP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv6,				ICE_FDIR_INSET_ETH_IPV6,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv6_frag_ext,			ICE_FDIR_INSET_ETH_IPV6,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv6_udp,				ICE_FDIR_INSET_ETH_IPV6_UDP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv6_tcp,				ICE_FDIR_INSET_ETH_IPV6_TCP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv6_sctp,				ICE_FDIR_INSET_ETH_IPV6_SCTP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv4_esp,				ICE_FDIR_INSET_IPV4_ESP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_esp,			ICE_FDIR_INSET_IPV4_NATT_ESP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv6_esp,				ICE_FDIR_INSET_IPV6_ESP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv6_udp_esp,			ICE_FDIR_INSET_IPV6_NATT_ESP,	ICE_INSET_NONE,			ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4,		ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_IPV4,		ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_udp,		ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_IPV4_UDP,	ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_tcp,		ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_IPV4_TCP,	ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_ipv4_sctp,		ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_IPV4_SCTP,	ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4,		ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_ETH_IPV4,	ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_udp,	ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_ETH_IPV4_UDP,	ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_tcp,	ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_ETH_IPV4_TCP,	ICE_INSET_NONE},
	{pattern_eth_ipv4_udp_vxlan_eth_ipv4_sctp,	ICE_FDIR_INSET_ETH_IPV4_VXLAN,	ICE_FDIR_INSET_ETH_IPV4_SCTP,	ICE_INSET_NONE},
	/* duplicated GTPU input set in 3rd column to align with shared code behavior. Ideally, only put GTPU field in 2nd column. */
	{pattern_eth_ipv4_gtpu,				ICE_FDIR_INSET_IPV4_GTPU,	ICE_FDIR_INSET_IPV4_GTPU,	ICE_INSET_NONE},
	{pattern_eth_ipv4_gtpu_eh,			ICE_FDIR_INSET_IPV4_GTPU_EH,	ICE_FDIR_INSET_IPV4_GTPU_EH,	ICE_INSET_NONE},
	{pattern_eth_ipv6_gtpu,				ICE_FDIR_INSET_IPV6_GTPU,	ICE_FDIR_INSET_IPV6_GTPU,	ICE_INSET_NONE},
	{pattern_eth_ipv6_gtpu_eh,			ICE_FDIR_INSET_IPV6_GTPU_EH,	ICE_FDIR_INSET_IPV6_GTPU_EH,	ICE_INSET_NONE},
};

static struct ice_flow_parser ice_fdir_parser;

static int
ice_fdir_is_tunnel_profile(enum ice_fdir_tunnel_type tunnel_type);

static const struct rte_memzone *
ice_memzone_reserve(const char *name, uint32_t len, int socket_id)
{
	const struct rte_memzone *mz;

	mz = rte_memzone_lookup(name);
	if (mz)
		return mz;

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
	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
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
	for (fltr_ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     fltr_ptype < ptype;
	     fltr_ptype++) {
		rte_free(hw->fdir_prof[fltr_ptype]);
		hw->fdir_prof[fltr_ptype] = NULL;
	}

	rte_free(hw->fdir_prof);
	hw->fdir_prof = NULL;

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

	for (i = 0; i < container->index_free; i++) {
		rte_free(container->pools[i]);
		container->pools[i] = NULL;
	}

	TAILQ_INIT(&container->pool_list);
	container->index_free = 0;

	return 0;
}

static struct ice_fdir_counter *
ice_fdir_counter_shared_search(struct ice_fdir_counter_pool_container
					*container,
			       uint32_t id)
{
	struct ice_fdir_counter_pool *pool;
	struct ice_fdir_counter *counter;
	int i;

	TAILQ_FOREACH(pool, &container->pool_list, next) {
		for (i = 0; i < ICE_FDIR_COUNTERS_PER_BLOCK; i++) {
			counter = &pool->counters[i];

			if (counter->shared &&
			    counter->ref_cnt &&
			    counter->id == id)
				return counter;
		}
	}

	return NULL;
}

static struct ice_fdir_counter *
ice_fdir_counter_alloc(struct ice_pf *pf, uint32_t shared, uint32_t id)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_counter_pool_container *container =
				&fdir_info->counter;
	struct ice_fdir_counter_pool *pool = NULL;
	struct ice_fdir_counter *counter_free = NULL;

	if (shared) {
		counter_free = ice_fdir_counter_shared_search(container, id);
		if (counter_free) {
			if (counter_free->ref_cnt + 1 == 0) {
				rte_errno = E2BIG;
				return NULL;
			}
			counter_free->ref_cnt++;
			return counter_free;
		}
	}

	TAILQ_FOREACH(pool, &container->pool_list, next) {
		counter_free = TAILQ_FIRST(&pool->counter_list);
		if (counter_free)
			break;
		counter_free = NULL;
	}

	if (!counter_free) {
		PMD_DRV_LOG(ERR, "No free counter found\n");
		return NULL;
	}

	counter_free->shared = shared;
	counter_free->id = id;
	counter_free->ref_cnt = 1;
	counter_free->pool = pool;

	/* reset statistic counter value */
	ICE_WRITE_REG(hw, GLSTAT_FD_CNT0H(counter_free->hw_index), 0);
	ICE_WRITE_REG(hw, GLSTAT_FD_CNT0L(counter_free->hw_index), 0);

	TAILQ_REMOVE(&pool->counter_list, counter_free, next);
	if (TAILQ_EMPTY(&pool->counter_list)) {
		TAILQ_REMOVE(&container->pool_list, pool, next);
		TAILQ_INSERT_TAIL(&container->pool_list, pool, next);
	}

	return counter_free;
}

static void
ice_fdir_counter_free(__rte_unused struct ice_pf *pf,
		      struct ice_fdir_counter *counter)
{
	if (!counter)
		return;

	if (--counter->ref_cnt == 0) {
		struct ice_fdir_counter_pool *pool = counter->pool;

		TAILQ_INSERT_TAIL(&pool->counter_list, counter, next);
	}
}

static int
ice_fdir_init_filter_list(struct ice_pf *pf)
{
	struct rte_eth_dev *dev = &rte_eth_devices[pf->dev_data->port_id];
	struct ice_fdir_info *fdir_info = &pf->fdir;
	char fdir_hash_name[RTE_HASH_NAMESIZE];
	int ret;

	struct rte_hash_parameters fdir_hash_params = {
		.name = fdir_hash_name,
		.entries = ICE_MAX_FDIR_FILTER_NUM,
		.key_len = sizeof(struct ice_fdir_fltr_pattern),
		.hash_func = rte_hash_crc,
		.hash_func_init_val = 0,
		.socket_id = rte_socket_id(),
		.extra_flag = RTE_HASH_EXTRA_FLAGS_EXT_TABLE,
	};

	/* Initialize hash */
	snprintf(fdir_hash_name, RTE_HASH_NAMESIZE,
		 "fdir_%s", dev->device->name);
	fdir_info->hash_table = rte_hash_create(&fdir_hash_params);
	if (!fdir_info->hash_table) {
		PMD_INIT_LOG(ERR, "Failed to create fdir hash table!");
		return -EINVAL;
	}
	fdir_info->hash_map = rte_zmalloc("ice_fdir_hash_map",
					  sizeof(*fdir_info->hash_map) *
					  ICE_MAX_FDIR_FILTER_NUM,
					  0);
	if (!fdir_info->hash_map) {
		PMD_INIT_LOG(ERR,
			     "Failed to allocate memory for fdir hash map!");
		ret = -ENOMEM;
		goto err_fdir_hash_map_alloc;
	}
	return 0;

err_fdir_hash_map_alloc:
	rte_hash_free(fdir_info->hash_table);

	return ret;
}

static void
ice_fdir_release_filter_list(struct ice_pf *pf)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;

	rte_free(fdir_info->hash_map);
	rte_hash_free(fdir_info->hash_table);

	fdir_info->hash_map = NULL;
	fdir_info->hash_table = NULL;
}

/*
 * ice_fdir_setup - reserve and initialize the Flow Director resources
 * @pf: board private structure
 */
static int
ice_fdir_setup(struct ice_pf *pf)
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[pf->dev_data->port_id];
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

	err = ice_fdir_init_filter_list(pf);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to init FDIR filter list.");
		return -EINVAL;
	}

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

	/* Enable FDIR MSIX interrupt */
	vsi->nb_used_qps = 1;
	ice_vsi_queues_bind_intr(vsi);
	ice_vsi_enable_queues_intr(vsi);

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
	pf->fdir.mz = mz;

	err = ice_fdir_prof_alloc(hw);
	if (err) {
		PMD_DRV_LOG(ERR, "Cannot allocate memory for "
			    "flow director profile.");
		err = -ENOMEM;
		goto fail_prof;
	}

	PMD_DRV_LOG(INFO, "FDIR setup successfully, with programming queue %u.",
		    vsi->base_queue);
	return ICE_SUCCESS;

fail_prof:
	rte_memzone_free(pf->fdir.mz);
	pf->fdir.mz = NULL;
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

	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
	     ptype < ICE_FLTR_PTYPE_MAX;
	     ptype++) {
		rte_free(hw->fdir_prof[ptype]);
		hw->fdir_prof[ptype] = NULL;
	}

	rte_free(hw->fdir_prof);
	hw->fdir_prof = NULL;
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
			ice_flow_rem_entry(hw, ICE_BLK_FD,
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

	for (ptype = ICE_FLTR_PTYPE_NONF_NONE + 1;
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
	struct rte_eth_dev *eth_dev = &rte_eth_devices[pf->dev_data->port_id];
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_vsi *vsi;
	int err;

	vsi = pf->fdir.fdir_vsi;
	if (!vsi)
		return;

	ice_vsi_disable_queues_intr(vsi);

	err = ice_fdir_tx_queue_stop(eth_dev, pf->fdir.txq->queue_id);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to stop TX queue.");

	err = ice_fdir_rx_queue_stop(eth_dev, pf->fdir.rxq->queue_id);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to stop RX queue.");

	err = ice_fdir_counter_release(pf);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to release FDIR counter resource.");

	ice_fdir_release_filter_list(pf);

	ice_tx_queue_release(pf->fdir.txq);
	pf->fdir.txq = NULL;
	ice_rx_queue_release(pf->fdir.rxq);
	pf->fdir.rxq = NULL;
	ice_fdir_prof_rm_all(pf);
	ice_fdir_prof_free(hw);
	ice_release_vsi(vsi);
	pf->fdir.fdir_vsi = NULL;

	if (pf->fdir.mz) {
		err = rte_memzone_free(pf->fdir.mz);
		pf->fdir.mz = NULL;
		if (err)
			PMD_DRV_LOG(ERR, "Failed to free FDIR memezone.");
	}
}

static int
ice_fdir_cur_prof_conflict(struct ice_pf *pf,
			   enum ice_fltr_ptype ptype,
			   struct ice_flow_seg_info *seg,
			   bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_flow_seg_info *ori_seg;
	struct ice_fd_hw_prof *hw_prof;

	hw_prof = hw->fdir_prof[ptype];
	ori_seg = hw_prof->fdir_seg[is_tunnel];

	/* profile does not exist */
	if (!ori_seg)
		return 0;

	/* if no input set conflict, return -EEXIST */
	if ((!is_tunnel && !memcmp(ori_seg, seg, sizeof(*seg))) ||
	    (is_tunnel && !memcmp(&ori_seg[1], &seg[1], sizeof(*seg)))) {
		PMD_DRV_LOG(DEBUG, "Profile already exists for flow type %d.",
			    ptype);
		return -EEXIST;
	}

	/* a rule with input set conflict already exist, so give up */
	if (pf->fdir_fltr_cnt[ptype][is_tunnel]) {
		PMD_DRV_LOG(DEBUG, "Failed to create profile for flow type %d due to conflict with existing rule.",
			    ptype);
		return -EINVAL;
	}

	/* it's safe to delete an empty profile */
	ice_fdir_prof_rm(pf, ptype, is_tunnel);
	return 0;
}

static bool
ice_fdir_prof_resolve_conflict(struct ice_pf *pf,
			       enum ice_fltr_ptype ptype,
			       bool is_tunnel)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fd_hw_prof *hw_prof;
	struct ice_flow_seg_info *seg;

	hw_prof = hw->fdir_prof[ptype];
	seg = hw_prof->fdir_seg[is_tunnel];

	/* profile does not exist */
	if (!seg)
		return true;

	/* profile exists and rule exists, fail to resolve the conflict */
	if (pf->fdir_fltr_cnt[ptype][is_tunnel] != 0)
		return false;

	/* it's safe to delete an empty profile */
	ice_fdir_prof_rm(pf, ptype, is_tunnel);

	return true;
}

static int
ice_fdir_cross_prof_conflict(struct ice_pf *pf,
			     enum ice_fltr_ptype ptype,
			     bool is_tunnel)
{
	enum ice_fltr_ptype cflct_ptype;

	switch (ptype) {
	/* IPv4 */
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_TCP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_SCTP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	/* IPv4 GTPU */
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_ICMP:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_OTHER:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_UDP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_TCP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_IPV4_ICMP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	/* IPv6 */
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV6_SCTP:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_OTHER:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_UDP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_TCP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV6_SCTP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_SCTP:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_OTHER;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_OTHER:
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_UDP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_TCP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		cflct_ptype = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_SCTP;
		if (!ice_fdir_prof_resolve_conflict
			(pf, cflct_ptype, is_tunnel))
			goto err;
		break;
	default:
		break;
	}
	return 0;
err:
	PMD_DRV_LOG(DEBUG, "Failed to create profile for flow type %d due to conflict with existing rule of flow type %d.",
		    ptype, cflct_ptype);
	return -EINVAL;
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
	struct ice_fd_hw_prof *hw_prof;
	struct ice_flow_prof *prof;
	uint64_t entry_1 = 0;
	uint64_t entry_2 = 0;
	uint16_t vsi_num;
	int ret;
	uint64_t prof_id;

	/* check if have input set conflict on current profile. */
	ret = ice_fdir_cur_prof_conflict(pf, ptype, seg, is_tunnel);
	if (ret)
		return ret;

	/* check if the profile is conflict with other profile. */
	ret = ice_fdir_cross_prof_conflict(pf, ptype, is_tunnel);
	if (ret)
		return ret;

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

	hw_prof = hw->fdir_prof[ptype];
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
	ice_flow_rem_entry(hw, ICE_BLK_FD, entry_1);
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
		{ICE_INSET_ETHERTYPE, ICE_FLOW_FIELD_IDX_ETH_TYPE},
		{ICE_INSET_IPV4_SRC, ICE_FLOW_FIELD_IDX_IPV4_SA},
		{ICE_INSET_IPV4_DST, ICE_FLOW_FIELD_IDX_IPV4_DA},
		{ICE_INSET_IPV4_TOS, ICE_FLOW_FIELD_IDX_IPV4_DSCP},
		{ICE_INSET_IPV4_TTL, ICE_FLOW_FIELD_IDX_IPV4_TTL},
		{ICE_INSET_IPV4_PROTO, ICE_FLOW_FIELD_IDX_IPV4_PROT},
		{ICE_INSET_IPV4_PKID, ICE_FLOW_FIELD_IDX_IPV4_ID},
		{ICE_INSET_IPV6_SRC, ICE_FLOW_FIELD_IDX_IPV6_SA},
		{ICE_INSET_IPV6_DST, ICE_FLOW_FIELD_IDX_IPV6_DA},
		{ICE_INSET_IPV6_TC, ICE_FLOW_FIELD_IDX_IPV6_DSCP},
		{ICE_INSET_IPV6_NEXT_HDR, ICE_FLOW_FIELD_IDX_IPV6_PROT},
		{ICE_INSET_IPV6_HOP_LIMIT, ICE_FLOW_FIELD_IDX_IPV6_TTL},
		{ICE_INSET_IPV6_PKID, ICE_FLOW_FIELD_IDX_IPV6_ID},
		{ICE_INSET_TCP_SRC_PORT, ICE_FLOW_FIELD_IDX_TCP_SRC_PORT},
		{ICE_INSET_TCP_DST_PORT, ICE_FLOW_FIELD_IDX_TCP_DST_PORT},
		{ICE_INSET_UDP_SRC_PORT, ICE_FLOW_FIELD_IDX_UDP_SRC_PORT},
		{ICE_INSET_UDP_DST_PORT, ICE_FLOW_FIELD_IDX_UDP_DST_PORT},
		{ICE_INSET_SCTP_SRC_PORT, ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT},
		{ICE_INSET_SCTP_DST_PORT, ICE_FLOW_FIELD_IDX_SCTP_DST_PORT},
		{ICE_INSET_IPV4_SRC, ICE_FLOW_FIELD_IDX_IPV4_SA},
		{ICE_INSET_IPV4_DST, ICE_FLOW_FIELD_IDX_IPV4_DA},
		{ICE_INSET_TCP_SRC_PORT, ICE_FLOW_FIELD_IDX_TCP_SRC_PORT},
		{ICE_INSET_TCP_DST_PORT, ICE_FLOW_FIELD_IDX_TCP_DST_PORT},
		{ICE_INSET_UDP_SRC_PORT, ICE_FLOW_FIELD_IDX_UDP_SRC_PORT},
		{ICE_INSET_UDP_DST_PORT, ICE_FLOW_FIELD_IDX_UDP_DST_PORT},
		{ICE_INSET_SCTP_SRC_PORT, ICE_FLOW_FIELD_IDX_SCTP_SRC_PORT},
		{ICE_INSET_SCTP_DST_PORT, ICE_FLOW_FIELD_IDX_SCTP_DST_PORT},
		{ICE_INSET_GTPU_TEID, ICE_FLOW_FIELD_IDX_GTPU_IP_TEID},
		{ICE_INSET_GTPU_QFI, ICE_FLOW_FIELD_IDX_GTPU_EH_QFI},
		{ICE_INSET_VXLAN_VNI, ICE_FLOW_FIELD_IDX_VXLAN_VNI},
		{ICE_INSET_ESP_SPI, ICE_FLOW_FIELD_IDX_ESP_SPI},
		{ICE_INSET_NAT_T_ESP_SPI, ICE_FLOW_FIELD_IDX_NAT_T_ESP_SPI},
	};

	for (i = 0, j = 0; i < RTE_DIM(ice_inset_map); i++) {
		if ((inset & ice_inset_map[i].inset) ==
		    ice_inset_map[i].inset)
			field[j++] = ice_inset_map[i].fld;
	}
}

static void
ice_fdir_input_set_hdrs(enum ice_fltr_ptype flow, struct ice_flow_seg_info *seg)
{
	switch (flow) {
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_UDP |
				  ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_TCP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_TCP |
				  ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_SCTP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_SCTP |
				  ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_OTHER:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_FRAG_IPV4:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_FRAG);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_UDP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_UDP |
				  ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_TCP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_TCP |
				  ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_SCTP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_SCTP |
				  ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_OTHER:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_FRAG_IPV6:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_FRAG);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_UDP:
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_TCP:
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_SCTP:
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_OTHER:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_GTPU_IP |
				  ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_GTPU_EH:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_GTPU_EH |
				  ICE_FLOW_SEG_HDR_GTPU_IP |
				  ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_GTPU:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_GTPU_IP |
				  ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_GTPU_EH:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_GTPU_EH |
				  ICE_FLOW_SEG_HDR_GTPU_IP |
				  ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NON_IP_L2:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_ETH_NON_IP);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_ESP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_ESP |
				  ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_ESP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_ESP |
				  ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV4_NAT_T_ESP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_NAT_T_ESP |
				  ICE_FLOW_SEG_HDR_IPV4 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	case ICE_FLTR_PTYPE_NONF_IPV6_NAT_T_ESP:
		ICE_FLOW_SET_HDRS(seg, ICE_FLOW_SEG_HDR_NAT_T_ESP |
				  ICE_FLOW_SEG_HDR_IPV6 |
				  ICE_FLOW_SEG_HDR_IPV_OTHER);
		break;
	default:
		PMD_DRV_LOG(ERR, "not supported filter type.");
		break;
	}
}

static int
ice_fdir_input_set_conf(struct ice_pf *pf, enum ice_fltr_ptype flow,
			uint64_t inner_input_set, uint64_t outer_input_set,
			enum ice_fdir_tunnel_type ttype)
{
	struct ice_flow_seg_info *seg;
	struct ice_flow_seg_info *seg_tun = NULL;
	enum ice_flow_field field[ICE_FLOW_FIELD_IDX_MAX];
	uint64_t input_set;
	bool is_tunnel;
	int k, i, ret = 0;

	if (!(inner_input_set | outer_input_set))
		return -EINVAL;

	seg_tun = (struct ice_flow_seg_info *)
		ice_malloc(hw, sizeof(*seg_tun) * ICE_FD_HW_SEG_MAX);
	if (!seg_tun) {
		PMD_DRV_LOG(ERR, "No memory can be allocated");
		return -ENOMEM;
	}

	/* use seg_tun[1] to record tunnel inner part */
	for (k = 0; k <= ICE_FD_HW_SEG_TUN; k++) {
		seg = &seg_tun[k];
		input_set = (k == ICE_FD_HW_SEG_TUN) ? inner_input_set : outer_input_set;
		if (input_set == 0)
			continue;

		for (i = 0; i < ICE_FLOW_FIELD_IDX_MAX; i++)
			field[i] = ICE_FLOW_FIELD_IDX_MAX;

		ice_fdir_input_set_parse(input_set, field);

		ice_fdir_input_set_hdrs(flow, seg);

		for (i = 0; field[i] != ICE_FLOW_FIELD_IDX_MAX; i++) {
			ice_flow_set_fld(seg, field[i],
					 ICE_FLOW_FLD_OFF_INVAL,
					 ICE_FLOW_FLD_OFF_INVAL,
					 ICE_FLOW_FLD_OFF_INVAL, false);
		}
	}

	is_tunnel = ice_fdir_is_tunnel_profile(ttype);

	ret = ice_fdir_hw_tbl_conf(pf, pf->main_vsi, pf->fdir.fdir_vsi,
				   seg_tun, flow, is_tunnel);

	if (!ret) {
		return ret;
	} else if (ret < 0) {
		rte_free(seg_tun);
		return (ret == -EEXIST) ? 0 : ret;
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
	struct ice_flow_parser *parser;
	int ret;

	if (ad->hw.dcf_enabled)
		return 0;

	ret = ice_fdir_setup(pf);
	if (ret)
		return ret;

	parser = &ice_fdir_parser;

	return ice_register_parser(parser, ad);
}

static void
ice_fdir_uninit(struct ice_adapter *ad)
{
	struct ice_flow_parser *parser;
	struct ice_pf *pf = &ad->pf;

	if (ad->hw.dcf_enabled)
		return;

	parser = &ice_fdir_parser;

	ice_unregister_parser(parser, ad);

	ice_fdir_teardown(pf);
}

static int
ice_fdir_is_tunnel_profile(enum ice_fdir_tunnel_type tunnel_type)
{
	if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_VXLAN)
		return 1;
	else
		return 0;
}

static int
ice_fdir_add_del_raw(struct ice_pf *pf,
		     struct ice_fdir_filter_conf *filter,
		     bool add)
{
	struct ice_hw *hw = ICE_PF_TO_HW(pf);

	unsigned char *pkt = (unsigned char *)pf->fdir.prg_pkt;
	rte_memcpy(pkt, filter->pkt_buf, filter->pkt_len);

	struct ice_fltr_desc desc;
	memset(&desc, 0, sizeof(desc));
	filter->input.comp_report = ICE_FXD_FLTR_QW0_COMP_REPORT_SW;
	ice_fdir_get_prgm_desc(hw, &filter->input, &desc, add);

	return ice_fdir_programming(pf, &desc);
}

static int
ice_fdir_add_del_filter(struct ice_pf *pf,
			struct ice_fdir_filter_conf *filter,
			bool add)
{
	struct ice_fltr_desc desc;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	unsigned char *pkt = (unsigned char *)pf->fdir.prg_pkt;
	bool is_tun;
	int ret;

	filter->input.dest_vsi = pf->main_vsi->idx;

	memset(&desc, 0, sizeof(desc));
	filter->input.comp_report = ICE_FXD_FLTR_QW0_COMP_REPORT_SW;
	ice_fdir_get_prgm_desc(hw, &filter->input, &desc, add);

	is_tun = ice_fdir_is_tunnel_profile(filter->tunnel_type);

	memset(pkt, 0, ICE_FDIR_PKT_LEN);
	ret = ice_fdir_get_gen_prgm_pkt(hw, &filter->input, pkt, false, is_tun);
	if (ret) {
		PMD_DRV_LOG(ERR, "Generate dummy packet failed");
		return -EINVAL;
	}

	return ice_fdir_programming(pf, &desc);
}

static void
ice_fdir_extract_fltr_key(struct ice_fdir_fltr_pattern *key,
			  struct ice_fdir_filter_conf *filter)
{
	struct ice_fdir_fltr *input = &filter->input;
	memset(key, 0, sizeof(*key));

	key->flow_type = input->flow_type;
	rte_memcpy(&key->ip, &input->ip, sizeof(key->ip));
	rte_memcpy(&key->mask, &input->mask, sizeof(key->mask));
	rte_memcpy(&key->ext_data, &input->ext_data, sizeof(key->ext_data));
	rte_memcpy(&key->ext_mask, &input->ext_mask, sizeof(key->ext_mask));

	rte_memcpy(&key->gtpu_data, &input->gtpu_data, sizeof(key->gtpu_data));
	rte_memcpy(&key->gtpu_mask, &input->gtpu_mask, sizeof(key->gtpu_mask));

	key->tunnel_type = filter->tunnel_type;
}

/* Check if there exists the flow director filter */
static struct ice_fdir_filter_conf *
ice_fdir_entry_lookup(struct ice_fdir_info *fdir_info,
			const struct ice_fdir_fltr_pattern *key)
{
	int ret;

	ret = rte_hash_lookup(fdir_info->hash_table, key);
	if (ret < 0)
		return NULL;

	return fdir_info->hash_map[ret];
}

/* Add a flow director entry into the SW list */
static int
ice_fdir_entry_insert(struct ice_pf *pf,
		      struct ice_fdir_filter_conf *entry,
		      struct ice_fdir_fltr_pattern *key)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;
	int ret;

	ret = rte_hash_add_key(fdir_info->hash_table, key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to insert fdir entry to hash table %d!",
			    ret);
		return ret;
	}
	fdir_info->hash_map[ret] = entry;

	return 0;
}

/* Delete a flow director entry from the SW list */
static int
ice_fdir_entry_del(struct ice_pf *pf, struct ice_fdir_fltr_pattern *key)
{
	struct ice_fdir_info *fdir_info = &pf->fdir;
	int ret;

	ret = rte_hash_del_key(fdir_info->hash_table, key);
	if (ret < 0) {
		PMD_DRV_LOG(ERR,
			    "Failed to delete fdir filter to hash table %d!",
			    ret);
		return ret;
	}
	fdir_info->hash_map[ret] = NULL;

	return 0;
}

static int
ice_fdir_create_filter(struct ice_adapter *ad,
		       struct rte_flow *flow,
		       void *meta,
		       struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_filter_conf *filter = meta;
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_filter_conf *entry, *node;
	struct ice_fdir_fltr_pattern key;
	bool is_tun;
	int ret;
	int i;

	if (filter->parser_ena) {
		struct ice_hw *hw = ICE_PF_TO_HW(pf);

		int id = ice_find_first_bit(filter->prof->ptypes, UINT16_MAX);
		int ptg = hw->blk[ICE_BLK_FD].xlt1.t[id];
		u16 ctrl_vsi = pf->fdir.fdir_vsi->idx;
		u16 main_vsi = pf->main_vsi->idx;
		bool fv_found = false;

		struct ice_fdir_prof_info *pi = &ad->fdir_prof_info[ptg];
		if (pi->fdir_actived_cnt != 0) {
			for (i = 0; i < ICE_MAX_FV_WORDS; i++)
				if (pi->prof.fv[i].proto_id !=
				    filter->prof->fv[i].proto_id ||
				    pi->prof.fv[i].offset !=
				    filter->prof->fv[i].offset ||
				    pi->prof.fv[i].msk !=
				    filter->prof->fv[i].msk)
					break;
			if (i == ICE_MAX_FV_WORDS) {
				fv_found = true;
				pi->fdir_actived_cnt++;
			}
		}

		if (!fv_found) {
			ret = ice_flow_set_hw_prof(hw, main_vsi, ctrl_vsi,
						   filter->prof, ICE_BLK_FD);
			if (ret)
				goto error;
		}

		ret = ice_fdir_add_del_raw(pf, filter, true);
		if (ret)
			goto error;

		if (!fv_found) {
			for (i = 0; i < filter->prof->fv_num; i++) {
				pi->prof.fv[i].proto_id =
					filter->prof->fv[i].proto_id;
				pi->prof.fv[i].offset =
					filter->prof->fv[i].offset;
				pi->prof.fv[i].msk = filter->prof->fv[i].msk;
			}
			pi->fdir_actived_cnt = 1;
		}

		if (filter->mark_flag == 1)
			ice_fdir_rx_parsing_enable(ad, 1);

		entry = rte_zmalloc("fdir_entry", sizeof(*entry), 0);
		if (!entry)
			goto error;

		rte_memcpy(entry, filter, sizeof(*filter));

		flow->rule = entry;

		return 0;
	}

	ice_fdir_extract_fltr_key(&key, filter);
	node = ice_fdir_entry_lookup(fdir_info, &key);
	if (node) {
		rte_flow_error_set(error, EEXIST,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Rule already exists!");
		return -rte_errno;
	}

	entry = rte_zmalloc("fdir_entry", sizeof(*entry), 0);
	if (!entry) {
		rte_flow_error_set(error, ENOMEM,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Failed to allocate memory");
		return -rte_errno;
	}

	is_tun = ice_fdir_is_tunnel_profile(filter->tunnel_type);

	ret = ice_fdir_input_set_conf(pf, filter->input.flow_type,
				      filter->input_set_i, filter->input_set_o,
				      filter->tunnel_type);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Profile configure failed.");
		goto free_entry;
	}

	/* alloc counter for FDIR */
	if (filter->input.cnt_ena) {
		struct rte_flow_action_count *act_count = &filter->act_count;

		filter->counter = ice_fdir_counter_alloc(pf, 0, act_count->id);
		if (!filter->counter) {
			rte_flow_error_set(error, EINVAL,
					RTE_FLOW_ERROR_TYPE_ACTION, NULL,
					"Failed to alloc FDIR counter.");
			goto free_entry;
		}
		filter->input.cnt_index = filter->counter->hw_index;
	}

	ret = ice_fdir_add_del_filter(pf, filter, true);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Add filter rule failed.");
		goto free_counter;
	}

	if (filter->mark_flag == 1)
		ice_fdir_rx_parsing_enable(ad, 1);

	rte_memcpy(entry, filter, sizeof(*entry));
	ret = ice_fdir_entry_insert(pf, entry, &key);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Insert entry to table failed.");
		goto free_entry;
	}

	flow->rule = entry;
	ice_fdir_cnt_update(pf, filter->input.flow_type, is_tun, true);

	return 0;

free_counter:
	if (filter->counter) {
		ice_fdir_counter_free(pf, filter->counter);
		filter->counter = NULL;
	}

free_entry:
	rte_free(entry);
	return -rte_errno;

error:
	rte_free(filter->prof);
	rte_free(filter->pkt_buf);
	return -rte_errno;
}

static int
ice_fdir_destroy_filter(struct ice_adapter *ad,
			struct rte_flow *flow,
			struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_info *fdir_info = &pf->fdir;
	struct ice_fdir_filter_conf *filter, *entry;
	struct ice_fdir_fltr_pattern key;
	bool is_tun;
	int ret;

	filter = (struct ice_fdir_filter_conf *)flow->rule;

	if (filter->parser_ena) {
		struct ice_hw *hw = ICE_PF_TO_HW(pf);

		int id = ice_find_first_bit(filter->prof->ptypes, UINT16_MAX);
		int ptg = hw->blk[ICE_BLK_FD].xlt1.t[id];
		u16 ctrl_vsi = pf->fdir.fdir_vsi->idx;
		u16 main_vsi = pf->main_vsi->idx;
		enum ice_block blk = ICE_BLK_FD;
		u16 vsi_num;

		ret = ice_fdir_add_del_raw(pf, filter, false);
		if (ret)
			return -rte_errno;

		struct ice_fdir_prof_info *pi = &ad->fdir_prof_info[ptg];
		if (pi->fdir_actived_cnt != 0) {
			pi->fdir_actived_cnt--;
			if (!pi->fdir_actived_cnt) {
				vsi_num = ice_get_hw_vsi_num(hw, ctrl_vsi);
				ice_rem_prof_id_flow(hw, blk, vsi_num, id);

				vsi_num = ice_get_hw_vsi_num(hw, main_vsi);
				ice_rem_prof_id_flow(hw, blk, vsi_num, id);
			}
		}

		if (filter->mark_flag == 1)
			ice_fdir_rx_parsing_enable(ad, 0);

		flow->rule = NULL;

		rte_free(filter->prof);
		rte_free(filter->pkt_buf);
		rte_free(filter);

		return 0;
	}

	is_tun = ice_fdir_is_tunnel_profile(filter->tunnel_type);

	if (filter->counter) {
		ice_fdir_counter_free(pf, filter->counter);
		filter->counter = NULL;
	}

	ice_fdir_extract_fltr_key(&key, filter);
	entry = ice_fdir_entry_lookup(fdir_info, &key);
	if (!entry) {
		rte_flow_error_set(error, ENOENT,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Can't find entry.");
		return -rte_errno;
	}

	ret = ice_fdir_add_del_filter(pf, filter, false);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Del filter rule failed.");
		return -rte_errno;
	}

	ret = ice_fdir_entry_del(pf, &key);
	if (ret) {
		rte_flow_error_set(error, -ret,
				   RTE_FLOW_ERROR_TYPE_HANDLE, NULL,
				   "Remove entry from table failed.");
		return -rte_errno;
	}

	ice_fdir_cnt_update(pf, filter->input.flow_type, is_tun, false);

	if (filter->mark_flag == 1)
		ice_fdir_rx_parsing_enable(ad, 0);

	flow->rule = NULL;

	rte_free(filter);

	return 0;
}

static int
ice_fdir_query_count(struct ice_adapter *ad,
		      struct rte_flow *flow,
		      struct rte_flow_query_count *flow_stats,
		      struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_hw *hw = ICE_PF_TO_HW(pf);
	struct ice_fdir_filter_conf *filter = flow->rule;
	struct ice_fdir_counter *counter = filter->counter;
	uint64_t hits_lo, hits_hi;

	if (!counter) {
		rte_flow_error_set(error, EINVAL,
				  RTE_FLOW_ERROR_TYPE_ACTION,
				  NULL,
				  "FDIR counters not available");
		return -rte_errno;
	}

	/*
	 * Reading the low 32-bits latches the high 32-bits into a shadow
	 * register. Reading the high 32-bit returns the value in the
	 * shadow register.
	 */
	hits_lo = ICE_READ_REG(hw, GLSTAT_FD_CNT0L(counter->hw_index));
	hits_hi = ICE_READ_REG(hw, GLSTAT_FD_CNT0H(counter->hw_index));

	flow_stats->hits_set = 1;
	flow_stats->hits = hits_lo | (hits_hi << 32);
	flow_stats->bytes_set = 0;
	flow_stats->bytes = 0;

	if (flow_stats->reset) {
		/* reset statistic counter value */
		ICE_WRITE_REG(hw, GLSTAT_FD_CNT0H(counter->hw_index), 0);
		ICE_WRITE_REG(hw, GLSTAT_FD_CNT0L(counter->hw_index), 0);
	}

	return 0;
}

static struct ice_flow_engine ice_fdir_engine = {
	.init = ice_fdir_init,
	.uninit = ice_fdir_uninit,
	.create = ice_fdir_create_filter,
	.destroy = ice_fdir_destroy_filter,
	.query_count = ice_fdir_query_count,
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
	const struct rte_flow_action_count *act_count;
	uint32_t dest_num = 0;
	uint32_t mark_num = 0;
	uint32_t counter_num = 0;
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
				ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_OTHER;
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
			filter->mark_flag = 1;
			mark_spec = actions->conf;
			filter->input.fltr_id = mark_spec->id;
			filter->input.fdid_prio = ICE_FXD_FLTR_QW1_FDID_PRI_ONE;
			break;
		case RTE_FLOW_ACTION_TYPE_COUNT:
			counter_num++;

			act_count = actions->conf;
			filter->input.cnt_ena = ICE_FXD_FLTR_QW0_STAT_ENA_PKTS;
			rte_memcpy(&filter->act_count, act_count,
						sizeof(filter->act_count));

			break;
		default:
			rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ACTION, actions,
				   "Invalid action.");
			return -rte_errno;
		}
	}

	if (dest_num >= 2) {
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

	if (counter_num >= 2) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Too many count actions");
		return -rte_errno;
	}

	if (dest_num + mark_num + counter_num == 0) {
		rte_flow_error_set(error, EINVAL,
			   RTE_FLOW_ERROR_TYPE_ACTION, actions,
			   "Empty action");
		return -rte_errno;
	}

	/* set default action to PASSTHRU mode, in "mark/count only" case. */
	if (dest_num == 0)
		filter->input.dest_ctl =
			ICE_FLTR_PRGM_DESC_DEST_DIRECT_PKT_OTHER;

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
	enum rte_flow_item_type l4 = RTE_FLOW_ITEM_TYPE_END;
	enum ice_fdir_tunnel_type tunnel_type = ICE_FDIR_TUNNEL_TYPE_NONE;
	const struct rte_flow_item_raw *raw_spec, *raw_mask;
	const struct rte_flow_item_eth *eth_spec, *eth_mask;
	const struct rte_flow_item_ipv4 *ipv4_spec, *ipv4_last, *ipv4_mask;
	const struct rte_flow_item_ipv6 *ipv6_spec, *ipv6_mask;
	const struct rte_flow_item_ipv6_frag_ext *ipv6_frag_spec,
					*ipv6_frag_mask;
	const struct rte_flow_item_tcp *tcp_spec, *tcp_mask;
	const struct rte_flow_item_udp *udp_spec, *udp_mask;
	const struct rte_flow_item_sctp *sctp_spec, *sctp_mask;
	const struct rte_flow_item_vxlan *vxlan_spec, *vxlan_mask;
	const struct rte_flow_item_gtp *gtp_spec, *gtp_mask;
	const struct rte_flow_item_gtp_psc *gtp_psc_spec, *gtp_psc_mask;
	const struct rte_flow_item_esp *esp_spec, *esp_mask;
	uint64_t input_set_i = ICE_INSET_NONE; /* only for tunnel inner */
	uint64_t input_set_o = ICE_INSET_NONE; /* non-tunnel and tunnel outer */
	uint64_t *input_set;
	uint8_t flow_type = ICE_FLTR_PTYPE_NONF_NONE;
	uint8_t  ipv6_addr_mask[16] = {
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
		0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF
	};
	uint32_t vtc_flow_cpu;
	uint16_t ether_type;
	enum rte_flow_item_type next_type;
	bool is_outer = true;
	struct ice_fdir_extra *p_ext_data;
	struct ice_fdir_v4 *p_v4 = NULL;
	struct ice_fdir_v6 *p_v6 = NULL;
	struct ice_parser_result rslt;
	struct ice_parser *psr;
	uint8_t item_num = 0;

	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		if (item->type == RTE_FLOW_ITEM_TYPE_VXLAN)
			tunnel_type = ICE_FDIR_TUNNEL_TYPE_VXLAN;
		/* To align with shared code behavior, save gtpu outer
		 * fields in inner struct.
		 */
		if (item->type == RTE_FLOW_ITEM_TYPE_GTPU ||
		    item->type == RTE_FLOW_ITEM_TYPE_GTP_PSC) {
			is_outer = false;
		}
		item_num++;
	}

	/* This loop parse flow pattern and distinguish Non-tunnel and tunnel
	 * flow. input_set_i is used for inner part.
	 */
	for (item = pattern; item->type != RTE_FLOW_ITEM_TYPE_END; item++) {
		item_type = item->type;

		if (item->last && !(item_type == RTE_FLOW_ITEM_TYPE_IPV4 ||
				    item_type ==
				    RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT)) {
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM, item,
					   "Not support range");
		}

		input_set = (tunnel_type && !is_outer) ?
			    &input_set_i : &input_set_o;

		switch (item_type) {
		case RTE_FLOW_ITEM_TYPE_RAW: {
			raw_spec = item->spec;
			raw_mask = item->mask;

			if (item_num != 1)
				break;

			/* convert raw spec & mask from byte string to int */
			unsigned char *tmp_spec =
				(uint8_t *)(uintptr_t)raw_spec->pattern;
			unsigned char *tmp_mask =
				(uint8_t *)(uintptr_t)raw_mask->pattern;
			uint16_t udp_port = 0;
			uint16_t tmp_val = 0;
			uint8_t pkt_len = 0;
			uint8_t tmp = 0;
			int i, j;

			pkt_len = strlen((char *)(uintptr_t)raw_spec->pattern);
			if (strlen((char *)(uintptr_t)raw_mask->pattern) !=
				pkt_len)
				return -rte_errno;

			for (i = 0, j = 0; i < pkt_len; i += 2, j++) {
				tmp = tmp_spec[i];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_val = tmp - 'a' + 10;
				if (tmp >= 'A' && tmp <= 'F')
					tmp_val = tmp - 'A' + 10;
				if (tmp >= '0' && tmp <= '9')
					tmp_val = tmp - '0';

				tmp_val *= 16;
				tmp = tmp_spec[i + 1];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_spec[j] = tmp_val + tmp - 'a' + 10;
				if (tmp >= 'A' && tmp <= 'F')
					tmp_spec[j] = tmp_val + tmp - 'A' + 10;
				if (tmp >= '0' && tmp <= '9')
					tmp_spec[j] = tmp_val + tmp - '0';

				tmp = tmp_mask[i];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_val = tmp - 'a' + 10;
				if (tmp >= 'A' && tmp <= 'F')
					tmp_val = tmp - 'A' + 10;
				if (tmp >= '0' && tmp <= '9')
					tmp_val = tmp - '0';

				tmp_val *= 16;
				tmp = tmp_mask[i + 1];
				if (tmp >= 'a' && tmp <= 'f')
					tmp_mask[j] = tmp_val + tmp - 'a' + 10;
				if (tmp >= 'A' && tmp <= 'F')
					tmp_mask[j] = tmp_val + tmp - 'A' + 10;
				if (tmp >= '0' && tmp <= '9')
					tmp_mask[j] = tmp_val + tmp - '0';
			}

			pkt_len /= 2;

			if (ice_parser_create(&ad->hw, &psr))
				return -rte_errno;
			if (ice_get_open_tunnel_port(&ad->hw, TNL_VXLAN,
						     &udp_port))
				ice_parser_vxlan_tunnel_set(psr, udp_port,
							    true);
			if (ice_parser_run(psr, tmp_spec, pkt_len, &rslt))
				return -rte_errno;
			ice_parser_destroy(psr);

			if (!tmp_mask)
				return -rte_errno;

			filter->prof = (struct ice_parser_profile *)
				ice_malloc(&ad->hw, sizeof(*filter->prof));
			if (!filter->prof)
				return -ENOMEM;

			if (ice_parser_profile_init(&rslt, tmp_spec, tmp_mask,
				pkt_len, ICE_BLK_FD, true, filter->prof))
				return -rte_errno;

			u8 *pkt_buf = (u8 *)ice_malloc(&ad->hw, pkt_len + 1);
			if (!pkt_buf)
				return -ENOMEM;
			rte_memcpy(pkt_buf, tmp_spec, pkt_len);
			filter->pkt_buf = pkt_buf;

			filter->pkt_len = pkt_len;

			filter->parser_ena = true;

			break;
		}

		case RTE_FLOW_ITEM_TYPE_ETH:
			flow_type = ICE_FLTR_PTYPE_NON_IP_L2;
			eth_spec = item->spec;
			eth_mask = item->mask;

			if (!(eth_spec && eth_mask))
				break;

			if (!rte_is_zero_ether_addr(&eth_mask->dst))
				*input_set |= ICE_INSET_DMAC;
			if (!rte_is_zero_ether_addr(&eth_mask->src))
				*input_set |= ICE_INSET_SMAC;

			next_type = (item + 1)->type;
			/* Ignore this field except for ICE_FLTR_PTYPE_NON_IP_L2 */
			if (eth_mask->type == RTE_BE16(0xffff) &&
			    next_type == RTE_FLOW_ITEM_TYPE_END) {
				*input_set |= ICE_INSET_ETHERTYPE;
				ether_type = rte_be_to_cpu_16(eth_spec->type);

				if (ether_type == RTE_ETHER_TYPE_IPV4 ||
				    ether_type == RTE_ETHER_TYPE_IPV6) {
					rte_flow_error_set(error, EINVAL,
							   RTE_FLOW_ERROR_TYPE_ITEM,
							   item,
							   "Unsupported ether_type.");
					return -rte_errno;
				}
			}

			p_ext_data = (tunnel_type && is_outer) ?
				     &filter->input.ext_data_outer :
				     &filter->input.ext_data;
			rte_memcpy(&p_ext_data->src_mac,
				   &eth_spec->src, RTE_ETHER_ADDR_LEN);
			rte_memcpy(&p_ext_data->dst_mac,
				   &eth_spec->dst, RTE_ETHER_ADDR_LEN);
			rte_memcpy(&p_ext_data->ether_type,
				   &eth_spec->type, sizeof(eth_spec->type));
			break;
		case RTE_FLOW_ITEM_TYPE_IPV4:
			flow_type = ICE_FLTR_PTYPE_NONF_IPV4_OTHER;
			l3 = RTE_FLOW_ITEM_TYPE_IPV4;
			ipv4_spec = item->spec;
			ipv4_last = item->last;
			ipv4_mask = item->mask;
			p_v4 = (tunnel_type && is_outer) ?
			       &filter->input.ip_outer.v4 :
			       &filter->input.ip.v4;

			if (!(ipv4_spec && ipv4_mask))
				break;

			/* Check IPv4 mask and update input set */
			if (ipv4_mask->hdr.version_ihl ||
			    ipv4_mask->hdr.total_length ||
			    ipv4_mask->hdr.hdr_checksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv4 mask.");
				return -rte_errno;
			}

			if (ipv4_last &&
			    (ipv4_last->hdr.version_ihl ||
			     ipv4_last->hdr.type_of_service ||
			     ipv4_last->hdr.time_to_live ||
			     ipv4_last->hdr.total_length |
			     ipv4_last->hdr.next_proto_id ||
			     ipv4_last->hdr.hdr_checksum ||
			     ipv4_last->hdr.src_addr ||
			     ipv4_last->hdr.dst_addr)) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv4 last.");
				return -rte_errno;
			}

			/* Mask for IPv4 src/dst addrs not supported */
			if (ipv4_mask->hdr.src_addr &&
				ipv4_mask->hdr.src_addr != UINT32_MAX)
				return -rte_errno;
			if (ipv4_mask->hdr.dst_addr &&
				ipv4_mask->hdr.dst_addr != UINT32_MAX)
				return -rte_errno;

			if (ipv4_mask->hdr.dst_addr == UINT32_MAX)
				*input_set |= ICE_INSET_IPV4_DST;
			if (ipv4_mask->hdr.src_addr == UINT32_MAX)
				*input_set |= ICE_INSET_IPV4_SRC;
			if (ipv4_mask->hdr.time_to_live == UINT8_MAX)
				*input_set |= ICE_INSET_IPV4_TTL;
			if (ipv4_mask->hdr.next_proto_id == UINT8_MAX)
				*input_set |= ICE_INSET_IPV4_PROTO;
			if (ipv4_mask->hdr.type_of_service == UINT8_MAX)
				*input_set |= ICE_INSET_IPV4_TOS;

			p_v4->dst_ip = ipv4_spec->hdr.dst_addr;
			p_v4->src_ip = ipv4_spec->hdr.src_addr;
			p_v4->ttl = ipv4_spec->hdr.time_to_live;
			p_v4->proto = ipv4_spec->hdr.next_proto_id;
			p_v4->tos = ipv4_spec->hdr.type_of_service;

			/* fragment Ipv4:
			 * spec is 0x2000, mask is 0x2000
			 */
			if (ipv4_spec->hdr.fragment_offset ==
			    rte_cpu_to_be_16(RTE_IPV4_HDR_MF_FLAG) &&
			    ipv4_mask->hdr.fragment_offset ==
			    rte_cpu_to_be_16(RTE_IPV4_HDR_MF_FLAG)) {
				/* all IPv4 fragment packet has the same
				 * ethertype, if the spec and mask is valid,
				 * set ethertype into input set.
				 */
				flow_type = ICE_FLTR_PTYPE_FRAG_IPV4;
				*input_set |= ICE_INSET_ETHERTYPE;
				input_set_o |= ICE_INSET_ETHERTYPE;
			} else if (ipv4_mask->hdr.packet_id == UINT16_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv4 mask.");
				return -rte_errno;
			}

			break;
		case RTE_FLOW_ITEM_TYPE_IPV6:
			flow_type = ICE_FLTR_PTYPE_NONF_IPV6_OTHER;
			l3 = RTE_FLOW_ITEM_TYPE_IPV6;
			ipv6_spec = item->spec;
			ipv6_mask = item->mask;
			p_v6 = (tunnel_type && is_outer) ?
			       &filter->input.ip_outer.v6 :
			       &filter->input.ip.v6;

			if (!(ipv6_spec && ipv6_mask))
				break;

			/* Check IPv6 mask and update input set */
			if (ipv6_mask->hdr.payload_len) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid IPv6 mask");
				return -rte_errno;
			}

			if (!memcmp(ipv6_mask->hdr.src_addr, ipv6_addr_mask,
				    RTE_DIM(ipv6_mask->hdr.src_addr)))
				*input_set |= ICE_INSET_IPV6_SRC;
			if (!memcmp(ipv6_mask->hdr.dst_addr, ipv6_addr_mask,
				    RTE_DIM(ipv6_mask->hdr.dst_addr)))
				*input_set |= ICE_INSET_IPV6_DST;

			if ((ipv6_mask->hdr.vtc_flow &
			     rte_cpu_to_be_32(ICE_IPV6_TC_MASK))
			    == rte_cpu_to_be_32(ICE_IPV6_TC_MASK))
				*input_set |= ICE_INSET_IPV6_TC;
			if (ipv6_mask->hdr.proto == UINT8_MAX)
				*input_set |= ICE_INSET_IPV6_NEXT_HDR;
			if (ipv6_mask->hdr.hop_limits == UINT8_MAX)
				*input_set |= ICE_INSET_IPV6_HOP_LIMIT;

			rte_memcpy(&p_v6->dst_ip, ipv6_spec->hdr.dst_addr, 16);
			rte_memcpy(&p_v6->src_ip, ipv6_spec->hdr.src_addr, 16);
			vtc_flow_cpu = rte_be_to_cpu_32(ipv6_spec->hdr.vtc_flow);
			p_v6->tc = (uint8_t)(vtc_flow_cpu >> ICE_FDIR_IPV6_TC_OFFSET);
			p_v6->proto = ipv6_spec->hdr.proto;
			p_v6->hlim = ipv6_spec->hdr.hop_limits;
			break;
		case RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT:
			l3 = RTE_FLOW_ITEM_TYPE_IPV6_FRAG_EXT;
			flow_type = ICE_FLTR_PTYPE_FRAG_IPV6;
			ipv6_frag_spec = item->spec;
			ipv6_frag_mask = item->mask;

			if (!(ipv6_frag_spec && ipv6_frag_mask))
				break;

			/* fragment Ipv6:
			 * spec is 0x1, mask is 0x1
			 */
			if (ipv6_frag_spec->hdr.frag_data ==
			    rte_cpu_to_be_16(1) &&
			    ipv6_frag_mask->hdr.frag_data ==
			    rte_cpu_to_be_16(1)) {
				/* all IPv6 fragment packet has the same
				 * ethertype, if the spec and mask is valid,
				 * set ethertype into input set.
				 */
				*input_set |= ICE_INSET_ETHERTYPE;
				input_set_o |= ICE_INSET_ETHERTYPE;
			} else if (ipv6_frag_mask->hdr.id == UINT32_MAX) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item, "Invalid IPv6 mask.");
				return -rte_errno;
			}

			break;

		case RTE_FLOW_ITEM_TYPE_TCP:
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_TCP;
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_TCP;

			tcp_spec = item->spec;
			tcp_mask = item->mask;

			if (!(tcp_spec && tcp_mask))
				break;

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

			/* Mask for TCP src/dst ports not supported */
			if (tcp_mask->hdr.src_port &&
				tcp_mask->hdr.src_port != UINT16_MAX)
				return -rte_errno;
			if (tcp_mask->hdr.dst_port &&
				tcp_mask->hdr.dst_port != UINT16_MAX)
				return -rte_errno;

			if (tcp_mask->hdr.src_port == UINT16_MAX)
				*input_set |= ICE_INSET_TCP_SRC_PORT;
			if (tcp_mask->hdr.dst_port == UINT16_MAX)
				*input_set |= ICE_INSET_TCP_DST_PORT;

			/* Get filter info */
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
				assert(p_v4);
				p_v4->dst_port = tcp_spec->hdr.dst_port;
				p_v4->src_port = tcp_spec->hdr.src_port;
			} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
				assert(p_v6);
				p_v6->dst_port = tcp_spec->hdr.dst_port;
				p_v6->src_port = tcp_spec->hdr.src_port;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_UDP:
			l4 = RTE_FLOW_ITEM_TYPE_UDP;
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_UDP;
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_UDP;

			udp_spec = item->spec;
			udp_mask = item->mask;

			if (!(udp_spec && udp_mask))
				break;

			/* Check UDP mask and update input set*/
			if (udp_mask->hdr.dgram_len ||
			    udp_mask->hdr.dgram_cksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return -rte_errno;
			}

			/* Mask for UDP src/dst ports not supported */
			if (udp_mask->hdr.src_port &&
				udp_mask->hdr.src_port != UINT16_MAX)
				return -rte_errno;
			if (udp_mask->hdr.dst_port &&
				udp_mask->hdr.dst_port != UINT16_MAX)
				return -rte_errno;

			if (udp_mask->hdr.src_port == UINT16_MAX)
				*input_set |= ICE_INSET_UDP_SRC_PORT;
			if (udp_mask->hdr.dst_port == UINT16_MAX)
				*input_set |= ICE_INSET_UDP_DST_PORT;

			/* Get filter info */
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
				assert(p_v4);
				p_v4->dst_port = udp_spec->hdr.dst_port;
				p_v4->src_port = udp_spec->hdr.src_port;
			} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
				assert(p_v6);
				p_v6->src_port = udp_spec->hdr.src_port;
				p_v6->dst_port = udp_spec->hdr.dst_port;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_SCTP:
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_SCTP;
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_SCTP;

			sctp_spec = item->spec;
			sctp_mask = item->mask;

			if (!(sctp_spec && sctp_mask))
				break;

			/* Check SCTP mask and update input set */
			if (sctp_mask->hdr.cksum) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid UDP mask");
				return -rte_errno;
			}

			/* Mask for SCTP src/dst ports not supported */
			if (sctp_mask->hdr.src_port &&
				sctp_mask->hdr.src_port != UINT16_MAX)
				return -rte_errno;
			if (sctp_mask->hdr.dst_port &&
				sctp_mask->hdr.dst_port != UINT16_MAX)
				return -rte_errno;

			if (sctp_mask->hdr.src_port == UINT16_MAX)
				*input_set |= ICE_INSET_SCTP_SRC_PORT;
			if (sctp_mask->hdr.dst_port == UINT16_MAX)
				*input_set |= ICE_INSET_SCTP_DST_PORT;

			/* Get filter info */
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4) {
				assert(p_v4);
				p_v4->dst_port = sctp_spec->hdr.dst_port;
				p_v4->src_port = sctp_spec->hdr.src_port;
			} else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6) {
				assert(p_v6);
				p_v6->dst_port = sctp_spec->hdr.dst_port;
				p_v6->src_port = sctp_spec->hdr.src_port;
			}
			break;
		case RTE_FLOW_ITEM_TYPE_VOID:
			break;
		case RTE_FLOW_ITEM_TYPE_VXLAN:
			l3 = RTE_FLOW_ITEM_TYPE_END;
			vxlan_spec = item->spec;
			vxlan_mask = item->mask;
			is_outer = false;

			if (!(vxlan_spec && vxlan_mask))
				break;

			if (vxlan_mask->hdr.vx_flags) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid vxlan field");
				return -rte_errno;
			}

			if (vxlan_mask->hdr.vx_vni)
				*input_set |= ICE_INSET_VXLAN_VNI;

			filter->input.vxlan_data.vni = vxlan_spec->hdr.vx_vni;

			break;
		case RTE_FLOW_ITEM_TYPE_GTPU:
			l3 = RTE_FLOW_ITEM_TYPE_END;
			tunnel_type = ICE_FDIR_TUNNEL_TYPE_GTPU;
			gtp_spec = item->spec;
			gtp_mask = item->mask;

			if (!(gtp_spec && gtp_mask))
				break;

			if (gtp_mask->v_pt_rsv_flags ||
			    gtp_mask->msg_type ||
			    gtp_mask->msg_len) {
				rte_flow_error_set(error, EINVAL,
						   RTE_FLOW_ERROR_TYPE_ITEM,
						   item,
						   "Invalid GTP mask");
				return -rte_errno;
			}

			if (gtp_mask->teid == UINT32_MAX)
				input_set_o |= ICE_INSET_GTPU_TEID;

			filter->input.gtpu_data.teid = gtp_spec->teid;
			break;
		case RTE_FLOW_ITEM_TYPE_GTP_PSC:
			tunnel_type = ICE_FDIR_TUNNEL_TYPE_GTPU_EH;
			gtp_psc_spec = item->spec;
			gtp_psc_mask = item->mask;

			if (!(gtp_psc_spec && gtp_psc_mask))
				break;

			if (gtp_psc_mask->hdr.qfi == 0x3F)
				input_set_o |= ICE_INSET_GTPU_QFI;

			filter->input.gtpu_data.qfi =
				gtp_psc_spec->hdr.qfi;
			break;
		case RTE_FLOW_ITEM_TYPE_ESP:
			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
			    l4 == RTE_FLOW_ITEM_TYPE_UDP)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_NAT_T_ESP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6 &&
				 l4 == RTE_FLOW_ITEM_TYPE_UDP)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_NAT_T_ESP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV4 &&
				 l4 == RTE_FLOW_ITEM_TYPE_END)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV4_ESP;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6 &&
				 l4 == RTE_FLOW_ITEM_TYPE_END)
				flow_type = ICE_FLTR_PTYPE_NONF_IPV6_ESP;

			esp_spec = item->spec;
			esp_mask = item->mask;

			if (!(esp_spec && esp_mask))
				break;

			if (esp_mask->hdr.spi == UINT32_MAX) {
				if (l4 == RTE_FLOW_ITEM_TYPE_UDP)
					*input_set |= ICE_INSET_NAT_T_ESP_SPI;
				else
					*input_set |= ICE_INSET_ESP_SPI;
			}

			if (l3 == RTE_FLOW_ITEM_TYPE_IPV4)
				filter->input.ip.v4.sec_parm_idx =
					esp_spec->hdr.spi;
			else if (l3 == RTE_FLOW_ITEM_TYPE_IPV6)
				filter->input.ip.v6.sec_parm_idx =
					esp_spec->hdr.spi;
			break;
		default:
			rte_flow_error_set(error, EINVAL,
					   RTE_FLOW_ERROR_TYPE_ITEM,
					   item,
					   "Invalid pattern item.");
			return -rte_errno;
		}
	}

	if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_GTPU &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV4_UDP)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV4_GTPU;
	else if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_GTPU_EH &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV4_UDP)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV4_GTPU_EH;
	else if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_GTPU &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV6_UDP)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV6_GTPU;
	else if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_GTPU_EH &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV6_UDP)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV6_GTPU_EH;
	else if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_VXLAN &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV4_UDP)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_UDP;
	else if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_VXLAN &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV4_TCP)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_TCP;
	else if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_VXLAN &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV4_SCTP)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_SCTP;
	else if (tunnel_type == ICE_FDIR_TUNNEL_TYPE_VXLAN &&
		flow_type == ICE_FLTR_PTYPE_NONF_IPV4_OTHER)
		flow_type = ICE_FLTR_PTYPE_NONF_IPV4_UDP_VXLAN_IPV4_OTHER;

	filter->tunnel_type = tunnel_type;
	filter->input.flow_type = flow_type;
	filter->input_set_o = input_set_o;
	filter->input_set_i = input_set_i;

	return 0;
}

static int
ice_fdir_parse(struct ice_adapter *ad,
	       struct ice_pattern_match_item *array,
	       uint32_t array_len,
	       const struct rte_flow_item pattern[],
	       const struct rte_flow_action actions[],
	       uint32_t priority,
	       void **meta,
	       struct rte_flow_error *error)
{
	struct ice_pf *pf = &ad->pf;
	struct ice_fdir_filter_conf *filter = &pf->fdir.conf;
	struct ice_pattern_match_item *item = NULL;
	uint64_t input_set;
	bool raw = false;
	int ret;

	memset(filter, 0, sizeof(*filter));
	item = ice_search_pattern_match_item(ad, pattern, array, array_len,
					     error);

	if (!ad->devargs.pipe_mode_support && priority >= 1)
		return -rte_errno;

	if (!item)
		return -rte_errno;

	ret = ice_fdir_parse_pattern(ad, pattern, error, filter);
	if (ret)
		goto error;

	if (item->pattern_list[0] == RTE_FLOW_ITEM_TYPE_RAW)
		raw = true;

	input_set = filter->input_set_o | filter->input_set_i;
	input_set = raw ? ~input_set : input_set;

	if (!input_set || filter->input_set_o &
	    ~(item->input_set_mask_o | ICE_INSET_ETHERTYPE) ||
	    filter->input_set_i & ~item->input_set_mask_i) {
		rte_flow_error_set(error, EINVAL,
				   RTE_FLOW_ERROR_TYPE_ITEM_SPEC,
				   pattern,
				   "Invalid input set");
		ret = -rte_errno;
		goto error;
	}

	ret = ice_fdir_parse_action(ad, actions, error, filter);
	if (ret)
		goto error;

	if (meta)
		*meta = filter;

	rte_free(item);
	return ret;
error:
	rte_free(filter->prof);
	rte_free(filter->pkt_buf);
	rte_free(item);
	return ret;
}

static struct ice_flow_parser ice_fdir_parser = {
	.engine = &ice_fdir_engine,
	.array = ice_fdir_pattern_list,
	.array_len = RTE_DIM(ice_fdir_pattern_list),
	.parse_pattern_action = ice_fdir_parse,
	.stage = ICE_FLOW_STAGE_DISTRIBUTOR,
};

RTE_INIT(ice_fdir_engine_register)
{
	ice_register_flow_engine(&ice_fdir_engine);
}
