# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

default: all

ifeq ($(CONFIG_RTE_BUILD_SHARED_LIB),y)
EXT:=.so
else
EXT:=.a
endif

RTE_LIBNAME := dpdk
COMBINEDLIB := lib$(RTE_LIBNAME)$(EXT)

LIBS := $(filter-out $(COMBINEDLIB), $(sort $(notdir $(wildcard $(RTE_OUTPUT)/lib/*$(EXT)))))

all: FORCE
	$(Q)echo "GROUP ( $(LIBS) )" > $(RTE_OUTPUT)/lib/$(COMBINEDLIB)

#
# Clean all generated files
#
.PHONY: clean
clean:
	$(Q)rm -f $(RTE_OUTPUT)/lib/$(COMBINEDLIB)

.PHONY: FORCE
FORCE:
