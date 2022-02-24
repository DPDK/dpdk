.. SPDX-License-Identifier: BSD-3-Clause
   Copyright 2021 The DPDK contributors

.. include:: <isonum.txt>

DPDK Release 22.03
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      ninja -C build doc
      xdg-open build/doc/guides/html/rel_notes/release_22_03.html


New Features
------------

.. This section should contain new features added in this release.
   Sample format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense.
     The description should be enough to allow someone scanning
     the release notes to understand the new feature.

     If the feature adds a lot of sub-features you can use a bullet list
     like this:

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     Suggested order in release notes items:
     * Core libs (EAL, mempool, ring, mbuf, buses)
     * Device abstraction libs and PMDs (ordered alphabetically by vendor name)
       - ethdev (lib, PMDs)
       - cryptodev (lib, PMDs)
       - eventdev (lib, PMDs)
       - etc
     * Other libs
     * Apps, Examples, Tools (if significant)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =======================================================

* **Added ability to reuse hugepages in Linux.**

  It is possible to reuse files in hugetlbfs to speed up hugepage mapping,
  which may be useful for fast restart and large allocations.
  The new mode is activated with ``--huge-unlink=never``
  and has security implications, refer to the user and programmer guides.

* **Added functions to calculate UDP/TCP checksum in mbuf.**

  * Added the following functions to calculate UDP/TCP checksum of packets
    which can be over multi-segments:
    - ``rte_ipv4_udptcp_cksum_mbuf()``
    - ``rte_ipv4_udptcp_cksum_mbuf_verify()``
    - ``rte_ipv6_udptcp_cksum_mbuf()``
    - ``rte_ipv6_udptcp_cksum_mbuf_verify()``

* **Added functions to configure flow engine.**

  * Added ``rte_flow_configure`` API to configure flow management
    engine, allowing to pre-allocate some resources for better performance.
    Added ``rte_flow_info_get`` API to retrieve available resources.

  * Added ``rte_flow_template_table_create`` API to group flow rules
    with the same flow attributes and common matching patterns and actions
    defined by ``rte_flow_pattern_template_create`` and
    ``rte_flow_actions_template_create`` respectively.
    Corresponding functions to destroy these entities are:
    ``rte_flow_template_table_destroy``, ``rte_flow_pattern_template_destroy``
    and ``rte_flow_actions_template_destroy``.

* **Added functions for asynchronous flow rules creation/destruction.**

  * Added ``rte_flow_async_create`` and ``rte_flow_async_destroy`` API
    to enqueue flow creaion/destruction operations asynchronously as well as
    ``rte_flow_pull`` to poll and retrieve results of these operations and
    ``rte_flow_push`` to push all the in-flight	operations to the NIC.

  * Added asynchronous API for indirect actions management:
    ``rte_flow_async_action_handle_create``,
    ``rte_flow_async_action_handle_destroy`` and
    ``rte_flow_async_action_handle_update``.

* **Added rte_flow support for matching GRE optional fields.**

  Added ``gre_option`` item in rte_flow to support checksum/key/sequence
  matching in GRE packets.

* **Added new RSS offload types for L2TPv2 in RSS flow.**

  Added macro RTE_ETH_RSS_L2TPV2, now L2TPv2 session ID field can be used as
  input set for RSS.

* **Added IP reassembly Ethernet offload API, to get and set config.**

  Added IP reassembly offload APIs which provide functions to query IP
  reassembly capabilities, to set configuration and to get currently set
  reassembly configuration.

* **Added an API to enable queue based priority flow ctrl(PFC).**

  New APIs, ``rte_eth_dev_priority_flow_ctrl_queue_info_get()`` and
  ``rte_eth_dev_priority_flow_ctrl_queue_configure()``, was added.

* **Added a private dump API, to dump private info from device.**

  Added the private dump API which provides querying private info from device.
  There exists many private properties in different PMD drivers.
  The information of these properties is important for debug.
  As the information is private, a dump function is introduced.

* **Updated AF_XDP PMD**

  * Added support for libxdp >=v1.2.2.
  * Re-enabled secondary process support. RX/TX is not supported.

* **Updated Amazon ENA PMD.**

  The new driver version (v2.6.0) introduced bug fixes and improvements, including:

  * Added new checksum related xstats: ``l3_csum_bad``, ``l4_csum_bad`` and
    ``l4_csum_good``.
  * Added support for the link status configuration.
  * Added optimized memcpy support for the ARM platforms.
  * Added ENA admin queue support for the MP applications.
  * Added free Tx mbuf on demand feature support.
  * Added ``rte_eth_xstats_get_names_by_id`` API support.
  * Added ``miss_txc_to`` device argument for setting the Tx completion timeout.

* **Updated Cisco enic driver.**

  * Added rte_flow support for matching GENEVE packets.
  * Added rte_flow support for matching eCPRI packets.

* **Updated Intel iavf driver.**

  * Added L2TPv2 (include PPP over L2TPv2) RSS support based on outer
    MAC src/dst address and L2TPv2 session ID.
  * Added L2TPv2 (include PPP over L2TPv2) FDIR support based on outer
    MAC src/dst address and L2TPv2 session ID.
  * Added PPPoL2TPv2oUDP FDIR distribute packets based on inner IP
    src/dst address and UDP/TCP src/dst port.

* **Updated Marvell cnxk ethdev PMD.**

  * Added queue based priority flow control support for CN9K & CN10K.
  * Added support for IP reassembly for inline inbound IPsec packets.
  * Added support for packet marking in traffic manager.
  * Added support for CNF95xx B0 variant SoC.

* **Updated Mellanox mlx5 driver.**

  * Supported ConnectX-7 capability to schedule traffic sending on timestamp.
  * Added WQE based hardware steering support with ``rte_flow_async`` API.
  * Added steering for external Rx queue created outside the PMD.
  * Added GRE optional fields matching.

* **Updated Wangxun ngbe driver.**

  * Added support for devices of custom PHY interfaces.
    - M88E1512 PHY connects to RJ45
    - M88E1512 PHY connects to RGMII combo
    - YT8521S PHY connects to SFP
  * Added LED OEM support.

* **Updated Wangxun txgbe driver.**

  * Added LED OEM support.

* **Added an API for private user data in asymmetric crypto session.**

  An API was added to get/set an asymmetric crypto session's user data.

* **Updated Marvell cnxk crypto PMD.**

  * Added SHA256-HMAC support in lookaside protocol (IPsec) for CN10K.
  * Added SHA384-HMAC support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added SHA512-HMAC support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES-CTR support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added NULL cipher support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES-XCBC support in lookaside protocol (IPsec) for CN9K & CN10K.
  * Added AES-CMAC support in CN9K & CN10K.
  * Added ESN and anti-replay support in lookaside protocol (IPsec) for CN10K.

* **Updated Intel QuickAssist Technology crypto PMD.**

  * Added support for CPM2.0b (4942) devices.
  * Added ECDSA algorithm support.
  * Added ECPM algorithm support.

* **Added an API to retrieve event port id of ethdev Rx adapter.**

  The new API ``rte_event_eth_rx_adapter_event_port_get()`` was added.

* **Added support for Kunpeng930 DMA devices to HiSilicon DMA PMD.**

  * Kunpeng930 DMA devices are now enabled for HiSilicon DMA PMD.

* **Added CNXK GPIO PMD.**

  Added a new rawdevice PMD which allows to manage userspace GPIOs and install
  custom GPIO interrupt handlers which bypass kernel. This is especially useful
  for applications, that besides providing standard dataplane functionality
  want to have fast and low latency access to GPIO pin state.

  See the :doc:`../rawdevs/cnxk_gpio` rawdev guide for more details on this
  driver.

* **Updated testpmd.**

  * Called ``rte_ipv4/6_udptcp_cksum_mbuf()`` functions in testpmd csum mode
    to support software UDP/TCP checksum over multiple segments.

* **Added crypto producer mode in test-eventdev.**

  * Crypto producer mode helps to measure performance of OP_NEW and OP_FORWARD
    modes of event crypto adapter.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* **Removed experimental performance thread example application.**


API Changes
-----------

.. This section should contain API changes. Sample format:

   * sample: Add a short 1-2 sentence description of the API change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* net: added experimental functions ``rte_ipv4_udptcp_cksum_mbuf()``,
  ``rte_ipv4_udptcp_cksum_mbuf_verify()``, ``rte_ipv6_udptcp_cksum_mbuf()``,
  ``rte_ipv6_udptcp_cksum_mbuf_verify()``

* ethdev: Old public macros and enumeration constants without ``RTE_ETH_`` prefix,
  which are kept for backward compatibility, are marked as deprecated.

* cryptodev: The asymmetric session handling was modified to use a single
  mempool object. An API ``rte_cryptodev_asym_session_pool_create`` was added
  to create a mempool with element size big enough to hold the generic asymmetric
  session header, max size for a device private session data, and user data size.
  The session structure was moved to ``cryptodev_pmd.h``,
  hiding it from applications.
  The API ``rte_cryptodev_asym_session_init`` was removed as the initialization
  is now moved to ``rte_cryptodev_asym_session_create``, which was updated to
  return an integer value to indicate initialisation errors.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * sample: Add a short 1-2 sentence description of the ABI change
     which was announced in the previous releases and made in this release.
     Start with a scope label like "ethdev:".
     Use fixed width quotes for ``function_names`` or ``struct_names``.
     Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================

* No ABI change that would break compatibility with 21.11.


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested
   with this release.

   The format is:

   * <vendor> platform with <vendor> <type of devices> combinations

     * List of CPU
     * List of OS
     * List of devices
     * Other relevant details...

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =======================================================
