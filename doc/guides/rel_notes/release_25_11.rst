.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2025 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 25.11
==================

New Features
------------

* **Added automatic lcore-id remapping option.**

  Added the EAL option ``--remap-lcore-ids`` or ``-R``
  to enable automatic remapping of lcore-ids to a contiguous set starting from 0,
  or from a user-provided value.
  When this flag is passed, the lcores specified by core mask or core list options
  are taken as the physical cores on which the application will run,
  and one thread will be started per core, with sequential lcore-ids.
  For example: ``dpdk-test -l 140-144 -R``
  will start 5 threads with lcore-ids 0 to 4 on physical cores 140 to 144.

* **Added inter-process and inter-OS DMA device API.**

  * Added parameters in DMA device virtual channel to configure DMA operations
    that span across different processes or operating system domains.
  * Added functions to exchange handlers between DMA devices.

* **Added 800G speed.**

  Added Ethernet link speed for 800 Gb/s as it is well standardized in IEEE,
  and some devices already support this speed.

* **Added mbuf tracking for debug.**

  Added history dynamic field in mbuf
  to store successive states of the mbuf lifecycle.
  Some functions were added to dump statistics.
  A script was added to parse mbuf tracking stored in a file.
  This feature is disabled by default.

* **Added ethdev API to get link connector.**

  Added API to report type of link connector for a port.
  The following connectors are enumerated:

  * None
  * Twisted Pair
  * Attachment Unit Interface (AUI)
  * Optical Fiber Link
  * BNC
  * Direct Attach Copper
  * XFI, SFI
  * Media Independent Interface (MII)
  * SGMII, QSGMII
  * XLAUI, GAUI, AUI, CAUI, LAUI
  * SFP, SFP+, SFP28, SFP-DD
  * QSFP, QSFP+, QSFP28, QSFP56, QSFP-DD
  * OTHER

  By default, it reports ``RTE_ETH_LINK_CONNECTOR_NONE``
  unless the driver specifies it.

* **Updated Amazon ENA (Elastic Network Adapter) ethernet driver.**

  * Added support for retrieving HW timestamps for Rx packets with nanosecond resolution.
  * Fixed PCI BAR mapping on 64K page size.

* **Added Huawei hinic3 ethernet driver.**

  Added network driver for the Huawei SPx series Network Adapters.

* **Updated Intel ice ethernet driver.**

  * Added support for Data Center Bridging (DCB).
  * Added support for Priority Flow Control (PFC).

* **Updated Marvell cnxk ethernet driver.**

  Added support to set/get link configuration as outlined below:

  * Get speed capability from firmware.
  * Report link type, mode and status.
  * Configure link mode.

* **Added Nebulamatrix nbl ethernet driver.**

  Added the PMD for Nebulamatrix NICs.

* **Updated NVIDIA mlx5 driver.**

  * Added support for NVIDIA ConnectX-9 SuperNIC adapters.
  * Added support for count and age flow actions on root tables
    with HW steering flow engine.

* **Updated NXP DPAA2 ethernet driver.**

  * Enabled software taildrop for ordered queues.
  * Added additional MAC counters in xstats.

* **Added NXP ENETC4 ethernet driver.**

  Added ENETC4 PMD for multiple new generation SoCs.

* **Updated TAP ethernet driver.**

  * Replaced ioctl-based link control with a Netlink-based implementation.
  * Linux net devices can now be renamed without breaking link control.
  * Linux net devices can now be moved to different namespaces
    without breaking link control (requires Linux >= 5.2).

* **Updated Wangxun txgbe ethernet driver.**

  Added support for Wangxun Amber-Lite NIC series,
  including FF5025 (supporting 10G and 25G) and FF5040 (supporting 40G).
  As these new models share hardware similarities with the existing 10G Sapphire NICs,
  many of the existing configurations and practices are expected to apply.

* **Updated Yunsilicon xsc ethernet driver.**

  * Added FW version query.
  * Added TSO support.
  * Added module EEPROM dump.
  * Added promiscuous mode.
  * Added link status.
  * Added link event.
  * Added FEC get and set.
  * Added multi-process per port.
  * Optimized code.

* **Added PQC ML algorithms in cryptodev.**

  * Added PQC ML-KEM support with reference to FIPS203.
  * Added PQC ML-DSA support with reference to FIPS204.

* **Updated openssl crypto driver.**

  * Added support for PQC ML-KEM and ML-DSA algorithms.

* **Updated Intel QuickAssist Technology (QAT) crypto driver.**

  * Added SM2 encryption and decryption algorithms.

* **Renamed HiSilicon DMA driver.**

  Renamed ``dma/hisilicon`` to ``dma/hisi_pciep`` (PCIe internal endpoint)
  to reflect hardware IP.

* **Added HiSilicon Accelerator DMA driver.**

  Kunpeng SoC has an internal accelerator unit which includes zip function,
  and the zip also supports data copy and fill.
  This driver exposes this capability to DPDK applications.

* **Added RCU support in the FIB6 library.**

  It is now possible to register an RCU QSBR object
  to handle graceful deletion of table groups.

* **Added packet capture (pdump) for secondary process.**

  Added multi-process support to allow packets sent and received
  by secondary process to be visible in packet capture.

* **Allow overriding the automatic usage/help generation in argparse library.**

  The argparse library now supports overriding the automatic help text generation,
  by allowing the user to provide a custom function to generate the output text.
  The built-in help text function is available as a public function
  which can be reused by custom functions, if so desired.


Removed Items
-------------

* build: as previously announced in the deprecation notices,
  the ``enable_kmods`` build option has been removed.
  Kernel modules will now be built automatically for OSes where out-of-tree kernel modules
  are required for DPDK operation.
  Currently, this means that modules will only be built for FreeBSD.
  No modules are shipped with DPDK for either Linux or Windows.

* ethdev: As previously announced in deprecation notes,
  queue specific stats fields are now removed from ``struct rte_eth_stats``.
  Affected fields are: ``q_ipackets``, ``q_opackets``, ``q_ibytes``, ``q_obytes``, ``q_errors``.
  Queue stats will be received via the xstats API instead.
  Also, the compile time flag ``RTE_ETHDEV_QUEUE_STAT_CNTRS`` is removed from public headers.

* telemetry: As previously announced in the deprecation notices,
  the functions ``rte_tel_data_add_array_u64`` and ``rte_tel_data_add_dict_u64`` are removed.
  They are replaced by ``rte_tel_data_add_array_uint`` and ``rte_tel_data_add_dict_uint`` respectively.

* net/mlx5: The ``repr_matching_en`` device argument has been removed.
  Applications which disabled this option were able to receive traffic
  from any physical port/VF/SF on any representor port.
  Specifically, in most cases, this was used to process all traffic on a representor port
  which is a transfer proxy port.
  Similar behavior in mlx5 PMD can be achieved without this device argument,
  by using ``RTE_FLOW_ACTION_TYPE_RSS`` in transfer flow rules.


API Changes
-----------

* rawdev: Changed the return type of ``rte_rawdev_get_dev_id()``
  to allow negative error values.

* pcapng: Changed the API for adding interfaces to include a link type argument.
  The link type was previously hardcoded to the Ethernet link type in the API.
  This argument is added to ``rte_pcapng_add_interface``.

* bitmap: Changed the return type of ``rte_bitmap_free()`` to void
  for consistency with other free functions.


ABI Changes
-----------

* eal: The structure ``rte_mp_msg`` alignment has been updated to 8 bytes to limit unaligned
  accesses in messages payload.

* stack: The structure ``rte_stack_lf_head`` alignment has been updated to 16 bytes
  to avoid unaligned accesses.

* ethdev: Added ``link_connector`` field to ``rte_eth_link`` structure
  to report the type of link connector for a port.

* cryptodev: The ``rte_crypto_sm2_op_param`` struct member ``cipher`` to hold ciphertext
  is changed to a union data type. This change is required to support partial SM2 calculation
  which is driven by ``RTE_CRYPTO_SM2_PARTIAL`` capability flag.

* cryptodev: The enum ``rte_crypto_asym_xform_type``, struct ``rte_crypto_asym_xform``
  and struct ``rte_crypto_asym_op`` are updated to include new values
  to support ML-KEM and ML-DSA.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| x7835RE
    * Intel\ |reg| Xeon\ |reg| 6553P-B CPU @ 2.6GHz
    * Intel\ |reg| Xeon\ |reg| 6767P CPU @ 2.4GHz
    * Intel\ |reg| Xeon\ |reg| CPU Max 9480
    * Intel\ |reg| Xeon\ |reg| CPU Max 9480 CPU @ 1.9GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6330 CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8468H CPU @ 2.1GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8490H CPU @ 1.9GHz

  * OS:

    * Fedora 42
    * FreeBSD 14.3
    * Microsoft Azure Linux 3.0
    * OpenAnolis OS 8.10
    * openEuler 24.03 (LTS-SP2)
    * Red Hat Enterprise Linux Server release 10
    * Red Hat Enterprise Linux Server release 9.6
    * Ubuntu 24.04 LTS
    * Ubuntu 24.04.3 LTS
    * Vmware ESXi 8.0.3

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.90 0x80020eeb 1.3863.0
      * Device ID (PF/VF): 8086:1593 / 8086:1889
      * Driver version (out-of-tree): 2.3.14 (ice)
      * Driver version (in-tree): 6.8.0-86-generic (Ubuntu24.04 LTS) (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.90 0x80020ef2 1.3863.0
      * Device ID (PF/VF): 8086:1592 / 8086:1889
      * Driver version (out-of-tree): 2.3.14 (ice)
      * Driver version (in-tree): 6.6.12.1-1.azl3+ice+ (Microsoft Azure Linux 3.0) /
        6.12.0-55.9.1.el10_0.x86_64+rt (RHEL10) (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E830-CC for SFP

      * Firmware version: 1.20 0x800179c0 1.3862.0
      * Device ID (PF/VF): 8086:12d3 / 8086:1889
      * Driver version: 2.3.14 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Controller E830-CC for QSFP

      * Firmware version: 1.20 0x800179c1 1.3862.0
      * Device ID (PF/VF): 8086:12d2 / 8086:1889
      * Driver version: 2.3.14 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Connection E825-C for QSFP

      * Firmware version: 4.04 0x80007e5c 1.3885.0
      * Device ID (PF/VF): 8086:579d / 8086:1889
      * Driver version: 2.3.14 (ice)
      * OS Default DDP: 1.3.43.0
      * COMMS DDP: 1.3.55.0
      * Wireless Edge DDP: 1.3.25.0

    * Intel\ |reg| Ethernet Network Adapter E610-XT2

      * Firmware version: 1.30 0x8000e8a2 1.3864.0
      * Device ID (PF/VF): 8086:57b0 / 8086:57ad
      * Driver version (out-of-tree): 6.2.5 (ixgbe)

    * Intel\ |reg| Ethernet Network Adapter E610-XT4

      * Firmware version: 1.30 0x8000e8a5 1.3864.0
      * Device ID (PF/VF): 8086:57b0 / 8086:57ad
      * Driver version (out-of-tree): 6.2.5 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.55 0x8000fdf4 1.3862.0
      * Device ID (PF/VF): 8086:1572 / 8086:154c
      * Driver version (in-tree): 6.8.0-88-generic (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.51 0x80004430 1.3862.0
      * Device ID (PF/VF): 8086:37d0 / 8086:37cd
      * Driver version (out-of-tree): 2.28.13 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version:  9.55 0x8000fe4c 1.3862.0
      * Device ID (PF/VF): 8086:158b / 8086:154c
      * Driver version (out-of-tree): 2.28.13 (i40e)
      * Driver version (in-tree): 6.8.0-88-generic (Ubuntu24.04.3 LTS) /
        6.12.0-55.9.1.el10_0.x86_64 (RHEL10) (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.55 0x8000fe31 1.3862.0
      * Device ID (PF/VF): 8086:1583 / 8086:154c
      * Driver version (out-of-tree): 2.28.13 (i40e)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device ID (PF): 8086:125b
      * Driver version (in-tree): 6.8.0-87-generic (Ubuntu24.04.3 LTS) (igc)

    * Intel\ |reg| Corporation Ethernet Server Adapter I350-T4

      * Firmware version: 1.63, 0x8000116b
      * Device ID (PF/VF): 8086:1521 /8086:1520
      * Driver version: 6.14.0-35-generic (Ubuntu 24.04.3 LTS) (igb)

    * Intel\ |reg| Infrastructure Processing Unit (Intel\ |reg| IPU) E2100

      * Firmware version: ci-ts.release.2.0.0.11126
      * Device ID (idpf/cpfl): 8086:1452/8086:1453
      * Driver version: 0.0.772 (idpf)

* Intel\ |reg| platforms with NVIDIA\ |reg| NICs combinations

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

    * Red Hat Enterprise Linux release 9.2 (Plow)
    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Ubuntu 24.04
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP4

  * DOCA:

    * DOCA 3.2.0-125000 and above.

  * upstream kernel:

    * Linux 6.17.0 and above

  * rdma-core:

    * rdma-core-58.0 and above

  * NICs

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-8 SuperNIC 400G  MT4131 - 900-9X81Q-00CN-STA (2x400G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1023
      * Firmware version: 40.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-9 SuperNIC 800G  MT4133 - 900-9X91E-00EB-STA_Ax (1x800G)

      * Host interface: PCI Express 6.0 x16
      * Device ID: 15b3:1025
      * Firmware version: 82.47.0366 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.47.1026 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.47.1026 and above

  * Embedded software:

    * Ubuntu 24.04
    * MLNX_OFED 25.10-1.2.8.0
    * bf-bundle-3.2.0-113_25.10_ubuntu-24.04
    * DPDK application running on ARM cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Ubuntu 20.04

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.47.1026 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.47.1026 and above

  * DOCA:

    * DOCA 3.2.0-125000 and above

* IBM Power 10 with NVIDIA\ |reg| NIC `[added in March 2026]`

  * CPU:

    * IBM Power 10 CHRP IBM,9105-22A

  * OS:

    * RHEL 10.0 kernel: 6.12.0-55.9.1.el10_0.ppc64le

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MT28800 Family

      * Host interface: PCI Express 3.0 x16
      * OFED 25.07-0.9.7
      * Firmware Version: 16.35.2000

25.11.1 Release Notes
---------------------


25.11.1 Fixes
~~~~~~~~~~~~~

* app/dma-perf: fix buffer overflow with high core count
* app/dma-perf: fix reversed CPU copy
* app/graph: fix variable shadowing
* app/pdump: fix variable shadowing
* app/testpmd: check start for DCB forwarding TC commands
* app/testpmd: fix DCB forwarding TC mismatch handling
* app/testpmd: fix FD leak in mbuf history command parsing
* app/testpmd: fix flow queue job leaks
* app/testpmd: fix function names in logs
* app/testpmd: fix memory leak in port attach
* app/testpmd: fix memory leak in port flow configure
* app/testpmd: fix variable shadowing
* bbdev: fix variable shadowing
* bitops: allow variable as first argument of shift macros
* bpf: disallow empty program
* bpf: fix add/subtract overflow
* bpf: fix signed shift overflows in ARM JIT
* bpf: fix starting with conditional jump
* bpf: fix x86 call stack alignment for external calls
* buildtools/test: suppress empty output
* bus/fslmc: fix devargs not propagated on hotplug
* bus/pci: fix variable shadowing
* ci: fix debug build type
* common/cnxk: fix array out-of-bounds
* common/cnxk: fix buffer overflow in SA setup
* common/cnxk: fix cipher key length validation
* common/cnxk: fix CPT CQ roll over handling
* common/cnxk: fix duplicated branches
* common/cnxk: fix engine capabilities fetch logic
* common/cnxk: fix width of DPC set field
* common/cnxk: remove VLA in interrupt configuration
* common/mlx5: fix bonding check
* common/mlx5: fix error logging for queue modify
* common/mlx5: fix MAC deletion on Linux
* common/mlx5: fix variable shadowing
* common/sfc_efx/base: define mask for pause mode capabilities
* common/sfc_efx/base: fix flow control on legacy MCDI
* common/sfc_efx/base: fix indication of requestable FEC flags
* compress/zlib: fix UDC checksum
* config/arm: drop crypto extension from Cortex-A78AE
* config/arm: fix 32-bit build
* crypto/cnxk: return decrypted data for RSA verify
* cryptodev: fix memory corruption in secondary process
* crypto/openssl: fix SM2 public key buffer overflow
* crypto/qat: align vector address
* dma/idxd: mark portal pointer as volatile
* doc: add IBM Power 10 testing to 25.11 release notes
* doc: add --query-rate option in flow-perf guide
* doc: fix E830 recommended firmware in ice guide
* doc: fix inline crypto feature status for iavf
* doc: fix TSO and checksum offload feature status for ice
* doc: fix TSO feature status for i40e
* doc: fix TSO feature status for iavf
* doc: remove references to obsolete testpmd flag
* doc: update AMD EPYC guide
* dts: avoid resources conflict on quick stop/start
* dts: fix doc for QinQ test suite
* dts: fix doc for RSS test suite
* dts: fix doc generation for testpmd shell
* dts: fix docstring typo in dynamic queue suite
* dts: fix test suite filtering
* dts: reduce digit count in single core forwarding metrics
* dts: show missing NIC capabilities in logs
* dts: sniff only inbound packets
* eal: fix annotation on lcore variable allocator
* eal: fix cache guard for pedantic compilation
* eal: fix variable shadowing
* eal/linux: fix fbarray name collision in containers
* eal/linux: fix HPET symbols export
* eal/linux: handle interrupt epoll events
* eal/x86: fix TSC frequency query
* ethdev: fix mbuf fast release requirements description
* ethdev: fix variable shadowing
* event/cnxk: fix crash on CN10K
* eventdev/eth_rx: fix crash with telemetry
* eventdev: fix variable shadowing
* examples/ethtool: fix error message about ports limit
* examples/ethtool: fix size of mempool name
* examples/fips_validation: fix dangling pointers
* examples/fips_validation: fix RSA memcpy
* examples/ipsec-secgw: fix build with glibc 2.43
* examples/l2fwd-jobstats: fix stats availability
* examples/packet_ordering: fix format specifier for port ID
* examples/ptpclient: fix format specifier for port ID
* examples/vmdq_dcb: initialize all configuration structures
* examples/vm_power: fix format-truncation warning
* examples/vm_power_manager: fix format specifier for port ID
* fib: fix prefix addition handling
* hash: avoid leaking entries on RCU defer queue failure
* hash: fix maybe-uninitialized warnings on build
* hash: fix overflow of 32-bit offsets
* hash: fix unaligned access for CRC
* hash: free replaced data on overwrite when RCU is configured
* interrupts: add interrupt event info
* mbuf: fix packet data read
* mem: check fbarray name truncation in secondary process
* net/af_packet: fix MTU set data size calculation
* net/af_packet: fix receive buffer overflow
* net/af_xdp: fix external mbuf transmit
* net/axgbe: add 100 Mbps MAC speed select
* net/axgbe: fix 100M SGMII mode
* net/axgbe: fix auto-negotiation capabilities
* net/axgbe: fix MAC TCR speed select field width
* net/axgbe: fix SGMII auto-negotiation status bits
* net/bnxt: fix build with GCC 16
* net/bnxt: fix commas instead of semicolons
* net/bnxt: fix crash in HWRM capabilities query
* net/bnxt: fix reported VLAN stripped flag for Thor2
* net/bnxt: fix statistics for high number of queues
* net/bnxt: fix uninitialized read
* net/bnxt: remove unbuilt source files
* net/bnxt: support statistics query when port is stopped
* net/bnxt/tf_ulp: fix minsize build
* net/bonding: clamp Rx free threshold for small rings
* net/cnxk: fix security flag for custom inbound SA
* net/cpfl: fix variable shadowing
* net/dpaa2: add SG table walk upper bound in Rx
* net/dpaa2: fix burst mode info
* net/dpaa2: fix error packet dump
* net/dpaa2: fix L3/L4 checksum offload flags
* net/dpaa2: fix L4 packet type in slow parse path
* net/dpaa2: fix link after port stop/start
* net/dpaa2: fix MAC stats DMA alloc per xstats call
* net/dpaa2: fix queue block memory leak on port close
* net/dpaa2: fix resource leak on soft parser failure
* net/dpaa2: fix Rx error queue memory leaks
* net/dpaa2: fix software taildrop buffer access
* net/dpaa2: fix spurious VLAN insertion on non-VLAN packet
* net/dpaa2: warn on Rx descriptor limit in high perf mode
* net/e1000: fix allocation of context desc for launch time
* net/e1000: fix igc launch time calculation
* net/e1000: fix variable shadowing
* net/e1000: use device timestamp for clock read in igc
* net: fix packet type for stacked VLAN
* net: fix variable shadowing
* net/hns3: fix outer UDP checksum with simple BD
* net/i40e: fix IPv6 GTPU handling
* net/i40e: fix QinQ stripping
* net/i40e: fix raw flow item validation
* net/i40e: fix secondary process Rx path selection
* net/i40e: fix unused variable
* net/i40e: fix variable shadowing
* net/i40e: move filter config to flow create
* net/i40e: validate raw flow items before dereferencing
* net/iavf: fix deletion of primary MAC address
* net/iavf: fix IPv4 flow subscription
* net/iavf: fix memory leak on egress IPsec flows
* net/iavf: fix memory leak on uninit
* net/iavf: fix order of tags for AVX512 Tx QinQ offload
* net/iavf: fix reported max Tx and Rx queues
* net/iavf: fix secondary process Rx path selection
* net/iavf: fix struct size in IPsec status get
* net/iavf: negotiate PTP before reporting Rx timestamping
* net/ice/base: disable MSVC warning
* net/ice/base: fix adjust timer programming for E830
* net/ice/base: fix double HW reinitialization
* net/ice/base: fix integer types in comparisons
* net/ice: check null scheduler root node
* net/ice: fix memory leak in DCF QoS bandwidth config
* net/ice: fix memory leak in FDIR flow parsing
* net/ice: fix priority mode printing in Tx scheduler dump
* net/ice: fix RSS LUT access when using global LUT
* net/ice: fix secondary process Rx path selection
* net/ice: fix variable shadowing
* net/ice: re-enable strict priority on non-root levels
* net/idpf: fix typo in CQ scan threshold macro name
* net/intel: fix comma operator warnings
* net/intel: fix memory leak on Tx queue setup failure
* net/intel: prevent selection of a null Rx burst function
* net/intel: update key length when getting RSS key
* net/ixgbe: add missing E610 MAC type checks
* net/ixgbe: fix memory leak in security flows
* net/ixgbe: fix pointer handling in IPsec
* net/ixgbe: fix potential null dereference in IPsec flow
* net/ixgbe: fix potential null dereference with IPsec config
* net/ixgbe: fix variable shadowing
* net/mana: fix CQE suppression handling on error completions
* net/mana: fix fast-path ops setup in secondary process
* net/mana: fix PD resource leak on device close
* net/memif: fix descriptor Tx flags corruption
* net/memif: fix multi-segment Rx corruption
* net/mlx4: fix fast-path ops setup in secondary process
* net/mlx5: add RSS TIR registration API
* net/mlx5: allow MTU mismatch for running shared queue
* net/mlx5: check DevX disconnect/error interrupt events
* net/mlx5: fix bonding check
* net/mlx5: fix build with LTO
* net/mlx5: fix counter leak in hairpin queue setup
* net/mlx5: fix counters resource leak
* net/mlx5: fix counter truncation in queue counter read
* net/mlx5: fix default flow engine on Windows
* net/mlx5: fix default memzone requirements in HWS
* net/mlx5: fix fast-path ops setup in secondary process
* net/mlx5: fix flex item capability check
* net/mlx5: fix flex parser creation length attribute
* net/mlx5: fix flow devargs handling for future HW
* net/mlx5: fix flow mark reading after reconfigure
* net/mlx5: fix flow metadata between E-Switch and VM
* net/mlx5: fix flow metadata sharing with E-Switch and VM
* net/mlx5: fix heap buffer overflow in sample group match
* net/mlx5: fix HW flow counter query
* net/mlx5: fix internal HWS pattern template creation
* net/mlx5: fix IP tunnel detection with HWS
* net/mlx5: fix IPv6 SRH flex node header length
* net/mlx5: fix IPv6 SRH flex parser initialization
* net/mlx5: fix job leak on indirect meter creation failure
* net/mlx5: fix memory leak after device spawn failure
* net/mlx5: fix meter ASO action leak on release to pool
* net/mlx5: fix MPESW PF probe for any number of ports
* net/mlx5: fix NAT64 HW registers calculation
* net/mlx5: fix null dereference in Tx queue start
* net/mlx5: fix port down in link detection failure
* net/mlx5: fix probing to allow BlueField Socket Direct
* net/mlx5: fix redundant control rules in promiscuous mode
* net/mlx5: fix send skew settings when using wait on time
* net/mlx5: fix shared Rx queue limitations
* net/mlx5: fix use-after-free in ASO management init
* net/mlx5: fix VLAN strip info for CQE compression
* net/mlx5: fix VXLAN and NVGRE encap in async flow API
* net/mlx5/hws: fix null dereference in rule skip
* net/mlx5/hws: fix stack alignment for ASan compatibility
* net/mlx5: report share group and queue ID
* net/mlx5/windows: fix MAC address ownership tracking
* net/mvpp2: fix variable shadowing
* net/nbl: check ioctl returns
* net/nbl: checks for unsupported UIO drivers
* net/nbl: fix hardware stats interrupt nesting
* net/nbl: fix integer overflow
* net/nbl: fix mbuf double-free in queue cleanup
* net/nbl: fix mbuf headroom usage in packet Tx
* net/nbl: fix memzone leak on queue release
* net/nbl: fix null pointer dereference
* net/nbl: fix queue stats on restart
* net/nbl: improve mailbox exception handling
* net/netvsc: fix devargs memory leak on hotplug
* net/netvsc: fix double-free of primary Rx queue on uninit
* net/netvsc: fix event callback leak on Rx filter failure
* net/netvsc: fix race conditions on VF add/remove events
* net/netvsc: fix resource leak on init failure
* net/netvsc: fix resource leaks on MTU change
* net/netvsc: fix subchannel leak on device removal
* net/netvsc: support multi-process VF device removal
* net/netvsc: switch data path to synthetic on device stop
* net/nfb: fix bad pointer access in queue stats
* net/nfb: fix resources release
* net/nfb: stop only started queues in fail path
* net/nfb: use constant values for max Rx/Tx queues count
* net/nfb: use process private variable for internal
* net/null: fix Tx statistics accumulation
* net/r8169: ensure old mapping is used
* net/r8169: fix bitmask logic for RTL8127
* net/r8169: fix crash in RTL8168FP init
* net/r8169: fix RTL8168KB speed classification
* net/sfc: avoid speed reset when setting FEC in started state
* net/sfc: drop AUTO from FEC capabilities and fix comment
* net/sfc: fix reporting status of autonegotiation
* net/sfc: rework capability check that is done on FEC set
* net/tap: fix handling of queue stats
* net/tap: fix resource leaks in secondary process probe
* net/tap: fix resource leaks on creation failure
* net/tap: fix use-after-free on remote flow creation failure
* net/tap: free IPC reply buffer on queue count mismatch
* net/tap: free remote flow when implicit rule already exists
* net/tap: remove log when running without multiprocess
* net/tap: use correct length for interface names
* pcapng: chain additional mbuf when comment exceeds tailroom
* pcapng: document return values
* pcapng: fix variable shadowing
* pcapng: use malloc instead of fixed buffer size
* pdcp: add digest physical address
* pipeline: fix variable shadowing
* power: fix variable shadowing
* rcu: fix build with MSVC
* table: fix variable shadowing
* telemetry: fix adding dict in container array
* test: add file-prefix for all fast-tests on Linux
* test: add pause to synchronization spinloops
* test/atomic: scale test based on core count
* test/bpf: fix error handling in ELF load tests
* test/bpf: fix payload size for Rx/Tx load tests
* test/bpf: fix unsupported instructions in ELF load test
* test/cfgfile: avoid temp filename truncation
* test/crypto: check for vdev args overflow
* test/crypto: fix mbuf segment number
* test/crypto: fix RSA sign data length
* test: fix dependencies on net null driver
* test/latency: relax check on zero latency
* test/mcslock: scale test based on core count
* test/memcpy: reduce alignment offset coverage
* test/pcapng: fix for Windows
* test/pcapng: skip test if null driver missing
* test/red: fix some undefined behaviour
* test/security: skip inline protocol test if no HW support
* test/stack: scale test based on core count
* test/table: avoid input line overflow
* test/timer: fix hang on secondary process failure
* test/timer: replace volatile with C11 atomics
* test/timer: scale test based on core count
* test/trace: fix parallel execution with traces enabled
* usertools/pmdinfo: fix search for PMD info string
* version: 25.11.1-rc1
* vhost: fix descriptor chain bounds check in control queue
* vhost: fix mmap error check in VDUSE IOTLB miss handler
* vhost: fix resource leak on driver registration failure
* vhost: fix use-after-free fdset during shutdown
* vhost: fix use-after-free race during cleanup
* vhost: fix virtqueue array size for control queue

25.11.1 Validation
~~~~~~~~~~~~~~~~~~

* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2026-April/057349.html>`__

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Buffer Split
      * Tx scheduling

   * Build tests

   * ConnectX-7

      * Ubuntu 24.04, DOCA-Host 3.3.0, fw 28.48.1000

   * BlueField-3 DPU

      * Ubuntu 24.04, DOCA SW 3.3.0, fw 24.48.1000


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2026-April/057351.html>`__

   * Compile testing

      * Ubuntu 25.04, Ubuntu 24.04.3, RHEL 10, RHEL 9.6
      * Fedora 43, FreeBSD 15, SUSE 16
      * OpenAnolis 8.10, OpenEuler 24.04-SP2, AzureLinux 3.0

   * Functional testing

      * PF/VF (i40e, ixgbe)
      * PF/VF (ice)
      * IPsec
      * Virtio
      * Cryptodev
      * DLB
      * AF_XDP, Power, CBDMA, DSA

   * Performance testing

      * Throughput performance
      * Cryptodev latency
      * PF/VF NIC single core/NIC performance
      * XXV710/E810 NIC Performance


* `IBM(R) Testing <https://mails.dpdk.org/archives/stable/2026-April/057356.html>`__

   * Build testing

      * Fedora 40, 41, 42 and 43 container images for ppc64le

   * Platform

      * IBM Power11 (9105-22A)
      * RHEL 10.1
      * Kernel 6.12.0-124.39.1.el10_1.ppc64le
      * GCC 14.2.1
      * ConnectX-7 100 GbE, fw 28.47.1088, OFED 26.01-1.0.0

   * Functional testing

      * Basic PF on Mellanox

   * Performance testing

      * TestPMD throughput single core tests

25.11.2 Release Notes
---------------------

This release is address a regression in DPDK v25.11.1 as reported
at https://bugs.dpdk.org/show_bug.cgi?id=1941.

25.11.2 Fixes
~~~~~~~~~~~~~

* Revert "net: fix packet type for stacked VLAN"

25.11.2 Validation
~~~~~~~~~~~~~~~~~~

* Github Actions, UNH CI, OBS CI
