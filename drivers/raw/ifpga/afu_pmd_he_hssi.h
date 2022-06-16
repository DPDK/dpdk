/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2022 Intel Corporation
 */

#ifndef AFU_PMD_HE_HSSI_H
#define AFU_PMD_HE_HSSI_H

#ifdef __cplusplus
extern "C" {
#endif

#include "afu_pmd_core.h"
#include "rte_pmd_afu.h"

#define HE_HSSI_UUID_L    0xbb370242ac130002
#define HE_HSSI_UUID_H    0x823c334c98bf11ea
#define NUM_HE_HSSI_PORTS 8

/* HE-HSSI registers definition */
#define TRAFFIC_CTRL_CMD    0x30
#define TRAFFIC_CTRL_DATA   0x38
#define TRAFFIC_CTRL_CH_SEL 0x40
#define AFU_SCRATCHPAD      0x48

#define TG_NUM_PKT        0x3c00
#define TG_PKT_LEN_TYPE   0x3c01
#define TG_DATA_PATTERN   0x3c02
#define TG_START_XFR      0x3c03
#define TG_STOP_XFR       0x3c04
#define TG_SRC_MAC_L      0x3c05
#define TG_SRC_MAC_H      0x3c06
#define TG_DST_MAC_L      0x3c07
#define TG_DST_MAC_H      0x3c08
#define TG_PKT_XFRD       0x3c09
#define TG_NUM_RND_SEEDS  3
#define TG_RANDOM_SEED(n) (0x3c0a + (n))
#define TG_PKT_LEN        0x3c0d

#define TM_NUM_PKT        0x3d00
#define TM_PKT_GOOD       0x3d01
#define TM_PKT_BAD        0x3d02
#define TM_BYTE_CNT0      0x3d03
#define TM_BYTE_CNT1      0x3d04
#define TM_AVST_RX_ERR    0x3d07
#define   OVERFLOW_ERR    (1 << 9)
#define   LENGTH_ERR      (1 << 8)
#define   OVERSIZE_ERR    (1 << 7)
#define   UNDERSIZE_ERR   (1 << 6)
#define   MAC_CRC_ERR     (1 << 5)
#define   PHY_ERR         (1 << 4)
#define   ERR_VALID       (1 << 3)

#define LOOPBACK_EN          0x3e00
#define LOOPBACK_FIFO_STATUS 0x3e01
#define   ALMOST_EMPTY    (1 << 1)
#define   ALMOST_FULL     (1 << 0)

#define MAILBOX_TIMEOUT_MS       100
#define MAILBOX_POLL_INTERVAL_MS 10

struct traffic_ctrl_cmd {
	union {
		uint64_t csr;
		struct {
			uint32_t read_cmd:1;
			uint32_t write_cmd:1;
			uint32_t ack_trans:1;
			uint32_t rsvd1:29;
			uint32_t afu_cmd_addr:16;
			uint32_t rsvd2:16;
		};
	};
};

struct traffic_ctrl_data {
	union {
		uint64_t csr;
		struct {
			uint32_t read_data;
			uint32_t write_data;
		};
	};
};

struct traffic_ctrl_ch_sel {
	union {
		uint64_t csr;
		struct {
			uint32_t channel_sel:3;
			uint32_t rsvd1:29;
			uint32_t rsvd2;
		};
	};
};

struct he_hssi_ctx {
	uint8_t *addr;
};

struct he_hssi_priv {
	struct rte_pmd_afu_he_hssi_cfg he_hssi_cfg;
	struct he_hssi_ctx he_hssi_ctx;
};

#ifdef __cplusplus
}
#endif

#endif /* AFU_PMD_HE_HSSI_H */
