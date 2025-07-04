/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2023 Marvell.
 */

#include <rte_mldev.h>

#include <mldev_utils.h>

#include <roc_api.h>

#include "cnxk_ml_io.h"

inline int
cnxk_ml_io_quantize_single(struct cnxk_ml_io *input, uint8_t *dbuffer, uint8_t *qbuffer)
{
	enum rte_ml_io_type qtype;
	enum rte_ml_io_type dtype;
	uint32_t nb_elements;
	float qscale;
	int ret = 0;

	dtype = input->dtype;
	qtype = input->qtype;
	qscale = input->scale;
	nb_elements = input->nb_elements;

	if (dtype == qtype) {
		rte_memcpy(qbuffer, dbuffer, input->sz_d);
		return ret;
	}

	switch (qtype) {
	case RTE_ML_IO_TYPE_INT8:
		ret = rte_ml_io_float32_to_int8(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT8:
		ret = rte_ml_io_float32_to_uint8(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_INT16:
		ret = rte_ml_io_float32_to_int16(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT16:
		ret = rte_ml_io_float32_to_uint16(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_INT32:
		ret = rte_ml_io_float32_to_int32(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT32:
		ret = rte_ml_io_float32_to_uint32(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_INT64:
		ret = rte_ml_io_float32_to_int64(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT64:
		ret = rte_ml_io_float32_to_uint64(dbuffer, qbuffer, nb_elements, 1.0 / qscale, 0);
		break;
	case RTE_ML_IO_TYPE_FP16:
		ret = rte_ml_io_float32_to_float16(dbuffer, qbuffer, nb_elements);
		break;
	default:
		plt_err("Unsupported qtype : %u", qtype);
		ret = -ENOTSUP;
	}

	return ret;
}

inline int
cnxk_ml_io_dequantize_single(struct cnxk_ml_io *output, uint8_t *qbuffer, uint8_t *dbuffer)
{
	enum rte_ml_io_type qtype;
	enum rte_ml_io_type dtype;
	uint32_t nb_elements;
	float dscale;
	int ret = 0;

	dtype = output->dtype;
	qtype = output->qtype;
	dscale = output->scale;
	nb_elements = output->nb_elements;

	if (dtype == qtype) {
		rte_memcpy(dbuffer, qbuffer, output->sz_q);
		return 0;
	}

	switch (qtype) {
	case RTE_ML_IO_TYPE_INT8:
		ret = rte_ml_io_int8_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT8:
		ret = rte_ml_io_uint8_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_INT16:
		ret = rte_ml_io_int16_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT16:
		ret = rte_ml_io_uint16_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_INT32:
		ret = rte_ml_io_int32_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT32:
		ret = rte_ml_io_uint32_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_INT64:
		ret = rte_ml_io_int64_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_UINT64:
		ret = rte_ml_io_uint64_to_float32(qbuffer, dbuffer, nb_elements, dscale, 0);
		break;
	case RTE_ML_IO_TYPE_FP16:
		ret = rte_ml_io_float16_to_float32(qbuffer, dbuffer, nb_elements);
		break;
	default:
		plt_err("Unsupported qtype: %u", qtype);
		ret = -ENOTSUP;
	}

	return ret;
}
