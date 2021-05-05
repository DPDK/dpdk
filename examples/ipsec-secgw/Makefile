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
SRCS-y += sad.c
SRCS-y += rt.c
SRCS-y += ipsec_process.c
SRCS-y += ipsec-secgw.c
SRCS-y += ipsec_worker.c
SRCS-y += event_helper.c
SRCS-y += flow.c

CFLAGS += -gdwarf-2

PKGCONF ?= pkg-config

# Build using pkg-config variables if possible
ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
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
CFLAGS += -Wno-address-of-packed-member

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
