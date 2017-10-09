# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

APP = ipsec-secgw

#
# all source are stored in SRCS-y
#
SRCS-y += parser.c
SRCS-y += ipsec.c
SRCS-y += esp.c
SRCS-y += sp4.c
SRCS-y += sp6.c
SRCS-y += sa.c
SRCS-y += rt.c
SRCS-y += ipsec-secgw.c

CFLAGS += -gdwarf-2

# Build using pkg-config variables if possible
$(shell pkg-config --exists libdpdk)
ifeq ($(.SHELLSTATUS),0)

PC_FILE := $(shell pkg-config --path libdpdk)
CFLAGS += $(shell pkg-config --cflags libdpdk)
LDFLAGS += $(shell pkg-config --libs libdpdk)

CFLAGS += -DALLOW_EXPERIMENTAL_API

build/$(APP): $(SRCS-y) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP)
	rmdir --ignore-fail-on-non-empty build

else

ifeq ($(RTE_SDK),)
	$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(MAKECMDGOALS),clean)
ifneq ($(CONFIG_RTE_LIBRTE_SECURITY),y)
$(error "RTE_LIBRTE_SECURITY is required to build ipsec-secgw")
endif
endif

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -O3 -gdwarf-2
CFLAGS += $(WERROR_FLAGS)
ifeq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS_sa.o += -diag-disable=vec
endif

ifeq ($(DEBUG),1)
CFLAGS += -DIPSEC_DEBUG -fstack-protector-all -O0
endif

include $(RTE_SDK)/mk/rte.extapp.mk

endif
