/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
 *   All rights reserved.
 *
 *   Redistribution and use in source and binary forms, with or without
 *   modification, are permitted provided that the following conditions
 *   are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *     * Neither the name of Intel Corporation nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 *   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 *   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 *   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 *   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 *   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 *   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
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
 *
 ***/

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

#ifdef __cplusplus
}
#endif

#endif /* __INCLUDE_RTE_APPROX_H__ */
