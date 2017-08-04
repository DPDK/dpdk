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

MAKEFLAGS += --no-print-directory

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

ifeq ($(RTE_SDK),)
$(error RTE_SDK is not defined)
endif

RTE_SRCDIR = $(CURDIR)
export RTE_SRCDIR

BUILDING_RTE_SDK := 1
export BUILDING_RTE_SDK

#
# We can specify the configuration template when doing the "make
# config". For instance: make config T=x86_64-native-linuxapp-gcc
#
RTE_CONFIG_TEMPLATE :=
ifdef T
ifeq ("$(origin T)", "command line")
RTE_CONFIG_TEMPLATE := $(RTE_SRCDIR)/config/defconfig_$(T)
endif
endif
export RTE_CONFIG_TEMPLATE

#
# Default output is $(RTE_SRCDIR)/build
# output files wil go in a separate directory
#
ifdef O
ifeq ("$(origin O)", "command line")
RTE_OUTPUT := $(abspath $(O))
endif
endif
RTE_OUTPUT ?= $(RTE_SRCDIR)/build
export RTE_OUTPUT

# the directory where intermediate build files are stored, like *.o,
# *.d, *.cmd, ...
BUILDDIR = $(RTE_OUTPUT)/build
export BUILDDIR

export ROOTDIRS-y ROOTDIRS- ROOTDIRS-n

.PHONY: default
default: all

.PHONY: config defconfig showconfigs showversion showversionum
config defconfig showconfigs showversion showversionum:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkconfig.mk $@

.PHONY: cscope gtags tags etags
cscope gtags tags etags:
	$(Q)$(RTE_SDK)/devtools/build-tags.sh $@ $T

.PHONY: test test-basic test-fast test-ring test-mempool test-perf coverage
test test-basic test-fast test-ring test-mempool test-perf coverage:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdktest.mk $@

test: test-build

.PHONY: install
install:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkinstall.mk pre_install
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkinstall.mk $@
install-%:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkinstall.mk $@

.PHONY: doc help
doc: doc-all
help: doc-help
doc-%:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkdoc.mk $*

.PHONY: gcov gcovclean
gcov gcovclean:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkgcov.mk $@

.PHONY: examples examples_clean
examples examples_clean:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkexamples.mk $@

# all other build targets
%:
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkconfig.mk checkconfig
	$(Q)$(MAKE) -f $(RTE_SDK)/mk/rte.sdkbuild.mk $@
