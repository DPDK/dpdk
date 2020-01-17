# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 6WIND S.A.
# Copyright 2018 Mellanox Technologies, Ltd

if not is_linux
	build = false
	reason = 'only supported on Linux'
	subdir_done()
endif
build = true

pmd_dlopen = (get_option('ibverbs_link') == 'dlopen')
LIB_GLUE_BASE = 'librte_pmd_mlx5_glue.so'
LIB_GLUE_VERSION = '20.02.0'
LIB_GLUE = LIB_GLUE_BASE + '.' + LIB_GLUE_VERSION
if pmd_dlopen
	dpdk_conf.set('RTE_IBVERBS_LINK_DLOPEN', 1)
	cflags += [
		'-DMLX5_GLUE="@0@"'.format(LIB_GLUE),
		'-DMLX5_GLUE_VERSION="@0@"'.format(LIB_GLUE_VERSION),
	]
endif

libnames = [ 'mlx5', 'ibverbs' ]
libs = []
foreach libname:libnames
	lib = dependency('lib' + libname, required:false)
	if not lib.found()
		lib = cc.find_library(libname, required:false)
	endif
	if lib.found()
		libs += [ lib ]
	else
		build = false
		reason = 'missing dependency, "' + libname + '"'
	endif
endforeach

if build
	allow_experimental_apis = true
	deps += ['hash']
	ext_deps += libs
	sources = files(
		'mlx5.c',
		'mlx5_ethdev.c',
		'mlx5_flow.c',
		'mlx5_flow_meter.c',
		'mlx5_flow_dv.c',
		'mlx5_flow_verbs.c',
		'mlx5_mac.c',
		'mlx5_mr.c',
		'mlx5_nl.c',
		'mlx5_rss.c',
		'mlx5_rxmode.c',
		'mlx5_rxq.c',
		'mlx5_rxtx.c',
		'mlx5_mp.c',
		'mlx5_stats.c',
		'mlx5_trigger.c',
		'mlx5_txq.c',
		'mlx5_vlan.c',
		'mlx5_devx_cmds.c',
		'mlx5_utils.c',
		'mlx5_socket.c',
	)
	if (dpdk_conf.has('RTE_ARCH_X86_64')
		or dpdk_conf.has('RTE_ARCH_ARM64')
		or dpdk_conf.has('RTE_ARCH_PPC_64'))
		sources += files('mlx5_rxtx_vec.c')
	endif
	if not pmd_dlopen
		sources += files('mlx5_glue.c')
	endif
	cflags_options = [
		'-std=c11',
		'-Wno-strict-prototypes',
		'-D_BSD_SOURCE',
		'-D_DEFAULT_SOURCE',
		'-D_XOPEN_SOURCE=600'
	]
	foreach option:cflags_options
		if cc.has_argument(option)
			cflags += option
		endif
	endforeach
	if get_option('buildtype').contains('debug')
		cflags += [ '-pedantic', '-UNDEBUG', '-DPEDANTIC' ]
	else
		cflags += [ '-DNDEBUG', '-UPEDANTIC' ]
	endif
	# To maintain the compatibility with the make build system
	# mlx5_autoconf.h file is still generated.
	# input array for meson member search:
	# [ "MACRO to define if found", "header for the search",
	#   "symbol to search", "struct member to search" ]
	has_member_args = [
		[ 'HAVE_IBV_MLX5_MOD_SWP', 'infiniband/mlx5dv.h',
		'struct mlx5dv_sw_parsing_caps', 'sw_parsing_offloads' ],
		[ 'HAVE_IBV_DEVICE_COUNTERS_SET_V42', 'infiniband/verbs.h',
		'struct ibv_counter_set_init_attr', 'counter_set_id' ],
		[ 'HAVE_IBV_DEVICE_COUNTERS_SET_V45', 'infiniband/verbs.h',
		'struct ibv_counters_init_attr', 'comp_mask' ],
	]
	# input array for meson symbol search:
	# [ "MACRO to define if found", "header for the search",
	#   "symbol to search" ]
	has_sym_args = [
		[ 'HAVE_IBV_DEVICE_STRIDING_RQ_SUPPORT', 'infiniband/mlx5dv.h',
		'MLX5DV_CQE_RES_FORMAT_CSUM_STRIDX' ],
		[ 'HAVE_IBV_DEVICE_TUNNEL_SUPPORT', 'infiniband/mlx5dv.h',
		'MLX5DV_CONTEXT_MASK_TUNNEL_OFFLOADS' ],
		[ 'HAVE_IBV_MLX5_MOD_MPW', 'infiniband/mlx5dv.h',
		'MLX5DV_CONTEXT_FLAGS_MPW_ALLOWED' ],
		[ 'HAVE_IBV_MLX5_MOD_CQE_128B_COMP', 'infiniband/mlx5dv.h',
		'MLX5DV_CONTEXT_FLAGS_CQE_128B_COMP' ],
		[ 'HAVE_IBV_MLX5_MOD_CQE_128B_PAD', 'infiniband/mlx5dv.h',
		'MLX5DV_CQ_INIT_ATTR_FLAGS_CQE_PAD' ],
		[ 'HAVE_IBV_FLOW_DV_SUPPORT', 'infiniband/mlx5dv.h',
		'mlx5dv_create_flow_action_packet_reformat' ],
		[ 'HAVE_IBV_DEVICE_MPLS_SUPPORT', 'infiniband/verbs.h',
		'IBV_FLOW_SPEC_MPLS' ],
		[ 'HAVE_IBV_WQ_FLAGS_PCI_WRITE_END_PADDING', 'infiniband/verbs.h',
		'IBV_WQ_FLAGS_PCI_WRITE_END_PADDING' ],
		[ 'HAVE_IBV_WQ_FLAG_RX_END_PADDING', 'infiniband/verbs.h',
		'IBV_WQ_FLAG_RX_END_PADDING' ],
		[ 'HAVE_MLX5DV_DR_DEVX_PORT', 'infiniband/mlx5dv.h',
		'mlx5dv_query_devx_port' ],
		[ 'HAVE_IBV_DEVX_OBJ', 'infiniband/mlx5dv.h',
		'mlx5dv_devx_obj_create' ],
		[ 'HAVE_IBV_FLOW_DEVX_COUNTERS', 'infiniband/mlx5dv.h',
		'MLX5DV_FLOW_ACTION_COUNTERS_DEVX' ],
		[ 'HAVE_IBV_DEVX_ASYNC', 'infiniband/mlx5dv.h',
		'mlx5dv_devx_obj_query_async' ],
		[ 'HAVE_MLX5DV_DR_ACTION_DEST_DEVX_TIR', 'infiniband/mlx5dv.h',
		'mlx5dv_dr_action_create_dest_devx_tir' ],
		[ 'HAVE_MLX5_DR_CREATE_ACTION_FLOW_METER', 'infiniband/mlx5dv.h',
		'mlx5dv_dr_action_create_flow_meter' ],
		[ 'HAVE_MLX5DV_MMAP_GET_NC_PAGES_CMD', 'infiniband/mlx5dv.h',
		'MLX5_MMAP_GET_NC_PAGES_CMD' ],
		[ 'HAVE_MLX5DV_DR', 'infiniband/mlx5dv.h',
		'MLX5DV_DR_DOMAIN_TYPE_NIC_RX' ],
		[ 'HAVE_MLX5DV_DR_ESWITCH', 'infiniband/mlx5dv.h',
		'MLX5DV_DR_DOMAIN_TYPE_FDB' ],
		[ 'HAVE_MLX5DV_DR_VLAN', 'infiniband/mlx5dv.h',
		'mlx5dv_dr_action_create_push_vlan' ],
		[ 'HAVE_SUPPORTED_40000baseKR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseKR4_Full' ],
		[ 'HAVE_SUPPORTED_40000baseCR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseCR4_Full' ],
		[ 'HAVE_SUPPORTED_40000baseSR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseSR4_Full' ],
		[ 'HAVE_SUPPORTED_40000baseLR4_Full', 'linux/ethtool.h',
		'SUPPORTED_40000baseLR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseKR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseKR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseCR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseCR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseSR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseSR4_Full' ],
		[ 'HAVE_SUPPORTED_56000baseLR4_Full', 'linux/ethtool.h',
		'SUPPORTED_56000baseLR4_Full' ],
		[ 'HAVE_ETHTOOL_LINK_MODE_25G', 'linux/ethtool.h',
		'ETHTOOL_LINK_MODE_25000baseCR_Full_BIT' ],
		[ 'HAVE_ETHTOOL_LINK_MODE_50G', 'linux/ethtool.h',
		'ETHTOOL_LINK_MODE_50000baseCR2_Full_BIT' ],
		[ 'HAVE_ETHTOOL_LINK_MODE_100G', 'linux/ethtool.h',
		'ETHTOOL_LINK_MODE_100000baseKR4_Full_BIT' ],
		[ 'HAVE_IFLA_NUM_VF', 'linux/if_link.h',
		'IFLA_NUM_VF' ],
		[ 'HAVE_IFLA_EXT_MASK', 'linux/if_link.h',
		'IFLA_EXT_MASK' ],
		[ 'HAVE_IFLA_PHYS_SWITCH_ID', 'linux/if_link.h',
		'IFLA_PHYS_SWITCH_ID' ],
		[ 'HAVE_IFLA_PHYS_PORT_NAME', 'linux/if_link.h',
		'IFLA_PHYS_PORT_NAME' ],
		[ 'HAVE_RDMA_NL_NLDEV', 'rdma/rdma_netlink.h',
		'RDMA_NL_NLDEV' ],
		[ 'HAVE_RDMA_NLDEV_CMD_GET', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_CMD_GET' ],
		[ 'HAVE_RDMA_NLDEV_CMD_PORT_GET', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_CMD_PORT_GET' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_DEV_INDEX', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_DEV_INDEX' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_DEV_NAME', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_DEV_NAME' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_PORT_INDEX', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_PORT_INDEX' ],
		[ 'HAVE_RDMA_NLDEV_ATTR_NDEV_INDEX', 'rdma/rdma_netlink.h',
		'RDMA_NLDEV_ATTR_NDEV_INDEX' ],
		[ 'HAVE_MLX5_DR_FLOW_DUMP', 'infiniband/mlx5dv.h',
		'mlx5dv_dump_dr_domain'],
	]
	config = configuration_data()
	foreach arg:has_sym_args
		config.set(arg[0], cc.has_header_symbol(arg[1], arg[2],
			dependencies: libs))
	endforeach
	foreach arg:has_member_args
		file_prefix = '#include <' + arg[1] + '>'
		config.set(arg[0], cc.has_member(arg[2], arg[3],
			prefix : file_prefix, dependencies: libs))
	endforeach
	configure_file(output : 'mlx5_autoconf.h', configuration : config)
endif
# Build Glue Library
if pmd_dlopen and build
	dlopen_name = 'mlx5_glue'
	dlopen_lib_name = driver_name_fmt.format(dlopen_name)
	dlopen_so_version = LIB_GLUE_VERSION
	dlopen_sources = files('mlx5_glue.c')
	dlopen_install_dir = [ eal_pmd_path + '-glue' ]
	dlopen_includes = [global_inc]
	dlopen_includes += include_directories(
		'../../../lib/librte_eal/common/include/generic',
	)
	shared_lib = shared_library(
		dlopen_lib_name,
		dlopen_sources,
		include_directories: dlopen_includes,
		c_args: cflags,
		dependencies: libs,
		link_args: [
		'-Wl,-export-dynamic',
		'-Wl,-h,@0@'.format(LIB_GLUE),
		],
		soversion: dlopen_so_version,
		install: true,
		install_dir: dlopen_install_dir,
	)
endif
