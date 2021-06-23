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
- Jumbo frames
- SR-IOV VF
- Lock-free Tx queue
- Multiple queues for TX and RX
- Receiver Side Scaling (RSS)
- Link state information
- Scatter-Gather IO support
- Vector Poll mode driver

Prerequisites
-------------

See :doc:`../platform/cnxk` for setup information.


Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

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
   types are "higig2", "dsa", "chlen90b" and "chlen24b".

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
