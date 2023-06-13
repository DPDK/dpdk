# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
if has_libnuma
    dpdk_conf.set10('RTE_LIBRTE_VHOST_NUMA', true)
endif
if (toolchain == 'gcc' and cc.version().version_compare('>=8.3.0'))
    cflags += '-DVHOST_GCC_UNROLL_PRAGMA'
elif (toolchain == 'clang' and cc.version().version_compare('>=3.7.0'))
    cflags += '-DVHOST_CLANG_UNROLL_PRAGMA'
elif (toolchain == 'icc' and cc.version().version_compare('>=16.0.0'))
    cflags += '-DVHOST_ICC_UNROLL_PRAGMA'
endif
dpdk_conf.set('RTE_LIBRTE_VHOST_POSTCOPY', cc.has_header('linux/userfaultfd.h'))
cflags += '-fno-strict-aliasing'

sources = files(
        'fd_man.c',
        'iotlb.c',
        'socket.c',
        'vdpa.c',
        'vhost.c',
        'vhost_crypto.c',
        'vhost_user.c',
        'virtio_net.c',
        'virtio_net_ctrl.c',
)
if cc.has_header('linux/vduse.h')
    sources += files('vduse.c')
    cflags += '-DVHOST_HAS_VDUSE'
endif
headers = files(
        'rte_vdpa.h',
        'rte_vhost.h',
        'rte_vhost_async.h',
        'rte_vhost_crypto.h',
)
driver_sdk_headers = files(
        'vdpa_driver.h',
)
deps += ['ethdev', 'cryptodev', 'hash', 'pci', 'dmadev']

use_function_versioning = true
