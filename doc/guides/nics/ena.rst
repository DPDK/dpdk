.. BSD LICENSE

    Copyright (c) 2015-2016 Amazon.com, Inc. or its affiliates.
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


   * **CONFIG_RTE_LIBRTE_ENA_DEBUG_INIT** (default y): Enables or disables debug
     logging of device initialization within the ENA PMD driver.

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

* Jumbo frames up to 9K
* Port Hardware Statistics
* IPv4/TCP/UDP checksum offload
* TSO offload
* Multiple receive and transmit queues
* RSS
* Low Latency Queue for Tx

Unsupported features
--------------------

The features supported by the device and not yet supported by this PMD include:

* Asynchronous Event Notification Queue (AENQ)

Prerequisites
-------------

#. Prepare the system as recommended by DPDK suite.  This includes environment
   variables, hugepages configuration, tool-chains and configuration

#. Insert igb_uio kernel module using the command 'modprobe igb_uio'

#. Bind the intended ENA device to igb_uio module


At this point the system should be ready to run DPDK applications. Once the
application runs to completion, the ENA can be detached from igb_uio if necessary.

Usage example
-------------

This section demonstrates how to launch **testpmd** with Amazon ENA
devices managed by librte_pmd_ena.

#. Load the kernel modules:

   .. code-block:: console

      modprobe uio
      insmod ./x86_64-native-linuxapp-gcc/kmod/igb_uio.ko

   .. note::

      Currently Amazon ENA PMD driver depends on igb_uio user space I/O kernel module

#. Mount and request huge pages:

   .. code-block:: console

      mount -t hugetlbfs nodev /mnt/hugepages
      echo 1024 > /sys/kernel/mm/hugepages/hugepages-2048kB/nr_hugepages

#. Bind UIO driver to ENA device (using provided by DPDK binding tool):

   .. code-block:: console

      ./tools/dpdk-devbind.py --bind=igb_uio 0000:02:00.1

#. Start testpmd with basic parameters:

   .. code-block:: console

      ./x86_64-native-linuxapp-gcc/app/testpmd -c 0xf -n 4 -- -i

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:02:00.1 on NUMA socket -1
      EAL:   probe driver: 1d0f:ec20 rte_ena_pmd
      EAL:   PCI memory mapped at 0x7f9b6c400000
      PMD: eth_ena_dev_init(): Initializing 0:2:0.1
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      Port 0: 00:00:00:11:00:01
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>
