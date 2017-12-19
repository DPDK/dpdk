# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = ip_pipeline

VPATH += $(SRCDIR)/pipeline

INC += $(sort $(wildcard *.h)) $(sort $(wildcard pipeline/*.h))

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) := main.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += config_parse.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += parser.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += config_parse_tm.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += config_check.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += init.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += thread.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += thread_fe.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += cpu_core_map.c

SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_common_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_common_fe.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_master_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_master.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_passthrough_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_passthrough.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_firewall_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_firewall.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_flow_classification_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_flow_classification.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_flow_actions_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_flow_actions.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_routing_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_routing.c

CFLAGS += -I$(SRCDIR) -I$(SRCDIR)/pipeline
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -Wno-error=unused-function -Wno-error=unused-variable

include $(RTE_SDK)/mk/rte.extapp.mk
