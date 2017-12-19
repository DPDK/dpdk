# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

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

ARCH  ?= x86_64
ARCH_DIR := x86
CROSS ?=

CPU_CFLAGS  ?= -mx32
CPU_LDFLAGS ?= -melf32_x86_64
#CPU_ASFLAGS ?= -felf64
# x32 is supported by Linux distribution with gcc4.8 and newer in some
# cases there is backported support in gcc4.6
ifneq ($(shell echo | $(CC) $(CPU_CFLAGS) -E - 2>/dev/null 1>/dev/null && echo 0), 0)
	$(error This version of GCC does not support x32 ABI)
endif

export ARCH CROSS CPU_CFLAGS CPU_LDFLAGS CPU_ASFLAGS

RTE_OBJCOPY_TARGET = elf32-x86-64
RTE_OBJCOPY_ARCH = i386:x86-64

export RTE_OBJCOPY_TARGET RTE_OBJCOPY_ARCH
