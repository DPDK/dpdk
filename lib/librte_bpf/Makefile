# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 Intel Corporation

include $(RTE_SDK)/mk/rte.vars.mk

# library name
LIB = librte_bpf.a

CFLAGS += -O3
CFLAGS += $(WERROR_FLAGS) -I$(SRCDIR)
CFLAGS += -DALLOW_EXPERIMENTAL_API
LDLIBS += -lrte_net -lrte_eal
LDLIBS += -lrte_mempool -lrte_ring
LDLIBS += -lrte_mbuf -lrte_ethdev

EXPORT_MAP := rte_bpf_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_exec.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_load.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_validate.c

# install header files
SYMLINK-$(CONFIG_RTE_LIBRTE_BPF)-include += bpf_def.h
SYMLINK-$(CONFIG_RTE_LIBRTE_BPF)-include += rte_bpf.h

include $(RTE_SDK)/mk/rte.lib.mk
