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

Device Reset Support
--------------------

The MANA PMD supports automatic recovery from hardware service reset events.
When the MANA kernel driver receives a hardware service event,
it initiates a device reset and notifies userspace
via ``IBV_EVENT_DEVICE_FATAL``.

The driver handles this transparently through a two-phase reset flow:

Enter phase
   The interrupt handler blocks new data path bursts
   and waits for all in-flight burst calls to drain
   using per-queue atomic flags,
   then spawns a control thread for the remaining work.

Teardown and exit phase
   The control thread tears down IB resources and queues,
   unmaps secondary process doorbell pages, and closes the device.
   After a delay for hardware recovery, it re-probes the PCI device,
   reinstalls the interrupt handler, reinitializes resources, and restarts queues.

The driver emits the following ethdev recovery events
to notify upper layers (e.g. netvsc) of the reset lifecycle:

``RTE_ETH_EVENT_ERR_RECOVERING``
   Reset has started.

``RTE_ETH_EVENT_RECOVERY_SUCCESS``
   Device has recovered successfully.

``RTE_ETH_EVENT_RECOVERY_FAILED``
   Recovery failed.

To distinguish a PCI hot-remove from a service reset,
the driver registers for PCI device removal events.
This requires the application to call ``rte_dev_event_monitor_start()``
for removal events to be delivered (e.g. testpmd ``--hot-plug-handling`` option).
