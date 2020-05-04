# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

extra_flags = []

foreach flag: extra_flags
	if cc.has_argument(flag)
		cflags += flag
	endif
endforeach

use_function_versioning = true

sources = files('rte_mempool.c', 'rte_mempool_ops.c',
		'rte_mempool_ops_default.c', 'mempool_trace_points.c')
headers = files('rte_mempool.h', 'rte_mempool_trace.h',
		'rte_mempool_trace_fp.h')
deps += ['ring']
