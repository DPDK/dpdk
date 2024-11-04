/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2024 ZTE Corporation
 */

#ifndef ZXDH_MSG_H
#define ZXDH_MSG_H

#include <stdint.h>

#include <ethdev_driver.h>

#define ZXDH_BAR0_INDEX     0

enum ZXDH_DRIVER_TYPE {
	ZXDH_MSG_CHAN_END_MPF = 0,
	ZXDH_MSG_CHAN_END_PF,
	ZXDH_MSG_CHAN_END_VF,
	ZXDH_MSG_CHAN_END_RISC,
};

enum ZXDH_BAR_MSG_RTN {
	ZXDH_BAR_MSG_OK = 0,
	ZXDH_BAR_MSG_ERR_MSGID,
	ZXDH_BAR_MSG_ERR_NULL,
	ZXDH_BAR_MSG_ERR_TYPE, /* Message type exception */
	ZXDH_BAR_MSG_ERR_MODULE, /* Module ID exception */
	ZXDH_BAR_MSG_ERR_BODY_NULL, /* Message body exception */
	ZXDH_BAR_MSG_ERR_LEN, /* Message length exception */
	ZXDH_BAR_MSG_ERR_TIME_OUT, /* Message sending length too long */
	ZXDH_BAR_MSG_ERR_NOT_READY, /* Abnormal message sending conditions*/
	ZXDH_BAR_MEG_ERR_NULL_FUNC, /* Empty receive processing function pointer*/
	ZXDH_BAR_MSG_ERR_REPEAT_REGISTER, /* Module duplicate registration*/
	ZXDH_BAR_MSG_ERR_UNGISTER, /* Repeated deregistration*/
	/*
	 * The sending interface parameter boundary structure pointer is empty
	 */
	ZXDH_BAR_MSG_ERR_NULL_PARA,
	ZXDH_BAR_MSG_ERR_REPSBUFF_LEN, /* The length of reps_buff is too short*/
	/*
	 * Unable to find the corresponding message processing function for this module
	 */
	ZXDH_BAR_MSG_ERR_MODULE_NOEXIST,
	/*
	 * The virtual address in the parameters passed in by the sending interface is empty
	 */
	ZXDH_BAR_MSG_ERR_VIRTADDR_NULL,
	ZXDH_BAR_MSG_ERR_REPLY, /* sync msg resp_error */
	ZXDH_BAR_MSG_ERR_MPF_NOT_SCANNED,
	ZXDH_BAR_MSG_ERR_KERNEL_READY,
	ZXDH_BAR_MSG_ERR_USR_RET_ERR,
	ZXDH_BAR_MSG_ERR_ERR_PCIEID,
	ZXDH_BAR_MSG_ERR_SOCKET, /* netlink sockte err */
};

int zxdh_msg_chan_init(void);
int zxdh_bar_msg_chan_exit(void);
int zxdh_msg_chan_hwlock_init(struct rte_eth_dev *dev);

#endif /* ZXDH_MSG_H */
