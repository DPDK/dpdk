/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
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
 *     * Neither the name of Intel Corporation nor the names of its
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

#include <sys/queue.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_cycles.h>

#include <rte_interrupts.h>
#include <rte_log.h>
#include <rte_debug.h>
#include <rte_pci.h>
#include <rte_atomic.h>
#include <rte_branch_prediction.h>
#include <rte_memory.h>
#include <rte_memzone.h>
#include <rte_tailq.h>
#include <rte_eal.h>
#include <rte_alarm.h>
#include <rte_ether.h>
#include <rte_ethdev.h>
#include <rte_atomic.h>
#include <rte_malloc.h>
#include <rte_dev.h>

#include "i40e_logs.h"
#include "i40e/i40e_prototype.h"
#include "i40e/i40e_adminq_cmd.h"
#include "i40e/i40e_type.h"

#include "i40e_rxtx.h"
#include "i40e_ethdev.h"
#include "i40e_pf.h"
#define I40EVF_VSI_DEFAULT_MSIX_INTR 1

/* busy wait delay in msec */
#define I40EVF_BUSY_WAIT_DELAY 10
#define I40EVF_BUSY_WAIT_COUNT 50
#define MAX_RESET_WAIT_CNT     20

struct i40evf_arq_msg_info {
	enum i40e_virtchnl_ops ops;
	enum i40e_status_code result;
	uint16_t msg_len;
	uint8_t *msg;
};

struct vf_cmd_info {
	enum i40e_virtchnl_ops ops;
	uint8_t *in_args;
	uint32_t in_args_size;
	uint8_t *out_buffer;
	/* Input & output type. pass in buffer size and pass out
	 * actual return result
	 */
	uint32_t out_size;
};

enum i40evf_aq_result {
	I40EVF_MSG_ERR = -1, /* Meet error when accessing admin queue */
	I40EVF_MSG_NON,      /* Read nothing from admin queue */
	I40EVF_MSG_SYS,      /* Read system msg from admin queue */
	I40EVF_MSG_CMD,      /* Read async command result */
};

/* A share buffer to store the command result from PF driver */
static uint8_t cmd_result_buffer[I40E_AQ_BUF_SZ];

static int i40evf_dev_configure(struct rte_eth_dev *dev);
static int i40evf_dev_start(struct rte_eth_dev *dev);
static void i40evf_dev_stop(struct rte_eth_dev *dev);
static void i40evf_dev_info_get(struct rte_eth_dev *dev,
				struct rte_eth_dev_info *dev_info);
static int i40evf_dev_link_update(struct rte_eth_dev *dev,
				  __rte_unused int wait_to_complete);
static void i40evf_dev_stats_get(struct rte_eth_dev *dev,
				struct rte_eth_stats *stats);
static int i40evf_vlan_filter_set(struct rte_eth_dev *dev,
				  uint16_t vlan_id, int on);
static void i40evf_vlan_offload_set(struct rte_eth_dev *dev, int mask);
static int i40evf_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid,
				int on);
static void i40evf_dev_close(struct rte_eth_dev *dev);
static void i40evf_dev_promiscuous_enable(struct rte_eth_dev *dev);
static void i40evf_dev_promiscuous_disable(struct rte_eth_dev *dev);
static void i40evf_dev_allmulticast_enable(struct rte_eth_dev *dev);
static void i40evf_dev_allmulticast_disable(struct rte_eth_dev *dev);
static int i40evf_get_link_status(struct rte_eth_dev *dev,
				  struct rte_eth_link *link);
static int i40evf_init_vlan(struct rte_eth_dev *dev);
static struct eth_dev_ops i40evf_eth_dev_ops = {
	.dev_configure        = i40evf_dev_configure,
	.dev_start            = i40evf_dev_start,
	.dev_stop             = i40evf_dev_stop,
	.promiscuous_enable   = i40evf_dev_promiscuous_enable,
	.promiscuous_disable  = i40evf_dev_promiscuous_disable,
	.allmulticast_enable  = i40evf_dev_allmulticast_enable,
	.allmulticast_disable = i40evf_dev_allmulticast_disable,
	.link_update          = i40evf_dev_link_update,
	.stats_get            = i40evf_dev_stats_get,
	.dev_close            = i40evf_dev_close,
	.dev_infos_get        = i40evf_dev_info_get,
	.vlan_filter_set      = i40evf_vlan_filter_set,
	.vlan_offload_set     = i40evf_vlan_offload_set,
	.vlan_pvid_set        = i40evf_vlan_pvid_set,
	.rx_queue_setup       = i40e_dev_rx_queue_setup,
	.rx_queue_release     = i40e_dev_rx_queue_release,
	.tx_queue_setup       = i40e_dev_tx_queue_setup,
	.tx_queue_release     = i40e_dev_tx_queue_release,
};

static int
i40evf_set_mac_type(struct i40e_hw *hw)
{
	int status = I40E_ERR_DEVICE_NOT_SUPPORTED;

	if (hw->vendor_id == I40E_INTEL_VENDOR_ID) {
		switch (hw->device_id) {
		case I40E_DEV_ID_VF:
		case I40E_DEV_ID_VF_HV:
			hw->mac.type = I40E_MAC_VF;
			status = I40E_SUCCESS;
			break;
		default:
			;
		}
	}

	return status;
}

/*
 * Parse admin queue message.
 *
 * return value:
 *  < 0: meet error
 *  0: read sys msg
 *  > 0: read cmd result
 */
static enum i40evf_aq_result
i40evf_parse_pfmsg(struct i40e_vf *vf,
		   struct i40e_arq_event_info *event,
		   struct i40evf_arq_msg_info *data)
{
	enum i40e_virtchnl_ops opcode = (enum i40e_virtchnl_ops)\
			rte_le_to_cpu_32(event->desc.cookie_high);
	enum i40e_status_code retval = (enum i40e_status_code)\
			rte_le_to_cpu_32(event->desc.cookie_low);
	enum i40evf_aq_result ret = I40EVF_MSG_CMD;

	/* pf sys event */
	if (opcode == I40E_VIRTCHNL_OP_EVENT) {
		struct i40e_virtchnl_pf_event *vpe =
			(struct i40e_virtchnl_pf_event *)event->msg_buf;

		/* Initialize ret to sys event */
		ret = I40EVF_MSG_SYS;
		switch (vpe->event) {
		case I40E_VIRTCHNL_EVENT_LINK_CHANGE:
			vf->link_up =
				vpe->event_data.link_event.link_status;
			vf->pend_msg |= PFMSG_LINK_CHANGE;
			PMD_DRV_LOG(INFO, "Link status update:%s\n",
					vf->link_up ? "up" : "down");
			break;
		case I40E_VIRTCHNL_EVENT_RESET_IMPENDING:
			vf->vf_reset = true;
			vf->pend_msg |= PFMSG_RESET_IMPENDING;
			PMD_DRV_LOG(INFO, "vf is reseting\n");
			break;
		case I40E_VIRTCHNL_EVENT_PF_DRIVER_CLOSE:
			vf->dev_closed = true;
			vf->pend_msg |= PFMSG_DRIVER_CLOSE;
			PMD_DRV_LOG(INFO, "PF driver closed\n");
			break;
		default:
			PMD_DRV_LOG(ERR,
				"%s: Unknown event %d from pf\n",
				__func__, vpe->event);
		}
	} else {
		/* async reply msg on command issued by vf previously */
		ret = I40EVF_MSG_CMD;
		/* Actual buffer length read from PF */
		data->msg_len = event->msg_size;
	}
	/* fill the ops and result to notify VF */
	data->result = retval;
	data->ops = opcode;

	return ret;
}

/*
 * Read data in admin queue to get msg from pf driver
 */
static enum i40evf_aq_result
i40evf_read_pfmsg(struct rte_eth_dev *dev, struct i40evf_arq_msg_info *data)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct i40e_arq_event_info event;
	int ret;
	enum i40evf_aq_result result = I40EVF_MSG_NON;

	event.msg_size = data->msg_len;
	event.msg_buf = data->msg;
	ret = i40e_clean_arq_element(hw, &event, NULL);
	/* Can't read any msg from adminQ */
	if (ret) {
		if (ret == I40E_ERR_ADMIN_QUEUE_NO_WORK)
			result = I40EVF_MSG_NON;
		else
			result = I40EVF_MSG_ERR;
		return result;
	}

	/* Parse the event */
	result = i40evf_parse_pfmsg(vf, &event, data);

	return result;
}

/*
 * Polling read until command result return from pf driver or meet error.
 */
static int
i40evf_wait_cmd_done(struct rte_eth_dev *dev,
		     struct i40evf_arq_msg_info *data)
{
	int i = 0;
	enum i40evf_aq_result ret;

#define MAX_TRY_TIMES 10
#define ASQ_DELAY_MS  50
	do {
		/* Delay some time first */
		rte_delay_ms(ASQ_DELAY_MS);
		ret = i40evf_read_pfmsg(dev, data);

		if (ret == I40EVF_MSG_CMD)
			return 0;
		else if (ret == I40EVF_MSG_ERR)
			return -1;

		/* If don't read msg or read sys event, continue */
	} while(i++ < MAX_TRY_TIMES);

	return -1;
}

/**
 * clear current command. Only call in case execute
 * _atomic_set_cmd successfully.
 */
static inline void
_clear_cmd(struct i40e_vf *vf)
{
	rte_wmb();
	vf->pend_cmd = I40E_VIRTCHNL_OP_UNKNOWN;
}

/*
 * Check there is pending cmd in execution. If none, set new command.
 */
static inline int
_atomic_set_cmd(struct i40e_vf *vf, enum i40e_virtchnl_ops ops)
{
	int ret = rte_atomic32_cmpset(&vf->pend_cmd,
			I40E_VIRTCHNL_OP_UNKNOWN, ops);

	if (!ret)
		PMD_DRV_LOG(ERR, "There is incomplete cmd %d\n", vf->pend_cmd);

	return !ret;
}

static int
i40evf_execute_vf_cmd(struct rte_eth_dev *dev, struct vf_cmd_info *args)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int err = -1;
	struct i40evf_arq_msg_info info;

	if (_atomic_set_cmd(vf, args->ops))
		return -1;

	info.msg = args->out_buffer;
	info.msg_len = args->out_size;
	info.ops = I40E_VIRTCHNL_OP_UNKNOWN;
	info.result = I40E_SUCCESS;

	err = i40e_aq_send_msg_to_pf(hw, args->ops, I40E_SUCCESS,
		     args->in_args, args->in_args_size, NULL);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to send cmd %d\n", args->ops);
		return err;
	}

	err = i40evf_wait_cmd_done(dev, &info);
	/* read message and it's expected one */
	if (!err && args->ops == info.ops)
		_clear_cmd(vf);
	else if (err)
		PMD_DRV_LOG(ERR, "Failed to read message from AdminQ\n");
	else if (args->ops != info.ops)
		PMD_DRV_LOG(ERR, "command mismatch, expect %u, get %u\n",
				args->ops, info.ops);

	return (err | info.result);
}

/*
 * Check API version with sync wait until version read or fail from admin queue
 */
static int
i40evf_check_api_version(struct rte_eth_dev *dev)
{
	struct i40e_virtchnl_version_info version, *pver;
	int err;
	struct vf_cmd_info args;
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	version.major = I40E_VIRTCHNL_VERSION_MAJOR;
	version.minor = I40E_VIRTCHNL_VERSION_MINOR;

	args.ops = I40E_VIRTCHNL_OP_VERSION;
	args.in_args = (uint8_t *)&version;
	args.in_args_size = sizeof(version);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;

	err = i40evf_execute_vf_cmd(dev, &args);
	if (err) {
		PMD_INIT_LOG(ERR, "fail to execute command OP_VERSION\n");
		return err;
	}

	pver = (struct i40e_virtchnl_version_info *)args.out_buffer;
	/* We are talking with DPDK host */
	if (pver->major == I40E_DPDK_VERSION_MAJOR) {
		vf->host_is_dpdk = TRUE;
		PMD_DRV_LOG(INFO, "Detect PF host is DPDK app\n");
	}
	/* It's linux host driver */
	else if ((pver->major != version.major) ||
	    (pver->minor != version.minor)) {
		PMD_INIT_LOG(ERR, "pf/vf API version mismatch. "
			"(%u.%u)-(%u.%u)\n", pver->major, pver->minor,
					version.major, version.minor);
		return -1;
	}

	return 0;
}

static int
i40evf_get_vf_resource(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int err;
	struct vf_cmd_info args;
	uint32_t len;

	args.ops = I40E_VIRTCHNL_OP_GET_VF_RESOURCES;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;

	err = i40evf_execute_vf_cmd(dev, &args);

	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command "
					"OP_GET_VF_RESOURCE\n");
		return err;
	}

	len =  sizeof(struct i40e_virtchnl_vf_resource) +
		I40E_MAX_VF_VSI * sizeof(struct i40e_virtchnl_vsi_resource);

	(void)rte_memcpy(vf->vf_res, args.out_buffer,
			RTE_MIN(args.out_size, len));
	i40e_vf_parse_hw_config(hw, vf->vf_res);

	return 0;
}

static int
i40evf_config_promisc(struct rte_eth_dev *dev,
		      bool enable_unicast,
		      bool enable_multicast)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int err;
	struct vf_cmd_info args;
	struct i40e_virtchnl_promisc_info promisc;

	promisc.flags = 0;
	promisc.vsi_id = vf->vsi_res->vsi_id;

	if (enable_unicast)
		promisc.flags |= I40E_FLAG_VF_UNICAST_PROMISC;

	if (enable_multicast)
		promisc.flags |= I40E_FLAG_VF_MULTICAST_PROMISC;

	args.ops = I40E_VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE;
	args.in_args = (uint8_t *)&promisc;
	args.in_args_size = sizeof(promisc);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;

	err = i40evf_execute_vf_cmd(dev, &args);

	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command "
				"CONFIG_PROMISCUOUS_MODE\n");
	return err;
}

/* Configure vlan and double vlan offload. Use flag to specify which part to configure */
static int
i40evf_config_vlan_offload(struct rte_eth_dev *dev,
				bool enable_vlan_strip)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int err;
	struct vf_cmd_info args;
	struct i40e_virtchnl_vlan_offload_info offload;

	offload.vsi_id = vf->vsi_res->vsi_id;
	offload.enable_vlan_strip = enable_vlan_strip;

	args.ops = (enum i40e_virtchnl_ops)I40E_VIRTCHNL_OP_CFG_VLAN_OFFLOAD;
	args.in_args = (uint8_t *)&offload;
	args.in_args_size = sizeof(offload);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;

	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command CFG_VLAN_OFFLOAD\n");

	return err;
}

static int
i40evf_config_vlan_pvid(struct rte_eth_dev *dev,
				struct i40e_vsi_vlan_pvid_info *info)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int err;
	struct vf_cmd_info args;
	struct i40e_virtchnl_pvid_info tpid_info;

	if (dev == NULL || info == NULL) {
		PMD_DRV_LOG(ERR, "invalid parameters\n");
		return I40E_ERR_PARAM;
	}

	memset(&tpid_info, 0, sizeof(tpid_info));
	tpid_info.vsi_id = vf->vsi_res->vsi_id;
	(void)rte_memcpy(&tpid_info.info, info, sizeof(*info));

	args.ops = (enum i40e_virtchnl_ops)I40E_VIRTCHNL_OP_CFG_VLAN_PVID;
	args.in_args = (uint8_t *)&tpid_info;
	args.in_args_size = sizeof(tpid_info);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;

	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command CFG_VLAN_PVID\n");

	return err;
}

static int
i40evf_configure_queues(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct i40e_virtchnl_vsi_queue_config_info *queue_info;
	struct i40e_virtchnl_queue_pair_info *queue_cfg;
	struct i40e_rx_queue **rxq =
		(struct i40e_rx_queue **)dev->data->rx_queues;
	struct i40e_tx_queue **txq =
		(struct i40e_tx_queue **)dev->data->tx_queues;
	int i, len, nb_qpairs, num_rxq, num_txq;
	int err;
	struct vf_cmd_info args;
	struct rte_pktmbuf_pool_private *mbp_priv;

	nb_qpairs = vf->num_queue_pairs;
	len = sizeof(*queue_info) + sizeof(*queue_cfg) * nb_qpairs;
	queue_info = rte_zmalloc("queue_info", len, 0);
	if (queue_info == NULL) {
		PMD_INIT_LOG(ERR, "failed alloc memory for queue_info\n");
		return -1;
	}
	queue_info->vsi_id = vf->vsi_res->vsi_id;
	queue_info->num_queue_pairs = nb_qpairs;
	queue_cfg = queue_info->qpair;

	num_rxq = dev->data->nb_rx_queues;
	num_txq = dev->data->nb_tx_queues;
	/*
	 * PF host driver required to configure queues in pairs, which means
	 * rxq_num should equals to txq_num. The actual usage won't always
	 * work that way. The solution is fills 0 with HW ring option in case
	 * they are not equal.
	 */
	for (i = 0; i < nb_qpairs; i++) {
		/*Fill TX info */
		queue_cfg->txq.vsi_id = queue_info->vsi_id;
		queue_cfg->txq.queue_id = i;
		if (i < num_txq) {
			queue_cfg->txq.ring_len = txq[i]->nb_tx_desc;
			queue_cfg->txq.dma_ring_addr = txq[i]->tx_ring_phys_addr;
		} else {
			queue_cfg->txq.ring_len = 0;
			queue_cfg->txq.dma_ring_addr = 0;
		}

		/* Fill RX info */
		queue_cfg->rxq.vsi_id = queue_info->vsi_id;
		queue_cfg->rxq.queue_id = i;
		queue_cfg->rxq.max_pkt_size = vf->max_pkt_len;
		if (i < num_rxq) {
			mbp_priv = rte_mempool_get_priv(rxq[i]->mp);
			queue_cfg->rxq.databuffer_size = mbp_priv->mbuf_data_room_size -
						   RTE_PKTMBUF_HEADROOM;;
			queue_cfg->rxq.ring_len = rxq[i]->nb_rx_desc;
			queue_cfg->rxq.dma_ring_addr = rxq[i]->rx_ring_phys_addr;;
		} else {
			queue_cfg->rxq.ring_len = 0;
			queue_cfg->rxq.dma_ring_addr = 0;
			queue_cfg->rxq.databuffer_size = 0;
		}
		queue_cfg++;
	}

	args.ops = I40E_VIRTCHNL_OP_CONFIG_VSI_QUEUES;
	args.in_args = (u8 *)queue_info;
	args.in_args_size = len;
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command "
				"OP_CONFIG_VSI_QUEUES\n");
	rte_free(queue_info);

	return err;
}

static int
i40evf_config_irq_map(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct vf_cmd_info args;
	uint8_t cmd_buffer[sizeof(struct i40e_virtchnl_irq_map_info) + \
		sizeof(struct i40e_virtchnl_vector_map)];
	struct i40e_virtchnl_irq_map_info *map_info;
	int i, err;
	map_info = (struct i40e_virtchnl_irq_map_info *)cmd_buffer;
	map_info->num_vectors = 1;
	map_info->vecmap[0].rxitr_idx = RTE_LIBRTE_I40E_ITR_INTERVAL / 2;
	map_info->vecmap[0].txitr_idx = RTE_LIBRTE_I40E_ITR_INTERVAL / 2;
	map_info->vecmap[0].vsi_id = vf->vsi_res->vsi_id;
	/* Alway use default dynamic MSIX interrupt */
	map_info->vecmap[0].vector_id = I40EVF_VSI_DEFAULT_MSIX_INTR;
	/* Don't map any tx queue */
	map_info->vecmap[0].txq_map = 0;
	map_info->vecmap[0].rxq_map = 0;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		map_info->vecmap[0].rxq_map |= 1 << i;

	args.ops = I40E_VIRTCHNL_OP_CONFIG_IRQ_MAP;
	args.in_args = (u8 *)cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_ENABLE_QUEUES\n");

	return err;
}

static int
i40evf_enable_queues(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct i40e_virtchnl_queue_select queue_select;
	int err, i;
	struct vf_cmd_info args;

	queue_select.vsi_id = vf->vsi_res->vsi_id;

	queue_select.rx_queues = 0;
	/* Enable configured RX queues */
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		queue_select.rx_queues |= 1 << i;

	/* Enable configured TX queues */
	queue_select.tx_queues = 0;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		queue_select.tx_queues |= 1 << i;

	args.ops = I40E_VIRTCHNL_OP_ENABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_ENABLE_QUEUES\n");

	return err;
}

static int
i40evf_disable_queues(struct rte_eth_dev *dev)
{
	struct i40e_virtchnl_queue_select queue_select;
	int err, i;
	struct vf_cmd_info args;

	/* Enable configured RX queues */
	queue_select.rx_queues = 0;
	for (i = 0; i < dev->data->nb_rx_queues; i++)
		queue_select.rx_queues |= 1 << i;

	/* Enable configured TX queues */
	queue_select.tx_queues = 0;
	for (i = 0; i < dev->data->nb_tx_queues; i++)
		queue_select.tx_queues |= 1 << i;

	args.ops = I40E_VIRTCHNL_OP_DISABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command "
					"OP_DISABLE_QUEUES\n");

	return err;
}

static int
i40evf_add_mac_addr(struct rte_eth_dev *dev, struct ether_addr *addr)
{
	struct i40e_virtchnl_ether_addr_list *list;
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	uint8_t cmd_buffer[sizeof(struct i40e_virtchnl_ether_addr_list) + \
			sizeof(struct i40e_virtchnl_ether_addr)];
	int err;
	struct vf_cmd_info args;

	if (i40e_validate_mac_addr(addr->addr_bytes) != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Invalid mac:%x:%x:%x:%x:%x:%x\n",
			addr->addr_bytes[0], addr->addr_bytes[1],
			addr->addr_bytes[2], addr->addr_bytes[3],
			addr->addr_bytes[4], addr->addr_bytes[5]);
		return -1;
	}

	list = (struct i40e_virtchnl_ether_addr_list *)cmd_buffer;
	list->vsi_id = vf->vsi_res->vsi_id;
	list->num_elements = 1;
	(void)rte_memcpy(list->list[0].addr, addr->addr_bytes,
					sizeof(addr->addr_bytes));

	args.ops = I40E_VIRTCHNL_OP_ADD_ETHER_ADDRESS;
	args.in_args = cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command "
				"OP_ADD_ETHER_ADDRESS\n");

	return err;
}

static int
i40evf_del_mac_addr(struct rte_eth_dev *dev, struct ether_addr *addr)
{
	struct i40e_virtchnl_ether_addr_list *list;
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	uint8_t cmd_buffer[sizeof(struct i40e_virtchnl_ether_addr_list) + \
			sizeof(struct i40e_virtchnl_ether_addr)];
	int err;
	struct vf_cmd_info args;

	if (i40e_validate_mac_addr(addr->addr_bytes) != I40E_SUCCESS) {
		PMD_DRV_LOG(ERR, "Invalid mac:%x-%x-%x-%x-%x-%x\n",
			addr->addr_bytes[0], addr->addr_bytes[1],
			addr->addr_bytes[2], addr->addr_bytes[3],
			addr->addr_bytes[4], addr->addr_bytes[5]);
		return -1;
	}

	list = (struct i40e_virtchnl_ether_addr_list *)cmd_buffer;
	list->vsi_id = vf->vsi_res->vsi_id;
	list->num_elements = 1;
	(void)rte_memcpy(list->list[0].addr, addr->addr_bytes,
			sizeof(addr->addr_bytes));

	args.ops = I40E_VIRTCHNL_OP_DEL_ETHER_ADDRESS;
	args.in_args = cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command "
				"OP_DEL_ETHER_ADDRESS\n");

	return err;
}

static int
i40evf_get_statics(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct i40e_virtchnl_queue_select q_stats;
	struct i40e_eth_stats *pstats;
	int err;
	struct vf_cmd_info args;

	memset(&q_stats, 0, sizeof(q_stats));
	q_stats.vsi_id = vf->vsi_res->vsi_id;
	args.ops = I40E_VIRTCHNL_OP_GET_STATS;
	args.in_args = (u8 *)&q_stats;
	args.in_args_size = sizeof(q_stats);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;

	err = i40evf_execute_vf_cmd(dev, &args);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_GET_STATS\n");
		return err;
	}
	pstats = (struct i40e_eth_stats *)args.out_buffer;
	stats->ipackets = pstats->rx_unicast + pstats->rx_multicast +
						pstats->rx_broadcast;
	stats->opackets = pstats->tx_broadcast + pstats->tx_multicast +
						pstats->tx_unicast;
	stats->ierrors = pstats->rx_discards;
	stats->oerrors = pstats->tx_errors + pstats->tx_discards;
	stats->ibytes = pstats->rx_bytes;
	stats->obytes = pstats->tx_bytes;

	return 0;
}

static int
i40evf_add_vlan(struct rte_eth_dev *dev, uint16_t vlanid)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct i40e_virtchnl_vlan_filter_list *vlan_list;
	uint8_t cmd_buffer[sizeof(struct i40e_virtchnl_vlan_filter_list) +
							sizeof(uint16_t)];
	int err;
	struct vf_cmd_info args;

	vlan_list = (struct i40e_virtchnl_vlan_filter_list *)cmd_buffer;
	vlan_list->vsi_id = vf->vsi_res->vsi_id;
	vlan_list->num_elements = 1;
	vlan_list->vlan_id[0] = vlanid;

	args.ops = I40E_VIRTCHNL_OP_ADD_VLAN;
	args.in_args = (u8 *)&cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_ADD_VLAN\n");

	return err;
}

static int
i40evf_del_vlan(struct rte_eth_dev *dev, uint16_t vlanid)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct i40e_virtchnl_vlan_filter_list *vlan_list;
	uint8_t cmd_buffer[sizeof(struct i40e_virtchnl_vlan_filter_list) +
							sizeof(uint16_t)];
	int err;
	struct vf_cmd_info args;

	vlan_list = (struct i40e_virtchnl_vlan_filter_list *)cmd_buffer;
	vlan_list->vsi_id = vf->vsi_res->vsi_id;
	vlan_list->num_elements = 1;
	vlan_list->vlan_id[0] = vlanid;

	args.ops = I40E_VIRTCHNL_OP_DEL_VLAN;
	args.in_args = (u8 *)&cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_DEL_VLAN\n");

	return err;
}

static int
i40evf_get_link_status(struct rte_eth_dev *dev, struct rte_eth_link *link)
{
	int err;
	struct vf_cmd_info args;
	struct rte_eth_link *new_link;

	args.ops = (enum i40e_virtchnl_ops)I40E_VIRTCHNL_OP_GET_LINK_STAT;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = cmd_result_buffer;
	args.out_size = I40E_AQ_BUF_SZ;
	err = i40evf_execute_vf_cmd(dev, &args);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_GET_LINK_STAT\n");
		return err;
	}

	new_link = (struct rte_eth_link *)args.out_buffer;
	(void)rte_memcpy(link, new_link, sizeof(*link));

	return 0;
}

static struct rte_pci_id pci_id_i40evf_map[] = {
#define RTE_PCI_DEV_ID_DECL_I40EVF(vend, dev) {RTE_PCI_DEVICE(vend, dev)},
#include "rte_pci_dev_ids.h"
{ .vendor_id = 0, /* sentinel */ },
};

static inline int
i40evf_dev_atomic_write_link_status(struct rte_eth_dev *dev,
				    struct rte_eth_link *link)
{
	struct rte_eth_link *dst = &(dev->data->dev_link);
	struct rte_eth_link *src = link;

	if (rte_atomic64_cmpset((uint64_t *)dst, *(uint64_t *)dst,
					*(uint64_t *)src) == 0)
		return -1;

	return 0;
}

static int
i40evf_reset_vf(struct i40e_hw *hw)
{
	int i, reset;

	if (i40e_vf_reset(hw) != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "Reset VF NIC failed\n");
		return -1;
	}
	/**
	  * After issuing vf reset command to pf, pf won't necessarily
	  * reset vf, it depends on what state it exactly is. If it's not
	  * initialized yet, it won't have vf reset since it's in a certain
	  * state. If not, it will try to reset. Even vf is reset, pf will
	  * set I40E_VFGEN_RSTAT to COMPLETE first, then wait 10ms and set
	  * it to ACTIVE. In this duration, vf may not catch the moment that
	  * COMPLETE is set. So, for vf, we'll try to wait a long time.
	  */
	rte_delay_ms(200);

	for (i = 0; i < MAX_RESET_WAIT_CNT; i++) {
		reset = rd32(hw, I40E_VFGEN_RSTAT) &
			I40E_VFGEN_RSTAT_VFR_STATE_MASK;
		reset = reset >> I40E_VFGEN_RSTAT_VFR_STATE_SHIFT;
		if (I40E_VFR_COMPLETED == reset || I40E_VFR_VFACTIVE == reset)
			break;
		else
			rte_delay_ms(50);
	}

	if (i >= MAX_RESET_WAIT_CNT) {
		PMD_INIT_LOG(ERR, "Reset VF NIC failed\n");
		return -1;
	}

	return 0;
}

static int
i40evf_init_vf(struct rte_eth_dev *dev)
{
	int i, err, bufsz;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	err = i40evf_set_mac_type(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "set_mac_type failed: %d\n", err);
		goto err;
	}

	i40e_init_adminq_parameter(hw);
	err = i40e_init_adminq(hw);
	if (err) {
		PMD_INIT_LOG(ERR, "init_adminq failed: %d\n", err);
		goto err;
	}


	/* Reset VF and wait until it's complete */
	if (i40evf_reset_vf(hw)) {
		PMD_INIT_LOG(ERR, "reset NIC failed\n");
		goto err_aq;
	}

	/* VF reset, shutdown admin queue and initialize again */
	if (i40e_shutdown_adminq(hw) != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "i40e_shutdown_adminq failed\n");
		return -1;
	}

	i40e_init_adminq_parameter(hw);
	if (i40e_init_adminq(hw) != I40E_SUCCESS) {
		PMD_INIT_LOG(ERR, "init_adminq failed\n");
		return -1;
	}
	if (i40evf_check_api_version(dev) != 0) {
		PMD_INIT_LOG(ERR, "check_api version failed\n");
		goto err_aq;
	}
	bufsz = sizeof(struct i40e_virtchnl_vf_resource) +
		(I40E_MAX_VF_VSI * sizeof(struct i40e_virtchnl_vsi_resource));
	vf->vf_res = rte_zmalloc("vf_res", bufsz, 0);
	if (!vf->vf_res) {
		PMD_INIT_LOG(ERR, "unable to allocate vf_res memory\n");
			goto err_aq;
	}

	if (i40evf_get_vf_resource(dev) != 0) {
		PMD_INIT_LOG(ERR, "i40evf_get_vf_config failed\n");
		goto err_alloc;
	}

	/* got VF config message back from PF, now we can parse it */
	for (i = 0; i < vf->vf_res->num_vsis; i++) {
		if (vf->vf_res->vsi_res[i].vsi_type == I40E_VSI_SRIOV)
			vf->vsi_res = &vf->vf_res->vsi_res[i];
	}

	if (!vf->vsi_res) {
		PMD_INIT_LOG(ERR, "no LAN VSI found\n");
		goto err_alloc;
	}

	vf->vsi.vsi_id = vf->vsi_res->vsi_id;
	vf->vsi.type = vf->vsi_res->vsi_type;
	vf->vsi.nb_qps = vf->vsi_res->num_queue_pairs;

	/* check mac addr, if it's not valid, genrate one */
	if (I40E_SUCCESS != i40e_validate_mac_addr(\
			vf->vsi_res->default_mac_addr))
		eth_random_addr(vf->vsi_res->default_mac_addr);

	ether_addr_copy((struct ether_addr *)vf->vsi_res->default_mac_addr,
					(struct ether_addr *)hw->mac.addr);

	return 0;

err_alloc:
	rte_free(vf->vf_res);
err_aq:
	i40e_shutdown_adminq(hw); /* ignore error */
err:
	return -1;
}

static int
i40evf_dev_init(__rte_unused struct eth_driver *eth_drv,
		struct rte_eth_dev *eth_dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(\
			eth_dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	/* assign ops func pointer */
	eth_dev->dev_ops = &i40evf_eth_dev_ops;
	eth_dev->rx_pkt_burst = &i40e_recv_pkts;
	eth_dev->tx_pkt_burst = &i40e_xmit_pkts;

	/*
	 * For secondary processes, we don't initialise any further as primary
	 * has already done this work.
	 */
	if (rte_eal_process_type() != RTE_PROC_PRIMARY){
		if (eth_dev->data->scattered_rx)
			eth_dev->rx_pkt_burst = i40e_recv_scattered_pkts;
		return 0;
	}

	hw->vendor_id = eth_dev->pci_dev->id.vendor_id;
	hw->device_id = eth_dev->pci_dev->id.device_id;
	hw->subsystem_vendor_id = eth_dev->pci_dev->id.subsystem_vendor_id;
	hw->subsystem_device_id = eth_dev->pci_dev->id.subsystem_device_id;
	hw->bus.device = eth_dev->pci_dev->addr.devid;
	hw->bus.func = eth_dev->pci_dev->addr.function;
	hw->hw_addr = (void *)eth_dev->pci_dev->mem_resource[0].addr;

	if(i40evf_init_vf(eth_dev) != 0) {
		PMD_INIT_LOG(ERR, "Init vf failed\n");
		return -1;
	}

	/* copy mac addr */
	eth_dev->data->mac_addrs = rte_zmalloc("i40evf_mac",
					ETHER_ADDR_LEN, 0);
	if (eth_dev->data->mac_addrs == NULL) {
		PMD_INIT_LOG(ERR, "Failed to allocate %d bytes needed to "
				"store MAC addresses", ETHER_ADDR_LEN);
		return -ENOMEM;
	}
	ether_addr_copy((struct ether_addr *)hw->mac.addr,
		(struct ether_addr *)eth_dev->data->mac_addrs);

	return 0;
}

/*
 * virtual function driver struct
 */
static struct eth_driver rte_i40evf_pmd = {
	{
		.name = "rte_i40evf_pmd",
		.id_table = pci_id_i40evf_map,
		.drv_flags = RTE_PCI_DRV_NEED_MAPPING,
	},
	.eth_dev_init = i40evf_dev_init,
	.dev_private_size = sizeof(struct i40e_vf),
};

/*
 * VF Driver initialization routine.
 * Invoked one at EAL init time.
 * Register itself as the [Virtual Poll Mode] Driver of PCI Fortville devices.
 */
static int
rte_i40evf_pmd_init(const char *name __rte_unused,
		    const char *params __rte_unused)
{
	DEBUGFUNC("rte_i40evf_pmd_init");

	rte_eth_driver_register(&rte_i40evf_pmd);

	return 0;
}

static struct rte_driver rte_i40evf_driver = {
	.type = PMD_PDEV,
	.init = rte_i40evf_pmd_init,
};

PMD_REGISTER_DRIVER(rte_i40evf_driver);

static int
i40evf_dev_configure(struct rte_eth_dev *dev)
{
	return i40evf_init_vlan(dev);
}

static int
i40evf_init_vlan(struct rte_eth_dev *dev)
{
	struct rte_eth_dev_data *data = dev->data;
	int ret;

	/* Apply vlan offload setting */
	i40evf_vlan_offload_set(dev, ETH_VLAN_STRIP_MASK);

	/* Apply pvid setting */
	ret = i40evf_vlan_pvid_set(dev, data->dev_conf.txmode.pvid,
				data->dev_conf.txmode.hw_vlan_insert_pvid);
	return ret;
}

static void
i40evf_vlan_offload_set(struct rte_eth_dev *dev, int mask)
{
	bool enable_vlan_strip = 0;
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	/* Linux pf host doesn't support vlan offload yet */
	if (vf->host_is_dpdk) {
		/* Vlan stripping setting */
		if (mask & ETH_VLAN_STRIP_MASK) {
			/* Enable or disable VLAN stripping */
			if (dev_conf->rxmode.hw_vlan_strip)
				enable_vlan_strip = 1;
			else
				enable_vlan_strip = 0;

			i40evf_config_vlan_offload(dev, enable_vlan_strip);
		}
	}
}

static int
i40evf_vlan_pvid_set(struct rte_eth_dev *dev, uint16_t pvid, int on)
{
	struct rte_eth_conf *dev_conf = &dev->data->dev_conf;
	struct i40e_vsi_vlan_pvid_info info;
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	memset(&info, 0, sizeof(info));
	info.on = on;

	/* Linux pf host don't support vlan offload yet */
	if (vf->host_is_dpdk) {
		if (info.on)
			info.config.pvid = pvid;
		else {
			info.config.reject.tagged =
				dev_conf->txmode.hw_vlan_reject_tagged;
			info.config.reject.untagged =
				dev_conf->txmode.hw_vlan_reject_untagged;
		}
		return i40evf_config_vlan_pvid(dev, &info);
	}

	return 0;
}

static int
i40evf_vlan_filter_set(struct rte_eth_dev *dev, uint16_t vlan_id, int on)
{
	int ret;

	if (on)
		ret = i40evf_add_vlan(dev, vlan_id);
	else
		ret = i40evf_del_vlan(dev,vlan_id);

	return ret;
}

static int
i40evf_rx_init(struct rte_eth_dev *dev)
{
	uint16_t i, j;
	struct i40e_rx_queue **rxq =
		(struct i40e_rx_queue **)dev->data->rx_queues;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	for (i = 0; i < dev->data->nb_rx_queues; i++) {
		if (i40e_alloc_rx_queue_mbufs(rxq[i]) != 0) {
			PMD_DRV_LOG(ERR, "alloc rx queues mbufs failed\n");
			goto err;
		}
		rxq[i]->qrx_tail = hw->hw_addr + I40E_QRX_TAIL1(i);
		I40E_PCI_REG_WRITE(rxq[i]->qrx_tail, rxq[i]->nb_rx_desc - 1);
	}

	/* Flush the operation to write registers */
	I40EVF_WRITE_FLUSH(hw);

	return 0;

err:
	/* Release all mbufs */
	for (j = 0; j < i; j++)
		i40e_rx_queue_release_mbufs(rxq[j]);

	return -1;
}

static void
i40evf_tx_init(struct rte_eth_dev *dev)
{
	uint16_t i;
	struct i40e_tx_queue **txq =
		(struct i40e_tx_queue **)dev->data->tx_queues;
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	for (i = 0; i < dev->data->nb_tx_queues; i++)
		txq[i]->qtx_tail = hw->hw_addr + I40E_QTX_TAIL1(i);
}

static inline void
i40evf_enable_queues_intr(struct i40e_hw *hw)
{
	I40E_WRITE_REG(hw, I40E_VFINT_DYN_CTLN1(I40EVF_VSI_DEFAULT_MSIX_INTR - 1),
			I40E_VFINT_DYN_CTLN1_INTENA_MASK |
			I40E_VFINT_DYN_CTLN_CLEARPBA_MASK);
}

static inline void
i40evf_disable_queues_intr(struct i40e_hw *hw)
{
	I40E_WRITE_REG(hw, I40E_VFINT_DYN_CTLN1(I40EVF_VSI_DEFAULT_MSIX_INTR - 1),
			0);
}

static int
i40evf_dev_start(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct ether_addr mac_addr;

	PMD_DRV_LOG(DEBUG, "i40evf_dev_start");

	vf->max_pkt_len = dev->data->dev_conf.rxmode.max_rx_pkt_len;
	if (dev->data->dev_conf.rxmode.jumbo_frame == 1) {
		if (vf->max_pkt_len <= ETHER_MAX_LEN ||
			vf->max_pkt_len > I40E_FRAME_SIZE_MAX) {
			PMD_DRV_LOG(ERR, "maximum packet length must "
				"be larger than %u and smaller than %u,"
					"as jumbo frame is enabled\n",
						(uint32_t)ETHER_MAX_LEN,
					(uint32_t)I40E_FRAME_SIZE_MAX);
			return I40E_ERR_CONFIG;
		}
	} else {
		if (vf->max_pkt_len < ETHER_MIN_LEN ||
			vf->max_pkt_len > ETHER_MAX_LEN) {
			PMD_DRV_LOG(ERR, "maximum packet length must be "
					"larger than %u and smaller than %u, "
					"as jumbo frame is disabled\n",
						(uint32_t)ETHER_MIN_LEN,
						(uint32_t)ETHER_MAX_LEN);
			return I40E_ERR_CONFIG;
		}
	}

	vf->num_queue_pairs = RTE_MAX(dev->data->nb_rx_queues,
					dev->data->nb_tx_queues);

	if (i40evf_rx_init(dev) != 0){
		PMD_DRV_LOG(ERR, "failed to do RX init\n");
		return -1;
	}

	i40evf_tx_init(dev);

	if (i40evf_configure_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "configure queues failed\n");
		goto err_queue;
	}
	if (i40evf_config_irq_map(dev)) {
		PMD_DRV_LOG(ERR, "config_irq_map failed\n");
		goto err_queue;
	}

	/* Set mac addr */
	(void)rte_memcpy(mac_addr.addr_bytes, hw->mac.addr,
				sizeof(mac_addr.addr_bytes));
	if (i40evf_add_mac_addr(dev, &mac_addr)) {
		PMD_DRV_LOG(ERR, "Failed to add mac addr\n");
		goto err_queue;
	}

	if (i40evf_enable_queues(dev) != 0) {
		PMD_DRV_LOG(ERR, "enable queues failed\n");
		goto err_mac;
	}
	i40evf_enable_queues_intr(hw);
	return 0;

err_mac:
	i40evf_del_mac_addr(dev, &mac_addr);
err_queue:
	i40e_dev_clear_queues(dev);
	return -1;
}

static void
i40evf_dev_stop(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	PMD_INIT_FUNC_TRACE();

	i40evf_disable_queues_intr(hw);
	i40evf_disable_queues(dev);
	i40e_dev_clear_queues(dev);
}

static int
i40evf_dev_link_update(struct rte_eth_dev *dev,
		       __rte_unused int wait_to_complete)
{
	struct rte_eth_link new_link;
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	/*
	 * DPDK pf host provide interfacet to acquire link status
	 * while Linux driver does not
	 */
	if (vf->host_is_dpdk)
		i40evf_get_link_status(dev, &new_link);
	else {
		/* Always assume it's up, for Linux driver PF host */
		new_link.link_duplex = ETH_LINK_AUTONEG_DUPLEX;
		new_link.link_speed  = ETH_LINK_SPEED_10000;
		new_link.link_status = 1;
	}
	i40evf_dev_atomic_write_link_status(dev, &new_link);

	return 0;
}

static void
i40evf_dev_promiscuous_enable(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int ret;

	/* If enabled, just return */
	if (vf->promisc_unicast_enabled)
		return;

	ret = i40evf_config_promisc(dev, 1, vf->promisc_multicast_enabled);
	if (ret == 0)
		vf->promisc_unicast_enabled = TRUE;
}

static void
i40evf_dev_promiscuous_disable(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int ret;

	/* If disabled, just return */
	if (!vf->promisc_unicast_enabled)
		return;

	ret = i40evf_config_promisc(dev, 0, vf->promisc_multicast_enabled);
	if (ret == 0)
		vf->promisc_unicast_enabled = FALSE;
}

static void
i40evf_dev_allmulticast_enable(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int ret;

	/* If enabled, just return */
	if (vf->promisc_multicast_enabled)
		return;

	ret = i40evf_config_promisc(dev, vf->promisc_unicast_enabled, 1);
	if (ret == 0)
		vf->promisc_multicast_enabled = TRUE;
}

static void
i40evf_dev_allmulticast_disable(struct rte_eth_dev *dev)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	int ret;

	/* If enabled, just return */
	if (!vf->promisc_multicast_enabled)
		return;

	ret = i40evf_config_promisc(dev, vf->promisc_unicast_enabled, 0);
	if (ret == 0)
		vf->promisc_multicast_enabled = FALSE;
}

static void
i40evf_dev_info_get(struct rte_eth_dev *dev, struct rte_eth_dev_info *dev_info)
{
	struct i40e_vf *vf = I40EVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);

	memset(dev_info, 0, sizeof(*dev_info));
	dev_info->max_rx_queues = vf->vsi_res->num_queue_pairs;
	dev_info->max_tx_queues = vf->vsi_res->num_queue_pairs;
	dev_info->min_rx_bufsize = I40E_BUF_SIZE_MIN;
	dev_info->max_rx_pktlen = I40E_FRAME_SIZE_MAX;
}

static void
i40evf_dev_stats_get(struct rte_eth_dev *dev, struct rte_eth_stats *stats)
{
	memset(stats, 0, sizeof(*stats));
	if (i40evf_get_statics(dev, stats))
		PMD_DRV_LOG(ERR, "Get statics failed\n");
}

static void
i40evf_dev_close(struct rte_eth_dev *dev)
{
	struct i40e_hw *hw = I40E_DEV_PRIVATE_TO_HW(dev->data->dev_private);

	i40evf_dev_stop(dev);
	i40evf_reset_vf(hw);
	i40e_shutdown_adminq(hw);
}
