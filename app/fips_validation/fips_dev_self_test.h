/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2019 Intel Corporation
 */

#ifndef _CRYPTO_PMD_SELF_TEST_H_
#define _CRYPTO_PMD_SELF_TEST_H_

#include <rte_crypto_sym.h>

enum fips_dev_self_test_dir {
	self_test_dir_enc_auth_gen = 0,
	self_test_dir_dec_auth_verify,
	self_test_dir_max
};

struct fips_dev_broken_test_config {
	uint32_t expect_fail_test_idx;
	enum fips_dev_self_test_dir expect_fail_dir;
};

int
fips_dev_self_test(uint8_t dev_id,
		struct fips_dev_broken_test_config *config);

#endif /* _CRYPTO_PMD_SELF_TEST_H_ */
