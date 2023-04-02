.. SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2015-2021 Atomic Rules LLC
    All rights reserved.

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

* FPGA Vendors Supported: AMD/Xilinx and Intel
* Number of RX/TX Queue-Pairs: up to 128
* PCIe Endpoint Technology: Gen3, Gen4, Gen5

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
-----------------

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

**DPDK Configuration Parameter**

   * **RTE_LIBRTE_ARK_MIN_TX_PKTLEN** (default 0): Sets the minimum
     packet length for tx packets to the FPGA.  Packets less than this
     length are padded to meet the requirement. This allows padding to
     be offloaded or remain in host software.


Dynamic PMD Extension
---------------------

Dynamic PMD extensions allow users to customize net/ark functionality
using their own code. Arkville RTL and this PMD support high-throughput data
movement, and these extensions allow PMD support for users' FPGA
features.
Dynamic PMD extensions operate by having users supply a shared object
file which is loaded by Arkville PMD during initialization.  The
object file contains extension (or hook) functions that are registered
and then called during PMD operations.

The allowable set of extension functions are defined and documented in
``ark_ext.h``, only the initialization function,
``rte_pmd_ark_dev_init()``, is required; all others are optional. The
following sections give a small extension example along with
instructions for compiling and using the extension.


Extension Example
^^^^^^^^^^^^^^^^^

The following example shows an extension which populates mbuf fields
during RX from user meta data coming from FPGA hardware.

.. code-block:: c

   #include <ark_ext.h>
   #include <rte_mbuf.h>
   #include <rte_ethdev.h>
   #include <rte_malloc.h>

   /* Global structure passed to extension/hook functions */
   struct ark_user_extension {
       int timestamp_dynfield_offset;
   };

   /* RX tuser field based on user's hardware */
   struct user_rx_meta {
      uint64_t timestamp;
      uint32_t rss;
   } __rte_packed;

   /* Create ark_user_extension object for use in other hook functions */
   void *rte_pmd_ark_dev_init(struct rte_eth_dev * dev,
                              void * abar, int port_id )
   {
      RTE_SET_USED(dev);
      RTE_SET_USED(abar);
      fprintf(stderr, "Called Arkville user extension for port %u\n",
              port_id);

      struct ark_user_extension *xdata = rte_zmalloc("macExtS",
             sizeof(struct ark_user_extension), 64);
      if (!xdata)
         return NULL;

      /* register dynfield for rx timestamp */
      rte_mbuf_dyn_rx_timestamp_register(&xdata->timestamp_dynfield_offset,
                                         NULL);

      fprintf(stderr, "timestamp fields offset in extension is %d\n",
              xdata->timestamp_dynfield_offset);
      return xdata;
   }

   /* uninitialization */
   void rte_pmd_ark_dev_uninit(struct rte_eth_dev * dev, void *user_data)
   {
      rte_free(user_data);
   }

   /* Hook function -- called for each RX packet
    * Extract RX timestamp and RSS from meta and place in mbuf
    */
   void rte_pmd_ark_rx_user_meta_hook(struct rte_mbuf *mbuf,
                                      const uint32_t *meta,
                                      void *user_data)
   {
      struct ark_user_extension *xdata = user_data;
      struct user_rx_meta *user_rx = (struct user_rx_meta*)meta;
      *RTE_MBUF_DYNFIELD(mbuf, xdata->timestamp_dynfield_offset, uint64_t*) =
                         user_rx->timestamp;
      mbuf->hash.rss = user_rx->rss;
   }


Compiling Extension
^^^^^^^^^^^^^^^^^^^

It is recommended to the compile the extension code with
``-Wmissing-prototypes`` flag to insure correct function types. Typical
DPDK options will also be needed.


An example command line is give below

.. code-block:: console

    cc `pkg-config --cflags libdpdk` \
    -O3 -DALLOW_EXPERIMENTAL_API -fPIC -Wall -Wmissing-prototypes -c \
    -o pmd_net_ark_ext.o pmd_net_ark_ext.c
    # Linking
    cc -o libfx1_100g_ext.so.1 -shared \
    `pkg-config --libs libdpdk` \
    -Wl,--unresolved-symbols=ignore-all \
    -Wl,-soname,libpmd_net_ark_ext.so.1 pmd_net_ark_ext.o

In a ``Makefile`` this would be

.. code-block:: Makefile

   CFLAGS += $(shell pkg-config --cflags libdpdk)
   CFLAGS += -O3 -DALLOW_EXPERIMENTAL_API -fPIC -Wall -Wmissing-prototypes
   # Linking
   LDFLAGS += $(shell pkg-config --libs libdpdk)
   LDFLAGS += -Wl,--unresolved-symbols=ignore-all -Wl,-soname,libpmd_net_ark_ext.so.1

The application must be linked with the ``-export-dynamic`` flags if any
DPDK or application specific code will called from the extension.


Enabling Extension
^^^^^^^^^^^^^^^^^^

The extensions are enabled in the application through the use of an
environment variable ``ARK_EXT_PATH`` This variable points to the lib
extension file generated above.  For example:

.. code-block:: console

   export ARK_EXT_PATH=$(PWD)/libpmd_net_ark_ext.so.1
   testpmd ...


Building DPDK
-------------

See the :ref:`DPDK Getting Started Guide for Linux <linux_gsg>` for
instructions on how to build DPDK.

By default the ARK PMD library will be built into the DPDK library.

For configuring and using UIO and VFIO frameworks, please also refer :ref:`the
documentation that comes with DPDK suite <linux_gsg>`.

To build with a non-zero minimum tx packet length, set the above macro in your
CFLAGS environment prior to the meson build step. I.e.,

.. code-block:: console

    export CFLAGS="-DRTE_LIBRTE_ARK_MIN_TX_PKTLEN=60"
    meson setup build


Supported ARK RTL PCIe Instances
--------------------------------

ARK PMD supports the following Arkville RTL PCIe instances including:

* ``1d6c:100d`` - AR-ARKA-FX0 [Arkville 32B DPDK Data Mover]
* ``1d6c:100e`` - AR-ARKA-FX1 [Arkville 64B DPDK Data Mover]
* ``1d6c:100f`` - AR-ARKA-FX1 [Arkville 64B DPDK Data Mover for Versal]
* ``1d6c:1010`` - AR-ARKA-FX1 [Arkville 64B DPDK Data Mover for Agilex]
* ``1d6c:1017`` - AR-ARK-FX1 [Arkville 64B Multi-Homed Primary Endpoint]
* ``1d6c:1018`` - AR-ARK-FX1 [Arkville 64B Multi-Homed Secondary Endpoint]
* ``1d6c:1019`` - AR-ARK-FX1 [Arkville 64B Multi-Homed Tertiary Endpoint]
* ``1d6c:101a`` - AR-ARK-SRIOV-FX0 [Arkville 32B Primary Physical Function]
* ``1d6c:101b`` - AR-ARK-SRIOV-FX1 [Arkville 64B Primary Physical Function]
* ``1d6c:101c`` - AR-ARK-SRIOV-VF [Arkville Virtual Function]
* ``1d6c:101e`` - AR-ARKA-FX1 [Arkville 64B DPDK Data Mover for Agilex R-Tile]
* ``1d6c:101f`` - AR-TK242 [2x100GbE Packet Capture Device]
* ``1d6c:1022`` - AR-ARKA-FX2 [Arkville 128B DPDK Data Mover for Agilex]

Arkville RTL Core Configurations
--------------------------------

Arkville's RTL core may be configured by the user with different
datapath widths to balance throughput against FPGA logic area.
The ARK PMD has introspection on the RTL core configuration and acts accordingly.
All Arkville configurations present identical RTL user-facing AXI
stream interfaces for both AMD/Xilinx and Intel FPGAs.

* ARK-FX0 - 256-bit 32B datapath (PCIe Gen3, Gen4)
* ARK-FX1 - 512-bit 64B datapath (PCIe Gen3, Gen4, Gen5)
* ARK-FX2 - 1024-bit 128B datapath (PCIe Gen5x16 Only)

DPDK and Arkville Firmware Versioning
-------------------------------------

Arkville's firmware releases and its PMD have version dependencies which
must be stepped together at certain releases. PMD code ensures the
versions are compatible. The following lists shows where version
compatible steps have occurred between DPDK releases and the corresponding
Arkville releases.  Intermediate releases not listed below remain
compatible, e.g., DPDK releases 21.05, 21.08, and 21.11 are all compatible
with Arkville releases 21.05, 21.08 and 21.11. LTS versions of DPDK remain
compatible with the corresponding Arkville version.  If other combinations
are required, please contact Atomic Rules support.

* DPDK 22.07 requires Arkville 22.07.
* DPDK 22.03 requires Arkville 22.03.
* DPDK 21.05 requires Arkville 21.05.
* DPDK 18.11 requires Arkville 18.11.
* DPDK 17.05 requires Arkville 17.05 -- initial version.

Supported Operating Systems
---------------------------

Any Linux distribution fulfilling the conditions described in ``System Requirements``
section of :ref:`the DPDK documentation <linux_gsg>` or refer to *DPDK
Release Notes*.  ARM and PowerPC architectures are not supported at this time.


Supported Features
------------------

* Dynamic ARK PMD extensions
* Dynamic per-queue MBUF (re)sizing up to 32KB
* SR-IOV, VF-based queue-separation
* Multiple receive and transmit queues
* Jumbo frames up to 9K
* Hardware Statistics

Unsupported Features
--------------------

Features that may be part of, or become part of, the Arkville RTL IP that are
not currently supported or exposed by the ARK PMD include:

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
**testpmd** with Atomic Rules ARK devices managed by librte_net_ark.

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
