/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2020 Marvell Semiconductor Inc.
 * All rights reserved.
 * www.marvell.com
 */

#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <time.h>
#include <rte_ethdev.h>
#include "base/bcm_osal.h"
#include "qede_ethdev.h"

int
qede_get_regs_len(struct qede_dev *qdev)
{
	struct ecore_dev *edev = &qdev->edev;
	int cur_engine, num_of_hwfns, regs_len = 0;
	uint8_t org_engine;

	if (IS_VF(edev))
		return 0;

	if (qdev->ops && qdev->ops->common) {
		num_of_hwfns = qdev->dev_info.common.num_hwfns;
		org_engine = qdev->ops->common->dbg_get_debug_engine(edev);
		for (cur_engine = 0; cur_engine < num_of_hwfns; cur_engine++) {
			/* compute required buffer size for idle_chks and
			 * grcDump for each hw function
			 */
			DP_NOTICE(edev, false,
				"Calculating idle_chk and grcdump register length for current engine\n");
			qdev->ops->common->dbg_set_debug_engine(edev,
								cur_engine);
			regs_len += REGDUMP_HEADER_SIZE +
				qdev->ops->common->dbg_idle_chk_size(edev) +
				REGDUMP_HEADER_SIZE +
				qdev->ops->common->dbg_idle_chk_size(edev) +
				REGDUMP_HEADER_SIZE +
				qdev->ops->common->dbg_grc_size(edev) +
				REGDUMP_HEADER_SIZE +
				qdev->ops->common->dbg_reg_fifo_size(edev) +
				REGDUMP_HEADER_SIZE +
				qdev->ops->common->dbg_protection_override_size(edev) +
				REGDUMP_HEADER_SIZE +
				qdev->ops->common->dbg_igu_fifo_size(edev) +
				REGDUMP_HEADER_SIZE +
				qdev->ops->common->dbg_fw_asserts_size(edev);
		}
		/* compute required buffer size for mcp trace and add it to the
		 * total required buffer size
		 */
		regs_len += REGDUMP_HEADER_SIZE +
			    qdev->ops->common->dbg_mcp_trace_size(edev);

		qdev->ops->common->dbg_set_debug_engine(edev, org_engine);
	}
	DP_NOTICE(edev, false, "Total length = %u\n", regs_len);

	return regs_len;
}

static uint32_t
qede_calc_regdump_header(enum debug_print_features feature, int engine,
			 uint32_t feature_size, uint8_t omit_engine)
{
	/* insert the engine, feature and mode inside the header and
	 * combine it with feature size
	 */
	return (feature_size | (feature << REGDUMP_HEADER_FEATURE_SHIFT) |
		(omit_engine << REGDUMP_HEADER_OMIT_ENGINE_SHIFT) |
		(engine << REGDUMP_HEADER_ENGINE_SHIFT));
}

int qede_get_regs(struct rte_eth_dev *eth_dev, struct rte_dev_reg_info *regs)
{
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	uint32_t *buffer = regs->data;
	int cur_engine, num_of_hwfns;
	/* '1' tells the parser to omit the engine number in the output files */
	uint8_t omit_engine = 0;
	uint8_t org_engine;
	uint32_t feature_size;
	uint32_t offset = 0;

	if (IS_VF(edev))
		return -ENOTSUP;

	if (buffer == NULL) {
		regs->length = qede_get_regs_len(qdev);
		regs->width =  sizeof(uint32_t);
		DP_INFO(edev, "Length %u\n", regs->length);
		return 0;
	}

	memset(buffer, 0, regs->length);
	num_of_hwfns = qdev->dev_info.common.num_hwfns;
	if (num_of_hwfns == 1)
		omit_engine = 1;

	OSAL_MUTEX_ACQUIRE(&edev->dbg_lock);

	org_engine = qdev->ops->common->dbg_get_debug_engine(edev);
	for (cur_engine = 0; cur_engine < num_of_hwfns; cur_engine++) {
		/* collect idle_chks and grcDump for each hw function */
		DP_NOTICE(edev, false, "obtaining idle_chk and grcdump for current engine\n");
		qdev->ops->common->dbg_set_debug_engine(edev, cur_engine);

		/* first idle_chk */
		qdev->ops->common->dbg_idle_chk(edev, (uint8_t *)buffer +
			offset + REGDUMP_HEADER_SIZE, &feature_size);
		*(uint32_t *)((uint8_t *)buffer + offset) =
			qede_calc_regdump_header(IDLE_CHK, cur_engine,
						 feature_size, omit_engine);
		offset += (feature_size + REGDUMP_HEADER_SIZE);
		DP_NOTICE(edev, false, "Idle Check1 feature_size %u\n",
			  feature_size);

		/* second idle_chk */
		qdev->ops->common->dbg_idle_chk(edev, (uint8_t *)buffer +
			offset + REGDUMP_HEADER_SIZE, &feature_size);
		*(uint32_t *)((uint8_t *)buffer + offset) =
			qede_calc_regdump_header(IDLE_CHK, cur_engine,
						 feature_size, omit_engine);
		offset += (feature_size + REGDUMP_HEADER_SIZE);
		DP_NOTICE(edev, false, "Idle Check2 feature_size %u\n",
			  feature_size);

		/* reg_fifo dump */
		qdev->ops->common->dbg_reg_fifo(edev, (uint8_t *)buffer +
			offset + REGDUMP_HEADER_SIZE, &feature_size);
		*(uint32_t *)((uint8_t *)buffer + offset) =
			qede_calc_regdump_header(REG_FIFO, cur_engine,
						 feature_size, omit_engine);
		offset += (feature_size + REGDUMP_HEADER_SIZE);
		DP_NOTICE(edev, false, "Reg fifo feature_size %u\n",
			  feature_size);

		/* igu_fifo dump */
		qdev->ops->common->dbg_igu_fifo(edev, (uint8_t *)buffer +
			offset + REGDUMP_HEADER_SIZE, &feature_size);
		*(uint32_t *)((uint8_t *)buffer + offset) =
			qede_calc_regdump_header(IGU_FIFO, cur_engine,
						 feature_size, omit_engine);
		offset += (feature_size + REGDUMP_HEADER_SIZE);
		DP_NOTICE(edev, false, "IGU fifo feature_size %u\n",
			  feature_size);

		/* protection_override dump */
		qdev->ops->common->dbg_protection_override(edev,
							   (uint8_t *)buffer +
			offset + REGDUMP_HEADER_SIZE, &feature_size);
		*(uint32_t *)((uint8_t *)buffer + offset) =
		       qede_calc_regdump_header(PROTECTION_OVERRIDE, cur_engine,
						feature_size, omit_engine);
		offset += (feature_size + REGDUMP_HEADER_SIZE);
		DP_NOTICE(edev, false, "Protection override feature_size %u\n",
			  feature_size);

		/* fw_asserts dump */
		qdev->ops->common->dbg_fw_asserts(edev, (uint8_t *)buffer +
			offset + REGDUMP_HEADER_SIZE, &feature_size);
		*(uint32_t *)((uint8_t *)buffer + offset) =
			qede_calc_regdump_header(FW_ASSERTS, cur_engine,
						 feature_size, omit_engine);
		offset += (feature_size + REGDUMP_HEADER_SIZE);
		DP_NOTICE(edev, false, "FW assert feature_size %u\n",
			  feature_size);

		/* grc dump */
		qdev->ops->common->dbg_grc(edev, (uint8_t *)buffer +
			offset + REGDUMP_HEADER_SIZE, &feature_size);
		*(uint32_t *)((uint8_t *)buffer + offset) =
			qede_calc_regdump_header(GRC_DUMP, cur_engine,
						 feature_size, omit_engine);
		offset += (feature_size + REGDUMP_HEADER_SIZE);
		DP_NOTICE(edev, false, "GRC dump feature_size %u\n",
			  feature_size);
	}

	/* mcp_trace */
	qdev->ops->common->dbg_mcp_trace(edev, (uint8_t *)buffer +
		offset + REGDUMP_HEADER_SIZE, &feature_size);
	*(uint32_t *)((uint8_t *)buffer + offset) =
		qede_calc_regdump_header(MCP_TRACE, cur_engine, feature_size,
					 omit_engine);
	offset += (feature_size + REGDUMP_HEADER_SIZE);
	DP_NOTICE(edev, false, "MCP trace feature_size %u\n", feature_size);

	qdev->ops->common->dbg_set_debug_engine(edev, org_engine);

	OSAL_MUTEX_RELEASE(&edev->dbg_lock);

	return 0;
}

static void
qede_set_fw_dump_file_name(struct qede_dev *qdev)
{
	time_t ltime;
	struct tm *tm;

	ltime = time(NULL);
	tm = localtime(&ltime);
	snprintf(qdev->dump_file, QEDE_FW_DUMP_FILE_SIZE,
		 "qede_pmd_dump_%02d-%02d-%02d_%02d-%02d-%02d.bin",
		 tm->tm_mon + 1, (int)tm->tm_mday, 1900 + tm->tm_year,
		 tm->tm_hour, tm->tm_min, tm->tm_sec);
}

static int
qede_write_fwdump(const char *dump_file, void *dump, size_t len)
{
	int err = 0;
	FILE *f;
	size_t bytes;

	f = fopen(dump_file, "wb+");

	if (!f) {
		fprintf(stderr, "Can't open file %s: %s\n",
			dump_file, strerror(errno));
		return 1;
	}
	bytes = fwrite(dump, 1, len, f);
	if (bytes != len) {
		fprintf(stderr,
			"Can not write all of dump data bytes=%zd len=%zd\n",
			bytes, len);
		err = 1;
	}

	if (fclose(f)) {
		fprintf(stderr, "Can't close file %s: %s\n",
			dump_file, strerror(errno));
		err = 1;
	}

	return err;
}

int
qede_save_fw_dump(uint16_t port_id)
{
	struct rte_eth_dev *eth_dev = &rte_eth_devices[port_id];
	struct rte_dev_reg_info regs;
	struct qede_dev *qdev = eth_dev->data->dev_private;
	struct ecore_dev *edev = &qdev->edev;
	int rc = 0;

	if (!rte_eth_dev_is_valid_port(port_id)) {
		DP_ERR(edev, "port %u invalid port ID", port_id);
		return -ENODEV;
	}

	memset(&regs, 0, sizeof(regs));
	regs.length = qede_get_regs_len(qdev);
	regs.data = OSAL_ZALLOC(eth_dev, GFP_KERNEL, regs.length);
	if (regs.data) {
		qede_get_regs(eth_dev, &regs);
		qede_set_fw_dump_file_name(qdev);
		rc = qede_write_fwdump(qdev->dump_file, regs.data, regs.length);
		if (!rc)
			DP_NOTICE(edev, false, "FW dump written to %s file\n",
				  qdev->dump_file);
		OSAL_FREE(edev, regs.data);
	}

	return rc;
}
