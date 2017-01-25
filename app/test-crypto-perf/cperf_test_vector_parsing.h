/*-
 *   BSD LICENSE
 *
 *   Copyright(c) 2016-2017 Intel Corporation. All rights reserved.
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

#ifndef APP_CRYPTO_PERF_CPERF_TEST_VECTOR_PARSING_H_
#define APP_CRYPTO_PERF_CPERF_TEST_VECTOR_PARSING_H_

#define CPERF_VALUE_DELIMITER ","
#define CPERF_ENTRY_DELIMITER "="

/**
 * Frees the allocated memory for test vector
 *
 * @param vector
 *   Destination vector test to release
 * @param opts
 *   Test options
 * @return
 *   0 on success, (-1) on error.
 */
int
free_test_vector(struct cperf_test_vector *vector, struct cperf_options *opts);

/**
 * Displays data in test vector
 *
 * @param vector
 *   Vector to display
 */
void
show_test_vector(struct cperf_test_vector *test_vector);

/**
 * Completes test vector with data from file
 *
 * @param opts
 *   Test options
 * @return
 *   NULL on error.
 *   Test vector pointer on successful.
 */
struct cperf_test_vector*
cperf_test_vector_get_from_file(struct cperf_options *opts);

#endif /* APP_CRYPTO_PERF_CPERF_TEST_VECTOR_PARSING_H_ */
