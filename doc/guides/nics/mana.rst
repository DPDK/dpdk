..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2022 Microsoft Corporation

MANA poll mode driver library
=============================

The MANA poll mode driver library (**librte_net_mana**) implements support
for Microsoft Azure Network Adapter VF in SR-IOV context.

Prerequisites
-------------

This driver relies on external libraries and kernel drivers
for resources allocations and initialization.
The following dependencies are not part of DPDK
and must be installed separately:

- **libibverbs** (provided by rdma-core package)

  User space verbs framework used by librte_net_mana.
  This library provides a generic interface between the kernel
  and low-level user space drivers such as libmana.

  It allows slow and privileged operations
  (context initialization, hardware resources allocations)
  to be managed by the kernel and fast operations to never leave user space.
  The minimum required rdma-core version is v44.

  In most cases, rdma-core is shipped as a package with an OS distribution.
  User can also install the upstream version of the rdma-core from
  https://github.com/linux-rdma/rdma-core.

- **libmana** (provided by rdma-core package)

  Low-level user space driver library
  for Microsoft Azure Network Adapter devices,
  it is automatically loaded by libibverbs.
  The minimum required version of rdma-core with libmana is v44.

- **Kernel modules**

  They provide the kernel-side verbs API and low level device drivers
  that manage actual hardware initialization
  and resources sharing with user space processes.
  The minimum required Linux kernel version is 6.2.

  Unlike most other PMDs, these modules must remain loaded
  and bound to their devices:

  - mana: Ethernet device driver that provides kernel network interfaces.
  - mana_ib: InifiniBand device driver.
  - ib_uverbs: user space driver for verbs (entry point for libibverbs).

Driver compilation and testing
------------------------------

Refer to the document
:ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>` for details.

Runtime Configuration
---------------------

The user can specify below argument in devargs.

#.  ``mac``:

    Specify the MAC address for this device.
    If it is set, the driver probes and loads the NIC
    with a matching MAC address.
    If it is not set, the driver probes on all the NICs on the PCI device.
    The default value is not set,
    meaning all the NICs will be probed and loaded.
    User can specify multiple mac=xx:xx:xx:xx:xx:xx arguments for up to 8 NICs.
