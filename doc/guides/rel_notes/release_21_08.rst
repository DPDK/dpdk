.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.08
==================

New Features
------------

* **Added auxiliary bus support.**

  An auxiliary bus provides a way to split a function into child-devices
  representing sub-domains of functionality. Each auxiliary device
  represents a part of its parent functionality.

* **Added XZ compressed firmware support.**

  Using ``rte_firmware_read`` a driver can now handle XZ compressed firmware
  in a transparent way, with EAL uncompressing using libarchive, if this library
  is available when building DPDK.

* **Updated Amazon ENA PMD.**

  Updated the Amazon ENA PMD. The new driver version (v2.4.0) introduced
  bug fixes and improvements, including:

  * Added Rx interrupt support.
  * RSS hash function key reconfiguration support.

* **Updated Intel iavf driver.**

  * Added Tx QoS VF queue TC mapping.
  * Added FDIR and RSS for GTPoGRE, and support for filters based on GTPU TEID/QFI,
    outermost L3 or innermost L3/L4.

* **Updated Intel ice driver.**

  * Added new RX and TX paths in the AVX2 code to use HW offload
    features. When the HW offload features are configured to be used, the
    offload paths are chosen automatically. In parallel the support for HW
    offload features was removed from the legacy AVX2 paths.
  * Added Tx QoS TC bandwidth configuration in DCF.

* **Added support for Marvell CN10K SoC Ethernet device.**

  * Added net/cnxk driver which provides the support for the integrated Ethernet
    device.

* **Updated Mellanox mlx5 driver.**

  * Added Sub-Function support based on auxiliary bus.
  * Added support for meter hierarchy.
  * Added support for metering policy actions of yellow color.
  * Added support for metering trTCM RFC2698 and RFC4115.
  * Added devargs option ``allow_duplicate_pattern``.
  * Added matching on IPv4 Internet Header Length (IHL).
  * Added support for matching on VXLAN header last 8-bits reserved field.
  * Optimized multi-thread flow rule insertion rate.

* **Updated Solarflare network PMD.**

  Updated the Solarflare ``sfc_efx`` driver with changes including:

  * Added COUNT action support for SN1000 NICs.

* **Added Wangxun ngbe PMD.**

  Added a new PMD for Wangxun 1Gb Ethernet NICs.
  See the :doc:`../nics/ngbe` for more details.

* **Added inflight packets clear API in vhost library.**

  Added an API which can clear the inflight packets submitted to
  the DMA engine in the vhost async data path.

* **Updated Intel QuickAssist crypto PMD.**

  Added fourth generation of QuickAssist Technology(QAT) devices support.
  Only symmetric crypto has been currently enabled. Compression and asymmetric
  crypto PMD will fail to create.

* **Added support for Marvell CNXK crypto driver.**

  * Added cnxk crypto PMD which provides support for an integrated
    crypto driver for CN9K and CN10K series of SOCs. Support for
    symmetric crypto algorithms was added to both the PMDs.
  * Added support for lookaside protocol (IPsec) offload in cn10k PMD.
  * Added support for asymmetric crypto operations in cn9k and cn10k PMD.

* **Updated Marvell OCTEON TX crypto PMD.**

  Added support for crypto adapter ``OP_FORWARD`` mode.

* **Added support for Nvidia crypto device driver.**

  Added mlx5 crypto driver to support AES-XTS cipher operations.
  The first device to support it is ConnectX-6.

* **Updated ISAL compress device PMD.**

  The ISAL compress device PMD now supports Arm platforms.

* **Added Baseband PHY CNXK PMD.**

  Added Baseband PHY PMD which allows configuration of the BPHY hardware block
  comprising accelerators and DSPs specifically tailored for 5G/LTE inline
  use cases. Configuration happens via standard rawdev enq/deq operations. See
  the :doc:`../rawdevs/cnxk_bphy` rawdev guide for more details on this driver.

* **Added support for Marvell CN10K, CN9K, event Rx/Tx adapter.**

  * Added Rx/Tx adapter support for event/cnxk when the Ethernet device requested
    is net/cnxk.
  * Added support for event vectorization for Rx/Tx adapter.

* **Added cppc_cpufreq support to Power Management library.**

  Added support for cppc_cpufreq driver which works on most arm64 platforms.

* **Added multi-queue support to Ethernet PMD Power Management.**

  The experimental PMD power management API now supports managing
  multiple Ethernet Rx queues per lcore.

* **Updated testpmd to output log errors to stderr.**

  Updated testpmd application to output log errors and warnings to stderr
  instead of stdout.


API Changes
-----------

* eal: ``rte_strscpy`` sets ``rte_errno`` to ``E2BIG`` in case of string
  truncation.

* eal: ``rte_bsf32_safe`` now takes a 32-bit value for its first argument.
  This fixes warnings about loss of precision
  when used with some compilers settings.

* eal: ``rte_power_monitor`` and the ``rte_power_monitor_cond`` struct changed
  to use a callback mechanism.

* rte_power: The experimental PMD power management API is no longer considered
  to be thread safe; all Rx queues affected by the API will now need to be
  stopped before making any changes to the power management scheme.


ABI Changes
-----------

* No ABI change that would break compatibility with 20.11.


Known Issues
------------

* **Last mbuf segment not implicitly reset.**

  It is expected that free mbufs have their field ``nb_seg`` set to 1,
  so that when it is allocated, the user does not need to set its value.
  The mbuf free functions are responsible of resetting this field to 1
  before returning the mbuf to the pool.

  When a multi-segment mbuf is freed, the field ``nb_seg`` is not reset
  to 1 for the last segment of the chain. On next allocation of this segment,
  if the field is not explicitly reset by the user,
  an invalid mbuf can be created, and can cause an undefined behavior.

  This issue has a root cause in DPDK 17.05, meaning it is 4 years old.
  A fix is available and discussed but not merged in DPDK 21.08:
  https://patches.dpdk.org/patch/86458/


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3958 @ 2.00GHz
    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1541 @ 2.10GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180M CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * Fedora 34
    * OpenWRT 19.07.4
    * FreeBSD 12.1
    * Red Hat Enterprise Linux Server release 8.3
    * Suse 15 SP2
    * Ubuntu 20.04
    * Ubuntu 21.04

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 3.10
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version: 1.7.0 (ice)
      * OS Default DDP: 1.3.27.0
      * COMMS DDP: 1.3.31.0
      * Wireless Edge DDP: 1.3.7.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 3.10
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version: 1.7.0 (ice)
      * OS Default DDP: 1.3.27.0
      * COMMS DDP: 1.3.31.0
      * Wireless Edge DDP: 1.3.7.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Corporation Ethernet Connection X552/X557-AT 10GBASE-T

      * Firmware version: 0x800003e7
      * Device id (pf/vf): 8086:15ad / 8086:15a8
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x8000113b
      * Device id (pf): 8086:1563
      * Driver version: 5.11.3 (ixgbe)

    * Intel\ |reg| Ethernet Controller 10-Gigabit X540-AT2

      * Firmware version: 0x800005f9
      * Device id (pf): 8086:1528
      * Driver version: 4.18.0-305.7.1.el8_4.x86_64 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 8.30 0x8000a49d 1.2926.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 5.30 0x80002a29 1.2926.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T (2x10G)

      * Firmware version: 4.10 0x80001a7a 1.2154.0
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 8.30 0x8000a485 1.2926.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 8.30 0x8000a4ae 1.2926.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.15.9 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 8.30 0x8000a489 1.2926.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.15.9 (i40e)

* Intel\ |reg| platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2640 @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 0 @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2620 v4 @ 2.10GHz

  * OS:

    * Red Hat Enterprise Linux release 8.2 (Ootpa)
    * Red Hat Enterprise Linux Server release 7.8 (Maipo)
    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Red Hat Enterprise Linux Server release 7.5 (Maipo)
    * Red Hat Enterprise Linux Server release 7.4 (Maipo)
    * Red Hat Enterprise Linux Server release 7.3 (Maipo)
    * Red Hat Enterprise Linux Server release 7.2 (Maipo)
    * Ubuntu 20.04
    * Ubuntu 18.04
    * Ubuntu 16.04
    * SUSE Enterprise Linux 15 SP2
    * SUSE Enterprise Linux 12 SP4

  * OFED:

    * MLNX_OFED 5.4-1.0.3.0 and above
    * MLNX_OFED 5.3-1.0.0.1

  * upstream kernel:

    * Linux 5.14.0-rc3 and above

  * rdma-core:

    * rdma-core-36.0 and above

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
      * Firmware version: 14.31.1014 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.31.1014 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.31.1014 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.31.1014 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.31.1014 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.31.1014 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.31.1014 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.31.1014 and above

* Mellanox\ |reg| BlueField\ |reg| SmartNIC

  * Mellanox\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.31.1014 and above

  * Embedded software:

    * CentOS Linux release 7.6.1810 (AltArch)
    * MLNX_OFED 5.4-1.0.3.0 and above
    * DPDK application running on Arm cores
