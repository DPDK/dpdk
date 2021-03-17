/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */

#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "ifpga_sec_mgr.h"

static struct ifpga_sec_mgr *sec_mgr;

static void set_rsu_control(struct ifpga_sec_mgr *smgr, uint32_t ctrl)
{
	if (smgr && smgr->rsu_control)
		*smgr->rsu_control = ctrl;
}

static uint32_t get_rsu_control(struct ifpga_sec_mgr *smgr)
{
	if (smgr && smgr->rsu_control)
		return *smgr->rsu_control;
	return 0;
}

static void set_rsu_status(struct ifpga_sec_mgr *smgr, uint32_t status,
	uint32_t progress)
{
	if (smgr && smgr->rsu_status)
		*smgr->rsu_status = IFPGA_RSU_STATUS(status, progress);
}

static void get_rsu_status(struct ifpga_sec_mgr *smgr, uint32_t *status,
	uint32_t *progress)
{
	if (smgr && smgr->rsu_status) {
		if (status)
			*status = IFPGA_RSU_GET_STAT(*smgr->rsu_status);
		if (progress)
			*progress = IFPGA_RSU_GET_PROG(*smgr->rsu_status);
	}
}

static void sig_handler(int sig, siginfo_t *info, void *data)
{
	(void)(info);
	(void)(data);

	switch (sig) {
	case SIGINT:
		if (sec_mgr) {
			dev_info(sec_mgr, "Interrupt secure flash update"
				" by keyboard\n");
			set_rsu_control(sec_mgr, IFPGA_RSU_ABORT);
		}
		break;
	default:
		break;
	}
}

static void log_time(time_t t, const char *msg)
{
	uint32_t h = 0;
	uint32_t m = 0;
	uint32_t s = 0;

	if (t < 60) {
		s = (uint32_t)t;
	} else if (t < 3600) {
		s = (uint32_t)(t % 60);
		m = (uint32_t)(t / 60);
	} else {
		s = (uint32_t)(t % 60);
		m = (uint32_t)((t % 3600) / 60);
		h = (uint32_t)(t / 3600);
	}
	printf("%s - %02u:%02u:%02u\n", msg, h, m, s);
}

static int start_flash_update(struct ifpga_sec_mgr *smgr)
{
	if (!smgr)
		return -ENODEV;

	if (!smgr->ops || !smgr->ops->prepare)
		return -EINVAL;

	return smgr->ops->prepare(smgr);
}

static int write_flash_image(struct ifpga_sec_mgr *smgr, const char *image,
	uint32_t offset)
{
	void *buf = NULL;
	int retry = 0;
	uint32_t length = 0;
	uint32_t to_transfer = 0;
	uint32_t one_percent = 0;
	uint32_t prog = 0;
	uint32_t old_prog = -1;
	ssize_t read_size = 0;
	int fd = -1;
	int ret = 0;

	if (!smgr)
		return -ENODEV;

	if (!smgr->ops || !smgr->ops->write_blk)
		return -EINVAL;

	fd = open(image, O_RDONLY);
	if (fd < 0) {
		dev_err(smgr,
			"Failed to open \'%s\' for RD [e:%s]\n",
			image, strerror(errno));
		return -EIO;
	}

	buf = malloc(IFPGA_RSU_DATA_BLK_SIZE);
	if (!buf) {
		dev_err(smgr, "Failed to allocate memory for flash update\n");
		close(fd);
		return -ENOMEM;
	}

	length = smgr->rsu_length;
	one_percent = length / 100;
	do {
		to_transfer = (length > IFPGA_RSU_DATA_BLK_SIZE) ?
			IFPGA_RSU_DATA_BLK_SIZE : length;
		if (lseek(fd, offset, SEEK_SET) < 0) {
			dev_err(smgr, "Failed to seek in \'%s\' [e:%s]\n",
				image, strerror(errno));
			ret = -EIO;
			goto end;
		}
		read_size = read(fd, buf, to_transfer);
		if (read_size < 0) {
			dev_err(smgr, "Failed to read from \'%s\' [e:%s]\n",
				image, strerror(errno));
			ret = -EIO;
			goto end;
		}
		if ((uint32_t)read_size != to_transfer) {
			dev_err(smgr,
				"Read length %zd is not expected [e:%u]\n",
				read_size, to_transfer);
			ret = -EIO;
			goto end;
		}

		retry = 0;
		do {
			if (get_rsu_control(smgr) == IFPGA_RSU_ABORT) {
				ret = -EAGAIN;
				goto end;
			}
			ret = smgr->ops->write_blk(smgr, buf, offset,
				to_transfer);
			if (ret == 0)
				break;
			sleep(1);
		} while (++retry <= IFPGA_RSU_WRITE_RETRY);
		if (retry > IFPGA_RSU_WRITE_RETRY) {
			dev_err(smgr, "Failed to write to staging area 0x%x\n",
				offset);
			ret = -EAGAIN;
			goto end;
		}

		length -= to_transfer;
		offset += to_transfer;
		prog = offset / one_percent;
		if (prog != old_prog) {
			printf("\r%d%%", prog);
			fflush(stdout);
			set_rsu_status(smgr, IFPGA_RSU_READY, prog);
			old_prog = prog;
		}
	} while (length > 0);
	set_rsu_status(smgr, IFPGA_RSU_READY, 100);
	printf("\n");

end:
	free(buf);
	close(fd);
	return ret;
}

static int apply_flash_update(struct ifpga_sec_mgr *smgr)
{
	uint32_t one_percent = 0;
	uint32_t one_percent_time = 0;
	uint32_t prog = 0;
	uint32_t old_prog = -1;
	uint32_t copy_time = 0;
	int ret = 0;

	if (!smgr)
		return -ENODEV;

	if (!smgr->ops || !smgr->ops->write_done || !smgr->ops->check_complete)
		return -EINVAL;

	if (smgr->ops->write_done(smgr) < 0) {
		dev_err(smgr, "Failed to apply flash update\n");
		return -EAGAIN;
	}

	one_percent = (smgr->rsu_length + 99) / 100;
	if (smgr->copy_speed == 0)   /* avoid zero divide fault */
		smgr->copy_speed = 1;
	one_percent_time = (one_percent + smgr->copy_speed - 1) /
		smgr->copy_speed;
	if (one_percent_time == 0)   /* avoid zero divide fault */
		one_percent_time = 1;

	do {
		ret = smgr->ops->check_complete(smgr);
		if (ret != -EAGAIN)
			break;
		sleep(1);
		copy_time += 1;
		prog = copy_time / one_percent_time;
		if (prog >= 100)
			prog = 99;
		if (prog != old_prog) {
			printf("\r%d%%", prog);
			fflush(stdout);
			set_rsu_status(smgr, IFPGA_RSU_COPYING, prog);
			old_prog = prog;
		}
	} while (true);

	if (ret < 0) {
		printf("\n");
		dev_err(smgr, "Failed to complete secure flash update\n");
	} else {
		printf("\r100%%\n");
		set_rsu_status(smgr, IFPGA_RSU_COPYING, 100);
	}

	return ret;
}

static int secure_update_cancel(struct ifpga_sec_mgr *smgr)
{
	if (!smgr)
		return -ENODEV;

	if (!smgr->ops || !smgr->ops->cancel)
		return -EINVAL;

	return smgr->ops->cancel(smgr);
}

static int secure_update_status(struct ifpga_sec_mgr *smgr, uint64_t *status)
{
	if (!smgr)
		return -ENODEV;

	if (!smgr->ops || !smgr->ops->get_hw_errinfo)
		return -EINVAL;

	if (status)
		*status = smgr->ops->get_hw_errinfo(smgr);

	return 0;
}

int fpga_update_flash(struct ifpga_fme_hw *fme, const char *image,
	uint64_t *status)
{
	struct ifpga_hw *hw = NULL;
	struct ifpga_sec_mgr *smgr = NULL;
	uint32_t rsu_stat = 0;
	int fd = -1;
	off_t len = 0;
	struct sigaction old_sigint_action;
	struct sigaction sa;
	time_t start;
	int ret = 0;

	if (!fme || !image || !status) {
		dev_err(fme, "Input parameter of %s is invalid\n", __func__);
		return -EINVAL;
	}

	hw = (struct ifpga_hw *)fme->parent;
	if (!hw) {
		dev_err(fme, "Parent of FME not found\n");
		return -ENODEV;
	}

	smgr = (struct ifpga_sec_mgr *)fme->sec_mgr;
	if (!smgr || !smgr->max10_dev) {
		dev_err(smgr, "Security manager not initialized\n");
		return -ENODEV;
	}

	opae_adapter_lock(hw->adapter, -1);
	get_rsu_status(smgr, &rsu_stat, NULL);
	if (rsu_stat != IFPGA_RSU_IDLE) {
		opae_adapter_unlock(hw->adapter);
		if (rsu_stat == IFPGA_RSU_REBOOT)
			dev_info(smgr, "Reboot is in progress\n");
		else
			dev_info(smgr, "Update is in progress\n");
		return -EAGAIN;
	}
	set_rsu_control(smgr, 0);
	set_rsu_status(smgr, IFPGA_RSU_PREPARE, 0);
	opae_adapter_unlock(hw->adapter);

	fd = open(image, O_RDONLY);
	if (fd < 0) {
		dev_err(smgr,
			"Failed to open \'%s\' for RD [e:%s]\n",
			image, strerror(errno));
		return -EIO;
	}
	len = lseek(fd, 0, SEEK_END);
	close(fd);

	if (len < 0) {
		dev_err(smgr,
			"Failed to get file length of \'%s\' [e:%s]\n",
			image, strerror(errno));
		return -EIO;
	}
	if (len == 0) {
		dev_err(smgr, "Length of file \'%s\' is invalid\n", image);
		return -EINVAL;
	}
	smgr->rsu_length = len;

	if (smgr->max10_dev->staging_area_size < smgr->rsu_length) {
		dev_err(dev, "Size of staging area is small than image length "
			"[%u<%u]\n", smgr->max10_dev->staging_area_size,
			smgr->rsu_length);
		return -EINVAL;
	}

	printf("Updating from file \'%s\' with size %u\n",
		image, smgr->rsu_length);

	sec_mgr = smgr;
	memset(&sa, 0, sizeof(struct sigaction));
	sa.sa_flags = SA_SIGINFO | SA_RESETHAND;
	sa.sa_sigaction = sig_handler;
	ret = sigaction(SIGINT, &sa, &old_sigint_action);
	if (ret < 0) {
		dev_warn(dev, "Failed to register signal handler"
			" [e:%d]\n", ret);
		sec_mgr = NULL;
	}

	start = time(NULL);
	log_time(time(NULL) - start, "Starting secure flash update");
	ret = start_flash_update(smgr);
	if (ret < 0)
		goto end;

	set_rsu_status(smgr, IFPGA_RSU_READY, 0);
	log_time(time(NULL) - start, "Writing to staging area");
	ret = write_flash_image(smgr, image, 0);
	if (ret < 0)
		goto end;

	set_rsu_status(smgr, IFPGA_RSU_COPYING, 0);
	log_time(time(NULL) - start, "Applying secure flash update");
	ret = apply_flash_update(smgr);

end:
	if (sec_mgr) {
		sec_mgr = NULL;
		if (sigaction(SIGINT, &old_sigint_action, NULL) < 0)
			dev_err(smgr, "Failed to unregister signal handler\n");
	}

	secure_update_status(smgr, status);
	if (ret < 0) {
		log_time(time(NULL) - start, "Secure flash update ERROR");
		if (ret == -EAGAIN)
			secure_update_cancel(smgr);
	} else {
		log_time(time(NULL) - start, "Secure flash update OK");
	}
	set_rsu_status(smgr, IFPGA_RSU_IDLE, 0);

	return ret;
}

int fpga_stop_flash_update(struct ifpga_fme_hw *fme, int force)
{
	struct ifpga_sec_mgr *smgr = NULL;
	uint32_t status = 0;
	int retry = IFPGA_RSU_CANCEL_RETRY;
	int ret = 0;

	if (!fme) {
		dev_err(fme, "Input parameter of %s is invalid\n", __func__);
		return -EINVAL;
	}
	smgr = (struct ifpga_sec_mgr *)fme->sec_mgr;

	get_rsu_status(smgr, &status, NULL);
	if (status != IFPGA_RSU_IDLE) {
		dev_info(smgr, "Cancel secure flash update\n");
		set_rsu_control(smgr, IFPGA_RSU_ABORT);
	}

	if (force) {
		sleep(2);
		do {
			get_rsu_status(smgr, &status, NULL);
			if (status == IFPGA_RSU_IDLE)
				break;
			if (secure_update_cancel(smgr) == 0)
				set_rsu_status(smgr, IFPGA_RSU_IDLE, 0);
			sleep(1);
		} while (--retry > 0);
		if (retry <= 0) {
			dev_err(smgr, "Failed to stop flash update\n");
			ret = -EAGAIN;
		}
	}

	return ret;
}

int fpga_reload(struct ifpga_fme_hw *fme, int type, int page)
{
	struct ifpga_sec_mgr *smgr = NULL;

	if (!fme) {
		dev_err(fme, "Input parameter of %s is invalid\n", __func__);
		return -EINVAL;
	}
	smgr = (struct ifpga_sec_mgr *)fme->sec_mgr;

	if (!smgr || !smgr->ops || !smgr->ops->reload)
		return -EINVAL;

	return smgr->ops->reload(smgr, type, page);
}
