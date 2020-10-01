/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2020 Intel Corporation
 */
#ifndef __INCLUDE_RTE_SWX_EXTERN_H__
#define __INCLUDE_RTE_SWX_EXTERN_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE SWX Extern objects and functions
 *
 * Extern object and extern function interfaces. The extern objects and extern
 * functions provide the mechanisms to hook external functionality into the
 * packet processing pipeline.
 */

#include <stdint.h>

/*
 * Extern type
 */

/**
 * Extern object constructor
 *
 * @param[in] args
 *   Extern object constructor arguments. It may be NULL.
 * @return
 *   Extern object handle.
 */
typedef void *
(*rte_swx_extern_type_constructor_t)(const char *args);

/**
 * Extern object destructor
 *
 * @param[in] object
 *   Extern object handle.
 */
typedef void
(*rte_swx_extern_type_destructor_t)(void *object);

/**
 * Extern object member function
 *
 * The mailbox is used to pass input arguments to the member function and
 * retrieve the output results. The mailbox mechanism allows for multiple
 * concurrent executions of the same member function for the same extern object.
 *
 * Multiple invocations of the same member function may be required in order for
 * the associated operation to complete. The completion is flagged by a return
 * value of 1, in which case the results are available in the mailbox; in case
 * of a return value of 0, the operation is not yet completed, so the member
 * function must be invoked again with exactly the same object and mailbox
 * arguments.
 *
 * @param[in] object
 *   Extern object handle.
 * @param[in] mailbox
 *   Extern object mailbox.
 * @return
 *   0 when the operation is not yet completed, and 1 when the operation is
 *   completed. No other return values are allowed.
 */
typedef int
(*rte_swx_extern_type_member_func_t)(void *object, void *mailbox);

/*
 * Extern function
 */

/** The mailbox is used to pass input arguments to the extern function and
 * retrieve the output results. The mailbox mechanism allows for multiple
 * concurrent executions of the same extern function.
 *
 * Multiple invocations of the same extern function may be required in order for
 * the associated operation to complete. The completion is flagged by a return
 * value of 1, in which case the results are available in the mailbox; in case
 * of a return value of 0, the operation is not yet completed, so the extern
 * function must be invoked again with exactly the same mailbox argument.
 *
 * @param[in] mailbox
 *   Extern object mailbox.
 * @return
 *   0 when the operation is not yet completed, and 1 when the operation is
 *   completed. No other return values are allowed.
 */
typedef int
(*rte_swx_extern_func_t)(void *mailbox);

#ifdef __cplusplus
}
#endif

#endif
