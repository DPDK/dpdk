/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) Huawei Technologies Co., Ltd. 2025. All rights reserved.
 */

#ifndef _HINIC3_MML_LIB
#define _HINIC3_MML_LIB

#include <string.h>
#include <stdint.h>

#include "hinic3_mml_cmd.h"
#include "hinic3_compat.h"
#include "hinic3_mgmt.h"

#define MAX_DEV_LEN	      16
#define TRGET_UNKNOWN_BUS_NUM (-1)

#ifndef DEV_NAME_LEN
#define DEV_NAME_LEN 64
#endif

enum {
	UDA_SUCCESS = 0x0,
	UDA_FAIL,
	UDA_ENXIO,
	UDA_ENONMEM,
	UDA_EBUSY,
	UDA_ECRC,
	UDA_EINVAL,
	UDA_EFAULT,
	UDA_ELEN,
	UDA_ECMD,
	UDA_ENODRIVER,
	UDA_EXIST,
	UDA_EOVERSTEP,
	UDA_ENOOBJ,
	UDA_EOBJ,
	UDA_ENOMATCH,
	UDA_ETIMEOUT,

	UDA_CONTOP,

	UDA_REBOOT = 0xFD,
	UDA_CANCEL = 0xFE,
	UDA_KILLED = 0xFF,
};

#define PARAM_NEED     1
#define PARAM_NOT_NEED 0

#define BASE_ALL 0
#define BASE_8	 8
#define BASE_10	 10
#define BASE_16	 16

enum module_name {
	SEND_TO_NPU = 1,
	SEND_TO_MPU,
	SEND_TO_SM,

	SEND_TO_HW_DRIVER,
	SEND_TO_NIC_DRIVER,
	SEND_TO_OVS_DRIVER,
	SEND_TO_ROCE_DRIVER,
	SEND_TO_TOE_DRIVER,
	SEND_TO_IWAP_DRIVER,
	SEND_TO_FC_DRIVER,
	SEND_FCOE_DRIVER,
};

enum driver_cmd_type {
	TX_INFO = 1,
	Q_NUM,
	TX_WQE_INFO,
	TX_MAPPING,
	RX_INFO,
	RX_WQE_INFO,
	RX_CQE_INFO
};

struct tool_target {
	int bus_num;
	char dev_name[MAX_DEV_LEN];
	void *pri;
};

struct nic_tx_hw_page {
	long long phy_addr;
	long long *map_addr;
};

struct nic_sq_info {
	unsigned short q_id;
	unsigned short pi; /**< Ring buffer queue producer point. */
	unsigned short ci; /**< Ring buffer queue consumer point. */
	unsigned short fi; /**< Ring buffer queue complete point. */
	unsigned int sq_depth;
	unsigned short sq_wqebb_size;
	unsigned short *ci_addr;
	uint64_t cla_addr;

	struct nic_tx_hw_page doorbell;
	unsigned int page_idx;
};

struct comm_info_l2nic_sq_ci_attr {
	struct mgmt_msg_head msg_head;

	uint16_t func_idx;
	uint8_t dma_attr_off;
	uint8_t pending_limit;

	uint8_t coalescing_time;
	uint8_t int_en;
	uint16_t int_offset;

	uint32_t l2nic_sqn;
	uint32_t rsv;
	uint64_t ci_addr;
};

struct nic_rq_info {
	unsigned short q_id; /**< Queue id in current function, 0, 1, 2... */

	unsigned short hw_pi; /**< Where pkt buf allocated. */
	unsigned short ci;    /**< Where hw pkt received, owned by hw. */
	unsigned short sw_pi; /**< Where driver begin receive pkt. */
	unsigned short rq_wqebb_size; /**< wqebb size, default to 32 bytes. */

	unsigned short rq_depth;
	unsigned short buf_len; /**< 2K. */
	void *ci_wqe_page_addr; /**< For queue context init. */
	void *ci_cla_tbl_addr;
	unsigned short int_num;	  /**< RSS support should consider int_num. */
	unsigned int msix_vector; /**< For debug. */
};

struct hinic_wqe_info {
	int q_id;
	void *slq_handle;
	uint32_t wqe_id;
	uint32_t wqebb_cnt;
};

struct npu_cmd_st {
	uint32_t mod : 8;
	uint32_t cmd : 8;
	uint32_t ack_type : 3;
	uint32_t direct_resp : 1;
	uint32_t len : 12;
};

struct mpu_cmd_st {
	uint32_t api_type : 8;
	uint32_t mod : 8;
	uint32_t cmd : 16;
};

struct msg_module {
	char device_name[DEV_NAME_LEN];
	uint32_t module;
	union {
		uint32_t msg_format; /**< For driver. */
		struct npu_cmd_st npu_cmd;
		struct mpu_cmd_st mpu_cmd;
	};
	uint32_t timeout; /**< For mpu/npu cmd. */
	uint32_t func_idx;
	uint32_t buf_in_size;
	uint32_t buf_out_size;
	void *in_buf;
	void *out_buf;
	int bus_num;
	uint32_t rsvd2[5];
};

/**
 * Convert the provided string into `uint32_t` according to the specified base.
 *
 * @param[in] nptr
 * The string to be converted.
 * @param[in] base
 * The base to use for conversion (e.g., 10 for decimal, 16 for hexadecimal).
 * @param[out] value
 * The output variable where the converted `uint32_t` value will be stored.
 *
 * @return
 * 0 on success, non-zero on failure.
 * - -UDA_EINVAL if the string is invalid or the value is out of range.
 */
static inline int
string_toui(const char *nptr, int base, uint32_t *value)
{
	char *endptr = NULL;
	long tmp_value;

	tmp_value = strtol(nptr, &endptr, base);
	if ((*endptr != 0) || tmp_value >= 0x7FFFFFFF || tmp_value < 0)
		return -UDA_EINVAL;
	*value = (uint32_t)tmp_value;
	return UDA_SUCCESS;
}

#define UDA_TRUE  1
#define UDA_FALSE 0

/**
 * Format and append a log message to a string buffer.
 *
 * @param[out] show_str
 * The string buffer where the formatted message will be appended.
 * @param[out] show_len
 * The current length of the string in the buffer. It is updated after
 * appending.
 * @param[in] fmt
 * The format string that specifies how to format the log message.
 * @param[in] args
 * The variable arguments to be formatted according to the format string.
 */
static inline void
__rte_format_printf(3, 4)
hinic3_pmd_mml_log(char *show_str, int *show_len, const char *fmt, ...)
{
	va_list args;
	int ret = 0;

	va_start(args, fmt);
	ret = vsprintf(show_str + *show_len, fmt, args);
	va_end(args);

	if (ret > 0) {
		*show_len += ret;
	} else {
		PMD_DRV_LOG(ERR, "MML show string snprintf failed, err: %d",
			    ret);
	}
}

/**
 * Get a valid target device based on the given name.
 *
 * This function checks if the device name is valid (within the length limit)
 * and then stores it in the target structure. The bus number is initialized to
 * 0.
 *
 * @param[in] name
 * The device name to be validated and stored.
 * @param[out] target
 * The structure where the device name and bus number will be stored.
 *
 * @return
 * 0 on success, non-zero on failure.
 */
int tool_get_valid_target(char *name, struct tool_target *target);

int hinic3_pmd_mml_ioctl(void *msg);

int lib_tx_sq_info_get(struct tool_target target, struct nic_sq_info *sq_info,
		       int sq_id);

int lib_tx_wqe_info_get(struct tool_target target, struct nic_sq_info *sq_info,
			int sq_id, int wqe_id, void *nwqe, int nwqe_size);

int lib_rx_rq_info_get(struct tool_target target, struct nic_rq_info *rq_info,
		       int rq_id);

int lib_rx_wqe_info_get(struct tool_target target, struct nic_rq_info *rq_info,
			int rq_id, int wqe_id, void *nwqe, int nwqe_size);

int lib_rx_cqe_info_get(struct tool_target target, struct nic_rq_info *rq_info,
			int rq_id, int wqe_id, void *nwqe, int nwqe_size);

int hinic3_pmd_mml_lib(const char *buf_in, uint32_t in_size, char *buf_out,
		       uint32_t *out_len, uint32_t max_buf_out_len);

#endif /* _HINIC3_MML_LIB */
