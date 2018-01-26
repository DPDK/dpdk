# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2010-2014 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

#
# library name
#
LIB = librte_sched.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS)

CFLAGS_rte_red.o := -D_GNU_SOURCE

LDLIBS += -lm
LDLIBS += -lrt
LDLIBS += -lrte_eal -lrte_mempool -lrte_mbuf -lrte_net
LDLIBS += -lrte_timer

EXPORT_MAP := rte_sched_version.map

LIBABIVER := 1

#
# all source are stored in SRCS-y
#
SRCS-$(CONFIG_RTE_LIBRTE_SCHED) += rte_sched.c rte_red.c rte_approx.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_SCHED)-include := rte_sched.h rte_sched_common.h rte_red.h rte_approx.h

include $(RTE_SDK)/mk/rte.lib.mk
