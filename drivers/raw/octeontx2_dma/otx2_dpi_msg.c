/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(C) 2019 Marvell International Ltd.
 */

#ifndef _DPI_MSG_H_
#define _DPI_MSG_H_

#include <dirent.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>

#include "otx2_dpi_rawdev.h"

/* DPI PF DBDF information macro's */
#define DPI_PF_DBDF_DOMAIN      0
#define DPI_PF_DBDF_BUS         5
#define DPI_PF_DBDF_DEVICE      0
#define DPI_PF_DBDF_FUNCTION    0

#define DPI_PF_MBOX_SYSFS_ENTRY "dpi_device_config"

union dpi_mbox_message_u {
	uint64_t u[2];
	struct dpi_mbox_message_s {
		/* VF ID to configure */
		uint64_t vfid           :4;
		/* Command code */
		uint64_t cmd            :4;
		/* Command buffer size in 8-byte words */
		uint64_t csize          :14;
		/* aura of the command buffer */
		uint64_t aura           :20;
		/* SSO PF function */
		uint64_t sso_pf_func    :16;
		/* NPA PF function */
		uint64_t npa_pf_func    :16;
	} s;
};

static inline int
send_msg_to_pf(struct rte_pci_addr *pci, const char *value, int size)
{
	char buff[255] = { 0 };
	int res, fd;

	res = snprintf(buff, sizeof(buff), "%s/" PCI_PRI_FMT "/%s",
		       rte_pci_get_sysfs_path(), pci->domain,
		       pci->bus, DPI_PF_DBDF_DEVICE & 0x7,
		       DPI_PF_DBDF_FUNCTION & 0x7, DPI_PF_MBOX_SYSFS_ENTRY);
	if ((res < 0) || ((size_t)res > sizeof(buff)))
		return -ERANGE;

	fd = open(buff, O_WRONLY);
	if (fd < 0)
		return -EACCES;
	res = write(fd, value, size);
	close(fd);
	if (res < 0)
		return -EACCES;

	return 0;
}

int
otx2_dpi_queue_open(struct dpi_vf_s *dpivf, uint32_t size, uint32_t gaura)
{
	union dpi_mbox_message_u mbox_msg;
	int ret = 0;

	/* DPI PF driver expects vfid starts from index 0 */
	mbox_msg.s.vfid = dpivf->vf_id;
	mbox_msg.s.cmd = DPI_QUEUE_OPEN;
	mbox_msg.s.csize = size;
	mbox_msg.s.aura = gaura;
	mbox_msg.s.sso_pf_func = otx2_sso_pf_func_get();
	mbox_msg.s.npa_pf_func = otx2_npa_pf_func_get();

	ret = send_msg_to_pf(&dpivf->dev->addr, (const char *)&mbox_msg,
				sizeof(mbox_msg));
	if (ret < 0)
		otx2_dpi_dbg("Failed to send mbox message to dpi pf");

	return ret;
}

int
otx2_dpi_queue_close(struct dpi_vf_s *dpivf)
{
	union dpi_mbox_message_u mbox_msg;
	int ret = 0;

	/* DPI PF driver expects vfid starts from index 0 */
	mbox_msg.s.vfid = dpivf->vf_id;
	mbox_msg.s.cmd = DPI_QUEUE_CLOSE;

	ret = send_msg_to_pf(&dpivf->dev->addr, (const char *)&mbox_msg,
				sizeof(mbox_msg));
	if (ret < 0)
		otx2_dpi_dbg("Failed to send mbox message to dpi pf");

	return ret;
}

#endif /* _DPI_MSG_H_ */
