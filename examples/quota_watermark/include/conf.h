/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef _CONF_H_
#define _CONF_H_

#define RING_SIZE 1024
#define MAX_PKT_QUOTA 64

#define RX_DESC_PER_QUEUE   1024
#define TX_DESC_PER_QUEUE   1024

#define MBUF_DATA_SIZE     RTE_MBUF_DEFAULT_BUF_SIZE
#define MBUF_PER_POOL 8192

#define QUOTA_WATERMARK_MEMZONE_NAME "qw_global_vars"

#endif /* _CONF_H_ */
