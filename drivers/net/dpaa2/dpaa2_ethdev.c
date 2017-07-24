/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright 2016 NXP.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Freescale Semiconductor, Inc nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <time.h>
#include <net/if.h>

#include <rte_mbuf.h>
#include <rte_ethdev.h>
#include <rte_malloc.h>
#include <rte_memcpy.h>
#include <rte_string_fns.h>
#include <rte_cycles.h>
#include <rte_kvargs.h>
#include <rte_dev.h>
#include <rte_fslmc.h>

#include <fslmc_logs.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>
#include <dpaa2_hw_mempool.h>
#include <dpaa2_hw_dpio.h>
#include <mc/fsl_dpmng.h>
#include "dpaa2_ethdev.h"

static struct rte_dpaa2_driver rte_dpaa2_pmd;
static int dpaa2_dev_uninit(struct rte_eth_dev *eth_dev);
static int dpaa2_dev_set_link_up(struct rte_eth_dev *dev);
static int dpaa2_dev_set_link_down(struct rte_eth_dev *dev);
static int dpaa2_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu);

/**
 * Atomically reads the link status information from global
 * structure rte_eth_dev.
 *
 * @param dev
 *   - Pointer to the structure rte_eth_dev to read from.
 *   - Pointer to the buffer to be saved with the link status.
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */
static inline int
dpaa2_dev_atomic_read_link_status(struct rte_eth_dev *dev,
				  struct rte_eth_link *link)
{
	struct rte_eth_link *dst = link;
	struct rte_eth_link *src = &dev->data->dev_link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

/**
 * Atomically writes the link status information into global
 * structure rte_eth_dev.
 *
 * @param dev
 *   - Pointer to the structure rte_eth_dev to read from.
 *   - Pointer to the buffer to be saved with the link status.
 *
 * @return
 *   - On success, zero.
 *   - On failure, negative value.
 */
static inline int
dpaa2_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				   struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &dev->data->dev_link;
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
				*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static int
dpaa2_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = priv->hw;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return -1;
	}

	if (on)
		ret = dpni_add_vlan_id(dpni, CMD_PRI_LOW,
				       priv->token, vlan_id);
	else
		ret = dpni_remove_vlan_id(dpni, CMD_PRI_LOW,
					  priv->token, vlan_id);

	if (ret < 0)
		PMD_DRV_LOG(ERR, "ret = %d Unable to add/rem vlan %d hwid =%d",
			    ret, vlan_id, priv->hw_id);

	return ret;
}

static void
dpaa2_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = priv->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (mask & ETH_VLAN_FILTER_MASK) {
		if (dev->data->dev_conf.rxmode.hw_vlan_filter)
			ret = dpni_enable_vlan_filter(dpni, CMD_PRI_LOW,
						      priv->token, true);
		else
			ret = dpni_enable_vlan_filter(dpni, CMD_PRI_LOW,
						      priv->token, false);
		if (ret < 0)
			RTE_LOG(ERR, PMD, "Unable to set vlan filter = %d\n",
				ret);
	}
}

static int
dpaa2_fw_version_get(struct rte_eth_dev *dev,
		     char *fw_version,
		     size_t fw_size)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = priv->hw;
	struct mc_soc_version mc_plat_info = {0};
	struct mc_version mc_ver_info = {0};

	PMD_INIT_FUNC_TRACE();

	if (mc_get_soc_version(dpni, CMD_PRI_LOW, &mc_plat_info))
		RTE_LOG(WARNING, PMD, "\tmc_get_soc_version failed\n");

	if (mc_get_version(dpni, CMD_PRI_LOW, &mc_ver_info))
		RTE_LOG(WARNING, PMD, "\tmc_get_version failed\n");

	ret = snprintf(fw_version, fw_size,
		       "%x-%d.%d.%d",
		       mc_plat_info.svr,
		       mc_ver_info.major,
		       mc_ver_info.minor,
		       mc_ver_info.revision);

	ret += 1; /* add the size of '\0' */
	if (fw_size < (uint32_t)ret)
		return ret;
	else
		return 0;
}

static void
dpaa2_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	dev_info->if_index = priv->hw_id;

	dev_info->max_mac_addrs = priv->max_mac_filters;
	dev_info->max_rx_pktlen = DPAA2_MAX_RX_PKT_LEN;
	dev_info->min_rx_bufsize = DPAA2_MIN_RX_BUF_SIZE;
	dev_info->max_rx_queues = (uint16_t)priv->nb_rx_queues;
	dev_info->max_tx_queues = (uint16_t)priv->nb_tx_queues;
	dev_info->rx_offload_capa =
		DEV_RX_OFFLOAD_IPV4_CKSUM |
		DEV_RX_OFFLOAD_UDP_CKSUM |
		DEV_RX_OFFLOAD_TCP_CKSUM |
		DEV_RX_OFFLOAD_OUTER_IPV4_CKSUM;
	dev_info->tx_offload_capa =
		DEV_TX_OFFLOAD_IPV4_CKSUM |
		DEV_TX_OFFLOAD_UDP_CKSUM |
		DEV_TX_OFFLOAD_TCP_CKSUM |
		DEV_TX_OFFLOAD_SCTP_CKSUM |
		DEV_TX_OFFLOAD_OUTER_IPV4_CKSUM;
	dev_info->speed_capa = ETH_LINK_SPEED_1G |
			ETH_LINK_SPEED_2_5G |
			ETH_LINK_SPEED_10G;
}

static int
dpaa2_alloc_rx_tx_queues(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	uint16_t dist_idx;
	uint32_t vq_id;
	struct dpaa2_queue *mc_q, *mcq;
	uint32_t tot_queues;
	int i;
	struct dpaa2_queue *dpaa2_q;

	PMD_INIT_FUNC_TRACE();

	tot_queues = priv->nb_rx_queues + priv->nb_tx_queues;
	mc_q = rte_malloc(NULL, sizeof(struct dpaa2_queue) * tot_queues,
			  RTE_CACHE_LINE_SIZE);
	if (!mc_q) {
		PMD_INIT_LOG(ERR, "malloc failed for rx/tx queues\n");
		return -1;
	}

	for (i = 0; i < priv->nb_rx_queues; i++) {
		mc_q->dev = dev;
		priv->rx_vq[i] = mc_q++;
		dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
		dpaa2_q->q_storage = rte_malloc("dq_storage",
					sizeof(struct queue_storage_info_t),
					RTE_CACHE_LINE_SIZE);
		if (!dpaa2_q->q_storage)
			goto fail;

		memset(dpaa2_q->q_storage, 0,
		       sizeof(struct queue_storage_info_t));
		if (dpaa2_alloc_dq_storage(dpaa2_q->q_storage))
			goto fail;
	}

	for (i = 0; i < priv->nb_tx_queues; i++) {
		mc_q->dev = dev;
		mc_q->flow_id = 0xffff;
		priv->tx_vq[i] = mc_q++;
		dpaa2_q = (struct dpaa2_queue *)priv->tx_vq[i];
		dpaa2_q->cscn = rte_malloc(NULL,
					   sizeof(struct qbman_result), 16);
		if (!dpaa2_q->cscn)
			goto fail_tx;
	}

	vq_id = 0;
	for (dist_idx = 0; dist_idx < priv->nb_rx_queues; dist_idx++) {
		mcq = (struct dpaa2_queue *)priv->rx_vq[vq_id];
		mcq->tc_index = DPAA2_DEF_TC;
		mcq->flow_id = dist_idx;
		vq_id++;
	}

	return 0;
fail_tx:
	i -= 1;
	while (i >= 0) {
		dpaa2_q = (struct dpaa2_queue *)priv->tx_vq[i];
		rte_free(dpaa2_q->cscn);
		priv->tx_vq[i--] = NULL;
	}
	i = priv->nb_rx_queues;
fail:
	i -= 1;
	mc_q = priv->rx_vq[0];
	while (i >= 0) {
		dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
		dpaa2_free_dq_storage(dpaa2_q->q_storage);
		rte_free(dpaa2_q->q_storage);
		priv->rx_vq[i--] = NULL;
	}
	rte_free(mc_q);
	return -1;
}

static int
dpaa2_eth_dev_configure(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct rte_eth_conf *eth_conf = &data->dev_conf;
	int ret;

	PMD_INIT_FUNC_TRACE();

	if (eth_conf->rxmode.jumbo_frame == 1) {
		if (eth_conf->rxmode.max_rx_pkt_len <= DPAA2_MAX_RX_PKT_LEN) {
			ret = dpaa2_dev_mtu_set(dev,
					eth_conf->rxmode.max_rx_pkt_len);
			if (ret) {
				PMD_INIT_LOG(ERR,
					     "unable to set mtu. check config\n");
				return ret;
			}
		} else {
			return -1;
		}
	}

	/* Check for correct configuration */
	if (eth_conf->rxmode.mq_mode != ETH_MQ_RX_RSS &&
	    data->nb_rx_queues > 1) {
		PMD_INIT_LOG(ERR, "Distribution is not enabled, "
			    "but Rx queues more than 1\n");
		return -1;
	}

	if (eth_conf->rxmode.mq_mode == ETH_MQ_RX_RSS) {
		/* Return in case number of Rx queues is 1 */
		if (data->nb_rx_queues == 1)
			return 0;
		ret = dpaa2_setup_flow_dist(dev,
				eth_conf->rx_adv_conf.rss_conf.rss_hf);
		if (ret) {
			PMD_INIT_LOG(ERR, "unable to set flow distribution."
				     "please check queue config\n");
			return ret;
		}
	}
	return 0;
}

/* Function to setup RX flow information. It contains traffic class ID,
 * flow ID, destination configuration etc.
 */
static int
dpaa2_dev_rx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t rx_queue_id,
			 uint16_t nb_rx_desc __rte_unused,
			 unsigned int socket_id __rte_unused,
			 const struct rte_eth_rxconf *rx_conf __rte_unused,
			 struct rte_mempool *mb_pool)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	struct mc_soc_version mc_plat_info = {0};
	struct dpaa2_queue *dpaa2_q;
	struct dpni_queue cfg;
	uint8_t options = 0;
	uint8_t flow_id;
	uint32_t bpid;
	int ret;

	PMD_INIT_FUNC_TRACE();

	PMD_INIT_LOG(DEBUG, "dev =%p, queue =%d, pool = %p, conf =%p",
		     dev, rx_queue_id, mb_pool, rx_conf);

	if (!priv->bp_list || priv->bp_list->mp != mb_pool) {
		bpid = mempool_to_bpid(mb_pool);
		ret = dpaa2_attach_bp_list(priv,
					   rte_dpaa2_bpid_info[bpid].bp_list);
		if (ret)
			return ret;
	}
	dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[rx_queue_id];
	dpaa2_q->mb_pool = mb_pool; /**< mbuf pool to populate RX ring. */

	/*Get the flow id from given VQ id*/
	flow_id = rx_queue_id % priv->nb_rx_queues;
	memset(&cfg, 0, sizeof(struct dpni_queue));

	options = options | DPNI_QUEUE_OPT_USER_CTX;
	cfg.user_context = (uint64_t)(dpaa2_q);

	/*if ls2088 or rev2 device, enable the stashing */

	if (mc_get_soc_version(dpni, CMD_PRI_LOW, &mc_plat_info))
		PMD_INIT_LOG(ERR, "\tmc_get_soc_version failed\n");

	if ((mc_plat_info.svr & 0xffff0000) != SVR_LS2080A) {
		options |= DPNI_QUEUE_OPT_FLC;
		cfg.flc.stash_control = true;
		cfg.flc.value &= 0xFFFFFFFFFFFFFFC0;
		/* 00 00 00 - last 6 bit represent annotation, context stashing,
		 * data stashing setting 01 01 00 (0x14) to enable
		 * 1 line data, 1 line annotation
		 */
		cfg.flc.value |= 0x14;
	}
	ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token, DPNI_QUEUE_RX,
			     dpaa2_q->tc_index, flow_id, options, &cfg);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error in setting the rx flow: = %d\n", ret);
		return -1;
	}

	if (!(priv->flags & DPAA2_RX_TAILDROP_OFF)) {
		struct dpni_taildrop taildrop;

		taildrop.enable = 1;
		/*enabling per rx queue congestion control */
		taildrop.threshold = CONG_THRESHOLD_RX_Q;
		taildrop.units = DPNI_CONGESTION_UNIT_BYTES;
		PMD_INIT_LOG(DEBUG, "Enabling Early Drop on queue = %d",
			     rx_queue_id);
		ret = dpni_set_taildrop(dpni, CMD_PRI_LOW, priv->token,
					DPNI_CP_QUEUE, DPNI_QUEUE_RX,
					dpaa2_q->tc_index, flow_id, &taildrop);
		if (ret) {
			PMD_INIT_LOG(ERR, "Error in setting the rx flow"
				     " err : = %d\n", ret);
			return -1;
		}
	}

	dev->data->rx_queues[rx_queue_id] = dpaa2_q;
	return 0;
}

static int
dpaa2_dev_tx_queue_setup(struct rte_eth_dev *dev,
			 uint16_t tx_queue_id,
			 uint16_t nb_tx_desc __rte_unused,
			 unsigned int socket_id __rte_unused,
			 const struct rte_eth_txconf *tx_conf __rte_unused)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct dpaa2_queue *dpaa2_q = (struct dpaa2_queue *)
		priv->tx_vq[tx_queue_id];
	struct fsl_mc_io *dpni = priv->hw;
	struct dpni_queue tx_conf_cfg;
	struct dpni_queue tx_flow_cfg;
	uint8_t options = 0, flow_id;
	uint32_t tc_id;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Return if queue already configured */
	if (dpaa2_q->flow_id != 0xffff)
		return 0;

	memset(&tx_conf_cfg, 0, sizeof(struct dpni_queue));
	memset(&tx_flow_cfg, 0, sizeof(struct dpni_queue));

	tc_id = tx_queue_id;
	flow_id = 0;

	ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token, DPNI_QUEUE_TX,
			     tc_id, flow_id, options, &tx_flow_cfg);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error in setting the tx flow: "
			     "tc_id=%d, flow =%d ErrorCode = %x\n",
			     tc_id, flow_id, -ret);
			return -1;
	}

	dpaa2_q->flow_id = flow_id;

	if (tx_queue_id == 0) {
		/*Set tx-conf and error configuration*/
		ret = dpni_set_tx_confirmation_mode(dpni, CMD_PRI_LOW,
						    priv->token,
						    DPNI_CONF_DISABLE);
		if (ret) {
			PMD_INIT_LOG(ERR, "Error in set tx conf mode settings"
				     " ErrorCode = %x", ret);
			return -1;
		}
	}
	dpaa2_q->tc_index = tc_id;

	if (!(priv->flags & DPAA2_TX_CGR_OFF)) {
		struct dpni_congestion_notification_cfg cong_notif_cfg;

		cong_notif_cfg.units = DPNI_CONGESTION_UNIT_FRAMES;
		cong_notif_cfg.threshold_entry = CONG_ENTER_TX_THRESHOLD;
		/* Notify that the queue is not congested when the data in
		 * the queue is below this thershold.
		 */
		cong_notif_cfg.threshold_exit = CONG_EXIT_TX_THRESHOLD;
		cong_notif_cfg.message_ctx = 0;
		cong_notif_cfg.message_iova = (uint64_t)dpaa2_q->cscn;
		cong_notif_cfg.dest_cfg.dest_type = DPNI_DEST_NONE;
		cong_notif_cfg.notification_mode =
					 DPNI_CONG_OPT_WRITE_MEM_ON_ENTER |
					 DPNI_CONG_OPT_WRITE_MEM_ON_EXIT |
					 DPNI_CONG_OPT_COHERENT_WRITE;

		ret = dpni_set_congestion_notification(dpni, CMD_PRI_LOW,
						       priv->token,
						       DPNI_QUEUE_TX,
						       tc_id,
						       &cong_notif_cfg);
		if (ret) {
			PMD_INIT_LOG(ERR,
			   "Error in setting tx congestion notification: = %d",
			   -ret);
			return -ret;
		}
	}
	dev->data->tx_queues[tx_queue_id] = dpaa2_q;
	return 0;
}

static void
dpaa2_dev_rx_queue_release(void *q __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

static void
dpaa2_dev_tx_queue_release(void *q __rte_unused)
{
	PMD_INIT_FUNC_TRACE();
}

static const uint32_t *
dpaa2_supported_ptypes_get(struct rte_eth_dev *dev)
{
	static const uint32_t ptypes[] = {
		/*todo -= add more types */
		RTE_PTYPE_L2_ETHER,
		RTE_PTYPE_L3_IPV4,
		RTE_PTYPE_L3_IPV4_EXT,
		RTE_PTYPE_L3_IPV6,
		RTE_PTYPE_L3_IPV6_EXT,
		RTE_PTYPE_L4_TCP,
		RTE_PTYPE_L4_UDP,
		RTE_PTYPE_L4_SCTP,
		RTE_PTYPE_L4_ICMP,
		RTE_PTYPE_UNKNOWN
	};

	if (dev->rx_pkt_burst == dpaa2_dev_prefetch_rx)
		return ptypes;
	return NULL;
}

static int
dpaa2_dev_start(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct dpaa2_dev_priv *priv = data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	struct dpni_queue cfg;
	struct dpni_error_cfg	err_cfg;
	uint16_t qdid;
	struct dpni_queue_id qid;
	struct dpaa2_queue *dpaa2_q;
	int ret, i;

	PMD_INIT_FUNC_TRACE();

	ret = dpni_enable(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure %d in enabling dpni %d device\n",
			     ret, priv->hw_id);
		return ret;
	}

	/* Power up the phy. Needed to make the link go Up */
	dpaa2_dev_set_link_up(dev);

	ret = dpni_get_qdid(dpni, CMD_PRI_LOW, priv->token,
			    DPNI_QUEUE_TX, &qdid);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error to get qdid:ErrorCode = %d\n", ret);
		return ret;
	}
	priv->qdid = qdid;

	for (i = 0; i < data->nb_rx_queues; i++) {
		dpaa2_q = (struct dpaa2_queue *)data->rx_queues[i];
		ret = dpni_get_queue(dpni, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_RX, dpaa2_q->tc_index,
				       dpaa2_q->flow_id, &cfg, &qid);
		if (ret) {
			PMD_INIT_LOG(ERR, "Error to get flow "
				     "information Error code = %d\n", ret);
			return ret;
		}
		dpaa2_q->fqid = qid.fqid;
	}

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_RX_L3_CSUM, true);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error to set RX l3 csum:Error = %d\n", ret);
		return ret;
	}

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_RX_L4_CSUM, true);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error to get RX l4 csum:Error = %d\n", ret);
		return ret;
	}

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_TX_L3_CSUM, true);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error to set TX l3 csum:Error = %d\n", ret);
		return ret;
	}

	ret = dpni_set_offload(dpni, CMD_PRI_LOW, priv->token,
			       DPNI_OFF_TX_L4_CSUM, true);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error to get TX l4 csum:Error = %d\n", ret);
		return ret;
	}

	/*checksum errors, send them to normal path and set it in annotation */
	err_cfg.errors = DPNI_ERROR_L3CE | DPNI_ERROR_L4CE;

	err_cfg.error_action = DPNI_ERROR_ACTION_CONTINUE;
	err_cfg.set_frame_annotation = true;

	ret = dpni_set_errors_behavior(dpni, CMD_PRI_LOW,
				       priv->token, &err_cfg);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error to dpni_set_errors_behavior:"
			     "code = %d\n", ret);
		return ret;
	}
	/* VLAN Offload Settings */
	if (priv->max_vlan_filters)
		dpaa2_vlan_offload_set(dev, ETH_VLAN_FILTER_MASK);

	return 0;
}

/**
 *  This routine disables all traffic on the adapter by issuing a
 *  global reset on the MAC.
 */
static void
dpaa2_dev_stop(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	int ret;
	struct rte_eth_link link;

	PMD_INIT_FUNC_TRACE();

	dpaa2_dev_set_link_down(dev);

	ret = dpni_disable(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure (ret %d) in disabling dpni %d dev\n",
			     ret, priv->hw_id);
		return;
	}

	/* clear the recorded link status */
	memset(&link, 0, sizeof(link));
	dpaa2_dev_atomic_write_link_status(dev, &link);
}

static void
dpaa2_dev_close(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	int i, ret;
	struct rte_eth_link link;
	struct dpaa2_queue *dpaa2_q;

	PMD_INIT_FUNC_TRACE();

	for (i = 0; i < data->nb_tx_queues; i++) {
		dpaa2_q = (struct dpaa2_queue *)data->tx_queues[i];
		if (!dpaa2_q->cscn) {
			rte_free(dpaa2_q->cscn);
			dpaa2_q->cscn = NULL;
		}
	}

	/* Clean the device first */
	ret = dpni_reset(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure cleaning dpni device with"
			     " error code %d\n", ret);
		return;
	}

	memset(&link, 0, sizeof(link));
	dpaa2_dev_atomic_write_link_status(dev, &link);
}

static void
dpaa2_dev_promiscuous_enable(
		struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	ret = dpni_set_unicast_promisc(dpni, CMD_PRI_LOW, priv->token, true);
	if (ret < 0)
		RTE_LOG(ERR, PMD, "Unable to enable U promisc mode %d\n", ret);

	ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW, priv->token, true);
	if (ret < 0)
		RTE_LOG(ERR, PMD, "Unable to enable M promisc mode %d\n", ret);
}

static void
dpaa2_dev_promiscuous_disable(
		struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	ret = dpni_set_unicast_promisc(dpni, CMD_PRI_LOW, priv->token, false);
	if (ret < 0)
		RTE_LOG(ERR, PMD, "Unable to disable U promisc mode %d\n", ret);

	if (dev->data->all_multicast == 0) {
		ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW,
						 priv->token, false);
		if (ret < 0)
			RTE_LOG(ERR, PMD,
				"Unable to disable M promisc mode %d\n",
				ret);
	}
}

static void
dpaa2_dev_allmulticast_enable(
		struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW, priv->token, true);
	if (ret < 0)
		RTE_LOG(ERR, PMD, "Unable to enable multicast mode %d\n", ret);
}

static void
dpaa2_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	/* must remain on for all promiscuous */
	if (dev->data->promiscuous == 1)
		return;

	ret = dpni_set_multicast_promisc(dpni, CMD_PRI_LOW, priv->token, false);
	if (ret < 0)
		RTE_LOG(ERR, PMD, "Unable to disable multicast mode %d\n", ret);
}

static int
dpaa2_dev_mtu_set(struct rte_eth_dev *dev, uint16_t mtu)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	uint32_t frame_size = mtu + ETHER_HDR_LEN + ETHER_CRC_LEN;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return -EINVAL;
	}

	/* check that mtu is within the allowed range */
	if ((mtu < ETHER_MIN_MTU) || (frame_size > DPAA2_MAX_RX_PKT_LEN))
		return -EINVAL;

	if (frame_size > ETHER_MAX_LEN)
		dev->data->dev_conf.rxmode.jumbo_frame = 1;
	else
		dev->data->dev_conf.rxmode.jumbo_frame = 0;

	/* Set the Max Rx frame length as 'mtu' +
	 * Maximum Ethernet header length
	 */
	ret = dpni_set_max_frame_length(dpni, CMD_PRI_LOW, priv->token,
					mtu + ETH_VLAN_HLEN);
	if (ret) {
		PMD_DRV_LOG(ERR, "setting the max frame length failed");
		return -1;
	}
	PMD_DRV_LOG(INFO, "MTU is configured %d for the device\n", mtu);
	return 0;
}

static int
dpaa2_dev_add_mac_addr(struct rte_eth_dev *dev,
		       struct ether_addr *addr,
		       __rte_unused uint32_t index,
		       __rte_unused uint32_t pool)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return -1;
	}

	ret = dpni_add_mac_addr(dpni, CMD_PRI_LOW,
				priv->token, addr->addr_bytes);
	if (ret)
		RTE_LOG(ERR, PMD,
			"error: Adding the MAC ADDR failed: err = %d\n", ret);
	return 0;
}

static void
dpaa2_dev_remove_mac_addr(struct rte_eth_dev *dev,
			  uint32_t index)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	struct rte_eth_dev_data *data = dev->data;
	struct ether_addr *macaddr;

	PMD_INIT_FUNC_TRACE();

	macaddr = &data->mac_addrs[index];

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	ret = dpni_remove_mac_addr(dpni, CMD_PRI_LOW,
				   priv->token, macaddr->addr_bytes);
	if (ret)
		RTE_LOG(ERR, PMD,
			"error: Removing the MAC ADDR failed: err = %d\n", ret);
}

static void
dpaa2_dev_set_mac_addr(struct rte_eth_dev *dev,
		       struct ether_addr *addr)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	ret = dpni_set_primary_mac_addr(dpni, CMD_PRI_LOW,
					priv->token, addr->addr_bytes);

	if (ret)
		RTE_LOG(ERR, PMD,
			"error: Setting the MAC ADDR failed %d\n", ret);
}
static
void dpaa2_dev_stats_get(struct rte_eth_dev *dev,
			 struct rte_eth_stats *stats)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	int32_t  retcode;
	uint8_t page0 = 0, page1 = 1, page2 = 2;
	union dpni_statistics value;

	memset(&value, 0, sizeof(union dpni_statistics));

	PMD_INIT_FUNC_TRACE();

	if (!dpni) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	if (!stats) {
		RTE_LOG(ERR, PMD, "stats is NULL\n");
		return;
	}

	/*Get Counters from page_0*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      page0, &value);
	if (retcode)
		goto err;

	stats->ipackets = value.page_0.ingress_all_frames;
	stats->ibytes = value.page_0.ingress_all_bytes;

	/*Get Counters from page_1*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      page1, &value);
	if (retcode)
		goto err;

	stats->opackets = value.page_1.egress_all_frames;
	stats->obytes = value.page_1.egress_all_bytes;

	/*Get Counters from page_2*/
	retcode = dpni_get_statistics(dpni, CMD_PRI_LOW, priv->token,
				      page2, &value);
	if (retcode)
		goto err;

	/* Ingress drop frame count due to configured rules */
	stats->ierrors = value.page_2.ingress_filtered_frames;
	/* Ingress drop frame count due to error */
	stats->ierrors += value.page_2.ingress_discarded_frames;

	stats->oerrors = value.page_2.egress_discarded_frames;
	stats->imissed = value.page_2.ingress_nobuffer_discards;

	return;

err:
	RTE_LOG(ERR, PMD, "Operation not completed:Error Code = %d\n", retcode);
	return;
};

static
void dpaa2_dev_stats_reset(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	int32_t  retcode;

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return;
	}

	retcode =  dpni_reset_statistics(dpni, CMD_PRI_LOW, priv->token);
	if (retcode)
		goto error;

	return;

error:
	RTE_LOG(ERR, PMD, "Operation not completed:Error Code = %d\n", retcode);
	return;
};

/* return 0 means link status changed, -1 means not changed */
static int
dpaa2_dev_link_update(struct rte_eth_dev *dev,
			int wait_to_complete __rte_unused)
{
	int ret;
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	struct rte_eth_link link, old;
	struct dpni_link_state state = {0};

	PMD_INIT_FUNC_TRACE();

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return 0;
	}
	memset(&old, 0, sizeof(old));
	dpaa2_dev_atomic_read_link_status(dev, &old);

	ret = dpni_get_link_state(dpni, CMD_PRI_LOW, priv->token, &state);
	if (ret < 0) {
		RTE_LOG(ERR, PMD, "error: dpni_get_link_state %d\n", ret);
		return -1;
	}

	if ((old.link_status == state.up) && (old.link_speed == state.rate)) {
		RTE_LOG(DEBUG, PMD, "No change in status\n");
		return -1;
	}

	memset(&link, 0, sizeof(struct rte_eth_link));
	link.link_status = state.up;
	link.link_speed = state.rate;

	if (state.options & DPNI_LINK_OPT_HALF_DUPLEX)
		link.link_duplex = ETH_LINK_HALF_DUPLEX;
	else
		link.link_duplex = ETH_LINK_FULL_DUPLEX;

	dpaa2_dev_atomic_write_link_status(dev, &link);

	if (link.link_status)
		PMD_DRV_LOG(INFO, "Port %d Link is Up\n", dev->data->port_id);
	else
		PMD_DRV_LOG(INFO, "Port %d Link is Down\n", dev->data->port_id);
	return 0;
}

/**
 * Toggle the DPNI to enable, if not already enabled.
 * This is not strictly PHY up/down - it is more of logical toggling.
 */
static int
dpaa2_dev_set_link_up(struct rte_eth_dev *dev)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	int en = 0;

	PMD_INIT_FUNC_TRACE();

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)priv->hw;

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "DPNI is NULL\n");
		return ret;
	}

	/* Check if DPNI is currently enabled */
	ret = dpni_is_enabled(dpni, CMD_PRI_LOW, priv->token, &en);
	if (ret) {
		/* Unable to obtain dpni status; Not continuing */
		PMD_DRV_LOG(ERR, "Interface Link UP failed (%d)", ret);
		return -EINVAL;
	}

	/* Enable link if not already enabled */
	if (!en) {
		ret = dpni_enable(dpni, CMD_PRI_LOW, priv->token);
		if (ret) {
			PMD_DRV_LOG(ERR, "Interface Link UP failed (%d)", ret);
			return -EINVAL;
		}
	}
	/* changing tx burst function to start enqueues */
	dev->tx_pkt_burst = dpaa2_dev_tx;
	dev->data->dev_link.link_status = 1;

	PMD_DRV_LOG(INFO, "Port %d Link UP successful", dev->data->port_id);
	return ret;
}

/**
 * Toggle the DPNI to disable, if not already disabled.
 * This is not strictly PHY up/down - it is more of logical toggling.
 */
static int
dpaa2_dev_set_link_down(struct rte_eth_dev *dev)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	int dpni_enabled = 0;
	int retries = 10;

	PMD_INIT_FUNC_TRACE();

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)priv->hw;

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "Device has not yet been configured\n");
		return ret;
	}

	/*changing  tx burst function to avoid any more enqueues */
	dev->tx_pkt_burst = dummy_dev_tx;

	/* Loop while dpni_disable() attempts to drain the egress FQs
	 * and confirm them back to us.
	 */
	do {
		ret = dpni_disable(dpni, 0, priv->token);
		if (ret) {
			PMD_DRV_LOG(ERR, "dpni disable failed (%d)", ret);
			return ret;
		}
		ret = dpni_is_enabled(dpni, 0, priv->token, &dpni_enabled);
		if (ret) {
			PMD_DRV_LOG(ERR, "dpni_is_enabled failed (%d)", ret);
			return ret;
		}
		if (dpni_enabled)
			/* Allow the MC some slack */
			rte_delay_us(100 * 1000);
	} while (dpni_enabled && --retries);

	if (!retries) {
		PMD_DRV_LOG(WARNING, "Retry count exceeded disabling DPNI\n");
		/* todo- we may have to manually cleanup queues.
		 */
	} else {
		PMD_DRV_LOG(INFO, "Port %d Link DOWN successful",
			    dev->data->port_id);
	}

	dev->data->dev_link.link_status = 0;

	return ret;
}

static int
dpaa2_flow_ctrl_get(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	struct dpni_link_state state = {0};

	PMD_INIT_FUNC_TRACE();

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)priv->hw;

	if (dpni == NULL || fc_conf == NULL) {
		RTE_LOG(ERR, PMD, "device not configured\n");
		return ret;
	}

	ret = dpni_get_link_state(dpni, CMD_PRI_LOW, priv->token, &state);
	if (ret) {
		RTE_LOG(ERR, PMD, "error: dpni_get_link_state %d\n", ret);
		return ret;
	}

	memset(fc_conf, 0, sizeof(struct rte_eth_fc_conf));
	if (state.options & DPNI_LINK_OPT_PAUSE) {
		/* DPNI_LINK_OPT_PAUSE set
		 *  if ASYM_PAUSE not set,
		 *	RX Side flow control (handle received Pause frame)
		 *	TX side flow control (send Pause frame)
		 *  if ASYM_PAUSE set,
		 *	RX Side flow control (handle received Pause frame)
		 *	No TX side flow control (send Pause frame disabled)
		 */
		if (!(state.options & DPNI_LINK_OPT_ASYM_PAUSE))
			fc_conf->mode = RTE_FC_FULL;
		else
			fc_conf->mode = RTE_FC_RX_PAUSE;
	} else {
		/* DPNI_LINK_OPT_PAUSE not set
		 *  if ASYM_PAUSE set,
		 *	TX side flow control (send Pause frame)
		 *	No RX side flow control (No action on pause frame rx)
		 *  if ASYM_PAUSE not set,
		 *	Flow control disabled
		 */
		if (state.options & DPNI_LINK_OPT_ASYM_PAUSE)
			fc_conf->mode = RTE_FC_TX_PAUSE;
		else
			fc_conf->mode = RTE_FC_NONE;
	}

	return ret;
}

static int
dpaa2_flow_ctrl_set(struct rte_eth_dev *dev, struct rte_eth_fc_conf *fc_conf)
{
	int ret = -EINVAL;
	struct dpaa2_dev_priv *priv;
	struct fsl_mc_io *dpni;
	struct dpni_link_state state = {0};
	struct dpni_link_cfg cfg = {0};

	PMD_INIT_FUNC_TRACE();

	priv = dev->data->dev_private;
	dpni = (struct fsl_mc_io *)priv->hw;

	if (dpni == NULL) {
		RTE_LOG(ERR, PMD, "dpni is NULL\n");
		return ret;
	}

	/* It is necessary to obtain the current state before setting fc_conf
	 * as MC would return error in case rate, autoneg or duplex values are
	 * different.
	 */
	ret = dpni_get_link_state(dpni, CMD_PRI_LOW, priv->token, &state);
	if (ret) {
		RTE_LOG(ERR, PMD, "Unable to get link state (err=%d)\n", ret);
		return -1;
	}

	/* Disable link before setting configuration */
	dpaa2_dev_set_link_down(dev);

	/* Based on fc_conf, update cfg */
	cfg.rate = state.rate;
	cfg.options = state.options;

	/* update cfg with fc_conf */
	switch (fc_conf->mode) {
	case RTE_FC_FULL:
		/* Full flow control;
		 * OPT_PAUSE set, ASYM_PAUSE not set
		 */
		cfg.options |= DPNI_LINK_OPT_PAUSE;
		cfg.options &= ~DPNI_LINK_OPT_ASYM_PAUSE;
		break;
	case RTE_FC_TX_PAUSE:
		/* Enable RX flow control
		 * OPT_PAUSE not set;
		 * ASYM_PAUSE set;
		 */
		cfg.options |= DPNI_LINK_OPT_ASYM_PAUSE;
		cfg.options &= ~DPNI_LINK_OPT_PAUSE;
		break;
	case RTE_FC_RX_PAUSE:
		/* Enable TX Flow control
		 * OPT_PAUSE set
		 * ASYM_PAUSE set
		 */
		cfg.options |= DPNI_LINK_OPT_PAUSE;
		cfg.options |= DPNI_LINK_OPT_ASYM_PAUSE;
		break;
	case RTE_FC_NONE:
		/* Disable Flow control
		 * OPT_PAUSE not set
		 * ASYM_PAUSE not set
		 */
		cfg.options &= ~DPNI_LINK_OPT_PAUSE;
		cfg.options &= ~DPNI_LINK_OPT_ASYM_PAUSE;
		break;
	default:
		RTE_LOG(ERR, PMD, "Incorrect Flow control flag (%d)\n",
			fc_conf->mode);
		return -1;
	}

	ret = dpni_set_link_cfg(dpni, CMD_PRI_LOW, priv->token, &cfg);
	if (ret)
		RTE_LOG(ERR, PMD,
			"Unable to set Link configuration (err=%d)\n",
			ret);

	/* Enable link */
	dpaa2_dev_set_link_up(dev);

	return ret;
}

static struct eth_dev_ops dpaa2_ethdev_ops = {
	.dev_configure	  = dpaa2_eth_dev_configure,
	.dev_start	      = dpaa2_dev_start,
	.dev_stop	      = dpaa2_dev_stop,
	.dev_close	      = dpaa2_dev_close,
	.promiscuous_enable   = dpaa2_dev_promiscuous_enable,
	.promiscuous_disable  = dpaa2_dev_promiscuous_disable,
	.allmulticast_enable  = dpaa2_dev_allmulticast_enable,
	.allmulticast_disable = dpaa2_dev_allmulticast_disable,
	.dev_set_link_up      = dpaa2_dev_set_link_up,
	.dev_set_link_down    = dpaa2_dev_set_link_down,
	.link_update	   = dpaa2_dev_link_update,
	.stats_get	       = dpaa2_dev_stats_get,
	.stats_reset	   = dpaa2_dev_stats_reset,
	.fw_version_get	   = dpaa2_fw_version_get,
	.dev_infos_get	   = dpaa2_dev_info_get,
	.dev_supported_ptypes_get = dpaa2_supported_ptypes_get,
	.mtu_set           = dpaa2_dev_mtu_set,
	.vlan_filter_set      = dpaa2_vlan_filter_set,
	.vlan_offload_set     = dpaa2_vlan_offload_set,
	.rx_queue_setup    = dpaa2_dev_rx_queue_setup,
	.rx_queue_release  = dpaa2_dev_rx_queue_release,
	.tx_queue_setup    = dpaa2_dev_tx_queue_setup,
	.tx_queue_release  = dpaa2_dev_tx_queue_release,
	.flow_ctrl_get	      = dpaa2_flow_ctrl_get,
	.flow_ctrl_set	      = dpaa2_flow_ctrl_set,
	.mac_addr_add         = dpaa2_dev_add_mac_addr,
	.mac_addr_remove      = dpaa2_dev_remove_mac_addr,
	.mac_addr_set         = dpaa2_dev_set_mac_addr,
};

static int
dpaa2_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_device *dev = eth_dev->device;
	struct rte_dpaa2_device *dpaa2_dev;
	struct fsl_mc_io *dpni_dev;
	struct dpni_attr attr;
	struct dpaa2_dev_priv *priv = eth_dev->data->dev_private;
	struct dpni_buffer_layout layout;
	int ret, hw_id;

	PMD_INIT_FUNC_TRACE();

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dpaa2_dev = container_of(dev, struct rte_dpaa2_device, device);

	hw_id = dpaa2_dev->object_id;

	dpni_dev = rte_malloc(NULL, sizeof(struct fsl_mc_io), 0);
	if (!dpni_dev) {
		PMD_INIT_LOG(ERR, "malloc failed for dpni device\n");
		return -1;
	}

	dpni_dev->regs = rte_mcp_ptr_list[0];
	ret = dpni_open(dpni_dev, CMD_PRI_LOW, hw_id, &priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "Failure in opening dpni@%d with err code %d\n",
			     hw_id, ret);
		rte_free(dpni_dev);
		return -1;
	}

	/* Clean the device first */
	ret = dpni_reset(dpni_dev, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "Failure cleaning dpni@%d with err code %d\n",
			     hw_id, ret);
		goto init_err;
	}

	ret = dpni_get_attributes(dpni_dev, CMD_PRI_LOW, priv->token, &attr);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "Failure in get dpni@%d attribute, err code %d\n",
			     hw_id, ret);
		goto init_err;
	}

	priv->num_tc = attr.num_tcs;

	/* Resetting the "num_rx_vqueues" to equal number of queues in first TC
	 * as only one TC is supported on Rx Side. Once Multiple TCs will be
	 * in use for Rx processing then this will be changed or removed.
	 */
	priv->nb_rx_queues = attr.num_queues;

	/* TODO:Using hard coded value for number of TX queues due to dependency
	 * in MC.
	 */
	priv->nb_tx_queues = 8;

	PMD_INIT_LOG(DEBUG, "num TC - RX %d", priv->num_tc);
	PMD_INIT_LOG(DEBUG, "nb_tx_queues %d", priv->nb_tx_queues);
	PMD_INIT_LOG(DEBUG, "nb_rx_queues %d", priv->nb_rx_queues);

	priv->hw = dpni_dev;
	priv->hw_id = hw_id;
	priv->options = attr.options;
	priv->max_mac_filters = attr.mac_filter_entries;
	priv->max_vlan_filters = attr.vlan_filter_entries;
	priv->flags = 0;

	/* Allocate memory for hardware structure for queues */
	ret = dpaa2_alloc_rx_tx_queues(eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "dpaa2_alloc_rx_tx_queuesFailed\n");
		goto init_err;
	}

	/* Allocate memory for storing MAC addresses */
	eth_dev->data->mac_addrs = rte_zmalloc("dpni",
		ETHER_ADDR_LEN * attr.mac_filter_entries, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR,
		   "Failed to allocate %d bytes needed to store MAC addresses",
			     ETHER_ADDR_LEN * attr.mac_filter_entries);
		ret = -ENOMEM;
		goto init_err;
	}

	ret = dpni_get_primary_mac_addr(dpni_dev, CMD_PRI_LOW,
					priv->token,
			(uint8_t *)(eth_dev->data->mac_addrs[0].addr_bytes));
	if (ret) {
		PMD_INIT_LOG(ERR, "DPNI get mac address failed:Err Code = %d\n",
			     ret);
		goto init_err;
	}

	/* ... tx buffer layout ... */
	memset(&layout, 0, sizeof(struct dpni_buffer_layout));
	layout.options = DPNI_BUF_LAYOUT_OPT_FRAME_STATUS;
	layout.pass_frame_status = 1;
	ret = dpni_set_buffer_layout(dpni_dev, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_TX, &layout);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error (%d) in setting tx buffer layout",
			     ret);
		goto init_err;
	}

	/* ... tx-conf and error buffer layout ... */
	memset(&layout, 0, sizeof(struct dpni_buffer_layout));
	layout.options = DPNI_BUF_LAYOUT_OPT_FRAME_STATUS;
	layout.pass_frame_status = 1;
	ret = dpni_set_buffer_layout(dpni_dev, CMD_PRI_LOW, priv->token,
				     DPNI_QUEUE_TX_CONFIRM, &layout);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error (%d) in setting tx-conf buffer layout",
			     ret);
		goto init_err;
	}

	eth_dev->dev_ops = &dpaa2_ethdev_ops;

	eth_dev->rx_pkt_burst = dpaa2_dev_prefetch_rx;
	eth_dev->tx_pkt_burst = dpaa2_dev_tx;
	rte_fslmc_vfio_dmamap();

	return 0;
init_err:
	dpaa2_dev_uninit(eth_dev);
	return ret;
}

static int
dpaa2_dev_uninit(struct rte_eth_dev *eth_dev)
{
	struct dpaa2_dev_priv *priv = eth_dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	int i, ret;
	struct dpaa2_queue *dpaa2_q;

	PMD_INIT_FUNC_TRACE();

	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	if (!dpni) {
		PMD_INIT_LOG(WARNING, "Already closed or not started");
		return -1;
	}

	dpaa2_dev_close(eth_dev);

	if (priv->rx_vq[0]) {
		/* cleaning up queue storage */
		for (i = 0; i < priv->nb_rx_queues; i++) {
			dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
			if (dpaa2_q->q_storage)
				rte_free(dpaa2_q->q_storage);
		}
		/*free the all queue memory */
		rte_free(priv->rx_vq[0]);
		priv->rx_vq[0] = NULL;
	}

	/* free memory for storing MAC addresses */
	if (eth_dev->data->mac_addrs) {
		rte_free(eth_dev->data->mac_addrs);
		eth_dev->data->mac_addrs = NULL;
	}

	/* Close the device at underlying layer*/
	ret = dpni_close(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR,
			     "Failure closing dpni device with err code %d\n",
			     ret);
	}

	/* Free the allocated memory for ethernet private data and dpni*/
	priv->hw = NULL;
	rte_free(dpni);

	eth_dev->dev_ops = NULL;
	eth_dev->rx_pkt_burst = NULL;
	eth_dev->tx_pkt_burst = NULL;

	return 0;
}

static int
rte_dpaa2_probe(struct rte_dpaa2_driver *dpaa2_drv,
		struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_eth_dev *eth_dev;
	int diag;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
		eth_dev = rte_eth_dev_allocate(dpaa2_dev->device.name);
		if (!eth_dev)
			return -ENODEV;
		eth_dev->data->dev_private = rte_zmalloc(
						"ethdev private structure",
						sizeof(struct dpaa2_dev_priv),
						RTE_CACHE_LINE_SIZE);
		if (eth_dev->data->dev_private == NULL) {
			PMD_INIT_LOG(CRIT, "Cannot allocate memzone for"
				     " private port data\n");
			rte_eth_dev_release_port(eth_dev);
			return -ENOMEM;
		}
	} else {
		eth_dev = rte_eth_dev_attach_secondary(dpaa2_dev->device.name);
		if (!eth_dev)
			return -ENODEV;
	}

	eth_dev->device = &dpaa2_dev->device;
	eth_dev->device->driver = &dpaa2_drv->driver;

	dpaa2_dev->eth_dev = eth_dev;
	eth_dev->data->rx_mbuf_alloc_failed = 0;

	/* Invoke PMD device initialization function */
	diag = dpaa2_dev_init(eth_dev);
	if (diag == 0)
		return 0;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);
	rte_eth_dev_release_port(eth_dev);
	return diag;
}

static int
rte_dpaa2_remove(struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_eth_dev *eth_dev;

	eth_dev = dpaa2_dev->eth_dev;
	dpaa2_dev_uninit(eth_dev);

	if (rte_eal_process_type() == RTE_PROC_PRIMARY)
		rte_free(eth_dev->data->dev_private);
	rte_eth_dev_release_port(eth_dev);

	return 0;
}

static struct rte_dpaa2_driver rte_dpaa2_pmd = {
	.drv_type = DPAA2_MC_DPNI_DEVID,
	.probe = rte_dpaa2_probe,
	.remove = rte_dpaa2_remove,
};

RTE_PMD_REGISTER_DPAA2(net_dpaa2, rte_dpaa2_pmd);
