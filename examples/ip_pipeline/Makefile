# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

# binary name
APP = ip_pipeline

# all source are stored in SRCS-y
SRCS-y := main.c
SRCS-y += config_parse.c
SRCS-y += parser.c
SRCS-y += config_parse_tm.c
SRCS-y += config_check.c
SRCS-y += init.c
SRCS-y += thread.c
SRCS-y += thread_fe.c
SRCS-y += cpu_core_map.c

SRCS-y += pipeline_common_be.c
SRCS-y += pipeline_common_fe.c
SRCS-y += pipeline_master_be.c
SRCS-y += pipeline_master.c
SRCS-y += pipeline_passthrough_be.c
SRCS-y += pipeline_passthrough.c
SRCS-y += pipeline_firewall_be.c
SRCS-y += pipeline_firewall.c
SRCS-y += pipeline_flow_classification_be.c
SRCS-y += pipeline_flow_classification.c
SRCS-y += pipeline_flow_actions_be.c
SRCS-y += pipeline_flow_actions.c
SRCS-y += pipeline_routing_be.c
SRCS-y += pipeline_routing.c

# Build using pkg-config variables if possible
$(shell pkg-config --exists libdpdk)
ifeq ($(.SHELLSTATUS),0)

PC_FILE := $(shell pkg-config --path libdpdk)
CFLAGS += $(shell pkg-config --cflags libdpdk)
LDFLAGS += $(shell pkg-config --libs libdpdk)

VPATH += pipeline
CFLAGS += -I. -I./pipeline/

OBJS := $(patsubst %.c,build/%.o,$(SRCS-y))

build/%.o: %.c Makefile $(PC_FILE) | build
	$(CC) $(CFLAGS) -c $< -o $@

build/$(APP): $(OBJS)
	$(CC) $(OBJS) -o $@ $(LDFLAGS)

build:
	@mkdir -p $@

.PHONY: clean
clean:
	rm -f build/$(APP) build/*.o
	rmdir --ignore-fail-on-non-empty build

else

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

VPATH += $(SRCDIR)/pipeline

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

INC += $(sort $(wildcard *.h)) $(sort $(wildcard pipeline/*.h))

SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) := $(SRCS-y)

CFLAGS += -I$(SRCDIR) -I$(SRCDIR)/pipeline
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -Wno-error=unused-function -Wno-error=unused-variable

include $(RTE_SDK)/mk/rte.extapp.mk

endif
