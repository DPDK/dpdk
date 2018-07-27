..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2016 The DPDK contributors

DPDK Release 16.11
==================

.. **Read this first.**

   The text below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text: ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      firefox build/doc/html/guides/rel_notes/release_16_11.html


New Features
------------

.. This section should contain new features added in this release. Sample format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense. The description
     should be enough to allow someone scanning the release notes to understand
     the new feature.

     If the feature adds a lot of sub-features you can use a bullet list like this.

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     This section is a comment. Make sure to start the actual text at the margin.


* **Added software parser for packet type.**

  * Added a new function ``rte_pktmbuf_read()`` to read the packet data from an
    mbuf chain, linearizing if required.
  * Added a new function ``rte_net_get_ptype()`` to parse an Ethernet packet
    in an mbuf chain and retrieve its packet type from software.
  * Added new functions ``rte_get_ptype_*()`` to dump a packet type as a string.

* **Improved offloads support in mbuf.**

  * Added a new function ``rte_raw_cksum_mbuf()`` to process the checksum of
    data embedded in an mbuf chain.
  * Added new Rx checksum flags in mbufs to describe more states: unknown,
    good, bad, or not present (useful for virtual drivers). This modification
    was done for IP and L4.
  * Added a new Rx LRO mbuf flag, used when packets are coalesced. This
    flag indicates that the segment size of original packets is known.

* **Added vhost-user dequeue zero copy support.**

  The copy in the dequeue path is avoided in order to improve the performance.
  In the VM2VM case, the boost is quite impressive. The bigger the packet size,
  the bigger performance boost you may get. However, for the VM2NIC case, there
  are some limitations, so the boost is not as  impressive as the VM2VM case.
  It may even drop quite a bit for small packets.

  For that reason, this feature is disabled by default. It can be enabled when
  the ``RTE_VHOST_USER_DEQUEUE_ZERO_COPY`` flag is set. Check the VHost section
  of the Programming Guide for more information.

* **Added vhost-user indirect descriptors support.**

  If the indirect descriptor feature is enabled, each packet sent by the guest
  will take exactly one slot in the enqueue virtqueue. Without this feature, as in
  the current version, even 64 bytes packets take two slots with Virtio PMD on guest
  side.

  The main impact is better performance for 0% packet loss use-cases, as it
  behaves as if the virtqueue size was enlarged, so more packets can be buffered
  in the case of system perturbations. On the downside, small performance degradations
  were measured when running micro-benchmarks.

* **Added vhost PMD xstats.**

  Added extended statistics to vhost PMD from a per port perspective.

* **Supported offloads with virtio.**

  Added support for the following offloads in virtio:

  * Rx/Tx checksums.
  * LRO.
  * TSO.

* **Added virtio NEON support for ARM.**

  Added NEON support for ARM based virtio.

* **Updated the ixgbe base driver.**

  Updated the ixgbe base driver, including the following changes:

  * Added X550em_a 10G PHY support.
  * Added support for flow control auto negotiation for X550em_a 1G PHY.
  * Added X550em_a FW ALEF support.
  * Increased mailbox version to ``ixgbe_mbox_api_13``.
  * Added two MAC operations for Hyper-V support.

* **Added APIs for VF management to the ixgbe PMD.**

  Eight new APIs have been added to the ixgbe PMD for VF management from the PF.
  The declarations for the API's can be found in ``rte_pmd_ixgbe.h``.

* **Updated the enic driver.**

  * Added update to use interrupt for link status checking instead of polling.
  * Added more flow director modes on UCS Blade with firmware version >= 2.0(13e).
  * Added full support for MTU update.
  * Added support for the ``rte_eth_rx_queue_count`` function.

* **Updated the mlx5 driver.**

  * Added support for RSS hash results.
  * Added several performance improvements.
  * Added several bug fixes.

* **Updated the QAT PMD.**

  The QAT PMD was updated with additional support for:

  * MD5_HMAC algorithm.
  * SHA224-HMAC algorithm.
  * SHA384-HMAC algorithm.
  * GMAC algorithm.
  * KASUMI (F8 and F9) algorithm.
  * 3DES algorithm.
  * NULL algorithm.
  * C3XXX device.
  * C62XX device.

* **Added openssl PMD.**

  A new crypto PMD has been added, which provides several ciphering and hashing algorithms.
  All cryptography operations use the Openssl library crypto API.

* **Updated the IPsec example.**

  Updated the IPsec example with the following support:

  * Configuration file support.
  * AES CBC IV generation with cipher forward function.
  * AES GCM/CTR mode.

* **Added support for new gcc -march option.**

  The GCC 4.9 ``-march`` option supports the Intel processor code names.
  The config option ``RTE_MACHINE`` can be used to pass code names to the compiler via the ``-march`` flag.


Resolved Issues
---------------

.. This section should contain bug fixes added to the relevant sections. Sample format:

   * **code/section Fixed issue in the past tense with a full stop.**

     Add a short 1-2 sentence description of the resolved issue in the past tense.
     The title should contain the code/lib section like a commit message.
     Add the entries in alphabetic order in the relevant sections below.

   This section is a comment. Make sure to start the actual text at the margin.


Drivers
~~~~~~~

* **enic: Fixed several flow director issues.**

* **enic: Fixed inadvertent setting of L4 checksum ptype on ICMP packets.**

* **enic: Fixed high driver overhead when servicing Rx queues beyond the first.**



Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue in the present
     tense. Add information on any known workarounds.

   This section is a comment. Make sure to start the actual text at the margin.

* **L3fwd-power app does not work properly when Rx vector is enabled.**

  The L3fwd-power app doesn't work properly with some drivers in vector mode
  since the queue monitoring works differently between scalar and vector modes
  leading to incorrect frequency scaling. In addition, L3fwd-power application
  requires the mbuf to have correct packet type set but in some drivers the
  vector mode must be disabled for this.

  Therefore, in order to use L3fwd-power, vector mode should be disabled
  via the config file.

* **Digest address must be supplied for crypto auth operation on QAT PMD.**

  The cryptodev API specifies that if the rte_crypto_sym_op.digest.data field,
  and by inference the digest.phys_addr field which points to the same location,
  is not set for an auth operation the driver is to understand that the digest
  result is located immediately following the region over which the digest is
  computed. The QAT PMD doesn't correctly handle this case and reads and writes
  to an incorrect location.

  Callers can workaround this by always supplying the digest virtual and
  physical address fields in the rte_crypto_sym_op for an auth operation.


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. Make sure to start the actual text at the margin.

* The driver naming convention has been changed to make them more
  consistent. It especially impacts ``--vdev`` arguments. For example
  ``eth_pcap`` becomes ``net_pcap`` and ``cryptodev_aesni_mb_pmd`` becomes
  ``crypto_aesni_mb``.

  For backward compatibility an alias feature has been enabled to support the
  original names.

* The log history has been removed.

* The ``rte_ivshmem`` feature (including library and EAL code) has been removed
  in 16.11 because it had some design issues which were not planned to be fixed.

* The ``file_name`` data type of ``struct rte_port_source_params`` and
  ``struct rte_port_sink_params`` is changed from ``char *`` to ``const char *``.

* **Improved device/driver hierarchy and generalized hotplugging.**

  The device and driver relationship has been restructured by introducing generic
  classes. This paves the way for having PCI, VDEV and other device types as
  instantiated objects rather than classes in themselves. Hotplugging has also
  been generalized into EAL so that Ethernet or crypto devices can use the
  common infrastructure.

  * Removed ``pmd_type`` as a way of segregation of devices.
  * Moved ``numa_node`` and ``devargs`` into ``rte_driver`` from
    ``rte_pci_driver``. These can now be used by any instantiated object of
    ``rte_driver``.
  * Added ``rte_device`` class and all PCI and VDEV devices inherit from it
  * Renamed devinit/devuninit handlers to probe/remove to make it more
    semantically correct with respect to the device <=> driver relationship.
  * Moved hotplugging support to EAL. Hereafter, PCI and vdev can use the
    APIs ``rte_eal_dev_attach`` and ``rte_eal_dev_detach``.
  * Renamed helpers and support macros to make them more synonymous
    with their device types
    (e.g. ``PMD_REGISTER_DRIVER`` => ``RTE_PMD_REGISTER_PCI``).
  * Device naming functions have been generalized from ethdev and cryptodev
    to EAL. ``rte_eal_pci_device_name`` has been introduced for obtaining
    unique device name from PCI Domain-BDF description.
  * Virtual device registration APIs have been added: ``rte_eal_vdrv_register``
    and ``rte_eal_vdrv_unregister``.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced in
     the previous releases and made in this release. Use fixed width quotes for
     ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. Make sure to start the actual text at the margin.



Shared Library Versions
-----------------------

.. Update any library version updated in this release and prepend with a ``+``
   sign, like this:

     libethdev.so.4
     librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.2



The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
   + librte_cryptodev.so.2
     librte_distributor.so.1
   + librte_eal.so.3
   + librte_ethdev.so.5
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_lpm.so.2
     librte_mbuf.so.2
     librte_mempool.so.2
     librte_meter.so.1
     librte_net.so.1
     librte_pdump.so.1
     librte_pipeline.so.3
     librte_pmd_bond.so.1
     librte_pmd_ring.so.2
     librte_port.so.3
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.3


Tested Platforms
----------------

.. This section should contain a list of platforms that were tested with this release.

   The format is:

   #. Platform name.

      * Platform details.
      * Platform details.

   This section is a comment. Make sure to start the actual text at the margin.

#. SuperMicro 1U

   - BIOS: 1.0c
   - Processor: Intel(R) Atom(TM) CPU C2758 @ 2.40GHz

#. SuperMicro 1U

   - BIOS: 1.0a
   - Processor: Intel(R) Xeon(R) CPU D-1540 @ 2.00GHz
   - Onboard NIC: Intel(R) X552/X557-AT (2x10G)

     - Firmware-version: 0x800001cf
     - Device ID (PF/VF): 8086:15ad /8086:15a8

   - kernel driver version: 4.2.5 (ixgbe)

#. SuperMicro 2U

   - BIOS: 1.0a
   - Processor: Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz

#. Intel(R) Server board S2600GZ

   - BIOS: SE5C600.86B.02.02.0002.122320131210
   - Processor: Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz

#. Intel(R) Server board W2600CR

   - BIOS: SE5C600.86B.02.01.0002.082220131453
   - Processor: Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz

#. Intel(R) Server board S2600CWT

   - BIOS: SE5C610.86B.01.01.0009.060120151350
   - Processor: Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz

#. Intel(R) Server board S2600WTT

   - BIOS: SE5C610.86B.01.01.0005.101720141054
   - Processor: Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz

#. Intel(R) Server board S2600WTT

   - BIOS: SE5C610.86B.11.01.0044.090120151156
   - Processor: Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz

#. Intel(R) Server board S2600WTT

   - Processor: Intel(R) Xeon(R) CPU E5-2697 v2 @ 2.70GHz

#. Intel(R) Server

   - Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz

#. IBM(R) Power8(R)

   - Machine type-model: 8247-22L
   - Firmware FW810.21 (SV810_108)
   - Processor: POWER8E (raw), AltiVec supported


Tested NICs
-----------

.. This section should contain a list of NICs that were tested with this release.

   The format is:

   #. NIC name.

      * NIC details.
      * NIC details.

   This section is a comment. Make sure to start the actual text at the margin.

#. Intel(R) Ethernet Controller X540-AT2

   - Firmware version: 0x80000389
   - Device id (pf): 8086:1528
   - Driver version: 3.23.2 (ixgbe)

#. Intel(R) 82599ES 10 Gigabit Ethernet Controller

   - Firmware version: 0x61bf0001
   - Device id (pf/vf): 8086:10fb / 8086:10ed
   - Driver version: 4.0.1-k (ixgbe)

#. Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

   - Firmware version: 0x800001cf
   - Device id (pf/vf): 8086:15ad / 8086:15a8
   - Driver version: 4.2.5 (ixgbe)

#. Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1572 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Ethernet Converged Network Adapter X710-DA2 (2x10G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1572 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Ethernet Converged Network Adapter XL710-QDA1 (1x40G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1584 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

   - Firmware version: 5.05
   - Device id (pf/vf): 8086:1583 / 8086:154c
   - Driver version: 1.5.23 (i40e)

#. Intel(R) Corporation I350 Gigabit Network Connection

   - Firmware version: 1.48, 0x800006e7
   - Device id (pf/vf): 8086:1521 / 8086:1520
   - Driver version: 5.2.13-k (igb)

#. Intel(R) Ethernet Multi-host Controller FM10000

   - Firmware version: N/A
   - Device id (pf/vf): 8086:15d0
   - Driver version: 0.17.0.9 (fm10k)

#. Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

   * Host interface: PCI Express 3.0 x16
   * Device ID: 15b3:1013
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 12.17.1010

#. Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1015
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 14.17.1010

#. Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

   * Host interface: PCI Express 3.0 x8
   * Device ID: 15b3:1015
   * MLNX_OFED: 3.4-1.0.0.0
   * Firmware version: 14.17.1010


Tested OSes
-----------

.. This section should contain a list of OSes that were tested with this release.
   The format is as follows, in alphabetical order:

   * CentOS 7.0
   * Fedora 23
   * Fedora 24
   * FreeBSD 10.3
   * Red Hat Enterprise Linux 7.2
   * SUSE Enterprise Linux 12
   * Ubuntu 15.10
   * Ubuntu 16.04 LTS
   * Wind River Linux 8

   This section is a comment. Make sure to start the actual text at the margin.

* CentOS 7.2
* Fedora 23
* Fedora 24
* FreeBSD 10.3
* FreeBSD 11
* Red Hat Enterprise Linux Server release 6.7 (Santiago)
* Red Hat Enterprise Linux Server release 7.0 (Maipo)
* Red Hat Enterprise Linux Server release 7.2 (Maipo)
* SUSE Enterprise Linux 12
* Wind River Linux 6.0.0.26
* Wind River Linux 8
* Ubuntu 14.04
* Ubuntu 15.04
* Ubuntu 16.04
