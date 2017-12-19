# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk
unexport RTE_SRCDIR RTE_OUTPUT RTE_EXTMK

DIRS-y += bridge

.PHONY: all clean $(DIRS-y)

all: $(DIRS-y)
clean: $(DIRS-y)

$(DIRS-y):
	$(MAKE) -C $@ $(MAKECMDGOALS) O=$(RTE_OUTPUT)
