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
  * Added support for BlueField-2 and ConnectX-6 Dx.

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

  * Mellanox\ |reg| BlueField\ |reg|-2 SmartNIC MT41686 - MBF2H332A-AEEOT_A1 (2x25G)

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
