# SPDX-License-Identifier: BSD-3-Clause
# Copyright (C) 2015 RehiveTech. All rights reserved.

ARCH  ?= arm
CROSS ?=

CPU_CFLAGS  ?= -marm -munaligned-access -D_FILE_OFFSET_BITS=64
CPU_LDFLAGS ?=
CPU_ASFLAGS ?= -felf

export ARCH CROSS CPU_CFLAGS CPU_LDFLAGS CPU_ASFLAGS

RTE_OBJCOPY_TARGET = elf32-littlearm
RTE_OBJCOPY_ARCH = arm

export RTE_OBJCOPY_TARGET RTE_OBJCOPY_ARCH
