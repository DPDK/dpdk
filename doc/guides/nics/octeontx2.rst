..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2019 Marvell International Ltd.

OCTEON TX2 Poll Mode driver
===========================

The OCTEON TX2 ETHDEV PMD (**librte_net_octeontx2**) provides poll mode ethdev
driver support for the inbuilt network device found in **Marvell OCTEON TX2**
SoC family as well as for their virtual functions (VF) in SR-IOV context.

More information can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors>`_.

Features
--------

Features of the OCTEON TX2 Ethdev PMD are:

- Packet type information
- Promiscuous mode
- Jumbo frames
- SR-IOV VF
- Lock-free Tx queue
- Multiple queues for TX and RX
- Receiver Side Scaling (RSS)
- MAC/VLAN filtering
- Multicast MAC filtering
- Generic flow API
- Inner and Outer Checksum offload
- VLAN/QinQ stripping and insertion
- Port hardware statistics
- Link state information
- Link flow control
- MTU update
- Scatter-Gather IO support
- Vector Poll mode driver
- Debug utilities - Context dump and error interrupt support
- IEEE1588 timestamping
- HW offloaded `ethdev Rx queue` to `eventdev event queue` packet injection
- Support Rx interrupt
- Inline IPsec processing support
- :ref:`Traffic Management API <otx2_tmapi>`

Prerequisites
-------------

See :doc:`../platform/octeontx2` for setup information.


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

      ./<build_dir>/app/dpdk-testpmd -c 0x300 -a 0002:02:00.0 -- --portmask=0x1 --nb-cores=1 --port-topology=loop --rxq=1 --txq=1
      EAL: Detected 24 lcore(s)
      EAL: Detected 1 NUMA nodes
      EAL: Multi-process socket /var/run/dpdk/rte/mp_socket
      EAL: No available hugepages reported in hugepages-2048kB
      EAL: Probing VFIO support...
      EAL: VFIO support initialized
      EAL: PCI device 0002:02:00.0 on NUMA socket 0
      EAL:   probe driver: 177d:a063 net_octeontx2
      EAL:   using IOMMU type 1 (Type 1)
      testpmd: create a new mbuf pool <mbuf_pool_socket_0>: n=267456, size=2176, socket=0
      testpmd: preferred mempool ops selected: octeontx2_npa
      Configuring Port 0 (socket 0)
      PMD: Port 0: Link Up - speed 40000 Mbps - full-duplex

      Port 0: link state change event
      Port 0: 36:10:66:88:7A:57
      Checking link statuses...
      Done
      No commandline core given, start packet forwarding
      io packet forwarding - ports=1 - cores=1 - streams=1 - NUMA support enabled, MP allocation mode: native
      Logical Core 9 (socket 0) forwards packets on 1 streams:
        RX P=0/Q=0 (socket 0) -> TX P=0/Q=0 (socket 0) peer=02:00:00:00:00:00

        io packet forwarding packets/burst=32
        nb forwarding cores=1 - nb forwarding ports=1
        port 0: RX queue number: 1 Tx queue number: 1
          Rx offloads=0x0 Tx offloads=0x10000
          RX queue: 0
            RX desc=512 - RX free threshold=0
            RX threshold registers: pthresh=0 hthresh=0  wthresh=0
            RX Offloads=0x0
          TX queue: 0
            TX desc=512 - TX free threshold=0
            TX threshold registers: pthresh=0 hthresh=0  wthresh=0
            TX offloads=0x10000 - TX RS bit threshold=0
      Press enter to exit

Runtime Config Options
----------------------

- ``Rx&Tx scalar mode enable`` (default ``0``)

   Ethdev supports both scalar and vector mode, it may be selected at runtime
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
   types are "higig2", "dsa" and "chlen90b".

- ``RSS tag as XOR`` (default ``0``)

   C0 HW revision onward, The HW gives an option to configure the RSS adder as

   * ``rss_adder<7:0> = flow_tag<7:0> ^ flow_tag<15:8> ^ flow_tag<23:16> ^ flow_tag<31:24>``

   * ``rss_adder<7:0> = flow_tag<7:0>``

   Latter one aligns with standard NIC behavior vs former one is a legacy
   RSS adder scheme used in OCTEON TX2 products.

   By default, the driver runs in the latter mode from C0 HW revision onward.
   Setting this flag to 1 to select the legacy mode.

   For example to select the legacy mode(RSS tag adder as XOR)::

      -a 0002:02:00.0,tag_as_xor=1

- ``Max SPI for inbound inline IPsec`` (default ``1``)

   Max SPI supported for inbound inline IPsec processing can be specified by
   ``ipsec_in_max_spi`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,ipsec_in_max_spi=128

   With the above configuration, application can enable inline IPsec processing
   on 128 SAs (SPI 0-127).

- ``Lock Rx contexts in NDC cache``

   Lock Rx contexts in NDC cache by using ``lock_rx_ctx`` parameter.

   For example::

      -a 0002:02:00.0,lock_rx_ctx=1

- ``Lock Tx contexts in NDC cache``

   Lock Tx contexts in NDC cache by using ``lock_tx_ctx`` parameter.

   For example::

      -a 0002:02:00.0,lock_tx_ctx=1

.. note::

   Above devarg parameters are configurable per device, user needs to pass the
   parameters to all the PCIe devices if application requires to configure on
   all the ethdev ports.

- ``Lock NPA contexts in NDC``

   Lock NPA aura and pool contexts in NDC cache.
   The device args take hexadecimal bitmask where each bit represent the
   corresponding aura/pool id.

   For example::

      -a 0002:02:00.0,npa_lock_mask=0xf

.. _otx2_tmapi:

Traffic Management API
----------------------

OCTEON TX2 PMD supports generic DPDK Traffic Management API which allows to
configure the following features:

#. Hierarchical scheduling
#. Single rate - Two color, Two rate - Three color shaping

Both DWRR and Static Priority(SP) hierarchical scheduling is supported.

Every parent can have atmost 10 SP Children and unlimited DWRR children.

Both PF & VF supports traffic management API with PF supporting 6 levels
and VF supporting 5 levels of topology.

Limitations
-----------

``mempool_octeontx2`` external mempool handler dependency
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The OCTEON TX2 SoC family NIC has inbuilt HW assisted external mempool manager.
``net_octeontx2`` pmd only works with ``mempool_octeontx2`` mempool handler
as it is performance wise most effective way for packet allocation and Tx buffer
recycling on OCTEON TX2 SoC platform.

CRC stripping
~~~~~~~~~~~~~

The OCTEON TX2 SoC family NICs strip the CRC for every packet being received by
the host interface irrespective of the offload configuration.

Multicast MAC filtering
~~~~~~~~~~~~~~~~~~~~~~~

``net_octeontx2`` pmd supports multicast mac filtering feature only on physical
function devices.

SDP interface support
~~~~~~~~~~~~~~~~~~~~~
OCTEON TX2 SDP interface support is limited to PF device, No VF support.

Inline Protocol Processing
~~~~~~~~~~~~~~~~~~~~~~~~~~
``net_octeontx2`` pmd doesn't support the following features for packets to be
inline protocol processed.
- TSO offload
- VLAN/QinQ offload
- Fragmentation

Debugging Options
-----------------

.. _table_octeontx2_ethdev_debug_options:

.. table:: OCTEON TX2 ethdev debug options

   +---+------------+-------------------------------------------------------+
   | # | Component  | EAL log command                                       |
   +===+============+=======================================================+
   | 1 | NIX        | --log-level='pmd\.net.octeontx2,8'                    |
   +---+------------+-------------------------------------------------------+
   | 2 | NPC        | --log-level='pmd\.net.octeontx2\.flow,8'              |
   +---+------------+-------------------------------------------------------+

RTE Flow Support
----------------

The OCTEON TX2 SoC family NIC has support for the following patterns and
actions.

Patterns:

.. _table_octeontx2_supported_flow_item_types:

.. table:: Item types

   +----+--------------------------------+
   | #  | Pattern Type                   |
   +====+================================+
   | 1  | RTE_FLOW_ITEM_TYPE_ETH         |
   +----+--------------------------------+
   | 2  | RTE_FLOW_ITEM_TYPE_VLAN        |
   +----+--------------------------------+
   | 3  | RTE_FLOW_ITEM_TYPE_E_TAG       |
   +----+--------------------------------+
   | 4  | RTE_FLOW_ITEM_TYPE_IPV4        |
   +----+--------------------------------+
   | 5  | RTE_FLOW_ITEM_TYPE_IPV6        |
   +----+--------------------------------+
   | 6  | RTE_FLOW_ITEM_TYPE_ARP_ETH_IPV4|
   +----+--------------------------------+
   | 7  | RTE_FLOW_ITEM_TYPE_MPLS        |
   +----+--------------------------------+
   | 8  | RTE_FLOW_ITEM_TYPE_ICMP        |
   +----+--------------------------------+
   | 9  | RTE_FLOW_ITEM_TYPE_UDP         |
   +----+--------------------------------+
   | 10 | RTE_FLOW_ITEM_TYPE_TCP         |
   +----+--------------------------------+
   | 11 | RTE_FLOW_ITEM_TYPE_SCTP        |
   +----+--------------------------------+
   | 12 | RTE_FLOW_ITEM_TYPE_ESP         |
   +----+--------------------------------+
   | 13 | RTE_FLOW_ITEM_TYPE_GRE         |
   +----+--------------------------------+
   | 14 | RTE_FLOW_ITEM_TYPE_NVGRE       |
   +----+--------------------------------+
   | 15 | RTE_FLOW_ITEM_TYPE_VXLAN       |
   +----+--------------------------------+
   | 16 | RTE_FLOW_ITEM_TYPE_GTPC        |
   +----+--------------------------------+
   | 17 | RTE_FLOW_ITEM_TYPE_GTPU        |
   +----+--------------------------------+
   | 18 | RTE_FLOW_ITEM_TYPE_GENEVE      |
   +----+--------------------------------+
   | 19 | RTE_FLOW_ITEM_TYPE_VXLAN_GPE   |
   +----+--------------------------------+
   | 20 | RTE_FLOW_ITEM_TYPE_IPV6_EXT    |
   +----+--------------------------------+
   | 21 | RTE_FLOW_ITEM_TYPE_VOID        |
   +----+--------------------------------+
   | 22 | RTE_FLOW_ITEM_TYPE_ANY         |
   +----+--------------------------------+
   | 23 | RTE_FLOW_ITEM_TYPE_GRE_KEY     |
   +----+--------------------------------+
   | 24 | RTE_FLOW_ITEM_TYPE_HIGIG2      |
   +----+--------------------------------+

.. note::

   ``RTE_FLOW_ITEM_TYPE_GRE_KEY`` works only when checksum and routing
   bits in the GRE header are equal to 0.

Actions:

.. _table_octeontx2_supported_ingress_action_types:

.. table:: Ingress action types

   +----+-----------------------------------------+
   | #  | Action Type                             |
   +====+=========================================+
   | 1  | RTE_FLOW_ACTION_TYPE_VOID               |
   +----+-----------------------------------------+
   | 2  | RTE_FLOW_ACTION_TYPE_MARK               |
   +----+-----------------------------------------+
   | 3  | RTE_FLOW_ACTION_TYPE_FLAG               |
   +----+-----------------------------------------+
   | 4  | RTE_FLOW_ACTION_TYPE_COUNT              |
   +----+-----------------------------------------+
   | 5  | RTE_FLOW_ACTION_TYPE_DROP               |
   +----+-----------------------------------------+
   | 6  | RTE_FLOW_ACTION_TYPE_QUEUE              |
   +----+-----------------------------------------+
   | 7  | RTE_FLOW_ACTION_TYPE_RSS                |
   +----+-----------------------------------------+
   | 8  | RTE_FLOW_ACTION_TYPE_SECURITY           |
   +----+-----------------------------------------+
   | 9  | RTE_FLOW_ACTION_TYPE_PF                 |
   +----+-----------------------------------------+
   | 10 | RTE_FLOW_ACTION_TYPE_VF                 |
   +----+-----------------------------------------+
   | 11 | RTE_FLOW_ACTION_TYPE_OF_POP_VLAN        |
   +----+-----------------------------------------+

.. _table_octeontx2_supported_egress_action_types:

.. table:: Egress action types

   +----+-----------------------------------------+
   | #  | Action Type                             |
   +====+=========================================+
   | 1  | RTE_FLOW_ACTION_TYPE_COUNT              |
   +----+-----------------------------------------+
   | 2  | RTE_FLOW_ACTION_TYPE_DROP               |
   +----+-----------------------------------------+
   | 3  | RTE_FLOW_ACTION_TYPE_OF_PUSH_VLAN       |
   +----+-----------------------------------------+
   | 4  | RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_VID    |
   +----+-----------------------------------------+
   | 5  | RTE_FLOW_ACTION_TYPE_OF_SET_VLAN_PCP    |
   +----+-----------------------------------------+
