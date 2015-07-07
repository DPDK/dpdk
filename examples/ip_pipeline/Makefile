#   BSD LICENSE
#
#   Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
#   All rights reserved.
#
#   Redistribution and use in source and binary forms, with or without
#   modification, are permitted provided that the following conditions
#   are met:
#
#     * Redistributions of source code must retain the above copyright
#       notice, this list of conditions and the following disclaimer.
#     * Redistributions in binary form must reproduce the above copyright
#       notice, this list of conditions and the following disclaimer in
#       the documentation and/or other materials provided with the
#       distribution.
#     * Neither the name of Intel Corporation nor the names of its
#       contributors may be used to endorse or promote products derived
#       from this software without specific prior written permission.
#
#   THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
#   "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
#   LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
#   A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
#   OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
#   SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
#   LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
#   DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
#   THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#   (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
#   OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
RTE_TARGET ?= x86_64-native-linuxapp-gcc

DIRS-(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline

include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = ip_pipeline

VPATH += $(SRCDIR)/pipeline

INC += $(wildcard *.h) $(wildcard pipeline/*.h)

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) := main.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += config_parse.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += config_parse_tm.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += config_check.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += init.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += thread.c
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
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_routing_be.c
SRCS-$(CONFIG_RTE_LIBRTE_PIPELINE) += pipeline_routing.c

CFLAGS += -I$(SRCDIR) -I$(SRCDIR)/pipeline
CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -Wno-error=unused-function -Wno-error=unused-variable

include $(RTE_SDK)/mk/rte.extapp.mk
