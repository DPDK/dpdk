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
- Inline IPsec processing support
- Ingress meter support
- Queue based priority flow control support

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

- ``SQB slack count`` (default ``12``)

   Send queue descriptor slack count added to SQB count when a Tx queue is
   created, can be set using ``sqb_slack`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,sqb_slack=32

   With the above configuration, each send queue's descriptor buffer count will
   be increased by 32, while keeping the queue limit to default configuration.

- ``Switch header enable`` (default ``none``)

   A port can be configured to a specific switch header type by using
   ``switch_header`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,switch_header="higig2"

   With the above configuration, higig2 will be enabled on that port and the
   traffic on this port should be higig2 traffic only. Supported switch header
   types are "chlen24b", "chlen90b", "dsa", "exdsa", "higig2", "vlan_exdsa" and
   "pre_l2".

- ``Flow pre_l2 info`` (default ``0x0/0x0/0x0``)

   pre_l2 headers are custom headers placed before the ethernet header. For
   parsing custom pre_l2 headers, an offset, mask within the offset and shift
   direction has to be provided within the custom header that holds the size of
   the custom header. This is valid only with switch header pre_l2. Maximum
   supported offset range is 0 to 255 and mask range is 1 to 255 and
   shift direction, 0: left shift, 1: right shift.
   Info format will be "offset/mask/shift direction". All parameters has to be
   in hexadecimal format and mask should be contiguous. Info can be configured
   using ``flow_pre_l2_info`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,switch_header="pre_l2",flow_pre_l2_info=0x2/0x7e/0x1

   With the above configuration, custom pre_l2 header will be enabled on that
   port and size of the header is placed at byte offset 0x2 in the packet with
   mask 0x7e and right shift will be used to get the size. That is, size will be
   (pkt[0x2] & 0x7e) >> shift count. Shift count will be calculated based on
   mask and shift direction. For example, if mask is 0x7c and shift direction is
   1 (i.e., right shift) then the shift count will be 2, that is, absolute
   position of the rightmost set bit. If the mask is 0x7c and shift direction
   is 0 (i.e., left shift) then the shift count will be 1, that is, (8 - n),
   where n is the absolute position of leftmost set bit.

- ``RSS tag as XOR`` (default ``0``)

   The HW gives two options to configure the RSS adder i.e

   * ``rss_adder<7:0> = flow_tag<7:0> ^ flow_tag<15:8> ^ flow_tag<23:16> ^ flow_tag<31:24>``

   * ``rss_adder<7:0> = flow_tag<7:0>``

   Latter one aligns with standard NIC behavior vs former one is a legacy
   RSS adder scheme used in OCTEON 9 products.

   By default, the driver runs in the latter mode.
   Setting this flag to 1 to select the legacy mode.

   For example to select the legacy mode(RSS tag adder as XOR)::

      -a 0002:02:00.0,tag_as_xor=1

- ``Min SPI for inbound inline IPsec`` (default ``0``)

   Min SPI supported for inbound inline IPsec processing can be specified by
   ``ipsec_in_min_spi`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,ipsec_in_min_spi=6

   With the above configuration, application can enable inline IPsec processing
   for inbound SA with min SPI of 6.

- ``Max SPI for inbound inline IPsec`` (default ``255``)

   Max SPI supported for inbound inline IPsec processing can be specified by
   ``ipsec_in_max_spi`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,ipsec_in_max_spi=128

   With the above configuration, application can enable inline IPsec processing
   with max SPI of 128.

- ``Max SA's for outbound inline IPsec`` (default ``4096``)

   Max number of SA's supported for outbound inline IPsec processing can be
   specified by ``ipsec_out_max_sa`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,ipsec_out_max_sa=128

   With the above configuration, application can enable inline IPsec processing
   for 128 outbound SAs.

- ``Enable custom SA action`` (default ``0``)

   Custom SA action can be enabled by specifying ``custom_sa_act`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,custom_sa_act=1

   With the above configuration, application can enable custom SA action. This
   configuration allows the potential for a MCAM entry to match many SAs,
   rather than only match a single SA.
   For cnxk device sa_index will be calculated based on SPI value. So, it will
   be 1 to 1 mapping. By enabling this devargs and setting a MCAM rule, will
   allow application to configure the sa_index as part of session create. And
   later original SPI value can be updated using session update.
   For example, application can set sa_index as 0 using session create as SPI value
   and later can update the original SPI value (for example 0x10000001) using
   session update. And create a flow rule with security action and algorithm as
   RTE_PMD_CNXK_SEC_ACTION_ALG0 and sa_hi as 0x1000 and sa_lo as 0x0001.

- ``Outbound CPT LF queue size`` (default ``8200``)

   Size of Outbound CPT LF queue in number of descriptors can be specified by
   ``outb_nb_desc`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,outb_nb_desc=16384

    With the above configuration, Outbound CPT LF will be created to accommodate
    at max 16384 descriptors at any given time.

- ``Outbound CPT LF count`` (default ``1``)

   Number of CPT LF's to attach for Outbound processing can be specified by
   ``outb_nb_crypto_qs`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,outb_nb_crypto_qs=2

   With the above configuration, two CPT LF's are setup and distributed among
   all the Tx queues for outbound processing.

- ``Disable using inline ipsec device for inbound`` (default ``0``)

   In CN10K, in event mode, driver can work in two modes,

   1. Inbound encrypted traffic received by probed ipsec inline device while
      plain traffic post decryption is received by ethdev.

   2. Both Inbound encrypted traffic and plain traffic post decryption are
      received by ethdev.

   By default event mode works using inline device i.e mode ``1``.
   This behaviour can be changed to pick mode ``2`` by using
   ``no_inl_dev`` ``devargs`` parameter.

   For example::

      -a 0002:02:00.0,no_inl_dev=1 -a 0002:03:00.0,no_inl_dev=1

   With the above configuration, inbound encrypted traffic from both the ports
   is received by ipsec inline device.

- ``Inline IPsec device channel and mask`` (default ``none``)

   Set channel and channel mask configuration for the inline IPSec device. This
   will be used when creating flow rules with RTE_FLOW_ACTION_TYPE_SECURITY
   action.

   By default, RTE Flow API sets the channel number of the port on which the
   rule is created in the MCAM entry and matches it exactly. This behaviour can
   be modified using the ``inl_cpt_channel`` ``devargs`` parameter.

   For example::

      -a 0002:1d:00.0,inl_cpt_channel=0x100/0xf00

   With the above configuration, RTE Flow rules API will set the channel
   and channel mask as 0x100 and 0xF00 in the MCAM entries of the  flow rules
   created with RTE_FLOW_ACTION_TYPE_SECURITY action. Since channel number is
   set with this custom mask, inbound encrypted traffic from all ports with
   matching channel number pattern will be directed to the inline IPSec device.

- ``Inline IPsec device flow rules`` (default ``none``)

   For inline IPsec device, reserve number of rules specified by ``max_ipsec_rules``
   and use them while installing rules with action as security.
   Rule priority should be 0.
   If specified number of rules not available,
   then only available number of rules will be allocated and used.
   If application try to insert more than allocated rules, flow creation will fail.

   For example::

      -a 0002:1d:00.0,max_ipsec_rules=100

   With the above configuration, 100 rules will be allocated from 0-99 if available
   and will be used for rules with action security.
   If 100 rules are not available, and only 50 are available,
   then only 50 rules will be allocated and used for flow rule creation.
   If application try to add more than 50 rules, the flow creation will fail.

- ``SDP device channel and mask`` (default ``none``)
   Set channel and channel mask configuration for the SDP device. This
   will be used when creating flow rules on the SDP device.

   By default, for rules created on the SDP device, the RTE Flow API sets the
   channel number and mask to cover the entire SDP channel range in the channel
   field of the MCAM entry. This behaviour can be modified using the
   ``sdp_channel_mask`` ``devargs`` parameter.

   For example::

      -a 0002:1d:00.0,sdp_channel_mask=0x700/0xf00

   With the above configuration, RTE Flow rules API will set the channel
   and channel mask as 0x700 and 0xF00 in the MCAM entries of the  flow rules
   created on the SDP device. This option needs to be used when more than one
   SDP interface is in use and RTE Flow rules created need to distinguish
   between traffic from each SDP interface. The channel and mask combination
   specified should match all the channels(or rings) configured on the SDP
   interface.

- ``Transmit completion handler`` (default ``0``)

   When transmit completion handler is enabled,
   the PMD invokes the callback handler provided by the application
   for every packet which has external buf attached to mbuf
   and frees main mbuf, external buffer is provided to applicatoin.
   Once external buffer is handed over to application,
   it is application responsibility either to free or reuse external buffer
   using ``tx_compl_ena`` devargs parameter.

   For example::

      -a 0002:01:00.1,tx_compl_ena=1

- ``Meta buffer size per ethdev port for inline inbound IPsec second pass``

   Size of meta buffer allocated for inline inbound IPsec second pass per
   ethdev port can be specified by ``meta_buf_sz`` ``devargs`` parameter.
   Default value is computed runtime based on pkt mbuf pools created and in use.
   This option is for OCTEON CN106-B0/CN103XX SoC family.

   For example::

      -a 0002:02:00.0,meta_buf_sz=512

   With the above configuration, PMD would allocate meta buffers of size 512 for
   inline inbound IPsec processing second pass.

.. note::

   Above devarg parameters are configurable per device, user needs to pass the
   parameters to all the PCIe devices if application requires to configure on
   all the ethdev ports.

Limitations
-----------

``mempool_cnxk`` external mempool handler dependency
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The OCTEON CN9K/CN10K SoC family NIC has inbuilt HW assisted external mempool manager.
``net_cnxk`` PMD only works with ``mempool_cnxk`` mempool handler
as it is performance wise most effective way for packet allocation and Tx buffer
recycling on OCTEON 9 SoC platform.

CRC stripping
~~~~~~~~~~~~~

The OCTEON CN9K/CN10K SoC family NICs strip the CRC for every packet being received by
the host interface irrespective of the offload configuration.

RTE flow GRE support
~~~~~~~~~~~~~~~~~~~~

- ``RTE_FLOW_ITEM_TYPE_GRE_KEY`` works only when checksum and routing
  bits in the GRE header are equal to 0.

RTE flow action represented_port support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``RTE_FLOW_ACTION_TYPE_REPRESENTED_PORT`` only works between a PF and its VFs.

RTE flow action port_id support
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``RTE_FLOW_ACTION_TYPE_PORT_ID`` is only supported between PF and its VFs.

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

RTE Flow mark item support
~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``RTE_FLOW_ITEM_TYPE_MARK`` can be used to create ingress flow rules to match
  packets from CPT(second pass packets). When mark item type is used, it should
  be the first item in the patterns specification.

Inline device support for CN10K
-------------------------------

CN10K HW provides a misc device Inline device that supports ethernet devices in
providing following features.

  - Aggregate all the inline IPsec inbound traffic from all the CN10K ethernet
    devices to be processed by the single inline IPSec device. This allows
    single rte security session to accept traffic from multiple ports.

  - Support for event generation on outbound inline IPsec processing errors.

  - Support CN106xx poll mode of operation for inline IPSec inbound processing.

Inline IPsec device is identified by PCI PF vendid:devid ``177D:A0F0`` or
VF ``177D:A0F1``.

Runtime Config Options for inline device
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

- ``Min SPI for inbound inline IPsec`` (default ``0``)

   Min SPI supported for inbound inline IPsec processing can be specified by
   ``ipsec_in_min_spi`` ``devargs`` parameter.

   For example::

      -a 0002:1d:00.0,ipsec_in_min_spi=6

   With the above configuration, application can enable inline IPsec processing
   for inbound SA with min SPI of 6 for traffic aggregated on inline device.

- ``Max SPI for inbound inline IPsec`` (default ``255``)

   Max SPI supported for inbound inline IPsec processing can be specified by
   ``ipsec_in_max_spi`` ``devargs`` parameter.

   For example::

      -a 0002:1d:00.0,ipsec_in_max_spi=128

   With the above configuration, application can enable inline IPsec processing
   for inbound SA with max SPI of 128 for traffic aggregated on inline device.

- ``Count of meta buffers for inline inbound IPsec second pass``

   Number of meta buffers allocated for inline inbound IPsec second pass can
   be specified by ``nb_meta_bufs`` ``devargs`` parameter. Default value is
   computed runtime based on pkt mbuf pools created and in use. Number of meta
   buffers should be at least equal to aggregated number of packet buffers of all
   packet mbuf pools in use by Inline IPsec enabled ethernet devices.

   For example::

      -a 0002:1d:00.0,nb_meta_bufs=1024

   With the above configuration, PMD would enable inline IPsec processing
   for inbound with 1024 meta buffers available for second pass.

- ``Meta buffer size for inline inbound IPsec second pass``

   Size of meta buffer allocated for inline inbound IPsec second pass can
   be specified by ``meta_buf_sz`` ``devargs`` parameter. Default value is
   computed runtime based on pkt mbuf pools created and in use.

   For example::

      -a 0002:1d:00.0,meta_buf_sz=512

   With the above configuration, PMD would allocate meta buffers of size 512 for
   inline inbound IPsec processing second pass.

- ``Inline Outbound soft expiry poll frequency in usec`` (default ``100``)

   Soft expiry poll frequency for Inline Outbound sessions can be specified by
   ``soft_exp_poll_freq`` ``devargs`` parameter.

   For example::

      -a 0002:1d:00.0,soft_exp_poll_freq=1000

   With the above configuration, driver would poll for soft expiry events every
   1000 usec.

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
