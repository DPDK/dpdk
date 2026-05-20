.. SPDX-License-Identifier: BSD-3-Clause
   Copyright (C), 2025, Wuxi Stars Micro System Technologies Co., Ltd.

SXE2 Poll Mode Driver
=====================

The sxe2 PMD (**librte_net_sxe2**) provides poll mode driver support
for 10/25/50/100 Gbps Network Adapters.
The embedded switch, Physical Functions (PF),
and SR-IOV Virtual Functions (VF) are supported.


Implementation details
----------------------

The sxe2 PMD is designed to operate alongside the sxe2 kernel network driver.
For management and control operations,
the PMD communicates with the kernel driver via ioctl interfaces.
These commands are processed by the kernel driver
and subsequently dispatched to the hardware firmware for execution.

For security and robustness, the driver's data path is optimized to operate
using virtual addresses (IOVA as VA mode).
However, to ensure full compatibility in system environments
where an IOMMU is absent or disabled,
the driver also provides an explicit path to support physical addressing
(IOVA as PA mode).

The hardware is capable of handling the corresponding IOVA addresses
(either VA or PA) directly, as provided by the DPDK memory subsystem.
This ensures that DPDK applications can only access memory segments
explicitly allocated to the current process,
preventing unauthorized access to random physical memory.

This capability allows the PMD to coexist with kernel network interfaces
which remain functional, although they stop receiving unicast packets
as long as they share the same MAC address.
