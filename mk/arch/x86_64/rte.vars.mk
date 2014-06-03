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
# arch:
#
#   - define ARCH variable (overriden by cmdline or by previous
#     optional define in machine .mk)
#   - define CROSS variable (overriden by cmdline or previous define
#     in machine .mk)
#   - define CPU_CFLAGS variable (overriden by cmdline or previous
#     define in machine .mk)
#   - define CPU_LDFLAGS variable (overriden by cmdline or previous
#     define in machine .mk)
#   - define CPU_ASFLAGS variable (overriden by cmdline or previous
#     define in machine .mk)
#   - may override any previously defined variable
#
# examples for CONFIG_RTE_ARCH: i686, x86_64, x86_64_32
#

ARCH  ?= x86_64
CROSS ?=

CPU_CFLAGS  ?= -m64
CPU_LDFLAGS ?=
CPU_ASFLAGS ?= -felf64

export ARCH CROSS CPU_CFLAGS CPU_LDFLAGS CPU_ASFLAGS
