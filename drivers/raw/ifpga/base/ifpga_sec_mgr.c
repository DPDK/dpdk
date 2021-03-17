/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "ifpga_sec_mgr.h"


static const char * const rsu_prog[] = {"IDLE", "PREPARING", "SLEEPING",
	"READY", "AUTHENTICATING", "COPYING", "CANCELLATION", "PROGRAMMING_KEY",
	"DONE", "PKVL_DONE"};
static const char * const rsu_statl[] = {"NORMAL", "TIMEOUT", "AUTH_FAIL",
	"COPY_FAIL", "FATAL", "PKVL_REJECT", "NON_INCR", "ERASE_FAIL",
	"WEAROUT"};
static const char * const rsu_stath[] = {"NIOS_OK", "USER_OK", "FACTORY_OK",
	"USER_FAIL", "FACTORY_FAIL", "NIOS_FLASH_ERR", "FPGA_FLASH_ERR"};

static const char *rsu_progress_name(uint32_t prog)
{
	if (prog > SEC_PROGRESS_PKVL_PROM_DONE)
		return "UNKNOWN";
	else
		return rsu_prog[prog];
}

static const char *rsu_status_name(uint32_t stat)
{
	if (stat >= SEC_STATUS_NIOS_OK) {
		if (stat > SEC_STATUS_FPGA_FLASH_ERR)
			return "UNKNOWN";
		else
			return rsu_stath[stat-SEC_STATUS_NIOS_OK];
	} else {
		if (stat > SEC_STATUS_WEAROUT)
			return "UNKNOWN";
		else
			return rsu_statl[stat];
	}
}

static bool secure_start_done(uint32_t doorbell)
{
	return (SEC_STATUS_G(doorbell) == SEC_STATUS_ERASE_FAIL ||
		SEC_STATUS_G(doorbell) == SEC_STATUS_WEAROUT ||
		(SEC_PROGRESS_G(doorbell) != SEC_PROGRESS_IDLE &&
		SEC_PROGRESS_G(doorbell) != SEC_PROGRESS_RSU_DONE));
}

static bool secure_prog_ready(uint32_t doorbell)
{
	return (SEC_PROGRESS_G(doorbell) != SEC_PROGRESS_READY);
}

static int poll_timeout(struct intel_max10_device *dev, uint32_t offset,
	bool (*cond)(uint32_t), uint32_t interval_ms, uint32_t timeout_ms)
{
	uint32_t val = 0;
	int ret = 0;

	for (;;) {
		ret = max10_sys_read(dev, offset, &val);
		if (ret < 0) {
			dev_err(dev,
				"Failed to read max10 register 0x%x [e:%d]\n",
				offset, ret);
			break;
		}

		if (cond(val)) {
			dev_debug(dev,
				"Read 0x%08x from max10 register 0x%x "
				"[poll success]\n", val, offset);
			ret = 0;
			break;
		}
		if (timeout_ms > interval_ms)
			timeout_ms -= interval_ms;
		else
			timeout_ms = 0;
		if (timeout_ms == 0) {
			dev_debug(dev,
				"Read 0x%08x from max10 register 0x%x "
				"[poll timeout]\n", val, offset);
			ret = -ETIMEDOUT;
			break;
		}
		msleep(interval_ms);
	}

	return ret;
}

static int n3000_secure_update_start(struct intel_max10_device *dev)
{
	uint32_t doorbell = 0;
	uint32_t prog = 0;
	uint32_t status = 0;
	int ret = 0;

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	prog = SEC_PROGRESS_G(doorbell);
	if ((prog != SEC_PROGRESS_IDLE) && (prog != SEC_PROGRESS_RSU_DONE)) {
		dev_debug(dev, "Current RSU progress is %s\n",
			rsu_progress_name(prog));
		return -EBUSY;
	}

	ret = max10_sys_update_bits(dev, MAX10_DOORBELL,
		RSU_REQUEST | HOST_STATUS, RSU_REQUEST);
	if (ret < 0) {
		dev_err(dev,
			"Failed to updt max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	ret = poll_timeout(dev, MAX10_DOORBELL, secure_start_done,
		IFPGA_SEC_START_INTERVAL_MS, IFPGA_SEC_START_TIMEOUT_MS);
	if (ret < 0) {
		dev_err(dev,
			"Failed to poll max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	status = SEC_STATUS_G(doorbell);
	if (status == SEC_STATUS_WEAROUT)
		return -EAGAIN;

	if (status == SEC_STATUS_ERASE_FAIL)
		return -EIO;

	return 0;
}

static int n3000_cancel(struct ifpga_sec_mgr *smgr)
{
	struct intel_max10_device *dev = NULL;
	uint32_t doorbell = 0;
	uint32_t prog = 0;
	int ret = 0;

	if (!smgr || !smgr->max10_dev)
		return -ENODEV;
	dev = (struct intel_max10_device *)smgr->max10_dev;

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	prog = SEC_PROGRESS_G(doorbell);
	if (prog == SEC_PROGRESS_IDLE)
		return 0;
	if (prog != SEC_PROGRESS_READY)
		return -EBUSY;

	return max10_sys_update_bits(dev, MAX10_DOORBELL, HOST_STATUS,
		HOST_STATUS_S(HOST_STATUS_ABORT_RSU));
}

static int n3000_prepare(struct ifpga_sec_mgr *smgr)
{
	struct intel_max10_device *dev = NULL;
	int retry = 0;
	int ret = 0;

	if (!smgr || !smgr->max10_dev)
		return -ENODEV;
	dev = (struct intel_max10_device *)smgr->max10_dev;

	ret = n3000_secure_update_start(dev);
	if (ret == -EBUSY)
		n3000_cancel(smgr);

	while (ret) {
		if (++retry > IFPGA_RSU_START_RETRY)
			break;
		msleep(1000);
		ret = n3000_secure_update_start(dev);
	}
	if (retry > IFPGA_RSU_START_RETRY) {
		dev_err(dev, "Failed to start secure flash update\n");
		ret = -EAGAIN;
	}

	return ret;
}

static int n3000_bulk_write(struct intel_max10_device *dev, uint32_t addr,
	char *buf, uint32_t len)
{
	uint32_t i = 0;
	uint32_t n = 0;
	uint32_t v = 0;
	uint32_t p = 0;
	int ret = 0;

	if (len & 0x3) {
		dev_err(dev,
			"Length of data block is not 4 bytes aligned [e:%u]\n",
			len);
		return -EINVAL;
	}

	n = len >> 2;
	for (i = 0; i < n; i++) {
		p = i << 2;
		v = *(uint32_t *)(buf + p);
		ret = max10_reg_write(dev, addr + p, v);
		if (ret < 0) {
			dev_err(dev,
				"Failed to write to staging area 0x%08x [e:%d]\n",
				addr + p, ret);
			return ret;
		}
		usleep(1);
	}

	return 0;
}

static int n3000_write_blk(struct ifpga_sec_mgr *smgr, char *buf,
	uint32_t offset, uint32_t len)
{
	struct intel_max10_device *dev = NULL;
	uint32_t doorbell = 0;
	uint32_t prog = 0;
	uint32_t m = 0;
	int ret = 0;

	if (!smgr || !smgr->max10_dev)
		return -ENODEV;
	dev = (struct intel_max10_device *)smgr->max10_dev;

	if (offset + len > dev->staging_area_size) {
		dev_err(dev,
			"Write position would be out of staging area [e:%u]\n",
			dev->staging_area_size);
		return -ENOMEM;
	}

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	prog = SEC_PROGRESS_G(doorbell);
	if (prog == SEC_PROGRESS_PREPARE)
		return -EAGAIN;
	else if (prog != SEC_PROGRESS_READY)
		return -EBUSY;

	m = len & 0x3;
	if (m != 0)
		len += 4 - m;   /* make length to 4 bytes align */

	return n3000_bulk_write(dev, dev->staging_area_base + offset, buf, len);
}

static int n3000_write_done(struct ifpga_sec_mgr *smgr)
{
	struct intel_max10_device *dev = NULL;
	uint32_t doorbell = 0;
	uint32_t prog = 0;
	uint32_t status = 0;
	int ret = 0;

	if (!smgr || !smgr->max10_dev)
		return -ENODEV;
	dev = (struct intel_max10_device *)smgr->max10_dev;

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	prog = SEC_PROGRESS_G(doorbell);
	if (prog != SEC_PROGRESS_READY)
		return -EBUSY;

	ret = max10_sys_update_bits(dev, MAX10_DOORBELL, HOST_STATUS,
		HOST_STATUS_S(HOST_STATUS_WRITE_DONE));
	if (ret < 0) {
		dev_err(dev,
			"Failed to update max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	ret = poll_timeout(dev, MAX10_DOORBELL, secure_prog_ready,
		IFPGA_NIOS_HANDSHAKE_INTERVAL_MS,
		IFPGA_NIOS_HANDSHAKE_TIMEOUT_MS);
	if (ret < 0) {
		dev_err(dev,
			"Failed to poll max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	status = SEC_STATUS_G(doorbell);
	switch (status) {
	case SEC_STATUS_NORMAL:
	case SEC_STATUS_NIOS_OK:
	case SEC_STATUS_USER_OK:
	case SEC_STATUS_FACTORY_OK:
		ret = 0;
		break;
	default:
		ret = -EIO;
		break;
	}

	return ret;
}

static int n3000_check_complete(struct ifpga_sec_mgr *smgr)
{
	struct intel_max10_device *dev = NULL;
	uint32_t doorbell = 0;
	uint32_t status = 0;
	uint32_t prog = 0;
	int ret = 0;

	if (!smgr || !smgr->max10_dev)
		return -ENODEV;
	dev = (struct intel_max10_device *)smgr->max10_dev;

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return ret;
	}

	status = SEC_STATUS_G(doorbell);
	switch (status) {
	case SEC_STATUS_NORMAL:
	case SEC_STATUS_NIOS_OK:
	case SEC_STATUS_USER_OK:
	case SEC_STATUS_FACTORY_OK:
	case SEC_STATUS_WEAROUT:
		break;
	default:
		return -EIO;
	}

	prog = SEC_PROGRESS_G(doorbell);
	switch (prog) {
	case SEC_PROGRESS_IDLE:
	case SEC_PROGRESS_RSU_DONE:
		return 0;
	case SEC_PROGRESS_AUTHENTICATING:
	case SEC_PROGRESS_COPYING:
	case SEC_PROGRESS_UPDATE_CANCEL:
	case SEC_PROGRESS_PROGRAM_KEY_HASH:
		return -EAGAIN;
	case SEC_PROGRESS_PREPARE:
	case SEC_PROGRESS_READY:
		return -EBUSY;
	default:
		return -EIO;
	}

	return 0;
}

static int n3000_reload_fpga(struct intel_max10_device *dev, int page)
{
	int ret = 0;

	dev_info(dev, "Reload FPGA\n");

	if (!dev || ((page != 0) && (page != 1))) {
		dev_err(dev, "Input parameter of %s is invalid\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	if (dev->flags & MAX10_FLAGS_SECURE) {
		ret = max10_sys_update_bits(dev, FPGA_RECONF_REG,
			SFPGA_RP_LOAD, 0);
		if (ret < 0) {
			dev_err(dev,
				"Failed to update max10 reconfig register [e:%d]\n",
				ret);
			goto end;
		}
		ret = max10_sys_update_bits(dev, FPGA_RECONF_REG,
			SFPGA_RP_LOAD | SFPGA_RECONF_PAGE,
			SFPGA_RP_LOAD | SFPGA_PAGE(page));
		if (ret < 0) {
			dev_err(dev,
				"Failed to update max10 reconfig register [e:%d]\n",
				ret);
			goto end;
		}
	} else {
		ret = max10_sys_update_bits(dev, RSU_REG, FPGA_RP_LOAD, 0);
		if (ret < 0) {
			dev_err(dev,
				"Failed to update max10 rsu register [e:%d]\n",
				ret);
			goto end;
		}
		ret = max10_sys_update_bits(dev, RSU_REG,
			FPGA_RP_LOAD | FPGA_RECONF_PAGE,
			FPGA_RP_LOAD | FPGA_PAGE(page));
		if (ret < 0) {
			dev_err(dev,
				"Failed to update max10 rsu register [e:%d]\n",
				ret);
			goto end;
		}
	}

	ret = max10_sys_update_bits(dev, FPGA_RECONF_REG, COUNTDOWN_START, 0);
	if (ret < 0) {
		dev_err(dev,
			"Failed to update max10 reconfig register [e:%d]\n",
			ret);
		goto end;
	}

	ret = max10_sys_update_bits(dev, FPGA_RECONF_REG, COUNTDOWN_START,
		COUNTDOWN_START);
	if (ret < 0) {
		dev_err(dev,
			"Failed to update max10 reconfig register [e:%d]\n",
			ret);
	}
end:
	if (ret < 0)
		dev_err(dev, "Failed to reload FPGA\n");

	return ret;
}

static int n3000_reload_bmc(struct intel_max10_device *dev, int page)
{
	uint32_t val = 0;
	int ret = 0;

	dev_info(dev, "Reload BMC\n");

	if (!dev || ((page != 0) && (page != 1))) {
		dev_err(dev, "Input parameter of %s is invalid\n", __func__);
		ret = -EINVAL;
		goto end;
	}

	if (dev->flags & MAX10_FLAGS_SECURE) {
		ret = max10_sys_update_bits(dev, MAX10_DOORBELL,
			CONFIG_SEL | REBOOT_REQ,
			CONFIG_SEL_S(page) | REBOOT_REQ);
	} else {
		val = (page == 0) ? 0x1 : 0x3;
		ret = max10_reg_write(dev, IFPGA_DUAL_CFG_CTRL1, val);
		if (ret < 0) {
			dev_err(dev,
				"Failed to write to dual config1 register [e:%d]\n",
				ret);
			goto end;
		}

		ret = max10_reg_write(dev, IFPGA_DUAL_CFG_CTRL0, 0x1);
		if (ret < 0) {
			if (ret == -EIO) {
				ret = 0;
				goto end;
			}
			dev_err(dev,
				"Failed to write to dual config0 register [e:%d]\n",
				ret);
		}
	}

end:
	if (ret < 0)
		dev_err(dev, "Failed to reload BMC\n");

	return ret;
}

static int n3000_reload(struct ifpga_sec_mgr *smgr, int type, int page)
{
	int psel = 0;
	int ret = 0;

	if (!smgr || !smgr->max10_dev)
		return -ENODEV;

	if (type == IFPGA_BOOT_TYPE_FPGA) {
		psel = (page == IFPGA_BOOT_PAGE_FACTORY ? 0 : 1);
		ret = n3000_reload_fpga(smgr->max10_dev, psel);
	} else if (type == IFPGA_BOOT_TYPE_BMC) {
		psel = (page == IFPGA_BOOT_PAGE_FACTORY ? 1 : 0);
		ret = n3000_reload_bmc(smgr->max10_dev, psel);
	} else {
		ret = -EINVAL;
	}

	return ret;
}

static uint64_t n3000_get_hw_errinfo(struct ifpga_sec_mgr *smgr)
{
	struct intel_max10_device *dev = NULL;
	uint32_t doorbell = 0;
	uint32_t stat = 0;
	uint32_t prog = 0;
	uint32_t auth_result = 0;
	int ret = 0;

	if (!smgr || !smgr->max10_dev)
		return -ENODEV;
	dev = (struct intel_max10_device *)smgr->max10_dev;

	ret = max10_sys_read(dev, MAX10_DOORBELL, &doorbell);
	if (ret < 0) {
		dev_err(dev, "Failed to read max10 doorbell register [e:%d]\n",
			ret);
		return -1;
	}
	stat = SEC_STATUS_G(doorbell);
	prog = SEC_PROGRESS_G(doorbell);
	dev_debug(dev, "Current RSU status is %s, progress is %s\n",
		rsu_status_name(stat), rsu_progress_name(prog));

	ret = max10_sys_read(dev, MAX10_AUTH_RESULT, &auth_result);
	if (ret < 0) {
		dev_err(dev,
			"Failed to read authenticate result register [e:%d]\n",
			ret);
		return -1;
	}

	return (uint64_t)doorbell << 32 | (uint64_t)auth_result;
}

static const struct ifpga_sec_ops n3000_sec_ops = {
	.prepare = n3000_prepare,
	.write_blk = n3000_write_blk,
	.write_done = n3000_write_done,
	.check_complete = n3000_check_complete,
	.reload = n3000_reload,
	.cancel = n3000_cancel,
	.cleanup = NULL,
	.get_hw_errinfo = n3000_get_hw_errinfo,
};

int init_sec_mgr(struct ifpga_fme_hw *fme)
{
	struct ifpga_hw *hw = NULL;
	opae_share_data *sd = NULL;
	struct ifpga_sec_mgr *smgr = NULL;

	if (!fme || !fme->max10_dev)
		return -ENODEV;

	smgr = (struct ifpga_sec_mgr *)malloc(sizeof(*smgr));
	if (!smgr) {
		dev_err(NULL, "Failed to allocate memory for security manager\n");
		return -ENOMEM;
	}
	fme->sec_mgr = smgr;

	hw = (struct ifpga_hw *)fme->parent;
	if (hw && hw->adapter && hw->adapter->shm.ptr) {
		sd = (opae_share_data *)hw->adapter->shm.ptr;
		smgr->rsu_control = &sd->rsu_ctrl;
		smgr->rsu_status = &sd->rsu_stat;
	} else {
		smgr->rsu_control = NULL;
		smgr->rsu_status = NULL;
	}

	if (hw && (hw->pci_data->device_id == IFPGA_N3000_DID) &&
		(hw->pci_data->vendor_id == IFPGA_N3000_VID)) {
		smgr->ops = &n3000_sec_ops;
		smgr->copy_speed = IFPGA_N3000_COPY_SPEED;
	} else {
		dev_err(NULL, "No operation for security manager\n");
		smgr->ops = NULL;
	}

	smgr->fme = fme;
	smgr->max10_dev = fme->max10_dev;

	return 0;
}

void release_sec_mgr(struct ifpga_fme_hw *fme)
{
	struct ifpga_sec_mgr *smgr = NULL;

	if (fme) {
		smgr = (struct ifpga_sec_mgr *)fme->sec_mgr;
		if (smgr) {
			fme->sec_mgr = NULL;
			free(smgr);
		}
	}
}
