/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2024 Marvell.
 */

#ifndef _ODM_PRIV_H_
#define _ODM_PRIV_H_

#define ODM_MAX_VFS    16
#define ODM_MAX_QUEUES 32

#define ODM_CMD_QUEUE_SIZE 4096

#define ODM_DEV_INIT	0x1
#define ODM_DEV_CLOSE	0x2
#define ODM_QUEUE_OPEN	0x3
#define ODM_QUEUE_CLOSE 0x4
#define ODM_REG_DUMP	0x5

struct odm_mbox_dev_msg {
	/* Response code */
	uint64_t rsp : 8;
	/* Number of VFs */
	uint64_t nvfs : 2;
	/* Error code */
	uint64_t err : 6;
	/* Reserved */
	uint64_t rsvd_16_63 : 48;
};

struct odm_mbox_queue_msg {
	/* Command code */
	uint64_t cmd : 8;
	/* VF ID to configure */
	uint64_t vfid : 8;
	/* Queue index in the VF */
	uint64_t qidx : 8;
	/* Reserved */
	uint64_t rsvd_24_63 : 40;
};

union odm_mbox_msg {
	uint64_t u[2];
	struct {
		struct odm_mbox_dev_msg d;
		struct odm_mbox_queue_msg q;
	};
};

#endif /* _ODM_PRIV_H_ */
