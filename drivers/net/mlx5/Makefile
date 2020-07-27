#   SPDX-License-Identifier: BSD-3-Clause
#   Copyright 2015 6WIND S.A.
#   Copyright 2015 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Library name.
LIB = librte_pmd_mlx5.a

# Sources.
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxq.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_txq.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_txpp.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxtx.c
ifneq ($(filter y,$(CONFIG_RTE_ARCH_X86_64) \
			$(CONFIG_RTE_ARCH_PPC_64) \
		  $(CONFIG_RTE_ARCH_ARM64)),)
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxtx_vec.c
endif
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_trigger.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_ethdev.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_mac.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rxmode.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_vlan.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_stats.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_rss.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_mr.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow_meter.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow_dv.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_flow_verbs.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += mlx5_utils.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += linux/mlx5_socket.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += linux/mlx5_os.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += linux/mlx5_ethdev_os.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += linux/mlx5_verbs.c
SRCS-$(CONFIG_RTE_LIBRTE_MLX5_PMD) += linux/mlx5_mp_os.c

# Basic CFLAGS.
CFLAGS += -O3
CFLAGS += -std=c11 -Wall -Wextra
CFLAGS += -g
CFLAGS += -I$(RTE_SDK)/drivers/common/mlx5
CFLAGS += -I$(RTE_SDK)/drivers/common/mlx5/linux
CFLAGS += -I$(RTE_SDK)/drivers/net/mlx5
CFLAGS += -I$(RTE_SDK)/drivers/net/mlx5/linux
CFLAGS += -I$(BUILDDIR)/drivers/common/mlx5
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -D_XOPEN_SOURCE=600
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-strict-prototypes
LDLIBS += -lrte_common_mlx5
LDLIBS += -lm
LDLIBS += -lrte_eal -lrte_mbuf -lrte_mempool -lrte_ring
LDLIBS += -lrte_ethdev -lrte_net -lrte_kvargs

# A few warnings cannot be avoided in external headers.
CFLAGS += -Wno-error=cast-qual

EXPORT_MAP := rte_pmd_mlx5_version.map

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

