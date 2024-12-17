.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 21.11
==================

New Features
------------

* **Enabled new devargs parser.**

  * Enabled devargs syntax:
    ``bus=X,paramX=x/class=Y,paramY=y/driver=Z,paramZ=z``.
  * Added bus-level parsing of the devargs syntax.
  * Kept compatibility with the legacy syntax as parsing fallback.

* **Updated EAL hugetlbfs mount handling for Linux.**

  * Modified EAL to allow ``--huge-dir`` option to specify a sub-directory
    within a hugetlbfs mountpoint.

* **Added dmadev library.**

  * Added a DMA device framework for management and provision of
    hardware and software DMA devices.
  * Added generic API which support a number of different DMA
    operations.
  * Added multi-process support.

* **Updated default KNI behavior on net devices control callbacks.**

  Updated KNI net devices control callbacks to run with ``rtnl`` kernel lock
  held by default. A newly added ``enable_bifurcated`` KNI kernel module
  parameter can be used to run callbacks with ``rtnl`` lock released.

* **Added HiSilicon DMA driver.**

  The HiSilicon DMA driver provides device drivers for the Kunpeng's DMA devices.
  This device driver can be used through the generic dmadev API.

* **Added IDXD dmadev driver implementation.**

  The IDXD dmadev driver provides device drivers for the Intel DSA devices.
  This device driver can be used through the generic dmadev API.

* **Added IOAT dmadev driver implementation.**

  The Intel I/O Acceleration Technology (IOAT) dmadev driver provides a device
  driver for Intel IOAT devices such as Crystal Beach DMA (CBDMA) on Ice Lake,
  Skylake and Broadwell. This device driver can be used through the generic dmadev API.

* **Added Marvell CNXK DMA driver.**

  Added dmadev driver for the DPI DMA hardware accelerator
  of Marvell OCTEONTX2 and OCTEONTX3 family of SoCs.

* **Added NXP DPAA DMA driver.**

  Added a new dmadev driver for the NXP DPAA platform.

* **Added support to get all MAC addresses of a device.**

  Added ``rte_eth_macaddrs_get`` to allow a user to retrieve all Ethernet
  addresses assigned to a given Ethernet port.

* **Introduced GPU device class.**

  Introduced the GPU device class with initial features:

  * Device information.
  * Memory management.
  * Communication flag and list.

* **Added NVIDIA GPU driver implemented with CUDA library.**

  Added NVIDIA GPU driver implemented with CUDA library under the new
  GPU device interface.

* **Added new RSS offload types for IPv4/L4 checksum in RSS flow.**

  Added macros ``ETH_RSS_IPV4_CHKSUM`` and ``ETH_RSS_L4_CHKSUM``. The IPv4 and
  TCP/UDP/SCTP header checksum field can now be used as input set for RSS.

* **Added L2TPv2 and PPP protocol support in flow API.**

  Added flow pattern items and header formats for the L2TPv2 and PPP protocols.

* **Added flow flex item.**

  The configurable flow flex item provides the capability to introduce
  an arbitrary user-specified network protocol header,
  configure the hardware accordingly, and perform match on this header
  with desired patterns and masks.

* **Added ethdev support to control delivery of Rx metadata from the HW to the PMD.**

  A new API, ``rte_eth_rx_metadata_negotiate()``, was added.
  The following parts of Rx metadata were defined:

  * ``RTE_ETH_RX_METADATA_USER_FLAG``
  * ``RTE_ETH_RX_METADATA_USER_MARK``
  * ``RTE_ETH_RX_METADATA_TUNNEL_ID``

* **Added an API to get a proxy port to manage "transfer" flows.**

  A new API, ``rte_flow_pick_transfer_proxy()``, was added.

* **Added ethdev shared Rx queue support.**

  * Added new device capability flag and Rx domain field to switch info.
  * Added share group and share queue ID to Rx queue configuration.
  * Added testpmd support and dedicated forwarding engine.

* **Updated af_packet ethdev driver.**

  * The default VLAN strip behavior has changed. The VLAN tag won't be stripped
    unless ``DEV_RX_OFFLOAD_VLAN_STRIP`` offload is enabled.

* **Added API to get device configuration in ethdev.**

  Added an ethdev API which can help users get device configuration.

* **Updated AF_XDP PMD.**

  * Disabled secondary process support due to insufficient state shared
    between processes which causes a crash. This will be fixed/re-enabled
    in the next release.

* **Updated Amazon ENA PMD.**

  Updated the Amazon ENA PMD. The new driver version (v2.5.0) introduced
  bug fixes and improvements, including:

  * Support for the ``tx_free_thresh`` and ``rx_free_thresh`` configuration parameters.
  * NUMA aware allocations for the queue helper structures.
  * A Watchdog feature which is checking for missing Tx completions.

* **Updated Broadcom bnxt PMD.**

  * Added flow offload support for Thor.
  * Added TruFlow and AFM SRAM partitioning support.
  * Implemented support for tunnel offload.
  * Updated HWRM API to version 1.10.2.68.
  * Added NAT support for destination IP and port combination.
  * Added support for socket redirection.
  * Added wildcard match support for ingress flows.
  * Added support for inner IP header for GRE tunnel flows.
  * Updated support for RSS action in flow rules.
  * Removed devargs option for stats accumulation.

* **Updated Cisco enic driver.**

  * Added rte_flow support for matching GTP, GTP-C and GTP-U headers.

* **Updated Intel e1000 emulated driver.**

  * Added Intel e1000 support on Windows.

* **Updated Intel iavf driver.**

  * Added Intel iavf support on Windows.
  * Added IPv4 and L4 (TCP/UDP/SCTP) checksum hash support in RSS flow.
  * Added PPPoL2TPv2oUDP RSS hash based on inner IP address and TCP/UDP port.
  * Added Intel iavf inline crypto support.

* **Updated Intel ice driver.**

  * Added protocol agnostic flow offloading support in Flow Director.
  * Added protocol agnostic flow offloading support in RSS hash.
  * Added 1PPS out support via devargs.
  * Added IPv4 and L4 (TCP/UDP/SCTP) checksum hash support in RSS flow.
  * Added ``DEV_RX_OFFLOAD_TIMESTAMP`` support.
  * Added timesync API support under scalar path.
  * Added DCF reset API support.

* **Updated Intel ixgbe driver.**

  * Added Intel ixgbe support on Windows.

* **Updated Marvell cnxk ethdev driver.**

  * Added rte_flow support for dual VLAN insert and strip actions.
  * Added rte_tm support.
  * Added support for Inline IPsec for CN9K event mode and CN10K
    poll mode and event mode.
  * Added support for ingress meter for CN10K platform.

* **Updated Mellanox mlx5 driver.**

  Updated the Mellanox mlx5 driver with new features and improvements, including:

  * Added implicit mempool registration to avoid data path hiccups (opt-out).
  * Added delay drop support for Rx queues.
  * Added NIC offloads for the PMD on Windows (TSO, VLAN strip, CRC keep).
  * Added socket direct mode bonding support.

* **Added NXP ENETFEC PMD [EXPERIMENTAL].**

  Added the new ENETFEC driver for the NXP IMX8MMEVK platform. See the
  :doc:`../nics/enetfec` NIC driver guide for more details on this new driver.

* **Updated Solarflare network PMD.**

  Updated the Solarflare ``sfc_efx`` driver with changes including:

  * Added port representors support on SN1000 SmartNICs
  * Added flow API transfer proxy support
  * Added SN1000 virtual functions (VF) support
  * Added support for flow counters without service cores
  * Added support for regioned DMA mapping required on SN1022 SoC

* **Added power monitor API in vhost library.**

  Added an API to support power monitor in vhost library.

* **Updated vhost PMD.**

  Add power monitor support in vhost PMD.

* **Updated virtio PMD.**

  * Initial support for RSS receive mode has been added to the Virtio PMD,
    with the capability for the application to configure the hash key,
    the RETA and the hash types.
    Virtio hash reporting is yet to be added.
  * Added power monitor support in virtio PMD.

* **Updated Wangxun ngbe driver.**

  * Added offloads and packet type on RxTx.
  * Added VLAN and MAC filters.
  * Added device basic statistics and extended stats.
  * Added multi-queue and RSS.
  * Added SRIOV.
  * Added flow control.
  * Added IEEE 1588.

* **Added new vDPA PMD based on Xilinx devices.**

  Added a new Xilinx vDPA  (``sfc_vdpa``) PMD.
  See the :doc:`../vdpadevs/sfc` guide for more details on this driver.

* **Added telemetry callbacks to the cryptodev library.**

  Added telemetry callback functions which allow a list of crypto devices,
  stats for a crypto device, and other device information to be queried.
  Also added callback to get cryptodev capabilities.

* **Added telemetry to security library.**

  Added telemetry callback functions to query security capabilities of
  crypto device.

* **Updated Marvell cnxk crypto PMD.**

  * Added AES-CBC SHA1-HMAC support in lookaside protocol (IPsec) for CN10K.
  * Added Transport mode support in lookaside protocol (IPsec) for CN10K.
  * Added UDP encapsulation support in lookaside protocol (IPsec) for CN10K.
  * Added support for lookaside protocol (IPsec) offload for CN9K.
  * Added support for ZUC algorithm with 256-bit key length for CN10K.
  * Added support for CN98xx dual block.
  * Added inner checksum support in lookaside protocol (IPsec) for CN10K.
  * Added AES-CBC NULL auth support in lookaside protocol (IPsec) for CN10K.
  * Added ESN and anti-replay support in lookaside protocol (IPsec) for CN9K.

* **Added support for event crypto adapter on Marvell CN10K and CN9K.**

  * Added event crypto adapter ``OP_FORWARD`` mode support.

* **Updated Mellanox mlx5 crypto driver.**

  * Added Windows support.
  * Added support for BlueField 2 and ConnectX-6 Dx.

* **Updated NXP dpaa_sec crypto PMD.**

  * Added DES-CBC, AES-XCBC-MAC, AES-CMAC and non-HMAC algorithm support.
  * Added PDCP short MAC-I support.
  * Added raw vector datapath API support.

* **Updated NXP dpaa2_sec crypto PMD.**

  * Added PDCP short MAC-I support.
  * Added raw vector datapath API support.

* **Added framework for consolidation of IPsec_MB dependent SW Crypto PMDs.**

  * The IPsec_MB framework was added to share common code between Intel
    SW Crypto PMDs that depend on the intel-ipsec-mb library.
  * Multiprocess support was added for the consolidated PMDs
    which requires v1.1 of the intel-ipsec-mb library.
  * The following PMDs were moved into a single source folder
    while their usage and EAL options remain unchanged.
    * AESNI_MB PMD.
    * AESNI_GCM PMD.
    * KASUMI PMD.
    * SNOW3G PMD.
    * ZUC PMD.
    * CHACHA20_POLY1305 - a new PMD.

* **Updated the aesni_mb crypto PMD.**

  * Added support for ZUC-EEA3-256 and ZUC-EIA3-256.

* **Added digest appended ops support for Snow3G PMD.**

  * Added support for out-of-place auth-cipher operations that encrypt
    the digest along with the rest of the raw data.
  * Added support for partially encrypted digest when using auth-cipher
    operations.

* **Updated the ACC100 bbdev PMD.**

  Added support for more comprehensive CRC options.

* **Updated the turbo_sw bbdev PMD.**

  Added support for more comprehensive CRC options.

* **Added NXP LA12xx baseband PMD.**

  * Added a new baseband PMD for NXP LA12xx Software defined radio.
  * See the :doc:`../bbdevs/la12xx` for more details.

* **Updated Mellanox compress driver.**

  * Added devargs option to allow manual setting of Huffman block size.

* **Updated Mellanox regex driver.**

  * Added support for new ROF file format.

* **Updated IPsec library.**

  * Added support for more AEAD algorithms AES_CCM, CHACHA20_POLY1305
    and AES_GMAC.
  * Added support for NAT-T / UDP encapsulated ESP.
  * Added support for SA telemetry.
  * Added support for setting a non default starting ESN value.
  * Added support for TSO in inline crypto mode.

* **Added optimized Toeplitz hash implementation.**

  Added optimized Toeplitz hash implementation using Galois Fields New Instructions.

* **Added multi-process support for testpmd.**

  Added command-line options to specify total number of processes and
  current process ID. Each process owns a subset of Rx and Tx queues.

* **Updated test-crypto-perf application with new cases.**

  * Added support for asymmetric crypto throughput performance measurement.
    Only modex is supported for now.
  * Added support for lookaside IPsec protocol offload throughput measurement.

* **Added lookaside protocol (IPsec) tests in dpdk-test.**

  * Added known vector tests (AES-GCM 128, 192, 256).
  * Added tests to verify error reporting with ICV corruption.
  * Added tests to verify IV generation.
  * Added tests to verify UDP encapsulation.
  * Added tests to verify UDP encapsulation ports.
  * Added tests to validate packets soft expiry.
  * Added tests to validate packets hard expiry.
  * Added tests to verify tunnel header verification in IPsec inbound.
  * Added tests to verify inner checksum.
  * Added tests for CHACHA20_POLY1305 PMD, including a new testcase for SGL OOP.

* **Updated l3fwd sample application.**

  * Increased number of routes to 16 for all lookup modes (LPM, EM and FIB).
    This helps in validating SoC with many Ethernet devices.
  * Updated EM mode to use RFC2544 reserved IP address space with RFC863
    UDP discard protocol.

* **Updated IPsec Security Gateway sample application with new features.**

  * Added support for TSO (only for inline crypto TCP packets).
  * Added support for telemetry.
  * Added support for more AEAD algorithms: AES-GMAC, AES_CTR, AES_XCBC_MAC,
    AES_CCM, CHACHA20_POLY1305
  * Added support for event vectors for inline protocol offload mode.

* **Revised packet capture framework.**

  * New dpdk-dumpcap program that has most of the features
    of the wireshark dumpcap utility including:
    capture of multiple interfaces, filtering,
    and stopping after number of bytes, packets.
  * New library for writing pcapng packet capture files.
  * Enhancements to the pdump library to support:
    * Packet filter with BPF.
    * Pcapng format with timestamps and meta-data.
    * Fixes packet capture with stripped VLAN tags.

* **Added ASan support.**

  Added ASan/AddressSanitizer support. `AddressSanitizer
  <https://github.com/google/sanitizers/wiki/AddressSanitizer>`_
  is a widely-used debugging tool to detect memory access errors.
  It helps to detect issues like use-after-free, various kinds of buffer
  overruns in C/C++ programs, and other similar errors, as well as
  printing out detailed debug information whenever an error is detected.


Removed Items
-------------

* eal: Removed the deprecated function ``rte_get_master_lcore()``
  and the iterator macro ``RTE_LCORE_FOREACH_SLAVE``.

* eal: The old API arguments that were deprecated for
  blacklist/whitelist are removed. Users must use the new
  block/allow list arguments.

* mbuf: Removed offload flag ``PKT_RX_EIP_CKSUM_BAD``.
  The ``PKT_RX_OUTER_IP_CKSUM_BAD`` flag should be used as a replacement.

* ethdev: Removed the port mirroring API. A more fine-grain flow API
  action ``RTE_FLOW_ACTION_TYPE_SAMPLE`` should be used instead.
  The structures ``rte_eth_mirror_conf`` and ``rte_eth_vlan_mirror`` and
  the functions ``rte_eth_mirror_rule_set`` and
  ``rte_eth_mirror_rule_reset`` along with the associated macros
  ``ETH_MIRROR_*`` are removed.

* ethdev: Removed the ``rte_eth_rx_descriptor_done()`` API function and its
  driver callback. It is replaced by the more complete function
  ``rte_eth_rx_descriptor_status()``.

* ethdev: Removed deprecated ``shared`` attribute of the
  ``struct rte_flow_action_count``. Shared counters should be managed
  using indirect actions API (``rte_flow_action_handle_create`` etc).

* i40e: Removed i40evf driver.
  iavf already became the default VF driver for i40e devices,
  so there is no need to maintain i40evf.


API Changes
-----------

* eal: The lcore state ``FINISHED`` is removed from
  the ``enum rte_lcore_state_t``.
  The lcore state ``WAIT`` is enough to represent the same state.

* eal: Made ``rte_intr_handle`` structure definition hidden.

* kvargs: The experimental function ``rte_kvargs_strcmp()`` has been
  removed. Its usages have been replaced by a new function
  ``rte_kvargs_get_with_value()``.

* cmdline: ``cmdline_stdin_exit()`` now frees the ``cmdline`` structure.
  Calls to ``cmdline_free()`` after it need to be deleted from applications.

* cmdline: Made ``cmdline`` structure definition hidden on Linux and FreeBSD.

* cmdline: Made ``rdline`` structure definition hidden. Functions are added
  to dynamically allocate and free it, and to access user data in callbacks.

* mempool: Added ``RTE_MEMPOOL_F_NON_IO`` flag to give a hint to DPDK components
  that objects from this pool will not be used for device IO (e.g. DMA).

* mempool: The mempool flags ``MEMPOOL_F_*`` will be deprecated in the future.
  Newly added flags with ``RTE_MEMPOOL_F_`` prefix should be used instead.

* mempool: Helper macro ``MEMPOOL_HEADER_SIZE()`` is deprecated.
  The replacement macro ``RTE_MEMPOOL_HEADER_SIZE()`` is internal only.

* mempool: Macro to register mempool driver ``MEMPOOL_REGISTER_OPS()`` is
  deprecated.  Use replacement ``RTE_MEMPOOL_REGISTER_OPS()``.

* mempool: The mempool API macros ``MEMPOOL_PG_*`` are deprecated and
  will be removed in DPDK 22.11.

* mbuf: The mbuf offload flags ``PKT_*`` are renamed as ``RTE_MBUF_F_*``. A
  compatibility layer will be kept until DPDK 22.11.
* net: Renamed ``s_addr`` and ``d_addr`` fields of ``rte_ether_hdr`` structure
  to ``src_addr`` and ``dst_addr``, respectively.

* net: Added ``version`` and ``ihl`` bit-fields to ``struct rte_ipv4_hdr``.
  Existing ``version_ihl`` field is kept for backward compatibility.

* ethdev: Added items and actions ``PORT_REPRESENTOR``, ``REPRESENTED_PORT`` to
  flow API.

* ethdev: Deprecated items and actions ``PF``, ``VF``, ``PHY_PORT``, ``PORT_ID``.
  Suggested items and actions ``PORT_REPRESENTOR``, ``REPRESENTED_PORT`` instead.

* ethdev: Deprecated the use of attributes ``ingress`` / ``egress`` combined
  with ``transfer``. See items ``PORT_REPRESENTOR``, ``REPRESENTED_PORT``.

* ethdev: ``rte_flow_action_modify_data`` structure updated, immediate data
  array is extended, data pointer field is explicitly added to union, the
  action behavior is defined in a more strict fashion and documentation updated.
  The immediate value behavior has been changed, the entire immediate field
  should be provided, and offset for immediate source bitfield is assigned
  from the destination one.

* vhost: ``rte_vdpa_register_device``, ``rte_vdpa_unregister_device``,
  ``rte_vhost_host_notifier_ctrl`` and ``rte_vdpa_relay_vring_used`` vDPA
  driver interface are marked as internal.

* cryptodev: The API ``rte_cryptodev_pmd_is_valid_dev()`` is modified to
  ``rte_cryptodev_is_valid_dev()`` as it can be used by the application as
  well as the PMD to check whether the device is valid or not.

* cryptodev: The ``rte_cryptodev_pmd.*`` files are renamed to ``cryptodev_pmd.*``
  since they are for drivers only and should be private to DPDK, and not
  installed for app use.

* cryptodev: A ``reserved`` byte from structure ``rte_crypto_op`` was
  renamed to ``aux_flags`` to indicate warnings and other information from
  the crypto/security operation. This field will be used to communicate
  events such as soft expiry with IPsec in lookaside mode.

* cryptodev: The field ``dataunit_len`` of the ``struct rte_crypto_cipher_xform``
  moved to the end of the structure and extended to ``uint32_t``.

* cryptodev: The structure ``rte_crypto_vec`` was updated to add ``tot_len``
  field to support total buffer length to facilitate protocol offload case.

* cryptodev: The structure ``rte_crypto_sym_vec`` was updated to add
  ``dest_sgl`` to support out of place processing.

* bbdev: Added device info related to data byte endianness processing.

* eventdev: Moved memory used by timer adapters to hugepage. This will prevent
  TLB misses if any and aligns to memory structure of other subsystems.

* fib: Added the ``rib_ext_sz`` field to ``rte_fib_conf`` and ``rte_fib6_conf``
  so that user can specify the size of the RIB extension inside the FIB.

* ip_frag: All macros updated to have ``RTE_IP_FRAG_`` prefix.
  Obsolete macros are kept for compatibility.
  DPDK components updated to use new names.
  Experimental function ``rte_frag_table_del_expired_entries()`` was renamed
  to ``rte_ip_frag_table_del_expired_entries()``
  to comply with other public API naming convention.


ABI Changes
-----------

* ethdev: All enums and macros updated to have ``RTE_ETH`` prefix and structures
  updated to have ``rte_eth`` prefix. DPDK components updated to use new names.

* ethdev: The input parameters for ``eth_rx_queue_count_t`` were changed.
  Instead of a pointer to ``rte_eth_dev`` and queue index, it now accepts a pointer
  to internal queue data as an input parameter. While this change is transparent
  to the user, it still counts as an ABI change, as ``eth_rx_queue_count_t``
  is used by the public inline function ``rte_eth_rx_queue_count``.

* ethdev: Made ``rte_eth_dev``, ``rte_eth_dev_data``, ``rte_eth_rxtx_callback``
  private data structures. ``rte_eth_devices[]`` can't be accessed directly
  by user any more. While it is an ABI breakage, this change is intended
  to be transparent for both users (no changes in user app is required) and
  PMD developers (no changes in PMD is required).

* vhost: rename ``struct vhost_device_ops`` to ``struct rte_vhost_device_ops``.

* cryptodev: Made ``rte_cryptodev``, ``rte_cryptodev_data`` private
  structures internal to DPDK. ``rte_cryptodevs`` can't be accessed directly
  by user any more. While it is an ABI breakage, this change is intended
  to be transparent for both users (no changes in user app is required) and
  PMD developers (no changes in PMD is required).

* security: ``rte_security_set_pkt_metadata`` and ``rte_security_get_userdata``
  routines used by inline outbound and inline inbound security processing were
  made inline and enhanced to do simple 64-bit set/get for PMDs that do not
  have much processing in PMD specific callbacks but just 64-bit set/get.
  This avoids a per packet function pointer jump overhead for such PMDs.

* security: A new option ``iv_gen_disable`` was added in structure
  ``rte_security_ipsec_sa_options`` to disable IV generation inside PMD,
  so that application can provide its own IV and test known test vectors.

* security: A new option ``tunnel_hdr_verify`` was added in structure
  ``rte_security_ipsec_sa_options`` to indicate whether outer header
  verification need to be done as part of inbound IPsec processing.

* security: A new option ``udp_ports_verify`` was added in structure
  ``rte_security_ipsec_sa_options`` to indicate whether UDP ports
  verification need to be done as part of inbound IPsec processing.

* security: A new structure ``rte_security_ipsec_lifetime`` was added to
  replace ``esn_soft_limit`` in IPsec configuration structure
  ``rte_security_ipsec_xform`` to allow applications to configure SA soft
  and hard expiry limits. Limits can be either in number of packets or bytes.

* security: The new options ``ip_csum_enable`` and ``l4_csum_enable`` were added
  in structure ``rte_security_ipsec_sa_options`` to indicate whether inner
  packet IPv4 header checksum and L4 checksum need to be offloaded to
  security device.

* security: A new structure ``esn`` was added in structure
  ``rte_security_ipsec_xform`` to set an initial ESN value. This permits
  applications to start from an arbitrary ESN value for debug and SA lifetime
  enforcement purposes.

* security: A new structure ``udp`` was added in structure
  ``rte_security_ipsec_xform`` to allow setting the source and destination ports
  for UDP encapsulated IPsec traffic.

* bbdev: Added capability related to more comprehensive CRC options,
  shifting values of the ``enum rte_bbdev_op_ldpcdec_flag_bitmasks``.

* eventdev: New variables ``rx_event_buf_count`` and ``rx_event_buf_size``
  were added in structure ``rte_event_eth_rx_adapter_stats`` to get additional
  status.

* eventdev: A new structure ``rte_event_fp_ops`` has been added which is now used
  by the fastpath inline functions. The structures ``rte_eventdev``,
  ``rte_eventdev_data`` have been made internal. ``rte_eventdevs[]`` can't be
  accessed directly by user any more. This change is transparent to both
  applications and PMDs.

* eventdev: Re-arranged fields in ``rte_event_timer`` to remove holes.

* ip_frag: Increased default value for config parameter
  ``RTE_LIBRTE_IP_FRAG_MAX_FRAG`` from ``4`` to ``8``.
  This parameter controls maximum number of fragments per packet
  in IP reassembly table. Increasing this value from ``4`` to ``8``
  will allow covering the common case with jumbo packet size of ``9000B``
  and fragments with default frame size ``(1500B)``.


Tested Platforms
----------------

* Intel\ |reg| platforms with Intel\ |reg| NICs combinations

  * CPU

    * Intel\ |reg| Atom\ |trade| CPU C3758 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2680 v2 @ 2.80GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v3 @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| CPU E5-2699 v4 @ 2.20GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6140M CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6139 CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6252N CPU @ 2.30GHz
    * Intel\ |reg| Xeon\ |reg| Gold 6348 CPU @ 2.60GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8180M CPU @ 2.50GHz
    * Intel\ |reg| Xeon\ |reg| Platinum 8280M CPU @ 2.70GHz

  * OS:

    * Fedora 34
    * OpenWRT 21.02.0
    * FreeBSD 13.0
    * Red Hat Enterprise Linux Server release 8.4
    * Suse 15 SP3
    * Ubuntu 20.04.3
    * Ubuntu 21.10

  * NICs:

    * Intel\ |reg| Ethernet Controller E810-C for SFP (4x25G)

      * Firmware version: 3.10 0x8000aa86 1.3100.0
      * Device id (pf/vf): 8086:1593 / 8086:1889
      * Driver version: 1.7.11_7_g444e5edb (ice)
      * OS Default DDP: 1.3.27.0
      * COMMS DDP: 1.3.31.0
      * Wireless Edge DDP: 1.3.7.0

    * Intel\ |reg| Ethernet Controller E810-C for QSFP (2x100G)

      * Firmware version: 3.10 0x8000aa66 1.3100.0
      * Device id (pf/vf): 8086:1592 / 8086:1889
      * Driver version: 1.7.11_7_g444e5edb (ice)
      * OS Default DDP: 1.3.27.0
      * COMMS DDP: 1.3.31.0
      * Wireless Edge DDP: 1.3.7.0

    * Intel\ |reg| 82599ES 10 Gigabit Ethernet Controller

      * Firmware version: 0x61bf0001
      * Device id (pf/vf): 8086:10fb / 8086:10ed
      * Driver version(in-tree): 5.1.0-k (ixgbe)
      * Driver version(out-tree): 5.13.4 (ixgbe)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-DA4 (4x10G)

      * PF Firmware version: 8.30 0x8000a49d 1.2926.0
      * VF Firmware version: 8.50 0x8000b6d9 1.3082.0
      * Device id (pf/vf): 8086:1572 / 8086:154c
      * Driver version: 2.17.4 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GbE SFP+ (4x10G)

      * Firmware version: 5.30 0x80002a29 1.2926.0
      * Device id (pf/vf): 8086:37d0 / 8086:37cd
      * Driver version: 2.17.4 (i40e)

    * Intel\ |reg| Corporation Ethernet Connection X722 for 10GBASE-T (2x10G)

      * Firmware version: 5.40 0x80002e2f 1.2935.0
      * Device id (pf/vf): 8086:37d2 / 8086:37cd
      * Driver version: 2.17.4 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XXV710-DA2 (2x25G)

      * PF Firmware version: 8.30 0x8000a483 1.2926.0
      * VF Firmware version: 8.50 0x8000b703 1.3082.0
      * Device id (pf/vf): 8086:158b / 8086:154c
      * Driver version: 2.17.4 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

      * PF Firmware version: 8.30 0x8000a4ae 1.2926.0
      * VF Firmware version: 8.50 0x8000b6c7 1.3082.0
      * Device id (pf/vf): 8086:1583 / 8086:154c
      * Driver version: 2.17.4 (i40e)

    * Intel\ |reg| Ethernet Converged Network Adapter X710-T2L

      * Firmware version: 8.30 0x8000a489 1.2879.0
      * Device id (pf): 8086:15ff
      * Driver version: 2.17.4 (i40e)

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

    * MLNX_OFED 5.5-0.5.9.0 and above
    * MLNX_OFED 5.4-3.1.0.0

  * upstream kernel:

    * Linux 5.16.0-rc2 and above

  * rdma-core:

    * rdma-core-37.1 and above

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
      * Firmware version: 14.32.0570 and above

    * Mellanox\ |reg| ConnectX\ |reg|-4 Lx 50G MCX4131A-GCAT (1x50G)

      * Host interface: PCI Express 3.0 x8
      * Device ID: 15b3:1015
      * Firmware version: 14.32.0570 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX516A-CCAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.0570 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.0570 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-EDAT (2x100G)

      * Host interface: PCI Express 3.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.0570 and above

    * Mellanox\ |reg| ConnectX\ |reg|-5 Ex EN 100G MCX516A-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1019
      * Firmware version: 16.32.0570 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx EN 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.32.0570 and above

    * Mellanox\ |reg| ConnectX\ |reg|-6 Lx EN 25G MCX631102AN-ADAT (2x25G)

      * Host interface: PCI Express 4.0 x8
      * Device ID: 15b3:101f
      * Firmware version: 26.32.0570 and above

* Mellanox\ |reg| BlueField\ |reg| SmartNIC

  * Mellanox\ |reg| BlueField\ |reg| 2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

    * Host interface: PCI Express 3.0 x16
    * Device ID: 15b3:a2d6
    * Firmware version: 24.32.0570 and above

  * Embedded software:

    * Ubuntu 20.04.3
    * MLNX_OFED 5.5-0.5.8 and above
    * DPDK application running on Arm cores

* IBM Power 9 platforms with Mellanox\ |reg| NICs combinations

  * CPU:

    * POWER9 2.2 (pvr 004e 1202) 2300MHz

  * OS:

    * Red Hat Enterprise Linux Server release 7.6

  * NICs:

    * Mellanox\ |reg| ConnectX\ |reg|-5 100G MCX556A-ECAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:1017
      * Firmware version: 16.32.0560

    * Mellanox\ |reg| ConnectX\ |reg|-6 Dx 100G MCX623106AN-CDAT (2x100G)

      * Host interface: PCI Express 4.0 x16
      * Device ID: 15b3:101d
      * Firmware version: 22.32.0560

  * OFED:

    * MLNX_OFED 5.5-0.5.9.0

* NXP ARM SoCs (with integrated NICs)

  * SoC:

    * NXP i.MX 8M Mini with ARM Cortex A53, Cortex M4

  * OS (Based on NXP LF support packages):

    * Kernel version: 5.10
    * Ubuntu 18.04

21.11.1 Release Notes
---------------------


21.11.1 Fixes
~~~~~~~~~~~~~

* acl: add missing C++ guards
* app/compress-perf: fix cycle count operations allocation
* app/compress-perf: fix number of queue pairs to setup
* app/compress-perf: fix socket ID type during init
* app/compress-perf: optimize operations pool allocation
* app/dumpcap: check for failure to set promiscuous
* app/fib: fix division by zero
* app/pdump: abort on multi-core capture limit
* app/regex: fix number of matches
* app/testpmd: check starting port is not in bonding
* app/testpmd: fix bonding mode set
* app/testpmd: fix build without drivers
* app/testpmd: fix dereference before null check
* app/testpmd: fix external buffer allocation
* app/testpmd: fix flow rule with flex input link
* app/testpmd: fix GENEVE parsing in checksum mode
* app/testpmd: fix GTP header parsing in checksum engine
* app/testpmd: fix raw encap of GENEVE option
* app/testpmd: fix show RSS RETA on Windows
* app/testpmd: fix stack overflow for EEPROM display
* app/testpmd: fix Tx scheduling interval
* baseband/acc100: avoid out-of-bounds access
* bpf: add missing C++ guards
* bpf: fix build with some libpcap version on FreeBSD
* build: fix build on FreeBSD with Meson 0.61.1
* build: fix warnings when running external commands
* build: hide local symbols in shared libraries
* build: remove deprecated Meson functions
* build: suppress rte_crypto_asym_op abi check
* buildtools: fix AVX512 check for Python 3.5
* bus/ifpga: remove useless check while browsing devices
* bus/pci: assign driver pointer before mapping
* common/cnxk: add missing checks of return values
* common/cnxk: add workaround for vWQE flush
* common/cnxk: always use single interrupt ID with NIX
* common/cnxk: fix base rule merge
* common/cnxk: fix bitmap usage for TM
* common/cnxk: fix byte order of frag sizes and infos
* common/cnxk: fix error checking
* common/cnxk: fix flow deletion
* common/cnxk: fix log level during MCAM allocation
* common/cnxk: fix mbuf data offset for VF
* common/cnxk: fix nibble parsing order when dumping MCAM
* common/cnxk: fix NPC key extraction validation
* common/cnxk: fix null pointer dereferences
* common/cnxk: fix reset of fields
* common/cnxk: fix shift offset for TL3 length disable
* common/cnxk: fix uninitialized pointer read
* common/cnxk: fix uninitialized variables
* common/cnxk fix unintended sign extension
* common/cnxk: reset stale values on error debug registers
* common/mlx5: add minimum WQE size for striding RQ
* common/mlx5: add Netlink event helpers
* common/mlx5: consider local functions as internal
* common/mlx5: fix error handling in multi-class probe
* common/mlx5: fix missing validation in devargs parsing
* common/mlx5: fix MR lookup for non-contiguous mempool
* common/mlx5: fix probing failure code
* common/mlx5: fix queue pair ack timeout configuration
* common/sfc_efx/base: add missing handler for 1-byte fields
* common/sfc_efx/base: fix recirculation ID set in outer rules
* compressdev: add missing C++ guards
* compressdev: fix missing space in log macro
* compressdev: fix socket ID type
* compress/mlx5: support out-of-space status
* compress/octeontx: fix null pointer dereference
* config: add arch define for Arm
* config: align mempool elements to 128 bytes on CN10K
* config/arm: add values for native armv7
* crypto/cnxk: enable allocated queues only
* crypto/cnxk: fix extend tail calculation
* crypto/cnxk: fix inflight count calculation
* crypto/cnxk: fix update of number of descriptors
* cryptodev: add missing C++ guards
* cryptodev: fix clang C++ include
* cryptodev: fix RSA key type name
* crypto/dpaax_sec: fix auth/cipher xform chain checks
* crypto/ipsec_mb: check missing operation types
* crypto/ipsec_mb: fix buffer overrun
* crypto/ipsec_mb: fix GCM requested digest length
* crypto/ipsec_mb: fix GMAC parameters setting
* crypto/ipsec_mb: fix length and offset settings
* crypto/ipsec_mb: fix length and offset settings
* crypto/ipsec_mb: fix premature dereference
* crypto/ipsec_mb: fix queue cleanup null pointer dereference
* crypto/ipsec_mb: fix queue setup null pointer dereference
* crypto/ipsec_mb: fix tainted data for session
* crypto/ipsec_mb: fix ZUC authentication verify
* crypto/ipsec_mb: fix ZUC operation overwrite
* crypto/ipsec_mb: remove useless check
* crypto/qat: fix GEN4 AEAD job in raw data path
* crypto/virtio: fix out-of-bounds access
* devargs: fix crash with uninitialized parsing
* devtools: fix comment detection in forbidden token check
* devtools: fix symbols check
* devtools: remove event/dlb exception in ABI check
* distributor: fix potential overflow
* dma/cnxk: fix installing internal headers
* dmadev: add missing header include
* dma/hisilicon: use common PCI device naming
* dma/idxd: configure maximum batch size to high value
* dma/idxd: fix burst capacity calculation
* dma/idxd: fix paths to driver sysfs directory
* dma/idxd: fix wrap-around in burst capacity calculation
* doc: add CUDA driver features
* doc: correct name of BlueField-2 in mlx5 guide
* doc: fix dlb2 guide
* doc: fix FIPS guide
* doc: fix KNI PMD name typo
* doc: fix missing note on UIO module in Linux guide
* doc: fix modify field action description for mlx5
* doc: fix telemetry example in cryptodev guide
* doc: fix typos and punctuation in flow API guide
* doc: improve configuration examples in idxd guide
* doc: remove dependency on findutils on FreeBSD
* doc: remove obsolete vector Tx explanations from mlx5 guide
* doc: replace broken links in mlx guides
* doc: replace characters for (R) symbol in Linux guide
* doc: replace deprecated distutils version parsing
* doc: update matching versions in ice guide
* eal: add missing C++ guards
* eal: fix C++ include
* eal/freebsd: add missing C++ include guards
* eal/linux: fix device monitor stop return
* eal/linux: fix illegal memory access in uevent handler
* eal/linux: log hugepage create errors with filename
* eal/windows: fix error code for not supported API
* efd: fix uninitialized structure
* ethdev: add internal function to device struct from name
* ethdev: add missing C++ guards
* ethdev: fix cast for C++ compatibility
* ethdev: fix doxygen comments for device info struct
* ethdev: fix MAC address in telemetry device info
* ethdev: fix Rx queue telemetry memory leak on failure
* ethdev: remove unnecessary null check
* event/cnxk: fix QoS devargs parsing
* event/cnxk: fix Rx adapter config check
* event/cnxk: fix sub-event clearing mask length
* event/cnxk: fix uninitialized local variables
* event/cnxk: fix variables casting
* eventdev: add missing C++ guards
* eventdev/eth_rx: fix missing internal port checks
* eventdev/eth_rx: fix parameters parsing memory leak
* eventdev/eth_rx: fix queue config query
* eventdev/eth_tx: fix queue add error code
* eventdev: fix C++ include
* eventdev: fix clang C++ include
* event/dlb2: add shift value check in sparse dequeue
* event/dlb2: poll HW CQ inflights before mapping queue
* event/dlb2: update rolling mask used for dequeue
* examples/distributor: reduce Tx queue number to 1
* examples/flow_classify: fix failure message
* examples/ipsec-secgw: fix buffer freeing in vector mode
* examples/ipsec-secgw: fix default flow rule creation
* examples/ipsec-secgw: fix eventdev start sequence
* examples/ipsec-secgw: fix offload flag used for TSO IPv6
* examples/kni: add missing trailing newline in log
* examples/l2fwd-crypto: fix port mask overflow
* examples/l3fwd: fix buffer overflow in Tx
* examples/l3fwd: fix Rx burst size for event mode
* examples/l3fwd: make Rx and Tx queue size configurable
* examples/l3fwd: share queue size variables
* examples/qos_sched: fix core mask overflow
* examples/vhost: fix launch with physical port
* fix spelling in comments and strings
* gpu/cuda: fix dependency loading path
* gpu/cuda: fix memory list cleanup
* graph: fix C++ include
* ipc: end multiprocess thread during cleanup
* ipsec: fix C++ include
* kni: add missing C++ guards
* kni: fix freeing order in device release
* maintainers: update for stable branches
* mem: check allocation in dynamic hugepage init
* mempool/cnxk: fix batch allocation failure path
* metrics: add missing C++ guards
* net/af_xdp: add missing trailing newline in logs
* net/af_xdp: ensure socket is deleted on Rx queue setup error
* net/af_xdp: fix build with -Wunused-function
* net/af_xdp: fix custom program loading with multiple queues
* net/axgbe: use PCI root complex device to distinguish device
* net/bnxt: add null check for mark table
* net/bnxt: cap maximum number of unicast MAC addresses
* net/bnxt: check VF representor pointer before access
* net/bnxt: fix check for autoneg enablement
* net/bnxt: fix crash by validating pointer
* net/bnxt: fix flow create when RSS is disabled
* net/bnxt: fix handling of VF configuration change
* net/bnxt: fix memzone allocation per VNIC
* net/bnxt: fix multicast address set
* net/bnxt: fix multicast MAC restore during reset recovery
* net/bnxt: fix null dereference in session cleanup
* net/bnxt: fix PAM4 mask setting
* net/bnxt: fix queue stop operation
* net/bnxt: fix restoring VLAN filtering after recovery
* net/bnxt: fix ring calculation for representors
* net/bnxt: fix ring teardown
* net/bnxt: fix VF resource allocation strategy
* net/bnxt: fix xstats names query overrun
* net/bnxt: fix xstats query
* net/bnxt: get maximum supported multicast filters count
* net/bnxt: handle ring cleanup in case of error
* net/bnxt: restore dependency on kernel modules
* net/bnxt: restore RSS configuration after reset recovery
* net/bnxt: set fast-path pointers only if recovery succeeds
* net/bnxt: set HW coalescing parameters
* net/bonding: fix mode type mismatch
* net/bonding: fix MTU set for slaves
* net/bonding: fix offloading configuration
* net/bonding: fix promiscuous and allmulticast state
* net/bonding: fix reference count on mbufs
* net/bonding: fix RSS with early configure
* net/bonding: fix slaves initializing on MTU setting
* net/cnxk: fix build with GCC 12
* net/cnxk: fix build with optimization
* net/cnxk: fix inline device RQ tag mask
* net/cnxk: fix inline IPsec security error handling
* net/cnxk: fix mbuf data length
* net/cnxk: fix promiscuous mode in multicast enable flow
* net/cnxk: fix RSS RETA table update
* net/cnxk: fix Rx/Tx function update
* net/cnxk: fix uninitialized local variable
* net/cnxk: register callback early to handle initial packets
* net/cxgbe: fix dangling pointer by mailbox access rework
* net/dpaa2: fix null pointer dereference
* net/dpaa2: fix timestamping for IEEE1588
* net/dpaa2: fix unregistering interrupt handler
* net/ena: check memory BAR before initializing LLQ
* net/ena: fix checksum flag for L4
* net/ena: fix meta descriptor DF flag setup
* net/ena: fix reset reason being overwritten
* net/ena: remove unused enumeration
* net/ena: remove unused offload variables
* net/ena: skip timer if reset is triggered
* net/enic: fix dereference before null check
* net: fix L2TPv2 common header
* net/hns3: delete duplicated RSS type
* net/hns3: fix double decrement of secondary count
* net/hns3: fix insecure way to query MAC statistics
* net/hns3: fix mailbox wait time
* net/hns3: fix max packet size rollback in PF
* net/hns3: fix operating queue when TCAM table is invalid
* net/hns3: fix RSS key with null
* net/hns3: fix RSS TC mode entry
* net/hns3: fix Rx/Tx functions update
* net/hns3: fix using enum as boolean
* net/hns3: fix vector Rx/Tx when PTP enabled
* net/hns3: fix VF RSS TC mode entry
* net/hns3: increase time waiting for PF reset completion
* net/hns3: remove duplicate macro definition
* net/i40e: enable maximum frame size at port level
* net/i40e: fix unintentional integer overflow
* net/iavf: count continuous DD bits for Arm
* net/iavf: count continuous DD bits for Arm in flex Rx
* net/iavf: fix AES-GMAC IV size
* net/iavf: fix function pointer in multi-process
* net/iavf: fix null pointer dereference
* net/iavf: fix potential out-of-bounds access
* net/iavf: fix segmentation offload buffer size
* net/iavf: fix segmentation offload condition
* net/iavf: remove git residue symbol
* net/iavf: reset security context pointer on stop
* net/iavf: support NAT-T / UDP encapsulation
* net/ice/base: add profile validation on switch filter
* net/ice: fix build with 16-byte Rx descriptor
* net/ice: fix link up when starting device
* net/ice: fix mbuf offload flag for Rx timestamp
* net/ice: fix overwriting of LSE bit by DCF
* net/ice: fix pattern check for flow director parser
* net/ice: fix pattern check in flow director
* net/ice: fix Tx checksum offload
* net/ice: fix Tx checksum offload capability
* net/ice: fix Tx offload path choice
* net/ice: track DCF state of PF
* net/ixgbe: add vector Rx parameter check
* net/ixgbe: check filter init failure
* net/ixgbe: fix FSP check for X550EM devices
* net/ixgbe: reset security context pointer on close
* net/kni: fix config initialization
* net/memif: remove pointer deference before null check
* net/memif: remove unnecessary Rx interrupt stub
* net/mlx5: fix ASO CT object release
* net/mlx5: fix assertion on flags set in packet mbuf
* net/mlx5: fix check in count action validation
* net/mlx5: fix committed bucket size
* net/mlx5: fix configuration without Rx queue
* net/mlx5: fix CPU socket ID for Rx queue creation
* net/mlx5: fix destroying empty matchers list
* net/mlx5: fix entry in shared Rx queues list
* net/mlx5: fix errno update in shared context creation
* net/mlx5: fix E-Switch manager vport ID
* net/mlx5: fix flex item availability
* net/mlx5: fix flex item availability
* net/mlx5: fix flex item header length translation
* net/mlx5: fix GCC uninitialized variable warning
* net/mlx5: fix GRE item translation in Verbs
* net/mlx5: fix GRE protocol type translation for Verbs
* net/mlx5: fix implicit tag insertion with sample action
* net/mlx5: fix indexed pool fetch overlap
* net/mlx5: fix ineffective metadata argument adjustment
* net/mlx5: fix inet IPIP protocol type
* net/mlx5: fix initial link status detection
* net/mlx5: fix inline length for multi-segment TSO
* net/mlx5: fix link status change detection
* net/mlx5: fix mark enabling for Rx
* net/mlx5: fix matcher priority with ICMP or ICMPv6
* net/mlx5: fix maximum packet headers size for TSO
* net/mlx5: fix memory socket selection in ASO management
* net/mlx5: fix metadata endianness in modify field action
* net/mlx5: fix meter capabilities reporting
* net/mlx5: fix meter creation default state
* net/mlx5: fix meter policy creation assert
* net/mlx5: fix meter sub-policy creation
* net/mlx5: fix modify field MAC address offset
* net/mlx5: fix modify port action validation
* net/mlx5: fix MPLS/GRE Verbs spec ordering
* net/mlx5: fix MPRQ stride devargs adjustment
* net/mlx5: fix MPRQ WQE size assertion
* net/mlx5: fix next protocol RSS expansion
* net/mlx5: fix NIC egress flow mismatch in switchdev mode
* net/mlx5: fix port matching in sample flow rule
* net/mlx5: fix RSS expansion with explicit next protocol
* net/mlx5: fix sample flow action on trusted device
* net/mlx5: fix shared counter flag in flow validation
* net/mlx5: fix shared RSS destroy
* net/mlx5: fix sibling device config check
* net/mlx5: fix VLAN push action validation
* net/mlx5: forbid multiple ASO actions in a single rule
* net/mlx5: improve stride parameter names
* net/mlx5: reduce flex item flow handle size
* net/mlx5: reject jump to root table
* net/mlx5: relax headroom assertion
* net/mlx5: remove unused function
* net/mlx5: remove unused reference counter
* net/mlx5: set flow error for hash list create
* net/nfb: fix array indexes in deinit functions
* net/nfb: fix multicast/promiscuous mode switching
* net/nfp: free HW ring memzone on queue release
* net/nfp: remove duplicated check when setting MAC address
* net/nfp: remove useless range checks
* net/ngbe: fix debug logs
* net/ngbe: fix missed link interrupt
* net/ngbe: fix packet statistics
* net/ngbe: fix Rx by initializing packet buffer early
* net/ngbe: fix Tx hang on queue disable
* net/qede: fix maximum Rx packet length
* net/qede: fix redundant condition in debug code
* net/qede: fix Rx bulk
* net/qede: fix Tx completion
* net/sfc: demand Tx fast free offload on EF10 simple datapath
* net/sfc: do not push fast free offload to default TxQ config
* net/sfc: fix flow tunnel support detection
* net/sfc: fix lock releases
* net/sfc: fix memory allocation size for cache
* net/sfc: reduce log level of tunnel restore info error
* net/sfc: validate queue span when parsing flow action RSS
* net/tap: fix to populate FDs in secondary process
* net/txgbe: fix debug logs
* net/txgbe: fix KR auto-negotiation
* net/txgbe: fix link up and down
* net/txgbe: fix queue statistics mapping
* net/txgbe: reset security context pointer on close
* net/virtio: fix slots number when indirect feature on
* net/virtio: fix Tx queue 0 overriden by queue 128
* net/virtio: fix uninitialized RSS key
* net/virtio-user: check FD flags getting failure
* net/virtio-user: fix resource leak on probing failure
* pcapng: handle failure of link status query
* pflock: fix header file installation
* pipeline: fix annotation checks
* pipeline: fix table state memory allocation
* raw/ifpga/base: fix port feature ID
* raw/ifpga/base: fix SPI transaction
* raw/ifpga: fix build with optimization
* raw/ifpga: fix interrupt handle allocation
* raw/ifpga: fix monitor thread
* raw/ifpga: fix thread closing
* raw/ifpga: fix variable initialization in probing
* raw/ntb: clear all valid doorbell bits on init
* regexdev: fix section attribute of symbols
* regex/mlx5: fix memory allocation check
* Revert "crypto/ipsec_mb: fix length and offset settings"
* Revert "net/mlx5: fix flex item availability"
* ring: fix error code when creating ring
* ring: fix overflow in memory size calculation
* sched: remove useless malloc in PIE data init
* stack: fix stubs header export
* table: fix C++ include
* telemetry: add missing C++ guards
* test/bpf: skip dump if conversion fails
* test/crypto: fix out-of-place SGL in raw datapath
* test/dma: fix missing checks for device capacity
* test/efd: fix sockets mask size
* test/mbuf: fix mbuf data content check
* test/mem: fix error check
* vdpa/ifc: fix log info mismatch
* vdpa/mlx5: workaround queue stop with traffic
* vdpa/sfc: fix null dereference during config
* vdpa/sfc: fix null dereference during removal
* version: 21.11.1-rc1
* vfio: cleanup the multiprocess sync handle
* vhost: add missing C++ guards
* vhost: fix C++ include
* vhost: fix FD leak with inflight messages
* vhost: fix field naming in guest page struct
* vhost: fix guest to host physical address mapping
* vhost: fix linker script syntax
* vhost: fix physical address mapping
* vhost: fix queue number check when setting inflight FD
* vhost: fix unsafe vring addresses modifications

21.11.1 Validation
~~~~~~~~~~~~~~~~~~

* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2022-April/037633.html>`_

   * testpmd send and receive multiple types of traffic
   * testpmd xstats counters
   * testpmd timestamp
   * Changing/checking link status through testpmd
   * RTE flow
   * Some RSS
   * VLAN stripping and insertion
   * checksum and TSO
   * ptype
   * ptype tests.
   * link_status_interrupt example application
   * l3fwd-power example application
   * multi-process example applications
   * Hardware LRO
   * Regex application
   * Buffer Split
   * Tx scheduling
   * Compilation tests

   * ConnectX-4 Lx

      * Ubuntu 20.04

      * driver MLNX_OFED_LINUX-5.5-1.0.3.2
      * fw 14.32.1010

   * ConnectX-5

      * Ubuntu 20.04

      * driver MLNX_OFED_LINUX-5.5-1.0.3.2
      * fw 16.32.2004

   * ConnectX-6 Dx

      * Ubuntu 20.04

      * driver MLNX_OFED_LINUX-5.5-1.0.3.2
      * fw 22.32.2004

   * BlueField-2

      * DOCA SW version: 1.2.1


* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2022-April/037650.html>`_

   * RHEL 8
   * Kernel 4.18
   * QEMU 6.2
   * Functionality

      * PF assignment
      * VF assignment
      * vhost single/multi queues and cross-NUMA
      * vhostclient reconnect
      * vhost live migration with single/multi queues and cross-NUMA
      * OVS PVP

   * Tested NICs

      * X540-AT2 NIC(ixgbe, 10G)


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2022-April/037680.html>`_

   * Compilation tests

   * Basic Intel(R) NIC(ixgbe, i40e, ice)

      * PF (i40e, ixgbe, ice)
      * VF (i40e, ixgbe, ice)
      * Intel NIC single core/NIC performance
      * IPsec test scenarios
      * Power test scenarios

   * Basic cryptodev and virtio

      * vhost/virtio basic loopback, PVP and performance
      * cryptodev function
      * cryptodev performance
      * vhost_crypto unit test and function/performance test

* `Canonical(R) Testing <https://mails.dpdk.org/archives/stable/2022-April/037717.html>`_

   * Build tests of DPDK & OVS 2.13.3 on Ubuntu 20.04 (meson based)
   * Functional and performance tests based on OVS-DPDK on x86_64
   * Autopkgtests for DPDK and OpenvSwitch

21.11.1 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.1 contains fixes up to DPDK 22.03
* Issues identified/fixed in DPDK main branch after DPDK 22.03 may be present in DPDK 21.11.1

21.11.2 Release Notes
---------------------


21.11.2 Fixes
~~~~~~~~~~~~~

* acl: fix rules with 8-byte field size
* app/flow-perf: fix build with GCC 12
* app/procinfo: show all non-owned ports
* app/regex: avoid division by zero
* app/regex: fix mbuf size for multi-segment buffer
* app/testpmd: add help messages for multi-process
* app/testpmd: check statistics query before printing
* app/testpmd: cleanup port resources after implicit close
* app/testpmd: do not poll stopped queues
* app/testpmd: fix bonding slave devices not released
* app/testpmd: fix flex parser destroy command
* app/testpmd: fix GTP PSC raw processing
* app/testpmd: fix GTP PSC raw processing
* app/testpmd: fix help of create meter command
* app/testpmd: fix metering and policing command for RFC4115
* app/testpmd: fix MTU verification
* app/testpmd: fix multicast address pool leak
* app/testpmd: fix packet segment allocation
* app/testpmd: fix port status of bonding slave device
* app/testpmd: fix supported RSS offload display
* app/testpmd: fix use of indirect action after port close
* app/testpmd: perform SW IP checksum for GRO/GSO packets
* app/testpmd: remove useless pointer checks
* app/testpmd: replace hardcoded min mbuf number with macro
* app/testpmd: revert MAC update in checksum forwarding
* avoid AltiVec keyword vector
* baseband/acc100: add protection for some negative scenario
* baseband/acc100: update companion PF configure function
* bus/fslmc: fix VFIO setup
* common/cnxk: allow changing PTP mode on CN10K
* common/cnxk: fix decrypt packet count register update
* common/cnxk: fix GRE tunnel parsing
* common/cnxk: fix null pointer dereference
* common/cnxk: fix SQ flush sequence
* common/cnxk: fix unaligned access to device memory
* common/cnxk: handle ROC model init failure
* common/cnxk: swap zuc-256 key
* common/cpt: fix build with GCC 12
* common/dpaax: fix short MAC-I IV calculation for ZUC
* common/mlx5: fix memory region range calculation
* common/mlx5: fix non-expandable global MR cache
* common/mlx5: remove unused lcore check
* common/sfc_efx/base: convert EFX PCIe INTF to MCDI value
* config: fix C++ cross compiler for Arm and PPC
* crypto/cnxk: fix build with GCC 12
* crypto/cnxk: swap zuc-256 iv
* crypto/dpaa2_sec: fix buffer pool ID check
* crypto/dpaa2_sec: fix chained FD length in raw datapath
* crypto/dpaa2_sec: fix crypto operation pointer
* crypto/dpaa2_sec: fix fle buffer leak
* crypto/dpaa2_sec: fix operation status for simple FD
* crypto/dpaa_sec: fix chained FD length in raw datapath
* crypto/dpaa_sec: fix digest size
* crypto/dpaa_sec: fix secondary process probing
* crypto/ipsec_mb: fix build with GCC 12
* crypto/mlx5: fix login cleanup
* crypto/qat: fix DOCSIS crash
* crypto/scheduler: fix queue pair in scheduler failover
* devargs: fix leak on hotplug failure
* devtools: fix null test for NUMA systems
* dma/hisilicon: enhance CQ scan robustness
* dma/hisilicon: fix includes in header file
* dma/hisilicon: fix index returned when no DMA completed
* dma/idxd: fix AVX2 in non-datapath functions
* dma/idxd: fix error code for PCI device commands
* dma/idxd: fix memory leak in PCI close
* dma/idxd: fix non-AVX builds with old compilers
* dma/idxd: fix null dereference in PCI remove
* dma/idxd: fix partial freeing in PCI close
* dma/skeleton: fix index returned when no memcpy completed
* doc: add missing auth algo for IPsec example
* doc: add more instructions for running as non-root
* doc: fix API index Markdown syntax
* doc: fix build with sphinx 4.5
* doc: fix flow integrity hardware support in mlx5 guide
* doc: fix formatting and link in BPF library guide
* doc: fix grammar and formatting in compressdev guide
* doc: fix grammar and parameters in l2fwd-crypto guide
* doc: fix readability in vhost guide
* doc: fix release note typo
* doc: fix vhost multi-queue reconnection
* doc: update matching versions in i40e guide
* doc: update matching versions in ice guide
* drivers/crypto: fix warnings for OpenSSL version
* eal: fix C++ include for device event and DMA
* eal/freebsd: fix use of newer cpuset macros
* eal/ppc: fix compilation for musl
* eal/windows: add missing C++ include guards
* eal/windows: fix data race when creating threads
* eal/x86: drop export of internal alignment macro
* eal/x86: fix unaligned access for small memcpy
* ethdev: fix build with vtune option
* ethdev: fix memory leak in xstats telemetry
* ethdev: fix port close in secondary process
* ethdev: fix port state when stop
* ethdev: fix possible null pointer access
* ethdev: fix RSS update when RSS is disabled
* ethdev: prohibit polling stopped queue
* event/cnxk: fix out of bounds access in test
* event/cnxk: fix QoS parameter handling
* event/cnxk: fix Tx adapter enqueue return for CN10K
* eventdev/eth_rx: fix telemetry Rx stats reset
* eventdev/eth_tx: fix adapter creation
* eventdev/eth_tx: fix queue delete
* event/dlb2: fix advertized capabilities
* event/dlb2: fix check of QID in-flight
* event/dlb2: rework queue drain handling
* event/octeontx: fix SSO fast path
* examples/bond: fix invalid use of trylock
* examples/distributor: fix distributor on Rx core
* examples/dma: fix MTU configuration
* examples/dma: fix Tx drop statistics
* examples/fips_validation: handle empty payload
* examples/ipsec-secgw: fix ESN setting
* examples/ipsec-secgw: fix NAT-T header fields
* examples/ipsec-secgw: fix promiscuous mode option
* examples/ipsec-secgw: fix uninitialized memory access
* examples/l2fwd-crypto: fix stats refresh rate
* examples/link_status_interrupt: fix stats refresh rate
* examples/performance-thread: fix build with GCC 12
* examples/vhost: fix crash when no VMDq
* examples/vhost: fix retry logic on Rx path
* gro: fix identifying fragmented packets
* ipsec: fix NAT-T ports and length
* kni: fix build
* kni: fix build with Linux 5.18
* kni: use dedicated function to set MAC address
* kni: use dedicated function to set random MAC address
* malloc: fix allocation of almost hugepage size
* malloc: fix ASan handling for unmapped memory
* mbuf: dump outer VLAN
* mem: skip attaching external memory in secondary process
* net/af_xdp: make compatible with libbpf >= 0.7.0
* net/af_xdp: use libxdp if available
* net/axgbe: fix xstats get return if xstats is null
* net/bnxt: allow Tx only or Rx only
* net/bnxt: avoid unnecessary endianness conversion
* net/bnxt: check duplicate queue IDs
* net/bnxt: cleanup MTU setting
* net/bnxt: disallow MTU change when device is started
* net/bnxt: fix check for autoneg enablement in the PHY FW
* net/bnxt: fix compatibility with some old firmwares
* net/bnxt: fix device capability reporting
* net/bnxt: fix freeing VNIC filters
* net/bnxt: fix link status when port is stopped
* net/bnxt: fix reordering in NEON Rx
* net/bnxt: fix ring group on Rx restart
* net/bnxt: fix RSS action
* net/bnxt: fix Rx configuration
* net/bnxt: fix setting forced speed
* net/bnxt: fix speed autonegotiation
* net/bnxt: fix switch domain allocation
* net/bnxt: fix tunnel stateless offloads
* net/bnxt: fix ULP parser to ignore segment offset
* net/bnxt: force PHY update on certain configurations
* net/bnxt: handle queue stop during RSS flow create
* net/bnxt: recheck FW readiness if in reset process
* net/bnxt: remove unused macro
* net/bonding: fix mbuf fast free usage
* net/bonding: fix RSS inconsistency between ports
* net/bonding: fix RSS key config with extended key length
* net/bonding: fix slave stop and remove on port close
* net/bonding: fix stopping non-active slaves
* net/cnxk: add barrier after meta batch free in scalar
* net/cnxk: add message on flow parsing failure
* net/cnxk: fix possible null dereference in telemetry
* net/cnxk: fix uninitialized variables
* net/cxgbe: fix port ID in Rx mbuf
* net/cxgbe: fix Tx queue stuck with mbuf chain coalescing
* net/dpaa2: fix dpdmux default interface
* net/dpaa: fix event queue detach
* net/ena: fix build with GCC 12
* net/enetfec: fix build with GCC 12
* net/failsafe: fix device freeing
* net: fix GTP PSC headers
* net/hns3: delete unused code
* net/hns3: fix an unreasonable memset
* net/hns3: fix code check warning
* net/hns3: fix crash from secondary process
* net/hns3: fix descriptors check with SVE
* net/hns3: fix link status capability query from VF
* net/hns3: fix MAC and queues HW statistics overflow
* net/hns3: fix mbuf free on Tx done cleanup
* net/hns3: fix order of clearing imissed register in PF
* net/hns3: fix pseudo-sharing between threads
* net/hns3: fix PTP interrupt logging
* net/hns3: fix return value for unsupported tuple
* net/hns3: fix rollback on RSS hash update
* net/hns3: fix RSS disable
* net/hns3: fix statistics locking
* net/hns3: fix TM capability
* net/hns3: fix xstats get return if xstats is null
* net/hns3: remove duplicate definition
* net/hns3: remove redundant RSS tuple field
* net/hns3: remove unnecessary RSS switch
* net/hns3: support backplane media type
* net/i40e: fix max frame size config at port level
* net/i40e: populate error in flow director parser
* net/iavf: fix data path selection
* net/iavf: fix device initialization without inline crypto
* net/iavf: fix device stop
* net/iavf: fix GTP-U extension flow
* net/iavf: fix mbuf release in multi-process
* net/iavf: fix NAT-T payload length
* net/iavf: fix queue start exception handling
* net/iavf: fix Rx queue interrupt setting
* net/iavf: fix segfaults when calling API after VF reset failed
* net/iavf: fix VF reset
* net/iavf: increase reset complete wait count
* net/iavf: remove dead code
* net/ice: add missing Tx burst mode name
* net/ice/base: fix build with GCC 12
* net/ice/base: fix direction of flow that matches any
* net/ice/base: fix getting sched node from ID type
* net/ice: fix build with GCC 12
* net/ice: fix MTU info for DCF
* net/ice: fix race condition in Rx timestamp
* net/ice: fix raw flow input pattern parsing
* net/ice: improve performance of Rx timestamp offload
* net/ice: refactor parser usage
* net/igc: support multi-process
* net/ipn3ke: fix xstats get return if xstats is null
* net/ixgbe: add option for link up check on pin SDP3
* net/memif: fix overwriting of head segment
* net/mlx5: add limitation for E-Switch Manager match
* net/mlx5: fix build with clang 14
* net/mlx5: fix counter in non-termination meter
* net/mlx5: fix GTP handling in header modify action
* net/mlx5: fix LRO configuration in drop Rx queue
* net/mlx5: fix LRO validation in Rx setup
* net/mlx5: fix metering on E-Switch Manager
* net/mlx5: fix no-green metering with RSS
* net/mlx5: fix probing with secondary bonding member
* net/mlx5: fix RSS expansion for patterns with ICMP item
* net/mlx5: fix RSS hash types adjustment
* net/mlx5: fix Rx queue recovery mechanism
* net/mlx5: fix Rx/Tx stats concurrency
* net/mlx5: fix stack buffer overflow in drop action
* net/mlx5: fix statistics read on Linux
* net/mlx5: fix Tx recovery
* net/mlx5: fix Tx when inlining is impossible
* net/mlx5: reject negative integrity item configuration
* net/mlx5: restrict Rx queue array access to boundary
* net/mvpp2: fix xstats get return if xstats is null
* net/netvsc: fix calculation of checksums based on mbuf flag
* net/netvsc: fix hot adding multiple VF PCI devices
* net/netvsc: fix vmbus device reference in multi-process
* net/nfp: fix disabling VLAN stripping
* net/nfp: fix initialization
* net/nfp: make sure MTU is never larger than mbuf size
* net/nfp: remove unneeded header inclusion
* net/nfp: update how max MTU is read
* net/ngbe: add more packet statistics
* net/ngbe: fix link speed check
* net/ngbe: fix PCIe related operations with bus API
* net/ngbe: fix reading PHY ID
* net/octeontx: fix port close
* net/qede: fix build with GCC 12
* net/qede: fix build with GCC 13
* net/tap: fix device freeing
* net/tap: fix interrupt handler freeing
* net/txgbe: fix max number of queues for SR-IOV
* net/txgbe: fix register polling
* net/txgbe: fix SGMII mode to link up
* net/vhost: fix access to freed memory
* net/vhost: fix deadlock on vring state change
* net/vhost: fix null pointer dereference
* net/vhost: fix TSO feature default disablement
* net/virtio: restore some optimisations with AVX512
* net/virtio: unmap PCI device in secondary process
* net/virtio-user: fix Rx interrupts with multi-queue
* net/virtio-user: fix socket non-blocking mode
* net/vmxnet3: fix Rx data ring initialization
* pcapng: fix timestamp wrapping in output files
* pipeline: fix emit instruction for invalid headers
* raw/ifpga: remove virtual devices on close
* raw/ifpga: unregister interrupt on close
* raw/ioat: fix build missing errno include
* raw/ioat: fix build when ioat dmadev enabled
* rib: fix references for IPv6 implementation
* rib: fix traversal with /32 route
* sched: remove unnecessary floating point
* security: fix SA lifetime comments
* service: fix lingering active status
* test: avoid hang if queues are full and Tx fails
* test/bonding: fix RSS test when disable RSS
* test/bpf: skip test if libpcap is unavailable
* test: check memory allocation for CRC
* test/crypto: fix authentication IV for ZUC SGL
* test/crypto: fix cipher offset for ZUC
* test/crypto: fix driver name for DPAA raw API test
* test/crypto: fix null check for ZUC authentication
* test/crypto: fix SNOW3G vector IV format
* test/crypto: fix ZUC vector IV format
* test/crypto: skip oop test for raw api
* test: drop reference to removed tests
* test/hash: fix out of bound access
* test/ipsec: fix build with GCC 12
* test/ipsec: fix performance test
* test/mem: disable ASan when accessing unallocated memory
* test/table: fix buffer overflow on lpm entry
* trace: fix crash when exiting
* trace: fix init with long file prefix
* vdpa/ifc/base: fix null pointer dereference
* vdpa/ifc: fix build with GCC 12
* vdpa/mlx5: fix dead loop when process interrupted
* vdpa/mlx5: fix interrupt trash that leads to crash
* vdpa/mlx5: fix leak on event thread creation
* vdpa/mlx5: fix maximum number of virtqs
* vdpa/mlx5: workaround var offset within page
* vdpa/sfc: fix sync between QEMU and vhost-user
* vdpa/sfc: resolve race between vhost lib and device conf
* version: 21.11.2-rc1
* vhost: add some trailing newline in log messages
* vhost/crypto: fix build with GCC 12
* vhost/crypto: fix descriptor processing
* vhost: discard too small descriptor chains
* vhost: fix async access
* vhost: fix deadlock when message handling failed
* vhost: fix header spanned across more than two descriptors
* vhost: fix missing enqueue pseudo-header calculation
* vhost: fix missing virtqueue lock protection
* vhost: restore device information in log messages

21.11.2 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2022-August/039801.html>`__

   * Platform

      * RHEL 8
      * Kernel 4.18
      * Qemu 6.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2022-August/040006.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e)
      * PF (ixgbe)
      * PF (ice)
      * VF (i40e)
      * VF (ixgbe)
      * VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * Power and IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2022-August/039931.html>`__

   * Basic functionality with testpmd

      * Tx/Rx
      * xstats
      * Timestamps
      * Link status
      * RTE flow and flow_director
      * RSS
      * VLAN filtering, stripping and insertion
      * Checksum/TSO
      * ptype
      * link_status_interrupt example application
      * l3fwd-power example application
      * Multi-process example applications
      * Hardware LRO tests
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests

      * Ubuntu 20.04.4 with MLNX_OFED_LINUX-5.7-1.0.2.0.
      * Ubuntu 20.04.4 with rdma-core master (23a0021).
      * Ubuntu 20.04.4 with rdma-core v28.0.
      * Ubuntu 18.04.6 with rdma-core v17.1.
      * Ubuntu 18.04.6 with rdma-core master (23a0021) (i386).
      * Ubuntu 16.04.7 with rdma-core v22.7.
      * Fedora 35 with rdma-core v39.0.
      * Fedora 37 (Rawhide) with rdma-core v39.0 (with clang only).
      * CentOS 7 7.9.2009 with rdma-core master (23a0021).
      * CentOS 7 7.9.2009 with MLNX_OFED_LINUX-5.7-1.0.2.0.
      * CentOS 8 8.4.2105 with rdma-core master (23a0021).
      * OpenSUSE Leap 15.4 with rdma-core v38.1.
      * Windows Server 2019 with Clang 11.0.0.

   * ConnectX-6 Dx

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-5.7-1.0.2.0
      * fw 22.34.1002

   * ConnectX-5

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-5.7-1.0.2.0
      * fw 16.34.1002

   * ConnectX-4 Lx

      * Ubuntu 20.04
      * Driver MLNX_OFED_LINUX-5.7-1.0.2.0
      * fw 14.32.1010

   * BlueField-2

      * DOCA SW version: 1.4.0


* `Intel(R) Testing with Open vSwitch <https://mails.dpdk.org/archives/stable/2022-August/040028.html>`__

   * 21.11.2 validated by Intel for i40e, ICE, vhost and MTU for OVS with DPDK

21.11.2 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.2 contains fixes up to DPDK 22.07 as well as fixes for CVE-2022-28199 and CVE-2022-2132
* Issues identified/fixed in DPDK main branch after DPDK 22.07 may be present in DPDK 21.11.2

21.11.3 Release Notes
---------------------


21.11.3 Fixes
~~~~~~~~~~~~~

* app/dumpcap: fix crash on cleanup
* app/dumpcap: fix pathname for output file
* app/eventdev: fix limits in error message
* app/testpmd: fix build with clang 15
* app/testpmd: fix build with clang 15 in flow code
* app/testpmd: fix MAC header in checksum forward engine
* app/testpmd: make quit flag volatile
* app/testpmd: remove jumbo offload
* app/testpmd: restore ixgbe bypass commands
* app/testpmd: skip port reset in secondary process
* baseband/acc100: add LDPC encoder padding function
* baseband/acc100: add null checks
* baseband/acc100: check AQ availability
* baseband/acc100: check turbo dec/enc input
* baseband/acc100: enforce additional check on FCW
* baseband/acc100: fix clearing PF IR outside handler
* baseband/acc100: fix close cleanup
* baseband/acc100: fix device minimum alignment
* baseband/acc100: fix double MSI intr in TB mode
* baseband/acc100: fix input length for CRC24B
* baseband/acc100: fix memory leak
* baseband/acc100: fix null HARQ input case
* baseband/acc100: fix ring availability calculation
* baseband/acc100: fix ring/queue allocation
* build: enable developer mode for all working trees
* buildtools: fix NUMA nodes count
* bus/auxiliary: prevent device from being probed again
* bus/dpaa: fix build with clang 15
* ci: bump versions of actions in GHA
* ci: enable ABI check in GHA
* ci: update to new API for step outputs in GHA
* common/cnxk: fix log level during MCAM allocation
* common/cnxk: fix missing flow counter reset
* common/cnxk: fix printing disabled MKEX registers
* common/cnxk: fix schedule weight update
* common/iavf: avoid copy in async mode
* common/mlx5: fix multi-process mempool registration
* common/mlx5: fix shared mempool subscription
* common/qat: fix VF to PF answer
* common/sfc_efx/base: fix maximum Tx data count
* common/sfc_efx/base: remove VQ index check during VQ start
* cryptodev: fix missing SHA3 algorithm strings
* cryptodev: fix unduly newlines in logs
* crypto/qat: fix build with GCC 12
* crypto/qat: fix null hash algorithm digest size
* devtools: fix checkpatch header retrieval from stdin
* dma/idxd: check DSA device allocation
* doc: add LRO size limitation in mlx5 guide
* doc: add Rx buffer split capability for mlx5
* doc: avoid meson deprecation in setup
* doc: document device dump in procinfo guide
* doc: fix application name in procinfo guide
* doc: fix colons in testpmd aged flow rules
* doc: fix dumpcap interface parameter option
* doc: fix event timer adapter guide
* doc: fix maximum packet size of virtio driver
* doc: fix reference to dma application example
* doc: fix support table for Ethernet/VLAN flow items
* doc: fix typo depreciated instead of deprecated
* doc: fix underlines in testpmd guide
* drivers: fix typos found by Lintian
* drivers: remove unused build variable
* eal: fix data race in multi-process support
* eal: fix doxygen comments for UUID
* eal: fix side effect in some pointer arithmetic macros
* event/cnxk: fix mbuf offset calculation
* event/cnxk: fix missing mempool cookie marking
* event/cnxk: fix missing xstats operations
* eventdev/crypto: fix multi-process
* eventdev/eth_tx: add spinlock for adapter start/stop
* eventdev/eth_tx: fix adapter stop
* eventdev/eth_tx: fix queue delete
* eventdev: fix name of Rx conf type in documentation
* event/dlb2: handle enqueuing more than maximum depth
* event/dsw: fix flow migration
* event/sw: fix device name in dump
* event/sw: fix flow ID init in self test
* event/sw: fix log in self test
* examples/fips_validation: fix typo in error log
* examples/ipsec-secgw: fix Tx checksum offload flag
* examples/ipsec-secgw: fix Tx checksum offload flag
* examples/ipsec-secgw: use Tx checksum offload conditionally
* examples/l2fwd-crypto: fix typo in error message
* examples/l3fwd: fix MTU configuration with event mode
* examples/qos_sched: fix number of subport profiles
* examples/vhost: fix use after free
* examples/vm_power_manager: use safe list iterator
* graph: fix node objects allocation
* gro: check payload length after trim
* gro: trim tail padding bytes
* hash: fix RCU configuration memory leak
* ipsec: fix build with GCC 12
* lib: remove empty return types from doxygen comments
* malloc: fix storage size for some allocations
* mem: fix API doc about allocation on secondary processes
* mempool/cnxk: fix destroying empty pool
* mempool: make event callbacks process-private
* net: accept unaligned data in checksum routines
* net/atlantic: fix build with clang 15
* net/axgbe: clear buffer on scattered Rx chaining failure
* net/axgbe: fix checksum and RSS in scattered Rx
* net/axgbe: fix length of each segment in scattered Rx
* net/axgbe: fix mbuf lengths in scattered Rx
* net/axgbe: fix scattered Rx
* net/axgbe: optimise scattered Rx
* net/axgbe: remove freeing buffer in scattered Rx
* net/axgbe: reset end of packet in scattered Rx
* net/axgbe: save segment data in scattered Rx
* net/bnxt: fix build with GCC 13
* net/bnxt: fix error code during MTU change
* net/bnxt: fix null pointer dereference in LED config
* net/bnxt: fix representor info freeing
* net/bnxt: remove unnecessary check
* net/bonding: fix array overflow in Rx burst
* net/bonding: fix descriptor limit reporting
* net/bonding: fix double slave link status query
* net/bonding: fix dropping valid MAC packets
* net/bonding: fix flow flush order on close
* net/bonding: fix mbuf fast free handling
* net/bonding: fix slave device Rx/Tx offload configuration
* net/bonding: fix Tx hash for TCP
* net/bonding: set initial value of descriptor count alignment
* net/cnxk: fix DF bit in vector mode
* net/cnxk: fix later skip to include mbuf private data
* net/dpaa2: fix buffer freeing on SG Tx
* net/dpaa2: fix build with clang 15
* net/dpaa2: fix DPDMUX error behaviour
* net/dpaa2: use internal mempool for SG table
* net/dpaa: fix buffer freeing in slow path
* net/dpaa: fix buffer freeing on SG Tx
* net/dpaa: fix jumbo packet Rx in case of VSP
* net/dpaa: use internal mempool for SG table
* net/enetfec: fix buffer leak
* net/enetfec: fix restart
* net/failsafe: fix interrupt handle leak
* net/hns3: add L3 and L4 RSS types
* net/hns3: delete unused markup
* net/hns3: extract functions to create RSS and FDIR flow rule
* net/hns3: fix clearing hardware MAC statistics
* net/hns3: fix crash in SVE Tx
* net/hns3: fix crash when secondary process access FW
* net/hns3: fix IPv4 and IPv6 RSS
* net/hns3: fix IPv4 RSS
* net/hns3: fix lock protection of RSS flow rule
* net/hns3: fix minimum Tx frame length
* net/hns3: fix next-to-use overflow in simple Tx
* net/hns3: fix next-to-use overflow in SVE Tx
* net/hns3: fix packet type for GENEVE
* net/hns3: fix restore filter function input
* net/hns3: fix RSS filter restore
* net/hns3: fix RSS flow rule restore
* net/hns3: fix RSS rule restore
* net/hns3: fix Rx with PTP
* net/hns3: fix typos in IPv6 SCTP fields
* net/hns3: fix VF mailbox message handling
* net/hns3: move flow direction rule recovery
* net/hns3: revert fix mailbox communication with HW
* net/hns3: revert Tx performance optimization
* net/i40e: fix build with MinGW GCC 12
* net/i40e: fix jumbo frame Rx with X722
* net/i40e: fix pctype configuration for X722
* net/i40e: fix VF representor release
* net/iavf: add thread for event callbacks
* net/iavf: check illegal packet sizes
* net/iavf: fix IPsec flow create error check
* net/iavf: fix L3 checksum Tx offload flag
* net/iavf: fix outer checksum flags
* net/iavf: fix pattern check for flow director parser
* net/iavf: fix processing VLAN TCI in SSE path
* net/iavf: fix queue stop for large VF
* net/iavf: fix SPI check
* net/iavf: fix Tx done descriptors cleanup
* net/iavf: fix VLAN insertion
* net/iavf: fix VLAN offload
* net/iavf: revert VLAN insertion fix
* net/iavf: update IPsec ESN values when updating session
* net/ice/base: fix 100M speed capability
* net/ice/base: fix add MAC rule
* net/ice/base: fix array overflow in add switch recipe
* net/ice/base: fix bit finding range over ptype bitmap
* net/ice/base: fix division during E822 PTP init
* net/ice/base: fix double VLAN in promiscuous mode
* net/ice/base: fix DSCP PFC TLV creation
* net/ice/base: fix duplicate flow rules
* net/ice/base: fix endian format
* net/ice/base: fix function descriptions for parser
* net/ice/base: fix inner symmetric RSS hash in raw flow
* net/ice/base: fix input set of GTPoGRE
* net/ice/base: fix media type of PHY 10G SFI C2C
* net/ice/base: ignore promiscuous already exist
* net/ice: check illegal packet sizes
* net/ice: fix interrupt handler unregister
* net/ice: fix null function pointer call
* net/ice: fix RSS hash update
* net/ice: fix scalar Rx path segment
* net/ice: fix scalar Tx path segment
* net/ice: support VXLAN-GPE tunnel offload
* net/ionic: fix adapter name for logging
* net/ionic: fix endianness for RSS
* net/ionic: fix endianness for Rx and Tx
* net/ionic: fix reported error stats
* net/ionic: fix Rx filter save
* net/ixgbe: fix broadcast Rx on VF after promisc removal
* net/ixgbe: fix unexpected VLAN Rx in promisc mode on VF
* net/ixgbevf: fix promiscuous and allmulti
* net/memif: fix crash with different number of Rx/Tx queues
* net/mlx4: fix Verbs FD leak in secondary process
* net/mlx5: fix action flag data type
* net/mlx5: fix assert when creating meter policy
* net/mlx5: fix build with recent compilers
* net/mlx5: fix check for orphan wait descriptor
* net/mlx5: fix drop action validation
* net/mlx5: fix first segment inline length
* net/mlx5: fix hairpin split with set VLAN VID action
* net/mlx5: fix indexed pool local cache crash
* net/mlx5: fix inline length exceeding descriptor limit
* net/mlx5: fix maximum LRO message size
* net/mlx5: fix meter profile delete after disable
* net/mlx5: fix mirror flow validation with ASO action
* net/mlx5: fix modify action with tunnel decapsulation
* net/mlx5: fix null check in devargs parsing
* net/mlx5: fix port event cleaning order
* net/mlx5: fix port initialization with small LRO
* net/mlx5: fix race condition in counter pool resizing
* net/mlx5: fix RSS expansion buffer size
* net/mlx5: fix shared Rx queue config reuse
* net/mlx5: fix single not inline packet storing
* net/mlx5: fix source port checking in sample flow rule
* net/mlx5: fix thread termination check on Windows
* net/mlx5: fix thread workspace memory leak
* net/mlx5: fix tunnel header with IPIP offload
* net/mlx5: fix Tx check for hardware descriptor length
* net/mlx5: fix Verbs FD leak in secondary process
* net/mvneta: fix build with GCC 12
* net/nfp: compose firmware file name with new hwinfo
* net/nfp: fix internal buffer size and MTU check
* net/nfp: fix memory leak in Rx
* net/nfp: fix Rx descriptor DMA address
* net/nfp: improve HW info header log readability
* net/ngbe: fix maximum frame size
* net/ngbe: remove semaphore between SW/FW
* net/ngbe: rename some extended statistics
* net/qede/base: fix 32-bit build with GCC 12
* net/tap: fix overflow of network interface index
* net/txgbe: fix IPv6 flow rule
* net/txgbe: remove semaphore between SW/FW
* net/txgbe: rename some extended statistics
* net/virtio: fix crash when configured twice
* node: check Rx element allocation
* pcapng: fix write more packets than IOV_MAX limit
* pdump: do not allow enable/disable in primary process
* power: fix some doxygen comments
* Revert "cryptodev: fix missing SHA3 algorithm strings"
* Revert "net/i40e: enable maximum frame size at port level"
* Revert "net/i40e: fix jumbo frame Rx with X722"
* Revert "net/i40e: fix max frame size config at port level"
* Revert "net/iavf: add thread for event callbacks"
* ring: fix description
* ring: remove leftover comment about watermark
* ring: squash gcc 12.2.1 warnings
* sched: fix subport profile configuration
* service: fix build with clang 15
* service: fix early move to inactive status
* test/crypto: fix bitwise operator in a SNOW3G case
* test/crypto: fix debug messages
* test/crypto: fix PDCP vectors
* test/crypto: fix wireless auth digest segment
* test/efd: fix build with clang 15
* test/event: fix build with clang 15
* test/hash: fix bulk lookup check
* test/hash: remove dead code in extendable bucket test
* test/ipsec: fix build with GCC 12
* test/ipsec: skip if no compatible device
* test/member: fix build with clang 15
* timer: fix stopping all timers
* trace: fix dynamically enabling trace points
* trace: fix leak with regexp
* trace: fix metadata dump
* trace: fix mode change
* trace: fix mode for new trace point
* trace: fix race in debug dump
* vdpa/ifc: handle data path update failure
* version: 21.11.3-rc1
* vhost: add non-blocking API for posting interrupt
* vhost: fix build with clang 15
* vhost: fix build with GCC 12
* vhost: fix doxygen warnings
* vhost: fix virtqueue use after free on NUMA reallocation

21.11.3 Validation
~~~~~~~~~~~~~~~~~~

* `Ubuntu Testing <https://mails.dpdk.org/archives/stable/2022-December/041641.html>`__

   * Physical NIC tests
   * Virtual NIC tests
   * OVS-DPDK VUC tests


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2022-December/041659.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e)
      * PF (ixgbe)
      * PF (ice)
      * VF (i40e)
      * VF (ixgbe)
      * VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * Power and IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2022-December/041665.html>`__

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
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-5
   * ConnectX-4 Lx
   * BlueField-2


* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2022-December/041667.html>`__

   * Platform

      * RHEL 8
      * Kernel 4.18
      * Qemu 6.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing

21.11.3 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.3 contains fixes up to DPDK 22.11
* Issues identified/fixed in DPDK main branch after DPDK 22.11 may be present in DPDK 21.11.3
* Some i40e `patches <https://mails.dpdk.org/archives/stable/2022-December/041648.html>`__ from DPDK 21.11.2 reverted as causing issue and no fix available.
* Some deprecation warnings for af_xdp driver with libbpf >= v0.7.0. See `mailing list <https://mails.dpdk.org/archives/dev/2022-December/257961.html>`__ for more details.

21.11.4 Release Notes
---------------------


21.11.4 Fixes
~~~~~~~~~~~~~

* acl: fix crash on PPC64 with GCC 11
* app/bbdev: check statistics failure
* app/compress-perf: fix remaining data for ops
* app/compress-perf: fix some typos
* app/compress-perf: fix testing single operation
* app/crypto-perf: fix IPsec direction
* app/crypto-perf: fix number of segments
* app/crypto-perf: fix SPI zero
* app/crypto-perf: fix test file memory leak
* app/dumpcap: fix storing port identifier
* app/flow-perf: fix division or module by zero
* app/testpmd: fix crash on cleanup
* app/testpmd: fix forwarding stats for Tx dropped
* app/testpmd: fix interactive mode with no ports
* app/testpmd: fix link check condition on port start
* app/testpmd: fix packet count in IEEE 1588 engine
* app/testpmd: fix packet transmission in noisy VNF engine
* app/testpmd: fix secondary process packet forwarding
* app/testpmd: fix Tx preparation in checksum engine
* baseband/acc: fix acc100 iteration counter in TB
* baseband/acc: fix memory leak on acc100 close
* build: detect backtrace availability
* build: fix dependencies lookup
* build: fix toolchain definition
* bus/ifpga: fix devargs handling
* ci: switch to Ubuntu 20.04
* cmdline: handle EOF as quit
* cmdline: make rdline status not private
* common/cnxk: add memory clobber to steor and ldeor
* common/cnxk: fix auth key length
* common/cnxk: fix dual VLAN parsing
* common/sfc_efx/base: add MAE mark reset action
* compressdev: fix empty devargs parsing
* compressdev: fix end of driver list
* compress/mlx5: fix decompress xform validation
* compress/mlx5: fix output Adler-32 checksum offset
* compress/mlx5: fix queue setup for partial transformations
* crypto/ccp: fix IOVA handling
* crypto/ccp: remove some dead code for UIO
* crypto/ccp: remove some printf
* cryptodev: fix empty devargs parsing
* cryptodev: fix telemetry data truncation
* crypto/qat: fix stream cipher direction
* devtools: fix escaped space in grep pattern
* dma/ioat: fix device stop if no copies done
* dma/ioat: fix error reporting on restart
* dma/ioat: fix indexes after restart
* dma/skeleton: fix empty devargs parsing
* doc: add gpudev to the Doxygen index
* doc: fix dependency setup in l2fwd-cat example guide
* doc: fix description of L2TPV2 flow item
* doc: fix LPM support in l3forward guide
* doc: fix pipeline example path in user guide
* doc: fix reference to event timer header
* eal: cleanup alarm and hotplug before memory detach
* eal/freebsd: fix lock in alarm callback
* eal/linux: fix hugetlbfs sub-directories discovery
* eal: use same atomic intrinsics for GCC and clang
* eal/windows: fix pedantic build
* ethdev: fix build with LTO
* ethdev: fix telemetry data truncation
* ethdev: remove telemetry Rx mbuf alloc failed field
* event/cnxk: fix burst timer arm
* event/cnxk: fix SSO cleanup
* event/cnxk: fix timer operations in secondary process
* event/cnxk: wait for CPT flow control on WQE path
* eventdev/eth_tx: fix devices loop
* eventdev/timer: fix overflow
* examples/cmdline: fix build with GCC 12
* examples/ipsec-secgw: fix auth IV length
* examples/qos_sched: fix config entries in wrong sections
* examples/qos_sched: fix debug mode
* examples/qos_sched: fix Tx port config when link down
* fbarray: fix metadata dump
* gpudev: fix deadlocks when registering callback
* graph: fix node shrink
* hash: fix GFNI implementation build with GCC 12
* kni: fix build on RHEL 9.1
* kni: fix possible starvation when mbufs are exhausted
* kvargs: add API documentation for process callback
* mailmap: add list of contributors
* mem: fix heap ID in telemetry
* mem: fix hugepage info mapping
* mem: fix telemetry data truncation
* mempool: fix telemetry data truncation
* net/af_xdp: squash deprecated-declaration warnings
* net/bnxt: fix link state change interrupt config
* net/bnxt: fix RSS hash in mbuf
* net/bnxt: fix Rx queue stats after queue stop and start
* net/bnxt: fix Tx queue stats after queue stop and start
* net/cnxk: fix LBK BPID usage
* net/e1000: fix saving of stripped VLAN TCI
* net/hns3: add debug info for Rx/Tx dummy function
* net/hns3: add verification of RSS types
* net/hns3: allow adding queue buffer size hash rule
* net/hns3: declare flow rule keeping capability
* net/hns3: extract common functions to set Rx/Tx
* net/hns3: extract common function to query device
* net/hns3: fix burst mode query with dummy function
* net/hns3: fix clearing RSS configuration
* net/hns3: fix config struct used for conversion
* net/hns3: fix duplicate RSS rule check
* net/hns3: fix empty devargs parsing
* net/hns3: fix inaccurate RTC time to read
* net/hns3: fix log about indirection table size
* net/hns3: fix possible truncation of hash key when config
* net/hns3: fix possible truncation of redirection table
* net/hns3: fix RSS key size compatibility
* net/hns3: fix warning on flush or destroy rule
* net/hns3: make getting Tx function static
* net/hns3: refactor set RSS hash algorithm and key interface
* net/hns3: reimplement hash flow function
* net/hns3: remove debug condition for Tx prepare
* net/hns3: remove unused structures
* net/hns3: remove useless code when destroy valid RSS rule
* net/hns3: save hash algo to RSS filter list node
* net/hns3: separate flow RSS config from RSS conf
* net/hns3: separate setting and clearing RSS rule
* net/hns3: separate setting hash algorithm
* net/hns3: separate setting hash key
* net/hns3: separate setting redirection table
* net/hns3: separate setting RSS types
* net/hns3: separate Tx prepare from getting Tx function
* net/hns3: use hardware config to report hash key
* net/hns3: use hardware config to report hash types
* net/hns3: use hardware config to report redirection table
* net/hns3: use new RSS rule to configure hardware
* net/hns3: use RSS filter list to check duplicated rule
* net/i40e: fix AVX512 fast-free path
* net/i40e: fix MAC loopback on X722
* net/i40e: fix validation of flow transfer attribute
* net/i40e: reduce interrupt interval in multi-driver mode
* net/iavf: add lock for VF commands
* net/iavf: fix building data desc
* net/iavf: fix device stop during reset
* net/iavf: fix VLAN offload with AVX2
* net/iavf: protect insertion in flow list
* net/ice: fix validation of flow transfer attribute
* net/ipn3ke: fix representor name
* net/ipn3ke: fix thread exit
* net/ixgbe: enable IPv6 mask in flow rules
* net/ixgbe: fix firmware version consistency
* net/ixgbe: fix IPv6 mask in flow director
* net/mlx5: check compressed CQE opcode in vectorized Rx
* net/mlx5: fix build with GCC 12 and ASan
* net/mlx5: fix CQE dump for Tx
* net/mlx5: fix error CQE dumping for vectorized Rx
* net/mlx5: fix flow sample with ConnectX-5
* net/mlx5: fix hairpin Tx queue reference count
* net/mlx5: fix sysfs port name translation
* net/mlx5: fix Windows build with MinGW GCC 12
* net/mlx5: ignore non-critical syndromes for Rx queue
* net/nfp: fix firmware name derived from PCI name
* net/nfp: fix getting RSS configuration
* net/nfp: fix MTU configuration order
* net/ngbe: fix packet type to parse from offload flags
* net/sfc: enforce fate action in transfer flow rules
* net/sfc: export pick transfer proxy callback to representors
* net/sfc: fix MAC address entry leak in transfer flow parsing
* net/sfc: fix resetting mark in tunnel offload switch rules
* net/sfc: invalidate switch port entry on representor unplug
* net/txgbe: fix default signal quality value for KX/KX4
* net/txgbe: fix interrupt loss
* net/txgbe: fix packet type to parse from offload flags
* net/txgbe: fix Rx buffer size in config register
* net/vhost: add missing newline in logs
* net/vhost: fix leak in interrupt handle setup
* net/vhost: fix Rx interrupt
* net/virtio: deduce IP length for TSO checksum
* net/virtio: fix empty devargs parsing
* net/virtio: remove address width limit for modern devices
* net/virtio-user: fix device starting failure handling
* pdump: fix build with GCC 12
* raw/ifpga/base: fix init with multi-process
* raw/skeleton: fix empty devargs parsing
* raw/skeleton: fix selftest
* regex/mlx5: fix doorbell record
* regex/mlx5: utilize all available queue pairs
* reorder: fix sequence number mbuf field register
* reorder: invalidate buffer from ready queue in drain
* sched: fix alignment of structs in subport
* table: fix action selector group size log2 setting
* telemetry: fix repeat display when callback don't init dict
* telemetry: move include after guard
* test/bbdev: extend HARQ tolerance
* test/bbdev: fix crash for non supported HARQ length
* test/bbdev: remove check for invalid opaque data
* test/crypto: add missing MAC-I to PDCP vectors
* test/crypto: fix capability check for ZUC cipher-auth
* test/crypto: fix statistics error messages
* test/crypto: fix typo in AES test
* test/crypto: fix ZUC digest length in comparison
* test: fix segment length in packet generator
* test/mbuf: fix mbuf reset test
* test/mbuf: fix test with mbuf debug enabled
* test/reorder: fix double free of drained buffers
* vdpa/ifc: fix argument compatibility check
* vdpa/ifc: fix reconnection in SW-assisted live migration
* version: 21.11.4-rc1
* vhost: decrease log level for unimplemented requests
* vhost: fix net header settings in datapath
* vhost: fix OOB access for invalid vhost ID
* vhost: fix possible FD leaks
* vhost: fix possible FD leaks on truncation

21.11.4 Validation
~~~~~~~~~~~~~~~~~~

* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2023-April/043590.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e)
      * PF (ixgbe)
      * PF (ice)
      * VF (i40e)
      * VF (ixgbe)
      * VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * Power and IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2023-April/043578.html>`__

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
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-5
   * ConnectX-4 Lx
   * BlueField-2


* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2023-April/043572.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 6.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing

21.11.4 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.4 contains fixes up to DPDK 23.03
* Issues identified/fixed in DPDK main branch after DPDK 23.03 may be present in DPDK 21.11.4
* Intel validation team reported a performance issue for a specific test on a specific platform, Intel(R) Xeon(R) Platinum 8280M CPU @ 2.70GHz CPU. Other tests and other platforms do not have this performance issue. See `mailing list <https://mails.dpdk.org/archives/stable/2023-May/043729.html>`__ for more details.

21.11.5 Release Notes
---------------------


21.11.5 Fixes
~~~~~~~~~~~~~

* app/crypto-perf: fix socket ID default value
* app/testpmd: fix checksum engine with GTP on 32-bit
* app/testpmd: fix GTP L2 length in checksum engine
* baseband/fpga_5gnr_fec: fix possible division by zero
* baseband/fpga_5gnr_fec: fix starting unconfigured queue
* build: fix case of project language name
* ci: fix libabigail cache in GHA
* common/cnxk: fix inline device VF identification
* common/cnxk: fix IPsec IPv6 tunnel address byte swap
* common/iavf: fix MAC type for 710 NIC
* common/mlx5: adjust fork call with new kernel API
* common/qat: detach crypto from compress build
* common/sfc_efx/base: fix Rx queue without RSS hash prefix
* crypto/ipsec_mb: fix enqueue counter for SNOW3G
* crypto/ipsec_mb: optimize allocation in session
* crypto/openssl: skip workaround at compilation time
* crypto/scheduler: fix last element for valid args
* doc: fix auth algos in cryptoperf app
* doc: fix event timer adapter guide
* doc: fix format in flow API guide
* doc: fix kernel patch link in hns3 guide
* doc: fix number of leading spaces in hns3 guide
* doc: fix syntax in hns3 guide
* doc: fix typo in cnxk platform guide
* doc: fix typo in graph guide
* doc: fix typos and wording in flow API guide
* doc: remove warning with Doxygen 1.9.7
* doc: update BIOS settings and supported HW for NTB
* eal: avoid calling cleanup twice
* eal/linux: fix legacy mem init with many segments
* eal/linux: fix secondary process crash for mp hotplug requests
* ethdev: check that at least one FEC mode is specified
* ethdev: fix indirect action conversion
* ethdev: fix MAC address occupies two entries
* ethdev: fix potential leak in PCI probing helper
* ethdev: update documentation for API to get FEC
* ethdev: update documentation for API to set FEC
* event/cnxk: fix nanoseconds to ticks conversion
* eventdev/timer: fix buffer flush
* eventdev/timer: fix timeout event wait behavior
* event/dsw: free rings on close
* examples/fips_validation: fix digest length in AES-GCM
* examples/ip_pipeline: fix build with GCC 13
* examples/ipsec-secgw: fix TAP default MAC address
* examples/l2fwd-cat: fix external build
* examples/ntb: fix build with GCC 13
* fib: fix adding default route
* hash: fix reading unaligned bits in Toeplitz hash
* ipc: fix file descriptor leakage with unhandled messages
* ipsec: fix NAT-T header length
* kernel/freebsd: fix function parameter list
* kni: fix build with Linux 6.3
* kni: fix build with Linux 6.5
* mbuf: fix Doxygen comment of distributor metadata
* mem: fix memsegs exhausted message
* net/bonding: fix destroy dedicated queues flow
* net/bonding: fix startup when NUMA is not supported
* net/cnxk: fix cookies check with security offload
* net/cnxk: fix flow queue index validation
* net/cnxk: flush SQ before configuring MTU
* net/dpaa2: fix checksum good flags
* net/e1000: fix queue number initialization
* net/e1000: fix Rx and Tx queue status
* net/hns3: delete duplicate macro definition
* net/hns3: extract PTP to its own header file
* net/hns3: fix build warning
* net/hns3: fix device start return value
* net/hns3: fix FEC mode check
* net/hns3: fix FEC mode for 200G ports
* net/hns3: fix IMP reset trigger
* net/hns3: fix inaccurate log
* net/hns3: fix index to look up table in NEON Rx
* net/hns3: fix mbuf leakage when RxQ started after reset
* net/hns3: fix mbuf leakage when RxQ started during reset
* net/hns3: fix missing FEC capability
* net/hns3: fix never set MAC flow control
* net/hns3: fix non-zero weight for disabled TC
* net/hns3: fix redundant line break in log
* net/hns3: fix RTC time after reset
* net/hns3: fix RTC time on initialization
* net/hns3: fix Rx multiple firmware reset interrupts
* net/hns3: fix uninitialized variable
* net/hns3: fix variable type mismatch
* net/hns3: get FEC capability from firmware
* net/hns3: uninitialize PTP
* net/i40e: fix comments
* net/i40e: fix Rx data buffer size
* net/i40e: fix tunnel packet Tx descriptor
* net/iavf: fix abnormal disable HW interrupt
* net/iavf: fix Rx data buffer size
* net/iavf: fix stop ordering
* net/iavf: fix tunnel TSO path selection
* net/iavf: fix VLAN insertion in vector path
* net/iavf: fix VLAN offload with AVX512
* net/iavf: release large VF when closing device
* net/ice: adjust timestamp mbuf register
* net/ice/base: remove unreachable code
* net/ice: fix 32-bit build
* net/ice: fix DCF control thread crash
* net/ice: fix DCF RSS initialization
* net/ice: fix outer UDP checksum offload
* net/ice: fix protocol agnostic offloading with big packets
* net/ice: fix RSS hash key generation
* net/ice: fix Rx data buffer size
* net/ice: fix statistics
* net/ice: fix timestamp enabling
* net/ice: fix tunnel packet Tx descriptor
* net/ice: fix VLAN mode parser
* net/ice: initialize parser for double VLAN
* net/igc: fix Rx and Tx queue status
* net/ixgbe: add proper memory barriers in Rx
* net/ixgbe: fix Rx and Tx queue status
* net/mlx5: enhance error log for tunnel offloading
* net/mlx5: fix device removal event handling
* net/mlx5: fix drop action attribute validation
* net/mlx5: fix drop action memory leak
* net/mlx5: fix duplicated tag index matching in SWS
* net/mlx5: fix flow dump for modify field
* net/mlx5: fix flow workspace destruction
* net/mlx5: fix LRO TCP checksum
* net/mlx5: fix risk in NEON Rx descriptor read
* net/mlx5: fix validation for conntrack indirect action
* net/mlx5: forbid MPRQ restart
* net/netvsc: fix sizeof calculation
* net/nfp: fix address always related with PF ID 0
* net/nfp: fix offloading flows
* net/ngbe: fix extended statistics
* net/ngbe: fix RSS offload capability
* net/qede: fix RSS indirection table initialization
* net/sfc: invalidate dangling MAE flow action FW resource IDs
* net/sfc: stop misuse of Rx ingress m-port metadata on EF100
* net/tap: set locally administered bit for fixed MAC address
* net/txgbe/base: fix Tx with fiber hotplug
* net/txgbe: fix extended statistics
* net/txgbe: fix interrupt enable mask
* net/txgbe: fix to set autoneg for 1G speed
* net/txgbe: fix use-after-free on remove
* net/virtio: fix initialization to return negative errno
* net/virtio: propagate interrupt configuration error values
* net/virtio-user: fix leak when initialisation fails
* net/vmxnet3: fix drop of empty segments in Tx
* net/vmxnet3: fix return code in initializing
* pci: fix comment referencing renamed function
* pipeline: fix double free for table stats
* ring: fix dequeue parameter name
* ring: fix use after free
* telemetry: fix autotest on Alpine
* test: add graph tests
* test/bonding: fix include of standard header
* test/crypto: fix PDCP-SDAP test vectors
* test/crypto: fix return value for SNOW3G
* test/crypto: fix session creation check
* test/malloc: fix missing free
* test/malloc: fix statistics checks
* test/mbuf: fix crash in a forked process
* version: 21.11.5-rc1
* vfio: fix include with musl runtime
* vhost: fix invalid call FD handling

21.11.5 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2023-August/045101.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 6.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2023-August/045124.html>`__

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
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-5
   * ConnectX-4 Lx
   * BlueField-2


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2023-August/045177.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e)
      * PF (ixgbe)
      * PF (ice)
      * VF (i40e)
      * VF (ixgbe)
      * VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * Power and IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance

21.11.5 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.5 contains fixes up to DPDK 23.07
* Issues identified/fixed in DPDK main branch after DPDK 23.07 may be present in DPDK 21.11.5

21.11.5 Fixes skipped and status unresolved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* c5b531d6ee  app/crypto-perf: fix session freeing
* 04dac73643  eventdev/crypto: fix enqueue count
* 4b04134cbb  eventdev/crypto: fix failed events
* da73a2a0d1  eventdev/crypto: fix offset used while flushing events
* f442c04001  eventdev/crypto: fix overflow in circular buffer
* 5a0f64d84b  net/cnxk: fix configuring large Rx/Tx queues
* 59ceaa72d5  common/cnxk: fix part number for CN10K
* 31a28a99fd  net/ngbe: add spinlock protection on YT PHY
* 5781638519  common/cnxk: fix RQ mask config for CN10KB chip
* 3fe71706ab  event/cnxk: fix stale data in workslot
* 927cb43fe9  examples/l3fwd: fix port group mask with AltiVec
* 0f044b6681  net/iavf: fix refine protocol header
* 0b241667cc  net/iavf: fix tainted scalar
* b125c0e721  net/iavf: fix tainted scalar
* cedb44dc87  common/mlx5: improve AES-XTS tweak capability check
* 0fd1386c30  app/testpmd: cleanup cleanly from signal
* f1d0993e03  app/testpmd: fix interactive mode on Windows
* 7be74edb90  common/mlx5: use just sufficient barrier for Arm
* 7bdf7a13ae  app/testpmd: fix encap/decap size calculation
* d2d7f0190b  doc: fix code blocks in cryptodev guide
* 7e7b6762ea  eal: enhance NUMA affinity heuristic
* e97738919c  net/nfp: fix Tx descriptor free logic of NFD3
* ebc352c77f  net/mlx5: fix matcher layout size calculation
* ad4d51d277  net/mlx5: forbid duplicated tag index in pattern template
* 6df1bc6b3b  mempool/cnxk: avoid hang when counting batch allocs
* 772e30281a  common/cnxk: fix CPT backpressure disable on LBK
* b37fe88a2c  event/cnxk: fix LMTST write for single event mode
* 92a16af450  net/iavf: fix virtchnl command called in interrupt
* 12011b11a3  net/txgbe: adapt to MNG veto bit setting
* 21f702d556  net/ngbe: fix link status in no LSC mode
* 659cfce01e  net/ngbe: remove redundant codes
* 6fd3a7a618  net/ice/base: fix internal etype in switch filter
* 9749dffe23  net/ice: fix MAC type of E822 and E823
* 1c7a4d37e7  common/cnxk: fix mailbox timeout due to deadlock
* 5e170dd8b6  net/txgbe: fix blocking system events
* 37ca457d3f  common/mlx5: fix obtaining IB device in LAG mode
* 8c047e823a  net/bnxt: fix multi-root card support
* 8b4618a7b4  crypto/qat: fix null algorithm digest placement
* 9a518054b5  Ahmad  examples/l3fwd: fix duplicate expression for default nexthop
* e6479f009f  net/mlx5: fix MPRQ stride size for headroom

21.11.6 Release Notes
---------------------


21.11.6 Fixes
~~~~~~~~~~~~~

* app/bbdev: fix link with NXP LA12XX
* app/dumpcap: allow multiple invocations
* app/dumpcap: fix mbuf pool ring type
* app/pipeline: add sigint handler
* app/procinfo: adjust format of RSS info
* app/procinfo: fix RSS info
* app/procinfo: remove unnecessary rte_malloc
* app/testpmd: add explicit check for tunnel TSO
* app/testpmd: fix help string
* app/testpmd: fix primary process not polling all queues
* app/testpmd: fix tunnel TSO capability check
* app/testpmd: fix tunnel TSO configuration
* app/testpmd: remove useless check in TSO command
* baseband/acc: fix ACC100 HARQ input alignment
* bus/dpaa: fix build with asserts for GCC 13
* bus/pci: fix device ID log
* common/cnxk: fix aura disable handling
* common/cnxk: fix default flow action setting
* common/cnxk: fix different size bit operations
* common/cnxk: fix DPI memzone name
* common/cnxk: fix pool buffer size in opaque mode
* common/cnxk: fix xstats for different packet sizes
* common/cnxk: remove dead Meson code
* common/mlx5: fix controller index parsing
* config/arm: fix aarch32 build with GCC 13
* cryptodev: add missing doc for security context
* crypto/ipsec_mb: add dependency check for cross build
* crypto/nitrox: fix panic with high number of segments
* crypto/qat: fix NULL algorithm digest placement
* crypto/qat: fix raw API null algorithm digest
* dma/cnxk: fix device state
* doc: fix hns3 build option about max queue number
* doc: fix RSS flow description in hns3 guide
* doc: fix some ordered lists
* doc: remove number of commands in vDPA guide
* doc: remove restriction on ixgbe vector support
* doc: replace code blocks with includes in security guide
* doc: update features in hns3 guide
* doc: update versions recommendations for i40e and ice
* eal/unix: fix firmware reading with external xz helper
* eal/windows: fix build with recent MinGW
* ethdev: account for smaller MTU when setting default
* ethdev: fix 32-bit build with GCC 13
* ethdev: fix ESP packet type description
* ethdev: fix function name in comment
* event/cnxk: fix getwork mode devargs parsing
* event/cnxk: fix return values for capability API
* eventdev/eth_rx: fix timestamp field register in mbuf
* eventdev: fix device pointer for vdev-based devices
* eventdev: fix missing driver names in info struct
* eventdev: fix symbol export for port maintenance
* event/dlb2: fix disable PASID
* event/dlb2: fix missing queue ordering capability flag
* event/dlb2: fix name check in self-test
* event/sw: fix ordering corruption with op release
* event/sw: remove obsolete comment
* examples/ethtool: fix pause configuration
* examples/ipsec-secgw: fix partial overflow
* fib6: fix adding default route as first route
* fib: fix adding default route overwriting entire table
* hash: align SSE lookup to scalar implementation
* malloc: remove return from void functions
* mempool: clarify enqueue/dequeue ops documentation
* mempool/cnxk: fix free from non-EAL threads
* mempool: fix default ops for an empty mempool
* mempool: fix get function documentation
* meter: fix RFC4115 trTCM API Doxygen
* net/af_packet: fix Rx and Tx queue state
* net/af_xdp: fix Rx and Tx queue state
* net/af_xdp: make compatible with libbpf 0.8.0
* net/avp: fix Rx and Tx queue state
* net/bnx2x: fix Rx and Tx queue state
* net/bnxt: fix Rx and Tx queue state
* net/bonding: fix header for C++
* net/bonding: fix link status callback stop
* net/bonding: fix possible overrun
* net/bonding: fix Rx and Tx queue state
* net/cnxk: fix uninitialized variable
* net/cnxk: fix uninitialized variable
* net/cxgbe: fix Rx and Tx queue state
* net/dpaa2: fix Rx and Tx queue state
* net/dpaa: fix Rx and Tx queue state
* net/e1000: fix Rx and Tx queue state
* net/ena: fix Rx and Tx queue state
* net/enetc: fix Rx and Tx queue state
* net/enic: avoid extra unlock in MTU set
* net/enic: fix Rx and Tx queue state
* net/hinic: fix Rx and Tx queue state
* net/hns3: extract common function to obtain revision ID
* net/hns3: fix crash for NEON and SVE
* net/hns3: fix double stats for IMP and global reset
* net/hns3: fix error code for multicast resource
* net/hns3: fix flushing multicast MAC address
* net/hns3: fix ignored reset event
* net/hns3: fix IMP or global reset
* net/hns3: fix LRO offload to report
* net/hns3: fix mailbox sync
* net/hns3: fix multiple reset detected log
* net/hns3: fix order in NEON Rx
* net/hns3: fix reset event status
* net/hns3: fix setting DCB capability
* net/hns3: fix some error logs
* net/hns3: fix some return values
* net/hns3: fix traffic management thread safety
* net/hns3: fix typo in function name
* net/hns3: fix unchecked Rx free threshold
* net/hns3: fix uninitialized hash algo value
* net/hns3: fix VF default MAC modified when set failed
* net/hns3: fix VF reset handler interruption
* net/hns3: keep set/get algo key functions local
* net/hns3: refactor interrupt state query
* net/hns3: remove reset log in secondary
* net/i40e: fix buffer leak on Rx reconfiguration
* net/i40e: fix FDIR queue receives broadcast packets
* net/iavf: fix checksum offloading
* net/iavf: fix ESN session update
* net/iavf: fix indent in Tx path
* net/iavf: fix port stats clearing
* net/iavf: fix TSO with big segments
* net/iavf: fix Tx debug
* net/iavf: fix Tx offload flags check
* net/iavf: fix Tx offload mask
* net/iavf: fix Tx preparation
* net/iavf: fix VLAN offload strip flag
* net/iavf: remove log from Tx prepare function
* net/iavf: unregister interrupt handler before FD close
* net/ice: fix crash on closing representor ports
* net/ice: fix DCF port statistics
* net/ice: fix initial link status
* net/ice: fix L1 check interval
* net/ice: fix TM configuration clearing
* net/ice: fix TSO with big segments
* net/ice: fix Tx preparation
* net/ice: remove log from Tx prepare function
* net/ice: write timestamp to first segment in scattered Rx
* net/ipn3ke: fix Rx and Tx queue state
* net/memif: fix Rx and Tx queue state
* net/mlx4: fix Rx and Tx queue state
* net/mlx5: fix decap action checking in sample flow
* net/mlx5: fix E-Switch mirror flow rule validation
* net/mlx5: fix hairpin queue states
* net/mlx5: fix hairpin queue unbind
* net/mlx5: fix leak in sysfs port name translation
* net/mlx5: fix matcher layout size calculation
* net/mlx5: fix MPRQ stride size to accommodate the headroom
* net/mlx5: fix multi-segment Tx inline data length
* net/mlx5: fix shared Rx queue list management
* net/mlx5: fix use after free on Rx queue start
* net/mlx5: fix validation of sample encap flow action
* net/mlx5: zero UDP checksum over IPv4 in encapsulation
* net/mvneta: fix Rx and Tx queue state
* net/mvpp2: fix Rx and Tx queue state
* net/netvsc: increase VSP response timeout to 60 seconds
* net/nfp: fix DMA error after abnormal exit
* net/nfp: fix link status interrupt
* net/nfp: fix reconfigure logic in PF initialization
* net/nfp: fix reconfigure logic in VF initialization
* net/nfp: fix reconfigure logic of set MAC address
* net/nfp: fix Tx descriptor free logic of NFD3
* net/ngbe: check process type in close operation
* net/ngbe: fix flow control
* net/ngbe: fix Rx and Tx queue state
* net/ngbe: keep link down after device close
* net/ngbe: reconfigure MAC Rx when link update
* net/null: fix Rx and Tx queue state
* net/octeon_ep: fix Rx and Tx queue state
* net/pfe: fix Rx and Tx queue state
* net/ring: fix Rx and Tx queue state
* net/sfc: account for data offset on Tx
* net/sfc: add missing error code indication to MAE init path
* net/sfc: fix Rx and Tx queue state
* net/sfc: remove null dereference in log
* net/sfc: set max Rx packet length for representors
* net/softnic: fix Rx and Tx queue state
* net/tap: fix IPv4 checksum offloading
* net/tap: fix L4 checksum offloading
* net/tap: fix RSS for fragmented packets
* net/tap: use MAC address parse API instead of local parser
* net/txgbe: add Tx queue maximum limit
* net/txgbe: check process type in close operation
* net/txgbe: fix GRE tunnel packet checksum
* net/txgbe: fix out of bound access
* net/txgbe: fix Rx and Tx queue state
* net/txgbe: keep link down after device close
* net/txgbe: reconfigure MAC Rx when link update
* net/vhost: fix Rx and Tx queue state
* net/virtio: fix descriptor addresses in 32-bit build
* net/virtio: fix link state interrupt vector setting
* net/virtio: fix missing next flag in Tx packed ring
* net/virtio: fix Rx and Tx queue state
* net/vmxnet3: fix Rx and Tx queue state
* pdump: fix error number on IPC response
* random: initialize state for unregistered non-EAL threads
* rawdev: fix device class in log message
* Revert "net/iavf: fix abnormal disable HW interrupt"
* Revert "net/iavf: fix tunnel TSO path selection"
* test/bbdev: assert failed test for queue configure
* test/bbdev: fix Python script subprocess
* test/bonding: add missing check
* test/bonding: fix uninitialized RSS configuration
* test/bonding: remove unreachable statement
* test/crypto: fix IV in some vectors
* test/crypto: fix typo in asym tests
* test/crypto: skip some synchronous tests with CPU crypto
* test/event: fix crypto null device creation
* test/hash: fix creation error log
* version: 21.11.6-rc1
* vhost: fix check on virtqueue access in async registration
* vhost: fix check on virtqueue access in in-flight getter
* vhost: fix missing check on virtqueue access
* vhost: fix missing lock protection in power monitor API
* vhost: fix missing vring call check on virtqueue access

21.11.6 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2023-December/046925.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 7.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2024-January/046939.html>`__

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
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-7
   * BlueField-2


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2024-January/047001.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e, ixgbe)
      * VF (i40e, ixgbe)
      * PF/VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance

21.11.6 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.6 contains fixes up to DPDK 23.11
* Issues identified/fixed in DPDK main branch after DPDK 23.11 may be present in DPDK 21.11.6

21.11.6 Fixes skipped and status unresolved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* c5b531d6ee app/crypto-perf: fix session freeing
* 61b52e7edb app/test: fix reference to master in bonding test
* 0fd1386c30 app/testpmd: cleanup cleanly from signal
* a996cd04ae app/testpmd: fix early exit from signal
* 7bdf7a13ae app/testpmd: fix encap/decap size calculation
* f1d0993e03 app/testpmd: fix interactive mode on Windows
* 5d8c1f6253 common/cnxk: check error in MAC address set
* a6f639e079 common/cnxk: fix BP threshold calculation
* 772e30281a common/cnxk: fix CPT backpressure disable on LBK
* 2e9863fbb6 common/cnxk: fix different size bit operations
* 4f6f36ce23 common/cnxk: fix leak in error path
* 1c7a4d37e7 common/cnxk: fix mailbox timeout due to deadlock
* 59ceaa72d5 common/cnxk: fix part number for CN10K
* 5781638519 common/cnxk: fix RQ mask config for CN10KB chip
* 56fa6f92e9 common/cnxk: fix RSS key configuration
* 37ca457d3f common/mlx5: fix obtaining IB device in LAG mode
* cedb44dc87 common/mlx5: improve AES-XTS tweak capability check
* 7be74edb90 common/mlx5: use just sufficient barrier for Arm
* 9d91c3047d crypto/openssl: fix memory leaks in asym operations
* 3dd3115078 dma/cnxk: fix chunk buffer failure return code
* ba39a261a7 dma/cnxk: fix completion ring tail wrap
* 95a955e3e0 dma/cnxk: fix device reconfigure
* 694e8e643d event/cnxk: fix CASP usage for clang
* b37fe88a2c event/cnxk: fix LMTST write for single event mode
* 3fe71706ab event/cnxk: fix stale data in workslot
* 04dac73643 eventdev/crypto: fix enqueue count
* 4b04134cbb eventdev/crypto: fix failed events
* da73a2a0d1 eventdev/crypto: fix offset used while flushing events
* f442c04001 eventdev/crypto: fix overflow in circular buffer
* 9a518054b5 examples/l3fwd: fix duplicate expression for default nexthop
* 927cb43fe9 examples/l3fwd: fix port group mask with AltiVec
* 6df1bc6b3b mempool/cnxk: avoid hang when counting batch allocs
* b3ddd649ad mempool/cnxk: fix alloc from non-EAL threads
* 0236016c02 net/axgbe: fix Rx and Tx queue state
* 5a0f64d84b net/cnxk: fix configuring large Rx/Tx queues
* 7752f14026 net/cnxk: fix Rx flush on CN10k
* 0f044b6681 net/iavf: fix refine protocol header
* 0b241667cc net/iavf: fix tainted scalar
* b125c0e721 net/iavf: fix tainted scalar
* 92a16af450 net/iavf: fix virtchnl command called in interrupt
* 6fd3a7a618 net/ice/base: fix internal etype in switch filter
* 9749dffe23 net/ice: fix MAC type of E822 and E823
* 2ece3b7186 net/mlx5: fix flow workspace double free in Windows
* c46216e77a net/nfp: fix Rx and Tx queue state
* a74c5001e9 net/ngbe: add proper memory barriers in Rx
* 31a28a99fd net/ngbe: add spinlock protection on YT PHY
* 21f702d556 net/ngbe: fix link status in no LSC mode
* 659cfce01e net/ngbe: remove redundant codes
* 12011b11a3 net/txgbe: adapt to MNG veto bit setting
* 5e170dd8b6 net/txgbe: fix blocking system events
* 166591931b pcapng: modify timestamp calculation
* 63bf81a617 test: fix named test macro

21.11.7 Release Notes
---------------------


21.11.7 Fixes
~~~~~~~~~~~~~

* app/crypto-perf: add missing op resubmission
* app/crypto-perf: fix data comparison
* app/crypto-perf: fix encrypt operation verification
* app/crypto-perf: fix next segment mbuf
* app/crypto-perf: fix out-of-place mbuf size
* app/crypto-perf: verify strdup return
* app/dumpcap: verify strdup return
* app/pdump: verify strdup return
* app/testpmd: fix crash in multi-process forwarding
* app/testpmd: fix --stats-period option check
* app/testpmd: hide --bitrate-stats in help if disabled
* build: fix linker warnings about undefined symbols
* bus/dpaa: verify strdup return
* bus/fslmc: verify strdup return
* bus/vdev: verify strdup return
* ci: update versions of actions in GHA
* common/cnxk: fix mbox region copy
* common/cnxk: fix mbox struct attributes
* common/cnxk: fix memory leak in CPT init
* common/cnxk: fix possible out-of-bounds access
* common/cnxk: fix RSS RETA configuration
* common/cnxk: fix Tx MTU configuration
* common/mlx5: fix calloc parameters
* common/mlx5: fix duplicate read of general capabilities
* common/sfc_efx/base: use C11 static assert
* config: fix CPU instruction set for cross-build
* cryptodev: remove unused extern variable
* crypto/ipsec_mb: fix incorrectly setting cipher keys
* dma/idxd: verify strdup return
* doc: add --latencystats option in testpmd guide
* doc: add link speeds configuration in features table
* doc: add traffic manager in features table
* doc: fix commands in eventdev test tool guide
* doc: fix configuration in baseband 5GNR driver guide
* doc: fix default IP fragments maximum in programmer guide
* doc: fix typo in packet framework guide
* doc: fix typo in profiling guide
* doc: fix typos in cryptodev overview
* doc: update link to Windows DevX in mlx5 guide
* drivers/net: fix buffer overflow for packet types list
* eal: verify strdup return
* eal/x86: add AMD vendor check for TSC calibration
* ethdev: fix NVGRE encap flow action description
* event/cnxk: fix dequeue timeout configuration
* event/cnxk: verify strdup return
* eventdev: fix Doxygen processing of vector struct
* eventdev: improve Doxygen comments on configure struct
* event/dlb2: remove superfluous memcpy
* examples/ipsec-secgw: fix typo in error message
* examples/l3fwd: fix Rx over not ready port
* examples/packet_ordering: fix Rx with reorder mode disabled
* examples/qos_sched: fix memory leak in args parsing
* hash: remove some dead code
* kernel/freebsd: fix module build on FreeBSD 14
* net: add macros for VLAN metadata parsing
* net/af_xdp: fix leak on XSK configuration failure
* net/af_xdp: fix memzone leak on config failure
* net/axgbe: fix Rx and Tx queue state
* net/bnx2x: fix calloc parameters
* net/bnx2x: fix warnings about memcpy lengths
* net/bnxt: fix 50G and 100G forced speed
* net/bnxt: fix array overflow
* net/bnxt: fix backward firmware compatibility
* net/bnxt: fix deadlock in ULP timer callback
* net/bnxt: fix null pointer dereference
* net/bnxt: fix number of Tx queues being created
* net/bnxt: fix speed change from 200G to 25G on Thor
* net/bnxt: modify locking for representor Tx
* net/cnxk: fix flow RSS configuration
* net/cnxk: fix MTU limit
* net/ena/base: limit exponential backoff
* net/ena/base: restructure interrupt handling
* net/failsafe: fix memory leak in args parsing
* net/hns3: enable PFC for all user priorities
* net/hns3: fix disable command with firmware
* net/hns3: fix reset level comparison
* net/hns3: fix VF multiple count on one reset
* net/hns3: refactor handle mailbox function
* net/hns3: refactor PF mailbox message struct
* net/hns3: refactor send mailbox function
* net/hns3: refactor VF mailbox message struct
* net/hns3: remove QinQ insert support for VF
* net/hns3: support new device
* net/i40e: remove incorrect 16B descriptor read block
* net/i40e: remove redundant judgment in flow parsing
* net/iavf: fix memory leak on security context error
* net/iavf: remove error logs for VLAN offloading
* net/iavf: remove incorrect 16B descriptor read block
* net/ice: fix link update
* net/ice: fix tunnel TSO capabilities
* net/ice: remove incorrect 16B descriptor read block
* net/ionic: fix device close
* net/ionic: fix RSS query
* net/ixgbe: fix memoy leak after device init failure
* net/ixgbe: increase VF reset timeout
* net/ixgbevf: fix RSS init for x550 NICs
* net/memif: fix extra mbuf refcnt update in zero copy Tx
* net/mlx5: fix age position in hairpin split
* net/mlx5: fix counters map in bonding mode
* net/mlx5: fix drop action release timing
* net/mlx5: fix error packets drop in regular Rx
* net/mlx5: fix GENEVE TLV option management
* net/mlx5: fix jump action validation
* net/mlx5: fix stats query crash in secondary process
* net/mlx5: fix use after free when releasing Tx queues
* net/mlx5: fix VLAN handling in meter split
* net/mlx5: fix warning about copy length
* net/mlx5: prevent ioctl failure log flooding
* net/netvsc: fix VLAN metadata parsing
* net/nfp: fix calloc parameters
* net/nfp: fix resource leak for PF initialization
* net/nfp: fix Rx and Tx queue state
* net/tap: do not overwrite flow API errors
* net/tap: fix traffic control handle calculation
* net/virtio: remove duplicate queue xstats
* net/vmxnet3: fix initialization on FreeBSD
* net/vmxnet3: ignore Rx queue interrupt setup on FreeBSD
* regexdev: fix logtype register
* telemetry: fix connected clients count
* telemetry: fix empty JSON dictionaries
* test/cfgfile: fix typo in error messages
* test: do not count skipped tests as executed
* test/event: fix crash in Tx adapter freeing
* test/event: skip test if no driver is present
* test: fix probing in secondary process
* test/mbuf: fix external mbuf case with assert enabled
* test/power: fix typo in error message
* test: verify strdup return
* version: 21.11.7-rc1
* vhost: fix deadlock during vDPA SW live migration
* vhost: fix memory leak in Virtio Tx split path
* vhost: fix virtqueue access check in vhost-user setup

21.11.7 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2024-April/048427.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 8.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2024-April/048450.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e, ixgbe)
      * VF (i40e, ixgbe)
      * PF/VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2024-April/048479.html>`__

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
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-7
   * BlueField-2

21.11.7 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.7 contains fixes up to DPDK 24.03
* Issues identified/fixed in DPDK main branch after DPDK 24.03 may be present in DPDK 21.11.7

21.11.7 Fixes skipped and status unresolved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* c5b531d6ee  app/crypto-perf: fix session freeing
* 61b52e7edb  app/test: fix reference to master in bonding test
* 0fd1386c30  app/testpmd: cleanup cleanly from signal
* a996cd04ae  app/testpmd: fix early exit from signal
* 7bdf7a13ae  app/testpmd: fix encap/decap size calculation
* 461c287ab5  app/testpmd: fix GRO packets flush on timeout
* f1d0993e03  app/testpmd: fix interactive mode on Windows
* 5d8c1f6253  common/cnxk: check error in MAC address set
* a6f639e079  common/cnxk: fix BP threshold calculation
* 772e30281a  common/cnxk: fix CPT backpressure disable on LBK
* 48054ca384  common/cnxk: fix link config for SDP
* 1c7a4d37e7  common/cnxk: fix mailbox timeout due to deadlock
* 59ceaa72d5  common/cnxk: fix part number for CN10K
* 5781638519  common/cnxk: fix RQ mask config for CN10KB chip
* 56fa6f92e9  common/cnxk: fix RSS key configuration
* 37ca457d3f  common/mlx5: fix obtaining IB device in LAG mode
* cedb44dc87  common/mlx5: improve AES-XTS tweak capability check
* 7be74edb90  common/mlx5: use just sufficient barrier for Arm
* 9d91c3047d  crypto/openssl: fix memory leaks in asym operations
* 3dd3115078  dma/cnxk: fix chunk buffer failure return code
* ba39a261a7  dma/cnxk: fix completion ring tail wrap
* 95a955e3e0  dma/cnxk: fix device reconfigure
* 694e8e643d  event/cnxk: fix CASP usage for clang
* b37fe88a2c  event/cnxk: fix LMTST write for single event mode
* 3fe71706ab  event/cnxk: fix stale data in workslot
* 04dac73643  eventdev/crypto: fix enqueue count
* 4b04134cbb  eventdev/crypto: fix failed events
* da73a2a0d1  eventdev/crypto: fix offset used while flushing events
* f442c04001  eventdev/crypto: fix overflow in circular buffer
* 9a518054b5  examples/l3fwd: fix duplicate expression for default nexthop
* 927cb43fe9  examples/l3fwd: fix port group mask with AltiVec
* 547f294357  gro: fix reordering of packets
* 6df1bc6b3b  mempool/cnxk: avoid hang when counting batch allocs
* b3ddd649ad  mempool/cnxk: fix alloc from non-EAL threads
* 02a2accb5f  net/bonding: fix flow count query
* 26a6bda9df  net/cnxk: add cookies check for multi-segment offload
* 5a0f64d84b  net/cnxk: fix configuring large Rx/Tx queues
* 3232c95d2c  net/cnxk: fix indirect mbuf handling in Tx
* 8ed5ca4dda  net/cnxk: fix mbuf fields in multi-segment Tx
* 7752f14026  net/cnxk: fix Rx flush on CN10k
* 0f044b6681  net/iavf: fix refine protocol header
* 0b241667cc  net/iavf: fix tainted scalar
* b125c0e721  net/iavf: fix tainted scalar
* 92a16af450  net/iavf: fix virtchnl command called in interrupt
* 6fd3a7a618  net/ice/base: fix internal etype in switch filter
* 9749dffe23  net/ice: fix MAC type of E822 and E823
* 87e4384d26  net/mlx5: fix condition of LACP miss flow
* 2ece3b7186  net/mlx5: fix flow workspace double free in Windows
* 2db234e769  net/mlx5: fix IP-in-IP tunnels recognition
* 1cfb78d2c4  net/mlx5: fix meter policy priority
* 27e44a6f53  net/mlx5: remove duplication of L3 flow item validation
* 48adbc80ba  net/mlx5: remove GENEVE options length limitation
* a74c5001e9  net/ngbe: add proper memory barriers in Rx
* 31a28a99fd  net/ngbe: add spinlock protection on YT PHY
* 21f702d556  net/ngbe: fix link status in no LSC mode
* 659cfce01e  net/ngbe: remove redundant codes
* 44a8635459  net/thunderx: fix DMAC control register update
* 12011b11a3  net/txgbe: adapt to MNG veto bit setting
* 5e170dd8b6  net/txgbe: fix blocking system events
* 166591931b  pcapng: modify timestamp calculation
* df33fb53e4  rcu: fix acked token in debug log
* 063cddfc74  rcu: use atomic operation on acked token
* 63bf81a617  test: fix named test macro

21.11.8 Release Notes
---------------------


21.11.8 Fixes
~~~~~~~~~~~~~

* app/bbdev: fix interrupt tests
* app/dumpcap: handle SIGTERM and SIGHUP
* app/pdump: handle SIGTERM and SIGHUP
* app/testpmd: cleanup cleanly from signal
* app/testpmd: fix build on signed comparison
* app/testpmd: fix early exit from signal
* app/testpmd: fix help string of BPF load command
* app/testpmd: fix interactive mode on Windows
* app/testpmd: fix lcore ID restriction
* app/testpmd: fix outer IP checksum offload
* app/testpmd: fix parsing for connection tracking item
* app/testpmd: handle IEEE1588 init failure
* baseband/la12xx: forbid secondary process
* bpf: fix load hangs with six IPv6 addresses
* bpf: fix MOV instruction evaluation
* buildtools: fix build with clang 17 and ASan
* bus/dpaa: fix bus scan for DMA devices
* bus/dpaa: fix memory leak in bus scan
* bus/dpaa: remove redundant file descriptor check
* bus/pci: fix build with musl 1.2.4 / Alpine 3.19
* bus/pci: fix FD in secondary process
* bus/pci: fix UIO resource mapping in secondary process
* bus/vdev: fix device reinitialization
* common/dpaax/caamflib: fix PDCP AES-AES watchdog error
* common/dpaax/caamflib: fix PDCP-SDAP watchdog error
* common/dpaax: fix IOVA table cleanup
* common/dpaax: fix node array overrun
* common/mlx5: fix unsigned/signed mismatch
* common/mlx5: remove unneeded field when modify RQ table
* config: fix warning for cross build with meson >= 1.3.0
* crypto/cnxk: fix minimal input normalization
* cryptodev: fix build without crypto callbacks
* cryptodev: validate crypto callbacks from next node
* crypto/dpaa2_sec: fix event queue user context
* crypto/openssl: optimize 3DES-CTR context init
* dmadev: fix structure alignment
* dma/idxd: add generic option for queue config
* dma/idxd: add verbose option to config script
* dma/idxd: fix default for workqueue options
* dma/idxd: fix setup with Ubuntu 24.04
* doc: add baseline mode in l3fwd-power guide
* doc: fix mbuf flags
* doc: fix testpmd ring size command
* doc: fix typo in l2fwd-crypto guide
* doc: remove reference to mbuf pkt field
* eal: fix logs for '--lcores'
* eal/linux: lower log level on allocation attempt failure
* eal/unix: support ZSTD compression for firmware
* eal/windows: install sched.h file
* ethdev: fix device init without socket-local memory
* ethdev: fix GENEVE option item conversion
* event/sw: fix warning from useless snprintf
* fbarray: fix finding for unaligned length
* fbarray: fix incorrect lookahead behavior
* fbarray: fix incorrect lookbehind behavior
* fbarray: fix lookahead ignore mask handling
* fbarray: fix lookbehind ignore mask handling
* hash: check name when creating a hash
* hash: fix RCU reclamation size
* hash: fix return code description in Doxygen
* kni: use strscpy
* latencystats: fix literal float suffix
* malloc: fix multi-process wait condition handling
* net/af_packet: align Rx/Tx structs to cache line
* net/af_xdp: count mbuf allocation failures
* net/af_xdp: fix port ID in Rx mbuf
* net/ark: fix index arithmetic
* net/axgbe: check only minimum speed for cables
* net/axgbe: delay AN timeout during KR training
* net/axgbe: disable interrupts during device removal
* net/axgbe: fix connection for SFP+ active cables
* net/axgbe: fix fluctuations for 1G Bel Fuse SFP
* net/axgbe: fix linkup in PHY status
* net/axgbe: fix MDIO access for non-zero ports and CL45 PHYs
* net/axgbe: fix SFP codes check for DAC cables
* net/axgbe: fix Tx flow on 30H HW
* net/axgbe: reset link when link never comes back
* net/axgbe: update DMA coherency values
* net/cnxk: fix outbound security with higher packet burst
* net/cnxk: fix promiscuous state after MAC change
* net/cnxk: fix RSS config
* net/e1000/base: fix link power down
* net/ena: fix bad checksum handling
* net/ena: fix checksum handling
* net/ena: fix return value check
* net: fix outer UDP checksum in Intel prepare helper
* net/fm10k: fix cleanup during init failure
* net/hns3: check Rx DMA address alignmnent
* net/hns3: disable SCTP verification tag for RSS hash input
* net/hns3: fix double free for Rx/Tx queue
* net/hns3: fix offload flag of IEEE 1588
* net/hns3: fix Rx timestamp flag
* net/hns3: fix uninitialized variable in FEC query
* net/hns3: fix variable overflow
* net/i40e: fix outer UDP checksum offload for X710
* net/ice/base: fix board type definition
* net/ice/base: fix check for existing switch rule
* net/ice/base: fix GCS descriptor field offsets
* net/ice/base: fix masking when reading context
* net/ice/base: fix pointer to variable outside scope
* net/ice/base: fix potential TLV length overflow
* net/ice/base: fix return type of bitmap hamming weight
* net/ice/base: fix sign extension
* net/ice/base: fix size when allocating children arrays
* net/ice/base: fix temporary failures reading NVM
* net/ice: fix check for outer UDP checksum offload
* net/ice: fix memory leaks in raw pattern parsing
* net/ice: fix return value for raw pattern parsing
* net/ixgbe/base: fix 5G link speed reported on VF
* net/ixgbe/base: fix PHY ID for X550
* net/ixgbe/base: revert advertising for X550 2.5G/5G
* net/ixgbe: do not create delayed interrupt handler twice
* net/ixgbe: do not update link status in secondary process
* net/mlx5: fix Arm build with GCC 9.1
* net/mlx5: fix end condition of reading xstats
* net/mlx5: fix hash Rx queue release in flow sample
* net/mlx5: fix indexed pool with invalid index
* net/mlx5: fix MTU configuration
* net/mlx5: fix uplink port probing in bonding mode
* net/nfp: fix disabling 32-bit build
* net/ngbe: fix hotplug remove
* net/ngbe: fix memory leaks
* net/ngbe: fix MTU range
* net/ngbe: keep PHY power down while device probing
* net/softnic: fix maybe-uninitialized warning
* net/tap: fix file descriptor check in isolated flow
* net/txgbe: fix flow filters in VT mode
* net/txgbe: fix hotplug remove
* net/txgbe: fix memory leaks
* net/txgbe: fix MTU range
* net/txgbe: fix Rx interrupt
* net/txgbe: fix tunnel packet parsing
* net/txgbe: fix Tx hang on queue disable
* net/txgbe: fix VF promiscuous and allmulticast
* net/txgbe: reconfigure more MAC Rx registers
* net/txgbe: restrict configuration of VLAN strip offload
* net/virtio: fix MAC table update
* net/virtio-user: add memcpy check
* net/vmxnet3: fix init logs
* pcapng: add memcpy check
* telemetry: fix connection parameter parsing
* telemetry: lower log level on socket error
* test/crypto: fix allocation comment
* test/crypto: fix asymmetric capability test
* test/crypto: fix vector global buffer overflow
* test/crypto: remove unused stats in setup
* vdpa/sfc: remove dead code
* version: 21.11.8-rc1
* vhost: cleanup resubmit info before inflight setup
* vhost: fix build with GCC 13

21.11.8 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/stable/2024-September/049938.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 8.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2024-September/049940.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e, ixgbe)
      * VF (i40e, ixgbe)
      * PF/VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance
      * IPsec

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2024-September/049948.html>`__

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
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-7
   * BlueField-2

21.11.8 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.8 contains fixes up to DPDK 24.07
* Issues identified/fixed in DPDK main branch after DPDK 24.07 may be present in DPDK 21.11.8

21.11.8 Fixes skipped and status unresolved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* c5b531d6ee  app/crypto-perf: fix session freeing
* 61b52e7edb  app/test: fix reference to master in bonding test
* 7bdf7a13ae  app/testpmd: fix encap/decap size calculation
* 461c287ab5  app/testpmd: fix GRO packets flush on timeout
* ecf408d2aa  app/testpmd: fix indirect action flush
* 4edbcc7b53  bpf: disable on 32-bit x86
* 4edbcc7b53  bpf: disable on 32-bit x86
* 5d8c1f6253  common/cnxk: check error in MAC address set
* a6f639e079  common/cnxk: fix BP threshold calculation
* 772e30281a  common/cnxk: fix CPT backpressure disable on LBK
* 48054ca384  common/cnxk: fix link config for SDP
* 1c7a4d37e7  common/cnxk: fix mailbox timeout due to deadlock
* 59ceaa72d5  common/cnxk: fix part number for CN10K
* 5781638519  common/cnxk: fix RQ mask config for CN10KB chip
* 56fa6f92e9  common/cnxk: fix RSS key configuration
* 37ca457d3f  common/mlx5: fix obtaining IB device in LAG mode
* cedb44dc87  common/mlx5: improve AES-XTS tweak capability check
* 7be74edb90  common/mlx5: use just sufficient barrier for Arm
* 046341575b  crypto/dpaa_sec: fix IPsec descriptor
* 9d91c3047d  crypto/openssl: fix memory leaks in asym operations
* 17d5bc6135  crypto/openssl: make per-QP auth context clones
* b1d7112602  crypto/openssl: make per-QP cipher context clones
* 3dd3115078  dma/cnxk: fix chunk buffer failure return code
* ba39a261a7  dma/cnxk: fix completion ring tail wrap
* 95a955e3e0  dma/cnxk: fix device reconfigure
* 2a3f42942a  dma/hisilicon: remove support for HIP09 platform
* b9a87346b0  ethdev: fix strict aliasing in link up
* 694e8e643d  event/cnxk: fix CASP usage for clang
* b37fe88a2c  event/cnxk: fix LMTST write for single event mode
* 3fe71706ab  event/cnxk: fix stale data in workslot
* 04dac73643  eventdev/crypto: fix enqueue count
* 4b04134cbb  eventdev/crypto: fix failed events
* da73a2a0d1  eventdev/crypto: fix offset used while flushing events
* ad12d08f05  eventdev/crypto: fix opaque field handling
* f442c04001  eventdev/crypto: fix overflow in circular buffer
* 4b97893816  examples: fix lcore ID restriction
* 548de9091c  examples: fix port ID restriction
* b23c5bd71a  examples: fix queue ID restriction
* 9a518054b5  examples/l3fwd: fix duplicate expression for default nexthop
* 927cb43fe9  examples/l3fwd: fix port group mask with AltiVec
* 547f294357  gro: fix reordering of packets
* 6df1bc6b3b  mempool/cnxk: avoid hang when counting batch allocs
* b3ddd649ad  mempool/cnxk: fix alloc from non-EAL threads
* 97039941b2  net/af_xdp: parse UMEM map info from mempool
* abdabad636  net/af_xdp: remove unused local statistic
* e82b0fe097  net/axgbe: disable RRC for yellow carp devices
* a78a1ed8ba  net/bonding: fix failover time of LACP with mode 4
* 02a2accb5f  net/bonding: fix flow count query
* 26a6bda9df  net/cnxk: add cookies check for multi-segment offload
* 5a0f64d84b  net/cnxk: fix configuring large Rx/Tx queues
* 3232c95d2c  net/cnxk: fix indirect mbuf handling in Tx
* 8ed5ca4dda  net/cnxk: fix mbuf fields in multi-segment Tx
* 7752f14026  net/cnxk: fix Rx flush on CN10k
* 4c2f14bc6d  net/cnxk: fix xstats reset
* ee0fa7552a  net/dpaa: forbid MTU configuration for shared interface
* 0f044b6681  net/iavf: fix refine protocol header
* 0b241667cc  net/iavf: fix tainted scalar
* b125c0e721  net/iavf: fix tainted scalar
* 92a16af450  net/iavf: fix virtchnl command called in interrupt
* df44ba7a3c  net/ice/base: fix preparing PHY for timesync command
* e2072ba1b0  net/ice/base: fix resource leak
* 9749dffe23  net/ice: fix MAC type of E822 and E823
* 87e4384d26  net/mlx5: fix condition of LACP miss flow
* 2ece3b7186  net/mlx5: fix flow workspace double free in Windows
* 1cfb78d2c4  net/mlx5: fix meter policy priority
* 48adbc80ba  net/mlx5: remove GENEVE options length limitation
* a74c5001e9  net/ngbe: add proper memory barriers in Rx
* 79be49dd2d  net/ngbe: add special config for YT8531SH-CA PHY
* 31a28a99fd  net/ngbe: add spinlock protection on YT PHY
* 21f702d556  net/ngbe: fix link status in no LSC mode
* 659cfce01e  net/ngbe: remove redundant codes
* 44a8635459  net/thunderx: fix DMAC control register update
* 12011b11a3  net/txgbe: adapt to MNG veto bit setting
* 5e170dd8b6  net/txgbe: fix blocking system events
* 166591931b  pcapng: modify timestamp calculation
* df33fb53e4  rcu: fix acked token in debug log
* 063cddfc74  rcu: use atomic operation on acked token
* 4ad17a1c8f  test/crypto: fix enqueue/dequeue callback case
* 63bf81a617  test: fix named test macro

21.11.9 Release Notes
---------------------


21.11.9 Fixes
~~~~~~~~~~~~~

* app/dumpcap: remove unused struct array
* app/procinfo: fix leak on exit
* app/testpmd: fix aged flow destroy
* app/testpmd: remove flex item init command leftover
* app/testpmd: remove redundant policy action condition
* app/testpmd: remove unnecessary cast
* baseband/la12xx: fix use after free in modem config
* bpf: fix free function mismatch if convert fails
* build: remove version check on compiler links function
* bus/dpaa: fix lock condition during error handling
* bus/dpaa: fix PFDRs leaks due to FQRNIs
* bus/dpaa: fix the fman details status
* bus/dpaa: fix VSP for 1G fm1-mac9 and 10
* bus/fslmc: fix Coverity warnings in QBMAN
* common/cnxk: fix base log level
* common/cnxk: fix build on Ubuntu 24.04
* common/cnxk: fix RSS key configuration
* common/dpaax/caamflib: fix PDCP SNOW-ZUC watchdog
* common/mlx5: fix error CQE handling for 128 bytes CQE
* common/mlx5: fix misalignment
* crypto/bcmfs: fix free function mismatch
* crypto/dpaa2_sec: fix memory leak
* crypto/openssl: fix 3DES-CTR with big endian CPUs
* dev: fix callback lookup when unregistering device
* devtools: fix check of multiple commits fixed at once
* dmadev: fix potential null pointer access
* dma/idxd: fix free function mismatch in device probe
* doc: correct definition of stats per queue feature
* eal/x86: fix 32-bit write combining store
* ethdev: fix overflow in descriptor count
* ethdev: verify queue ID in Tx done cleanup
* event/cnxk: fix free function mismatch in port config
* eventdev: fix possible array underflow/overflow
* event/octeontx: fix possible integer overflow
* examples/eventdev: fix queue crash with generic pipeline
* examples/l2fwd-event: fix spinlock handling
* examples/ntb: check info query return
* examples/vhost: fix free function mismatch
* fib6: add runtime checks in AVX512 lookup
* fib: fix AVX512 lookup
* hash: fix thash LFSR initialization
* member: fix choice of bucket for displacement
* net/bnx2x: fix always true expression
* net/bnx2x: fix duplicate branch
* net/bnx2x: fix possible infinite loop at startup
* net/bnx2x: remove dead conditional
* net/bnxt: fix bad action offset in Tx BD
* net/bnxt: fix reading SFF-8436 SFP EEPROMs
* net/bnxt: fix TCP and UDP checksum flags
* net/bnxt/tf_core: fix Thor TF EM key size check
* net/cnxk: fix Rx offloads to handle timestamp
* net/cnxk: fix Rx timestamp handling for VF
* net/dpaa: fix reallocate mbuf handling
* net/dpaa: fix typecasting channel ID
* net/e1000: fix link status crash in secondary process
* net/e1000: fix use after free in filter flush
* net/ena: revert redefining memcpy
* net/hns3: fix dump counter of registers
* net/hns3: fix error code for repeatedly create counter
* net/hns3: fix fully use hardware flow director table
* net/hns3: remove some basic address dump
* net/hns3: restrict tunnel flow rule to one header
* net/hns3: verify reset type from firmware
* net/i40e/base: fix blinking X722 with X557 PHY
* net/i40e/base: fix DDP loading with reserved track ID
* net/i40e/base: fix loop bounds
* net/i40e/base: fix misleading debug logs and comments
* net/i40e/base: fix repeated register dumps
* net/i40e/base: fix setting flags in init function
* net/i40e/base: fix unchecked return value
* net/i40e: check register read for outer VLAN
* net/i40e: fix AVX-512 pointer copy on 32-bit
* net/iavf: add segment-length check to Tx prep
* net/iavf: fix AVX-512 pointer copy on 32-bit
* net/iavf: fix crash when link is unstable
* net/iavf: fix VF reset when using DCF
* net/iavf: preserve MAC address with i40e PF Linux driver
* net/ice/base: add bounds check
* net/ice/base: fix iteration of TLVs in Preserved Fields Area
* net/ice/base: fix link speed for 200G
* net/ice/base: fix VLAN replay after reset
* net/ice: detect stopping a flow director queue twice
* net/ice: fix AVX-512 pointer copy on 32-bit
* net/ionic: fix build with Fedora Rawhide
* net/ixgbe/base: fix unchecked return value
* net/ixgbe: fix link status delay on FreeBSD
* net/memif: fix buffer overflow in zero copy Rx
* net/mlx5: fix memory leak in metering
* net/mlx5: fix miniCQEs number calculation
* net/mlx5: fix next protocol validation after flex item
* net/mlx5: fix non full word sample fields in flex item
* net/mlx5: fix number of supported flex parsers
* net/mlx5: fix reported Rx/Tx descriptor limits
* net/mlx5: fix shared queue port number in vector Rx
* net/mlx5: workaround list management of Rx queue control
* net/mvneta: fix possible out-of-bounds write
* net/netvsc: fix using Tx queue higher than Rx queues
* net/netvsc: force Tx VLAN offload on 801.2Q packet
* net/nfb: fix use after free
* net/nfp: fix link change return value
* net/ngbe: fix driver load bit to inform firmware
* net/ngbe: fix interrupt lost in legacy or MSI mode
* net/ngbe: reconfigure more MAC Rx registers
* net/ngbe: restrict configuration of VLAN strip offload
* net/pcap: fix blocking Rx
* net/pcap: set live interface as non-blocking
* net/sfc: fix use after free in debug logs
* net/tap: avoid memcpy with null argument
* net/tap: restrict maximum number of MP FDs
* net/txgbe: fix driver load bit to inform firmware
* net/txgbe: fix SWFW mbox
* net/txgbe: fix VF-PF mbox interrupt
* net/txgbe: remove outer UDP checksum capability
* net/virtio: fix Rx checksum calculation
* net/virtio-user: reset used index counter
* net/vmxnet3: fix crash after configuration failure
* power: enable CPPC
* power: fix log message when checking lcore ID
* power: fix log message when checking power modes
* power: fix mapped lcore ID
* raw/ifpga/base: fix use after free
* raw/ifpga: fix free function mismatch in interrupt config
* rcu: fix implicit conversion in bit shift
* Revert "test/bonding: fix loop on members"
* test/bonding: fix loop on members
* test/bonding: fix MAC address comparison
* test/bonding: remove redundant info query
* test/eal: fix lcore check
* test/eal: fix loop coverage for alignment macros
* test/event: avoid duplicate initialization
* vdpa: update used flags in used ring relay
* version: 21.11.9-rc1
* vhost: fix offset while mapping log base address

21.11.9 Validation
~~~~~~~~~~~~~~~~~~

* `Red Hat(R) Testing <https://mails.dpdk.org/archives/dev/2024-December/310322.html>`__

   * Platform

      * RHEL 9
      * Kernel 5.14
      * Qemu 7.2
      * X540-AT2 NIC(ixgbe, 10G)

   * Functionality

      * Guest with device assignment(PF) throughput testing(1G hugepage size)
      * Guest with device assignment(PF) throughput testing(2M hugepage size)
      * Guest with device assignment(VF) throughput testing
      * PVP (host dpdk testpmd as vswitch) 1Q: throughput testing
      * PVP vhost-user 2Q throughput testing
      * PVP vhost-user 1Q cross numa node  throughput testing
      * Guest with vhost-user 2 queues throughput testing
      * vhost-user reconnect with dpdk-client, qemu-server: qemu reconnect
      * vhost-user reconnect with dpdk-client, qemu-server: ovs reconnect
      * PVP 1Q live migration testing
      * PVP 1Q cross numa node live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing
      * Guest with ovs+dpdk+vhost-user 1Q live migration testing (2M)
      * Guest with ovs+dpdk+vhost-user 2Q live migration testing
      * Guest with ovs+dpdk+vhost-user 4Q live migration testing
      * Host PF + DPDK testing
      * Host VF + DPDK testing


* `Nvidia(R) Testing <https://mails.dpdk.org/archives/stable/2024-December/051225.html>`__

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
      * Regex application
      * Buffer Split
      * Tx scheduling

   * Build tests
   * ConnectX-6 Dx
   * ConnectX-7
   * BlueField-2


* `Intel(R) Testing <https://mails.dpdk.org/archives/stable/2024-December/051231.html>`__

   * Basic Intel(R) NIC(ixgbe, i40e and ice) testing

      * PF (i40e, ixgbe)
      * VF (i40e, ixgbe)
      * PF/VF (ice)
      * Compile Testing
      * Intel NIC single core/NIC performance - partial testing
      * IPseca - partial testing

   * Basic cryptodev and virtio testing

      * vhost/virtio basic loopback, PVP and performance test
      * cryptodev Function/Performance - partial testing

21.11.9 Known Issues
~~~~~~~~~~~~~~~~~~~~

* DPDK 21.11.9 contains fixes up to DPDK 24.11 and CVE-2024-11614
* Issues identified/fixed in DPDK main branch after DPDK 24.11 may be present in DPDK 21.11.9
* Ubuntu 24.04 with gcc 13.2.0 running at optimisation -O1 shows a maybe-uninitialized warning for iavf driver
* Some validation was ongoing at time of release

21.11.9 Fixes skipped and status unresolved
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

* c5b531d6ee  app/crypto-perf: fix session freeing
* 7bdf7a13ae  app/testpmd: fix encap/decap size calculation
* 461c287ab5  app/testpmd: fix GRO packets flush on timeout
* ecf408d2aa  app/testpmd: fix indirect action flush
* 0c5709824b  baseband/acc: fix ring memory allocation
* 4edbcc7b53  bpf: disable on 32-bit x86
* 4edbcc7b53  bpf: disable on 32-bit x86
* 5d8c1f6253  common/cnxk: check error in MAC address set
* a6f639e079  common/cnxk: fix BP threshold calculation
* 772e30281a  common/cnxk: fix CPT backpressure disable on LBK
* 6c3de40af8  common/cnxk: fix inline CTX write
* 48054ca384  common/cnxk: fix link config for SDP
* 2d4505dc6d  common/cnxk: fix MAC address change with active VF
* 1c7a4d37e7  common/cnxk: fix mailbox timeout due to deadlock
* 59ceaa72d5  common/cnxk: fix part number for CN10K
* 5781638519  common/cnxk: fix RQ mask config for CN10KB chip
* 37ca457d3f  common/mlx5: fix obtaining IB device in LAG mode
* cedb44dc87  common/mlx5: improve AES-XTS tweak capability check
* 7be74edb90  common/mlx5: use just sufficient barrier for Arm
* 046341575b  crypto/dpaa_sec: fix IPsec descriptor
* 17d5bc6135  crypto/openssl: make per-QP auth context clones
* b1d7112602  crypto/openssl: make per-QP cipher context clones
* 3dd3115078  dma/cnxk: fix chunk buffer failure return code
* ba39a261a7  dma/cnxk: fix completion ring tail wrap
* 95a955e3e0  dma/cnxk: fix device reconfigure
* 2a3f42942a  dma/hisilicon: remove support for HIP09 platform
* b9a87346b0  ethdev: fix strict aliasing in link up
* 694e8e643d  event/cnxk: fix CASP usage for clang
* b37fe88a2c  event/cnxk: fix LMTST write for single event mode
* 3fe71706ab  event/cnxk: fix stale data in workslot
* 04dac73643  eventdev/crypto: fix enqueue count
* 4b04134cbb  eventdev/crypto: fix failed events
* da73a2a0d1  eventdev/crypto: fix offset used while flushing events
* ad12d08f05  eventdev/crypto: fix opaque field handling
* f442c04001  eventdev/crypto: fix overflow in circular buffer
* 9a518054b5  examples/l3fwd: fix duplicate expression for default nexthop
* 927cb43fe9  examples/l3fwd: fix port group mask with AltiVec
* ebab0e8b22  examples/l3fwd: fix read beyond boundaries
* ebab0e8b22  examples/l3fwd: fix read beyond boundaries
* 547f294357  gro: fix reordering of packets
* 6df1bc6b3b  mempool/cnxk: avoid hang when counting batch allocs
* b3ddd649ad  mempool/cnxk: fix alloc from non-EAL threads
* 97039941b2  net/af_xdp: parse UMEM map info from mempool
* abdabad636  net/af_xdp: remove unused local statistic
* e82b0fe097  net/axgbe: disable RRC for yellow carp devices
* 78dcdb821c  net/bnxt/tf_core: fix WC TCAM multi-slice delete
* 8782e4de3e  net/bnxt/tf_ulp: fix parent child DB counters
* a78a1ed8ba  net/bonding: fix failover time of LACP with mode 4
* 02a2accb5f  net/bonding: fix flow count query
* 26a6bda9df  net/cnxk: add cookies check for multi-segment offload
* 5a0f64d84b  net/cnxk: fix configuring large Rx/Tx queues
* 3232c95d2c  net/cnxk: fix indirect mbuf handling in Tx
* 8ed5ca4dda  net/cnxk: fix mbuf fields in multi-segment Tx
* 7752f14026  net/cnxk: fix Rx flush on CN10k
* 4c2f14bc6d  net/cnxk: fix xstats reset
* ee0fa7552a  net/dpaa: forbid MTU configuration for shared interface
* 0f044b6681  net/iavf: fix refine protocol header
* 0b241667cc  net/iavf: fix tainted scalar
* b125c0e721  net/iavf: fix tainted scalar
* 92a16af450  net/iavf: fix virtchnl command called in interrupt
* df44ba7a3c  net/ice/base: fix preparing PHY for timesync command
* e2072ba1b0  net/ice/base: fix resource leak
* 9749dffe23  net/ice: fix MAC type of E822 and E823
* 87e4384d26  net/mlx5: fix condition of LACP miss flow
* b04b06f4cb  net/mlx5: fix flex item header length field translation
* 2ece3b7186  net/mlx5: fix flow workspace double free in Windows
* 1cfb78d2c4  net/mlx5: fix meter policy priority
* 27918f0d53  net/mlx5: fix real time counter reading from PCI BAR
* 3c9a82fa6e  net/mlx5: fix Rx queue control management
* f8f294c66b  net/mlx5: fix shared Rx queue control release
* 48adbc80ba  net/mlx5: remove GENEVE options length limitation
* a74c5001e9  net/ngbe: add proper memory barriers in Rx
* 79be49dd2d  net/ngbe: add special config for YT8531SH-CA PHY
* 31a28a99fd  net/ngbe: add spinlock protection on YT PHY
* 21f702d556  net/ngbe: fix link status in no LSC mode
* 659cfce01e  net/ngbe: remove redundant codes
* 44a8635459  net/thunderx: fix DMAC control register update
* 12011b11a3  net/txgbe: adapt to MNG veto bit setting
* 916aa13f4a  net/txgbe: fix a mass of interrupts
* 5e170dd8b6  net/txgbe: fix blocking system events
* 6db358536f  pcapng: fix handling of chained mbufs
* 166591931b  pcapng: modify timestamp calculation
* df33fb53e4  rcu: fix acked token in debug log
* 063cddfc74  rcu: use atomic operation on acked token
* 4ad17a1c8f  test/crypto: fix enqueue/dequeue callback case
* 251fdc592d  test/crypto: fix synchronous API calls
* 63bf81a617  test: fix named test macro
