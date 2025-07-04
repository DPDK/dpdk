..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2021 Marvell.

OCTEON TX EP Poll Mode driver
=============================

The OCTEON TX EP ETHDEV PMD (**librte_pmd_octeon_ep**) provides poll mode
ethdev driver support for the virtual functions (VF) of **Marvell OCTEON 9**
and **Cavium OCTEON** families of adapters in SR-IOV context.

More information can be found at `Marvell Official Website
<https://www.marvell.com/content/dam/marvell/en/public-collateral/embedded-processors/marvell-liquidio-III-solutions-brief.pdf>`_.

Runtime Config Options
----------------------

- ``Rx & Tx ISM memory accesses enable`` (default ``0``)

   The PMD supports two modes for checking Rx & Tx packet count,
   PMD may read the packet count directly from hardware registers
   or it may read from ISM memory,
   this may be selected at runtime using ``ism_enable`` devargs parameter.
   Performance is higher for bigger size packets with default value (``ism_enable=0``).
   Use this runtime option to enable ISM memory accesses
   to get better performance for lower size packets.

   For example::

      -a 0002:02:00.0,ism_enable=1

Prerequisites
-------------

This driver relies on external kernel PF driver for resources allocations
and initialization. The following dependencies are not part of DPDK and
must be installed separately:

- **Kernel module**
  This module, octeon_drv, drives the physical function, initializes hardware,
  allocates resources such as number of VFs, input/output queues for itself and
  the number of i/o queues each VF can use.

See :doc:`../platform/cnxk` for SDP interface information which provides PCIe endpoint support for a remote host.
