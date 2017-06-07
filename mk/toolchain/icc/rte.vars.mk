#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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
WERROR_FLAGS += -diag-disable 11074 -diag-disable 11076

ifeq ($(RTE_DEVEL_BUILD),y)
WERROR_FLAGS += -Werror-all
endif

# process cpu flags
include $(RTE_SDK)/mk/toolchain/$(RTE_TOOLCHAIN)/rte.toolchain-compat.mk

export CC AS AR LD OBJCOPY OBJDUMP STRIP READELF
export TOOLCHAIN_CFLAGS TOOLCHAIN_LDFLAGS TOOLCHAIN_ASFLAGS
