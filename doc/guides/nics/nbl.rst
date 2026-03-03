.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2025 Nebulamatrix Technology Co., Ltd

NBL Poll Mode Driver
====================

The NBL PMD (**librte_net_nbl**) provides poll mode driver support for
10/25/50/100/200 Gbps Nebulamatrix Series Network Adapters.


Features
--------

Features of the NBL PMD are:

- Multiple queues for Tx and Rx
- Receiver Side Scaling (RSS).
  Currently does not support user-configured RSS
  and only supports packet spraying via RSS.
- Jumbo frames


Supported NICs
--------------

The following Nebulamatrix device models are supported by the same nbl driver:

- S1205CQ-A00CHT
- S1105AS-A00CHT
- S1055AS-A00CHT
- S1052AS-A00CHT
- S1051AS-A00CHT
- S1045XS-A00CHT
- S1205CQ-A00CSP
- S1055AS-A00CSP
- S1052AS-A00CSP


Linux Prerequisites
-------------------

This driver relies on kernel drivers for resources allocations and initialization.
The following dependencies are not part of DPDK and must be installed separately:

- **Kernel modules**

  They provide low level device drivers that manage actual hardware initialization
  and resources sharing with user-space processes.

  Unlike most other PMDs, these modules must remain loaded and bound to their devices:

  - `nbl_core`: hardware driver managing Ethernet kernel network devices.

Because the `nbl_core` kernel driver code
has not been upstreamed to the Linux kernel community,
it cannot be provided by standard Linux distributions.
However, the `nbl_core` kernel driver has been upstreamed
to the openEuler and Anolis communities.
You can obtain the `nbl_core` code from the following links:

openEuler community:

- https://gitee.com/openeuler/kernel/pulls/11667

Anolis community:

- https://gitee.com/anolis/cloud-kernel/pulls/5185
- https://gitee.com/anolis/cloud-kernel/pulls/5059

Alternatively, you can contact us to obtain the `nbl_core` code and installation package.


Coexistence Between DPDK And Kernel Driver
------------------------------------------

When the device is bound to the `nbl_core` driver and IOMMU is in passthrough mode,
it is necessary to force I/O virtual address (IOVA)
to be mapped to physical address (PA)
with the EAL command line option ``--iova-mode=pa``.

Only PF supports coexistence between DPDK and kernel driver; VF does not.


Prerequisites
-------------

#. Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>`
   to setup the basic DPDK environment.

#. NBL PMD requires the ``vfio-pci`` kernel driver.
   The ``igb_uio`` and ``uio_pci_generic`` drivers are not supported.

   Insert the ``vfio-pci`` kernel module using the command ``modprobe vfio-pci``.
   Please make sure that IOMMU is enabled in your system,
   or use ``vfio-pci`` in ``noiommu`` mode::

     echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode

   Note that VF requires ``vfio-pci`` in ``noiommu`` mode
   when no IOMMU is available on the system.

#. Bind the intended NBL device to ``vfio-pci``.

#. Learn about `Nebulamatrix Series NICs
   <https://www.nebula-matrix.com/main>`_.

Limitations or Known Issues
---------------------------

- 32-bit architectures are not supported.
- Windows and BSD are not supported yet.
- Multiple processes are not supported.
