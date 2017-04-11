/*-
 *   BSD LICENSE
 *
 *   Copyright (c) 2016 Freescale Semiconductor, Inc. All rights reserved.
 *   Copyright (c) 2016 NXP. All rights reserved.
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
#include <rte_ethdev.h>
#include <rte_fslmc.h>

#include <fslmc_logs.h>
#include <fslmc_vfio.h>
#include <dpaa2_hw_pvt.h>

#include "dpaa2_ethdev.h"

static struct rte_dpaa2_driver rte_dpaa2_pmd;

static void
dpaa2_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;

	PMD_INIT_FUNC_TRACE();

	dev_info->if_index = priv->hw_id;

	dev_info->max_rx_queues = (uint16_t)priv->nb_rx_queues;
	dev_info->max_tx_queues = (uint16_t)priv->nb_tx_queues;

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
		dpaa2_q->q_storage->dq_storage[0] = rte_malloc(NULL,
			DPAA2_DQRR_RING_SIZE * sizeof(struct qbman_result),
			RTE_CACHE_LINE_SIZE);
	}

	for (i = 0; i < priv->nb_tx_queues; i++) {
		mc_q->dev = dev;
		mc_q->flow_id = DPNI_NEW_FLOW_ID;
		priv->tx_vq[i] = mc_q++;
	}

	vq_id = 0;
	for (dist_idx = 0; dist_idx < priv->nb_rx_queues; dist_idx++) {
		mcq = (struct dpaa2_queue *)priv->rx_vq[vq_id];
		mcq->tc_index = DPAA2_DEF_TC;
		mcq->flow_id = dist_idx;
		vq_id++;
	}

	return 0;
fail:
	i -= 1;
	mc_q = priv->rx_vq[0];
	while (i >= 0) {
		dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[i];
		rte_free(dpaa2_q->q_storage->dq_storage[0]);
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

	PMD_INIT_FUNC_TRACE();

	/* Check for correct configuration */
	if (eth_conf->rxmode.mq_mode != ETH_MQ_RX_RSS &&
	    data->nb_rx_queues > 1) {
		PMD_INIT_LOG(ERR, "Distribution is not enabled, "
			    "but Rx queues more than 1\n");
		return -1;
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
	struct dpaa2_queue *dpaa2_q;
	struct dpni_queue cfg;
	uint8_t options = 0;
	uint8_t flow_id;
	int ret;

	PMD_INIT_FUNC_TRACE();

	PMD_INIT_LOG(DEBUG, "dev =%p, queue =%d, pool = %p, conf =%p",
		     dev, rx_queue_id, mb_pool, rx_conf);

	dpaa2_q = (struct dpaa2_queue *)priv->rx_vq[rx_queue_id];
	dpaa2_q->mb_pool = mb_pool; /**< mbuf pool to populate RX ring. */

	/*Get the tc id and flow id from given VQ id*/
	flow_id = rx_queue_id;
	memset(&cfg, 0, sizeof(struct dpni_queue));

	options = options | DPNI_QUEUE_OPT_USER_CTX;
	cfg.user_context = (uint64_t)(dpaa2_q);

	ret = dpni_set_queue(dpni, CMD_PRI_LOW, priv->token, DPNI_QUEUE_RX,
			     dpaa2_q->tc_index, flow_id, options, &cfg);
	if (ret) {
		PMD_INIT_LOG(ERR, "Error in setting the rx flow: = %d\n", ret);
		return -1;
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
	if (dpaa2_q->flow_id != DPNI_NEW_FLOW_ID)
		return 0;

	memset(&tx_conf_cfg, 0, sizeof(struct dpni_queue));
	memset(&tx_flow_cfg, 0, sizeof(struct dpni_queue));

	tc_id = 0;
	flow_id = tx_queue_id;

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

static int
dpaa2_dev_start(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	struct dpaa2_dev_priv *priv = data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	struct dpni_queue cfg;
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

	PMD_INIT_FUNC_TRACE();

	ret = dpni_disable(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure (ret %d) in disabling dpni %d dev\n",
			     ret, priv->hw_id);
		return;
	}
}

static void
dpaa2_dev_close(struct rte_eth_dev *dev)
{
	struct dpaa2_dev_priv *priv = dev->data->dev_private;
	struct fsl_mc_io *dpni = (struct fsl_mc_io *)priv->hw;
	int ret;

	PMD_INIT_FUNC_TRACE();

	/* Clean the device first */
	ret = dpni_reset(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure cleaning dpni device with"
			     " error code %d\n", ret);
		return;
	}
}

static struct eth_dev_ops dpaa2_ethdev_ops = {
	.dev_configure	  = dpaa2_eth_dev_configure,
	.dev_start	      = dpaa2_dev_start,
	.dev_stop	      = dpaa2_dev_stop,
	.dev_close	      = dpaa2_dev_close,
	.dev_infos_get	   = dpaa2_dev_info_get,
	.rx_queue_setup    = dpaa2_dev_rx_queue_setup,
	.rx_queue_release  = dpaa2_dev_rx_queue_release,
	.tx_queue_setup    = dpaa2_dev_tx_queue_setup,
	.tx_queue_release  = dpaa2_dev_tx_queue_release,
};

static int
dpaa2_dev_init(struct rte_eth_dev *eth_dev)
{
	struct rte_device *dev = eth_dev->device;
	struct rte_dpaa2_device *dpaa2_dev;
	struct fsl_mc_io *dpni_dev;
	struct dpni_attr attr;
	struct dpaa2_dev_priv *priv = eth_dev->data->dev_private;
	int ret, hw_id;

	PMD_INIT_FUNC_TRACE();

	/* For secondary processes, the primary has done all the work */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY)
		return 0;

	dpaa2_dev = container_of(dev, struct rte_dpaa2_device, device);

	hw_id = dpaa2_dev->object_id;

	dpni_dev = (struct fsl_mc_io *)malloc(sizeof(struct fsl_mc_io));
	if (!dpni_dev) {
		PMD_INIT_LOG(ERR, "malloc failed for dpni device\n");
		return -1;
	}

	dpni_dev->regs = rte_mcp_ptr_list[0];
	ret = dpni_open(dpni_dev, CMD_PRI_LOW, hw_id, &priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure in opening dpni@%d device with"
			" error code %d\n", hw_id, ret);
		return -1;
	}

	/* Clean the device first */
	ret = dpni_reset(dpni_dev, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure cleaning dpni@%d device with"
			" error code %d\n", hw_id, ret);
		return -1;
	}

	ret = dpni_get_attributes(dpni_dev, CMD_PRI_LOW, priv->token, &attr);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure in getting dpni@%d attribute, "
			" error code %d\n", hw_id, ret);
		return -1;
	}

	priv->num_tc = attr.num_tcs;
	priv->nb_rx_queues = attr.num_queues;
	priv->nb_tx_queues = attr.num_queues;

	priv->hw = dpni_dev;
	priv->hw_id = hw_id;
	priv->flags = 0;

	/* Allocate memory for hardware structure for queues */
	ret = dpaa2_alloc_rx_tx_queues(eth_dev);
	if (ret) {
		PMD_INIT_LOG(ERR, "dpaa2_alloc_rx_tx_queuesFailed\n");
		return -ret;
	}

	eth_dev->dev_ops = &dpaa2_ethdev_ops;
	eth_dev->data->drv_name = rte_dpaa2_pmd.driver.name;

	return 0;
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
		return -EPERM;

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


	/*Close the device at underlying layer*/
	ret = dpni_close(dpni, CMD_PRI_LOW, priv->token);
	if (ret) {
		PMD_INIT_LOG(ERR, "Failure closing dpni device with"
			" error code %d\n", ret);
	}

	/*Free the allocated memory for ethernet private data and dpni*/
	priv->hw = NULL;
	free(dpni);

	eth_dev->dev_ops = NULL;

	return 0;
}

static int
rte_dpaa2_probe(struct rte_dpaa2_driver *dpaa2_drv __rte_unused,
		struct rte_dpaa2_device *dpaa2_dev)
{
	struct rte_eth_dev *eth_dev;
	char ethdev_name[RTE_ETH_NAME_MAX_LEN];

	int diag;

	sprintf(ethdev_name, "dpni-%d", dpaa2_dev->object_id);

	eth_dev = rte_eth_dev_allocate(ethdev_name);
	if (eth_dev == NULL)
		return -ENOMEM;

	if (rte_eal_process_type() == RTE_PROC_PRIMARY) {
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
	}
	eth_dev->device = &dpaa2_dev->device;
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
