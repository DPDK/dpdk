.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2020 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.02
==================

.. note::

   A **dependency** has been added for building DPDK on Linux or FreeBSD:
   the Python module **pyelftools** (version **0.22** or greater),
   often packaged as python3-pyelftools, is required.

   If not available as a distribution package, it can be installed with::

      pip3 install pyelftools


New Features
------------

* **Added new ethdev API for PMD power management.**

  Added ``rte_eth_get_monitor_addr()``, to be used in conjunction with
  ``rte_power_monitor()`` to enable automatic power management for PMDs.

* **Added Ethernet PMD power management helper API.**

  A new helper API has been added to make using Ethernet PMD power management
  easier for the user: ``rte_power_ethdev_pmgmt_queue_enable()``. Three power
  management schemes are supported initially:

  * Power saving based on UMWAIT instruction (x86 only)
  * Power saving based on ``rte_pause()`` (generic) or TPAUSE instruction (x86 only)
  * Power saving based on frequency scaling through the ``librte_power`` library

* **Added GENEVE TLV option in rte_flow.**

  Added support for matching and raw encap/decap of GENEVE TLV option.

* **Added support for Modify field action in the flow API.**

  Added "modify" action support to rte_flow to perform various operations on
  any arbitrary header field (as well as mark, metadata or tag values):
  ``RTE_FLOW_ACTION_TYPE_MODIFY_FIELD``.
  Supported operations are: overwriting a field with the content from
  another field, addition and subtraction using an immediate value.

* **Updated Broadcom bnxt driver.**

  Updated the Broadcom bnxt driver with fixes and improvements, including:

  * Added support for Stingray2 device.

* **Updated Cisco enic driver.**

  * Added support for 64B completion queue entries.

* **Updated Hisilicon hns3 driver.**

  * Added support for traffic management.

* **Updated Intel i40e driver.**

  * Added Intel i40e support on Windows.

* **Updated Intel ice driver.**

  Updated the Intel ice driver with new features and improvements, including:

  * Added Double VLAN support for DCF switch QinQ filtering.
  * Added support for UDP dynamic port assignment for eCPRI tunnels in DCF.

* **Updated Intel iavf driver.**

  Updated iavf PMD with new features and improvements, including:

  * Added support for FDIR/RSS packet steering for eCPRI flow.
  * Added support for FDIR TCP/UDP pattern without input set.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Introduced basic support on Windows.
  * Added GTP PDU session container matching and raw encap/decap.
  * Added support for a RSS action in the sample sub-actions list.
  * Added support for E-Switch mirroring and jump action in the same flow.
  * Added support to handle the modify action in correct order regarding the
    mirroring action on E-Switch.
  * Enlarged the number of flow priorities to 21844 (0 - 21843) for ingress or
    egress flow groups greater than 0 and for any transfer flow group.
  * Added support for the Tx mbuf fast free offload.
  * Added support for flow modify field action.

* **Updated the Pensando ionic driver.**

  Updated the Pensando ionic driver with new features and improvements, including:

  * Fixed bugs related to link autonegotiation.
  * Fixed bugs related to port start/stop and queue start/stop.
  * Added support for probing the supported queue versions. Note that
    extremely old (pre-1.0) firmware will no longer be usable with the PMD.
  * Removed unused code.
  * Reduced device startup time.

* **Updated Wangxun txgbe driver.**

  Updated the Wangxun txgbe driver with new features and improvements, including:

  * Add support for generic flow API.
  * Add support for traffic manager.
  * Add support for IPsec.

* **Updated GSO support.**

  * Added inner UDP/IPv4 support for VXLAN IPv4 GSO.

* **Added enqueue and dequeue callback APIs for cryptodev library.**

  The Cryptodev library has been enhanced with enqueue and dequeue callback APIs to
  enable applications to add/remove user callbacks which get called
  for every enqueue/dequeue operation.

* **Updated the OCTEON TX2 crypto PMD.**

  * Updated the OCTEON TX2 crypto PMD lookaside protocol offload for IPsec with
    ESN and anti-replay support.
  * Updated the OCTEON TX2 crypto PMD with CN98xx support.
  * Added support for aes-cbc sha1-hmac cipher combination in OCTEON TX2 crypto
    PMD lookaside protocol offload for IPsec.
  * Added support for aes-cbc sha256-128-hmac cipher combination in OCTEON TX2
    crypto PMD lookaside protocol offload for IPsec.

* **Added mlx5 compress PMD.**

  Added a new compress PMD for BlueField-2 adapters.

  See the :doc:`../compressdevs/mlx5` for more details.

* **Added python script to run crypto perf tests and graph the results.**

  A new Python script has been added to automate running crypto performance
  tests and output graphed results to PDF files.
  See the :doc:`../tools/cryptoperf` guide for more details.

* **Added Windows support to pmdinfogen.**

  PMD information strings were added for Windows as well as for other OS.
  Extracting them from Windows DLL is not yet supported.
  The build-time tool pmdinfogen was rewritten in Python,
  thus libelf dependency was replaced with pyelftools as new build dependency.

* **Added support for build-time checking of header includes.**

  A new build option ``check_includes`` has been added, which, when enabled,
  will perform build-time checking on DPDK public header files, to ensure none
  are missing dependent header includes. This feature, disabled by default, is
  intended for use by developers contributing to the DPDK SDK itself, and is
  integrated into the build scripts and automated CI for patch contributions.


Removed Items
-------------

* The internal header files ``rte_ethdev_driver.h``, ``rte_ethdev_vdev.h`` and
  ``rte_ethdev_pci.h`` are no longer installed as part of the DPDK
  ``ninja install`` action and are renamed to ``ethdev_driver.h``,
  ``ethdev_vdev.h`` and ``ethdev_pci.h`` respectively in the source tree, to
  reflect the fact that they are non-public headers.

* The internal header files ``rte_eventdev_pmd.h``, ``rte_eventdev_pmd_vdev.h``
  and ``rte_eventdev_pmd_pci.h`` are no longer installed as part of the DPDK
  ``ninja install`` action and are renamed to ``eventdev_pmd.h``,
  ``eventdev_pmd_vdev.h`` and ``eventdev_pmd_pci.h`` respectively in the source
  tree, to reflect the fact that they are non-public headers.

* Removed support for NetXtreme devices belonging to ``BCM573xx and
  BCM5740x`` families. Specifically the support for the following Broadcom
  PCI device IDs ``0x16c8, 0x16c9, 0x16ca, 0x16ce, 0x16cf, 0x16df, 0x16d0,``
  ``0x16d1, 0x16d2, 0x16d4, 0x16d5, 0x16e7, 0x16e8, 0x16e9`` has been removed.

* The ``check-includes.sh`` script for checking DPDK header files has been
  removed, being replaced by the ``check_includes`` build option described
  above.


API Changes
-----------

* config: Removed the old macros, included in ``rte_config.h``,
  to indicate which DPDK libraries and drivers are built.
  The new macros are generated by meson in a standardized format:
  ``RTE_LIB_<NAME>`` and ``RTE_<CLASS>_<NAME>``, where ``NAME`` is
  the upper-case component name, e.g. ``EAL``, ``ETHDEV``, ``VIRTIO``,
  and ``CLASS`` is the upper-case driver class, e.g. ``NET``, ``CRYPTO``.

* cryptodev: The structure ``rte_cryptodev`` has been updated with pointers
  for adding enqueue and dequeue callbacks.


ABI Changes
-----------

* No ABI change that would break compatibility with 20.11.

* The experimental function ``rte_telemetry_init`` has been removed from the
  public API and is now an internal-only function. Where telemetry library is
  available, it is called automatically from ``rte_eal_init()`` and so no end
  application need use it.


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
    * Intel\ |reg| Xeon\ |reg| Gold 5218N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * CentOS 8.3
    * CentOS Stream 8
    * Fedora 33
    * FreeBSD 12.1
    * OpenWRT 19.07.4
    * Red Hat Enterprise Linux Server release 8.3
    * Suse 15 SP2
    * Ubuntu 20.04
    * Ubuntu 20.10

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 2.40
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version: 1.4.5 (ice)
      * OS Default DDP: 1.3.21.0
      * COMMS DDP: 1.3.25.0
      * Wireless Edge DDP: 1.3.1.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 2.40
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version: 1.4.5 (ice)
      * OS Default DDP: 1.3.21.0
      * COMMS DDP: 1.3.25.0
      * Wireless Edge DDP: 1.3.1.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version: 5.10.2 (ixgbe)

    * Intel\ |reg| Corporation Ethernet Connection X552/X557-AT 10GBASE-T

      * Firmware version: 0x800003e7
      * Device id (pf/vf): 8086:15ad / 8086:15a8
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Corporation Ethernet Controller 10G X550T

      * Firmware version: 0x80000482
      * Device id (pf): 8086:1563
      * Driver version: 5.10.2 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 8.00 0x80008b82 1.2766.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.14.13 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 5.00 0x800023c3 1.2766.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.14.13 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T (2x10G)

      * Firmware version: 4.10 0x80001a7a
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version: 2.14.13 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 8.00 0x80008c1a 1.2766.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.14.13 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version: 8.00 0x80008b82 1.2766.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.14.13 (i40e)

    * Intel\ |reg| Corporation I350 Gigabit Network Connection

      * Firmware version: 1.63, 0x80000cbc
      * Device id (pf/vf): 8086:1521 / 8086:1520
      * Driver version: 5.6.0-k (igb)

    * Intel\ |reg| Corporation I210 Gigabit Network Connection

      * Firmware version: 3.25, 0x800006eb
      * Device id (pf): 8086:1533
      * Driver version: 5.6.0-k (igb)

    * Intel\ |reg| Ethernet Controller 10-Gigabit X540-AT2

      * Firmware version: 0x800005f9
      * Device id (pf): 8086:1528
      * Driver version: 5.1.0-k (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 8.00 0x80008d10 1.2766.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.14.13 (i40e)

* Intel\ |reg| platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697A v4 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2697 v3 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2670 0 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v4 @ 2.20GHz
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

    * MLNX_OFED 5.2-2.2.0.0 and above
    * MLNX_OFED 5.1-2.5.8.0

  * upstream kernel:

    * Linux 5.11.0-rc7 and above

  * rdma-core:

    * rdma-core-33.1-1 and above

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
      * Firmware version: 14.29.2002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.29.2002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.29.2002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.29.2002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.29.2002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.29.2002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.29.2002 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.29.2002 and above

* Mellanox\ |reg| BlueField\ |reg| SmartNIC

  * Mellanox\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d2
    * Firmware version: 24.29.2002 and above

  * Embedded software:

    * CentOS Linux release 7.6.1810 (AltArch)
    * MLNX_OFED 5.2-2.2.0 and above
    * DPDK application running on Arm cores

* Intel\ |reg| platforms with Broadcom\ |reg| NICs combinations

  * CPU:

    * Intel\ |reg| Xeon\ |reg| Gold 6154 CPU @ 3.00GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2667 v3 @ 3.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2650 v2 @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6142 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| CPU E3-1270 v3 @ 3.50GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6134M CPU @ 3.20GHz

  * OS:

    * Red Hat Enterprise Linux Server release 8.1
    * Red Hat Enterprise Linux Server release 7.6
    * Centos 8.1
    * Centos 7.8
    * Centos 7.7

  * upstream kernel:

    * Linux 5.3.4

  * NICs:

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P225p (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Firmware version: 214.4.114.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P425p (4x25G)

      * Host interface: PCI Express 3.0 x16
      * Firmware version: 218.0.124.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P2100G (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Firmware version: 218.0.124.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P425p (4x25G)

      * Host interface: PCI Express 4.0 x16
      * Firmware version: 218.0.124.0 and above

    * Broadcom\ |reg| NetXtreme-E\ |reg| Series P2100G (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Firmware version: 218.0.124.0 and above

* Broadcom\ |reg| NetXtreme-S\ |reg| Series SmartNIC

  * Broadcom\ |reg| NetXtreme-S\ |reg| Series PS225-H16 SmartNIC (2x25G)

    * Host interface: PCI Express 3.0 x8
    * Firmware version: 218.1.143.0

  * Embedded software:

    * Broadcom Yocto Linux
    * Kernel version: 4.14.196
    * DPDK application running on 8 Arm Cortex-A72 cores

* NXP ARMv8 SoCs (with integrated NICs)

  * SoC:

    * LX2xxx, LS2xxx, LS10xx

  * OS (based on NXP LSDK-20.04):

    * Kernel version: 4.19.90
    * Kernel version: 5.4.47
    * Ubuntu 18.04
