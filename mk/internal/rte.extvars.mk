# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

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
RTE_EXTMK ?= $(RTE_SRCDIR)/$(notdir $(firstword $(MAKEFILE_LIST)))
export RTE_EXTMK

# RTE_SDK_BIN must point to .config, include/ and lib/.
RTE_SDK_BIN := $(RTE_SDK)/$(RTE_TARGET)
ifeq ($(wildcard $(RTE_SDK_BIN)/.config),)
$(error Cannot find .config in $(RTE_SDK_BIN))
endif

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
