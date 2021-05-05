# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2015-2020 Intel Corporation


PKGCONF ?= pkg-config

ifneq ($(shell $(PKGCONF) --exists libdpdk && echo 0),0)
$(error "no installation of DPDK found")
endif
ifneq ($(shell uname),Linux)
$(error This application can only operate in a linux environment)
endif

# library name
LIB = librte_ethtool.so
LIB_STATIC = librte_ethtool.a
SRCS = rte_ethtool.c

CFLAGS += -O3
CFLAGS += -fPIC
CFLAGS += -DALLOW_EXPERIMENTAL_API

PC_FILE := $(shell $(PKGCONF) --path libdpdk 2>/dev/null)
CFLAGS += -O3 $(shell $(PKGCONF) --cflags libdpdk)
LDFLAGS_SHARED = $(shell $(PKGCONF) --libs libdpdk)
LDFLAGS += -Wl,--no-undefined $(LDFLAGS_SHARED)

# check for ixgbe by grepping pre-processor output
ifneq ($(shell $(CC) $(CFLAGS) -dM -E - < /dev/null | grep IXGBE),)
LDFLAGS += -lrte_net_ixgbe
endif

.PHONY: all clean static shared
all shared: build/$(LIB)
static: build/$(LIB_STATIC)

clean:
	rm -f build/$(LIB)
	test -d build && rmdir -p build || true

build:
	@mkdir -p $@

build/%.so: $(SRCS) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) -o $@ -shared $(SRCS) $(LDFLAGS)

build/%.a: $(SRCS) Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) -c $(SRCS) -o build/$(SRCS).o
	$(AR) -cr $@ build/*.o
