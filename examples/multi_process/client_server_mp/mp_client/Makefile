# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

ifeq ($(RTE_SDK),)
$(error "Please define RTE_SDK environment variable")
endif

# Default target, can be overridden by command line or environment
include $(RTE_SDK)/mk/rte.vars.mk

# binary name
APP = mp_client

# all source are stored in SRCS-y
SRCS-y := client.c

CFLAGS += $(WERROR_FLAGS) -O3
CFLAGS += -I$(SRCDIR)/../shared

include $(RTE_SDK)/mk/rte.extapp.mk
