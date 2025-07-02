/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */
#include "hinic3_compat.h"
#include "hinic3_mml_lib.h"

int
tool_get_valid_target(char *name, struct tool_target *target)
{
	int ret = UDA_SUCCESS;

	if (strlen(name) >= MAX_DEV_LEN) {
		PMD_DRV_LOG(ERR,
			    "Input parameter of device name is too long.");
		ret = -UDA_ELEN;
	} else {
		memcpy(target->dev_name, name, strlen(name));
		target->bus_num = 0;
	}

	return ret;
}

static void
fill_ioctl_msg_hd(struct msg_module *msg, unsigned int module,
		  unsigned int msg_format, unsigned int in_buff_len,
		  unsigned int out_buff_len, char *dev_name, int bus_num)
{
	memcpy(msg->device_name, dev_name, strlen(dev_name) + 1);

	msg->module = module;
	msg->msg_format = msg_format;
	msg->buf_in_size = in_buff_len;
	msg->buf_out_size = out_buff_len;
	msg->bus_num = bus_num;
}

static int
lib_ioctl(struct msg_module *in_buf, void *out_buf)
{
	in_buf->out_buf = out_buf;

	return hinic3_pmd_mml_ioctl(in_buf);
}

int
lib_tx_sq_info_get(struct tool_target target, struct nic_sq_info *sq_info,
		   int sq_id)
{
	struct msg_module msg_to_kernel;

	memset(&msg_to_kernel, 0, sizeof(msg_to_kernel));
	fill_ioctl_msg_hd(&msg_to_kernel, SEND_TO_NIC_DRIVER, TX_INFO,
			  (unsigned int)sizeof(int),
			  (unsigned int)sizeof(struct nic_sq_info),
			  target.dev_name, target.bus_num);
	msg_to_kernel.in_buf = (void *)&sq_id;

	return lib_ioctl(&msg_to_kernel, sq_info);
}

int
lib_tx_wqe_info_get(struct tool_target target, struct nic_sq_info *sq_info,
		    int sq_id, int wqe_id, void *nwqe, int nwqe_size)
{
	struct msg_module msg_to_kernel;
	struct hinic_wqe_info wqe = {0};

	wqe.wqe_id = wqe_id;
	wqe.q_id = sq_id;
	wqe.wqebb_cnt = nwqe_size / sq_info->sq_wqebb_size;

	memset(&msg_to_kernel, 0, sizeof(msg_to_kernel));
	fill_ioctl_msg_hd(&msg_to_kernel, SEND_TO_NIC_DRIVER, TX_WQE_INFO,
			  (unsigned int)(sizeof(struct hinic_wqe_info)),
			  nwqe_size, target.dev_name, target.bus_num);
	msg_to_kernel.in_buf = (void *)&wqe;

	return lib_ioctl(&msg_to_kernel, nwqe);
}

int
lib_rx_rq_info_get(struct tool_target target, struct nic_rq_info *rq_info,
		   int rq_id)
{
	struct msg_module msg_to_kernel;

	memset(&msg_to_kernel, 0, sizeof(msg_to_kernel));
	fill_ioctl_msg_hd(&msg_to_kernel, SEND_TO_NIC_DRIVER, RX_INFO,
			  (unsigned int)(sizeof(int)),
			  (unsigned int)sizeof(struct nic_rq_info),
			  target.dev_name, target.bus_num);
	msg_to_kernel.in_buf = &rq_id;

	return lib_ioctl(&msg_to_kernel, rq_info);
}

int
lib_rx_wqe_info_get(struct tool_target target, struct nic_rq_info *rq_info,
		    int rq_id, int wqe_id, void *nwqe, int nwqe_size)
{
	struct msg_module msg_to_kernel;
	struct hinic_wqe_info wqe = {0};

	wqe.wqe_id = wqe_id;
	wqe.q_id = rq_id;
	wqe.wqebb_cnt = nwqe_size / rq_info->rq_wqebb_size;

	memset(&msg_to_kernel, 0, sizeof(msg_to_kernel));
	fill_ioctl_msg_hd(&msg_to_kernel, SEND_TO_NIC_DRIVER, RX_WQE_INFO,
			  (unsigned int)(sizeof(struct hinic_wqe_info)),
			  nwqe_size, target.dev_name, target.bus_num);
	msg_to_kernel.in_buf = (void *)&wqe;

	return lib_ioctl(&msg_to_kernel, nwqe);
}

int
lib_rx_cqe_info_get(struct tool_target target,
		    __rte_unused struct nic_rq_info *rq_info, int rq_id,
		    int wqe_id, void *nwqe, int nwqe_size)
{
	struct msg_module msg_to_kernel;
	struct hinic_wqe_info wqe = {0};

	wqe.wqe_id = wqe_id;
	wqe.q_id = rq_id;

	memset(&msg_to_kernel, 0, sizeof(msg_to_kernel));
	fill_ioctl_msg_hd(&msg_to_kernel, SEND_TO_NIC_DRIVER, RX_CQE_INFO,
			  (unsigned int)(sizeof(struct hinic_wqe_info)),
			  nwqe_size, target.dev_name, target.bus_num);
	msg_to_kernel.in_buf = (void *)&wqe;

	return lib_ioctl(&msg_to_kernel, nwqe);
}
