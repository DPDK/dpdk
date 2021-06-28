/* SPDX-License-Identifier: (BSD-3-Clause OR GPL-2.0)
 * Copyright(c) 2021 Intel Corporation
 */
#ifndef ADF_PF2VF_MSG_H_
#define ADF_PF2VF_MSG_H_

/* VF/PF compatibility version. */
/* ADF_PFVF_COMPATIBILITY_EXT_CAP: Support for extended capabilities */
#define ADF_PFVF_COMPATIBILITY_CAPABILITIES	2
/* ADF_PFVF_COMPATIBILITY_FAST_ACK: In-use pattern cleared by receiver */
#define ADF_PFVF_COMPATIBILITY_FAST_ACK		3
#define ADF_PFVF_COMPATIBILITY_RING_TO_SVC_MAP	4
#define ADF_PFVF_COMPATIBILITY_VERSION		4	/* PF<->VF compat */

#define ADF_PFVF_INT				    1
#define ADF_PFVF_MSGORIGIN_SYSTEM		2
#define ADF_PFVF_1X_MSGTYPE_SHIFT		2
#define ADF_PFVF_1X_MSGTYPE_MASK		0xF
#define ADF_PFVF_1X_MSGDATA_SHIFT		6
#define ADF_PFVF_1X_MSGDATA_MASK		0x3FF
#define ADF_PFVF_2X_MSGTYPE_SHIFT		2
#define ADF_PFVF_2X_MSGTYPE_MASK		0x3F
#define ADF_PFVF_2X_MSGDATA_SHIFT		8
#define ADF_PFVF_2X_MSGDATA_MASK		0xFFFFFF

#define ADF_PFVF_IN_USE				0x6AC2
#define ADF_PFVF_IN_USE_MASK			0xFFFE
#define ADF_PFVF_VF_MSG_SHIFT			16

/* PF->VF messages */
#define ADF_PF2VF_MSGTYPE_RESTARTING		0x01
#define ADF_PF2VF_MSGTYPE_VERSION_RESP		0x02
#define ADF_PF2VF_MSGTYPE_BLOCK_RESP		0x03
#define ADF_PF2VF_MSGTYPE_FATAL_ERROR		0x04
/* Do not use messages which start from 0x10 to 1.x as 1.x only use
 * 4 bits as message types. Hence they are only applicable to 2.0
 */
#define ADF_PF2VF_MSGTYPE_RP_RESET_RESP		0x10

/* PF->VF Version Response - ADF_PF2VF_MSGTYPE_VERSION_RESP */
#define ADF_PF2VF_VERSION_RESP_VERS_MASK	0xFF
#define ADF_PF2VF_VERSION_RESP_VERS_SHIFT	0
#define ADF_PF2VF_VERSION_RESP_RESULT_MASK	0x03
#define ADF_PF2VF_VERSION_RESP_RESULT_SHIFT	8
#define ADF_PF2VF_MINORVERSION_SHIFT		0
#define ADF_PF2VF_MAJORVERSION_SHIFT		4
#define ADF_PF2VF_VF_COMPATIBLE			1
#define ADF_PF2VF_VF_INCOMPATIBLE		2
#define ADF_PF2VF_VF_COMPAT_UNKNOWN		3

/* PF->VF Block Response Type - ADF_PF2VF_MSGTYPE_BLOCK_RESP */
#define ADF_PF2VF_BLOCK_RESP_TYPE_DATA		0x0
#define ADF_PF2VF_BLOCK_RESP_TYPE_CRC		0x1
#define ADF_PF2VF_BLOCK_RESP_TYPE_ERROR		0x2
#define ADF_PF2VF_BLOCK_RESP_TYPE_MASK		0x03
#define ADF_PF2VF_BLOCK_RESP_TYPE_SHIFT		0
#define ADF_PF2VF_BLOCK_RESP_DATA_MASK		0xFF
#define ADF_PF2VF_BLOCK_RESP_DATA_SHIFT		2

/*
 * PF->VF Block Error Code - Returned in data field when the
 * response type indicates an error
 */
#define ADF_PF2VF_INVALID_BLOCK_TYPE		0x0
#define ADF_PF2VF_INVALID_BYTE_NUM_REQ		0x1
#define ADF_PF2VF_PAYLOAD_TRUNCATED		0x2
#define ADF_PF2VF_UNSPECIFIED_ERROR		0x3

/* VF->PF messages */
#define ADF_VF2PF_MSGTYPE_INIT			0x3
#define ADF_VF2PF_MSGTYPE_SHUTDOWN		0x4
#define ADF_VF2PF_MSGTYPE_VERSION_REQ		0x5
#define ADF_VF2PF_MSGTYPE_COMPAT_VER_REQ	0x6
#define ADF_VF2PF_MSGTYPE_GET_LARGE_BLOCK_REQ	0x7
#define ADF_VF2PF_MSGTYPE_GET_MEDIUM_BLOCK_REQ	0x8
#define ADF_VF2PF_MSGTYPE_GET_SMALL_BLOCK_REQ	0x9
/* Do not use messages which start from 0x10 to 1.x as 1.x only use
 * 4 bits as message types. Hence they are only applicable to 2.0
 */
#define ADF_VF2PF_MSGTYPE_RP_RESET		0x10

/* VF->PF Block Request Type - ADF_VF2PF_MSGTYPE_GET_xxx_BLOCK_REQ  */
#define ADF_VF2PF_MIN_SMALL_MESSAGE_TYPE	0
#define ADF_VF2PF_MAX_SMALL_MESSAGE_TYPE \
		(ADF_VF2PF_MIN_SMALL_MESSAGE_TYPE + 15)
#define ADF_VF2PF_MIN_MEDIUM_MESSAGE_TYPE \
		(ADF_VF2PF_MAX_SMALL_MESSAGE_TYPE + 1)
#define ADF_VF2PF_MAX_MEDIUM_MESSAGE_TYPE \
		(ADF_VF2PF_MIN_MEDIUM_MESSAGE_TYPE + 7)
#define ADF_VF2PF_MIN_LARGE_MESSAGE_TYPE \
		(ADF_VF2PF_MAX_MEDIUM_MESSAGE_TYPE + 1)
#define ADF_VF2PF_MAX_LARGE_MESSAGE_TYPE \
		(ADF_VF2PF_MIN_LARGE_MESSAGE_TYPE + 3)
#define ADF_VF2PF_SMALL_PAYLOAD_SIZE		30
#define ADF_VF2PF_MEDIUM_PAYLOAD_SIZE		62
#define ADF_VF2PF_LARGE_PAYLOAD_SIZE		126

#define ADF_VF2PF_BLOCK_REQ_TYPE_SHIFT		0
#define ADF_VF2PF_LARGE_BLOCK_REQ_TYPE_MASK	0x3
#define ADF_VF2PF_MEDIUM_BLOCK_REQ_TYPE_MASK	0x7
#define ADF_VF2PF_SMALL_BLOCK_REQ_TYPE_MASK	0xF

#define ADF_VF2PF_LARGE_BLOCK_BYTE_NUM_SHIFT	2
#define ADF_VF2PF_LARGE_BLOCK_BYTE_NUM_MASK	0x7F
#define ADF_VF2PF_MEDIUM_BLOCK_BYTE_NUM_SHIFT	3
#define ADF_VF2PF_MEDIUM_BLOCK_BYTE_NUM_MASK	0x3F
#define ADF_VF2PF_SMALL_BLOCK_BYTE_NUM_SHIFT	4
#define ADF_VF2PF_SMALL_BLOCK_BYTE_NUM_MASK	0x1F
#define ADF_VF2PF_BLOCK_REQ_CRC_SHIFT		9

/* PF-VF block message header bytes */
#define ADF_VF2PF_BLOCK_VERSION_BYTE		0
#define ADF_VF2PF_BLOCK_LEN_BYTE                1
#define ADF_VF2PF_BLOCK_DATA	                2

/* Block message types
 *  0..15 - 32 byte message
 * 16..23 - 64 byte message
 * 24..27 - 128 byte message
 * 2 - Get Capability Request message
 */
#define ADF_VF2PF_BLOCK_MSG_CAP_SUMMARY         0x2
#define ADF_VF2PF_BLOCK_MSG_GET_RING_TO_SVC_REQ 0x3

/* VF->PF Compatible Version Request - ADF_VF2PF_MSGTYPE_VERSION_REQ */
#define ADF_VF2PF_COMPAT_VER_SHIFT		0
#define ADF_VF2PF_COMPAT_VER_MASK		0xFF

/* How long to wait for far side to acknowledge receipt */
#define ADF_IOV_MSG_ACK_DELAY_US		5
#define ADF_IOV_MSG_ACK_MAX_RETRY	(100 * 1000 / ADF_IOV_MSG_ACK_DELAY_US)
/* If CSR is busy, how long to delay before retrying */
#define ADF_IOV_MSG_RETRY_DELAY			5
#define ADF_IOV_MSG_MAX_RETRIES			3
/* How long to wait for a response from the other side */
#define ADF_IOV_MSG_RESP_TIMEOUT		100
/* How often to retry when there is no response */
#define ADF_IOV_MSG_RESP_RETRIES		5

#define ADF_IOV_RATELIMIT_INTERVAL		8
#define ADF_IOV_RATELIMIT_BURST			130
/* PF VF message byte shift */
#define ADF_PFVF_DATA_SHIFT                     8
#define ADF_PFVF_DATA_MASK                      0xFF

/* CRC Calculation */
#define ADF_CRC8_INIT_VALUE 0xFF

/* Per device register offsets */
/* GEN 4 */
#define ADF_4XXXIOV_PF2VM_OFFSET	0x1008
#define ADF_4XXXIOV_VM2PF_OFFSET	0x100C

#endif /* ADF_IOV_MSG_H */
