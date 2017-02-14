DPDK Release 17.02
==================

.. **Read this first.**

   The text below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text: ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      firefox build/doc/html/guides/rel_notes/release_17_02.html


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

     This section is a comment. do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added support for representing buses in EAL**

  A new structure ``rte_bus`` is introduced in EAL. This allows for devices to
  be represented by buses they are connected to. A new bus can be added to
  DPDK by extending the ``rte_bus`` structure and implementing the scan and
  probe functions. Once a new bus is registered using provided APIs, new
  devices can be detected and initialized using bus scan and probe callbacks.

  With this change, devices other than PCI or VDEV type can also be represented
  in DPDK framework.

* **Added generic EAL API for I/O device memory read/write operations.**

  This API introduces 8-bit, 16-bit, 32bit, 64bit I/O device
  memory read/write operations along with the relaxed versions.

  The weakly-ordered machine like ARM needs additional I/O barrier for
  device memory read/write access over PCI bus.
  By introducing the EAL abstraction for I/O device memory read/write access,
  The drivers can access I/O device memory in architecture-agnostic manner.
  The relaxed version does not have additional I/O memory barrier, useful in
  accessing the device registers of integrated controllers which
  implicitly strongly ordered with respect to memory access.

* **Added generic flow API (rte_flow).**

  This API provides a generic means to configure hardware to match specific
  ingress or egress traffic, alter its fate and query related counters
  according to any number of user-defined rules.

  It is slightly higher-level than the legacy filtering framework which it
  encompasses and supersedes (including all functions and filter types) in
  order to expose a single interface with an unambiguous behavior that is
  common to all poll-mode drivers (PMDs).

  See the :ref:`Generic flow API <Generic_flow_API>` documentation for more
  information.

* **Added firmware version get API.**

  Added a new function ``rte_eth_dev_fw_version_get()`` to fetch firmware
  version by a given device.

* **Added APIs for MACsec offload support to the ixgbe PMD.**

  Six new APIs have been added to the ixgbe PMD for MACsec offload support.
  The declarations for the APIs can be found in ``rte_pmd_ixgbe.h``.

* **Added I219 NICs support.**

  Added support for I219 Intel 1GbE NICs.

* **Added VF Daemon (VFD) on i40e. - EXPERIMENTAL**

  This's an EXPERIMENTAL feature to enhance the capability of DPDK PF as many
  VF management features are not supported by kernel PF driver.
  Some new private APIs are implemented in PMD without abstrction layer.
  They can be used directly by some users who have the need.

  The new APIs to control VFs directly from PF include,
  1) set VF MAC anti-spoofing
  2) set VF VLAN anti-spoofing
  3) set TX loopback
  4) set VF unicast promiscuous mode
  5) set VF multicast promiscuous mode
  6) set VF MTU
  7) get/reset VF stats
  8) set VF MAC address
  9) set VF VLAN stripping
  10) VF VLAN insertion
  12) set VF broadcast mode
  13) set VF VLAN tag
  14) set VF VLAN filter
  VFD also includes VF to PF mailbox message management by APP.
  When PF receives mailbox messages from VF, PF should call the callback
  provided by APP to know if they're permitted to be processed.

  As an EXPERIMENTAL feature, please aware it can be changed or even
  removed without prior notice.

* **Updated the i40e base driver.**

  updated the i40e base driver, including the following changes:

  * replace existing legacy memcpy() calls with i40e_memcpy() calls.
  * use BIT() macro instead of bit fields
  * add clear all WoL filters implementation
  * add broadcast promiscuous control per VLAN
  * remove unused X722_SUPPORT and I40E_NDIS_SUPPORT MARCOs

* **Updated the enic driver.**

  * Set new Rx checksum flags in mbufs to indicate unknown, good or bad.
  * Fix set/remove of MAC addresses. Allow up to 64 addresses per device.
  * Enable TSO on outer headers.

* **Added Solarflare libefx-based network PMD.**

  A new network PMD which supports Solarflare SFN7xxx and SFN8xxx family
  of 10/40 Gbps adapters has been added.

* **Updated the mlx4 driver.**

  * Addressed a few bugs.

* **Added support for Mellanox ConnectX-5 adapters (mlx5).**

  Support for Mellanox ConnectX-5 family of 10/25/40/50/100 Gbps adapters
  has been added to the existing mlx5 PMD.

* **Updated the mlx5 driver.**

  * Improve Tx performance by using vector logic.
  * Improve RSS balancing when number of queues is not a power of two.
  * Generic flow API support for Ethernet, IPv4, IPv4, UDP, TCP, VLAN and
    VXLAN pattern items with DROP and QUEUE actions.
  * Support for extended statistics.
  * Addressed several data path bugs.
  * As of MLNX_OFED 4.0-1.0.1.0, the Toeplitz RSS hash function is not
    symmetric anymore for consistency with other PMDs.

* **virtio-user with vhost-kernel as another exceptional path.**

  Previously, we upstreamed a virtual device, virtio-user with vhost-user
  as the backend, as a way for IPC (Inter-Process Communication) and user
  space container networking.

  Virtio-user with vhost-kernel as the backend is a solution for exceptional
  path, such as KNI, which exchanges packets with kernel networking stack.
  This solution is very promising in:

  * maintenance: vhost and vhost-net (kernel) is upstreamed and extensively
    used kernel module.
  * features: vhost-net is born to be a networking solution, which has
    lots of networking related features, like multi-queue, TSO, multi-seg
    mbuf, etc.
  * performance: similar to KNI, this solution would use one or more
    kthreads to send/receive packets from user space DPDK applications,
    which has little impact on user space polling thread (except that
    it might enter into kernel space to wake up those kthreads if
    necessary).

* **Added virtio Rx interrupt suppprt.**

  This feature enables Rx interrupt mode for virtio pci net devices as
  binded to VFIO (noiommu mode) and drived by virtio PMD.

  With this feature, virtio PMD can switch between polling mode and
  interrupt mode, to achieve best performance, and at the same time save
  power. It can work on both legacy and modern virtio devices. At this mode,
  each rxq is mapped with an exluded MSIx interrupt.

  See the :ref:`Virtio Interrupt Mode <virtio_interrupt_mode>` documentation
  for more information.

* **Added ARMv8 crypto PMD.**

  A new crypto PMD has been added, which provides combined mode cryptografic
  operations optimized for ARMv8 processors. The driver can be used to enhance
  performance in processing chained operations such as cipher + HMAC.

* **Updated the QAT PMD.**

  The QAT PMD was updated with additional support for:

  * DES algorithm.
  * Scatter-gather list (SGL) support.

* **Updated the AESNI MB PMD.**

  * The Intel(R) Multi Buffer Crypto for IPsec library used in
    AESNI MB PMD has been moved to a new repository, in GitHub.
  * Support for single operations (cipher only and authentication only).

* **Updated the AES-NI GCM PMD.**

  The AES-NI GCM PMD was migrated from MB library to ISA-L library.
  The migration entailed the following additional support for:

  * GMAC algorithm.
  * 256-bit cipher key.
  * Session-less mode.
  * Out-of place processing
  * Scatter-gatter support for chained mbufs (only out-of place and destination
    mbuf must be contiguous)

* **Added crypto performance test application.**

  A new performance test application allows measuring performance parameters
  of PMDs available in crypto tree.

* **Added Elastic Flow Distributor library (rte_efd).**

  This new library uses perfect hashing to determine a target/value for a
  given incoming flow key.

  It does not store the key itself for lookup operations, and therefore,
  lookup performance is not dependent on the key size. Also, the target/value
  can be any arbitrary value (8 bits by default). Finally, the storage requirement
  is much smaller than a hash-based flow table and therefore, it can better fit for
  CPU cache, being able to scale to millions of flow keys.

  See the :ref:`Elastic Flow Distributor Library <Efd_Library>` documentation in
  the Programmers Guide document, for more information.


Resolved Issues
---------------

.. This section should contain bug fixes added to the relevant sections. Sample format:

   * **code/section Fixed issue in the past tense with a full stop.**

     Add a short 1-2 sentence description of the resolved issue in the past tense.
     The title should contain the code/lib section like a commit message.
     Add the entries in alphabetic order in the relevant sections below.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


EAL
~~~


Drivers
~~~~~~~

* **net/virtio: Fixed multiple process support.**

  Fixed few regressions introduced in recent releases that break the virtio
  multiple process support.


Libraries
~~~~~~~~~


Examples
~~~~~~~~

* **examples/ethtool: Fixed crash with non-PCI devices.**

  Querying a non-PCI device was dereferencing non-existent PCI data
  resulting in a segmentation fault.


Other
~~~~~


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue in the present
     tense. Add information on any known workarounds.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Moved five APIs for VF management from the ethdev to the ixgbe PMD.**

  The following five APIs for VF management from the PF have been removed from the ethdev,
  renamed and added to the ixgbe PMD::

    rte_eth_dev_set_vf_rate_limit
    rte_eth_dev_set_vf_rx
    rte_eth_dev_set_vf_rxmode
    rte_eth_dev_set_vf_tx
    rte_eth_dev_set_vf_vlan_filter

  The API's have been renamed to the following::

    rte_pmd_ixgbe_set_vf_rate_limit
    rte_pmd_ixgbe_set_vf_rx
    rte_pmd_ixgbe_set_vf_rxmode
    rte_pmd_ixgbe_set_vf_tx
    rte_pmd_ixgbe_set_vf_vlan_filter

  The declarations for the API’s can be found in ``rte_pmd_ixgbe.h``.

ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced in
     the previous releases and made in this release. Use fixed width quotes for
     ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================



Shared Library Versions
-----------------------

.. Update any library version updated in this release and prepend with a ``+``
   sign, like this:

     librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.2

   This section is a comment. do not overwrite or remove it.
   =========================================================


The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_cryptodev.so.2
     librte_distributor.so.1
     librte_eal.so.3
   + librte_ethdev.so.6
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

   * <vendor> platform with <vendor> <type of devices> combinations

     * List of CPU
     * List of OS
     * List of devices
     * Other relevant details...

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

This release has been tested with the below list of CPU/device/firmware/OS.
Each section describes a different set of combinations.

* Intel(R) platforms with Mellanox(R) NICs combinations

   * Platform details

     * Intel(R) Xeon(R) CPU E5-2697 v2 @ 2.70GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz

   * OS:

     * CentOS 7.0
     * Fedora 23
     * Fedora 24
     * FreeBSD 10.3
     * Red Hat Enterprise Linux 7.2
     * SUSE Enterprise Linux 12
     * Ubuntu 14.04 LTS
     * Ubuntu 15.10
     * Ubuntu 16.04 LTS
     * Wind River Linux 8

   * MLNX_OFED: 4.0-1.0.1.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.40.5030

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.1000

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.1000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.18.1000

     * Mellanox(R) ConnectX-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.18.1000

* IBM(R) Power8(R) with Mellanox(R) NICs combinations

   * Machine:

     * Processor: POWER8E (raw), AltiVec supported

       * type-model: 8247-22L
       * Firmware FW810.21 (SV810_108)

   * OS: Ubuntu 16.04 LTS PPC le

   * MLNX_OFED: 4.0-1.0.1.0

   * NICs:

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.1000

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.1000

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.1000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.18.1000

* Intel(R) platforms with Intel(R) NICs combinations

   * Platform details

     * Intel(R) Atom(TM) CPU C2758 @ 2.40GHz
     * Intel(R) Xeon(R) CPU D-1540 @ 2.00GHz
     * Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz
     * Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-2658 v2 @ 2.40GHz

   * OS:

     * CentOS 7.2
     * Fedora 25
     * FreeBSD 11
     * Red Hat Enterprise Linux Server release 7.3
     * SUSE Enterprise Linux 12
     * Wind River Linux 8
     * Ubuntu 16.04
     * Ubuntu 16.10

   * NICs:

     * Intel(R) 82599ES 10 Gigabit Ethernet Controller

       * Firmware version: 0x61bf0001
       * Device id (pf/vf): 8086:10fb / 8086:10ed
       * Driver version: 4.0.1-k (ixgbe)

     * Intel(R) Corporation Ethernet Connection X552/X557-AT 10GBASE-T

       * Firmware version: 0x800001cf
       * Device id (pf/vf): 8086:15ad / 8086:15a8
       * Driver version: 4.2.5 (ixgbe)

     * Intel(R) Ethernet Converged Network Adapter X710-DA4 (4x10G)

       * Firmware version: 5.05
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 1.5.23 (i40e)

     * Intel(R) Ethernet Converged Network Adapter X710-DA2 (2x10G)

       * Firmware version: 5.05
       * Device id (pf/vf): 8086:1572 / 8086:154c
       * Driver version: 1.5.23 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA1 (1x40G)

       * Firmware version: 5.05
       * Device id (pf/vf): 8086:1584 / 8086:154c
       * Driver version: 1.5.23 (i40e)

     * Intel(R) Ethernet Converged Network Adapter XL710-QDA2 (2X40G)

       * Firmware version: 5.05
       * Device id (pf/vf): 8086:1583 / 8086:154c
       * Driver version: 1.5.23 (i40e)

     * Intel(R) Corporation I350 Gigabit Network Connection

       * Firmware version: 1.48, 0x800006e7
       * Device id (pf/vf): 8086:1521 / 8086:1520
       * Driver version: 5.2.13-k (igb)
