#
#   BSD LICENSE
#
#  Copyright(c) 2015 Intel Corporation. All rights reserved.
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

# list the C files belonging to the lthread subsystem, these are common to all
# lthread apps. Any makefile including this should set VPATH to include this
# directory path
#

MKFILE_PATH=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))

ifeq ($(CONFIG_RTE_ARCH_X86_64),y)
ARCH_PATH += $(MKFILE_PATH)/arch/x86
else ifeq ($(CONFIG_RTE_ARCH_ARM64),y)
ARCH_PATH += $(MKFILE_PATH)/arch/arm64
endif

VPATH := $(MKFILE_PATH) $(ARCH_PATH)

SRCS-y += lthread.c lthread_sched.c lthread_cond.c lthread_tls.c lthread_mutex.c lthread_diag.c ctx.c

INCLUDES += -I$(MKFILE_PATH) -I$(ARCH_PATH)
