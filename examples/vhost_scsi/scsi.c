/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2017 Intel Corporation
 */

/**
 * This work is largely based on the "vhost-user-scsi" implementation by
 * SPDK(https://github.com/spdk/spdk).
 */

#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <assert.h>
#include <ctype.h>
#include <string.h>
#include <stddef.h>

#include <rte_atomic.h>
#include <rte_cycles.h>
#include <rte_log.h>
#include <rte_malloc.h>
#include <rte_byteorder.h>
#include <rte_string_fns.h>

#include "vhost_scsi.h"
#include "scsi_spec.h"

#define INQ_OFFSET(field) (offsetof(struct scsi_cdb_inquiry_data, field) + \
			  sizeof(((struct scsi_cdb_inquiry_data *)0x0)->field))

static void
vhost_strcpy_pad(void *dst, const char *src, size_t size, int pad)
{
	size_t len;

	len = strlen(src);
	if (len < size) {
		memcpy(dst, src, len);
		memset((char *)dst + len, pad, size - len);
	} else {
		memcpy(dst, src, size);
	}
}

static int
vhost_hex2bin(char ch)
{
	if ((ch >= '0') && (ch <= '9'))
		return ch - '0';
	ch = tolower(ch);
	if ((ch >= 'a') && (ch <= 'f'))
		return ch - 'a' + 10;
	return (int)ch;
}

static void
vhost_bdev_scsi_set_naa_ieee_extended(const char *name, uint8_t *buf)
{
	int i, value, count = 0;
	uint64_t *temp64, local_value;

	for (i = 0; (i < 16) && (name[i] != '\0'); i++) {
		value = vhost_hex2bin(name[i]);
		if (i % 2)
			buf[count++] |= value << 4;
		else
			buf[count] = value;
	}

	local_value = *(uint64_t *)buf;
	/*
	 * see spc3r23 7.6.3.6.2,
	 *  NAA IEEE Extended identifer format
	 */
	local_value &= 0x0fff000000ffffffull;
	/* NAA 02, and 00 03 47 for IEEE Intel */
	local_value |= 0x2000000347000000ull;

	temp64 = (uint64_t *)buf;
	*temp64 = rte_cpu_to_be_64(local_value);
}

static void
scsi_task_build_sense_data(struct vhost_scsi_task *task, int sk,
			   int asc, int ascq)
{
	uint8_t *cp;
	int resp_code;

	resp_code = 0x70; /* Current + Fixed format */

	/* Sense Data */
	cp = (uint8_t *)task->resp->sense;

	/* VALID(7) RESPONSE CODE(6-0) */
	cp[0] = 0x80 | resp_code;
	/* Obsolete */
	cp[1] = 0;
	/* FILEMARK(7) EOM(6) ILI(5) SENSE KEY(3-0) */
	cp[2] = sk & 0xf;
	/* INFORMATION */
	memset(&cp[3], 0, 4);

	/* ADDITIONAL SENSE LENGTH */
	cp[7] = 10;

	/* COMMAND-SPECIFIC INFORMATION */
	memset(&cp[8], 0, 4);
	/* ADDITIONAL SENSE CODE */
	cp[12] = asc;
	/* ADDITIONAL SENSE CODE QUALIFIER */
	cp[13] = ascq;
	/* FIELD REPLACEABLE UNIT CODE */
	cp[14] = 0;

	/* SKSV(7) SENSE KEY SPECIFIC(6-0,7-0,7-0) */
	cp[15] = 0;
	cp[16] = 0;
	cp[17] = 0;

	/* SenseLength */
	task->resp->sense_len = 18;
}

static void
scsi_task_set_status(struct vhost_scsi_task *task, int sc, int sk,
		     int asc, int ascq)
{
	if (sc == SCSI_STATUS_CHECK_CONDITION)
		scsi_task_build_sense_data(task, sk, asc, ascq);
	task->resp->status = sc;
}

static int
vhost_bdev_scsi_inquiry_command(struct vhost_block_dev *bdev,
				struct vhost_scsi_task *task)
{
	int hlen = 0;
	uint32_t alloc_len = 0;
	uint16_t len = 0;
	uint16_t *temp16;
	int pc;
	int pd;
	int evpd;
	int i;
	uint8_t *buf;
	struct scsi_cdb_inquiry *inq;

	inq = (struct scsi_cdb_inquiry *)task->req->cdb;

	assert(task->iovs_cnt == 1);

	/* At least 36Bytes for inquiry command */
	if (task->data_len < 0x24)
		goto inq_error;

	pd = SPC_PERIPHERAL_DEVICE_TYPE_DISK;
	pc = inq->page_code;
	evpd = inq->evpd & 0x1;

	if (!evpd && pc)
		goto inq_error;

	if (evpd) {
		struct scsi_vpd_page *vpage = (struct scsi_vpd_page *)
					      task->iovs[0].iov_base;

		/* PERIPHERAL QUALIFIER(7-5) PERIPHERAL DEVICE TYPE(4-0) */
		vpage->peripheral = pd;
		/* PAGE CODE */
		vpage->page_code = pc;

		switch (pc) {
		case SPC_VPD_SUPPORTED_VPD_PAGES:
			hlen = 4;
			vpage->params[0] = SPC_VPD_SUPPORTED_VPD_PAGES;
			vpage->params[1] = SPC_VPD_UNIT_SERIAL_NUMBER;
			vpage->params[2] = SPC_VPD_DEVICE_IDENTIFICATION;
			len = 3;
			/* PAGE LENGTH */
			vpage->alloc_len = rte_cpu_to_be_16(len);
			break;
		case SPC_VPD_UNIT_SERIAL_NUMBER:
			hlen = 4;
			strlcpy((char *)vpage->params, bdev->name,
					sizeof(vpage->params));
			vpage->alloc_len = rte_cpu_to_be_16(32);
			break;
		case SPC_VPD_DEVICE_IDENTIFICATION:
			buf = vpage->params;
			struct scsi_desig_desc *desig;

			hlen = 4;
			/* NAA designator */
			desig = (struct scsi_desig_desc *)buf;
			desig->code_set = SPC_VPD_CODE_SET_BINARY;
			desig->protocol_id = SPC_PROTOCOL_IDENTIFIER_ISCSI;
			desig->type = SPC_VPD_IDENTIFIER_TYPE_NAA;
			desig->association = SPC_VPD_ASSOCIATION_LOGICAL_UNIT;
			desig->reserved0 = 0;
			desig->piv = 1;
			desig->reserved1 = 0;
			desig->len = 8;
			vhost_bdev_scsi_set_naa_ieee_extended(bdev->name,
							      desig->desig);
			len = sizeof(struct scsi_desig_desc) + 8;

			buf += sizeof(struct scsi_desig_desc) + desig->len;

			/* T10 Vendor ID designator */
			desig = (struct scsi_desig_desc *)buf;
			desig->code_set = SPC_VPD_CODE_SET_ASCII;
			desig->protocol_id = SPC_PROTOCOL_IDENTIFIER_ISCSI;
			desig->type = SPC_VPD_IDENTIFIER_TYPE_T10_VENDOR_ID;
			desig->association = SPC_VPD_ASSOCIATION_LOGICAL_UNIT;
			desig->reserved0 = 0;
			desig->piv = 1;
			desig->reserved1 = 0;
			desig->len = 8 + 16 + 32;
			strlcpy((char *)desig->desig, "INTEL", 8);
			vhost_strcpy_pad((char *)&desig->desig[8],
					 bdev->product_name, 16, ' ');
			strlcpy((char *)&desig->desig[24], bdev->name, 32);
			len += sizeof(struct scsi_desig_desc) + 8 + 16 + 32;

			buf += sizeof(struct scsi_desig_desc) + desig->len;

			/* SCSI Device Name designator */
			desig = (struct scsi_desig_desc *)buf;
			desig->code_set = SPC_VPD_CODE_SET_UTF8;
			desig->protocol_id = SPC_PROTOCOL_IDENTIFIER_ISCSI;
			desig->type = SPC_VPD_IDENTIFIER_TYPE_SCSI_NAME;
			desig->association = SPC_VPD_ASSOCIATION_TARGET_DEVICE;
			desig->reserved0 = 0;
			desig->piv = 1;
			desig->reserved1 = 0;
			desig->len = strlcpy((char *)desig->desig, bdev->name,
					     255);
			len += sizeof(struct scsi_desig_desc) + desig->len;

			buf += sizeof(struct scsi_desig_desc) + desig->len;
			vpage->alloc_len = rte_cpu_to_be_16(len);
			break;
		default:
			goto inq_error;
		}

	} else {
		struct scsi_cdb_inquiry_data *inqdata =
			(struct scsi_cdb_inquiry_data *)task->iovs[0].iov_base;
		/* Standard INQUIRY data */
		/* PERIPHERAL QUALIFIER(7-5) PERIPHERAL DEVICE TYPE(4-0) */
		inqdata->peripheral = pd;

		/* RMB(7) */
		inqdata->rmb = 0;

		/* VERSION */
		/* See SPC3/SBC2/MMC4/SAM2 for more details */
		inqdata->version = SPC_VERSION_SPC3;

		/* NORMACA(5) HISUP(4) RESPONSE DATA FORMAT(3-0) */
		/* format 2 */ /* hierarchical support */
		inqdata->response = 2 | 1 << 4;

		hlen = 5;

		/* SCCS(7) ACC(6) TPGS(5-4) 3PC(3) PROTECT(0) */
		/* Not support TPGS */
		inqdata->flags = 0;

		/* MULTIP */
		inqdata->flags2 = 0x10;

		/* WBUS16(5) SYNC(4) LINKED(3) CMDQUE(1) VS(0) */
		/* CMDQUE */
		inqdata->flags3 = 0x2;

		/* T10 VENDOR IDENTIFICATION */
		strlcpy((char *)inqdata->t10_vendor_id, "INTEL",
			sizeof(inqdata->t10_vendor_id));

		/* PRODUCT IDENTIFICATION */
		strlcpy((char *)inqdata->product_id, bdev->product_name,
			RTE_DIM(inqdata->product_id));

		/* PRODUCT REVISION LEVEL */
		strlcpy((char *)inqdata->product_rev, "0001",
			sizeof(inqdata->product_rev));

		/* Standard inquiry data ends here. Only populate
		 * remaining fields if alloc_len indicates enough
		 * space to hold it.
		 */
		len = INQ_OFFSET(product_rev) - 5;

		if (alloc_len >= INQ_OFFSET(vendor)) {
			/* Vendor specific */
			memset(inqdata->vendor, 0x20, 20);
			len += sizeof(inqdata->vendor);
		}

		if (alloc_len >= INQ_OFFSET(ius)) {
			/* CLOCKING(3-2) QAS(1) IUS(0) */
			inqdata->ius = 0;
			len += sizeof(inqdata->ius);
		}

		if (alloc_len >= INQ_OFFSET(reserved)) {
			/* Reserved */
			inqdata->reserved = 0;
			len += sizeof(inqdata->reserved);
		}

		/* VERSION DESCRIPTOR 1-8 */
		if (alloc_len >= INQ_OFFSET(reserved) + 2) {
			temp16 = (uint16_t *)&inqdata->desc[0];
			*temp16 = rte_cpu_to_be_16(0x0960);
			len += 2;
		}

		if (alloc_len >= INQ_OFFSET(reserved) + 4) {
			/* SPC-3 (no version claimed) */
			temp16 = (uint16_t *)&inqdata->desc[2];
			*temp16 = rte_cpu_to_be_16(0x0300);
			len += 2;
		}

		if (alloc_len >= INQ_OFFSET(reserved) + 6) {
			/* SBC-2 (no version claimed) */
			temp16 = (uint16_t *)&inqdata->desc[4];
			*temp16 = rte_cpu_to_be_16(0x0320);
			len += 2;
		}

		if (alloc_len >= INQ_OFFSET(reserved) + 8) {
			/* SAM-2 (no version claimed) */
			temp16 = (uint16_t *)&inqdata->desc[6];
			*temp16 = rte_cpu_to_be_16(0x0040);
			len += 2;
		}

		if (alloc_len > INQ_OFFSET(reserved) + 8) {
			i = alloc_len - (INQ_OFFSET(reserved) + 8);
			if (i > 30)
				i = 30;
			memset(&inqdata->desc[8], 0, i);
			len += i;
		}

		/* ADDITIONAL LENGTH */
		inqdata->add_len = len;
	}

	/* STATUS GOOD */
	scsi_task_set_status(task, SCSI_STATUS_GOOD, 0, 0, 0);
	return hlen + len;

inq_error:
	scsi_task_set_status(task, SCSI_STATUS_CHECK_CONDITION,
				     SCSI_SENSE_ILLEGAL_REQUEST,
				     SCSI_ASC_INVALID_FIELD_IN_CDB,
				     SCSI_ASCQ_CAUSE_NOT_REPORTABLE);
	return 0;
}

static int
vhost_bdev_scsi_readwrite(struct vhost_block_dev *bdev,
			  struct vhost_scsi_task *task,
			  uint64_t lba, __rte_unused uint32_t xfer_len)
{
	uint32_t i;
	uint64_t offset;
	uint32_t nbytes = 0;

	offset = lba * bdev->blocklen;

	for (i = 0; i < task->iovs_cnt; i++) {
		if (task->dxfer_dir == SCSI_DIR_TO_DEV)
			memcpy(bdev->data + offset, task->iovs[i].iov_base,
			       task->iovs[i].iov_len);
		else
			memcpy(task->iovs[i].iov_base, bdev->data + offset,
			       task->iovs[i].iov_len);
		offset += task->iovs[i].iov_len;
		nbytes += task->iovs[i].iov_len;
	}

	return nbytes;
}

static int
vhost_bdev_scsi_process_block(struct vhost_block_dev *bdev,
			      struct vhost_scsi_task *task)
{
	uint64_t lba, *temp64;
	uint32_t xfer_len, *temp32;
	uint16_t *temp16;
	uint8_t *cdb = (uint8_t *)task->req->cdb;

	switch (cdb[0]) {
	case SBC_READ_6:
	case SBC_WRITE_6:
		lba = (uint64_t)cdb[1] << 16;
		lba |= (uint64_t)cdb[2] << 8;
		lba |= (uint64_t)cdb[3];
		xfer_len = cdb[4];
		if (xfer_len == 0)
			xfer_len = 256;
		return vhost_bdev_scsi_readwrite(bdev, task, lba, xfer_len);

	case SBC_READ_10:
	case SBC_WRITE_10:
		temp32 = (uint32_t *)&cdb[2];
		lba = rte_be_to_cpu_32(*temp32);
		temp16 = (uint16_t *)&cdb[7];
		xfer_len = rte_be_to_cpu_16(*temp16);
		return vhost_bdev_scsi_readwrite(bdev, task, lba, xfer_len);

	case SBC_READ_12:
	case SBC_WRITE_12:
		temp32 = (uint32_t *)&cdb[2];
		lba = rte_be_to_cpu_32(*temp32);
		temp32 = (uint32_t *)&cdb[6];
		xfer_len = rte_be_to_cpu_32(*temp32);
		return vhost_bdev_scsi_readwrite(bdev, task, lba, xfer_len);

	case SBC_READ_16:
	case SBC_WRITE_16:
		temp64 = (uint64_t *)&cdb[2];
		lba = rte_be_to_cpu_64(*temp64);
		temp32 = (uint32_t *)&cdb[10];
		xfer_len = rte_be_to_cpu_32(*temp32);
		return vhost_bdev_scsi_readwrite(bdev, task, lba, xfer_len);

	case SBC_READ_CAPACITY_10: {
		uint8_t buffer[8];

		if (bdev->blockcnt - 1 > 0xffffffffULL)
			memset(buffer, 0xff, 4);
		else {
			temp32 = (uint32_t *)buffer;
			*temp32 = rte_cpu_to_be_32(bdev->blockcnt - 1);
		}
		temp32 = (uint32_t *)&buffer[4];
		*temp32 = rte_cpu_to_be_32(bdev->blocklen);
		memcpy(task->iovs[0].iov_base, buffer, sizeof(buffer));
		task->resp->status = SCSI_STATUS_GOOD;
		return sizeof(buffer);
	}

	case SBC_SYNCHRONIZE_CACHE_10:
	case SBC_SYNCHRONIZE_CACHE_16:
		task->resp->status = SCSI_STATUS_GOOD;
		return 0;
	}

	scsi_task_set_status(task, SCSI_STATUS_CHECK_CONDITION,
			     SCSI_SENSE_ILLEGAL_REQUEST,
			     SCSI_ASC_INVALID_FIELD_IN_CDB,
			     SCSI_ASCQ_CAUSE_NOT_REPORTABLE);
	return 0;
}

int
vhost_bdev_process_scsi_commands(struct vhost_block_dev *bdev,
				 struct vhost_scsi_task *task)
{
	int len;
	uint8_t *data;
	uint64_t *temp64, fmt_lun = 0;
	uint32_t *temp32;
	const uint8_t *lun;
	uint8_t *cdb = (uint8_t *)task->req->cdb;

	lun = (const uint8_t *)task->req->lun;
	/* only 1 LUN supported */
	if (lun[0] != 1 || lun[1] >= 1)
		return -1;

	switch (cdb[0]) {
	case SPC_INQUIRY:
		len = vhost_bdev_scsi_inquiry_command(bdev, task);
		task->data_len = len;
		break;
	case SPC_REPORT_LUNS:
		data = (uint8_t *)task->iovs[0].iov_base;
		fmt_lun |= (0x0ULL & 0x00ffULL) << 48;
		temp64 = (uint64_t *)&data[8];
		*temp64 = rte_cpu_to_be_64(fmt_lun);
		temp32 = (uint32_t *)data;
		*temp32 = rte_cpu_to_be_32(8);
		task->data_len = 16;
		scsi_task_set_status(task, SCSI_STATUS_GOOD, 0, 0, 0);
		break;
	case SPC_MODE_SELECT_6:
	case SPC_MODE_SELECT_10:
		/* don't support it now */
		scsi_task_set_status(task, SCSI_STATUS_GOOD, 0, 0, 0);
		break;
	case SPC_MODE_SENSE_6:
	case SPC_MODE_SENSE_10:
		/* don't support it now */
		scsi_task_set_status(task, SCSI_STATUS_GOOD, 0, 0, 0);
		break;
	case SPC_TEST_UNIT_READY:
		scsi_task_set_status(task, SCSI_STATUS_GOOD, 0, 0, 0);
		break;
	default:
		len = vhost_bdev_scsi_process_block(bdev, task);
		task->data_len = len;
	}

	return 0;
}
