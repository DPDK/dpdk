..  BSD LICENSE
    Copyright 2016 NXP.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of NXP nor the names of its
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

DPAA2 Poll Mode Driver
======================

The DPAA2 NIC PMD (**librte_pmd_dpaa2**) provides poll mode driver
support for the inbuilt NIC found in the **NXP DPAA2** SoC family.

More information can be found at `NXP Official Website
<http://www.nxp.com/products/microcontrollers-and-processors/arm-processors/qoriq-arm-processors:QORIQ-ARM>`_.

NXP DPAA2 (Data Path Acceleration Architecture Gen2)
----------------------------------------------------

This section provides an overview of the NXP DPAA2 architecture
and how it is integrated into the DPDK.

Contents summary

- DPAA2 overview
- Overview of DPAA2 objects
- DPAA2 driver architecture overview

.. _dpaa2_overview:

DPAA2 Overview
~~~~~~~~~~~~~~

Reference: `FSL MC BUS in Linux Kernel <https://www.kernel.org/doc/readme/drivers-staging-fsl-mc-README.txt>`_.

DPAA2 is a hardware architecture designed for high-speed network
packet processing.  DPAA2 consists of sophisticated mechanisms for
processing Ethernet packets, queue management, buffer management,
autonomous L2 switching, virtual Ethernet bridging, and accelerator
(e.g. crypto) sharing.

A DPAA2 hardware component called the Management Complex (or MC) manages the
DPAA2 hardware resources.  The MC provides an object-based abstraction for
software drivers to use the DPAA2 hardware.

The MC uses DPAA2 hardware resources such as queues, buffer pools, and
network ports to create functional objects/devices such as network
interfaces, an L2 switch, or accelerator instances.

The MC provides memory-mapped I/O command interfaces (MC portals)
which DPAA2 software drivers use to operate on DPAA2 objects:

The diagram below shows an overview of the DPAA2 resource management
architecture:

.. code-block:: console

  +--------------------------------------+
  |                  OS                  |
  |                        DPAA2 drivers |
  |                             |        |
  +-----------------------------|--------+
                                |
                                | (create,discover,connect
                                |  config,use,destroy)
                                |
                  DPAA2         |
  +------------------------| mc portal |-+
  |                             |        |
  |   +- - - - - - - - - - - - -V- - -+  |
  |   |                               |  |
  |   |   Management Complex (MC)     |  |
  |   |                               |  |
  |   +- - - - - - - - - - - - - - - -+  |
  |                                      |
  | Hardware                  Hardware   |
  | Resources                 Objects    |
  | ---------                 -------    |
  | -queues                   -DPRC      |
  | -buffer pools             -DPMCP     |
  | -Eth MACs/ports           -DPIO      |
  | -network interface        -DPNI      |
  |  profiles                 -DPMAC     |
  | -queue portals            -DPBP      |
  | -MC portals                ...       |
  |  ...                                 |
  |                                      |
  +--------------------------------------+

The MC mediates operations such as create, discover,
connect, configuration, and destroy.  Fast-path operations
on data, such as packet transmit/receive, are not mediated by
the MC and are done directly using memory mapped regions in
DPIO objects.

Overview of DPAA2 Objects
~~~~~~~~~~~~~~~~~~~~~~~~~

The section provides a brief overview of some key DPAA2 objects.
A simple scenario is described illustrating the objects involved
in creating a network interfaces.

DPRC (Datapath Resource Container)

 A DPRC is a container object that holds all the other
 types of DPAA2 objects.  In the example diagram below there
 are 8 objects of 5 types (DPMCP, DPIO, DPBP, DPNI, and DPMAC)
 in the container.

.. code-block:: console

    +---------------------------------------------------------+
    | DPRC                                                    |
    |                                                         |
    |  +-------+  +-------+  +-------+  +-------+  +-------+  |
    |  | DPMCP |  | DPIO  |  | DPBP  |  | DPNI  |  | DPMAC |  |
    |  +-------+  +-------+  +-------+  +---+---+  +---+---+  |
    |  | DPMCP |  | DPIO  |                                   |
    |  +-------+  +-------+                                   |
    |  | DPMCP |                                              |
    |  +-------+                                              |
    |                                                         |
    +---------------------------------------------------------+

From the point of view of an OS, a DPRC behaves similar to a plug and
play bus, like PCI.  DPRC commands can be used to enumerate the contents
of the DPRC, discover the hardware objects present (including mappable
regions and interrupts).

.. code-block:: console

    DPRC.1 (bus)
      |
      +--+--------+-------+-------+-------+
         |        |       |       |       |
       DPMCP.1  DPIO.1  DPBP.1  DPNI.1  DPMAC.1
       DPMCP.2  DPIO.2
       DPMCP.3

Hardware objects can be created and destroyed dynamically, providing
the ability to hot plug/unplug objects in and out of the DPRC.

A DPRC has a mappable MMIO region (an MC portal) that can be used
to send MC commands.  It has an interrupt for status events (like
hotplug).

All objects in a container share the same hardware "isolation context".
This means that with respect to an IOMMU the isolation granularity
is at the DPRC (container) level, not at the individual object
level.

DPRCs can be defined statically and populated with objects
via a config file passed to the MC when firmware starts
it.  There is also a Linux user space tool called "restool"
that can be used to create/destroy containers and objects
dynamically.

DPAA2 Objects for an Ethernet Network Interface
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

A typical Ethernet NIC is monolithic-- the NIC device contains TX/RX
queuing mechanisms, configuration mechanisms, buffer management,
physical ports, and interrupts.  DPAA2 uses a more granular approach
utilizing multiple hardware objects.  Each object provides specialized
functions. Groups of these objects are used by software to provide
Ethernet network interface functionality.  This approach provides
efficient use of finite hardware resources, flexibility, and
performance advantages.

The diagram below shows the objects needed for a simple
network interface configuration on a system with 2 CPUs.

.. code-block:: console

    +---+---+ +---+---+
       CPU0     CPU1
    +---+---+ +---+---+
        |         |
    +---+---+ +---+---+
       DPIO     DPIO
    +---+---+ +---+---+
          \     /
           \   /
            \ /
         +---+---+
            DPNI  --- DPBP,DPMCP
         +---+---+
             |
             |
         +---+---+
           DPMAC
         +---+---+
             |
          port/PHY

Below the objects are described.  For each object a brief description
is provided along with a summary of the kinds of operations the object
supports and a summary of key resources of the object (MMIO regions
and IRQs).

DPMAC (Datapath Ethernet MAC): represents an Ethernet MAC, a
hardware device that connects to an Ethernet PHY and allows
physical transmission and reception of Ethernet frames.

- MMIO regions: none
- IRQs: DPNI link change
- commands: set link up/down, link config, get stats, IRQ config, enable, reset

DPNI (Datapath Network Interface): contains TX/RX queues,
network interface configuration, and RX buffer pool configuration
mechanisms.  The TX/RX queues are in memory and are identified by
queue number.

- MMIO regions: none
- IRQs: link state
- commands: port config, offload config, queue config, parse/classify config, IRQ config, enable, reset

DPIO (Datapath I/O): provides interfaces to enqueue and dequeue
packets and do hardware buffer pool management operations.  The DPAA2
architecture separates the mechanism to access queues (the DPIO object)
from the queues themselves.  The DPIO provides an MMIO interface to
enqueue/dequeue packets.  To enqueue something a descriptor is written
to the DPIO MMIO region, which includes the target queue number.
There will typically be one DPIO assigned to each CPU.  This allows all
CPUs to simultaneously perform enqueue/dequeued operations.  DPIOs are
expected to be shared by different DPAA2 drivers.

- MMIO regions: queue operations, buffer management
- IRQs: data availability, congestion notification, buffer pool depletion
- commands: IRQ config, enable, reset

DPBP (Datapath Buffer Pool): represents a hardware buffer
pool.

- MMIO regions: none
- IRQs: none
- commands: enable, reset

DPMCP (Datapath MC Portal): provides an MC command portal.
Used by drivers to send commands to the MC to manage
objects.

- MMIO regions: MC command portal
- IRQs: command completion
- commands: IRQ config, enable, reset

Object Connections
~~~~~~~~~~~~~~~~~~

Some objects have explicit relationships that must
be configured:

- DPNI <--> DPMAC
- DPNI <--> DPNI
- DPNI <--> L2-switch-port

A DPNI must be connected to something such as a DPMAC,
another DPNI, or L2 switch port.  The DPNI connection
is made via a DPRC command.

.. code-block:: console

    +-------+  +-------+
    | DPNI  |  | DPMAC |
    +---+---+  +---+---+
        |          |
        +==========+

- DPNI <--> DPBP

A network interface requires a 'buffer pool' (DPBP object) which provides
a list of pointers to memory where received Ethernet data is to be copied.
The Ethernet driver configures the DPBPs associated with the network
interface.

Interrupts
~~~~~~~~~~

All interrupts generated by DPAA2 objects are message
interrupts.  At the hardware level message interrupts
generated by devices will normally have 3 components--
1) a non-spoofable 'device-id' expressed on the hardware
bus, 2) an address, 3) a data value.

In the case of DPAA2 devices/objects, all objects in the
same container/DPRC share the same 'device-id'.
For ARM-based SoC this is the same as the stream ID.


DPAA2 DPDK - Poll Mode Driver Overview
--------------------------------------

This section provides an overview of the drivers for
DPAA2-- 1) the bus driver and associated "DPAA2 infrastructure"
drivers and 2) functional object drivers (such as Ethernet).

As described previously, a DPRC is a container that holds the other
types of DPAA2 objects.  It is functionally similar to a plug-and-play
bus controller.

Each object in the DPRC is a Linux "device" and is bound to a driver.
The diagram below shows the dpaa2 drivers involved in a networking
scenario and the objects bound to each driver.  A brief description
of each driver follows.

.. code-block: console


                                       +------------+
                                       | DPDK DPAA2 |
                                       |     PMD    |
                                       +------------+       +------------+
                                       |  Ethernet  |.......|  Mempool   |
                    . . . . . . . . .  |   (DPNI)   |       |  (DPBP)    |
                   .                   +---+---+----+       +-----+------+
                  .                        ^   |                  .
                 .                         |   |<enqueue,         .
                .                          |   | dequeue>         .
               .                           |   |                  .
              .                        +---+---V----+             .
             .      . . . . . . . . . .| DPIO driver|             .
            .      .                   |  (DPIO)    |             .
           .      .                    +-----+------+             .
          .      .                     |  QBMAN     |             .
         .      .                      |  Driver    |             .
    +----+------+-------+              +-----+----- |             .
    |   dpaa2 bus       |                    |                    .
    |   VFIO fslmc-bus  |....................|.....................
    |                   |                    |
    |     /bus/fslmc    |                    |
    +-------------------+                    |
                                             |
    ========================== HARDWARE =====|=======================
                                           DPIO
                                             |
                                           DPNI---DPBP
                                             |
                                           DPMAC
                                             |
                                            PHY
    =========================================|========================


A brief description of each driver is provided below.

DPAA2 bus driver
~~~~~~~~~~~~~~~~

The DPAA2 bus driver is a rte_bus driver which scans the fsl-mc bus.
Key functions include:

- Reading the container and setting up vfio group
- Scanning and parsing the various MC objects and adding them to
  their respective device list.

Additionally, it also provides the object driver for generic MC objects.

DPIO driver
~~~~~~~~~~~

The DPIO driver is bound to DPIO objects and provides services that allow
other drivers such as the Ethernet driver to enqueue and dequeue data for
their respective objects.
Key services include:

- Data availability notifications
- Hardware queuing operations (enqueue and dequeue of data)
- Hardware buffer pool management

To transmit a packet the Ethernet driver puts data on a queue and
invokes a DPIO API.  For receive, the Ethernet driver registers
a data availability notification callback.  To dequeue a packet
a DPIO API is used.

There is typically one DPIO object per physical CPU for optimum
performance, allowing different CPUs to simultaneously enqueue
and dequeue data.

The DPIO driver operates on behalf of all DPAA2 drivers
active  --  Ethernet, crypto, compression, etc.

DPBP based Mempool driver
~~~~~~~~~~~~~~~~~~~~~~~~~

The DPBP driver is bound to a DPBP objects and provides sevices to
create a hardware offloaded packet buffer mempool.

DPAA2 NIC Driver
~~~~~~~~~~~~~~~~
The Ethernet driver is bound to a DPNI and implements the kernel
interfaces needed to connect the DPAA2 network interface to
the network stack.

Each DPNI corresponds to a DPDK network interface.

Features
^^^^^^^^

Features of the DPAA2 PMD are:

- Multiple queues for TX and RX
- Receive Side Scaling (RSS)
- MAC/VLAN filtering
- Packet type information
- Checksum offload
- Promiscuous mode
- Multicast mode
- Port hardware statistics
- Jumbo frames
- Link flow control
- Scattered and gather for TX and RX

Supported DPAA2 SoCs
--------------------

- LS2080A/LS2040A
- LS2084A/LS2044A
- LS2088A/LS2048A
- LS1088A/LS1048A

Prerequisites
-------------

There are three main pre-requisities for executing DPAA2 PMD on a DPAA2
compatible board:

1. **ARM 64 Tool Chain**

   For example, the `*aarch64* Linaro Toolchain <https://releases.linaro.org/components/toolchain/binaries/4.9-2017.01/aarch64-linux-gnu>`_.

2. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://github.com/qoriq-open-source/linux>`_.

3. **Rootfile system**

   Any *aarch64* supporting filesystem can be used. For example,
   Ubuntu 15.10 (Wily) or 16.04 LTS (Xenial) userland which can be obtained
   from `here <http://cdimage.ubuntu.com/ubuntu-base/releases/16.04/release/ubuntu-base-16.04.1-base-arm64.tar.gz>`_.

As an alternative method, DPAA2 PMD can also be executed using images provided
as part of SDK from NXP. The SDK includes all the above prerequisites necessary
to bring up a DPAA2 board.

The following dependencies are not part of DPDK and must be installed
separately:

- **NXP Linux SDK**

  NXP Linux software development kit (SDK) includes support for family
  of QorIQ® ARM-Architecture-based system on chip (SoC) processors
  and corresponding boards.

  It includes the Linux board support packages (BSPs) for NXP SoCs,
  a fully operational tool chain, kernel and board specific modules.

  SDK and related information can be obtained from:  `NXP QorIQ SDK  <http://www.nxp.com/products/software-and-tools/run-time-software/linux-sdk/linux-sdk-for-qoriq-processors:SDKLINUX>`_.

- **DPDK Extra Scripts**

  DPAA2 based resources can be configured easily with the help of ready scripts
  as provided in the DPDK Extra repository.

  `DPDK Extras Scripts <https://github.com/qoriq-open-source/dpdk-extras>`_.

Currently supported by DPDK:

- NXP SDK **2.0+**.
- MC Firmware version **10.0.0** and higher.
- Supported architectures:  **arm64 LE**.

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

.. note::

   Some part of fslmc bus code (mc flib - object library) routines are
   dual licensed (BSD & GPLv2).

Pre-Installation Configuration
------------------------------

Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_FSLMC_BUS`` (default ``n``)

  By default it is enabled only for defconfig_arm64-dpaa2-* config.
  Toggle compilation of the ``librte_bus_fslmc`` driver.

- ``CONFIG_RTE_LIBRTE_DPAA2_PMD`` (default ``n``)

  By default it is enabled only for defconfig_arm64-dpaa2-* config.
  Toggle compilation of the ``librte_pmd_dpaa2`` driver.

- ``CONFIG_RTE_LIBRTE_DPAA2_DEBUG_DRIVER`` (default ``n``)

  Toggle display of generic debugging messages

- ``CONFIG_RTE_LIBRTE_DPAA2_USE_PHYS_IOVA`` (default ``y``)

  Toggle to use physical address vs virtual address for hardware accelerators.

- ``CONFIG_RTE_LIBRTE_DPAA2_DEBUG_INIT`` (default ``n``)

  Toggle display of initialization related messages.

- ``CONFIG_RTE_LIBRTE_DPAA2_DEBUG_RX`` (default ``n``)

  Toggle display of receive fast path run-time message

- ``CONFIG_RTE_LIBRTE_DPAA2_DEBUG_TX`` (default ``n``)

  Toggle display of transmit fast path run-time message

- ``CONFIG_RTE_LIBRTE_DPAA2_DEBUG_TX_FREE`` (default ``n``)

  Toggle display of transmit fast path buffer free run-time message

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

#. Running testpmd:

   Follow instructions available in the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
   to run testpmd.

   Example output:

   .. code-block:: console

      ./arm64-dpaa2-linuxapp-gcc/testpmd -c 0xff -n 1 \
        -- -i --portmask=0x3 --nb-cores=1 --no-flush-rx

      .....
      EAL: Registered [pci] bus.
      EAL: Registered [fslmc] bus.
      EAL: Detected 8 lcore(s)
      EAL: Probing VFIO support...
      EAL: VFIO support initialized
      .....
      PMD: DPAA2: Processing Container = dprc.2
      EAL: fslmc: DPRC contains = 51 devices
      EAL: fslmc: Bus scan completed
      .....
      Configuring Port 0 (socket 0)
      Port 0: 00:00:00:00:00:01
      Configuring Port 1 (socket 0)
      Port 1: 00:00:00:00:00:02
      .....
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>

Limitations
-----------

Platform Requirement
~~~~~~~~~~~~~~~~~~~~
DPAA2 drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA2 SoCs``.

Maximum packet length
~~~~~~~~~~~~~~~~~~~~~

The DPAA2 SoC family support a maximum of a 10240 jumbo frame. The value
is fixed and cannot be changed. So, even when the ``rxmode.max_rx_pkt_len``
member of ``struct rte_eth_conf`` is set to a value lower than 10240, frames
up to 10240 bytes can still reach the host interface.

Other Limitations
~~~~~~~~~~~~~~~~~

- RSS hash key cannot be modified.
- RSS RETA cannot be configured.
- Secondary process packet I/O is not supported.
