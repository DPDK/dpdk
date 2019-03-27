# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2018 Intel Corporation

# binary name
APP = ip_pipeline

# all source are stored in SRCS-y
SRCS-y := action.c
SRCS-y += cli.c
SRCS-y += conn.c
SRCS-y += kni.c
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

# Build using pkg-config variables if possible
$(shell pkg-config --exists libdpdk)
ifeq ($(.SHELLSTATUS),0)

all: shared
.PHONY: shared static
shared: build/$(APP)-shared
	ln -sf $(APP)-shared build/$(APP)
static: build/$(APP)-static
	ln -sf $(APP)-static build/$(APP)

PC_FILE := $(shell pkg-config --path libdpdk)
CFLAGS += -O3 $(shell pkg-config --cflags libdpdk)
LDFLAGS_SHARED = $(shell pkg-config --libs libdpdk)
LDFLAGS_STATIC = -Wl,-Bstatic $(shell pkg-config --static --libs libdpdk)

CFLAGS += -I.

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
	rmdir --ignore-fail-on-non-empty build

else

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, detect a build directory, by looking for a path with a .config
RTE_TARGET ?= $(notdir $(abspath $(dir $(firstword $(wildcard $(RTE_SDK)/*/.config)))))

include $(RTE_SDK)/mk/rte.vars.mk

ifneq ($(CONFIG_RTE_EXEC_ENV_LINUX),y)
$(info This application can only operate in a linux environment, \
please change the definition of the RTE_TARGET environment variable)
all:
clean:
else

INC += $(sort $(wildcard *.h))

SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) := $(SRCS-y)

CFLAGS += -DALLOW_EXPERIMENTAL_API
CFLAGS += -I$(SRCDIR)
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

include $(RTE_SDK)/mk/rte.extapp.mk

endif
endif
