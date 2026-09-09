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
- TCP and UDP segmentation offload (TSO) on VFs, enabled per Tx queue
  when the TSO offload flag is requested.
- Large receive offload (LRO) on the receive path via hardware Receive
  Segment Coalesce (RSC), enabled when the TCP LRO Rx offload flag is
  requested. RSC requires the FCS to be stripped, so it cannot be combined
  with the KEEP_CRC Rx offload. RSC also requires the SCATTER Rx offload
  (coalesced frames span multiple buffers) and is not supported with the
  ``nc=1`` non-cacheable descriptor ring mode.
- Per-queue Rx interrupts on VFs (cacheable Rx path only), enabling
  interrupt-driven receive with ``vfio-pci``. Applications set
  ``intr_conf.rxq = 1`` in ``rte_eth_conf`` to activate this feature.
  See `Rx Interrupt Mode (VF)`_ for setup details.
- Firmware version: The NETC IP version is reported via ``rte_eth_dev_fw_version_get``.
- Registers dump: The station interface, port (PF only) and BD ring registers are dumped via ``rte_eth_dev_get_reg_info``.
- SI-based port VLAN (pvid): Hardware VLAN tag insertion on Tx and removal on Rx, configured
  via ``rte_eth_dev_set_vlan_pvid``. On a PF the registers are written directly; on a privileged
  VF the request is forwarded to the kernel PF through the VSI-PSI mailbox (class 0x24).
  Use the testpmd command ``tx_vlan set pvid <port_id> <vlan_id> on|off`` to enable or disable.

.. note::

   The LRO (RSC) and TSO (LSO) offloads use a doubled 32B buffer descriptor
   ring layout that the primary process selects only during queue setup. A
   secondary process cannot observe that per-queue choice and always uses the
   base descriptor stride, so running the Rx/Tx datapath from a secondary
   process is not supported when LRO or TSO is enabled. The ``nc=1``
   non-cacheable ring mode is stored in shared memory and is honoured by the
   secondary.


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

Follow instructions available in the document :doc:`build_and_test` to launch **testpmd**.


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

``vf_link_legacy``
  Select the legacy 4-bit PF-to-VF link speed code layout.
  Set to ``1`` when the host PF is running a kernel older than v6.18.37.
  Kernels before that release encode link speed in only 4 bits; without this
  flag the driver interprets those codes as 8-bit values and reports incorrect
  link speeds.

  Usage example::

    dpdk-testpmd -a 0000:00:01.0,vf_link_legacy=1 -- -i

PF/Common devargs
~~~~~~~~~~~~~~~~~

``enetc4_txq_prior``
  Set per-queue Tx ring priority (TBMR bits).
  The value is a ``|``-separated list of priority values, one per Tx queue.
  Values beyond the maximum supported Tx queue count are discarded.

  Usage example::

    dpdk-testpmd -a 0000:00:00.0,enetc4_txq_prior="1|2|3" -- -i

``enetc4_txq_wrr``
  Set per-queue WRR weight for the LEAF-level Tx scheduler (TBaMR bits [6:4]).
  The value is a ``|``-separated list of WRR weights, one per Tx queue.
  Meaningful only when the corresponding queues share the same strict-priority
  level via ``enetc4_txq_prior``; queues with different priorities are
  scheduled strictly regardless of their WRR weight.
  Values beyond the maximum supported Tx queue count are discarded.

  Usage example (WRR 2:4:1 on three equal-priority rings)::

    dpdk-testpmd -a 0000:00:00.0,enetc4_txq_prior="1|1|1",enetc4_txq_wrr="2|4|1" -- -i

``nc``
  Select non-cacheable Rx/Tx ops (BD rings mapped as non-cacheable memory).
  Set to ``1`` to use non-cacheable descriptor ring operations.
  By default, cacheable BD rings with software cache maintenance are used.
  Applies to both PF and VF.

  Usage example::

    dpdk-testpmd -a 0000:00:00.0,nc=1 -- -i


Rx Interrupt Mode (VF)
----------------------

The ENETC4 VF PMD supports per-queue MSI-X Rx interrupts on the cacheable
(default) Rx path. This allows applications to block in ``epoll_wait``
instead of busy-polling, reducing CPU utilization when traffic is absent.

**MSI-X vector assignment**

ENETC4 VF MSI-X vector 0 is reserved for the PSI-to-VSI mailbox interrupt
(link status notifications). Rx queue ``i`` is mapped to vector ``i + 1``.
The driver allocates all required eventfds before calling
``rte_intr_enable()`` so that ``vfio-pci`` can wire each MSI-X vector to
its eventfd when it programs the MSI-X table.

**Kernel and driver requirements**

- ``vfio-pci`` kernel module with no-IOMMU mode enabled (no SMMU required).
- The non-cacheable memory mode (``nc=1`` devarg) does **not** support
  Rx interrupts and returns ``-ENOTSUP`` from ``rx_queue_intr_enable``.

**Host setup**

.. code-block:: console

   # Enable vfio-pci no-IOMMU mode (if SMMU is not available)
   modprobe vfio enable_unsafe_noiommu_mode=1
   modprobe vfio-pci

   # Bind the VF to vfio-pci
   echo vfio-pci > /sys/bus/pci/devices/<vf_pci_addr>/driver_override
   echo <vf_pci_addr> > /sys/bus/pci/drivers_probe

**Running l3fwd-power with a single queue and core**

The ``l3fwd-power`` sample application demonstrates interrupt-driven Rx.
It sets ``intr_conf.rxq = 1`` in ``rte_eth_conf``, which triggers the VF
interrupt setup in the driver. The ``--vfio-intr=msix`` EAL flag instructs
DPDK to use MSI-X eventfds for interrupt signalling.

.. code-block:: console

   ./dpdk-l3fwd-power -l 0-1 -n 1 --vfio-intr=msix \
       -a <vf_pci_addr> -- \
       -p 0x1 --config="(0,0,1)" --no-numa --interrupt-only

Where:

- ``-l 0-1`` assigns the main thread to core 0 and the forwarding lcore to
  core 1.
- ``-a <vf_pci_addr>`` specifies the VF PCI address (e.g. ``0000:01:00.1``).
- ``--config="(0,0,1)"`` maps port 0, queue 0 to lcore 1.
- ``--interrupt-only`` enables pure interrupt mode (no busy-poll fallback).

With no incoming traffic the forwarding lcore sleeps in ``epoll_wait``;
CPU utilization drops to near zero. On the first arriving packet the MSI-X
interrupt fires, the lcore wakes, drains the ring, disables the interrupt,
processes the burst, then re-enables and re-arms the interrupt before
returning to sleep.
