# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017 Intel Corporation

# meson file, for building this example as part of a main DPDK build.
#
# To build this example as a standalone application with an already-installed
# DPDK instance, use 'make'

if host_machine.system() != 'linux'
	build = false
endif
deps += 'vhost'
cflags += ['-D_GNU_SOURCE','-D_FILE_OFFSET_BITS=64']
sources = files(
	'scsi.c', 'vhost_scsi.c'
)
