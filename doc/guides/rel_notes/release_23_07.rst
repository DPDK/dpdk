.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2023 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 23.07
==================

New Features
------------

* **Added AMD CDX bus support.**

  CDX bus driver has been added to support the AMD CDX bus
  which operates on FPGA based CDX devices.
  The CDX devices are memory mapped on system buses for embedded CPUs.

* **Added MMIO read and write API to PCI bus.**

  Introduced ``rte_pci_mmio_read()`` and ``rte_pci_mmio_write()`` APIs
  to PCI bus so that PCI drivers can access PCI memory resources
  when they are not mapped to process address spaces.

* **Added ethdev Rx/Tx queue ID check API.**

  Added ethdev Rx/Tx queue ID check API.
  If the queue has been set up it is considered valid.

* **Added LLRS FEC mode in ethdev.**

  Added LLRS algorithm to Forward Error Correction (FEC) modes.

* **Added flow matching of Tx queue.**

  Added ``RTE_FLOW_ITEM_TYPE_TX_QUEUE`` rte_flow pattern
  to match the Tx queue of the sent packet.

* **Added flow matching of Infiniband BTH.**

  Added ``RTE_FLOW_ITEM_TYPE_IB_BTH`` to match Infiniband BTH fields.

* **Added actions to push or remove IPv6 extension.**

  Added ``RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH`` and ``RTE_FLOW_ACTION_TYPE_IPV6_EXT_PUSH``
  to push or remove the specific IPv6 extension into or from the packets.
  Push always puts the new extension as the last one due to the next header awareness.

* **Added indirect list flow action.**

  Added API to manage (create, destroy, update) a list of indirect actions.

* **Added flow rule update.**

  * Added API for updating the action list in an already existing rule.
    Introduced both ``rte_flow_actions_update()`` and
    ``rte_flow_async_actions_update()`` functions.

* **Added vhost callback API for interrupt handling.**

  Introduced a new callback, ``guest_notify`` that can be used to handle
  the interrupt kick outside of the datapath fast path.
  In addition, a new API, ``rte_vhost_notify_guest()``,
  was added to raise the interrupt outside of the fast path.

* **Added vhost API to set maximum queue pairs supported.**

  Introduced ``rte_vhost_driver_set_max_queue_num()`` to be able to limit
  the maximum number of supported queue pairs, required for VDUSE support.

* **Added VDUSE support into vhost library.**

  VDUSE aims at implementing vDPA devices in userspace.
  It can be used as an alternative to Vhost-user when using Vhost-vDPA,
  but it also enables providing a virtio-net netdev to the host
  when using Virtio-vDPA driver.

  A limitation in this release is the lack of reconnection support.
  While VDUSE support is already available in the upstream kernel,
  a couple of patches are required to support network device types,
  which are being upstreamed:
  https://lore.kernel.org/all/20230419134329.346825-1-maxime.coquelin@redhat.com/

* **Updated Google GVE net driver.**

  * Added DQO queue descriptor format support.

* **Updated Intel e1000 driver.**

  * Added support for new I219 devices.

* **Updated Intel igc driver.**

  * Added support for I225-LMVP.

* **Updated Intel ice driver.**

  * Added support for link speed change.
  * Added support for double VLAN.
  * Added support for UDP fragmentation offload.

* **Updated Intel iavf driver.**

  * Added new AVX2 Rx/Tx to use HW offload features.
  * Added support for Rx timestamp offload on vector paths.
  * Added support for UDP fragmentation offload.

* **Updated Intel idpf driver.**

  * Added support for Intel IPU E2100 VF.

* **Updated Intel cpfl driver.**

  * Added support for hairpin queue.

* **Updated Marvell cnxk ethdev driver.**

  * Added support for reassembly of multi-segment packets.
  * Extended ``RTE_FLOW_ACTION_TYPE_PORT_ID`` to redirect traffic across PF ports.
  * Added support for inline MACsec processing using security library
    for CN103 platform.

* **Updated NVIDIA mlx5 net driver.**

  * Added support for multi-packet receive queue (MPRQ) on Windows.
  * Added support for CQE compression on Windows.
  * Added support for enhanced multi-packet write on Windows.
  * Added support for InfiniBand BTH matching.
  * Added support for quota flow action and item.
  * Added support for flow rule update.

* **Updated Solarflare network PMD.**

  * Added support for configuring FEC mode, querying FEC capabilities and
    current FEC mode from a device.
  * Added partial support for transfer flow actions ``SET_IPV4_DST``, ``SET_TP_DST``,
    ``SET_IPV4_SRC`` and ``SET_TP_SRC`` on SN1000 SmartNICs.
  * Added support for transfer flow action ``INDIRECT`` with subtype ``COUNT``,
    for aggregated statistics.
  * Added support for keeping CRC.
  * Added VLAN stripping support on SN1000 SmartNICs.

* **Added vmxnet3 version 7 support.**

  Added support for vmxnet3 version 7 which includes support
  for uniform passthrough (UPT). The patches also add support
  for new capability registers, large passthrough BAR and some
  performance enhancements for UPT.

* **Added new algorithms to cryptodev.**

  * Added asymmetric algorithm ShangMi 2 (SM2) along with prime field curve support.
  * Added symmetric hash algorithm SM3-HMAC.
  * Added symmetric cipher algorithm ShangMi 4 (SM4) in CFB and OFB modes.

* **Updated Intel QuickAssist Technology (QAT) crypto driver.**

  * Added support for combined Cipher-CRC offload for DOCSIS for QAT GENs 2, 3 and 4.
  * Added support for SM3-HMAC algorithm for QAT GENs 3 and 4.

* **Updated Marvell cnxk crypto driver.**

  * Added support for PDCP chain in cn10k crypto driver.
  * Added support for SM3 hash operations.
  * Added support for SM4 operations in cn10k driver.
  * Added support for AES-CCM in cn9k and cn10k drivers.

* **Updated NVIDIA mlx5 crypto driver.**

  * Added support for AES-GCM crypto.

* **Updated OpenSSL crypto driver.**

  * Added SM2 algorithm support in asymmetric crypto operations.

* **Updated ipsec_mb crypto driver.**

  * Added Intel IPsec MB v1.4 library support for x86 platform.

* **Added PDCP Library.**

  Added an experimental library to provide PDCP UL and DL processing of packets.

  The library supports all PDCP algorithms
  and leverages lookaside crypto offloads to cryptodevs for crypto processing.
  PDCP features such as IV generation, sequence number handling, etc are supported.
  It is planned to add more features such as packet caching in future releases.

  See :doc:`../prog_guide/pdcp_lib` for more information.

* **Added TCP/IPv6 support in GRO library.**

  Enhanced the GRO library to support TCP packets over IPv6 network.

* **Added mcore dispatch model in graph library.**

  * Added set, get and validate model APIs to enhance graph framework
    to allow choice of different walk models.
  * Added mcore dispatch model to support cross-core dispatching mechanism.
  * Added a command option ``--model`` in l3fwd-graph example
    to choose RTC or mcore dispatch model.

* **Added DMA device performance test application.**

  Added an application to test the performance of DMA device and CPU.

  See the :doc:`../tools/dmaperf` for more details.


Removed Items
-------------

* Removed LiquidIO ethdev driver located at ``drivers/net/liquidio/``.


API Changes
-----------

* ethdev: Ensured all entries in MAC address list are unique.
  When setting a default MAC address with the function
  ``rte_eth_dev_default_mac_addr_set``,
  the default one needs to be removed by the user
  if it was already in the address list.


ABI Changes
-----------

* No ABI change that would break compatibility with 22.11.

* ethdev: In the experimental ``struct rte_flow_action_modify_data``:

  * ``level`` field was reduced to 8 bits.
  * ``tag_index`` field replaced ``level`` field in representing tag array for
    ``RTE_FLOW_FIELD_TAG`` type.


Known Issues
------------

* **Testpmd is not forwarding on queues individually stopped.**

  Testpmd forwards packets on started queues.
  If a queue explicitly stopped, and later port stopped and started again,
  the status of the previously stopped queue is not updated,
  so forwarding is not working on those queues.

  As a workaround start queues back explicitly, instead of port stop/start.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU D-1553N @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| D-1747NTE CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| D-2796NT CPU @ 2.00GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180 CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8380 CPU @ 2.30GHz
    * Genuine Intel\ |reg| 0000

  * OS:

    * CBL Mariner 2.0
    * Fedora 38
    * FreeBSD 13.2
    * OpenAnolis OS 8.8
    * Red Hat Enterprise Linux Server release 8.7
    * Red Hat Enterprise Linux Server release 9.0
    * SUSE Linux Enterprise Server 15 SP4
    * Ubuntu 22.04.2

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 4.30 0x80019dcc 1.3415.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version(out-tree): 1.12.4_dirty (ice)
      * Driver version(in-tree): 5.15.0-70-generic (Ubuntu22.04.2)/ 5.14.0-70.13.1.rt21.83.el9_0.x86_64 (RHEL9.0)/ 5.15.107-rt62 (Ubuntu22.04.2)(ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 4.30 0x80019dad 1.3415.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version(out-tree): 1.12.4_dirty (ice)
      * Driver version(in-tree): 5.15.55.1-1.cm2-5464b22cac7+ (CBL Mariner 2.0) (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| Ethernet Controller E810-XXV for SFP (2x25G)

      * Firmware version: 4.30 0x80019da5 1.3415.0
      * Device id (pf/vf): 8086:159b / 8086:1889
      * Driver version: 1.12.4_dirty (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0

    * Intel\ |reg| Ethernet Connection E823-C for QSFP

      * Firmware version: 3.32 0x8001a33d 1.3353.0
      * Device id (pf/vf): 8086:188b / 8086:1889
      * Driver version: 1.12.4_dirty (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| Ethernet Connection E823-L for QSFP

      * Firmware version: 3.32 0x8001a66d 1.3353.0
      * Device id (pf/vf): 8086:124c / 8086:1889
      * Driver version: 1.12.4_dirty (ice)
      * OS Default DDP: 1.3.35.0
      * COMMS DDP: 1.3.45.0
      * Wireless Edge DDP: 1.3.13.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(out-tree): 5.18.13 (ixgbe)
      * Driver version(in-tree): 5.15.0-70-generic (Ubuntu22.04.2)/ 5.14.0-70.13.1.el9_0.x86_64 (RHEL9.0)(ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * Firmware version: 9.20 0x8000d8bd 1.3353.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version(out-tree): 2.23.5 (Fedora 38)(i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (2x10G)

      * Firmware version: 6.20 0x80003d82 1.3353.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version(out-tree): 2.22.20 (i40e)
      * Driver version(in-tree): 5.14.0-70.13.1.el9_0.x86_64 (RHEL9.0)(i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T

      * Firmware version: 6.20 0x80003d3e 1.2935.0
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version(out-tree): 2.22.20 (i40e)
      * Driver version(in-tree): 5.14.0-70.13.1.el9_0.x86_64 (RHEL9.0) (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * Firmware version: 9.20 0x8000d89c 1.3353.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version(out-tree): 2.22.20 (i40e)
      * Driver version(in-tree): 5.15.0-71-generic (Ubuntu22.04.2)/5.14.0-70.13.1.el9_0.x86_64 (RHEL9.0)(i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * Firmware version(PF): 9.20 0x8000d893 1.3353.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version(out-tree): 2.22.20 /2.23.5 (Fedora 38)(i40e)

    * Intel\ |reg| Ethernet Controller I225-LM

      * Firmware version: 1.3, 0x800000c9
      * Device id (pf): 8086:15f2
      * Driver version(in-tree): 5.15.0-70-generic (Ubuntu22.04.2)(igc)

    * Intel\ |reg| Ethernet Controller I226-LM

      * Firmware version: 2.14, 0x8000028c
      * Device id (pf): 8086:125b
      * Driver version(in-tree): 5.15.0-71-generic (Ubuntu22.04.2)(igc)

    * Intel Corporation Ethernet Connection (16) I219-V

      * Firmware version: 0.6-4
      * Device id (pf): 8086:1a1f
      * Driver version(in-tree): 5.15.113-rt64 (Ubuntu22.04.2)(e1000)

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

    * Red Hat Enterprise Linux release 9.1 (Plow)
    * Red Hat Enterprise Linux release 8.6 (Ootpa)
    * Red Hat Enterprise Linux release 8.4 (Ootpa)
    * Red Hat Enterprise Linux Server release 7.9 (Maipo)
    * Red Hat Enterprise Linux Server release 7.6 (Maipo)
    * Ubuntu 22.04
    * Ubuntu 20.04
    * SUSE Enterprise Linux 15 SP2

  * OFED:

    * MLNX_OFED 23.04-1.1.3.0 and above

  * upstream kernel:

    * Linux 6.4.0 and above

  * rdma-core:

    * rdma-core-46.0 and above

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCC_Ax (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * NVIDIA\ |reg| ConnectX\ |reg|-3 Pro 40G MCX354A-FCCT (2x40G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1007
      * Firmware version: 2.42.5000

    * NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 25G MCX4121A-ACAT (2x25G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.37.1014 and above

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.37.1014 and above

* NVIDIA\ |reg| BlueField\ |reg| SmartNIC

  * NVIDIA\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.37.1300 and above

  * NVIDIA\ |reg| BlueField\ |reg|-3 P-Series DPU MT41692 - 900-9D3B6-00CV-AAB (2x200G)

    * Host interface: PCI Express 5.0 x16
    * Device ID: 15b3:a2dc
    * Firmware version: 32.37.1306 and above

  * Embedded software:

    * Ubuntu 22.04
    * MLNX_OFED 23.04-0.5.3.0 and above
    * DOCA_2.0.2 BSP_4.0.3_Ubuntu_22.04-8.23-04
    * DPDK application running on ARM cores

* IBM Power 9 platforms with NVIDIA\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202)

  * OS:

    * Ubuntu 20.04

  * NICs:

    * NVIDIA\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.37.1014

    * NVIDIA\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.37.1014

    * NVIDIA\ |reg| ConnectX\ |reg|-7 200G CX713106AE-HEA_QP1_Ax (2x200G)

      * Host interface: PCI Express 5.0 x16
      * Device ID: 15b3:1021
      * Firmware version: 28.37.1014 and above

  * OFED:

    * MLNX_OFED 23.04-1.1.3.0
