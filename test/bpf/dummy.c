/* SPDX-License-Identifier: BSD-3-Clause
 * Copyright(c) 2018 Intel Corporation
 */

/*
 * eBPF program sample.
 * does nothing always return success.
 * used to measure BPF infrastructure overhead.
 * To compile:
 * clang -O2 -target bpf -c dummy.c
 */

#include <stdint.h>
#include <stddef.h>

uint64_t
entry(void *arg)
{
	return 1;
}
