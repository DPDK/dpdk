# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2020 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Library name.
LIB = librte_pmd_mlx5_regex.a

# Sources.
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_REGEX_PMD) += mlx5_regex.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_REGEX_PMD) += mlx5_rxp.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_REGEX_PMD) += mlx5_regex_devx.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_REGEX_PMD) += mlx5_regex_control.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_REGEX_PMD) += mlx5_regex_fastpath.c

# Basic CFLAGS.
CFLAGS += -O3
CFLAGS += -std=c11 -Wall -Wextra
CFLAGS += -g
CFLAGS += -I$(RTE_SDK)/drivers/common/mlx5
CFLAGS += -I$(BUILDDIR)/drivers/common/mlx5
CFLAGS += -I$(RTE_SDK)/drivers/common/mlx5/linux
CFLAGS += -I$(BUILDDIR)/drivers/common/mlx5/linux
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -D_XOPEN_SOURCE=600
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-strict-prototypes
LDLIBS += -lrte_common_mlx5
LDLIBS += -lm
LDLIBS += -lrte_eal -lrte_mbuf -lrte_regexdev
LDLIBS += -lrte_kvargs
LDLIBS += -lrte_bus_pci
LDLIBS += -lrte_pci

# A few warnings cannot be avoided in external headers.
CFLAGS += -Wno-error=cast-qual

EXPORT_MAP := rte_pmd_mlx5_regex_version.map

include $(RTE_SDK)/mk/rte.lib.mk
