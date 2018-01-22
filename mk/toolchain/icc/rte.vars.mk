# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

#
# toolchain:
#
#   - define CC, LD, AR, AS, ... (overridden by cmdline value)
#   - define TOOLCHAIN_CFLAGS variable (overridden by cmdline value)
#   - define TOOLCHAIN_LDFLAGS variable (overridden by cmdline value)
#   - define TOOLCHAIN_ASFLAGS variable (overridden by cmdline value)
#

# Warning: we do not use CROSS environment variable as icc is mainly a
# x86->x86 compiler

CC        = icc
KERNELCC  = gcc
CPP       = cpp
AS        = nasm
AR        = ar
LD        = ld
OBJCOPY   = objcopy
OBJDUMP   = objdump
STRIP     = strip
READELF   = readelf

ifeq ($(KERNELRELEASE),)
HOSTCC    = icc
else
HOSTCC    = gcc
endif
HOSTAS    = as

TOOLCHAIN_CFLAGS =
TOOLCHAIN_LDFLAGS =
TOOLCHAIN_ASFLAGS =

# Turn off some ICC warnings -
#   Remark #271   : trailing comma is nonstandard
#   Warning #1478 : function "<func_name>" (declared at line N of "<filename>")
#   error #13368: loop was not vectorized with "vector always assert"
#   error #15527: loop was not vectorized: function call to fprintf cannot be vectorize
#                   was declared "deprecated"
#   Warning #11074, 11076: to prevent "inline-max-size" warnings.
WERROR_FLAGS := -Wall -w2 -diag-disable 271 -diag-warning 1478
WERROR_FLAGS += -diag-disable 13368 -diag-disable 15527
WERROR_FLAGS += -diag-disable 188
WERROR_FLAGS += -diag-disable 11074 -diag-disable 11076 -Wdeprecated

ifeq ($(RTE_DEVEL_BUILD),y)
WERROR_FLAGS += -Werror-all
endif

# process cpu flags
include $(RTE_SDK)/mk/toolchain/$(RTE_TOOLCHAIN)/rte.toolchain-compat.mk

export CC AS AR LD OBJCOPY OBJDUMP STRIP READELF
export TOOLCHAIN_CFLAGS TOOLCHAIN_LDFLAGS TOOLCHAIN_ASFLAGS
