# SPDX-License-Identifier: BSD-3-Clause
# Copyright 2018 6WIND S.A.
# Copyright 2018 Mellanox Technologies, Ltd

pmd_dlopen = (get_option('ibverbs_link') == 'dlopen')
LIB_GLUE_BASE = 'librte_pmd_mlx4_glue.so'
LIB_GLUE_VERSION = '18.02.0'
LIB_GLUE = LIB_GLUE_BASE + '.' + LIB_GLUE_VERSION
if pmd_dlopen
	dpdk_conf.set('RTE_IBVERBS_LINK_DLOPEN', 1)
	cflags += [
		'-DMLX4_GLUE="@0@"'.format(LIB_GLUE),
		'-DMLX4_GLUE_VERSION="@0@"'.format(LIB_GLUE_VERSION),
	]
endif
libnames = [ 'mnl', 'mlx4', 'ibverbs' ]
libs = []
build = true
foreach libname:libnames
	lib = dependency('lib' + libname, required:false)
	if not lib.found()
		lib = cc.find_library(libname, required:false)
	endif
	if lib.found()
		libs += [ lib ]
	else
		build = false
	endif
endforeach
# Compile PMD
if build
	allow_experimental_apis = true
	ext_deps += libs
	sources = files(
		'mlx4.c',
		'mlx4_ethdev.c',
		'mlx4_flow.c',
		'mlx4_intr.c',
		'mlx4_mp.c',
		'mlx4_mr.c',
		'mlx4_rxq.c',
		'mlx4_rxtx.c',
		'mlx4_txq.c',
		'mlx4_utils.c',
	)
	if not pmd_dlopen
		sources += files('mlx4_glue.c')
	endif
	cflags_options = [
		'-Wextra',
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
	# mlx4_autoconf.h file is still generated.
	# input array for meson member search:
	# [ "MACRO to define if found", "header for the search",
	#   "symbol to search","struct member to search" ]
	#
	has_member_args = [
		[ 'HAVE_IBV_MLX4_WQE_LSO_SEG', 'infiniband/mlx4dv.h',
		'struct mlx4_wqe_lso_seg', 'mss_hdr_size' ],
	]
	# input array for meson symbol search:
	# [ "MACRO to define if found", "header for the search",
	#   "symbol to search" ]
	has_sym_args = [
		[ 'HAVE_IBV_MLX4_BUF_ALLOCATORS', 'infiniband/mlx4dv.h',
		'MLX4DV_SET_CTX_ATTR_BUF_ALLOCATORS' ],
		[ 'HAVE_IBV_MLX4_UAR_MMAP_OFFSET', 'infiniband/mlx4dv.h',
		'MLX4DV_QP_MASK_UAR_MMAP_OFFSET' ],
	]
	config = configuration_data()
	foreach arg:has_sym_args
		config.set(arg[0], cc.has_header_symbol(arg[1], arg[2]))
	endforeach
	foreach arg:has_member_args
		file_prefix = '#include<' + arg[1] + '>'
		config.set(arg[0], cc.has_member(arg[2], arg[3],
			prefix : file_prefix))
	endforeach
	configure_file(output : 'mlx4_autoconf.h', configuration : config)
endif
# Build Glue Library
if pmd_dlopen and build
	dlopen_name = 'mlx4_glue'
	dlopen_lib_name = driver_name_fmt.format(dlopen_name)
	dlopen_so_version = LIB_GLUE_VERSION
	dlopen_sources = files('mlx4_glue.c')
	dlopen_install_dir = [ eal_pmd_path + '-glue' ]
	shared_lib = shared_library(
		dlopen_lib_name,
		dlopen_sources,
		include_directories: global_inc,
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
