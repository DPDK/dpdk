/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2017 Intel Corporation
 */

#include <fcntl.h>
#include <stdio.h>
#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <stdarg.h>
#include <inttypes.h>
#include <rte_byteorder.h>
#include <rte_common.h>
#include <rte_os_shim.h>

#include <rte_debug.h>
#include <rte_alarm.h>
#include <rte_atomic.h>
#include <rte_eal.h>
#include <rte_ether.h>
#include <ethdev_driver.h>
#include <ethdev_pci.h>
#include <dev_driver.h>

#include "iavf.h"
#include "iavf_rxtx.h"

#define MAX_TRY_TIMES 2000
#define ASQ_DELAY_MS  1

#define MAX_EVENT_PENDING 16

struct iavf_event_element {
	TAILQ_ENTRY(iavf_event_element) next;
	struct rte_eth_dev *dev;
	enum rte_eth_event_type event;
	void *param;
	size_t param_alloc_size;
	uint8_t param_alloc_data[0];
};

struct iavf_event_handler {
	uint32_t ndev;
	rte_thread_t tid;
	int fd[2];
	pthread_mutex_t lock;
	TAILQ_HEAD(event_list, iavf_event_element) pending;
};

static struct iavf_event_handler event_handler = {
	.fd = {-1, -1},
};

#ifndef TAILQ_FOREACH_SAFE
#define TAILQ_FOREACH_SAFE(var, head, field, tvar) \
	for ((var) = TAILQ_FIRST((head)); \
		(var) && ((tvar) = TAILQ_NEXT((var), field), 1); \
	(var) = (tvar))
#endif

static uint32_t
iavf_dev_event_handle(void *param __rte_unused)
{
	struct iavf_event_handler *handler = &event_handler;
	TAILQ_HEAD(event_list, iavf_event_element) pending;

	while (true) {
		char unused[MAX_EVENT_PENDING];
		ssize_t nr = read(handler->fd[0], &unused, sizeof(unused));
		if (nr <= 0)
			break;

		TAILQ_INIT(&pending);
		pthread_mutex_lock(&handler->lock);
		TAILQ_CONCAT(&pending, &handler->pending, next);
		pthread_mutex_unlock(&handler->lock);

		struct iavf_event_element *pos, *save_next;
		TAILQ_FOREACH_SAFE(pos, &pending, next, save_next) {
			TAILQ_REMOVE(&pending, pos, next);

			struct iavf_adapter *adapter = pos->dev->data->dev_private;
			if (pos->event == RTE_ETH_EVENT_INTR_RESET &&
			    adapter->devargs.auto_reset) {
				iavf_handle_hw_reset(pos->dev);
				rte_free(pos);
				continue;
			}

			rte_eth_dev_callback_process(pos->dev, pos->event, pos->param);
			rte_free(pos);
		}
	}

	return 0;
}

void
iavf_dev_event_post(struct rte_eth_dev *dev,
		enum rte_eth_event_type event,
		void *param, size_t param_alloc_size)
{
	struct iavf_event_handler *handler = &event_handler;
	char notify_byte;
	struct iavf_event_element *elem = rte_malloc(NULL, sizeof(*elem) + param_alloc_size, 0);
	if (!elem)
		return;

	elem->dev = dev;
	elem->event = event;
	elem->param = param;
	elem->param_alloc_size = param_alloc_size;
	if (param && param_alloc_size) {
		rte_memcpy(elem->param_alloc_data, param, param_alloc_size);
		elem->param = elem->param_alloc_data;
	}

	pthread_mutex_lock(&handler->lock);
	TAILQ_INSERT_TAIL(&handler->pending, elem, next);
	pthread_mutex_unlock(&handler->lock);

	ssize_t nw = write(handler->fd[1], &notify_byte, 1);
	RTE_SET_USED(nw);
}

int
iavf_dev_event_handler_init(void)
{
	struct iavf_event_handler *handler = &event_handler;

	if (__atomic_fetch_add(&handler->ndev, 1, __ATOMIC_RELAXED) + 1 != 1)
		return 0;
#if defined(RTE_EXEC_ENV_IS_WINDOWS) && RTE_EXEC_ENV_IS_WINDOWS != 0
	int err = _pipe(handler->fd, MAX_EVENT_PENDING, O_BINARY);
#else
	int err = pipe(handler->fd);
#endif
	if (err != 0) {
		__atomic_fetch_sub(&handler->ndev, 1, __ATOMIC_RELAXED);
		return -1;
	}

	TAILQ_INIT(&handler->pending);
	pthread_mutex_init(&handler->lock, NULL);

	if (rte_thread_create_internal_control(&handler->tid, "iavf-event",
				iavf_dev_event_handle, NULL)) {
		__atomic_fetch_sub(&handler->ndev, 1, __ATOMIC_RELAXED);
		return -1;
	}

	return 0;
}

void
iavf_dev_event_handler_fini(void)
{
	struct iavf_event_handler *handler = &event_handler;

	if (__atomic_fetch_sub(&handler->ndev, 1, __ATOMIC_RELAXED) - 1 != 0)
		return;

	int unused = pthread_cancel((pthread_t)handler->tid.opaque_id);
	RTE_SET_USED(unused);
	close(handler->fd[0]);
	close(handler->fd[1]);
	handler->fd[0] = -1;
	handler->fd[1] = -1;

	rte_thread_join(handler->tid, NULL);
	pthread_mutex_destroy(&handler->lock);

	struct iavf_event_element *pos, *save_next;
	TAILQ_FOREACH_SAFE(pos, &handler->pending, next, save_next) {
		TAILQ_REMOVE(&handler->pending, pos, next);
		rte_free(pos);
	}
}

static uint32_t
iavf_convert_link_speed(enum virtchnl_link_speed virt_link_speed)
{
	uint32_t speed;

	switch (virt_link_speed) {
	case VIRTCHNL_LINK_SPEED_100MB:
		speed = 100;
		break;
	case VIRTCHNL_LINK_SPEED_1GB:
		speed = 1000;
		break;
	case VIRTCHNL_LINK_SPEED_10GB:
		speed = 10000;
		break;
	case VIRTCHNL_LINK_SPEED_40GB:
		speed = 40000;
		break;
	case VIRTCHNL_LINK_SPEED_20GB:
		speed = 20000;
		break;
	case VIRTCHNL_LINK_SPEED_25GB:
		speed = 25000;
		break;
	case VIRTCHNL_LINK_SPEED_2_5GB:
		speed = 2500;
		break;
	case VIRTCHNL_LINK_SPEED_5GB:
		speed = 5000;
		break;
	default:
		speed = 0;
		break;
	}

	return speed;
}

/* Read data in admin queue to get msg from pf driver */
static enum iavf_aq_result
iavf_read_msg_from_pf(struct iavf_adapter *adapter, uint16_t buf_len,
		     uint8_t *buf)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_arq_event_info event;
	enum iavf_aq_result result = IAVF_MSG_NON;
	enum virtchnl_ops opcode;
	int ret;

	event.buf_len = buf_len;
	event.msg_buf = buf;
	ret = iavf_clean_arq_element(hw, &event, NULL);
	/* Can't read any msg from adminQ */
	if (ret) {
		PMD_DRV_LOG(DEBUG, "Can't read msg from AQ");
		if (ret != IAVF_ERR_ADMIN_QUEUE_NO_WORK)
			result = IAVF_MSG_ERR;
		return result;
	}

	opcode = (enum virtchnl_ops)rte_le_to_cpu_32(event.desc.cookie_high);
	vf->cmd_retval = (enum virtchnl_status_code)rte_le_to_cpu_32(
			event.desc.cookie_low);

	PMD_DRV_LOG(DEBUG, "AQ from pf carries opcode %u, retval %d",
		    opcode, vf->cmd_retval);

	if (opcode == VIRTCHNL_OP_EVENT) {
		struct virtchnl_pf_event *vpe =
			(struct virtchnl_pf_event *)event.msg_buf;

		result = IAVF_MSG_SYS;
		switch (vpe->event) {
		case VIRTCHNL_EVENT_LINK_CHANGE:
			vf->link_up =
				vpe->event_data.link_event.link_status;
			if (vf->vf_res->vf_cap_flags &
				VIRTCHNL_VF_CAP_ADV_LINK_SPEED) {
				vf->link_speed =
				    vpe->event_data.link_event_adv.link_speed;
			} else {
				enum virtchnl_link_speed speed;
				speed = vpe->event_data.link_event.link_speed;
				vf->link_speed = iavf_convert_link_speed(speed);
			}
			iavf_dev_link_update(vf->eth_dev, 0);
			iavf_dev_event_post(vf->eth_dev, RTE_ETH_EVENT_INTR_LSC, NULL, 0);
			if (vf->link_up && !vf->vf_reset) {
				iavf_dev_watchdog_disable(adapter);
			} else {
				if (!vf->link_up)
					iavf_dev_watchdog_enable(adapter);
			}
			if (adapter->devargs.no_poll_on_link_down) {
				iavf_set_no_poll(adapter, true);
				if (adapter->no_poll)
					PMD_DRV_LOG(DEBUG, "VF no poll turned on");
				else
					PMD_DRV_LOG(DEBUG, "VF no poll turned off");
			}
			PMD_DRV_LOG(INFO, "Link status update:%s",
					vf->link_up ? "up" : "down");
			break;
		case VIRTCHNL_EVENT_RESET_IMPENDING:
			vf->vf_reset = true;
			iavf_set_no_poll(adapter, false);
			PMD_DRV_LOG(INFO, "VF is resetting");
			break;
		case VIRTCHNL_EVENT_PF_DRIVER_CLOSE:
			vf->dev_closed = true;
			PMD_DRV_LOG(INFO, "PF driver closed");
			break;
		default:
			PMD_DRV_LOG(ERR, "%s: Unknown event %d from pf",
					__func__, vpe->event);
		}
	}  else {
		/* async reply msg on command issued by vf previously */
		result = IAVF_MSG_CMD;
		if (opcode != vf->pend_cmd) {
			PMD_DRV_LOG(WARNING, "command mismatch, expect %u, get %u",
					vf->pend_cmd, opcode);
			result = IAVF_MSG_ERR;
		}
	}

	return result;
}

static int
iavf_execute_vf_cmd(struct iavf_adapter *adapter, struct iavf_cmd_info *args,
	int async)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	enum iavf_aq_result result;
	enum iavf_status ret;
	int err = 0;
	int i = 0;

	if (vf->vf_reset)
		return -EIO;


	if (async) {
		if (_atomic_set_async_response_cmd(vf, args->ops))
			return -1;
	} else {
		if (_atomic_set_cmd(vf, args->ops))
			return -1;
	}

	ret = iavf_aq_send_msg_to_pf(hw, args->ops, IAVF_SUCCESS,
				    args->in_args, args->in_args_size, NULL);
	if (ret) {
		PMD_DRV_LOG(ERR, "fail to send cmd %d", args->ops);
		_clear_cmd(vf);
		return err;
	}

	switch (args->ops) {
	case VIRTCHNL_OP_RESET_VF:
	case VIRTCHNL_OP_REQUEST_QUEUES:
		/*no need to wait for response */
		_clear_cmd(vf);
		break;
	case VIRTCHNL_OP_VERSION:
	case VIRTCHNL_OP_GET_VF_RESOURCES:
	case VIRTCHNL_OP_GET_SUPPORTED_RXDIDS:
	case VIRTCHNL_OP_GET_OFFLOAD_VLAN_V2_CAPS:
		/* for init virtchnl ops, need to poll the response */
		do {
			result = iavf_read_msg_from_pf(adapter, args->out_size,
						   args->out_buffer);
			if (result == IAVF_MSG_CMD)
				break;
			iavf_msec_delay(ASQ_DELAY_MS);
		} while (i++ < MAX_TRY_TIMES);
		if (i >= MAX_TRY_TIMES ||
		    vf->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
			err = -1;
			PMD_DRV_LOG(ERR, "No response or return failure (%d)"
				    " for cmd %d", vf->cmd_retval, args->ops);
		}
		_clear_cmd(vf);
		break;
	default:
		if (rte_thread_is_intr()) {
			/* For virtchnl ops were executed in eal_intr_thread,
			 * need to poll the response.
			 */
			do {
				result = iavf_read_msg_from_pf(adapter, args->out_size,
							args->out_buffer);
				if (result == IAVF_MSG_CMD)
					break;
				iavf_msec_delay(ASQ_DELAY_MS);
			} while (i++ < MAX_TRY_TIMES);
			if (i >= MAX_TRY_TIMES ||
				vf->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
				err = -1;
				PMD_DRV_LOG(ERR, "No response or return failure (%d)"
						" for cmd %d", vf->cmd_retval, args->ops);
			}
			_clear_cmd(vf);
		} else {
			/* For other virtchnl ops in running time,
			 * wait for the cmd done flag.
			 */
			do {
				if (vf->pend_cmd == VIRTCHNL_OP_UNKNOWN)
					break;
				iavf_msec_delay(ASQ_DELAY_MS);
				/* If don't read msg or read sys event, continue */
			} while (i++ < MAX_TRY_TIMES);

			if (i >= MAX_TRY_TIMES) {
				PMD_DRV_LOG(ERR, "No response for cmd %d", args->ops);
				_clear_cmd(vf);
				err = -EIO;
			} else if (vf->cmd_retval ==
				VIRTCHNL_STATUS_ERR_NOT_SUPPORTED) {
				PMD_DRV_LOG(ERR, "Cmd %d not supported", args->ops);
				err = -ENOTSUP;
			} else if (vf->cmd_retval != VIRTCHNL_STATUS_SUCCESS) {
				PMD_DRV_LOG(ERR, "Return failure %d for cmd %d",
						vf->cmd_retval, args->ops);
				err = -EINVAL;
			}
		}
		break;
	}

	return err;
}

static int
iavf_execute_vf_cmd_safe(struct iavf_adapter *adapter,
	struct iavf_cmd_info *args, int async)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	int ret;
	int is_intr_thread = rte_thread_is_intr();

	if (is_intr_thread) {
		if (!rte_spinlock_trylock(&vf->aq_lock))
			return -EIO;
	} else {
		rte_spinlock_lock(&vf->aq_lock);
	}
	ret = iavf_execute_vf_cmd(adapter, args, async);
	rte_spinlock_unlock(&vf->aq_lock);

	return ret;
}

static void
iavf_handle_pf_event_msg(struct rte_eth_dev *dev, uint8_t *msg,
			uint16_t msglen)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = &adapter->vf;
	struct virtchnl_pf_event *pf_msg =
			(struct virtchnl_pf_event *)msg;

	if (adapter->closed) {
		PMD_DRV_LOG(DEBUG, "Port closed");
		return;
	}

	if (msglen < sizeof(struct virtchnl_pf_event)) {
		PMD_DRV_LOG(DEBUG, "Error event");
		return;
	}
	switch (pf_msg->event) {
	case VIRTCHNL_EVENT_RESET_IMPENDING:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_RESET_IMPENDING event");
		vf->link_up = false;
		if (!vf->vf_reset) {
			vf->vf_reset = true;
			iavf_set_no_poll(adapter, false);
			iavf_dev_event_post(dev, RTE_ETH_EVENT_INTR_RESET,
				NULL, 0);
		}
		break;
	case VIRTCHNL_EVENT_LINK_CHANGE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_LINK_CHANGE event");
		vf->link_up = pf_msg->event_data.link_event.link_status;
		if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_ADV_LINK_SPEED) {
			vf->link_speed =
				pf_msg->event_data.link_event_adv.link_speed;
		} else {
			enum virtchnl_link_speed speed;
			speed = pf_msg->event_data.link_event.link_speed;
			vf->link_speed = iavf_convert_link_speed(speed);
		}
		iavf_dev_link_update(dev, 0);
		if (vf->link_up && !vf->vf_reset) {
			iavf_dev_watchdog_disable(adapter);
		} else {
			if (!vf->link_up)
				iavf_dev_watchdog_enable(adapter);
		}
		if (adapter->devargs.no_poll_on_link_down) {
			iavf_set_no_poll(adapter, true);
			if (adapter->no_poll)
				PMD_DRV_LOG(DEBUG, "VF no poll turned on");
			else
				PMD_DRV_LOG(DEBUG, "VF no poll turned off");
		}
		iavf_dev_event_post(dev, RTE_ETH_EVENT_INTR_LSC, NULL, 0);
		break;
	case VIRTCHNL_EVENT_PF_DRIVER_CLOSE:
		PMD_DRV_LOG(DEBUG, "VIRTCHNL_EVENT_PF_DRIVER_CLOSE event");
		break;
	default:
		PMD_DRV_LOG(ERR, " unknown event received %u", pf_msg->event);
		break;
	}
}

void
iavf_handle_virtchnl_msg(struct rte_eth_dev *dev)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_arq_event_info info;
	uint16_t pending, aq_opc;
	enum virtchnl_ops msg_opc;
	enum iavf_status msg_ret;
	int ret;

	info.buf_len = IAVF_AQ_BUF_SZ;
	if (!vf->aq_resp) {
		PMD_DRV_LOG(ERR, "Buffer for adminq resp should not be NULL");
		return;
	}
	info.msg_buf = vf->aq_resp;

	pending = 1;
	while (pending) {
		ret = iavf_clean_arq_element(hw, &info, &pending);

		if (ret != IAVF_SUCCESS) {
			PMD_DRV_LOG(INFO, "Failed to read msg from AdminQ,"
				    "ret: %d", ret);
			break;
		}
		aq_opc = rte_le_to_cpu_16(info.desc.opcode);
		/* For the message sent from pf to vf, opcode is stored in
		 * cookie_high of struct iavf_aq_desc, while return error code
		 * are stored in cookie_low, Which is done by PF driver.
		 */
		msg_opc = (enum virtchnl_ops)rte_le_to_cpu_32(
						  info.desc.cookie_high);
		msg_ret = (enum iavf_status)rte_le_to_cpu_32(
						  info.desc.cookie_low);
		switch (aq_opc) {
		case iavf_aqc_opc_send_msg_to_vf:
			if (msg_opc == VIRTCHNL_OP_EVENT) {
				iavf_handle_pf_event_msg(dev, info.msg_buf,
						info.msg_len);
			} else {
				/* check for unsolicited messages i.e. events */
				if (info.msg_len > 0) {
					switch (msg_opc) {
					case VIRTCHNL_OP_INLINE_IPSEC_CRYPTO: {
						struct inline_ipsec_msg *imsg =
							(struct inline_ipsec_msg *)info.msg_buf;
						if (imsg->ipsec_opcode
								== INLINE_IPSEC_OP_EVENT) {
							struct rte_eth_event_ipsec_desc desc;
							struct virtchnl_ipsec_event *ev =
									imsg->ipsec_data.event;
							desc.subtype =
									RTE_ETH_EVENT_IPSEC_UNKNOWN;
							desc.metadata =
									ev->ipsec_event_data;
							iavf_dev_event_post(dev,
								RTE_ETH_EVENT_IPSEC,
								&desc, sizeof(desc));
							continue;
					}
				}
						break;
					default:
						break;
					}

				}

				/* read message and it's expected one */
				if (msg_opc == vf->pend_cmd) {
					uint32_t cmd_count =
					__atomic_fetch_sub(&vf->pend_cmd_count,
							1, __ATOMIC_RELAXED) - 1;
					if (cmd_count == 0)
						_notify_cmd(vf, msg_ret);
				} else {
					PMD_DRV_LOG(ERR,
					"command mismatch, expect %u, get %u",
						vf->pend_cmd, msg_opc);
				}
				PMD_DRV_LOG(DEBUG,
				"adminq response is received, opcode = %d",
						msg_opc);
			}
			break;
		default:
			PMD_DRV_LOG(DEBUG, "Request %u is not supported yet",
				    aq_opc);
			break;
		}
	}
}

int
iavf_enable_vlan_strip(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int ret;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_ENABLE_VLAN_STRIPPING;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	ret = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (ret)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " OP_ENABLE_VLAN_STRIPPING");

	return ret;
}

int
iavf_disable_vlan_strip(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int ret;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_DISABLE_VLAN_STRIPPING;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	ret = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (ret)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " OP_DISABLE_VLAN_STRIPPING");

	return ret;
}

#define VIRTCHNL_VERSION_MAJOR_START 1
#define VIRTCHNL_VERSION_MINOR_START 1

/* Check API version with sync wait until version read from admin queue */
int
iavf_check_api_version(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_version_info version, *pver;
	struct iavf_cmd_info args;
	int err;

	version.major = VIRTCHNL_VERSION_MAJOR;
	version.minor = VIRTCHNL_VERSION_MINOR;

	args.ops = VIRTCHNL_OP_VERSION;
	args.in_args = (uint8_t *)&version;
	args.in_args_size = sizeof(version);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_INIT_LOG(ERR, "Fail to execute command of OP_VERSION");
		return err;
	}

	pver = (struct virtchnl_version_info *)args.out_buffer;
	vf->virtchnl_version = *pver;

	if (vf->virtchnl_version.major < VIRTCHNL_VERSION_MAJOR_START ||
	    (vf->virtchnl_version.major == VIRTCHNL_VERSION_MAJOR_START &&
	     vf->virtchnl_version.minor < VIRTCHNL_VERSION_MINOR_START)) {
		PMD_INIT_LOG(ERR, "VIRTCHNL API version should not be lower"
			     " than (%u.%u) to support Adaptive VF",
			     VIRTCHNL_VERSION_MAJOR_START,
			     VIRTCHNL_VERSION_MAJOR_START);
		return -1;
	} else if (vf->virtchnl_version.major > VIRTCHNL_VERSION_MAJOR ||
		   (vf->virtchnl_version.major == VIRTCHNL_VERSION_MAJOR &&
		    vf->virtchnl_version.minor > VIRTCHNL_VERSION_MINOR)) {
		PMD_INIT_LOG(ERR, "PF/VF API version mismatch:(%u.%u)-(%u.%u)",
			     vf->virtchnl_version.major,
			     vf->virtchnl_version.minor,
			     VIRTCHNL_VERSION_MAJOR,
			     VIRTCHNL_VERSION_MINOR);
		return -1;
	}

	PMD_DRV_LOG(DEBUG, "Peer is supported PF host");
	return 0;
}

int
iavf_get_vf_resource(struct iavf_adapter *adapter)
{
	struct iavf_hw *hw = IAVF_DEV_PRIVATE_TO_HW(adapter);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	uint32_t caps, len;
	int err, i;

	args.ops = VIRTCHNL_OP_GET_VF_RESOURCES;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	caps = IAVF_BASIC_OFFLOAD_CAPS | VIRTCHNL_VF_CAP_ADV_LINK_SPEED |
		VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC |
		VIRTCHNL_VF_OFFLOAD_FDIR_PF |
		VIRTCHNL_VF_OFFLOAD_ADV_RSS_PF |
		VIRTCHNL_VF_OFFLOAD_FSUB_PF |
		VIRTCHNL_VF_OFFLOAD_REQ_QUEUES |
		VIRTCHNL_VF_OFFLOAD_CRC |
		VIRTCHNL_VF_OFFLOAD_VLAN_V2 |
		VIRTCHNL_VF_LARGE_NUM_QPAIRS |
		VIRTCHNL_VF_OFFLOAD_QOS |
		VIRTCHNL_VF_OFFLOAD_INLINE_IPSEC_CRYPTO |
		VIRTCHNL_VF_CAP_PTP;

	args.in_args = (uint8_t *)&caps;
	args.in_args_size = sizeof(caps);

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);

	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_GET_VF_RESOURCE");
		return -1;
	}

	len =  sizeof(struct virtchnl_vf_resource) +
		      IAVF_MAX_VF_VSI * sizeof(struct virtchnl_vsi_resource);

	rte_memcpy(vf->vf_res, args.out_buffer,
		   RTE_MIN(args.out_size, len));
	/* parse  VF config message back from PF*/
	iavf_vf_parse_hw_config(hw, vf->vf_res);
	for (i = 0; i < vf->vf_res->num_vsis; i++) {
		if (vf->vf_res->vsi_res[i].vsi_type == VIRTCHNL_VSI_SRIOV)
			vf->vsi_res = &vf->vf_res->vsi_res[i];
	}

	if (!vf->vsi_res) {
		PMD_INIT_LOG(ERR, "no LAN VSI found");
		return -1;
	}

	vf->vsi.vsi_id = vf->vsi_res->vsi_id;
	vf->vsi.nb_qps = vf->vsi_res->num_queue_pairs;
	vf->vsi.adapter = adapter;

	return 0;
}

int
iavf_get_supported_rxdid(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int ret;

	args.ops = VIRTCHNL_OP_GET_SUPPORTED_RXDIDS;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	ret = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (ret) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_GET_SUPPORTED_RXDIDS");
		return ret;
	}

	vf->supported_rxdid =
		((struct virtchnl_supported_rxdids *)args.out_buffer)->supported_rxdids;

	return 0;
}

int
iavf_config_vlan_strip_v2(struct iavf_adapter *adapter, bool enable)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_vlan_supported_caps *stripping_caps;
	struct virtchnl_vlan_setting vlan_strip;
	struct iavf_cmd_info args;
	uint32_t *ethertype;
	int ret;

	stripping_caps = &vf->vlan_v2_caps.offloads.stripping_support;

	if ((stripping_caps->outer & VIRTCHNL_VLAN_ETHERTYPE_8100) &&
	    (stripping_caps->outer & VIRTCHNL_VLAN_TOGGLE))
		ethertype = &vlan_strip.outer_ethertype_setting;
	else if ((stripping_caps->inner & VIRTCHNL_VLAN_ETHERTYPE_8100) &&
		 (stripping_caps->inner & VIRTCHNL_VLAN_TOGGLE))
		ethertype = &vlan_strip.inner_ethertype_setting;
	else
		return -ENOTSUP;

	memset(&vlan_strip, 0, sizeof(vlan_strip));
	vlan_strip.vport_id = vf->vsi_res->vsi_id;
	*ethertype = VIRTCHNL_VLAN_ETHERTYPE_8100;

	args.ops = enable ? VIRTCHNL_OP_ENABLE_VLAN_STRIPPING_V2 :
			    VIRTCHNL_OP_DISABLE_VLAN_STRIPPING_V2;
	args.in_args = (uint8_t *)&vlan_strip;
	args.in_args_size = sizeof(vlan_strip);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	ret = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (ret)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    enable ? "VIRTCHNL_OP_ENABLE_VLAN_STRIPPING_V2" :
				     "VIRTCHNL_OP_DISABLE_VLAN_STRIPPING_V2");

	return ret;
}

int
iavf_config_vlan_insert_v2(struct iavf_adapter *adapter, bool enable)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_vlan_supported_caps *insertion_caps;
	struct virtchnl_vlan_setting vlan_insert;
	struct iavf_cmd_info args;
	uint32_t *ethertype;
	int ret;

	insertion_caps = &vf->vlan_v2_caps.offloads.insertion_support;

	if ((insertion_caps->outer & VIRTCHNL_VLAN_ETHERTYPE_8100) &&
	    (insertion_caps->outer & VIRTCHNL_VLAN_TOGGLE))
		ethertype = &vlan_insert.outer_ethertype_setting;
	else if ((insertion_caps->inner & VIRTCHNL_VLAN_ETHERTYPE_8100) &&
		 (insertion_caps->inner & VIRTCHNL_VLAN_TOGGLE))
		ethertype = &vlan_insert.inner_ethertype_setting;
	else
		return -ENOTSUP;

	memset(&vlan_insert, 0, sizeof(vlan_insert));
	vlan_insert.vport_id = vf->vsi_res->vsi_id;
	*ethertype = VIRTCHNL_VLAN_ETHERTYPE_8100;

	args.ops = enable ? VIRTCHNL_OP_ENABLE_VLAN_INSERTION_V2 :
			    VIRTCHNL_OP_DISABLE_VLAN_INSERTION_V2;
	args.in_args = (uint8_t *)&vlan_insert;
	args.in_args_size = sizeof(vlan_insert);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	ret = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (ret)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    enable ? "VIRTCHNL_OP_ENABLE_VLAN_INSERTION_V2" :
				     "VIRTCHNL_OP_DISABLE_VLAN_INSERTION_V2");

	return ret;
}

int
iavf_add_del_vlan_v2(struct iavf_adapter *adapter, uint16_t vlanid, bool add)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_vlan_supported_caps *supported_caps;
	struct virtchnl_vlan_filter_list_v2 vlan_filter;
	struct virtchnl_vlan *vlan_setting;
	struct iavf_cmd_info args;
	uint32_t filtering_caps;
	int err;

	supported_caps = &vf->vlan_v2_caps.filtering.filtering_support;
	if (supported_caps->outer) {
		filtering_caps = supported_caps->outer;
		vlan_setting = &vlan_filter.filters[0].outer;
	} else {
		filtering_caps = supported_caps->inner;
		vlan_setting = &vlan_filter.filters[0].inner;
	}

	if (!(filtering_caps & VIRTCHNL_VLAN_ETHERTYPE_8100))
		return -ENOTSUP;

	memset(&vlan_filter, 0, sizeof(vlan_filter));
	vlan_filter.vport_id = vf->vsi_res->vsi_id;
	vlan_filter.num_elements = 1;
	vlan_setting->tpid = RTE_ETHER_TYPE_VLAN;
	vlan_setting->tci = vlanid;

	args.ops = add ? VIRTCHNL_OP_ADD_VLAN_V2 : VIRTCHNL_OP_DEL_VLAN_V2;
	args.in_args = (uint8_t *)&vlan_filter;
	args.in_args_size = sizeof(vlan_filter);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    add ? "OP_ADD_VLAN_V2" :  "OP_DEL_VLAN_V2");

	return err;
}

int
iavf_get_vlan_offload_caps_v2(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int ret;

	args.ops = VIRTCHNL_OP_GET_OFFLOAD_VLAN_V2_CAPS;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	ret = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (ret) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of VIRTCHNL_OP_GET_OFFLOAD_VLAN_V2_CAPS");
		return ret;
	}

	rte_memcpy(&vf->vlan_v2_caps, vf->aq_resp, sizeof(vf->vlan_v2_caps));

	return 0;
}

int
iavf_enable_queues(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select queue_select;
	struct iavf_cmd_info args;
	int err;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = vf->vsi_res->vsi_id;

	queue_select.rx_queues = BIT(adapter->dev_data->nb_rx_queues) - 1;
	queue_select.tx_queues = BIT(adapter->dev_data->nb_tx_queues) - 1;

	args.ops = VIRTCHNL_OP_ENABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_ENABLE_QUEUES");
		return err;
	}
	return 0;
}

int
iavf_disable_queues(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select queue_select;
	struct iavf_cmd_info args;
	int err;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = vf->vsi_res->vsi_id;

	queue_select.rx_queues = BIT(adapter->dev_data->nb_rx_queues) - 1;
	queue_select.tx_queues = BIT(adapter->dev_data->nb_tx_queues) - 1;

	args.ops = VIRTCHNL_OP_DISABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_DISABLE_QUEUES");
		return err;
	}
	return 0;
}

int
iavf_switch_queue(struct iavf_adapter *adapter, uint16_t qid,
		 bool rx, bool on)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select queue_select;
	struct iavf_cmd_info args;
	int err;

	if (adapter->closed)
		return -EIO;

	memset(&queue_select, 0, sizeof(queue_select));
	queue_select.vsi_id = vf->vsi_res->vsi_id;
	if (rx)
		queue_select.rx_queues |= 1 << qid;
	else
		queue_select.tx_queues |= 1 << qid;

	if (on)
		args.ops = VIRTCHNL_OP_ENABLE_QUEUES;
	else
		args.ops = VIRTCHNL_OP_DISABLE_QUEUES;
	args.in_args = (u8 *)&queue_select;
	args.in_args_size = sizeof(queue_select);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of %s",
			    on ? "OP_ENABLE_QUEUES" : "OP_DISABLE_QUEUES");
	return err;
}

int
iavf_enable_queues_lv(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_del_ena_dis_queues *queue_select;
	struct virtchnl_queue_chunk *queue_chunk;
	struct iavf_cmd_info args;
	int err, len;

	len = sizeof(struct virtchnl_del_ena_dis_queues) +
		  sizeof(struct virtchnl_queue_chunk) *
		  (IAVF_RXTX_QUEUE_CHUNKS_NUM - 1);
	queue_select = rte_zmalloc("queue_select", len, 0);
	if (!queue_select)
		return -ENOMEM;

	queue_chunk = queue_select->chunks.chunks;
	queue_select->chunks.num_chunks = IAVF_RXTX_QUEUE_CHUNKS_NUM;
	queue_select->vport_id = vf->vsi_res->vsi_id;

	queue_chunk[VIRTCHNL_QUEUE_TYPE_TX].type = VIRTCHNL_QUEUE_TYPE_TX;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_TX].start_queue_id = 0;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_TX].num_queues =
		adapter->dev_data->nb_tx_queues;

	queue_chunk[VIRTCHNL_QUEUE_TYPE_RX].type = VIRTCHNL_QUEUE_TYPE_RX;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_RX].start_queue_id = 0;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_RX].num_queues =
		adapter->dev_data->nb_rx_queues;

	args.ops = VIRTCHNL_OP_ENABLE_QUEUES_V2;
	args.in_args = (u8 *)queue_select;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_ENABLE_QUEUES_V2");

	rte_free(queue_select);
	return err;
}

int
iavf_disable_queues_lv(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_del_ena_dis_queues *queue_select;
	struct virtchnl_queue_chunk *queue_chunk;
	struct iavf_cmd_info args;
	int err, len;

	len = sizeof(struct virtchnl_del_ena_dis_queues) +
		  sizeof(struct virtchnl_queue_chunk) *
		  (IAVF_RXTX_QUEUE_CHUNKS_NUM - 1);
	queue_select = rte_zmalloc("queue_select", len, 0);
	if (!queue_select)
		return -ENOMEM;

	queue_chunk = queue_select->chunks.chunks;
	queue_select->chunks.num_chunks = IAVF_RXTX_QUEUE_CHUNKS_NUM;
	queue_select->vport_id = vf->vsi_res->vsi_id;

	queue_chunk[VIRTCHNL_QUEUE_TYPE_TX].type = VIRTCHNL_QUEUE_TYPE_TX;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_TX].start_queue_id = 0;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_TX].num_queues =
		adapter->dev_data->nb_tx_queues;

	queue_chunk[VIRTCHNL_QUEUE_TYPE_RX].type = VIRTCHNL_QUEUE_TYPE_RX;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_RX].start_queue_id = 0;
	queue_chunk[VIRTCHNL_QUEUE_TYPE_RX].num_queues =
		adapter->dev_data->nb_rx_queues;

	args.ops = VIRTCHNL_OP_DISABLE_QUEUES_V2;
	args.in_args = (u8 *)queue_select;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_DISABLE_QUEUES_V2");

	rte_free(queue_select);
	return err;
}

int
iavf_switch_queue_lv(struct iavf_adapter *adapter, uint16_t qid,
		 bool rx, bool on)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_del_ena_dis_queues *queue_select;
	struct virtchnl_queue_chunk *queue_chunk;
	struct iavf_cmd_info args;
	int err, len;

	len = sizeof(struct virtchnl_del_ena_dis_queues);
	queue_select = rte_zmalloc("queue_select", len, 0);
	if (!queue_select)
		return -ENOMEM;

	queue_chunk = queue_select->chunks.chunks;
	queue_select->chunks.num_chunks = 1;
	queue_select->vport_id = vf->vsi_res->vsi_id;

	if (rx) {
		queue_chunk->type = VIRTCHNL_QUEUE_TYPE_RX;
		queue_chunk->start_queue_id = qid;
		queue_chunk->num_queues = 1;
	} else {
		queue_chunk->type = VIRTCHNL_QUEUE_TYPE_TX;
		queue_chunk->start_queue_id = qid;
		queue_chunk->num_queues = 1;
	}

	if (on)
		args.ops = VIRTCHNL_OP_ENABLE_QUEUES_V2;
	else
		args.ops = VIRTCHNL_OP_DISABLE_QUEUES_V2;
	args.in_args = (u8 *)queue_select;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of %s",
			    on ? "OP_ENABLE_QUEUES_V2" : "OP_DISABLE_QUEUES_V2");

	rte_free(queue_select);
	return err;
}

int
iavf_configure_rss_lut(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_rss_lut *rss_lut;
	struct iavf_cmd_info args;
	int len, err = 0;

	len = sizeof(*rss_lut) + vf->vf_res->rss_lut_size - 1;
	rss_lut = rte_zmalloc("rss_lut", len, 0);
	if (!rss_lut)
		return -ENOMEM;

	rss_lut->vsi_id = vf->vsi_res->vsi_id;
	rss_lut->lut_entries = vf->vf_res->rss_lut_size;
	rte_memcpy(rss_lut->lut, vf->rss_lut, vf->vf_res->rss_lut_size);

	args.ops = VIRTCHNL_OP_CONFIG_RSS_LUT;
	args.in_args = (u8 *)rss_lut;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_CONFIG_RSS_LUT");

	rte_free(rss_lut);
	return err;
}

int
iavf_configure_rss_key(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_rss_key *rss_key;
	struct iavf_cmd_info args;
	int len, err = 0;

	len = sizeof(*rss_key) + vf->vf_res->rss_key_size - 1;
	rss_key = rte_zmalloc("rss_key", len, 0);
	if (!rss_key)
		return -ENOMEM;

	rss_key->vsi_id = vf->vsi_res->vsi_id;
	rss_key->key_len = vf->vf_res->rss_key_size;
	rte_memcpy(rss_key->key, vf->rss_key, vf->vf_res->rss_key_size);

	args.ops = VIRTCHNL_OP_CONFIG_RSS_KEY;
	args.in_args = (u8 *)rss_key;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_CONFIG_RSS_KEY");

	rte_free(rss_key);
	return err;
}

int
iavf_configure_queues(struct iavf_adapter *adapter,
		uint16_t num_queue_pairs, uint16_t index)
{
	struct iavf_rx_queue **rxq =
		(struct iavf_rx_queue **)adapter->dev_data->rx_queues;
	struct iavf_tx_queue **txq =
		(struct iavf_tx_queue **)adapter->dev_data->tx_queues;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_vsi_queue_config_info *vc_config;
	struct virtchnl_queue_pair_info *vc_qp;
	struct iavf_cmd_info args;
	uint16_t i, size;
	int err;

	size = sizeof(*vc_config) +
	       sizeof(vc_config->qpair[0]) * num_queue_pairs;
	vc_config = rte_zmalloc("cfg_queue", size, 0);
	if (!vc_config)
		return -ENOMEM;

	vc_config->vsi_id = vf->vsi_res->vsi_id;
	vc_config->num_queue_pairs = num_queue_pairs;

	for (i = index, vc_qp = vc_config->qpair;
		 i < index + num_queue_pairs;
	     i++, vc_qp++) {
		vc_qp->txq.vsi_id = vf->vsi_res->vsi_id;
		vc_qp->txq.queue_id = i;

		/* Virtchnnl configure tx queues by pairs */
		if (i < adapter->dev_data->nb_tx_queues) {
			vc_qp->txq.ring_len = txq[i]->nb_tx_desc;
			vc_qp->txq.dma_ring_addr = txq[i]->tx_ring_phys_addr;
		}

		vc_qp->rxq.vsi_id = vf->vsi_res->vsi_id;
		vc_qp->rxq.queue_id = i;
		vc_qp->rxq.max_pkt_size = vf->max_pkt_len;

		if (i >= adapter->dev_data->nb_rx_queues)
			continue;

		/* Virtchnnl configure rx queues by pairs */
		vc_qp->rxq.ring_len = rxq[i]->nb_rx_desc;
		vc_qp->rxq.dma_ring_addr = rxq[i]->rx_ring_phys_addr;
		vc_qp->rxq.databuffer_size = rxq[i]->rx_buf_len;
		vc_qp->rxq.crc_disable = rxq[i]->crc_len != 0 ? 1 : 0;
#ifndef RTE_LIBRTE_IAVF_16BYTE_RX_DESC
		if (vf->vf_res->vf_cap_flags &
		    VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC) {
			if (vf->supported_rxdid & BIT(rxq[i]->rxdid)) {
				vc_qp->rxq.rxdid = rxq[i]->rxdid;
				PMD_DRV_LOG(NOTICE, "request RXDID[%d] in Queue[%d]",
					    vc_qp->rxq.rxdid, i);
			} else {
				PMD_DRV_LOG(NOTICE, "RXDID[%d] is not supported, "
					    "request default RXDID[%d] in Queue[%d]",
					    rxq[i]->rxdid, IAVF_RXDID_LEGACY_1, i);
				vc_qp->rxq.rxdid = IAVF_RXDID_LEGACY_1;
			}

			if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_CAP_PTP &&
			    vf->ptp_caps & VIRTCHNL_1588_PTP_CAP_RX_TSTAMP &&
			    rxq[i]->offloads & RTE_ETH_RX_OFFLOAD_TIMESTAMP)
				vc_qp->rxq.flags |= VIRTCHNL_PTP_RX_TSTAMP;
		}
#else
		if (vf->vf_res->vf_cap_flags &
			VIRTCHNL_VF_OFFLOAD_RX_FLEX_DESC &&
			vf->supported_rxdid & BIT(IAVF_RXDID_LEGACY_0)) {
			vc_qp->rxq.rxdid = IAVF_RXDID_LEGACY_0;
			PMD_DRV_LOG(NOTICE, "request RXDID[%d] in Queue[%d]",
				    vc_qp->rxq.rxdid, i);
		} else {
			PMD_DRV_LOG(ERR, "RXDID[%d] is not supported",
				    IAVF_RXDID_LEGACY_0);
			return -1;
		}
#endif
	}

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_CONFIG_VSI_QUEUES;
	args.in_args = (uint8_t *)vc_config;
	args.in_args_size = size;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " VIRTCHNL_OP_CONFIG_VSI_QUEUES");

	rte_free(vc_config);
	return err;
}

int
iavf_config_irq_map(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_irq_map_info *map_info;
	struct virtchnl_vector_map *vecmap;
	struct iavf_cmd_info args;
	int len, i, err;

	len = sizeof(struct virtchnl_irq_map_info) +
	      sizeof(struct virtchnl_vector_map) * vf->nb_msix;

	map_info = rte_zmalloc("map_info", len, 0);
	if (!map_info)
		return -ENOMEM;

	map_info->num_vectors = vf->nb_msix;
	for (i = 0; i < adapter->dev_data->nb_rx_queues; i++) {
		vecmap =
		    &map_info->vecmap[vf->qv_map[i].vector_id - vf->msix_base];
		vecmap->vsi_id = vf->vsi_res->vsi_id;
		vecmap->rxitr_idx = IAVF_ITR_INDEX_DEFAULT;
		vecmap->vector_id = vf->qv_map[i].vector_id;
		vecmap->txq_map = 0;
		vecmap->rxq_map |= 1 << vf->qv_map[i].queue_id;
	}

	args.ops = VIRTCHNL_OP_CONFIG_IRQ_MAP;
	args.in_args = (u8 *)map_info;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_CONFIG_IRQ_MAP");

	rte_free(map_info);
	return err;
}

int
iavf_config_irq_map_lv(struct iavf_adapter *adapter, uint16_t num,
		uint16_t index)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_vector_maps *map_info;
	struct virtchnl_queue_vector *qv_maps;
	struct iavf_cmd_info args;
	int len, i, err;
	int count = 0;

	len = sizeof(struct virtchnl_queue_vector_maps) +
	      sizeof(struct virtchnl_queue_vector) * (num - 1);

	map_info = rte_zmalloc("map_info", len, 0);
	if (!map_info)
		return -ENOMEM;

	map_info->vport_id = vf->vsi_res->vsi_id;
	map_info->num_qv_maps = num;
	for (i = index; i < index + map_info->num_qv_maps; i++) {
		qv_maps = &map_info->qv_maps[count++];
		qv_maps->itr_idx = VIRTCHNL_ITR_IDX_0;
		qv_maps->queue_type = VIRTCHNL_QUEUE_TYPE_RX;
		qv_maps->queue_id = vf->qv_map[i].queue_id;
		qv_maps->vector_id = vf->qv_map[i].vector_id;
	}

	args.ops = VIRTCHNL_OP_MAP_QUEUE_VECTOR;
	args.in_args = (u8 *)map_info;
	args.in_args_size = len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command OP_MAP_QUEUE_VECTOR");

	rte_free(map_info);
	return err;
}

void
iavf_add_del_all_mac_addr(struct iavf_adapter *adapter, bool add)
{
	struct virtchnl_ether_addr_list *list;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct rte_ether_addr *addr;
	struct iavf_cmd_info args;
	int len, err, i, j;
	int next_begin = 0;
	int begin = 0;

	do {
		j = 0;
		len = sizeof(struct virtchnl_ether_addr_list);
		for (i = begin; i < IAVF_NUM_MACADDR_MAX; i++, next_begin++) {
			addr = &adapter->dev_data->mac_addrs[i];
			if (rte_is_zero_ether_addr(addr))
				continue;
			len += sizeof(struct virtchnl_ether_addr);
			if (len >= IAVF_AQ_BUF_SZ) {
				next_begin = i + 1;
				break;
			}
		}

		list = rte_zmalloc("iavf_del_mac_buffer", len, 0);
		if (!list) {
			PMD_DRV_LOG(ERR, "fail to allocate memory");
			return;
		}

		for (i = begin; i < next_begin; i++) {
			addr = &adapter->dev_data->mac_addrs[i];
			if (rte_is_zero_ether_addr(addr))
				continue;
			rte_memcpy(list->list[j].addr, addr->addr_bytes,
				   sizeof(addr->addr_bytes));
			list->list[j].type = (j == 0 ?
					      VIRTCHNL_ETHER_ADDR_PRIMARY :
					      VIRTCHNL_ETHER_ADDR_EXTRA);
			PMD_DRV_LOG(DEBUG, "add/rm mac:" RTE_ETHER_ADDR_PRT_FMT,
				    RTE_ETHER_ADDR_BYTES(addr));
			j++;
		}
		list->vsi_id = vf->vsi_res->vsi_id;
		list->num_elements = j;
		args.ops = add ? VIRTCHNL_OP_ADD_ETH_ADDR :
			   VIRTCHNL_OP_DEL_ETH_ADDR;
		args.in_args = (uint8_t *)list;
		args.in_args_size = len;
		args.out_buffer = vf->aq_resp;
		args.out_size = IAVF_AQ_BUF_SZ;
		err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
		if (err)
			PMD_DRV_LOG(ERR, "fail to execute command %s",
				    add ? "OP_ADD_ETHER_ADDRESS" :
				    "OP_DEL_ETHER_ADDRESS");
		rte_free(list);
		begin = next_begin;
	} while (begin < IAVF_NUM_MACADDR_MAX);
}

int
iavf_query_stats(struct iavf_adapter *adapter,
		struct virtchnl_eth_stats **pstats)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_queue_select q_stats;
	struct iavf_cmd_info args;
	int err;

	if (adapter->closed)
		return -EIO;

	memset(&q_stats, 0, sizeof(q_stats));
	q_stats.vsi_id = vf->vsi_res->vsi_id;
	args.ops = VIRTCHNL_OP_GET_STATS;
	args.in_args = (uint8_t *)&q_stats;
	args.in_args_size = sizeof(q_stats);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_GET_STATS");
		*pstats = NULL;
		return err;
	}
	*pstats = (struct virtchnl_eth_stats *)args.out_buffer;
	return 0;
}

int
iavf_config_promisc(struct iavf_adapter *adapter,
		   bool enable_unicast,
		   bool enable_multicast)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_promisc_info promisc;
	struct iavf_cmd_info args;
	int err;

	if (adapter->closed)
		return -EIO;

	promisc.flags = 0;
	promisc.vsi_id = vf->vsi_res->vsi_id;

	if (enable_unicast)
		promisc.flags |= FLAG_VF_UNICAST_PROMISC;

	if (enable_multicast)
		promisc.flags |= FLAG_VF_MULTICAST_PROMISC;

	args.ops = VIRTCHNL_OP_CONFIG_PROMISCUOUS_MODE;
	args.in_args = (uint8_t *)&promisc;
	args.in_args_size = sizeof(promisc);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);

	if (err) {
		PMD_DRV_LOG(ERR,
			    "fail to execute command CONFIG_PROMISCUOUS_MODE");

		if (err == -ENOTSUP)
			return err;

		return -EAGAIN;
	}

	vf->promisc_unicast_enabled = enable_unicast;
	vf->promisc_multicast_enabled = enable_multicast;
	return 0;
}

int
iavf_add_del_eth_addr(struct iavf_adapter *adapter, struct rte_ether_addr *addr,
		     bool add, uint8_t type)
{
	struct virtchnl_ether_addr_list *list;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint8_t cmd_buffer[sizeof(struct virtchnl_ether_addr_list) +
			   sizeof(struct virtchnl_ether_addr)];
	struct iavf_cmd_info args;
	int err;

	if (adapter->closed)
		return -EIO;

	list = (struct virtchnl_ether_addr_list *)cmd_buffer;
	list->vsi_id = vf->vsi_res->vsi_id;
	list->num_elements = 1;
	list->list[0].type = type;
	rte_memcpy(list->list[0].addr, addr->addr_bytes,
		   sizeof(addr->addr_bytes));

	args.ops = add ? VIRTCHNL_OP_ADD_ETH_ADDR : VIRTCHNL_OP_DEL_ETH_ADDR;
	args.in_args = cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    add ? "OP_ADD_ETH_ADDR" :  "OP_DEL_ETH_ADDR");
	return err;
}

int
iavf_add_del_vlan(struct iavf_adapter *adapter, uint16_t vlanid, bool add)
{
	struct virtchnl_vlan_filter_list *vlan_list;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint8_t cmd_buffer[sizeof(struct virtchnl_vlan_filter_list) +
							sizeof(uint16_t)];
	struct iavf_cmd_info args;
	int err;

	vlan_list = (struct virtchnl_vlan_filter_list *)cmd_buffer;
	vlan_list->vsi_id = vf->vsi_res->vsi_id;
	vlan_list->num_elements = 1;
	vlan_list->vlan_id[0] = vlanid;

	args.ops = add ? VIRTCHNL_OP_ADD_VLAN : VIRTCHNL_OP_DEL_VLAN;
	args.in_args = cmd_buffer;
	args.in_args_size = sizeof(cmd_buffer);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			    add ? "OP_ADD_VLAN" :  "OP_DEL_VLAN");

	return err;
}

int
iavf_fdir_add(struct iavf_adapter *adapter,
	struct iavf_fdir_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_fdir_add *fdir_ret;

	struct iavf_cmd_info args;
	int err;

	filter->add_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->add_fltr.validate_only = 0;

	args.ops = VIRTCHNL_OP_ADD_FDIR_FILTER;
	args.in_args = (uint8_t *)(&filter->add_fltr);
	args.in_args_size = sizeof(*(&filter->add_fltr));
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_ADD_FDIR_FILTER");
		return err;
	}

	fdir_ret = (struct virtchnl_fdir_add *)args.out_buffer;
	filter->flow_id = fdir_ret->flow_id;

	if (fdir_ret->status == VIRTCHNL_FDIR_SUCCESS) {
		PMD_DRV_LOG(INFO,
			"Succeed in adding rule request by PF");
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_NORESOURCE) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to no hw resource");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_EXIST) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to the rule is already existed");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_CONFLICT) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to the rule is conflict with existing rule");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_INVALID) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to the hw doesn't support");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_TIMEOUT) {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to time out for programming");
		return -1;
	} else {
		PMD_DRV_LOG(ERR,
			"Failed to add rule request due to other reasons");
		return -1;
	}

	return 0;
};

int
iavf_fdir_del(struct iavf_adapter *adapter,
	struct iavf_fdir_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_fdir_del *fdir_ret;

	struct iavf_cmd_info args;
	int err;

	filter->del_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->del_fltr.flow_id = filter->flow_id;

	args.ops = VIRTCHNL_OP_DEL_FDIR_FILTER;
	args.in_args = (uint8_t *)(&filter->del_fltr);
	args.in_args_size = sizeof(filter->del_fltr);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_DEL_FDIR_FILTER");
		return err;
	}

	fdir_ret = (struct virtchnl_fdir_del *)args.out_buffer;

	if (fdir_ret->status == VIRTCHNL_FDIR_SUCCESS) {
		PMD_DRV_LOG(INFO,
			"Succeed in deleting rule request by PF");
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_NONEXIST) {
		PMD_DRV_LOG(ERR,
			"Failed to delete rule request due to this rule doesn't exist");
		return -1;
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_TIMEOUT) {
		PMD_DRV_LOG(ERR,
			"Failed to delete rule request due to time out for programming");
		return -1;
	} else {
		PMD_DRV_LOG(ERR,
			"Failed to delete rule request due to other reasons");
		return -1;
	}

	return 0;
};

int
iavf_fdir_check(struct iavf_adapter *adapter,
		struct iavf_fdir_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_fdir_add *fdir_ret;

	struct iavf_cmd_info args;
	int err;

	filter->add_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->add_fltr.validate_only = 1;

	args.ops = VIRTCHNL_OP_ADD_FDIR_FILTER;
	args.in_args = (uint8_t *)(&filter->add_fltr);
	args.in_args_size = sizeof(*(&filter->add_fltr));
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to check flow director rule");
		return err;
	}

	fdir_ret = (struct virtchnl_fdir_add *)args.out_buffer;

	if (fdir_ret->status == VIRTCHNL_FDIR_SUCCESS) {
		PMD_DRV_LOG(INFO,
			"Succeed in checking rule request by PF");
	} else if (fdir_ret->status == VIRTCHNL_FDIR_FAILURE_RULE_INVALID) {
		PMD_DRV_LOG(ERR,
			"Failed to check rule request due to parameters validation"
			" or HW doesn't support");
		err = -1;
	} else {
		PMD_DRV_LOG(ERR,
			"Failed to check rule request due to other reasons");
		err =  -1;
	}

	return err;
}

int
iavf_flow_sub(struct iavf_adapter *adapter, struct iavf_fsub_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_flow_sub *fsub_cfg;
	struct iavf_cmd_info args;
	int err;

	filter->sub_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->sub_fltr.validate_only = 0;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_FLOW_SUBSCRIBE;
	args.in_args = (uint8_t *)(&filter->sub_fltr);
	args.in_args_size = sizeof(*(&filter->sub_fltr));
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to execute command of "
				 "OP_FLOW_SUBSCRIBE");
		return err;
	}

	fsub_cfg = (struct virtchnl_flow_sub *)args.out_buffer;
	filter->flow_id = fsub_cfg->flow_id;

	if (fsub_cfg->status == VIRTCHNL_FSUB_SUCCESS) {
		PMD_DRV_LOG(INFO, "Succeed in adding rule request by PF");
	} else if (fsub_cfg->status == VIRTCHNL_FSUB_FAILURE_RULE_NORESOURCE) {
		PMD_DRV_LOG(ERR, "Failed to add rule request due to no hw "
				 "resource");
		err = -1;
	} else if (fsub_cfg->status == VIRTCHNL_FSUB_FAILURE_RULE_EXIST) {
		PMD_DRV_LOG(ERR, "Failed to add rule request due to the rule "
				 "is already existed");
		err = -1;
	} else if (fsub_cfg->status == VIRTCHNL_FSUB_FAILURE_RULE_INVALID) {
		PMD_DRV_LOG(ERR, "Failed to add rule request due to the hw "
				 "doesn't support");
		err = -1;
	} else {
		PMD_DRV_LOG(ERR, "Failed to add rule request due to other "
				 "reasons");
		err = -1;
	}

	return err;
}

int
iavf_flow_unsub(struct iavf_adapter *adapter, struct iavf_fsub_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_flow_unsub *unsub_cfg;
	struct iavf_cmd_info args;
	int err;

	filter->unsub_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->unsub_fltr.flow_id = filter->flow_id;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_FLOW_UNSUBSCRIBE;
	args.in_args = (uint8_t *)(&filter->unsub_fltr);
	args.in_args_size = sizeof(filter->unsub_fltr);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to execute command of "
				 "OP_FLOW_UNSUBSCRIBE");
		return err;
	}

	unsub_cfg = (struct virtchnl_flow_unsub *)args.out_buffer;

	if (unsub_cfg->status == VIRTCHNL_FSUB_SUCCESS) {
		PMD_DRV_LOG(INFO, "Succeed in deleting rule request by PF");
	} else if (unsub_cfg->status == VIRTCHNL_FSUB_FAILURE_RULE_NONEXIST) {
		PMD_DRV_LOG(ERR, "Failed to delete rule request due to this "
				 "rule doesn't exist");
		err = -1;
	} else {
		PMD_DRV_LOG(ERR, "Failed to delete rule request due to other "
				 "reasons");
		err = -1;
	}

	return err;
}

int
iavf_flow_sub_check(struct iavf_adapter *adapter,
		    struct iavf_fsub_conf *filter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_flow_sub *fsub_cfg;

	struct iavf_cmd_info args;
	int err;

	filter->sub_fltr.vsi_id = vf->vsi_res->vsi_id;
	filter->sub_fltr.validate_only = 1;

	args.ops = VIRTCHNL_OP_FLOW_SUBSCRIBE;
	args.in_args = (uint8_t *)(&filter->sub_fltr);
	args.in_args_size = sizeof(*(&filter->sub_fltr));
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to check flow subscription rule");
		return err;
	}

	fsub_cfg = (struct virtchnl_flow_sub *)args.out_buffer;

	if (fsub_cfg->status == VIRTCHNL_FSUB_SUCCESS) {
		PMD_DRV_LOG(INFO, "Succeed in checking rule request by PF");
	} else if (fsub_cfg->status == VIRTCHNL_FSUB_FAILURE_RULE_INVALID) {
		PMD_DRV_LOG(ERR, "Failed to check rule request due to "
				 "parameters validation or HW doesn't "
				 "support");
		err = -1;
	} else {
		PMD_DRV_LOG(ERR, "Failed to check rule request due to other "
				 "reasons");
		err = -1;
	}

	return err;
}

int
iavf_add_del_rss_cfg(struct iavf_adapter *adapter,
		     struct virtchnl_rss_cfg *rss_cfg, bool add)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int err;

	memset(&args, 0, sizeof(args));
	args.ops = add ? VIRTCHNL_OP_ADD_RSS_CFG :
		VIRTCHNL_OP_DEL_RSS_CFG;
	args.in_args = (u8 *)rss_cfg;
	args.in_args_size = sizeof(*rss_cfg);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of %s",
			    add ? "OP_ADD_RSS_CFG" :
			    "OP_DEL_RSS_INPUT_CFG");

	return err;
}

int
iavf_get_hena_caps(struct iavf_adapter *adapter, uint64_t *caps)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int err;

	args.ops = VIRTCHNL_OP_GET_RSS_HENA_CAPS;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_GET_RSS_HENA_CAPS");
		return err;
	}

	*caps = ((struct virtchnl_rss_hena *)args.out_buffer)->hena;
	return 0;
}

int
iavf_set_hena(struct iavf_adapter *adapter, uint64_t hena)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_rss_hena vrh;
	struct iavf_cmd_info args;
	int err;

	vrh.hena = hena;
	args.ops = VIRTCHNL_OP_SET_RSS_HENA;
	args.in_args = (u8 *)&vrh;
	args.in_args_size = sizeof(vrh);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_SET_RSS_HENA");

	return err;
}

int
iavf_get_qos_cap(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	uint32_t len;
	int err;

	args.ops = VIRTCHNL_OP_GET_QOS_CAPS;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);

	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_GET_VF_RESOURCE");
		return -1;
	}

	len =  sizeof(struct virtchnl_qos_cap_list) +
		IAVF_MAX_TRAFFIC_CLASS * sizeof(struct virtchnl_qos_cap_elem);

	rte_memcpy(vf->qos_cap, args.out_buffer,
		   RTE_MIN(args.out_size, len));

	return 0;
}

int iavf_set_q_tc_map(struct rte_eth_dev *dev,
		struct virtchnl_queue_tc_mapping *q_tc_mapping, uint16_t size)
{
	struct iavf_adapter *adapter =
			IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_cmd_info args;
	int err;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_CONFIG_QUEUE_TC_MAP;
	args.in_args = (uint8_t *)q_tc_mapping;
	args.in_args_size = size;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " VIRTCHNL_OP_CONFIG_TC_MAP");
	return err;
}

int iavf_set_q_bw(struct rte_eth_dev *dev,
		struct virtchnl_queues_bw_cfg *q_bw, uint16_t size)
{
	struct iavf_adapter *adapter =
			IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(dev->data->dev_private);
	struct iavf_cmd_info args;
	int err;

	memset(&args, 0, sizeof(args));
	args.ops = VIRTCHNL_OP_CONFIG_QUEUE_BW;
	args.in_args = (uint8_t *)q_bw;
	args.in_args_size = size;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err)
		PMD_DRV_LOG(ERR, "Failed to execute command of"
			    " VIRTCHNL_OP_CONFIG_QUEUE_BW");
	return err;
}

int
iavf_add_del_mc_addr_list(struct iavf_adapter *adapter,
			struct rte_ether_addr *mc_addrs,
			uint32_t mc_addrs_num, bool add)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	uint8_t cmd_buffer[sizeof(struct virtchnl_ether_addr_list) +
		(IAVF_NUM_MACADDR_MAX * sizeof(struct virtchnl_ether_addr))];
	struct virtchnl_ether_addr_list *list;
	struct iavf_cmd_info args;
	uint32_t i;
	int err;

	if (mc_addrs == NULL || mc_addrs_num == 0)
		return 0;

	list = (struct virtchnl_ether_addr_list *)cmd_buffer;
	list->vsi_id = vf->vsi_res->vsi_id;
	list->num_elements = mc_addrs_num;

	for (i = 0; i < mc_addrs_num; i++) {
		if (!IAVF_IS_MULTICAST(mc_addrs[i].addr_bytes)) {
			PMD_DRV_LOG(ERR, "Invalid mac:" RTE_ETHER_ADDR_PRT_FMT,
				    RTE_ETHER_ADDR_BYTES(&mc_addrs[i]));
			return -EINVAL;
		}

		memcpy(list->list[i].addr, mc_addrs[i].addr_bytes,
			sizeof(list->list[i].addr));
		list->list[i].type = VIRTCHNL_ETHER_ADDR_EXTRA;
	}

	args.ops = add ? VIRTCHNL_OP_ADD_ETH_ADDR : VIRTCHNL_OP_DEL_ETH_ADDR;
	args.in_args = cmd_buffer;
	args.in_args_size = sizeof(struct virtchnl_ether_addr_list) +
		i * sizeof(struct virtchnl_ether_addr);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);

	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command %s",
			add ? "OP_ADD_ETH_ADDR" : "OP_DEL_ETH_ADDR");
		return err;
	}

	return 0;
}

int
iavf_request_queues(struct rte_eth_dev *dev, uint16_t num)
{
	struct iavf_adapter *adapter =
		IAVF_DEV_PRIVATE_TO_ADAPTER(dev->data->dev_private);
	struct iavf_info *vf =  IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_vf_res_request vfres;
	struct iavf_cmd_info args;
	uint16_t num_queue_pairs;
	int err;
	int i = 0;

	if (!(vf->vf_res->vf_cap_flags &
		VIRTCHNL_VF_OFFLOAD_REQ_QUEUES)) {
		PMD_DRV_LOG(ERR, "request queues not supported");
		return -1;
	}

	if (num == 0) {
		PMD_DRV_LOG(ERR, "queue number cannot be zero");
		return -1;
	}
	vfres.num_queue_pairs = num;

	args.ops = VIRTCHNL_OP_REQUEST_QUEUES;
	args.in_args = (u8 *)&vfres;
	args.in_args_size = sizeof(vfres);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	if (vf->vf_res->vf_cap_flags & VIRTCHNL_VF_OFFLOAD_WB_ON_ITR) {
		err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	} else {
		rte_eal_alarm_cancel(iavf_dev_alarm_handler, dev);
		err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
		rte_eal_alarm_set(IAVF_ALARM_INTERVAL,
				  iavf_dev_alarm_handler, dev);
	}

	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command OP_REQUEST_QUEUES");
		return err;
	}

	/* wait for interrupt notification vf is resetting */
	while (i++ < MAX_TRY_TIMES) {
		if (vf->vf_reset)
			break;
		iavf_msec_delay(ASQ_DELAY_MS);
	}

	/* request queues succeeded, vf is resetting */
	if (vf->vf_reset) {
		PMD_DRV_LOG(INFO, "vf is resetting");
		return 0;
	}

	/* request additional queues failed, return available number */
	num_queue_pairs =
	  ((struct virtchnl_vf_res_request *)args.out_buffer)->num_queue_pairs;
	PMD_DRV_LOG(ERR, "request queues failed, only %u queues "
		"available", num_queue_pairs);

	return -1;
}

int
iavf_get_max_rss_queue_region(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	uint16_t qregion_width;
	int err;

	args.ops = VIRTCHNL_OP_GET_MAX_RSS_QREGION;
	args.in_args = NULL;
	args.in_args_size = 0;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to execute command of VIRTCHNL_OP_GET_MAX_RSS_QREGION");
		return err;
	}

	qregion_width =
	((struct virtchnl_max_rss_qregion *)args.out_buffer)->qregion_width;

	vf->max_rss_qregion = (uint16_t)(1 << qregion_width);

	return 0;
}



int
iavf_ipsec_crypto_request(struct iavf_adapter *adapter,
		uint8_t *msg, size_t msg_len,
		uint8_t *resp_msg, size_t resp_msg_len)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	int err;

	args.ops = VIRTCHNL_OP_INLINE_IPSEC_CRYPTO;
	args.in_args = msg;
	args.in_args_size = msg_len;
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 1);
	if (err) {
		PMD_DRV_LOG(ERR, "fail to execute command %s",
				"OP_INLINE_IPSEC_CRYPTO");
		return err;
	}

	memcpy(resp_msg, args.out_buffer, resp_msg_len);

	return 0;
}

int
iavf_set_vf_quanta_size(struct iavf_adapter *adapter, u16 start_queue_id, u16 num_queues)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct iavf_cmd_info args;
	struct virtchnl_quanta_cfg q_quanta;
	int err;

	if (adapter->devargs.quanta_size == 0)
		return 0;

	q_quanta.quanta_size = adapter->devargs.quanta_size;
	q_quanta.queue_select.type = VIRTCHNL_QUEUE_TYPE_TX;
	q_quanta.queue_select.start_queue_id = start_queue_id;
	q_quanta.queue_select.num_queues = num_queues;

	args.ops = VIRTCHNL_OP_CONFIG_QUANTA;
	args.in_args = (uint8_t *)&q_quanta;
	args.in_args_size = sizeof(q_quanta);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR, "Failed to execute command VIRTCHNL_OP_CONFIG_QUANTA");
		return err;
	}

	return 0;
}

int
iavf_get_ptp_cap(struct iavf_adapter *adapter)
{
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_ptp_caps ptp_caps;
	struct iavf_cmd_info args;
	int err;

	ptp_caps.caps = VIRTCHNL_1588_PTP_CAP_RX_TSTAMP |
			VIRTCHNL_1588_PTP_CAP_READ_PHC;

	args.ops = VIRTCHNL_OP_1588_PTP_GET_CAPS;
	args.in_args = (uint8_t *)&ptp_caps;
	args.in_args_size = sizeof(ptp_caps);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of OP_1588_PTP_GET_CAPS");
		return err;
	}

	vf->ptp_caps = ((struct virtchnl_ptp_caps *)args.out_buffer)->caps;

	return 0;
}

int
iavf_get_phc_time(struct iavf_rx_queue *rxq)
{
	struct iavf_adapter *adapter = rxq->vsi->adapter;
	struct iavf_info *vf = IAVF_DEV_PRIVATE_TO_VF(adapter);
	struct virtchnl_phc_time phc_time;
	struct iavf_cmd_info args;
	int err = 0;

	args.ops = VIRTCHNL_OP_1588_PTP_GET_TIME;
	args.in_args = (uint8_t *)&phc_time;
	args.in_args_size = sizeof(phc_time);
	args.out_buffer = vf->aq_resp;
	args.out_size = IAVF_AQ_BUF_SZ;

	rte_spinlock_lock(&vf->phc_time_aq_lock);
	err = iavf_execute_vf_cmd_safe(adapter, &args, 0);
	if (err) {
		PMD_DRV_LOG(ERR,
			    "Failed to execute command of VIRTCHNL_OP_1588_PTP_GET_TIME");
		goto out;
	}
	rxq->phc_time = ((struct virtchnl_phc_time *)args.out_buffer)->time;

out:
	rte_spinlock_unlock(&vf->phc_time_aq_lock);
	return err;
}
