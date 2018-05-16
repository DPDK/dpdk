/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include "ifpga_feature_dev.h"

static u64
pr_err_handle(struct feature_fme_pr *fme_pr)
{
	struct feature_fme_pr_status fme_pr_status;
	unsigned long err_code;
	u64 fme_pr_error;
	int i;

	fme_pr_status.csr = readq(&fme_pr->ccip_fme_pr_status);
	if (!fme_pr_status.pr_status)
		return 0;

	err_code = readq(&fme_pr->ccip_fme_pr_err);
	fme_pr_error = err_code;

	for (i = 0; i < PR_MAX_ERR_NUM; i++) {
		if (err_code & (1 << i))
			dev_info(NULL, "%s\n", pr_err_msg[i]);
	}

	writeq(fme_pr_error, &fme_pr->ccip_fme_pr_err);
	return fme_pr_error;
}

static int fme_pr_write_init(struct ifpga_fme_hw *fme_dev,
			     struct fpga_pr_info *info)
{
	struct feature_fme_pr *fme_pr;
	struct feature_fme_pr_ctl fme_pr_ctl;
	struct feature_fme_pr_status fme_pr_status;

	fme_pr = get_fme_feature_ioaddr_by_index(fme_dev,
						 FME_FEATURE_ID_PR_MGMT);
	if (!fme_pr)
		return -EINVAL;

	if (info->flags != FPGA_MGR_PARTIAL_RECONFIG)
		return -EINVAL;

	dev_info(fme_dev, "resetting PR before initiated PR\n");

	fme_pr_ctl.csr = readq(&fme_pr->ccip_fme_pr_control);
	fme_pr_ctl.pr_reset = 1;
	writeq(fme_pr_ctl.csr, &fme_pr->ccip_fme_pr_control);

	fme_pr_ctl.pr_reset_ack = 1;

	if (fpga_wait_register_field(pr_reset_ack, fme_pr_ctl,
				     &fme_pr->ccip_fme_pr_control,
				     PR_WAIT_TIMEOUT, 1)) {
		dev_err(fme_dev, "maximum PR timeout\n");
		return -ETIMEDOUT;
	}

	fme_pr_ctl.csr = readq(&fme_pr->ccip_fme_pr_control);
	fme_pr_ctl.pr_reset = 0;
	writeq(fme_pr_ctl.csr, &fme_pr->ccip_fme_pr_control);

	dev_info(fme_dev, "waiting for PR resource in HW to be initialized and ready\n");

	fme_pr_status.pr_host_status = PR_HOST_STATUS_IDLE;

	if (fpga_wait_register_field(pr_host_status, fme_pr_status,
				     &fme_pr->ccip_fme_pr_status,
				     PR_WAIT_TIMEOUT, 1)) {
		dev_err(fme_dev, "maximum PR timeout\n");
		return -ETIMEDOUT;
	}

	dev_info(fme_dev, "check if have any previous PR error\n");
	pr_err_handle(fme_pr);
	return 0;
}

static int fme_pr_write(struct ifpga_fme_hw *fme_dev,
			int port_id, const char *buf, size_t count,
			struct fpga_pr_info *info)
{
	struct feature_fme_pr *fme_pr;
	struct feature_fme_pr_ctl fme_pr_ctl;
	struct feature_fme_pr_status fme_pr_status;
	struct feature_fme_pr_data fme_pr_data;
	int delay, pr_credit;
	int ret = 0;

	fme_pr = get_fme_feature_ioaddr_by_index(fme_dev,
						 FME_FEATURE_ID_PR_MGMT);
	if (!fme_pr)
		return -EINVAL;

	dev_info(fme_dev, "set PR port ID and start request\n");

	fme_pr_ctl.csr = readq(&fme_pr->ccip_fme_pr_control);
	fme_pr_ctl.pr_regionid = port_id;
	fme_pr_ctl.pr_start_req = 1;
	writeq(fme_pr_ctl.csr, &fme_pr->ccip_fme_pr_control);

	dev_info(fme_dev, "pushing data from bitstream to HW\n");

	fme_pr_status.csr = readq(&fme_pr->ccip_fme_pr_status);
	pr_credit = fme_pr_status.pr_credit;

	while (count > 0) {
		delay = 0;
		while (pr_credit <= 1) {
			if (delay++ > PR_WAIT_TIMEOUT) {
				dev_err(fme_dev, "maximum try\n");

				info->pr_err = pr_err_handle(fme_pr);
				return info->pr_err ? -EIO : -ETIMEDOUT;
			}
			udelay(1);

			fme_pr_status.csr = readq(&fme_pr->ccip_fme_pr_status);
			pr_credit = fme_pr_status.pr_credit;
		};

		if (count >= fme_dev->pr_bandwidth) {
			switch (fme_dev->pr_bandwidth) {
			case 4:
				fme_pr_data.rsvd = 0;
				fme_pr_data.pr_data_raw = *((const u32 *)buf);
				writeq(fme_pr_data.csr,
				       &fme_pr->ccip_fme_pr_data);
				break;
			default:
				ret = -EFAULT;
				goto done;
			}

			buf += fme_dev->pr_bandwidth;
			count -= fme_dev->pr_bandwidth;
			pr_credit--;
		} else {
			WARN_ON(1);
			ret = -EINVAL;
			goto done;
		}
	}

done:
	return ret;
}

static int fme_pr_write_complete(struct ifpga_fme_hw *fme_dev,
				 struct fpga_pr_info *info)
{
	struct feature_fme_pr *fme_pr;
	struct feature_fme_pr_ctl fme_pr_ctl;

	fme_pr = get_fme_feature_ioaddr_by_index(fme_dev,
						 FME_FEATURE_ID_PR_MGMT);

	fme_pr_ctl.csr = readq(&fme_pr->ccip_fme_pr_control);
	fme_pr_ctl.pr_push_complete = 1;
	writeq(fme_pr_ctl.csr, &fme_pr->ccip_fme_pr_control);

	dev_info(fme_dev, "green bitstream push complete\n");
	dev_info(fme_dev, "waiting for HW to release PR resource\n");

	fme_pr_ctl.pr_start_req = 0;

	if (fpga_wait_register_field(pr_start_req, fme_pr_ctl,
				     &fme_pr->ccip_fme_pr_control,
				     PR_WAIT_TIMEOUT, 1)) {
		printf("maximum try.\n");
		return -ETIMEDOUT;
	}

	dev_info(fme_dev, "PR operation complete, checking status\n");
	info->pr_err = pr_err_handle(fme_pr);
	if (info->pr_err)
		return -EIO;

	dev_info(fme_dev, "PR done successfully\n");
	return 0;
}

static int fpga_pr_buf_load(struct ifpga_fme_hw *fme_dev,
			    struct fpga_pr_info *info, const char *buf,
			    size_t count)
{
	int ret;

	info->state = FPGA_PR_STATE_WRITE_INIT;
	ret = fme_pr_write_init(fme_dev, info);
	if (ret) {
		dev_err(fme_dev, "Error preparing FPGA for writing\n");
		info->state = FPGA_PR_STATE_WRITE_INIT_ERR;
		return ret;
	}

	/*
	 * Write the FPGA image to the FPGA.
	 */
	info->state = FPGA_PR_STATE_WRITE;
	ret = fme_pr_write(fme_dev, info->port_id, buf, count, info);
	if (ret) {
		dev_err(fme_dev, "Error while writing image data to FPGA\n");
		info->state = FPGA_PR_STATE_WRITE_ERR;
		return ret;
	}

	/*
	 * After all the FPGA image has been written, do the device specific
	 * steps to finish and set the FPGA into operating mode.
	 */
	info->state = FPGA_PR_STATE_WRITE_COMPLETE;
	ret = fme_pr_write_complete(fme_dev, info);
	if (ret) {
		dev_err(fme_dev, "Error after writing image data to FPGA\n");
		info->state = FPGA_PR_STATE_WRITE_COMPLETE_ERR;
		return ret;
	}
	info->state = FPGA_PR_STATE_DONE;

	return 0;
}

static int fme_pr(struct ifpga_hw *hw, u32 port_id, void *buffer, u32 size,
		  u64 *status)
{
	struct feature_fme_header *fme_hdr;
	struct feature_fme_capability fme_capability;
	struct ifpga_fme_hw *fme = &hw->fme;
	struct fpga_pr_info info;
	struct ifpga_port_hw *port;
	int ret = 0;

	if (!buffer || size == 0)
		return -EINVAL;
	if (fme->state != IFPGA_FME_IMPLEMENTED)
		return -EINVAL;

	/*
	 * Padding extra zeros to align PR buffer with PR bandwidth, HW will
	 * ignore these zeros automatically.
	 */
	size = IFPGA_ALIGN(size, fme->pr_bandwidth);

	/* get fme header region */
	fme_hdr = get_fme_feature_ioaddr_by_index(fme,
						  FME_FEATURE_ID_HEADER);
	if (!fme_hdr)
		return -EINVAL;

	/* check port id */
	fme_capability.csr = readq(&fme_hdr->capability);
	if (port_id >= fme_capability.num_ports) {
		dev_err(fme,  "port number more than maximum\n");
		return -EINVAL;
	}

	memset(&info, 0, sizeof(struct fpga_pr_info));
	info.flags = FPGA_MGR_PARTIAL_RECONFIG;
	info.port_id = port_id;

	spinlock_lock(&fme->lock);

	/* get port device by port_id */
	port = &hw->port[port_id];

	/* Disable Port before PR */
	fpga_port_disable(port);

	ret = fpga_pr_buf_load(fme, &info, (void *)buffer, size);

	*status = info.pr_err;

	/* Re-enable Port after PR finished */
	fpga_port_enable(port);
	spinlock_unlock(&fme->lock);

	return ret;
}

int do_pr(struct ifpga_hw *hw, u32 port_id, void *buffer, u32 size, u64 *status)
{
	struct bts_header *bts_hdr;
	void *buf;
	struct ifpga_port_hw *port;
	int ret;

	if (!buffer || size == 0) {
		dev_err(hw, "invalid parameter\n");
		return -EINVAL;
	}

	bts_hdr = (struct bts_header *)buffer;

	if (is_valid_bts(bts_hdr)) {
		dev_info(hw, "this is a valid bitsteam..\n");
		size -= (sizeof(struct bts_header) +
				     bts_hdr->metadata_len);
		buf = (u8 *)buffer + sizeof(struct bts_header) +
			       bts_hdr->metadata_len;
	} else {
		return -EINVAL;
	}

	/* clean port error before do PR */
	port = &hw->port[port_id];
	ret = port_clear_error(port);
	if (ret) {
		dev_err(hw, "port cannot clear error\n");
		return -EINVAL;
	}

	return fme_pr(hw, port_id, buf, size, status);
}

static int fme_pr_mgmt_init(struct feature *feature)
{
	struct feature_fme_pr *fme_pr;
	struct feature_header fme_pr_header;
	struct ifpga_fme_hw *fme;

	dev_info(NULL, "FME PR MGMT Init.\n");

	fme = (struct ifpga_fme_hw *)feature->parent;

	fme_pr = (struct feature_fme_pr *)feature->addr;

	fme_pr_header.csr = readq(&fme_pr->header);
	if (fme_pr_header.revision == 2) {
		dev_info(NULL, "using 512-bit PR\n");
		fme->pr_bandwidth = 64;
	} else {
		dev_info(NULL, "using 32-bit PR\n");
		fme->pr_bandwidth = 4;
	}

	return 0;
}

static void fme_pr_mgmt_uinit(struct feature *feature)
{
	UNUSED(feature);

	dev_info(NULL, "FME PR MGMT UInit.\n");
}

struct feature_ops fme_pr_mgmt_ops = {
	.init = fme_pr_mgmt_init,
	.uinit = fme_pr_mgmt_uinit,
};
