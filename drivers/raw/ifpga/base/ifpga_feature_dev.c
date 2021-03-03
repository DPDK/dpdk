/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2018 Intel Corporation
 */

#include <sys/ioctl.h>
#include <rte_vfio.h>

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

	if (!uuid)
		return -EINVAL;

	port_hdr = get_port_feature_ioaddr_by_index(port, PORT_FEATURE_ID_UAFU);

	spinlock_lock(&port->lock);
	guidl = readq(&port_hdr->afu_header.guid.b[0]);
	guidh = readq(&port_hdr->afu_header.guid.b[8]);
	spinlock_unlock(&port->lock);

	opae_memcpy(uuid->b, &guidl, sizeof(u64));
	opae_memcpy(uuid->b + 8, &guidh, sizeof(u64));

	return 0;
}

int fpga_get_pr_uuid(struct ifpga_fme_hw *fme, struct uuid *uuid)
{
	struct feature_fme_pr *fme_pr;
	u64 guidl, guidh;

	if (!fme || !uuid)
		return -EINVAL;

	fme_pr = get_fme_feature_ioaddr_by_index(fme, FME_FEATURE_ID_PR_MGMT);

	spinlock_lock(&fme->lock);
	guidl = readq(&fme_pr->fme_pr_intfc_id_l);
	guidh = readq(&fme_pr->fme_pr_intfc_id_h);
	spinlock_unlock(&fme->lock);

	opae_memcpy(uuid->b, &guidl, sizeof(u64));
	opae_memcpy(uuid->b + 8, &guidh, sizeof(u64));

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

static struct feature_driver fme_feature_drvs[] = {
	{FEATURE_DRV(FME_FEATURE_ID_HEADER, FME_FEATURE_HEADER,
			&fme_hdr_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_THERMAL_MGMT, FME_FEATURE_THERMAL_MGMT,
			&fme_thermal_mgmt_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_POWER_MGMT, FME_FEATURE_POWER_MGMT,
			&fme_power_mgmt_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_GLOBAL_ERR, FME_FEATURE_GLOBAL_ERR,
			&fme_global_err_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_PR_MGMT, FME_FEATURE_PR_MGMT,
			&fme_pr_mgmt_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_GLOBAL_DPERF, FME_FEATURE_GLOBAL_DPERF,
			&fme_global_dperf_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_HSSI_ETH, FME_FEATURE_HSSI_ETH,
	&fme_hssi_eth_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_EMIF_MGMT, FME_FEATURE_EMIF_MGMT,
	&fme_emif_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_MAX10_SPI, FME_FEATURE_MAX10_SPI,
	&fme_spi_master_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_NIOS_SPI, FME_FEATURE_NIOS_SPI,
	&fme_nios_spi_master_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_I2C_MASTER, FME_FEATURE_I2C_MASTER,
	&fme_i2c_master_ops),},
	{FEATURE_DRV(FME_FEATURE_ID_ETH_GROUP, FME_FEATURE_ETH_GROUP,
	&fme_eth_group_ops),},
	{0, NULL, NULL}, /* end of arrary */
};

static struct feature_driver port_feature_drvs[] = {
	{FEATURE_DRV(PORT_FEATURE_ID_HEADER, PORT_FEATURE_HEADER,
			&ifpga_rawdev_port_hdr_ops)},
	{FEATURE_DRV(PORT_FEATURE_ID_ERROR, PORT_FEATURE_ERR,
			&ifpga_rawdev_port_error_ops)},
	{FEATURE_DRV(PORT_FEATURE_ID_UINT, PORT_FEATURE_UINT,
			&ifpga_rawdev_port_uint_ops)},
	{FEATURE_DRV(PORT_FEATURE_ID_STP, PORT_FEATURE_STP,
			&ifpga_rawdev_port_stp_ops)},
	{FEATURE_DRV(PORT_FEATURE_ID_UAFU, PORT_FEATURE_UAFU,
			&ifpga_rawdev_port_afu_ops)},
	{0, NULL, NULL}, /* end of array */
};

const char *get_fme_feature_name(unsigned int id)
{
	struct feature_driver *drv = fme_feature_drvs;

	while (drv->name) {
		if (drv->id == id)
			return drv->name;

		drv++;
	}

	return NULL;
}

const char *get_port_feature_name(unsigned int id)
{
	struct feature_driver *drv = port_feature_drvs;

	while (drv->name) {
		if (drv->id == id)
			return drv->name;

		drv++;
	}

	return NULL;
}

static void feature_uinit(struct ifpga_feature_list *list)
{
	struct ifpga_feature *feature;

	TAILQ_FOREACH(feature, list, next) {
		if (feature->state != IFPGA_FEATURE_ATTACHED)
			continue;
		if (feature->ops && feature->ops->uinit)
			feature->ops->uinit(feature);
	}
}

static int feature_init(struct feature_driver *drv,
		struct ifpga_feature_list *list)
{
	struct ifpga_feature *feature;
	int ret;

	while (drv->ops) {
		TAILQ_FOREACH(feature, list, next) {
			if (feature->state != IFPGA_FEATURE_ATTACHED)
				continue;
			if (feature->id == drv->id) {
				feature->ops = drv->ops;
				feature->name = drv->name;
				if (feature->ops->init) {
					ret = feature->ops->init(feature);
					if (ret)
						goto error;
				}
			}
		}
		drv++;
	}

	return 0;
error:
	feature_uinit(list);
	return ret;
}

int fme_hw_init(struct ifpga_fme_hw *fme)
{
	int ret;

	if (fme->state != IFPGA_FME_IMPLEMENTED)
		return -ENODEV;

	ret = feature_init(fme_feature_drvs, &fme->feature_list);
	if (ret)
		return ret;

	return 0;
}

void fme_hw_uinit(struct ifpga_fme_hw *fme)
{
	feature_uinit(&fme->feature_list);
}

void port_hw_uinit(struct ifpga_port_hw *port)
{
	feature_uinit(&port->feature_list);
}

int port_hw_init(struct ifpga_port_hw *port)
{
	int ret;

	if (port->state == IFPGA_PORT_UNUSED)
		return 0;

	ret = feature_init(port_feature_drvs, &port->feature_list);
	if (ret)
		goto error;

	return 0;
error:
	port_hw_uinit(port);
	return ret;
}

#define FPGA_MAX_MSIX_VEC_COUNT	128
/* irq set buffer length for interrupt */
#define MSIX_IRQ_SET_BUF_LEN (sizeof(struct vfio_irq_set) + \
				sizeof(int) * FPGA_MAX_MSIX_VEC_COUNT)

/* only support msix for now*/
static int vfio_msix_enable_block(s32 vfio_dev_fd, unsigned int vec_start,
				  unsigned int count, s32 *fds)
{
	char irq_set_buf[MSIX_IRQ_SET_BUF_LEN];
	struct vfio_irq_set *irq_set;
	int len, ret;
	int *fd_ptr;

	len = sizeof(irq_set_buf);

	irq_set = (struct vfio_irq_set *)irq_set_buf;
	irq_set->argsz = len;
	/* 0 < irq_set->count < FPGA_MAX_MSIX_VEC_COUNT */
	irq_set->count = count ?
		(count > FPGA_MAX_MSIX_VEC_COUNT ?
		 FPGA_MAX_MSIX_VEC_COUNT : count) : 1;
	irq_set->flags = VFIO_IRQ_SET_DATA_EVENTFD |
				VFIO_IRQ_SET_ACTION_TRIGGER;
	irq_set->index = VFIO_PCI_MSIX_IRQ_INDEX;
	irq_set->start = vec_start;

	fd_ptr = (int *)&irq_set->data;
	opae_memcpy(fd_ptr, fds, sizeof(int) * count);

	ret = ioctl(vfio_dev_fd, VFIO_DEVICE_SET_IRQS, irq_set);
	if (ret)
		printf("Error enabling MSI-X interrupts\n");

	return ret;
}

int fpga_msix_set_block(struct ifpga_feature *feature, unsigned int start,
			unsigned int count, s32 *fds)
{
	struct feature_irq_ctx *ctx = feature->ctx;
	unsigned int i;
	int ret;

	if (start >= feature->ctx_num || start + count > feature->ctx_num)
		return -EINVAL;

	/* assume that each feature has continuous vector space in msix*/
	ret = vfio_msix_enable_block(feature->vfio_dev_fd,
				     ctx[start].idx, count, fds);
	if (!ret) {
		for (i = 0; i < count; i++)
			ctx[i].eventfd = fds[i];
	}

	return ret;
}
