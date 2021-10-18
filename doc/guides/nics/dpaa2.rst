..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016,2020-2021 NXP


DPAA2 Poll Mode Driver
======================

The DPAA2 NIC PMD (**librte_net_dpaa2**) provides poll mode driver
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

.. code-block:: console


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

The DPBP driver is bound to a DPBP objects and provides services to
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
- :ref:`Traffic Management API <dptmapi>`


Supported DPAA2 SoCs
--------------------
- LX2160A
- LS2084A/LS2044A
- LS2088A/LS2048A
- LS1088A/LS1048A

Prerequisites
-------------

See :doc:`../platform/dpaa2` for setup information

- Follow the DPDK :ref:`Getting Started Guide for Linux <linux_gsg>` to setup the basic DPDK environment.

.. note::

   Some part of fslmc bus code (mc flib - object library) routines are
   dual licensed (BSD & GPLv2), however they are used as BSD in DPDK in userspace.


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

      ./dpdk-testpmd -c 0xff -n 1 -- -i --portmask=0x3 --nb-cores=1 --no-flush-rx

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


* Use dev arg option ``drv_loopback=1`` to loopback packets at
  driver level. Any packet received will be reflected back by the
  driver on same port. e.g. ``fslmc:dpni.1,drv_loopback=1``

* Use dev arg option ``drv_no_prefetch=1`` to disable prefetching
  of the packet pull command which is issued  in the previous cycle.
  e.g. ``fslmc:dpni.1,drv_no_prefetch=1``

* Use dev arg option  ``drv_tx_conf=1`` to enable TX confirmation mode.
  In this mode tx conf queues need to be polled to free the buffers.
  e.g. ``fslmc:dpni.1,drv_tx_conf=1``

* Use dev arg option  ``drv_error_queue=1`` to enable Packets in Error queue.
  DPAA2 hardware drops the error packet in hardware. This option enables the
  hardware to not drop the error packet and let the driver dump the error
  packets, so that user can check what is wrong with those packets.
  e.g. ``fslmc:dpni.1,drv_error_queue=1``

Enabling logs
-------------

For enabling logging for DPAA2 PMD, following log-level prefix can be used:

 .. code-block:: console

    <dpdk app> <EAL args> --log-level=bus.fslmc:<level> -- ...

Using ``bus.fslmc`` as log matching criteria, all FSLMC bus logs can be enabled
which are lower than logging ``level``.

 Or

 .. code-block:: console

    <dpdk app> <EAL args> --log-level=pmd.net.dpaa2:<level> -- ...

Using ``pmd.net.dpaa2`` as log matching criteria, all PMD logs can be enabled
which are lower than logging ``level``.

Allowing & Blocking
-------------------

For blocking a DPAA2 device, following commands can be used.

 .. code-block:: console

    <dpdk app> <EAL args> -b "fslmc:dpni.x" -- ...

Where x is the device object id as configured in resource container.

Running secondary debug app without blocklist
---------------------------------------------

dpaa2 hardware imposes limits on some H/W access devices like Management
Control Port and H/W portal. This causes issue in their shared usages in
case of multi-process applications. It can overcome by using
allowlist/blocklist in primary and secondary applications.

In order to ease usage of standard debugging apps like dpdk-procinfo, dpaa2
driver reserves extra Management Control Port and H/W portal which can be
used by debug application to debug any existing application without
blocking these devices in primary process.

Limitations
-----------

Platform Requirement
~~~~~~~~~~~~~~~~~~~~
DPAA2 drivers for DPDK can only work on NXP SoCs as listed in the
``Supported DPAA2 SoCs``.

Maximum packet length
~~~~~~~~~~~~~~~~~~~~~

The DPAA2 SoC family support a maximum of a 10240 jumbo frame. The value
is fixed and cannot be changed. So, even when the ``rxmode.mtu``
member of ``struct rte_eth_conf`` is set to a value lower than 10240, frames
up to 10240 bytes can still reach the host interface.

Other Limitations
~~~~~~~~~~~~~~~~~

- RSS hash key cannot be modified.
- RSS RETA cannot be configured.

.. _dptmapi:

Traffic Management API
----------------------

DPAA2 PMD supports generic DPDK Traffic Management API which allows to
configure the following features:

1. Hierarchical scheduling
2. Traffic shaping

Internally TM is represented by a hierarchy (tree) of nodes.
Node which has a parent is called a leaf whereas node without
parent is called a non-leaf (root).

Nodes hold following types of settings:

- for egress scheduler configuration: weight
- for egress rate limiter: private shaper

Hierarchy is always constructed from the top, i.e first a root node is added
then some number of leaf nodes. Number of leaf nodes cannot exceed number
of configured tx queues.

After hierarchy is complete it can be committed.

For an additional description please refer to DPDK :doc:`Traffic Management API <../prog_guide/traffic_management>`.

Supported Features
~~~~~~~~~~~~~~~~~~

The following capabilities are supported:

- Level0 (root node) and Level1 are supported.
- 1 private shaper at root node (port level) is supported.
- 8 TX queues per port supported (1 channel per port)
- Both SP and WFQ scheduling mechanisms are supported on all 8 queues.
- Congestion notification is supported. It means if there is congestion on
    the network, DPDK driver will not enqueue any packet (no taildrop or WRED)

  User can also check node, level capabilities using testpmd commands.

Usage example
~~~~~~~~~~~~~

For a detailed usage description please refer to "Traffic Management" section in DPDK :doc:`Testpmd Runtime Functions <../testpmd_app_ug/testpmd_funcs>`.

1. Run testpmd as follows:

   .. code-block:: console

	./dpdk-testpmd  -c 0xf -n 1 -- -i --portmask 0x3 --nb-cores=1 --txq=4 --rxq=4

2. Stop all ports:

   .. code-block:: console

	testpmd> port stop all

3. Add shaper profile:

   One port level shaper and strict priority on all 4 queues of port 0:

   .. code-block:: console

	add port tm node shaper profile 0 1 104857600 64 100 0 0
	add port tm nonleaf node 0 8 -1 0 1 0 1 1 1 0
	add port tm leaf node 0 0 8 0 1 1 -1 0 0 0 0
	add port tm leaf node 0 1 8 1 1 1 -1 0 0 0 0
	add port tm leaf node 0 2 8 2 1 1 -1 0 0 0 0
	add port tm leaf node 0 3 8 3 1 1 -1 0 0 0 0
	port tm hierarchy commit 0 no

	or

   One port level shaper and WFQ on all 4 queues of port 0:

   .. code-block:: console

	add port tm node shaper profile 0 1 104857600 64 100 0 0
	add port tm nonleaf node 0 8 -1 0 1 0 1 1 1 0
	add port tm leaf node 0 0 8 0 200 1 -1 0 0 0 0
	add port tm leaf node 0 1 8 0 300 1 -1 0 0 0 0
	add port tm leaf node 0 2 8 0 400 1 -1 0 0 0 0
	add port tm leaf node 0 3 8 0 500 1 -1 0 0 0 0
	port tm hierarchy commit 0 no

4. Create flows as per the source IP addresses:

   .. code-block:: console

	flow create 1 group 0 priority 1 ingress pattern ipv4 src is \
	10.10.10.1 / end actions queue index 0 / end
	flow create 1 group 0 priority 2 ingress pattern ipv4 src is \
	10.10.10.2 / end actions queue index 1 / end
	flow create 1 group 0 priority 3 ingress pattern ipv4 src is \
	10.10.10.3 / end actions queue index 2 / end
	flow create 1 group 0 priority 4 ingress pattern ipv4 src is \
	10.10.10.4 / end actions queue index 3 / end

5. Start all ports

   .. code-block:: console

	testpmd> port start all



6. Enable forwarding

   .. code-block:: console

		testpmd> start

7. Inject the traffic on port1 as per the configured flows, you will see shaped and scheduled forwarded traffic on port0
