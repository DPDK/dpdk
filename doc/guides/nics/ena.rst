.. BSD LICENSE

    Copyright (c) 2015-2018 Amazon.com, Inc. or its affiliates.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Amazon.com, Inc. nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

ENA Poll Mode Driver
====================

The ENA PMD is a DPDK poll-mode driver for the Amazon Elastic
Network Adapter (ENA) family.

Overview
--------

The ENA driver exposes a lightweight management interface with a
minimal set of memory mapped registers and an extendable command set
through an Admin Queue.

The driver supports a wide range of ENA adapters, is link-speed
independent (i.e., the same driver is used for 10GbE, 25GbE, 40GbE,
etc.), and it negotiates and supports an extendable feature set.

ENA adapters allow high speed and low overhead Ethernet traffic
processing by providing a dedicated Tx/Rx queue pair per CPU core.

The ENA driver supports industry standard TCP/IP offload features such
as checksum offload and TCP transmit segmentation offload (TSO).

Receive-side scaling (RSS) is supported for multi-core scaling.

Some of the ENA devices support a working mode called Low-latency
Queue (LLQ), which saves several more microseconds.

Management Interface
--------------------

ENA management interface is exposed by means of:

* Device Registers
* Admin Queue (AQ) and Admin Completion Queue (ACQ)

ENA device memory-mapped PCIe space for registers (MMIO registers)
are accessed only during driver initialization and are not involved
in further normal device operation.

AQ is used for submitting management commands, and the
results/responses are reported asynchronously through ACQ.

ENA introduces a very small set of management commands with room for
vendor-specific extensions. Most of the management operations are
framed in a generic Get/Set feature command.

The following admin queue commands are supported:

* Create I/O submission queue
* Create I/O completion queue
* Destroy I/O submission queue
* Destroy I/O completion queue
* Get feature
* Set feature
* Get statistics

Refer to ``ena_admin_defs.h`` for the list of supported Get/Set Feature
properties.

Data Path Interface
-------------------

I/O operations are based on Tx and Rx Submission Queues (Tx SQ and Rx
SQ correspondingly). Each SQ has a completion queue (CQ) associated
with it.

The SQs and CQs are implemented as descriptor rings in contiguous
physical memory.

Refer to ``ena_eth_io_defs.h`` for the detailed structure of the descriptor

The driver supports multi-queue for both Tx and Rx.

Configuration information
-------------------------

**DPDK Configuration Parameters**

  The following configuration options are available for the ENA PMD:

   * **CONFIG_RTE_LIBRTE_ENA_PMD** (default y): Enables or disables inclusion
     of the ENA PMD driver in the DPDK compilation.

   * **CONFIG_RTE_LIBRTE_ENA_DEBUG_RX** (default n): Enables or disables debug
     logging of RX logic within the ENA PMD driver.

   * **CONFIG_RTE_LIBRTE_ENA_DEBUG_TX** (default n): Enables or disables debug
     logging of TX logic within the ENA PMD driver.

   * **CONFIG_RTE_LIBRTE_ENA_COM_DEBUG** (default n): Enables or disables debug
     logging of low level tx/rx logic in ena_com(base) within the ENA PMD driver.

**ENA Configuration Parameters**

   * **Number of Queues**

     This is the requested number of queues upon initialization, however, the actual
     number of receive and transmit queues to be created will be the minimum between
     the maximal number supported by the device and number of queues requested.

   * **Size of Queues**

     This is the requested size of receive/transmit queues, while the actual size
     will be the minimum between the requested size and the maximal receive/transmit
     supported by the device.

Building DPDK
-------------

See the :ref:`DPDK Getting Started Guide for Linux <linux_gsg>` for
instructions on how to build DPDK.

By default the ENA PMD library will be built into the DPDK library.

For configuring and using UIO and VFIO frameworks, please also refer :ref:`the
documentation that comes with DPDK suite <linux_gsg>`.

Supported ENA adapters
----------------------

Current ENA PMD supports the following ENA adapters including:

* ``1d0f:ec20`` - ENA VF
* ``1d0f:ec21`` - ENA VF with LLQ support

Supported Operating Systems
---------------------------

Any Linux distribution fulfilling the conditions described in ``System Requirements``
section of :ref:`the DPDK documentation <linux_gsg>` or refer to *DPDK Release Notes*.

Supported features
------------------

* MTU configuration
* Jumbo frames up to 9K
* IPv4/TCP/UDP checksum offload
* TSO offload
* Multiple receive and transmit queues
* RSS hash
* RSS indirection table configuration
* Low Latency Queue for Tx
* Basic and extended statistics
* LSC event notification
* Watchdog (requires handling of timers in the application)
* Device reset upon failure

Prerequisites
-------------

#. Prepare the system as recommended by DPDK suite.  This includes environment
   variables, hugepages configuration, tool-chains and configuration.

#. ENA PMD can operate with ``vfio-pci``(*) or ``igb_uio`` driver.

   (*) ENAv2 hardware supports Low Latency Queue v2 (LLQv2). This feature
   reduces the latency of the packets by pushing the header directly through
   the PCI to the device, before the DMA is even triggered. For proper work
   kernel PCI driver must support write combining (WC). In mainline version of
   ``igb_uio`` (in DPDK repo) it must be enabled by loading module with
   ``wc_activate=1`` flag (example below). However, mainline's vfio-pci
   driver in kernel doesn't have WC support yet (planed to be added).
   If vfio-pci used user should be either turn off ENAv2 (to avoid performance
   impact) or recompile vfio-pci driver with patch provided in
   `amzn-github <https://github.com/amzn/amzn-drivers/tree/master/userspace/dpdk/enav2-vfio-patch>`_.

#. Insert ``vfio-pci`` or ``igb_uio`` kernel module using the command
   ``modprobe vfio-pci`` or ``modprobe uio; insmod igb_uio.ko wc_activate=1``
   respectively.

#. For ``vfio-pci`` users only:
   Please make sure that ``IOMMU`` is enabled in your system,
   or use ``vfio`` driver in ``noiommu`` mode::

     echo 1 > /sys/module/vfio/parameters/enable_unsafe_noiommu_mode

#. Bind the intended ENA device to ``vfio-pci`` or ``igb_uio`` module.


At this point the system should be ready to run DPDK applications. Once the
application runs to completion, the ENA can be detached from igb_uio if necessary.

Usage example
-------------

Follow instructions available in the document
:ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>` to launch
**testpmd** with Amazon ENA devices managed by librte_pmd_ena.

Example output:

.. code-block:: console

   [...]
   EAL: PCI device 0000:00:06.0 on NUMA socket -1
   EAL:   Invalid NUMA socket, default to 0
   EAL:   probe driver: 1d0f:ec20 net_ena

   Interactive-mode selected
   testpmd: create a new mbuf pool <mbuf_pool_socket_0>: n=171456, size=2176, socket=0
   testpmd: preferred mempool ops selected: ring_mp_mc
   Warning! port-topology=paired and odd forward ports number, the last port will pair with itself.
   Configuring Port 0 (socket 0)
   Port 0: 00:00:00:11:00:01
   Checking link statuses...

   Done
   testpmd>
