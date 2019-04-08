# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if not is_linux
	build = false
endif

if not cc.has_header('linux/virtio_scsi.h')
	build = false
endif

deps += 'vhost'
cflags += ['-D_FILE_OFFSET_BITS=64']
sources = files(
	'scsi.c', 'vhost_scsi.c'
)
