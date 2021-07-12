..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2021 Marvell.

CNXK Poll Mode driver
=====================

The CNXK ETHDEV PMD (**librte_net_cnxk**) provides poll mode ethdev driver
support for the inbuilt network device found in **Marvell OCTEON CN9K/CN10K**
SoC family as well as for their virtual functions (VF) in SR-IOV context.

More information can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors>`_.

Features
--------

Features of the CNXK Ethdev PMD are:

- Packet type information
- Promiscuous mode
- Jumbo frames
- SR-IOV VF
- Lock-free Tx queue
- Multiple queues for TX and RX
- Receiver Side Scaling (RSS)
- MAC filtering
- Generic flow API
- Inner and Outer Checksum offload
- Port hardware statistics
- Link state information
- Link flow control
- MTU update
- Scatter-Gather IO support
- Vector Poll mode driver
- Debug utilities - Context dump and error interrupt support
- Support Rx interrupt

Prerequisites
-------------

See :doc:`../platform/cnxk` for setup information.


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

      ./<build_dir>/app/dpdk-testpmd -c 0xc -a 0002:02:00.0 -- --portmask=0x1 --nb-cores=1 --port-topology=loop --rxq=1 --txq=1
      EAL: Detected 4 lcore(s)
      EAL: Detected 1 NUMA nodes
      EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
      EAL: Selected IOVA mode 'VA'
      EAL: No available hugepages reported in hugepages-16777216kB
      EAL: No available hugepages reported in hugepages-2048kB
      EAL: Probing VFIO support...
      EAL: VFIO support initialized
      EAL:   using IOMMU type 1 (Type 1)
      [ 2003.202721] vfio-pci 0002:02:00.0: vfio_cap_init: hiding cap 0x14@0x98
      EAL: Probe PCI driver: net_cn10k (177d:a063) device: 0002:02:00.0 (socket 0)
      PMD: RoC Model: cn10k
      EAL: No legacy callbacks, legacy socket not created
      testpmd: create a new mbuf pool <mb_pool_0>: n=155456, size=2176, socket=0
      testpmd: preferred mempool ops selected: cn10k_mempool_ops
      Configuring Port 0 (socket 0)
      PMD: Port 0: Link Up - speed 25000 Mbps - full-duplex

      Port 0: link state change event
      Port 0: 96:D4:99:72:A5:BF
      Checking link statuses...
      Done
      No commandline core given, start packet forwarding
      io packet forwarding - ports=1 - cores=1 - streams=1 - NUMA support enabled, MP allocation mode: native
      Logical Core 3 (socket 0) forwards packets on 1 streams:
        RX P=0/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0) peer=02:00:00:00:00:00

        io packet forwarding packets/burst=32
        nb forwarding cores=1 - nb forwarding ports=1
        port 0: RX queue number: 1 Tx queue number: 1
          Rx offloads=0x0 Tx offloads=0x10000
          RX queue: 0
            RX desc=4096 - RX free threshold=0
            RX threshold registers: pthresh=0 hthresh=0  wthresh=0
            RX Offloads=0x0
          TX queue: 0
            TX desc=512 - TX free threshold=0
            TX threshold registers: pthresh=0 hthresh=0  wthresh=0
            TX offloads=0x0 - TX RS bit threshold=0
      Press enter to exit

Runtime Config Options
----------------------

- ``Rx&Tx scalar mode enable`` (default ``0``)

   PMD supports both scalar and vector mode, it may be selected at runtime
   using ``scalar_enable`` ``devargs`` parameter.

- ``RSS reta size`` (default ``64``)

   RSS redirection table size may be configured during runtime using ``reta_size``
   ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,reta_size=256

   With the above configuration, reta table of size 256 is populated.

- ``Flow priority levels`` (default ``3``)

   RTE Flow priority levels can be configured during runtime using
   ``flow_max_priority`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,flow_max_priority=10

   With the above configuration, priority level was set to 10 (0-9). Max
   priority level supported is 32.

- ``Reserve Flow entries`` (default ``8``)

   RTE flow entries can be pre allocated and the size of pre allocation can be
   selected runtime using ``flow_prealloc_size`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,flow_prealloc_size=4

   With the above configuration, pre alloc size was set to 4. Max pre alloc
   size supported is 32.

- ``Max SQB buffer count`` (default ``512``)

   Send queue descriptor buffer count may be limited during runtime using
   ``max_sqb_count`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,max_sqb_count=64

   With the above configuration, each send queue's descriptor buffer count is
   limited to a maximum of 64 buffers.

- ``Switch header enable`` (default ``none``)

   A port can be configured to a specific switch header type by using
   ``switch_header`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,switch_header="higig2"

   With the above configuration, higig2 will be enabled on that port and the
   traffic on this port should be higig2 traffic only. Supported switch header
   types are "chlen24b", "chlen90b", "dsa", "exdsa", "higig2" and "vlan_exdsa".

- ``RSS tag as XOR`` (default ``0``)

   The HW gives two options to configure the RSS adder i.e

   * ``rss_adder<7:0> = flow_tag<7:0> ^ flow_tag<15:8> ^ flow_tag<23:16> ^ flow_tag<31:24>``

   * ``rss_adder<7:0> = flow_tag<7:0>``

   Latter one aligns with standard NIC behavior vs former one is a legacy
   RSS adder scheme used in OCTEON TX2 products.

   By default, the driver runs in the latter mode.
   Setting this flag to 1 to select the legacy mode.

   For example to select the legacy mode(RSS tag adder as XOR)::

      -a 0002:02:00.0,tag_as_xor=1


.. note::

   Above devarg parameters are configurable per device, user needs to pass the
   parameters to all the PCIe devices if application requires to configure on
   all the ethdev ports.

Limitations
-----------

``mempool_cnxk`` external mempool handler dependency
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The OCTEON CN9K/CN10K SoC family NIC has inbuilt HW assisted external mempool manager.
``net_cnxk`` pmd only works with ``mempool_cnxk`` mempool handler
as it is performance wise most effective way for packet allocation and Tx buffer
recycling on OCTEON TX2 SoC platform.

CRC stripping
~~~~~~~~~~~~~

The OCTEON CN9K/CN10K SoC family NICs strip the CRC for every packet being received by
the host interface irrespective of the offload configuration.

RTE flow GRE support
~~~~~~~~~~~~~~~~~~~~

- ``RTE_FLOW_ITEM_TYPE_GRE_KEY`` works only when checksum and routing
  bits in the GRE header are equal to 0.

Custom protocols supported in RTE Flow
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The ``RTE_FLOW_ITEM_TYPE_RAW`` can be used to parse the below custom protocols.

* ``vlan_exdsa`` and ``exdsa`` can be parsed at L2 level.
* ``NGIO`` can be parsed at L3 level.

For ``vlan_exdsa`` and ``exdsa``, the port has to be configured with the
respective switch header.

For example::

   -a 0002:02:00.0,switch_header="vlan_exdsa"

The below fields of ``struct rte_flow_item_raw`` shall be used to specify the
pattern.

- ``relative`` Selects the layer at which parsing is done.

  - 0 for ``exdsa`` and ``vlan_exdsa``.

  - 1 for  ``NGIO``.

- ``offset`` The offset in the header where the pattern should be matched.
- ``length`` Length of the pattern.
- ``pattern`` Pattern as a byte string.

Example usage in testpmd::

   ./dpdk-testpmd -c 3 -w 0002:02:00.0,switch_header=exdsa -- -i \
                  --rx-offloads=0x00080000 --rxq 8 --txq 8
   testpmd> flow create 0 ingress pattern eth / raw relative is 0 pattern \
          spec ab pattern mask ab offset is 4 / end actions queue index 1 / end

Debugging Options
-----------------

.. _table_cnxk_ethdev_debug_options:

.. table:: cnxk ethdev debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | NIX        | --log-level='pmd\.net.cnxk,8'                         |
   +---+------------+-------------------------------------------------------+
   | 2 | NPC        | --log-level='pmd\.net.cnxk\.flow,8'                   |
   +---+------------+-------------------------------------------------------+
