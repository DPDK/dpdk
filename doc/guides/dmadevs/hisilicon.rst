..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2021 HiSilicon Limited.

HISILICON Kunpeng DMA Driver
============================

Kunpeng SoC has an internal DMA unit which can be used by application
to accelerate data copies.
The DMA PF function supports multiple DMA channels.


Supported Kunpeng SoCs
----------------------

* Kunpeng 920


Device Setup
-------------

Kunpeng DMA devices will need to be bound to a suitable DPDK-supported
user-space IO driver such as ``vfio-pci`` in order to be used by DPDK.
