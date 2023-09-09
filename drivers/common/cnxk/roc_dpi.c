/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2021 Marvell.
 */

#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "roc_api.h"
#include "roc_priv.h"

#define DPI_PF_MBOX_SYSFS_ENTRY "dpi_device_config"

static inline int
send_msg_to_pf(struct plt_pci_addr *pci_addr, const char *value, int size)
{
	char buf[255] = {0};
	int res, fd;

	res = snprintf(
		buf, sizeof(buf), "/sys/bus/pci/devices/" PCI_PRI_FMT "/%s",
		pci_addr->domain, pci_addr->bus, DPI_PF_DBDF_DEVICE & 0x7,
		DPI_PF_DBDF_FUNCTION & 0x7, DPI_PF_MBOX_SYSFS_ENTRY);

	if ((res < 0) || ((size_t)res > sizeof(buf)))
		return -ERANGE;

	fd = open(buf, O_WRONLY);
	if (fd < 0)
		return -EACCES;

	res = write(fd, value, size);
	close(fd);
	if (res < 0)
		return -EACCES;

	return 0;
}

int
roc_dpi_enable(struct roc_dpi *dpi)
{
	plt_write64(0x1, dpi->rbase + DPI_VDMA_EN);
	return 0;
}

int
roc_dpi_disable(struct roc_dpi *dpi)
{
	plt_write64(0x0, dpi->rbase + DPI_VDMA_EN);
	return 0;
}

int
roc_dpi_configure(struct roc_dpi *roc_dpi, uint32_t chunk_sz, uint64_t aura, uint64_t chunk_base)
{
	struct plt_pci_device *pci_dev;
	dpi_mbox_msg_t mbox_msg;
	uint64_t reg;
	int rc;

	if (!roc_dpi) {
		plt_err("roc_dpi is NULL");
		return -EINVAL;
	}

	pci_dev = roc_dpi->pci_dev;

	roc_dpi_disable(roc_dpi);
	reg = plt_read64(roc_dpi->rbase + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(63)))
		reg = plt_read64(roc_dpi->rbase + DPI_VDMA_SADDR);

	plt_write64(0x0, roc_dpi->rbase + DPI_VDMA_REQQ_CTL);
	plt_write64(chunk_base, roc_dpi->rbase + DPI_VDMA_SADDR);
	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	/* DPI PF driver expects vfid starts from index 0 */
	mbox_msg.s.vfid = roc_dpi->vfid;
	mbox_msg.s.cmd = DPI_QUEUE_OPEN;
	mbox_msg.s.csize = chunk_sz;
	mbox_msg.s.aura = aura;
	mbox_msg.s.sso_pf_func = idev_sso_pffunc_get();
	mbox_msg.s.npa_pf_func = idev_npa_pffunc_get();

	rc = send_msg_to_pf(&pci_dev->addr, (const char *)&mbox_msg,
			    sizeof(dpi_mbox_msg_t));
	if (rc < 0)
		plt_err("Failed to send mbox message %d to DPI PF, err %d",
			mbox_msg.s.cmd, rc);

	return rc;
}

int
roc_dpi_dev_init(struct roc_dpi *roc_dpi)
{
	struct plt_pci_device *pci_dev = roc_dpi->pci_dev;
	uint16_t vfid;

	roc_dpi->rbase = pci_dev->mem_resource[0].addr;
	vfid = ((pci_dev->addr.devid & 0x1F) << 3) | (pci_dev->addr.function & 0x7);
	vfid -= 1;
	roc_dpi->vfid = vfid;

	return 0;
}

int
roc_dpi_dev_fini(struct roc_dpi *roc_dpi)
{
	struct plt_pci_device *pci_dev = roc_dpi->pci_dev;
	dpi_mbox_msg_t mbox_msg;
	uint64_t reg;
	int rc;

	/* Wait for SADDR to become idle */
	reg = plt_read64(roc_dpi->rbase + DPI_VDMA_SADDR);
	while (!(reg & BIT_ULL(63)))
		reg = plt_read64(roc_dpi->rbase + DPI_VDMA_SADDR);

	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	mbox_msg.s.vfid = roc_dpi->vfid;
	mbox_msg.s.cmd = DPI_QUEUE_CLOSE;

	rc = send_msg_to_pf(&pci_dev->addr, (const char *)&mbox_msg, sizeof(dpi_mbox_msg_t));
	if (rc < 0)
		plt_err("Failed to send mbox message %d to DPI PF, err %d", mbox_msg.s.cmd, rc);

	return rc;
}
