# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2018 Intel Corporation

# binary name
APP = ip_pipeline

# all source are stored in SRCS-y
SRCS-y := action.c
SRCS-y += cli.c
SRCS-y += conn.c
SRCS-y += link.c
SRCS-y += main.c
SRCS-y += mempool.c
SRCS-y += parser.c
SRCS-y += pipeline.c
SRCS-y += swq.c
SRCS-y += tap.c
SRCS-y += thread.c
SRCS-y += tmgr.c
SRCS-y += cryptodev.c

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

ifeq ($(MAKECMDGOALS),static)
# check for broken pkg-config
ifeq ($(shell echo $(LDFLAGS_STATIC) | grep 'whole-archive.*l:lib.*no-whole-archive'),)
$(warning "pkg-config output list does not contain drivers between 'whole-archive'/'no-whole-archive' flags.")
$(error "Cannot generate statically-linked binaries with this version of pkg-config")
endif
endif

CFLAGS += -I. -DALLOW_EXPERIMENTAL_API -D_GNU_SOURCE

OBJS := $(patsubst %.c,build/%.o,$(SRCS-y))

build/%.o: %.c Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) -c $< -o $@

build/$(APP)-shared: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDFLAGS_SHARED)

build/$(APP)-static: $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS) $(LDFLAGS_STATIC)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP)* build/*.o
	test -d build && rmdir -p build || true
