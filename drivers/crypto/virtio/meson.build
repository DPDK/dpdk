# SPDX-License-Identifier: BSD-3-Clause
# Copyright(c) 2018 HUAWEI TECHNOLOGIES CO., LTD.

if is_windows
    build = false
    reason = 'not supported on Windows'
    subdir_done()
endif

includes += include_directories('../../../lib/vhost')
deps += 'bus_pci'
sources = files(
        'virtio_cryptodev.c',
        'virtio_cvq.c',
        'virtio_pci.c',
        'virtio_rxtx.c',
        'virtqueue.c',
)

if is_linux
    sources += files('virtio_user_cryptodev.c',
        'virtio_user/vhost_vdpa.c',
        'virtio_user/virtio_user_dev.c')
    deps += ['bus_vdev']
endif
