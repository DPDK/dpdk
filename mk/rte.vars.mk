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
# To be included at the beginning of all RTE user Makefiles. This
# .mk will define the RTE environment variables by including the
# config file of SDK. It also includes the config file from external
# application if any.
#

ifeq ($(RTE_SDK),)
$(error RTE_SDK is not defined)
endif
ifeq ($(wildcard $(RTE_SDK)),)
$(error RTE_SDK variable points to an invalid location)
endif

# define Q to '@' or not. $(Q) is used to prefix all shell commands to
# be executed silently.
Q=@
ifeq '$V' '0'
override V=
endif
ifdef V
ifeq ("$(origin V)", "command line")
Q=
endif
endif
export Q

# if we are building SDK, only includes SDK configuration
ifneq ($(BUILDING_RTE_SDK),)
  include $(RTE_OUTPUT)/.config
  # remove double-quotes from config names
  RTE_ARCH := $(CONFIG_RTE_ARCH:"%"=%)
  RTE_MACHINE := $(CONFIG_RTE_MACHINE:"%"=%)
  RTE_EXEC_ENV := $(CONFIG_RTE_EXEC_ENV:"%"=%)
  RTE_TOOLCHAIN := $(CONFIG_RTE_TOOLCHAIN:"%"=%)
  RTE_SDK_BIN := $(RTE_OUTPUT)
endif

RTE_TARGET ?= $(RTE_ARCH)-$(RTE_MACHINE)-$(RTE_EXEC_ENV)-$(RTE_TOOLCHAIN)

ifeq ($(BUILDING_RTE_SDK),)
# if we are building an external app/lib, include internal/rte.extvars.mk that will
# define RTE_OUTPUT, RTE_SRCDIR, RTE_EXTMK, RTE_SDK_BIN, (etc ...)
include $(RTE_SDK)/mk/internal/rte.extvars.mk
endif

CONFIG_RTE_LIBRTE_E1000_PMD = $(CONFIG_RTE_LIBRTE_IGB_PMD)
ifneq ($(CONFIG_RTE_LIBRTE_E1000_PMD),y)
  CONFIG_RTE_LIBRTE_E1000_PMD = $(CONFIG_RTE_LIBRTE_EM_PMD)
endif

ifeq ($(RTE_ARCH),)
$(error RTE_ARCH is not defined)
endif

ifeq ($(RTE_MACHINE),)
$(error RTE_MACHINE is not defined)
endif

ifeq ($(RTE_EXEC_ENV),)
$(error RTE_EXEC_ENV is not defined)
endif

ifeq ($(RTE_TOOLCHAIN),)
$(error RTE_TOOLCHAIN is not defined)
endif

# can be overridden by make command line or exported environment variable
RTE_KERNELDIR ?= /lib/modules/$(shell uname -r)/build

export RTE_TARGET
export RTE_ARCH
export RTE_MACHINE
export RTE_EXEC_ENV
export RTE_TOOLCHAIN

# developer build automatically enabled in a git tree
ifneq ($(wildcard $(RTE_SDK)/.git),)
RTE_DEVEL_BUILD ?= y
endif

# SRCDIR is the current source directory
ifdef S
SRCDIR := $(abspath $(RTE_SRCDIR)/$(S))
else
SRCDIR := $(RTE_SRCDIR)
endif

# helper: return y if option is set to y, else return an empty string
testopt = $(if $(strip $(subst y,,$(1)) $(subst $(1),,y)),,y)

# helper: return an empty string if option is set, else return y
not = $(if $(strip $(subst y,,$(1)) $(subst $(1),,y)),,y)

ifneq ($(wildcard $(RTE_SDK)/mk/target/$(RTE_TARGET)/rte.vars.mk),)
include $(RTE_SDK)/mk/target/$(RTE_TARGET)/rte.vars.mk
else
include $(RTE_SDK)/mk/target/generic/rte.vars.mk
endif
