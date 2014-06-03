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
# directory where sources are located
#
ifdef S
ifeq ("$(origin S)", "command line")
RTE_SRCDIR := $(abspath $(S))
endif
endif
RTE_SRCDIR  ?= $(CURDIR)
export RTE_SRCDIR

#
# Makefile to call once $(RTE_OUTPUT) is created
#
ifdef M
ifeq ("$(origin M)", "command line")
RTE_EXTMK := $(abspath $(M))
endif
endif
RTE_EXTMK ?= $(RTE_SRCDIR)/Makefile
export RTE_EXTMK

RTE_SDK_BIN := $(RTE_SDK)/$(RTE_TARGET)

#
# Output files wil go in a separate directory: default output is
# $(RTE_SRCDIR)/build
# Output dir can be given as command line using "O="
#
ifdef O
ifeq ("$(origin O)", "command line")
RTE_OUTPUT := $(abspath $(O))
endif
endif
RTE_OUTPUT ?= $(RTE_SRCDIR)/build
export RTE_OUTPUT

# if we are building an external application, include SDK
# configuration and include project configuration if any
include $(RTE_SDK_BIN)/.config
ifneq ($(wildcard $(RTE_OUTPUT)/.config),)
  include $(RTE_OUTPUT)/.config
endif
# remove double-quotes from config names
RTE_ARCH := $(CONFIG_RTE_ARCH:"%"=%)
RTE_MACHINE := $(CONFIG_RTE_MACHINE:"%"=%)
RTE_EXEC_ENV := $(CONFIG_RTE_EXEC_ENV:"%"=%)
RTE_TOOLCHAIN := $(CONFIG_RTE_TOOLCHAIN:"%"=%)


