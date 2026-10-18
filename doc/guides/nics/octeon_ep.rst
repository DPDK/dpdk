..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2021 Marvell.

OCTEON TX EP Poll Mode driver
=============================

The OCTEON TX EP ETHDEV PMD (**librte_pmd_octeon_ep**) provides poll mode
ethdev driver support for the virtual functions (VF) of **Marvell OCTEON 9**,
**OCTEON 10**, **CN20K** and legacy **Cavium OCTEON** adapters in SR-IOV
context when the SoC operates as a PCIe endpoint (SDP).

More information can be found at `Marvell Official Website
<https://www.marvell.com/content/dam/marvell/en/public-collateral/embedded-processors/marvell-liquidio-III-solutions-brief.pdf>`_.

Supported OCTEON SDP endpoint SoCs
----------------------------------

- OCTEON TX (CN8XX)
- CN9XX
- CN10XX
- CN20XX

Supported SDP VF PCI devices
----------------------------

.. list-table::
   :widths: 30 20 50
   :header-rows: 1

   * - SoC family
     - PCI device ID
     - Description
   * - OCTEON TX
     - 0xA303
     - SDP VF
   * - CN9XX
     - 0xB103
     - CN98XX SDP VF
   * - CN9XX
     - 0xB203
     - CN9K SDP VF
   * - CN9XX
     - 0xB403
     - CNF95N SDP VF
   * - CN9XX
     - 0xB603
     - CNF95O SDP VF
   * - CN10XX
     - 0xB903
     - CN10KA SDP VF
   * - CN10XX
     - 0xBA03
     - CNF10KA SDP VF
   * - CN10XX
     - 0xBC03
     - CNF10KB SDP VF
   * - CN10XX
     - 0xBD03
     - CN10KB SDP VF
   * - CN20XX
     - 0xC203
     - CN20KA SDP VF
   * - CN20XX
     - 0xCA03
     - CNF20KA SDP VF

On CN20K SDP VFs, IO queue rings are allocated through a mailbox to the SDP
physical function. Link status and device control also use this mailbox.

Runtime Config Options
----------------------

The following ``devargs`` parameters can be passed when binding the device.
For example::

   -a 0002:02:00.0,ism_enable=1

``ism_enable``

  Enable ISM memory for Rx and Tx packet-count polling on CN9K, CN10K and
  CN20K SDP VFs (default ``0``).

  When disabled, the PMD reads packet counts from hardware registers.
  When enabled, the PMD reads packet counts from ISM memory.
  Register polling tends to perform better for larger packets; ISM polling can
  perform better for smaller packets.

CN20K runtime config options
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The following ``devargs`` parameter applies to CN20K SDP VFs.
For example::

   -a 0002:02:00.0,max_rings=4

``max_rings``

  Number of Tx/Rx queue pairs to use (default ``8``).
  The value must be in the range ``1`` to ``8``.

Prerequisites
-------------

This driver relies on an external kernel PF driver for resource allocation
and initialization. The following dependencies are not part of DPDK and
must be installed separately:

- **Kernel module**
  The ``octeon_drv`` module drives the physical function, initializes hardware,
  allocates resources such as the number of VFs, input/output queues for the PF,
  and the number of I/O queues each VF can use.

See :doc:`/platform/cnxk` for SDP interface information which provides PCIe
endpoint support for a remote host.
