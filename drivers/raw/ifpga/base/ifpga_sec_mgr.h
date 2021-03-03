/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#ifndef _IFPGA_FME_RSU_H_
#define _IFPGA_FME_RSU_H_


#include "ifpga_hw.h"

#define IFPGA_N3000_VID     0x8086
#define IFPGA_N3000_DID     0x0b30

#define IFPGA_BOOT_TYPE_FPGA     0
#define IFPGA_BOOT_TYPE_BMC      1

#define IFPGA_BOOT_PAGE_FACTORY  0
#define IFPGA_BOOT_PAGE_USER     1

#define IFPGA_RSU_DATA_BLK_SIZE  32768
#define IFPGA_RSU_START_RETRY    120
#define IFPGA_RSU_WRITE_RETRY    10
#define IFPGA_RSU_CANCEL_RETRY   30

#define IFPGA_N3000_COPY_SPEED   42700

/* status */
#define IFPGA_RSU_IDLE       0
#define IFPGA_RSU_PREPARE    1
#define IFPGA_RSU_READY      2
#define IFPGA_RSU_COPYING    3
#define IFPGA_RSU_REBOOT     4

#define IFPGA_RSU_GET_STAT(v)  (((v) >> 16) & 0xffff)
#define IFPGA_RSU_GET_PROG(v)  ((v) & 0xffff)
#define IFPGA_RSU_STATUS(s, p) ((((s) << 16) & 0xffff0000) | ((p) & 0xffff))

/* control */
#define IFPGA_RSU_ABORT      1

#define IFPGA_DUAL_CFG_CTRL0     0x200020
#define IFPGA_DUAL_CFG_CTRL1     0x200024

#define IFPGA_SEC_START_INTERVAL_MS       100
#define IFPGA_SEC_START_TIMEOUT_MS        20000
#define IFPGA_NIOS_HANDSHAKE_INTERVAL_MS  100
#define IFPGA_NIOS_HANDSHAKE_TIMEOUT_MS   5000

#define IFPGA_RSU_ERR_HW_ERROR		-1
#define IFPGA_RSU_ERR_TIMEOUT		-2
#define IFPGA_RSU_ERR_CANCELED		-3
#define IFPGA_RSU_ERR_BUSY			-4
#define IFPGA_RSU_ERR_INVALID_SIZE	-5
#define IFPGA_RSU_ERR_RW_ERROR		-6
#define IFPGA_RSU_ERR_WEAROUT		-7
#define IFPGA_RSU_ERR_FILE_READ		-8

struct ifpga_sec_mgr;

struct ifpga_sec_ops {
	int (*prepare)(struct ifpga_sec_mgr *smgr);
	int (*write_blk)(struct ifpga_sec_mgr *smgr, char *buf, uint32_t offset,
		uint32_t size);
	int (*write_done)(struct ifpga_sec_mgr *smgr);
	int (*check_complete)(struct ifpga_sec_mgr *smgr);
	int (*reload)(struct ifpga_sec_mgr *smgr, int type, int page);
	int (*cancel)(struct ifpga_sec_mgr *smgr);
	void (*cleanup)(struct ifpga_sec_mgr *smgr);
	u64 (*get_hw_errinfo)(struct ifpga_sec_mgr *smgr);
};

struct ifpga_sec_mgr {
	struct ifpga_fme_hw *fme;
	struct intel_max10_device *max10_dev;
	unsigned int rsu_length;
	/* number of bytes that copied from staging area to working area
	 * in one second, which is calculated by experiment
	 */
	unsigned int copy_speed;
	unsigned int *rsu_control;
	unsigned int *rsu_status;
	const struct ifpga_sec_ops *ops;
};

int init_sec_mgr(struct ifpga_fme_hw *fme);
void release_sec_mgr(struct ifpga_fme_hw *fme);
int fpga_update_flash(struct ifpga_fme_hw *fme, const char *image,
	uint64_t *status);
int fpga_stop_flash_update(struct ifpga_fme_hw *fme, int force);
int fpga_reload(struct ifpga_fme_hw *fme, int type, int page);


#endif /* _IFPGA_FME_RSU_H_ */
