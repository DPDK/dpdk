# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 NXP

allow_experimental_apis = true

if host_machine.system() != 'linux'
        build = false
endif

sources = files('dpaax_iova_table.c')

cflags += ['-D_GNU_SOURCE']
