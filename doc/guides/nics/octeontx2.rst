..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2019 Marvell International Ltd.

OCTEON TX2 Poll Mode driver
===========================

The OCTEON TX2 ETHDEV PMD (**librte_pmd_octeontx2**) provides poll mode ethdev
driver support for the inbuilt network device found in **Marvell OCTEON TX2**
SoC family as well as for their virtual functions (VF) in SR-IOV context.

More information can be found at `Marvell Official Website
<https://www.marvell.com/embedded-processors/infrastructure-processors>`_.

Features
--------

Features of the OCTEON TX2 Ethdev PMD are:

- Packet type information
- Promiscuous mode
- SR-IOV VF
- Lock-free Tx queue
- Multiple queues for TX and RX
- Receiver Side Scaling (RSS)
- MAC filtering
- Port hardware statistics
- Link state information
- Debug utilities - Context dump and error interrupt support

Prerequisites
-------------

See :doc:`../platform/octeontx2` for setup information.

Compile time Config Options
---------------------------

The following options may be modified in the ``config`` file.

- ``CONFIG_RTE_LIBRTE_OCTEONTX2_PMD`` (default ``y``)

  Toggle compilation of the ``librte_pmd_octeontx2`` driver.

Runtime Config Options
----------------------

- ``HW offload ptype parsing disable`` (default ``0``)

   Packet type parsing is HW offloaded by default and this feature may be toggled
   using ``ptype_disable`` ``devargs`` parameter.

- ``Rx&Tx scalar mode enable`` (default ``0``)

   Ethdev supports both scalar and vector mode, it may be selected at runtime
   using ``scalar_enable`` ``devargs`` parameter.

- ``RSS reta size`` (default ``64``)

   RSS redirection table size may be configured during runtime using ``reta_size``
   ``devargs`` parameter.

   For example::

      -w 0002:02:00.0,reta_size=256

   With the above configuration, reta table of size 256 is populated.

- ``Flow priority levels`` (default ``3``)

   RTE Flow priority levels can be configured during runtime using
   ``flow_max_priority`` ``devargs`` parameter.

   For example::

      -w 0002:02:00.0,flow_max_priority=10

   With the above configuration, priority level was set to 10 (0-9). Max
   priority level supported is 32.

- ``Reserve Flow entries`` (default ``8``)

   RTE flow entries can be pre allocated and the size of pre allocation can be
   selected runtime using ``flow_prealloc_size`` ``devargs`` parameter.

   For example::

      -w 0002:02:00.0,flow_prealloc_size=4

   With the above configuration, pre alloc size was set to 4. Max pre alloc
   size supported is 32.

- ``Max SQB buffer count`` (default ``512``)

   Send queue descriptor buffer count may be limited during runtime using
   ``max_sqb_count`` ``devargs`` parameter.

   For example::

      -w 0002:02:00.0,max_sqb_count=64

   With the above configuration, each send queue's decscriptor buffer count is
   limited to a maximum of 64 buffers.


.. note::

   Above devarg parameters are configurable per device, user needs to pass the
   parameters to all the PCIe devices if application requires to configure on
   all the ethdev ports.
