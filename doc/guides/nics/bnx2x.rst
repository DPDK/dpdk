..  SPDX-License-Identifier: BSD-3-Clause
    Copyright (c) 2015 QLogic Corporation

BNX2X Poll Mode Driver
======================

The BNX2X poll mode driver library (**librte_net_bnx2x**) implements support
for **QLogic 578xx** 10/20 Gbps family of adapters as well as their virtual
functions (VF) in SR-IOV context. It is supported on several standard Linux
distros like RHEL and SLES. It is compile-tested under FreeBSD OS.

More information can be found at `QLogic Corporation's Official Website
<http://www.qlogic.com>`_.

Supported Features
------------------

BNX2X PMD has support for:

- Base L2 features
- Unicast/multicast filtering
- Promiscuous mode
- Port hardware statistics
- SR-IOV VF

Non-supported Features
----------------------

The features not yet supported include:

- TSS (Transmit Side Scaling)
- RSS (Receive Side Scaling)
- LRO/TSO offload
- Checksum offload
- SR-IOV PF
- Rx TX scatter gather

Co-existence considerations
---------------------------

- QLogic 578xx CNAs support Ethernet, iSCSI and FCoE functionalities.
  These functionalities are supported using QLogic Linux kernel
  drivers bnx2x, cnic, bnx2i and bnx2fc. DPDK is supported on these
  adapters using bnx2x PMD.

- When SR-IOV is not enabled on the adapter,
  QLogic Linux kernel drivers (bnx2x, cnic, bnx2i and bnx2fc) and bnx2x
  PMD can’t be attached to different PFs on a given QLogic 578xx
  adapter.
  A given adapter needs to be completely used by DPDK or Linux drivers.
  Before binding DPDK driver to one or more PFs on the adapter,
  please make sure to unbind Linux drivers from all PFs of the adapter.
  If there are multiple adapters on the system, one or more adapters
  can be used by DPDK driver completely and other adapters can be used
  by Linux drivers completely.

- When SR-IOV is enabled on the adapter,
  Linux kernel drivers (bnx2x, cnic, bnx2i and bnx2fc) can be bound
  to the PFs of a given adapter and either bnx2x PMD or Linux drivers
  bnx2x can be bound to the VFs of the adapter.

Supported QLogic NICs
---------------------

- 578xx

Prerequisites
-------------

- Requires firmware version **7.13.11.0**. It is included in most of the
  standard Linux distros. If it is not available visit
  `linux-firmware git repository <https://git.kernel.org/pub/scm/linux/kernel/git/firmware/linux-firmware.git/plain/bnx2x/bnx2x-e2-7.13.11.0.fw>`_
  to get the required firmware.

Pre-Installation Configuration
------------------------------

Config Options
~~~~~~~~~~~~~~

The following options can be enabled with Meson flags. Please note that
enabling debugging options may affect system performance.

- ``RTE_LIBRTE_BNX2X_DEBUG_TX`` (default **disabled**)

  Toggle display of transmit fast path run-time messages.

- ``RTE_LIBRTE_BNX2X_DEBUG_RX`` (default **disabled**)

  Toggle display of receive fast path run-time messages.

- ``RTE_LIBRTE_BNX2X_DEBUG_PERIODIC`` (default **disabled**)

  Toggle display of register reads and writes.

.. _bnx2x_driver-compilation:

Driver compilation and testing
------------------------------

Refer to the document :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
for details.

Jumbo: Limitation
-----------------

Rx descriptor limit for number of segments per MTU is set to 1.
PMD doesn't support Jumbo Rx scatter gather. Some applications can
adjust mbuf_size based on this param and max_pkt_len.

For others, PMD detects the condition where Rx packet length cannot
be held by configured mbuf size and logs the message.

Example output:

   .. code-block:: console

      [...]
      [bnx2x_recv_pkts:397(04:00.0:dpdk-port-0)] mbuf size 2048 is not enough to hold Rx packet length more than 2046

SR-IOV: Prerequisites and sample Application Notes
--------------------------------------------------

This section provides instructions to configure SR-IOV with Linux OS.

#. Verify SR-IOV and ARI capabilities are enabled on the adapter using ``lspci``:

   .. code-block:: console

      lspci -s <slot> -vvv

   Example output:

   .. code-block:: console

      [...]
      Capabilities: [1b8 v1] Alternative Routing-ID Interpretation (ARI)
      [...]
      Capabilities: [1c0 v1] Single Root I/O Virtualization (SR-IOV)
      [...]
      Kernel driver in use: igb_uio

#. Load the kernel module:

   .. code-block:: console

      modprobe bnx2x

   Example output:

   .. code-block:: console

      systemd-udevd[4848]: renamed network interface eth0 to ens5f0
      systemd-udevd[4848]: renamed network interface eth1 to ens5f1

#. Bring up the PF ports:

   .. code-block:: console

      ifconfig ens5f0 up
      ifconfig ens5f1 up

#. Create VF device(s):

   Echo the number of VFs to be created into "sriov_numvfs" sysfs entry
   of the parent PF.

   Example output:

   .. code-block:: console

      echo 2 > /sys/devices/pci0000:00/0000:00:03.0/0000:81:00.0/sriov_numvfs

#. Assign VF MAC address:

   Assign MAC address to the VF using iproute2 utility. The syntax is:
   ip link set <PF iface> vf <VF id> mac <macaddr>

   Example output:

   .. code-block:: console

      ip link set ens5f0 vf 0 mac 52:54:00:2f:9d:e8

#. PCI Passthrough:

   The VF devices may be passed through to the guest VM using virt-manager or
   virsh etc. bnx2x PMD should be used to bind the VF devices in the guest VM
   using the instructions outlined in the Application notes below.

#. Running testpmd:
   (Supply ``--log-level="pmd.net.bnx2x.driver",7`` to view informational messages):

   Follow instructions available in the document
   :ref:`compiling and testing a PMD for a NIC <pmd_build_and_test>`
   to run testpmd.

   Example output:

   .. code-block:: console

      [...]
      EAL: PCI device 0000:84:00.0 on NUMA socket 1
      EAL:   probe driver: 14e4:168e rte_bnx2x_pmd
      EAL:   PCI memory mapped at 0x7f14f6fe5000
      EAL:   PCI memory mapped at 0x7f14f67e5000
      EAL:   PCI memory mapped at 0x7f15fbd9b000
      EAL: PCI device 0000:84:00.1 on NUMA socket 1
      EAL:   probe driver: 14e4:168e rte_bnx2x_pmd
      EAL:   PCI memory mapped at 0x7f14f5fe5000
      EAL:   PCI memory mapped at 0x7f14f57e5000
      EAL:   PCI memory mapped at 0x7f15fbd4f000
      Interactive-mode selected
      Configuring Port 0 (socket 0)
      PMD: bnx2x_dev_tx_queue_setup(): fp[00] req_bd=512, thresh=512,
                   usable_bd=1020, total_bd=1024,
                                tx_pages=4
      PMD: bnx2x_dev_rx_queue_setup(): fp[00] req_bd=128, thresh=0,
                   usable_bd=510, total_bd=512,
                                rx_pages=1, cq_pages=8
      PMD: bnx2x_print_adapter_info():
      [...]
      Checking link statuses...
      Port 0 Link Up - speed 10000 Mbps - full-duplex
      Port 1 Link Up - speed 10000 Mbps - full-duplex
      Done
      testpmd>
