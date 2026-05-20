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
