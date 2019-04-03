# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2019 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_stack.a

CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR) -O3
CFLAGS += -DALLOW_EXPERIMENTAL_API
LDLIBS += -lrte_eal

EXPORT_MAP := rte_stack_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_STACK) := rte_stack.c \
				   rte_stack_std.c \
				   rte_stack_lf.c

# install includes
SYMLINK-$(CONFIG_RTE_LIBRTE_STACK)-include := rte_stack.h \
					      rte_stack_std.h \
					      rte_stack_lf.h \
					      rte_stack_lf_generic.h \
					      rte_stack_lf_c11.h

include $(RTE_SDK)/mk/rte.lib.mk
