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
roc_dpi_configure(struct roc_dpi *roc_dpi)
{
	struct plt_pci_device *pci_dev;
	const struct plt_memzone *dpi_mz;
	dpi_mbox_msg_t mbox_msg;
	struct npa_pool_s pool;
	struct npa_aura_s aura;
	int rc, count, buflen;
	uint64_t aura_handle;
	plt_iova_t iova;
	char name[32];

	if (!roc_dpi) {
		plt_err("roc_dpi is NULL");
		return -EINVAL;
	}

	pci_dev = roc_dpi->pci_dev;
	memset(&pool, 0, sizeof(struct npa_pool_s));
	pool.nat_align = 1;

	memset(&aura, 0, sizeof(aura));
	rc = roc_npa_pool_create(&aura_handle, DPI_CMD_QUEUE_SIZE,
				 DPI_CMD_QUEUE_BUFS, &aura, &pool);
	if (rc) {
		plt_err("Failed to create NPA pool, err %d\n", rc);
		return rc;
	}

	snprintf(name, sizeof(name), "dpimem%d", roc_dpi->vfid);
	buflen = DPI_CMD_QUEUE_SIZE * DPI_CMD_QUEUE_BUFS;
	dpi_mz = plt_memzone_reserve_aligned(name, buflen, 0,
					     DPI_CMD_QUEUE_SIZE);
	if (dpi_mz == NULL) {
		plt_err("dpi memzone reserve failed");
		rc = -ENOMEM;
		goto err1;
	}

	roc_dpi->mz = dpi_mz;
	iova = dpi_mz->iova;
	for (count = 0; count < DPI_CMD_QUEUE_BUFS; count++) {
		roc_npa_aura_op_free(aura_handle, 0, iova);
		iova += DPI_CMD_QUEUE_SIZE;
	}

	roc_dpi->chunk_base = (void *)roc_npa_aura_op_alloc(aura_handle, 0);
	if (!roc_dpi->chunk_base) {
		plt_err("Failed to alloc buffer from NPA aura");
		rc = -ENOMEM;
		goto err2;
	}

	roc_dpi->chunk_next = (void *)roc_npa_aura_op_alloc(aura_handle, 0);
	if (!roc_dpi->chunk_next) {
		plt_err("Failed to alloc buffer from NPA aura");
		rc = -ENOMEM;
		goto err2;
	}

	roc_dpi->aura_handle = aura_handle;
	/* subtract 2 as they have already been alloc'ed above */
	roc_dpi->pool_size_m1 = (DPI_CMD_QUEUE_SIZE >> 3) - 2;

	plt_write64(0x0, roc_dpi->rbase + DPI_VDMA_REQQ_CTL);
	plt_write64(((uint64_t)(roc_dpi->chunk_base) >> 7) << 7,
		    roc_dpi->rbase + DPI_VDMA_SADDR);
	mbox_msg.u[0] = 0;
	mbox_msg.u[1] = 0;
	/* DPI PF driver expects vfid starts from index 0 */
	mbox_msg.s.vfid = roc_dpi->vfid;
	mbox_msg.s.cmd = DPI_QUEUE_OPEN;
	mbox_msg.s.csize = DPI_CMD_QUEUE_SIZE;
	mbox_msg.s.aura = roc_npa_aura_handle_to_aura(aura_handle);
	mbox_msg.s.sso_pf_func = idev_sso_pffunc_get();
	mbox_msg.s.npa_pf_func = idev_npa_pffunc_get();

	rc = send_msg_to_pf(&pci_dev->addr, (const char *)&mbox_msg,
			    sizeof(dpi_mbox_msg_t));
	if (rc < 0) {
		plt_err("Failed to send mbox message %d to DPI PF, err %d",
			mbox_msg.s.cmd, rc);
		goto err2;
	}

	return rc;

err2:
	plt_memzone_free(dpi_mz);
err1:
	roc_npa_pool_destroy(aura_handle);
	return rc;
}

int
roc_dpi_dev_init(struct roc_dpi *roc_dpi)
{
	struct plt_pci_device *pci_dev = roc_dpi->pci_dev;
	uint16_t vfid;

	roc_dpi->rbase = pci_dev->mem_resource[0].addr;
	vfid = ((pci_dev->addr.devid & 0x1F) << 3) |
	       (pci_dev->addr.function & 0x7);
	vfid -= 1;
	roc_dpi->vfid = vfid;
	plt_spinlock_init(&roc_dpi->chunk_lock);

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

	rc = send_msg_to_pf(&pci_dev->addr, (const char *)&mbox_msg,
			    sizeof(dpi_mbox_msg_t));
	if (rc < 0)
		plt_err("Failed to send mbox message %d to DPI PF, err %d",
			mbox_msg.s.cmd, rc);

	roc_npa_pool_destroy(roc_dpi->aura_handle);
	plt_memzone_free(roc_dpi->mz);

	return rc;
}
