/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2010-2014 Intel Corporation
 */

#ifndef __INCLUDE_RTE_APPROX_H__
#define __INCLUDE_RTE_APPROX_H__

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @file
 * RTE Rational Approximation
 *
 * Given a rational number alpha with 0 < alpha < 1 and a precision d, the goal
 * is to find positive integers p, q such that alpha - d < p/q < alpha + d, and
 * q is minimal.
 */

#include <stdint.h>

/**
 * Find best rational approximation
 *
 * @param alpha
 *   Rational number to approximate
 * @param d
 *   Precision for the rational approximation
 * @param p
 *   Pointer to pre-allocated space where the numerator of the rational
 *   approximation will be stored when operation is successful
 * @param q
 *   Pointer to pre-allocated space where the denominator of the rational
 *   approximation will be stored when operation is successful
 * @return
 *   0 upon success, error code otherwise
 */
int rte_approx(double alpha, double d, uint32_t *p, uint32_t *q);

/**
 * Find best rational approximation (64 bit version)
 *
 * @param alpha
 *   Rational number to approximate
 * @param d
 *   Precision for the rational approximation
 * @param p
 *   Pointer to pre-allocated space where the numerator of the rational
 *   approximation will be stored when operation is successful
 * @param q
 *   Pointer to pre-allocated space where the denominator of the rational
 *   approximation will be stored when operation is successful
 * @return
 *   0 upon success, error code otherwise
 */
int rte_approx_64(double alpha, double d, uint64_t *p, uint64_t *q);

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_APPROX_H__ */
