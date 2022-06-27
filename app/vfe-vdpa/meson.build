# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2022, NVIDIA CORPORATION & AFFILIATES.

if has_libev == 0
	build = false
	reason = 'missing dependency, "libev"'
	subdir_done()
endif

sources = files('cJSON.c', 'jsonrpc-c.c', 'jsonrpc-client.c', 'main.c', 'vdpa_rpc.c')
headers = files('cJSON.h', 'jsonrpc-c.h', 'jsonrpc-client.h', 'vdpa_rpc.h')
deps += ['vhost', 'ethdev', 'cmdline', 'vdpa_virtio', 'common_virtio', 'common_virtio_mi']
