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
ifeq ($(CONFIG_RTE_LIBRTE_BPF_ELF),y)
LDLIBS += -lelf
endif

EXPORT_MAP := rte_bpf_version.map

LIBABIVER := 1

# all source are stored in SRCS-y
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_exec.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_load.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_pkt.c
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_validate.c
ifeq ($(CONFIG_RTE_LIBRTE_BPF_ELF),y)
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_load_elf.c
endif
ifeq ($(CONFIG_RTE_ARCH_X86_64),y)
SRCS-$(CONFIG_RTE_LIBRTE_BPF) += bpf_jit_x86.c
endif

# install header files
SYMLINK-$(CONFIG_RTE_LIBRTE_BPF)-include += bpf_def.h
SYMLINK-$(CONFIG_RTE_LIBRTE_BPF)-include += rte_bpf.h
SYMLINK-$(CONFIG_RTE_LIBRTE_BPF)-include += rte_bpf_ethdev.h

include $(RTE_SDK)/mk/rte.lib.mk
