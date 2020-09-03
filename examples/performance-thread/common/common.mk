# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015 Intel Corporation

# list the C files belonging to the lthread subsystem, these are common to all
# lthread apps. Any makefile including this should set VPATH to include this
# directory path
#

MKFILE_PATH=$(abspath $(dir $(lastword $(MAKEFILE_LIST))))

ifeq ($(shell uname -m),x86_64)
ARCH_PATH += $(MKFILE_PATH)/arch/x86
else ifeq ($(shell uname -m),arm64)
ARCH_PATH += $(MKFILE_PATH)/arch/arm64
endif

VPATH := $(MKFILE_PATH) $(ARCH_PATH)

SRCS-y += lthread.c lthread_sched.c lthread_cond.c lthread_tls.c lthread_mutex.c lthread_diag.c ctx.c

CFLAGS += -I$(MKFILE_PATH) -I$(ARCH_PATH)
