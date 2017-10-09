# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2016 Intel Corporation

# binary name
APP = l2fwd-cat

# all source are stored in SRCS-y
SRCS-y := l2fwd-cat.c cat.c

# Build using pkg-config variables if possible
$(shell pkg-config --exists libdpdk)
ifeq ($(.SHELLSTATUS),0)

PC_FILE := $(shell pkg-config --path libdpdk)
CFLAGS += $(shell pkg-config --cflags libdpdk)
LDFLAGS += $(shell pkg-config --libs libdpdk)

CFLAGS += -D_GNU_SOURCE
LDFLAGS += -lpqos

build/$(APP): $(SRCS-y) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP)
	rmdir --ignore-fail-on-non-empty build

else # Build using legacy build system

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

ifeq ($(PQOS_INSTALL_PATH),)
$(error "Please define PQOS_INSTALL_PATH environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

CFLAGS += $(WERROR_FLAGS)

# workaround for a gcc bug with noreturn attribute
# http://gcc.gnu.org/bugzilla/show_bug.cgi?id=12603
ifeq ($(CONFIG_RTE_TOOLCHAIN_GCC),y)
CFLAGS_main.o += -Wno-return-type
endif

EXTRA_CFLAGS += -O3 -g -Wfatal-errors

CFLAGS += -I$(PQOS_INSTALL_PATH)/../include
CFLAGS_cat.o := -D_GNU_SOURCE

LDLIBS += -L$(PQOS_INSTALL_PATH)
LDLIBS += -lpqos

include $(RTE_SDK)/mk/rte.extapp.mk
endif
