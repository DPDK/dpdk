..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

NTB Rawdev Driver
=================

The ``ntb`` rawdev driver provides a non-transparent bridge between two
separate hosts so that they can communicate with each other. Thus, many
user cases can benefit from this, such as fault tolerance and visual
acceleration.

BIOS setting on Intel Skylake
-----------------------------

Intel Non-transparent Bridge needs special BIOS setting. Since the PMD only
supports Intel Skylake platform, introduce BIOS setting here. The referencce
is https://www.intel.com/content/dam/support/us/en/documents/server-products/Intel_Xeon_Processor_Scalable_Family_BIOS_User_Guide.pdf

- Set the needed PCIe port as NTB to NTB mode on both hosts.
- Enable NTB bars and set bar size of bar 23 and bar 45 as 12-29 (2K-512M)
  on both hosts. Note that bar size on both hosts should be the same.
- Disable split bars for both hosts.
- Set crosslink control override as DSD/USP on one host, USD/DSP on
  another host.
- Disable PCIe PII SSC (Spread Spectrum Clocking) for both hosts. This
  is a hardware requirement.

Build Options
-------------

- ``CONFIG_RTE_LIBRTE_PMD_NTB_RAWDEV`` (default ``y``)

   Toggle compilation of the ``ntb`` driver.

Device Setup
------------

The Intel NTB devices need to be bound to a DPDK-supported kernel driver
to use, i.e. igb_uio, vfio. The ``dpdk-devbind.py`` script can be used to
show devices status and to bind them to a suitable kernel driver. They will
appear under the category of "Misc (rawdev) devices".

Limitation
----------

- The FIFO hasn't been introduced and will come in 19.11 release.
- This PMD only supports Intel Skylake platform.
