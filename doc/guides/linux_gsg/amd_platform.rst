.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2023 Advanced Micro Devices, Inc. All rights reserved.

How to get best performance on AMD platform
===========================================

This document provides a detailed, step-by-step guide
on configuring AMD EPYC System-on-Chip (SoC) for optimal performance
in DPDK applications across different SoC families.

The overall performance is influenced by factors such as BIOS settings,
NUMA per socket configuration, memory per NUMA allocation,
and proximity to IO devices.

These are covered in various sections of tuning guides shared below.


Tuning Guides for AMD EPYC SoC
------------------------------

#. `MILAN <https://www.amd.com/content/dam/amd/en/documents/epyc-technical-docs/tuning-guides/data-plane-development-kit-tuning-guide-amd-epyc7003-series-processors.pdf>`_

#. `GENOA <https://www.amd.com/content/dam/amd/en/documents/epyc-technical-docs/tuning-guides/58017-amd-epyc-9004-tg-data-plane-dpdk.pdf>`_

#. `BERGAMO|SIENNA <https://www.amd.com/content/dam/amd/en/documents/epyc-technical-docs/tuning-guides/58310_amd-epyc-8004-tg-data-plane-dpdk.pdf>`_


General Requirements
--------------------

Memory
~~~~~~

Refer to the `Memory Configuration` section for specific details related to the System-on-Chip (SoC).

.. note::

   As a general guideline, it is recommended to populate
   at least one memory DIMM in each memory channel.
   The optimal memory size for each DIMM is at least 8, 16, or 32 GB,
   utilizing ECC modules.


BIOS
----

Refer to the `BIOS Performance` section in tuning guide for recommended settings.


Linux GRUB
----------

Refer to the `Linux OS & Kernel` in tuning guide for recommended settings.


NIC and Accelerator
-------------------

AMD EPYC supports PCIe Generation of 1|2|3|4|5 depending upon SoC families.
For best performance ensure the right slots are used which provides adequate bandwidth.

Use ``lspci`` to check the speed of a PCI slot::

   lspci -s 41:00.0 -vv | grep LnkSta

   LnkSta: Speed 16GT/s, Width x16, TrErr- Train- SlotClk+ DLActive- ...
   LnkSta2: Current De-emphasis Level: -6dB, EqualizationComplete+ ...


Compiler
--------

Refer to the `Compiler Flags` in tuning guide for recommended version and `-march` flags.


Max LCores
----------

Refer to the `Compilation` in tuning guide for allowing more threads to run as DPDK `lcores`.


Power
-----

Core
~~~~

AMD EPYC SoC supports CPU power functions via ``rte_power`` API from `23.11 LTS` onwards.
These are tested and validated on MILAN, GENOA, BERGAMO and SIENA
using Linux kernel `6.4` and above with ``amd_pstate`` power driver.

.. note::

   * Power libraries are supported on Linux only.
   * DPDK uncore support on Linux is work in progress.
