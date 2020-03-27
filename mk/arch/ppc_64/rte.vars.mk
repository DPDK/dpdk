# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) IBM Corporation 2014.

ARCH  ?= powerpc
ARCH_DIR := ppc
CROSS ?=

CPU_CFLAGS  ?= -m64
CPU_LDFLAGS ?=
CPU_ASFLAGS ?= -felf64

export ARCH CROSS CPU_CFLAGS CPU_LDFLAGS CPU_ASFLAGS

RTE_OBJCOPY_TARGET = elf64-powerpcle
RTE_OBJCOPY_ARCH = powerpc:common64

export RTE_OBJCOPY_TARGET RTE_OBJCOPY_ARCH
