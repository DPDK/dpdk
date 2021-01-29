..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(C) 2021 Marvell.

OCTEON TX EP Poll Mode driver
=============================

The OCTEON TX EP ETHDEV PMD (**librte_pmd_octeontx_ep**) provides poll mode
ethdev driver support for the virtual functions (VF) of **Marvell OCTEON TX2**
and **Cavium OCTEON TX** families of adapters in SR-IOV context.

More information can be found at `Marvell Official Website
<https://www.marvell.com/content/dam/marvell/en/public-collateral/embedded-processors/marvell-liquidio-III-solutions-brief.pdf>`_.


Prerequisites
-------------

This driver relies on external kernel PF driver for resources allocations
and initialization. The following dependencies are not part of DPDK and
must be installed separately:

- **Kernel module**
  This module, octeon_drv, drives the physical function, initializes hardware,
  allocates resources such as number of VFs, input/output queues for itself and
  the number of i/o queues each VF can use.

See :doc:`../platform/octeontx2` for SDP interface information which provides PCIe endpoint support for a remote host.
