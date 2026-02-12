# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2020 Intel Corporation

PKGCONF ?= pkg-config

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif

# binary name
APP = vm_power_mgr

# all source are stored in SRCS-y
SRCS-y := main.c vm_power_cli.c power_manager.c channel_manager.c
SRCS-y += channel_monitor.c parse.c
ifeq ($(shell uname -m),x86_64)
SRCS-y += oob_monitor_x86.c
else
SRCS-y += oob_monitor_nop.c
endif

all: shared
.PHONY: shared static
shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)
static: build/$(APP)-static
	ln -sf $(APP)-static build/$(APP)

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS_STATIC = $(shell $(PKGCONF) --static --libs libdpdk)

CFLAGS += -DALLOW_EXPERIMENTAL_API

ifneq ($(shell $(PKGCONF) --atleast-version=0.9.3 libvirt; echo $$?), 0)
$(error vm_power_manager requires libvirt >= 0.9.3)
endif
LDFLAGS += $(shell $(PKGCONF) --libs libvirt)

JANSSON := $(shell $(PKGCONF) --exists jansson; echo $$?)
ifeq ($(JANSSON), 0)
LDFLAGS += $(shell $(PKGCONF) --libs jansson)
CFLAGS += -DUSE_JANSSON
endif

# for shared library builds, we need to explicitly link these PMDs
LDFLAGS_SHARED += -lrte_net_ixgbe -lrte_net_i40e -lrte_net_bnxt

build/$(APP)-shared: $(SRCS-y) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/$(APP)-static: $(SRCS-y) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) $(SRCS-y) -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP) build/$(APP)-static build/$(APP)-shared
	test -d build && rmdir -p build || true
