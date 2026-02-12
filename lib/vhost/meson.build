# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2017-2018 Intel Corporation

if not is_linux
    build = false
    reason = 'only supported on Linux'
endif
if has_libnuma
    dpdk_conf.set10('RTE_LIBRTE_VHOST_NUMA', true)
endif
dpdk_conf.set('RTE_LIBRTE_VHOST_POSTCOPY', cc.has_header('linux/userfaultfd.h'))

cflags += no_wvla_cflag

extra_flags = [
        '-fno-strict-aliasing',
        '-Wno-address-of-packed-member',
]

foreach arg: extra_flags
    if cc.has_argument(arg)
        cflags += arg
    endif
endforeach

sources = files(
        'fd_man.c',
        'iotlb.c',
        'socket.c',
        'vdpa.c',
        'vduse.c',
        'vhost.c',
        'vhost_crypto.c',
        'vhost_user.c',
        'virtio_net.c',
        'virtio_net_ctrl.c',
)
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
