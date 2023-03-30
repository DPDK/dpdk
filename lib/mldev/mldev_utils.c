/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#include <errno.h>
#include <stdint.h>

#include <rte_mldev.h>
#include <rte_string_fns.h>

#include "mldev_utils.h"

/* Description:
 * This file implements Machine Learning utility routines, except type conversion routines.
 */

int
rte_ml_io_type_size_get(enum rte_ml_io_type type)
{
	switch (type) {
	case RTE_ML_IO_TYPE_UNKNOWN:
		return -EINVAL;
	case RTE_ML_IO_TYPE_INT8:
		return sizeof(int8_t);
	case RTE_ML_IO_TYPE_UINT8:
		return sizeof(uint8_t);
	case RTE_ML_IO_TYPE_INT16:
		return sizeof(int16_t);
	case RTE_ML_IO_TYPE_UINT16:
		return sizeof(uint16_t);
	case RTE_ML_IO_TYPE_INT32:
		return sizeof(int32_t);
	case RTE_ML_IO_TYPE_UINT32:
		return sizeof(uint32_t);
	case RTE_ML_IO_TYPE_FP8:
		return sizeof(uint8_t);
	case RTE_ML_IO_TYPE_FP16:
		return sizeof(uint8_t) * 2;
	case RTE_ML_IO_TYPE_FP32:
		return sizeof(uint8_t) * 4;
	case RTE_ML_IO_TYPE_BFLOAT16:
		return sizeof(uint8_t) * 2;
	default:
		return -EINVAL;
	}
}

void
rte_ml_io_type_to_str(enum rte_ml_io_type type, char *str, int len)
{
	switch (type) {
	case RTE_ML_IO_TYPE_UNKNOWN:
		rte_strlcpy(str, "unknown", len);
		break;
	case RTE_ML_IO_TYPE_INT8:
		rte_strlcpy(str, "int8", len);
		break;
	case RTE_ML_IO_TYPE_UINT8:
		rte_strlcpy(str, "uint8", len);
		break;
	case RTE_ML_IO_TYPE_INT16:
		rte_strlcpy(str, "int16", len);
		break;
	case RTE_ML_IO_TYPE_UINT16:
		rte_strlcpy(str, "uint16", len);
		break;
	case RTE_ML_IO_TYPE_INT32:
		rte_strlcpy(str, "int32", len);
		break;
	case RTE_ML_IO_TYPE_UINT32:
		rte_strlcpy(str, "uint32", len);
		break;
	case RTE_ML_IO_TYPE_FP8:
		rte_strlcpy(str, "float8", len);
		break;
	case RTE_ML_IO_TYPE_FP16:
		rte_strlcpy(str, "float16", len);
		break;
	case RTE_ML_IO_TYPE_FP32:
		rte_strlcpy(str, "float32", len);
		break;
	case RTE_ML_IO_TYPE_BFLOAT16:
		rte_strlcpy(str, "bfloat16", len);
		break;
	default:
		rte_strlcpy(str, "invalid", len);
	}
}

void
rte_ml_io_format_to_str(enum rte_ml_io_format format, char *str, int len)
{
	switch (format) {
	case RTE_ML_IO_FORMAT_NCHW:
		rte_strlcpy(str, "NCHW", len);
		break;
	case RTE_ML_IO_FORMAT_NHWC:
		rte_strlcpy(str, "NHWC", len);
		break;
	case RTE_ML_IO_FORMAT_CHWN:
		rte_strlcpy(str, "CHWN", len);
		break;
	case RTE_ML_IO_FORMAT_3D:
		rte_strlcpy(str, "3D", len);
		break;
	case RTE_ML_IO_FORMAT_2D:
		rte_strlcpy(str, "Matrix", len);
		break;
	case RTE_ML_IO_FORMAT_1D:
		rte_strlcpy(str, "Vector", len);
		break;
	case RTE_ML_IO_FORMAT_SCALAR:
		rte_strlcpy(str, "Scalar", len);
		break;
	default:
		rte_strlcpy(str, "invalid", len);
	}
}
