# SPDX-License-Identifier: BSD-3-Clause
# Copyright (c) 2022 Marvell.

if not is_linux or not dpdk_conf.get('RTE_ARCH_64')
    build = false
    reason = 'only supported on 64-bit Linux'
    subdir_done()
endif

enable_mvtvm = true

if not libarchive.found()
        message('drivers/ml/cnxk: libarchive not found')
        enable_mvtvm = false
endif

if not jansson_dep.found()
        message('drivers/ml/cnxk: jansson not found')
        enable_mvtvm = false
endif

dlpack_dep = dependency('dlpack', method: 'cmake', required: false, cmake_args: 'CONFIG')
if not dlpack_dep.found()
        message('drivers/ml/cnxk: dlpack not found')
        enable_mvtvm = false
endif

dmlc_dep = dependency('dmlc', method: 'cmake', required: false, cmake_args: 'CONFIG')
if not dmlc_dep.found()
        message('drivers/ml/cnxk: dmlc not found')
        enable_mvtvm = false
endif

tvm_dep = dependency('tvm', method: 'cmake', required: false, cmake_args: 'CONFIG', modules : ['tvm::tvm_runtime'])
if not tvm_dep.found()
        message('drivers/ml/cnxk: tvm_runtime not found')
        enable_mvtvm = false
endif

tvmdp_dep = dependency('tvmdp', method: 'pkg-config', required: false)
if not tvmdp_dep.found()
        message('drivers/ml/cnxk: tvmdp not found')
        enable_mvtvm = false
endif

sources = files(
        'cn10k_ml_dev.c',
        'cn10k_ml_ops.c',
        'cn10k_ml_model.c',
        'cn10k_ml_ocm.c',
        'cnxk_ml_dev.c',
        'cnxk_ml_io.c',
        'cnxk_ml_model.c',
        'cnxk_ml_ops.c',
        'cnxk_ml_utils.c',
)

deps += ['mldev', 'common_cnxk', 'kvargs', 'hash']

if enable_mvtvm

dpdk_conf.set('RTE_MLDEV_CNXK_ENABLE_MVTVM', 1)

sources += files(
        'mvtvm_ml_dev.c',
        'mvtvm_ml_ops.c',
        'mvtvm_ml_model.c',
)

ext_deps += jansson_dep
ext_deps += dlpack_dep
ext_deps += dmlc_dep
ext_deps += tvm_dep
ext_deps += tvmdp_dep
ext_deps += cc.find_library('stdc++', required: true)

deps += ['bus_vdev']

message('drivers/ml/cnxk: Enabled TVM model support')
else
message('drivers/ml/cnxk: Disabled TVM model support')

sources += files(
        'mvtvm_ml_stubs.c',
)

endif

require_iova_in_mbuf = false

if get_option('buildtype').contains('debug')
        cflags += [ '-DCNXK_ML_DEV_DEBUG' ]
else
        cflags += [ '-UCNXK_ML_DEV_DEBUG' ]
endif
