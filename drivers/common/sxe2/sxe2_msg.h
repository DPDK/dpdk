
/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.
 */

#ifndef __SXE2_MSG_H__
#define __SXE2_MSG_H__

enum sfp_type_identifier {
	SXE2_SFP_TYPE_UNKNOWN       = 0x00,
	SXE2_SFP_TYPE_SFP          = 0x03,

	SXE2_SFP_TYPE_QSFP_PLUS    = 0x0D,
	SXE2_SFP_TYPE_QSFP28       = 0x11,

	SXE2_SFP_TYPE_MAX          = 0xFF,
};

#ifndef SFP_DEFINE
#define SFP_DEFINE

#define SXE2_SFP_EEP_WR            0x1
#define SXE2_SFP_EEP_QSFP          0x1

enum sfp_bus_addr {
	SXE2_SFP_EEP_I2C_ADDR0 = 0xA0,
	SXE2_SFP_EEP_I2C_ADDR1 = 0xA2,
	SXE2_SFP_EEP_I2C_ADDR_NR = 0xFFFF,
};

struct sxe2_sfp_req {
	uint8_t is_wr;
	uint8_t is_qsfp;
	uint16_t bus_addr;
	uint16_t page_cnt;
	uint16_t offset;
	uint16_t data_len;
	uint16_t rvd;
	uint8_t data[];
};

struct sxe2_sfp_resp {
	uint8_t is_wr;
	uint8_t is_qsfp;
	uint16_t data_len;
	uint8_t data[];
};

enum sfp_page_cnt {
	SXE2_SFP_EEP_PAGE_CNT0     = 0,
	SXE2_SFP_EEP_PAGE_CNT1,
	SXE2_SFP_EEP_PAGE_CNT2,
	SXE2_SFP_EEP_PAGE_CNT3,
	SXE2_SFP_EEP_PAGE_CNT20    = 20,
	SXE2_SFP_EEP_PAGE_CNT21    = 21,

	SXE2_SFP_EEP_PAGE_CNT_NR   = 0xFFFF,
};

#define SXE2_SFP_E2P_I2C_7BIT_ADDR0             (SXE2_SFP_EEP_I2C_ADDR0 >> 1)
#define SXE2_SFP_E2P_I2C_7BIT_ADDR1             (SXE2_SFP_EEP_I2C_ADDR1 >> 1)

#define SXE2_QSFP_PAGE_OFST_START  128
#define SXE2_SFP_EEP_OFST_MAX      255
#define SXE2_SFP_EEP_LEN_MAX       256
#endif

#ifndef FW_STATE_DEFINE
#define FW_STATE_DEFINE

#define SXE2_FW_STATUS_MAIN_SHIF       (16)
#define SXE2_FW_STATUS_MAIN_MASK       (0xFF0000)
#define SXE2_FW_STATUS_SUB_MASK        (0xFFFF)

enum Sxe2FwStateMain {
	SXE2_FW_STATE_MAIN_UNDEFINED       = 0x00,
	SXE2_FW_STATE_MAIN_INIT            = 0x10000,
	SXE2_FW_STATE_MAIN_RUN             = 0x20000,
	SXE2_FW_STATE_MAIN_ABNOMAL         = 0x30000,
};

enum Sxe2FwState {
	SXE2_FW_START_STATE_UNDEFINED = SXE2_FW_STATE_MAIN_UNDEFINED,
	SXE2_FW_START_STATE_INIT_BASE = (SXE2_FW_STATE_MAIN_INIT + 0x1),
	SXE2_FW_START_STATE_SCAN_DEVICE = (SXE2_FW_STATE_MAIN_INIT + 0x20),
	SXE2_FW_START_STATE_FINISHED = (SXE2_FW_STATE_MAIN_RUN + 0x0),
	SXE2_FW_START_STATE_UPGRADE = (SXE2_FW_STATE_MAIN_RUN + 0x1),
	SXE2_FW_START_STATE_SYNC = (SXE2_FW_STATE_MAIN_RUN + 0x2),
	SXE2_FW_RUNNING_STATE_ABNOMAL = (SXE2_FW_STATE_MAIN_ABNOMAL + 0x1),
	SXE2_FW_RUNNING_STATE_ABNOMAL_CORE1 = (SXE2_FW_STATE_MAIN_ABNOMAL + 0x2),
	SXE2_FW_RUNNING_STATE_ABNOMAL_HEART = (SXE2_FW_STATE_MAIN_ABNOMAL + 0x3),
	SXE2_FW_START_STATE_MASK = (SXE2_FW_STATUS_MAIN_MASK | SXE2_FW_STATUS_SUB_MASK),
};
#endif

#ifndef LED_DEFINE
#define LED_DEFINE

enum sxe2_led_mode {
	SXE2_IDENTIFY_LED_BLINK_ON   = 0,
	SXE2_IDENTIFY_LED_BLINK_OFF,
	SXE2_IDENTIFY_LED_ON,
	SXE2_IDENTIFY_LED_OFF,
	SXE2_IDENTIFY_LED_RESET,
};


typedef struct sxe2_led_ctrl {
	uint32_t mode;
	uint32_t duration;
} sxe2_led_ctrl_s;

typedef struct sxe2_led_ctrl_resp {
	uint32_t ack;
} sxe2_led_ctrl_resp_s;
#endif

#endif /* __SXE2_MSG_H__ */
