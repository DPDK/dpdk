.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2024-2026 NXP

ENETC4 Poll Mode Driver
=======================

The ENETC4 NIC PMD (**librte_net_enetc**) provides poll mode driver
support for the inbuilt NIC found in multiple NXP new generation SoCs.

More information can be found at `NXP Official Website
<https://www.nxp.com/products/processors-and-microcontrollers/arm-processors/i-mx-applications-processors/i-mx-9-processors/i-mx-95-applications-processor-family-high-performance-safety-enabled-platform-with-eiq-neutron-npu:iMX95>`_.

This section provides an overview of the NXP ENETC4
and how it is integrated into the DPDK.


ENETC4 Overview
---------------

ENETC4 is a PCI Integrated End Point (IEP).
IEP implements peripheral devices in a SoC
such that software sees them as PCIe device.
ENETC4 is an evolution of BDR (Buffer Descriptor Ring) based networking IPs.

This infrastructure simplifies adding support for IEP and facilitates in following:

- Device discovery and location
- Resource requirement discovery and allocation
  (e.g. interrupt assignment, device register address)
- Event reporting


Supported ENETC4 SoCs
---------------------

- i.MX95
- i.MX943


NIC Driver (PMD)
----------------

The ENETC4 PMD is a traditional DPDK PMD
that bridges the DPDK framework and ENETC4 internal drivers,
supporting both Virtual Functions (VFs) and Physical Functions (PF).
Key functionality includes:

- Driver registration: The device vendor table is registered in the PCI subsystem.
- Device discovery: The DPDK framework scans the PCI bus for connected devices,
  triggering the ENETC4 driver's probe function.
- Initialization: The probe function configures basic device registers
  and sets up Buffer Descriptor (BD) rings.
- Receive processing: Upon packet reception, the BD Ring status bit is set,
  facilitating packet processing.
- Transmission: Packet transmission precedes reception, ensuring efficient data transfer.


Prerequisites
-------------

There are three main pre-requisites for executing ENETC4 PMD
on ENETC4 compatible boards:

#. **ARM64 Toolchain**

   For example, the `*aarch64* ARM toolchain
   <https://developer.arm.com/-/media/Files/downloads/gnu/13.3.rel1/binrel/arm-gnu-toolchain-13.3.rel1-x86_64-aarch64-none-linux-gnu.tar.xz>`_.

#. **Linux Kernel**

   It can be obtained from `NXP's Github hosting <https://github.com/nxp-imx/linux-imx>`_.

The following dependencies are not part of DPDK and must be installed separately:

- **NXP Linux LF**

  NXP Linux LF refers to NXP's Linux Factory releases,
  which are specific Linux distributions and Board Support Packages (BSPs)
  provided by NXP for their i.MX family of applications processors
  and other embedded platforms.

  i.MX LF release and related information can be obtained from: `LF
  <https://www.nxp.com/design/design-center/software/embedded-software/i-mx-software/embedded-linux-for-i-mx-applications-processors:IMXLINUX>`_
  Refer section: Linux Current Release.


Driver compilation and testing
------------------------------

Follow instructions available in the document
:ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
to launch **testpmd**.


Driver Arguments (devargs)
--------------------------

The ENETC4 PMD supports the following device arguments (devargs)
that can be passed via ``-a`` (allow-list) option in DPDK applications.

VF-specific devargs
~~~~~~~~~~~~~~~~~~~

``enetc4_vsi_disable``
  Disable VSI-PSI messaging for the VF.
  When present, features that require VSI-PSI communication
  (link update, MAC filter, VLAN filter) are replaced with no-op stubs.
  Useful when the PF driver does not support VSI-PSI messages.

  Usage example::

    dpdk-testpmd -a 0000:00:01.0,enetc4_vsi_disable -- -i

``enetc4_vsi_timeout``
  Set the VSI-PSI message wait timeout as an iteration count.
  Controls how many polling iterations the driver waits
  for a VSI-PSI response before timing out.
  Defaults to ``ENETC4_DEF_VSI_WAIT_TIMEOUT_UPDATE`` when not set.

  Usage example::

    dpdk-testpmd -a 0000:00:01.0,enetc4_vsi_timeout=200 -- -i

``enetc4_vsi_delay``
  Set the VSI-PSI message wait delay in microseconds between polling iterations.
  Defaults to ``ENETC4_DEF_VSI_WAIT_DELAY_UPDATE`` when not set.

  Usage example::

    dpdk-testpmd -a 0000:00:01.0,enetc4_vsi_delay=10 -- -i

PF/Common devargs
~~~~~~~~~~~~~~~~~

``enetc4_txq_prior``
  Set per-queue Tx ring priority (TBMR bits).
  The value is a ``|``-separated list of priority values, one per Tx queue.
  Values beyond the maximum supported Tx queue count are discarded.

  Usage example::

    dpdk-testpmd -a 0000:00:00.0,enetc4_txq_prior=1|2|3 -- -i
