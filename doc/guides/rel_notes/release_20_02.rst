.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2019 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 20.02
==================

New Features
------------

* **Added Wait Until Equal API.**

  A new API has been added to wait for a memory location to be updated with a
  16-bit, 32-bit, 64-bit value.

* **Added rte_ring_xxx_elem APIs.**

  New APIs have been added to support rings with custom element size.

* **Added mbuf pool with pinned external memory.**

  Added support of mbuf with data buffer allocated in an external device memory.

* **Updated rte_flow api to support L2TPv3 over IP flows.**

  Added support for new flow item to handle L2TPv3 over IP rte_flow patterns.

* **Added DSCP rewrite action.**

  New actions ``RTE_FLOW_ACTION_TYPE_SET_IPV[4/6]_DSCP`` have been added
  to support rewrite the DSCP field in the IP header.

* **Added IONIC net PMD.**

  Added the new ``ionic`` net driver for Pensando Ethernet Network Adapters.
  See the :doc:`../nics/ionic` NIC guide for more details on this new driver.

* **Updated Broadcom bnxt driver.**

  Updated Broadcom bnxt driver with new features and improvements, including:

  * Added support for MARK action.

* **Updated Hisilicon hns3 driver.**

  Updated Hisilicon hns3 driver with new features and improvements, including:

  * Added support for Rx interrupt.
  * Added support setting VF MAC address by PF driver.

* **Updated the Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added support for MAC rules on a specific port.
  * Added support for MAC/VLAN with TCP/UDP in switch rule.
  * Added support for 1/10G device.
  * Added support for API ``rte_eth_tx_done_cleanup``.

* **Updated Intel iavf driver.**

  Updated iavf PMD with new features and improvements, including:

  * Added more supported device IDs.
  * Updated virtual channel to latest AVF spec.

* **Updated the Intel ixgbe driver.**

  Updated ixgbe PMD with new features and improvements, including:

  * Added support for API ``rte_eth_tx_done_cleanup()``.
  * Added support setting VF MAC address by PF driver.
  * Added support for setting the link to specific speed.

* **Updated Intel i40e driver.**

  Updated i40e PMD with new features and improvements, including:

  * Added support for L2TPv3 over IP profiles which can be programmed by the
    dynamic device personalization (DDP) process.
  * Added support for ESP-AH profiles which can be programmed by the
    dynamic device personalization (DDP) process.
  * Added PF support Malicious Device Drive event catch and notify.
  * Added LLDP support.
  * Extended PHY access AQ cmd.
  * Added support for reading LPI counters.
  * Added support for Energy Efficient Ethernet.
  * Added support for API ``rte_eth_tx_done_cleanup()``.
  * Added support for VF multiple queues interrupt.
  * Added support for setting the link to specific speed.

* **Updated Mellanox mlx5 driver.**

  Updated Mellanox mlx5 driver with new features and improvements, including:

  * Added support for the mbufs with external pinned buffers.
  * Added support for RSS using L3/L4 source/destination only.
  * Added support for matching on GTP tunnel header item.
  * Removed limitation of matching on tagged/untagged packets (when using DV flow engine).
  * Added support for IPv4/IPv6 DSCP rewrite action.
  * Added BlueField-2 integrated ConnectX-6 Dx device support.

* **Add new vDPA PMD based on Mellanox devices.**

  Added a new Mellanox vDPA  (``mlx5_vdpa``) PMD.
  See the :doc:`../vdpadevs/mlx5` guide for more details on this driver.

* **Added support for virtio-PMD notification data.**

  Added support for virtio-PMD notification data so that the driver
  passes extra data (besides identifying the virtqueue) in its device
  notifications, expanding the notifications to include the avail index and
  avail wrap counter (When split ring is used, the avail wrap counter is not
  included in the notification data).

* **Updated testpmd application.**

  Added support for ESP and L2TPv3 over IP rte_flow patterns to the testpmd
  application.

* **Added algorithms to cryptodev API.**

  Added new algorithms to the cryptodev API:

  * ECDSA (Elliptic Curve Digital Signature Algorithm) is added to
    asymmetric crypto library specifications.
  * ECPM (Elliptic Curve Point Multiplication) is added to
    asymmetric crypto library specifications.

* **Added synchronous Crypto burst API.**

  A new API has been introduced in the crypto library to handle synchronous cryptographic
  operations allowing it to achieve performance gains for cryptodevs which use
  CPU based acceleration, such as Intel AES-NI. An implementation for aesni_gcm
  cryptodev is provided. The IPsec example application and ipsec library itself
  were changed to allow utilization of this new feature.

* **Added handling of mixed algorithms in encrypted digest requests in QAT PMD.**

  Added handling of mixed algorithms in encrypted digest hash-cipher
  (generation) and cipher-hash (verification) requests (e.g. SNOW3G + ZUC or
  ZUC + AES CTR) in QAT PMD possible when running on GEN3 QAT hardware.
  Such algorithm combinations are not supported on GEN1/GEN2 hardware
  and executing the request returns ``RTE_CRYPTO_OP_STATUS_INVALID_SESSION``.

* **Queue-pairs are now thread-safe on Intel QuickAssist Technology (QAT) PMD.**

  Queue-pairs are thread-safe on Intel CPUs but Queues are not (that is, within
  a single queue-pair all enqueues to the TX queue must be done from one thread
  and all dequeues from the RX queue must be done from one thread, but enqueues
  and dequeues may be done in different threads.).

* **Updated the ZUC PMD.**

  * Transitioned underlying library from libSSO ZUC to intel-ipsec-mb
    library (minimum version required 0.53).
  * Removed dynamic library limitation, so PMD can be built as a shared
    object now.

* **Updated the KASUMI PMD.**

  * Transitioned underlying library from libSSO KASUMI to intel-ipsec-mb
    library (minimum version required 0.53).

* **Updated the SNOW3G PMD.**

  * Transitioned underlying library from libSSO SNOW3G to intel-ipsec-mb
    library (minimum version required 0.53).

* **Changed armv8 crypto PMD external dependency.**

  Changed armv8 crypto PMD external dependency. The
  armv8 crypto PMD now depends on the Arm crypto library, and Marvell's
  armv8 crypto library is not used anymore. The library name has been changed
  from armv8_crypto to AArch64crypto.

* **Added inline IPsec support to Marvell OCTEON TX2 PMD.**

  Added inline IPsec support to Marvell OCTEON TX2 PMD. With this feature,
  applications will be able to offload entire IPsec offload to the hardware.
  For the configured sessions, hardware will do the lookup and perform
  decryption and IPsec transformation. For the outbound path, applications
  can submit a plain packet to the PMD, and it will be sent out on the wire
  after doing encryption and IPsec transformation of the packet.

* **Added Marvell OCTEON TX2 End Point rawdev PMD.**

  Added a new OCTEON TX2 rawdev PMD for End Point mode of operation.
  See ``rawdevs/octeontx2_ep`` for more details on this new PMD.

* **Added event mode to l3fwd sample application.**

  Added event device support for the ``l3fwd`` sample application. It demonstrates
  usage of poll and event mode IO mechanism under a single application.

* **Added cycle-count mode to the compression performance tool.**

  Enhanced the compression performance tool by adding a cycle-count mode
  which can be used to help measure and tune hardware and software PMDs.

* **Added OpenWrt howto guide.**

  Added document which describes how to enable DPDK on OpenWrt in both virtual and
  physical machines.


Removed Items
-------------

* **Disabled building all the Linux kernel modules by default.**

  In order to remove the build time dependency on the Linux kernel,
  the Technical Board decided to disable all the kernel modules
  by default from 20.02 version.

* **Removed coalescing feature from Intel QuickAssist Technology (QAT) PMD.**

  The internal tail write coalescing feature was removed as not compatible with
  dual-thread feature. It was replaced with a threshold feature. At busy times
  if only a small number of packets can be enqueued, each enqueue causes
  an expensive MMIO write. These MMIO write occurrences can be optimized by using
  the new threshold parameter on process start. Please see QAT documentation for
  more details.


API Changes
-----------

* No change in this release.


.. _20_02_abi_changes:

ABI Changes
-----------

* No change, kept ABI v20. DPDK 20.02 is compatible with DPDK 19.11.

* The soname for each stable ABI version should be just the ABI version major
  number without the minor number. Unfortunately both major and minor were used
  in the v19.11 release, causing version v20.x releases to be incompatible with
  ABI v20.0.

  The `commit f26c2b39b271 <https://git.dpdk.org/dpdk/commit/?id=f26c2b39b271>`_
  fixed the issue by switching from 2-part to 3-part ABI version numbers so that
  we can keep v20.0 as soname and using the final digits to identify the DPDK
  20.x releases which are ABI compatible.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Atom\ |trade| CPU C3858 @ 2.00GHz
    * Intel\ |reg| Atom\ |trade| CPU C3958 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1541 @ 2.10GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 0 @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * CentOS 7.7
    * CentOS 8.0
    * Fedora 31
    * FreeBSD 12.1
    * Red Hat Enterprise Linux Server release 8.0
    * Red Hat Enterprise Linux Server release 7.7
    * Suse15SP1
    * Ubuntu 14.04
    * Ubuntu 16.04
    * Ubuntu 16.10
    * Ubuntu 18.04
    * Ubuntu 19.04

  * NICs:

    * Intel\ |reg| Corporation Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 1.02 0x80002b69
      * Device id (pf): 8086:1593
      * Driver version: 0.12.34 (ice)

    * Intel\ |reg| Corporation Ethernet Controller E810-C for SFP (2x100G)

      * Firmware version: 1.02 0x80002b68
      * Device id (pf): 8086:1592
      * Driver version: 0.12.34 (ice)

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version: 5.6.1 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Connection X552/X557-AT 10GBASE-T

      * Firmware version: 0x800003e7
      * Device id (pf/vf): 8086:15ad / 8086:15a8
      * Driver version: 5.1.0 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x80000482
      * Device id (pf): 8086:1563
      * Driver version: 5.6.1 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 7.20 0x800079e8
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.10.19.30 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 4.11 0x80001def
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.10.19.30 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T (2x10G)

      * Firmware version: 4.10 0x80001a7a
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version: 2.10.19.30 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 7.20 0x80007947
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.10.19.30 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 7.20 0x80007948
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.10.19.30 (i40e)

    * Intel\ |reg| Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80000cbc
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version: 5.4.0-k (igb)

    * Intel\ |reg| Corporation I210 Gigabit Network Connection

      * Firmware version: 3.25, 0x800006eb
      * Device id (pf): 8086:1533
      * Driver version: 5.4.0-k(igb)

* Intel\ |reg| platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:
    * Red Hat Enterprise Linux Server release 7.5 (Maipo)
    * Red Hat Enterprise Linux Server release 7.4 (Maipo)
    * Red Hat Enterprise Linux Server release 7.3 (Maipo)
    * Red Hat Enterprise Linux Server release 7.2 (Maipo)
    * Ubuntu 18.04
    * Ubuntu 16.04

  * OFED:

    * MLNX_OFED 4.7-3.2.9.0
    * MLNX_OFED 5.0-0.4.1.0 and above

  * upstream kernel:

    * Linux 5.5 and above

  * rdma-core:

    * rdma-core-28.0-1 and above

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCCT (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.27.1000 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.27.1000 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.27.1000 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.27.1000 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.27.1000 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.27.1000 and above


* Mellanox\ |reg| BlueField SmartNIC

  * Mellanox\ |reg| BlueField SmartNIC MT416842 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d2
    * Firmware version: 18.27.1000

  * SoC Arm cores running OS:

    * CentOS Linux release 7.5.1804 (AltArch)
    * MLNX_OFED 5.0-0.4.0.0

  * DPDK application running on Arm cores inside SmartNIC

* IBM Power 9 platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202) 2300MHz

  * OS:

    * Ubuntu 18.04.1 LTS (Bionic Beaver)

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.27.1000

  * OFED:

    * MLNX_OFED 5.0-0.4.1.0

* ARMv8 SoC combinations from Marvell (with integrated NICs)

  * SoC:

    * CN83xx, CN96xx, CN93xx

  * OS (Based on Marvell OCTEON TX SDK-10.3.2.x):

    * Arch Linux
    * Buildroot 2018.11
    * Ubuntu 16.04.1 LTS
    * Ubuntu 16.10
    * Ubuntu 18.04.1
    * Ubuntu 19.04
