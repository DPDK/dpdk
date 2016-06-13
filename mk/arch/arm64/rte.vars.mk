#   BSD LICENSE
#
#   Copyright (C) Cavium networks 2015. All rights reserved.
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
#     * Neither the name of Cavium networks nor the names of its
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
