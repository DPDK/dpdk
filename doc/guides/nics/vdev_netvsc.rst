..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2017 6WIND S.A.
    Copyright 2017 Mellanox Technologies, Ltd.

VDEV_NETVSC driver
==================

The VDEV_NETVSC driver (librte_pmd_vdev_netvsc) provides support for NetVSC
interfaces and associated SR-IOV virtual function (VF) devices found in
Linux virtual machines running on Microsoft Hyper-V_ (including Azure)
platforms.

.. _Hyper-V: https://docs.microsoft.com/en-us/windows-hardware/drivers/network/overview-of-hyper-v

Build options
-------------

- ``CONFIG_RTE_LIBRTE_VDEV_NETVSC_PMD`` (default ``y``)

   Toggle compilation of this driver.
