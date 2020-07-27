#   SPDX-License-Identifier: BSD-3-Clause
#   Copyright 2019 Mellanox Technologies, Ltd

include $(RTE_SDK)/mk/rte.vars.mk

# Library name.
LIB = librte_common_mlx5.a
LIB_GLUE = $(LIB_GLUE_BASE).$(LIB_GLUE_VERSION)
LIB_GLUE_BASE = librte_pmd_mlx5_glue.so
LIB_GLUE_VERSION = 20.02.0

# Sources.
ifeq ($(findstring y,$(CONFIG_RTE_LIBRTE_MLX5_PMD)$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD)$(CONFIG_RTE_LIBRTE_MLX5_REGEX_PMD)),y)
ifneq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)
SRCS-y += linux/mlx5_glue.c
endif
SRCS-y += mlx5_devx_cmds.c
SRCS-y += mlx5_common.c
SRCS-y += linux/mlx5_common_os.c
SRCS-y += linux/mlx5_nl.c
SRCS-y += linux/mlx5_common_verbs.c
SRCS-y += mlx5_common_mp.c
SRCS-y += mlx5_common_mr.c
SRCS-y += mlx5_malloc.c
SRCS-y += mlx5_common_pci.c
ifeq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)
INSTALL-y-lib += $(LIB_GLUE)
endif
endif

# Basic CFLAGS.
CFLAGS += -O3
CFLAGS += -std=c11 -Wall -Wextra
CFLAGS += -g
CFLAGS += -I.
CFLAGS += -I$(SRCDIR)
CFLAGS += -I$(SRCDIR)/linux
CFLAGS += -D_BSD_SOURCE
CFLAGS += -D_DEFAULT_SOURCE
CFLAGS += -D_XOPEN_SOURCE=600
CFLAGS += $(WERROR_FLAGS)
CFLAGS += -Wno-strict-prototypes
ifeq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)
CFLAGS += -DMLX5_GLUE='"$(LIB_GLUE)"'
CFLAGS += -DMLX5_GLUE_VERSION='"$(LIB_GLUE_VERSION)"'
CFLAGS_linux/mlx5_glue.o += -fPIC
LDLIBS += -ldl
else ifeq ($(CONFIG_RTE_IBVERBS_LINK_STATIC),y)
LDLIBS += $(shell $(RTE_SDK)/buildtools/options-ibverbs-static.sh)
else
LDLIBS += -libverbs -lmlx5
endif

LDLIBS += -lrte_eal -lrte_pci -lrte_kvargs -lrte_net
LDLIBS += -lrte_bus_pci

# A few warnings cannot be avoided in external headers.
CFLAGS += -Wno-error=cast-qual -UPEDANTIC

EXPORT_MAP := rte_common_mlx5_version.map

include $(RTE_SDK)/mk/rte.lib.mk

# Generate and clean-up mlx5_autoconf.h.

export CC CFLAGS CPPFLAGS EXTRA_CFLAGS EXTRA_CPPFLAGS
export AUTO_CONFIG_CFLAGS = -Wno-error

ifndef V
AUTOCONF_OUTPUT := >/dev/null
endif

mlx5_autoconf.h.new: FORCE

mlx5_autoconf.h.new: $(RTE_SDK)/buildtools/auto-config-h.sh
	$Q $(RM) -f -- '$@'
	$Q sh -- '$<' '$@' \
		HAVE_IBV_RELAXED_ORDERING \
		infiniband/verbs.h \
		enum IBV_ACCESS_RELAXED_ORDERING \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT \
		infiniband/mlx5dv.h \
		enum MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_TUNNEL_SUPPORT \
		infiniband/mlx5dv.h \
		enum MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_MPLS_SUPPORT \
		infiniband/verbs.h \
		enum IBV_FLOW_SPEC_MPLS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING \
		infiniband/verbs.h \
		enum IBV_WQ_FLAGS_PCI_WRITE_END_PADDING \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_WQ_FLAG_RX_END_PADDING \
		infiniband/verbs.h \
		enum IBV_WQ_FLAG_RX_END_PADDING \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_MLX5_MOD_SWP \
		infiniband/mlx5dv.h \
		type 'struct mlx5dv_sw_parsing_caps' \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_MLX5_MOD_MPW \
		infiniband/mlx5dv.h \
		enum MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_MLX5_MOD_CQE_128B_COMP \
		infiniband/mlx5dv.h \
		enum MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_MLX5_MOD_CQE_128B_PAD \
		infiniband/mlx5dv.h \
		enum MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_FLOW_DV_SUPPORT \
		infiniband/mlx5dv.h \
		func mlx5dv_create_flow_action_packet_reformat \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR \
		infiniband/mlx5dv.h \
		enum MLX5DV_DR_DOMAIN_TYPE_NIC_RX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR_ESWITCH \
		infiniband/mlx5dv.h \
		enum MLX5DV_DR_DOMAIN_TYPE_FDB \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR_VLAN \
		infiniband/mlx5dv.h \
		func mlx5dv_dr_action_create_push_vlan \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR_DEVX_PORT \
		infiniband/mlx5dv.h \
		func mlx5dv_query_devx_port \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVX_OBJ \
		infiniband/mlx5dv.h \
		func mlx5dv_devx_obj_create \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_FLOW_DEVX_COUNTERS \
		infiniband/mlx5dv.h \
		enum MLX5DV_FLOW_ACTION_COUNTERS_DEVX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5_DR_CREATE_ACTION_DEFAULT_MISS \
		infiniband/mlx5dv.h \
		enum MLX5DV_FLOW_ACTION_DEFAULT_MISS \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVX_ASYNC \
		infiniband/mlx5dv.h \
		func mlx5dv_devx_obj_query_async \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVX_QP \
		infiniband/mlx5dv.h \
		func mlx5dv_devx_qp_query \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DEVX_UAR_OFFSET \
		infiniband/mlx5dv.h \
		field "struct mlx5dv_devx_uar.mmap_off" \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_PP_ALLOC \
		infiniband/mlx5dv.h \
		func mlx5dv_pp_alloc \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR \
		infiniband/mlx5dv.h \
		func mlx5dv_dr_action_create_dest_devx_tir \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVX_EVENT \
		infiniband/mlx5dv.h \
		func mlx5dv_devx_get_event \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER \
		infiniband/mlx5dv.h \
		func mlx5dv_dr_action_create_flow_meter \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5_DR_FLOW_DUMP \
		infiniband/mlx5dv.h \
		func mlx5dv_dump_dr_domain \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_MMAP_GET_NC_PAGES_CMD \
		infiniband/mlx5dv.h \
		enum MLX5_MMAP_GET_NC_PAGES_CMD \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_VAR \
		infiniband/mlx5dv.h \
		func mlx5dv_alloc_var \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5DV_DR_MEM_RECLAIM \
		infiniband/mlx5dv.h \
		func mlx5dv_dr_domain_set_reclaim_device_memory \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5_OPCODE_ENHANCED_MPSW \
		infiniband/mlx5dv.h \
		enum MLX5_OPCODE_ENHANCED_MPSW \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5_OPCODE_SEND_EN \
		infiniband/mlx5dv.h \
		enum MLX5_OPCODE_SEND_EN \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_MLX5_OPCODE_WAIT \
		infiniband/mlx5dv.h \
		enum MLX5_OPCODE_WAIT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_ETHTOOL_LINK_MODE_25G \
		/usr/include/linux/ethtool.h \
		enum ETHTOOL_LINK_MODE_25000baseCR_Full_BIT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_ETHTOOL_LINK_MODE_50G \
		/usr/include/linux/ethtool.h \
		enum ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_ETHTOOL_LINK_MODE_100G \
		/usr/include/linux/ethtool.h \
		enum ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_COUNTERS_SET_V42 \
		infiniband/verbs.h \
		type 'struct ibv_counter_set_init_attr' \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IBV_DEVICE_COUNTERS_SET_V45 \
		infiniband/verbs.h \
		type 'struct ibv_counters_init_attr' \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NL_NLDEV \
		rdma/rdma_netlink.h \
		enum RDMA_NL_NLDEV \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_CMD_GET \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_CMD_GET \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_CMD_PORT_GET \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_CMD_PORT_GET \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_DEV_INDEX \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_DEV_INDEX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_DEV_NAME \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_DEV_NAME \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_PORT_INDEX \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_PORT_INDEX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX \
		rdma/rdma_netlink.h \
		enum RDMA_NLDEV_ATTR_NDEV_INDEX \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_NUM_VF \
		linux/if_link.h \
		enum IFLA_NUM_VF \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_EXT_MASK \
		linux/if_link.h \
		enum IFLA_EXT_MASK \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_PHYS_SWITCH_ID \
		linux/if_link.h \
		enum IFLA_PHYS_SWITCH_ID \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_IFLA_PHYS_PORT_NAME \
		linux/if_link.h \
		enum IFLA_PHYS_PORT_NAME \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_DEVLINK \
		linux/devlink.h \
		define DEVLINK_GENL_NAME \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseKR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseKR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseCR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseCR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseSR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseSR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_40000baseLR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_40000baseLR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseKR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseKR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseCR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseCR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseSR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseSR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_SUPPORTED_56000baseLR4_Full \
		/usr/include/linux/ethtool.h \
		define SUPPORTED_56000baseLR4_Full \
		$(AUTOCONF_OUTPUT)
	$Q sh -- '$<' '$@' \
		HAVE_STATIC_ASSERT \
		/usr/include/assert.h \
		define static_assert \
		$(AUTOCONF_OUTPUT)

# Create mlx5_autoconf.h or update it in case it differs from the new one.

mlx5_autoconf.h: mlx5_autoconf.h.new
	$Q [ -f '$@' ] && \
		cmp '$<' '$@' $(AUTOCONF_OUTPUT) || \
		mv '$<' '$@'

ifeq ($(findstring y,$(CONFIG_RTE_LIBRTE_MLX5_PMD)$(CONFIG_RTE_LIBRTE_MLX5_VDPA_PMD)$(CONFIG_RTE_LIBRTE_MLX5_REGEX_PMD)),y)
$(SRCS-y:.c=.o): mlx5_autoconf.h
endif

# Generate dependency plug-in for rdma-core when the PMD must not be linked
# directly, so that applications do not inherit this dependency.

ifeq ($(CONFIG_RTE_IBVERBS_LINK_DLOPEN),y)

$(LIB): $(LIB_GLUE)

ifeq ($(LINK_USING_CC),1)
GLUE_LDFLAGS := $(call linkerprefix,$(LDFLAGS))
else
GLUE_LDFLAGS := $(LDFLAGS)
endif
$(LIB_GLUE): linux/mlx5_glue.o
	$Q $(LD) $(GLUE_LDFLAGS) $(EXTRA_LDFLAGS) \
		-Wl,-h,$(LIB_GLUE) \
		-shared -o $@ $< -libverbs -lmlx5

linux/mlx5_glue.o: mlx5_autoconf.h

endif

clean_mlx5: FORCE
	$Q rm -f -- mlx5_autoconf.h mlx5_autoconf.h.new
	$Q rm -f -- linux/mlx5_glue.o $(LIB_GLUE_BASE)*

clean: clean_mlx5
