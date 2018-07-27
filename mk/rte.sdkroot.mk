# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

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

.PHONY: test test-fast test-perf coverage test-drivers test-dump
test test-fast test-perf coverage test-drivers test-dump:
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
