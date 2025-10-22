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


Prerequisites
-------------

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>`
  to setup the basic DPDK environment.

- Learn about `Nebulamatrix Series NICs
  <https://www.nebula-matrix.com/main>`_.


Multiple Processes
------------------

The NBL PMD does not support multiple processes.


Limitations or Known Issues
---------------------------

32-bit architectures are not supported.

Windows and BSD are not supported yet.
