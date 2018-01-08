# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 Cavium, Inc
#

#
# arch:
#
#   - define ARCH variable (overridden by cmdline or by previous
#     optional define in machine .mk)
#   - define CROSS variable (overridden by cmdline or previous define
#     in machine .mk)
#   - define CPU_CFLAGS variable (overridden by cmdline or previous
#     define in machine .mk)
#   - define CPU_LDFLAGS variable (overridden by cmdline or previous
#     define in machine .mk)
#   - define CPU_ASFLAGS variable (overridden by cmdline or previous
#     define in machine .mk)
#   - may override any previously defined variable
#
# examples for CONFIG_RTE_ARCH: i686, x86_64, x86_64_32
#

ARCH  ?= arm64
# common arch dir in eal headers
ARCH_DIR := arm
CROSS ?=

CPU_CFLAGS  ?=
CPU_LDFLAGS ?=
CPU_ASFLAGS ?= -felf

export ARCH CROSS CPU_CFLAGS CPU_LDFLAGS CPU_ASFLAGS

RTE_OBJCOPY_TARGET = elf64-littleaarch64
RTE_OBJCOPY_ARCH = aarch64

export RTE_OBJCOPY_TARGET RTE_OBJCOPY_ARCH
