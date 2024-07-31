/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright (c) 2022 Marvell.
 */

#ifndef RTE_MLDEV_UTILS_H
#define RTE_MLDEV_UTILS_H

/**
 * @file
 *
 * ML Device PMD utility API
 *
 * These APIs for the use from ML drivers, user applications shouldn't use them.
 */

#include <rte_compat.h>
#include <rte_mldev.h>

#ifdef __cplusplus
extern "C" {
#endif

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

#ifdef __cplusplus
}
#endif

#endif /* RTE_MLDEV_UTILS_H */
