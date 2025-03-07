/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2023 Ericsson AB
 */

#include <errno.h>
#include <stdbool.h>
#include <stdint.h>
#include <sys/types.h>

#include <eal_export.h>
#include "rte_bitset.h"

RTE_EXPORT_EXPERIMENTAL_SYMBOL(rte_bitset_to_str, 24.11)
ssize_t
rte_bitset_to_str(const uint64_t *bitset, size_t num_bits, char *buf, size_t capacity)
{
	size_t i;

	if (capacity < (num_bits + 1))
		return -EINVAL;

	for (i = 0; i < num_bits; i++) {
		bool value;

		value = rte_bitset_test(bitset, num_bits - 1 - i);
		buf[i] = value ? '1' : '0';
	}

	buf[num_bits] = '\0';

	return num_bits + 1;
}
