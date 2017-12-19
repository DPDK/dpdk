# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2015 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

ifeq ($(CONFIG_RTE_LIBRTE_PIPELINE),y)

#
# library name
#
APP = testpipeline

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

#
# all source are stored in SRCS-y
#
SRCS-y := main.c
SRCS-y += config.c
SRCS-y += init.c
SRCS-y += runtime.c
SRCS-y += pipeline_stub.c
SRCS-y += pipeline_hash.c
SRCS-y += pipeline_lpm.c
SRCS-y += pipeline_lpm_ipv6.c

# include ACL lib if available
SRCS-$(CONFIG_RTE_LIBRTE_ACL) += pipeline_acl.c

include $(RTE_SDK)/mk/rte.app.mk

endif
