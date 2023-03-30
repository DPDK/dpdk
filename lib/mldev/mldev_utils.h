/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef RTE_MLDEV_UTILS_H
#define RTE_MLDEV_UTILS_H

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 *
 * ML Device PMD utility API
 *
 * These APIs for the use from ML drivers, user applications shouldn't use them.
 *
 */

#include <rte_compat.h>
#include <rte_mldev.h>

/**
 * @internal
 *
 * Get the size an ML IO type in bytes.
 *
 * @param[in] type
 *	Enumeration of ML IO data type.
 *
 * @return
 *	- > 0, Size of the data type in bytes.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_type_size_get(enum rte_ml_io_type type);

/**
 * @internal
 *
 * Get the name of an ML IO type.
 *
 * @param[in] type
 *	Enumeration of ML IO data type.
 * @param[in] str
 *	Address of character array.
 * @param[in] len
 *	Length of character array.
 */
__rte_internal
void
rte_ml_io_type_to_str(enum rte_ml_io_type type, char *str, int len);

/**
 * @internal
 *
 * Get the name of an ML IO format.
 *
 * @param[in] type
 *	Enumeration of ML IO format.
 * @param[in] str
 *	Address of character array.
 * @param[in] len
 *	Length of character array.
 */
__rte_internal
void
rte_ml_io_format_to_str(enum rte_ml_io_format format, char *str, int len);

/**
 * @internal
 *
 * Convert a buffer containing numbers in single precision floating format (float32) to signed 8-bit
 * integer format (INT8).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 * @param[out] output
 *	Output buffer to store INT8 numbers. Size of buffer is equal to (nb_elements * 1) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_float32_to_int8(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in signed 8-bit integer format (INT8) to single precision
 * floating format (float32).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing INT8 numbers. Size of buffer is equal to (nb_elements * 1) bytes.
 * @param[out] output
 *	Output buffer to store float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_int8_to_float32(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in single precision floating format (float32) to unsigned
 * 8-bit integer format (UINT8).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 * @param[out] output
 *	Output buffer to store UINT8 numbers. Size of buffer is equal to (nb_elements * 1) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_float32_to_uint8(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in unsigned 8-bit integer format (UINT8) to single precision
 * floating format (float32).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing UINT8 numbers. Size of buffer is equal to (nb_elements * 1) bytes.
 * @param[out] output
 *	Output buffer to store float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_uint8_to_float32(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in single precision floating format (float32) to signed
 * 16-bit integer format (INT16).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 * @param[out] output
 *	Output buffer to store INT16 numbers. Size of buffer is equal to (nb_elements * 2) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_float32_to_int16(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in signed 16-bit integer format (INT16) to single precision
 * floating format (float32).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing INT16 numbers. Size of buffer is equal to (nb_elements * 2) bytes.
 * @param[out] output
 *	Output buffer to store float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_int16_to_float32(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in single precision floating format (float32) to unsigned
 * 16-bit integer format (UINT16).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 * @param[out] output
 *	Output buffer to store UINT16 numbers. Size of buffer is equal to (nb_elements * 2) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_float32_to_uint16(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in unsigned 16-bit integer format (UINT16) to single
 * precision floating format (float32).
 *
 * @param[in] scale
 *      Scale factor for conversion.
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing UINT16 numbers. Size of buffer is equal to (nb_elements * 2) bytes.
 * @param[out] output
 *	Output buffer to store float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_uint16_to_float32(float scale, uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in single precision floating format (float32) to half
 * precision floating point format (FP16).
 *
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing float32 numbers. Size of buffer is equal to (nb_elements *4) bytes.
 * @param[out] output
 *	Output buffer to store float16 numbers. Size of buffer is equal to (nb_elements * 2) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_float32_to_float16(uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in half precision floating format (FP16) to single precision
 * floating point format (float32).
 *
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing float16 numbers. Size of buffer is equal to (nb_elements * 2) bytes.
 * @param[out] output
 *	Output buffer to store float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_float16_to_float32(uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in single precision floating format (float32) to brain
 * floating point format (bfloat16).
 *
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing float32 numbers. Size of buffer is equal to (nb_elements *4) bytes.
 * @param[out] output
 *	Output buffer to store bfloat16 numbers. Size of buffer is equal to (nb_elements * 2) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_float32_to_bfloat16(uint64_t nb_elements, void *input, void *output);

/**
 * @internal
 *
 * Convert a buffer containing numbers in brain floating point format (bfloat16) to single precision
 * floating point format (float32).
 *
 * @param[in] nb_elements
 *	Number of elements in the buffer.
 * @param[in] input
 *	Input buffer containing bfloat16 numbers. Size of buffer is equal to (nb_elements * 2)
 * bytes.
 * @param[out] output
 *	Output buffer to store float32 numbers. Size of buffer is equal to (nb_elements * 4) bytes.
 *
 * @return
 *	- 0, Success.
 *	- < 0, Error code on failure.
 */
__rte_internal
int
rte_ml_io_bfloat16_to_float32(uint64_t nb_elements, void *input, void *output);

#ifdef __cplusplus
}
#endif

#endif /* RTE_MLDEV_UTILS_H */
