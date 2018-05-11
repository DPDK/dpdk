/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <sys/ioctl.h>

#include "ifpga_feature_dev.h"

/*
 * Enable Port by clear the port soft reset bit, which is set by default.
 * The AFU is unable to respond to any MMIO access while in reset.
 * __fpga_port_enable function should only be used after __fpga_port_disable
 * function.
 */
void __fpga_port_enable(struct ifpga_port_hw *port)
{
	struct feature_port_header *port_hdr;
	struct feature_port_control control;

	WARN_ON(!port->disable_count);

	if (--port->disable_count != 0)
		return;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);
	WARN_ON(!port_hdr);

	control.csr = readq(&port_hdr->control);
	control.port_sftrst = 0x0;
	writeq(control.csr, &port_hdr->control);
}

int __fpga_port_disable(struct ifpga_port_hw *port)
{
	struct feature_port_header *port_hdr;
	struct feature_port_control control;

	if (port->disable_count++ != 0)
		return 0;

	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);
	WARN_ON(!port_hdr);

	/* Set port soft reset */
	control.csr = readq(&port_hdr->control);
	control.port_sftrst = 0x1;
	writeq(control.csr, &port_hdr->control);

	/*
	 * HW sets ack bit to 1 when all outstanding requests have been drained
	 * on this port and minimum soft reset pulse width has elapsed.
	 * Driver polls port_soft_reset_ack to determine if reset done by HW.
	 */
	control.port_sftrst_ack = 1;

	if (fpga_wait_register_field(port_sftrst_ack, control,
				     &port_hdr->control, RST_POLL_TIMEOUT,
				     RST_POLL_INVL)) {
		dev_err(port, "timeout, fail to reset device\n");
		return -ETIMEDOUT;
	}

	return 0;
}

int fpga_get_afu_uuid(struct ifpga_port_hw *port, struct uuid *uuid)
{
	struct feature_port_header *port_hdr;
	u64 guidl, guidh;

	port_hdr = get_port_feature_ioaddr_by_index(port, PORT_FEATURE_ID_UAFU);

	spinlock_lock(&port->lock);
	guidl = readq(&port_hdr->afu_header.guid.b[0]);
	guidh = readq(&port_hdr->afu_header.guid.b[8]);
	spinlock_unlock(&port->lock);

	memcpy(uuid->b, &guidl, sizeof(u64));
	memcpy(uuid->b + 8, &guidh, sizeof(u64));

	return 0;
}

/* Mask / Unmask Port Errors by the Error Mask register. */
void port_err_mask(struct ifpga_port_hw *port, bool mask)
{
	struct feature_port_error *port_err;
	struct feature_port_err_key err_mask;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);

	if (mask)
		err_mask.csr = PORT_ERR_MASK;
	else
		err_mask.csr = 0;

	writeq(err_mask.csr, &port_err->error_mask);
}

/* Clear All Port Errors. */
int port_err_clear(struct ifpga_port_hw *port, u64 err)
{
	struct feature_port_header *port_hdr;
	struct feature_port_error *port_err;
	struct feature_port_err_key mask;
	struct feature_port_first_err_key first;
	struct feature_port_status status;
	int ret = 0;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);
	port_hdr = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_HEADER);

	/*
	 * Clear All Port Errors
	 *
	 * - Check for AP6 State
	 * - Halt Port by keeping Port in reset
	 * - Set PORT Error mask to all 1 to mask errors
	 * - Clear all errors
	 * - Set Port mask to all 0 to enable errors
	 * - All errors start capturing new errors
	 * - Enable Port by pulling the port out of reset
	 */

	/* If device is still in AP6 state, can not clear any error.*/
	status.csr = readq(&port_hdr->status);
	if (status.power_state == PORT_POWER_STATE_AP6) {
		dev_err(dev, "Could not clear errors, device in AP6 state.\n");
		return -EBUSY;
	}

	/* Halt Port by keeping Port in reset */
	ret = __fpga_port_disable(port);
	if (ret)
		return ret;

	/* Mask all errors */
	port_err_mask(port, true);

	/* Clear errors if err input matches with current port errors.*/
	mask.csr = readq(&port_err->port_error);

	if (mask.csr == err) {
		writeq(mask.csr, &port_err->port_error);

		first.csr = readq(&port_err->port_first_error);
		writeq(first.csr, &port_err->port_first_error);
	} else {
		ret = -EBUSY;
	}

	/* Clear mask */
	port_err_mask(port, false);

	/* Enable the Port by clear the reset */
	__fpga_port_enable(port);

	return ret;
}

int port_clear_error(struct ifpga_port_hw *port)
{
	struct feature_port_error *port_err;
	struct feature_port_err_key error;

	port_err = get_port_feature_ioaddr_by_index(port,
						    PORT_FEATURE_ID_ERROR);
	error.csr = readq(&port_err->port_error);

	dev_info(port, "read port error: 0x%lx\n", (unsigned long)error.csr);

	return port_err_clear(port, error.csr);
}

void fme_hw_uinit(struct ifpga_fme_hw *fme)
{
	struct feature *feature;
	int i;

	if (fme->state != IFPGA_FME_IMPLEMENTED)
		return;

	for (i = 0; i < FME_FEATURE_ID_MAX; i++) {
		feature = &fme->sub_feature[i];
		if (feature->state == IFPGA_FEATURE_ATTACHED &&
		    feature->ops && feature->ops->uinit)
			feature->ops->uinit(feature);
	}
}

int fme_hw_init(struct ifpga_fme_hw *fme)
{
	struct feature *feature;
	int i, ret;

	if (fme->state != IFPGA_FME_IMPLEMENTED)
		return -EINVAL;

	for (i = 0; i < FME_FEATURE_ID_MAX; i++) {
		feature = &fme->sub_feature[i];
		if (feature->state == IFPGA_FEATURE_ATTACHED &&
		    feature->ops && feature->ops->init) {
			ret = feature->ops->init(feature);
			if (ret) {
				fme_hw_uinit(fme);
				return ret;
			}
		}
	}

	return 0;
}

void port_hw_uinit(struct ifpga_port_hw *port)
{
	struct feature *feature;
	int i;

	for (i = 0; i < PORT_FEATURE_ID_MAX; i++) {
		feature = &port->sub_feature[i];
		if (feature->state == IFPGA_FEATURE_ATTACHED &&
		    feature->ops && feature->ops->uinit)
			feature->ops->uinit(feature);
	}
}

int port_hw_init(struct ifpga_port_hw *port)
{
	struct feature *feature;
	int i, ret;

	if (port->state == IFPGA_PORT_UNUSED)
		return 0;

	for (i = 0; i < PORT_FEATURE_ID_MAX; i++) {
		feature = &port->sub_feature[i];
		if (feature->ops && feature->ops->init) {
			ret = feature->ops->init(feature);
			if (ret) {
				port_hw_uinit(port);
				return ret;
			}
		}
	}

	return 0;
}

