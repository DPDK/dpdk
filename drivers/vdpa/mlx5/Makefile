#   SPDX-License-Identifier: BSD-3-Clause
#   Copyright 2019 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Library name.
LIB = librte_pmd_mlx5_vdpa.a

# Sources.
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD) += mlx5_vdpa.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD) += mlx5_vdpa_mem.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD) += mlx5_vdpa_event.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD) += mlx5_vdpa_virtq.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD) += mlx5_vdpa_steer.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD) += mlx5_vdpa_lm.c


# Basic CFLAGS.
CFLAGS += -O3
CFLAGS += -std=c11 -Wall -Wextra
CFLAGS += -g
CFLAGS += -I$(RTE_SDK)/drivers/common/mlx5
CFLAGS += -I$(RTE_SDK)/drivers/common/mlx5/linux
CFLAGS += -I$(RTE_SDK)/drivers/net/mlx5_vdpa
CFLAGS += -I$(RTE_SDK)/lib/librte_sched
CFLAGS += -I$(BUILDDIR)/drivers/common/mlx5
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -D_XOPEN_SOURCE=600
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-strict-prototypes
LDLIBS += -lrte_common_mlx5
LDLIBS += -lrte_eal -lrte_vhost -lrte_kvargs -lrte_pci -lrte_sched
LDLIBS += -pthread

# A few warnings cannot be avoided in external headers.
CFLAGS += -Wno-error=cast-qual

EXPORT_MAP := rte_pmd_mlx5_vdpa_version.map

# DEBUG which is usually provided on the command-line may enable
# CONFIG_RTE_LIBRTE_MLX5_DEBUG.
ifeq ($(DEBUG),1)
CONFIG_RTE_LIBRTE_MLX5_DEBUG := y
endif

# User-defined CFLAGS.
ifeq ($(CONFIG_RTE_LIBRTE_MLX5_DEBUG),y)
CFLAGS += -pedantic
ifneq ($(CONFIG_RTE_TOOLCHAIN_ICC),y)
CFLAGS += -DPEDANTIC
endif
AUTO_CONFIG_CFLAGS += -Wno-pedantic
else
CFLAGS += -UPEDANTIC
endif

include $(RTE_SDK)/mk/rte.lib.mk
