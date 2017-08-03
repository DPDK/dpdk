.. BSD LICENSE

    Copyright (c) 2015-2017 Atomic Rules LLC
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
    * Neither the name of Atomic Rules LLC nor the names of its
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

ARK Poll Mode Driver
====================

The ARK PMD is a DPDK poll-mode driver for the Atomic Rules Arkville
(ARK) family of devices.

More information can be found at the `Atomic Rules website
<http://atomicrules.com>`_.

Overview
--------

The Atomic Rules Arkville product is DPDK and AXI compliant product
that marshals packets across a PCIe conduit between host DPDK mbufs and
FPGA AXI streams.

The ARK PMD, and the spirit of the overall Arkville product,
has been to take the DPDK API/ABI as a fixed specification;
then implement much of the business logic in FPGA RTL circuits.
The approach of *working backwards* from the DPDK API/ABI and having
the GPP host software *dictate*, while the FPGA hardware *copes*,
results in significant performance gains over a naive implementation.

While this document describes the ARK PMD software, it is helpful to
understand what the FPGA hardware is and is not. The Arkville RTL
component provides a single PCIe Physical Function (PF) supporting
some number of RX/Ingress and TX/Egress Queues. The ARK PMD controls
the Arkville core through a dedicated opaque Core BAR (CBAR).
To allow users full freedom for their own FPGA application IP,
an independent FPGA Application BAR (ABAR) is provided.

One popular way to imagine Arkville's FPGA hardware aspect is as the
FPGA PCIe-facing side of a so-called Smart NIC. The Arkville core does
not contain any MACs, and is link-speed independent, as well as
agnostic to the number of physical ports the application chooses to
use. The ARK driver exposes the familiar PMD interface to allow packet
movement to and from mbufs across multiple queues.

However FPGA RTL applications could contain a universe of added
functionality that an Arkville RTL core does not provide or can
not anticipate. To allow for this expectation of user-defined
innovation, the ARK PMD provides a dynamic mechanism of adding
capabilities without having to modify the ARK PMD.

The ARK PMD is intended to support all instances of the Arkville
RTL Core, regardless of configuration, FPGA vendor, or target
board. While specific capabilities such as number of physical
hardware queue-pairs are negotiated; the driver is designed to
remain constant over a broad and extendable feature set.

Intentionally, Arkville by itself DOES NOT provide common NIC
capabilities such as offload or receive-side scaling (RSS).
These capabilities would be viewed as a gate-level "tax" on
Green-box FPGA applications that do not require such function.
Instead, they can be added as needed with essentially no
overhead to the FPGA Application.

The ARK PMD also supports optional user extensions, through dynamic linking.
The ARK PMD user extensions are a feature of Arkville’s DPDK
net/ark poll mode driver, allowing users to add their
own code to extend the net/ark functionality without
having to make source code changes to the driver. One motivation for
this capability is that while DPDK provides a rich set of functions
to interact with NIC-like capabilities (e.g. MAC addresses and statistics),
the Arkville RTL IP does not include a MAC.  Users can supply their
own MAC or custom FPGA applications, which may require control from
the PMD.  The user extension is the means providing the control
between the user's FPGA application and the existing DPDK features via
the PMD.

Device Parameters
-------------------

The ARK PMD supports device parameters that are used for packet
routing and for internal packet generation and packet checking.  This
section describes the supported parameters.  These features are
primarily used for diagnostics, testing, and performance verification
under the guidance of an Arkville specialist.  The nominal use of
Arkville does not require any configuration using these parameters.

"Pkt_dir"

The Packet Director controls connectivity between Arkville's internal
hardware components. The features of the Pkt_dir are only used for
diagnostics and testing; it is not intended for nominal use.  The full
set of features are not published at this level.

Format:
Pkt_dir=0x00110F10

"Pkt_gen"

The packet generator parameter takes a file as its argument.  The file
contains configuration parameters used internally for regression
testing and are not intended to be published at this level.  The
packet generator is an internal Arkville hardware component.

Format:
Pkt_gen=./config/pg.conf

"Pkt_chkr"

The packet checker parameter takes a file as its argument.  The file
contains configuration parameters used internally for regression
testing and are not intended to be published at this level.  The
packet checker is an internal Arkville hardware component.

Format:
Pkt_chkr=./config/pc.conf


Data Path Interface
-------------------

Ingress RX and Egress TX operation is by the nominal DPDK API .
The driver supports single-port, multi-queue for both RX and TX.

Configuration Information
-------------------------

**DPDK Configuration Parameters**

  The following configuration options are available for the ARK PMD:

   * **CONFIG_RTE_LIBRTE_ARK_PMD** (default y): Enables or disables inclusion
     of the ARK PMD driver in the DPDK compilation.

   * **CONFIG_RTE_LIBRTE_ARK_PAD_TX** (default y):  When enabled TX
     packets are padded to 60 bytes to support downstream MACS.

   * **CONFIG_RTE_LIBRTE_ARK_DEBUG_RX** (default n): Enables or disables debug
     logging and internal checking of RX ingress logic within the ARK PMD driver.

   * **CONFIG_RTE_LIBRTE_ARK_DEBUG_TX** (default n): Enables or disables debug
     logging and internal checking of TX egress logic within the ARK PMD driver.

   * **CONFIG_RTE_LIBRTE_ARK_DEBUG_STATS** (default n): Enables or disables debug
     logging of detailed packet and performance statistics gathered in
     the PMD and FPGA.

   * **CONFIG_RTE_LIBRTE_ARK_DEBUG_TRACE** (default n): Enables or disables debug
     logging of detailed PMD events and status.


Building DPDK
-------------

See the :ref:`DPDK Getting Started Guide for Linux <linux_gsg>` for
instructions on how to build DPDK.

By default the ARK PMD library will be built into the DPDK library.

For configuring and using UIO and VFIO frameworks, please also refer :ref:`the
documentation that comes with DPDK suite <linux_gsg>`.

Supported ARK RTL PCIe Instances
--------------------------------

ARK PMD supports the following Arkville RTL PCIe instances including:

* ``1d6c:100d`` - AR-ARKA-FX0 [Arkville 32B DPDK Data Mover]
* ``1d6c:100e`` - AR-ARKA-FX1 [Arkville 64B DPDK Data Mover]

Supported Operating Systems
---------------------------

Any Linux distribution fulfilling the conditions described in ``System Requirements``
section of :ref:`the DPDK documentation <linux_gsg>` or refer to *DPDK
Release Notes*.  ARM and PowerPC architectures are not supported at this time.


Supported Features
------------------

* Dynamic ARK PMD extensions
* Multiple receive and transmit queues
* Jumbo frames up to 9K
* Hardware Statistics

Unsupported Features
--------------------

Features that may be part of, or become part of, the Arkville RTL IP that are
not currently supported or exposed by the ARK PMD include:

* PCIe SR-IOV Virtual Functions (VFs)
* Arkville's Packet Generator Control and Status
* Arkville's Packet Director Control and Status
* Arkville's Packet Checker Control and Status
* Arkville's Timebase Management

Pre-Requisites
--------------

#. Prepare the system as recommended by DPDK suite.  This includes environment
   variables, hugepages configuration, tool-chains and configuration

#. Insert igb_uio kernel module using the command 'modprobe igb_uio'

#. Bind the intended ARK device to igb_uio module

At this point the system should be ready to run DPDK applications. Once the
application runs to completion, the ARK PMD can be detached from igb_uio if necessary.

Usage Example
-------------

Follow instructions available in the document
:ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>` to launch
**testpmd** with Atomic Rules ARK devices managed by librte_pmd_ark.

Example output:

.. code-block:: console

   [...]
   EAL: PCI device 0000:01:00.0 on NUMA socket -1
   EAL:   probe driver: 1d6c:100e rte_ark_pmd
   EAL:   PCI memory mapped at 0x7f9b6c400000
   PMD: eth_ark_dev_init(): Initializing 0:2:0.1
   ARKP PMD CommitID: 378f3a67
   Configuring Port 0 (socket 0)
   Port 0: DC:3C:F6:00:00:01
   Checking link statuses...
   Port 0 Link Up - speed 100000 Mbps - full-duplex
   Done
   testpmd>
