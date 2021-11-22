..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

IFCVF vDPA driver
=================

The IFCVF vDPA (vhost data path acceleration) driver provides support for the
Intel FPGA 100G VF (IFCVF). IFCVF's datapath is virtio ring compatible, it
works as a HW vhost backend which can send/receive packets to/from virtio
directly by DMA. Besides, it supports dirty page logging and device state
report/restore, this driver enables its vDPA functionality.


IFCVF vDPA Implementation
-------------------------

IFCVF's vendor ID and device ID are same as that of virtio net pci device,
with its specific subsystem vendor ID and device ID. To let the device be
probed by IFCVF driver, adding "vdpa=1" parameter helps to specify that this
device is to be used in vDPA mode, rather than polling mode, virtio PMD will
skip when it detects this message. If no this parameter specified, device
will not be used as a vDPA device, and it will be driven by virtio PMD.

Different VF devices serve different virtio frontends which are in different
VMs, so each VF needs to have its own DMA address translation service. During
the driver probe a new container is created for this device, with this
container vDPA driver can program DMA remapping table with the VM's memory
region information.

The device argument "sw-live-migration=1" will configure the driver into SW
assisted live migration mode. In this mode, the driver will set up a SW relay
thread when LM happens, this thread will help device to log dirty pages. Thus
this mode does not require HW to implement a dirty page logging function block,
but will consume some percentage of CPU resource depending on the network
throughput. If no this parameter specified, driver will rely on device's logging
capability.

Key IFCVF vDPA driver ops
~~~~~~~~~~~~~~~~~~~~~~~~~

- ifcvf_dev_config:
  Enable VF data path with virtio information provided by vhost lib, including
  IOMMU programming to enable VF DMA to VM's memory, VFIO interrupt setup to
  route HW interrupt to virtio driver, create notify relay thread to translate
  virtio driver's kick to a MMIO write onto HW, HW queues configuration.

  This function gets called to set up HW data path backend when virtio driver
  in VM gets ready.

- ifcvf_dev_close:
  Revoke all the setup in ifcvf_dev_config.

  This function gets called when virtio driver stops device in VM.

To create a vhost port with IFC VF
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- Create a vhost socket and assign a VF's device ID to this socket via
  vhost API. When QEMU vhost connection gets ready, the assigned VF will
  get configured automatically.


Features
--------

Features of the IFCVF driver are:

- Compatibility with virtio 0.95 and 1.0.
- SW assisted vDPA live migration.


Prerequisites
-------------

- Platform with IOMMU feature. IFC VF needs address translation service to
  Rx/Tx directly with virtio driver in VM.


Limitations
-----------

Dependency on vfio-pci
~~~~~~~~~~~~~~~~~~~~~~

vDPA driver needs to setup VF MSIX interrupts, each queue's interrupt vector
is mapped to a callfd associated with a virtio ring. Currently only vfio-pci
allows multiple interrupts, so the IFCVF driver is dependent on vfio-pci.

Live Migration with VIRTIO_NET_F_GUEST_ANNOUNCE
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

IFC VF doesn't support RARP packet generation, virtio frontend supporting
VIRTIO_NET_F_GUEST_ANNOUNCE feature can help to do that.
