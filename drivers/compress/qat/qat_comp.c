/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */


#include <rte_mempool.h>
#include <rte_mbuf.h>
#include <rte_hexdump.h>
#include <rte_comp.h>
#include <rte_bus_pci.h>
#include <rte_byteorder.h>
#include <rte_memcpy.h>
#include <rte_common.h>
#include <rte_spinlock.h>
#include <rte_log.h>
#include <rte_malloc.h>

#include "qat_logs.h"
#include "qat_comp.h"
#include "qat_comp_pmd.h"

unsigned int
qat_comp_xform_size(void)
{
	return RTE_ALIGN_CEIL(sizeof(struct qat_comp_xform), 8);
}

static void qat_comp_create_req_hdr(struct icp_qat_fw_comn_req_hdr *header,
				    enum qat_comp_request_type request)
{
	if (request == QAT_COMP_REQUEST_FIXED_COMP_STATELESS)
		header->service_cmd_id = ICP_QAT_FW_COMP_CMD_STATIC;
	else if (request == QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS)
		header->service_cmd_id = ICP_QAT_FW_COMP_CMD_DYNAMIC;
	else if (request == QAT_COMP_REQUEST_DECOMPRESS)
		header->service_cmd_id = ICP_QAT_FW_COMP_CMD_DECOMPRESS;

	header->service_type = ICP_QAT_FW_COMN_REQ_CPM_FW_COMP;
	header->hdr_flags =
	    ICP_QAT_FW_COMN_HDR_FLAGS_BUILD(ICP_QAT_FW_COMN_REQ_FLAG_SET);

	header->comn_req_flags = ICP_QAT_FW_COMN_FLAGS_BUILD(
	    QAT_COMN_CD_FLD_TYPE_16BYTE_DATA, QAT_COMN_PTR_TYPE_FLAT);
}

static int qat_comp_create_templates(struct qat_comp_xform *qat_xform,
			const struct rte_memzone *interm_buff_mz __rte_unused,
			const struct rte_comp_xform *xform)
{
	struct icp_qat_fw_comp_req *comp_req;
	int comp_level, algo;
	uint32_t req_par_flags;
	int direction = ICP_QAT_HW_COMPRESSION_DIR_COMPRESS;

	if (unlikely(qat_xform == NULL)) {
		QAT_LOG(ERR, "Session was not created for this device");
		return -EINVAL;
	}

	if (qat_xform->qat_comp_request_type == QAT_COMP_REQUEST_DECOMPRESS) {
		direction = ICP_QAT_HW_COMPRESSION_DIR_DECOMPRESS;
		comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_1;
		req_par_flags = ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
				ICP_QAT_FW_COMP_SOP, ICP_QAT_FW_COMP_EOP,
				ICP_QAT_FW_COMP_BFINAL, ICP_QAT_FW_COMP_NO_CNV,
				ICP_QAT_FW_COMP_NO_CNV_RECOVERY);

	} else {
		if (xform->compress.level == RTE_COMP_LEVEL_PMD_DEFAULT)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_8;
		else if (xform->compress.level == 1)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_1;
		else if (xform->compress.level == 2)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_4;
		else if (xform->compress.level == 3)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_8;
		else if (xform->compress.level >= 4 &&
			 xform->compress.level <= 9)
			comp_level = ICP_QAT_HW_COMPRESSION_DEPTH_16;
		else {
			QAT_LOG(ERR, "compression level not supported");
			return -EINVAL;
		}
		req_par_flags = ICP_QAT_FW_COMP_REQ_PARAM_FLAGS_BUILD(
				ICP_QAT_FW_COMP_SOP, ICP_QAT_FW_COMP_EOP,
				ICP_QAT_FW_COMP_BFINAL, ICP_QAT_FW_COMP_CNV,
				ICP_QAT_FW_COMP_CNV_RECOVERY);
	}

	switch (xform->compress.algo) {
	case RTE_COMP_ALGO_DEFLATE:
		algo = ICP_QAT_HW_COMPRESSION_ALGO_DEFLATE;
		break;
	case RTE_COMP_ALGO_LZS:
	default:
		/* RTE_COMP_NULL */
		QAT_LOG(ERR, "compression algorithm not supported");
		return -EINVAL;
	}

	comp_req = &qat_xform->qat_comp_req_tmpl;

	/* Initialize header */
	qat_comp_create_req_hdr(&comp_req->comn_hdr,
					qat_xform->qat_comp_request_type);

	comp_req->comn_hdr.serv_specif_flags = ICP_QAT_FW_COMP_FLAGS_BUILD(
	    ICP_QAT_FW_COMP_STATELESS_SESSION,
	    ICP_QAT_FW_COMP_NOT_AUTO_SELECT_BEST,
	    ICP_QAT_FW_COMP_NOT_ENH_AUTO_SELECT_BEST,
	    ICP_QAT_FW_COMP_NOT_DISABLE_TYPE0_ENH_AUTO_SELECT_BEST,
	    ICP_QAT_FW_COMP_DISABLE_SECURE_RAM_USED_AS_INTMD_BUF);

	comp_req->cd_pars.sl.comp_slice_cfg_word[0] =
	    ICP_QAT_HW_COMPRESSION_CONFIG_BUILD(
		direction,
		/* In CPM 1.6 only valid mode ! */
		ICP_QAT_HW_COMPRESSION_DELAYED_MATCH_ENABLED, algo,
		/* Translate level to depth */
		comp_level, ICP_QAT_HW_COMPRESSION_FILE_TYPE_0);

	comp_req->comp_pars.initial_adler = 1;
	comp_req->comp_pars.initial_crc32 = 0;
	comp_req->comp_pars.req_par_flags = req_par_flags;


	if (qat_xform->qat_comp_request_type ==
			QAT_COMP_REQUEST_FIXED_COMP_STATELESS ||
	    qat_xform->qat_comp_request_type == QAT_COMP_REQUEST_DECOMPRESS) {
		ICP_QAT_FW_COMN_NEXT_ID_SET(&comp_req->comp_cd_ctrl,
					    ICP_QAT_FW_SLICE_DRAM_WR);
		ICP_QAT_FW_COMN_CURR_ID_SET(&comp_req->comp_cd_ctrl,
					    ICP_QAT_FW_SLICE_COMP);
	} else if (qat_xform->qat_comp_request_type ==
		   QAT_COMP_REQUEST_DYNAMIC_COMP_STATELESS) {

		QAT_LOG(ERR, "Dynamic huffman encoding not supported");
		return -EINVAL;
	}

#if RTE_LOG_DP_LEVEL >= RTE_LOG_DEBUG
	QAT_DP_HEXDUMP_LOG(DEBUG, "qat compression message template:", comp_req,
		    sizeof(struct icp_qat_fw_comp_req));
#endif
	return 0;
}

/**
 * Create driver private_xform data.
 *
 * @param dev
 *   Compressdev device
 * @param xform
 *   xform data from application
 * @param private_xform
 *   ptr where handle of pmd's private_xform data should be stored
 * @return
 *  - if successful returns 0
 *    and valid private_xform handle
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 *  - Returns -ENOTSUP if comp device does not support the comp transform.
 *  - Returns -ENOMEM if the private_xform could not be allocated.
 */
int
qat_comp_private_xform_create(struct rte_compressdev *dev,
			      const struct rte_comp_xform *xform,
			      void **private_xform)
{
	struct qat_comp_dev_private *qat = dev->data->dev_private;

	if (unlikely(private_xform == NULL)) {
		QAT_LOG(ERR, "QAT: private_xform parameter is NULL");
		return -EINVAL;
	}
	if (unlikely(qat->xformpool == NULL)) {
		QAT_LOG(ERR, "QAT device has no private_xform mempool");
		return -ENOMEM;
	}
	if (rte_mempool_get(qat->xformpool, private_xform)) {
		QAT_LOG(ERR, "Couldn't get object from qat xform mempool");
		return -ENOMEM;
	}

	struct qat_comp_xform *qat_xform =
			(struct qat_comp_xform *)*private_xform;

	if (xform->type == RTE_COMP_COMPRESS) {
		if (xform->compress.deflate.huffman ==
				RTE_COMP_HUFFMAN_DYNAMIC) {
			QAT_LOG(ERR,
			"QAT device doesn't support dynamic compression");
			return -ENOTSUP;
		}

		if (xform->compress.deflate.huffman == RTE_COMP_HUFFMAN_FIXED ||
		  ((xform->compress.deflate.huffman == RTE_COMP_HUFFMAN_DEFAULT)
				   && qat->interm_buff_mz == NULL))

			qat_xform->qat_comp_request_type =
					QAT_COMP_REQUEST_FIXED_COMP_STATELESS;


	} else {
		qat_xform->qat_comp_request_type = QAT_COMP_REQUEST_DECOMPRESS;
	}

	qat_xform->checksum_type = xform->compress.chksum;

	if (qat_comp_create_templates(qat_xform, qat->interm_buff_mz, xform)) {
		QAT_LOG(ERR, "QAT: Problem with setting compression");
		return -EINVAL;
	}
	return 0;
}

/**
 * Free driver private_xform data.
 *
 * @param dev
 *   Compressdev device
 * @param private_xform
 *   handle of pmd's private_xform data
 * @return
 *  - 0 if successful
 *  - <0 in error cases
 *  - Returns -EINVAL if input parameters are invalid.
 */
int
qat_comp_private_xform_free(struct rte_compressdev *dev __rte_unused,
			    void *private_xform)
{
	struct qat_comp_xform *qat_xform =
			(struct qat_comp_xform *)private_xform;

	if (qat_xform) {
		memset(qat_xform, 0, qat_comp_xform_size());
		struct rte_mempool *mp = rte_mempool_from_obj(qat_xform);

		rte_mempool_put(mp, qat_xform);
		return 0;
	}
	return -EINVAL;
}
