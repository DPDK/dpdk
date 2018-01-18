DPDK Release 17.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_17_05.html


New Features
------------

.. This section should contain new features added in this release. Sample
   format:

   * **Add a title in the past tense with a full stop.**

     Add a short 1-2 sentence description in the past tense. The description
     should be enough to allow someone scanning the release notes to
     understand the new feature.

     If the feature adds a lot of sub-features you can use a bullet list like
     this:

     * Added feature foo to do something.
     * Enhanced feature bar to do something else.

     Refer to the previous release notes for examples.

     This section is a comment. do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Reorganized mbuf structure.**

  The mbuf structure has been reorganized as follows:

  * Align fields to facilitate the writing of ``data_off``, ``refcnt``, and
    ``nb_segs`` in one operation.
  * Use 2 bytes for port and number of segments.
  * Move the sequence number to the second cache line.
  * Add a timestamp field.
  * Set default value for ``refcnt``, ``next`` and ``nb_segs`` at mbuf free.

* **Added mbuf raw free API.**

  Moved ``rte_mbuf_raw_free()`` and ``rte_pktmbuf_prefree_seg()`` functions to
  the public API.

* **Added free Tx mbuf on demand API.**

  Added a new function ``rte_eth_tx_done_cleanup()`` which allows an
  application to request the driver to release mbufs that are no longer in use
  from a Tx ring, independent of whether or not the ``tx_rs_thresh`` has been
  crossed.

* **Added device removal interrupt.**

  Added a new ethdev event ``RTE_ETH_DEV_INTR_RMV`` to signify
  the sudden removal of a device.
  This event can be advertised by PCI drivers and enabled accordingly.

* **Added EAL dynamic log framework.**

  Added new APIs to dynamically register named log types, and control
  the level of each type independently.

* **Added descriptor status ethdev API.**

  Added a new API to get the status of a descriptor.

  For Rx, it is almost similar to the ``rx_descriptor_done`` API, except
  it differentiates descriptors which are held by the driver and not
  returned to the hardware. For Tx, it is a new API.

* **Increased number of next hops for LPM IPv6 to 2^21.**

  The next_hop field has been extended from 8 bits to 21 bits for IPv6.

* **Added VFIO hotplug support.**

  Added hotplug support for VFIO in addition to the existing UIO support.

* **Added PowerPC support to pci probing for vfio-pci devices.**

  Enabled sPAPR IOMMU based pci probing for vfio-pci devices.

* **Kept consistent PMD batching behavior.**

  Removed the limit of fm10k/i40e/ixgbe Tx burst size and vhost Rx/Tx burst size
  in order to support the same policy of "make an best effort to Rx/Tx pkts"
  for PMDs.

* **Updated the ixgbe base driver.**

  Updated the ixgbe base driver, including the following changes:

  * Add link block check for KR.
  * Complete HW initialization even if SFP is not present.
  * Add VF xcast promiscuous mode.

* **Added PowerPC support for i40e and its vector PMD.**

  Enabled i40e PMD and its vector PMD by default in PowerPC.

* **Added VF max bandwidth setting in i40e.**

  Enabled capability to set the max bandwidth for a VF in i40e.

* **Added VF TC min and max bandwidth setting in i40e.**

  Enabled capability to set the min and max allocated bandwidth for a TC on a
  VF in i40.

* **Added TC strict priority mode setting on i40e.**

  There are 2 Tx scheduling modes supported for TCs by i40e HW: round robin
  mode and strict priority mode. By default the round robin mode is used. It
  is now possible to change the Tx scheduling mode for a TC. This is a global
  setting on a physical port.

* **Added i40e dynamic device personalization support.**

  * Added dynamic device personalization processing to i40e firmware.

* **Updated i40e driver to support MPLSoUDP/MPLSoGRE.**

  Updated i40e PMD to support MPLSoUDP/MPLSoGRE with MPLSoUDP/MPLSoGRE
  supporting profiles which can be programmed by dynamic device personalization
  (DDP) process.

* **Added Cloud Filter for QinQ steering to i40e.**

  * Added a QinQ cloud filter on the i40e PMD, for steering traffic to a VM
    using both VLAN tags. Note, this feature is not supported in Vector Mode.

* **Updated mlx5 PMD.**

  Updated the mlx5 driver, including the following changes:

  * Added Generic flow API support for classification according to ether type.
  * Extended Generic flow API support for classification of IPv6 flow
    according to Vtc flow, Protocol and Hop limit.
  * Added Generic flow API support for FLAG action.
  * Added Generic flow API support for RSS action.
  * Added support for TSO for non-tunneled and VXLAN packets.
  * Added support for hardware Tx checksum offloads for VXLAN packets.
  * Added support for user space Rx interrupt mode.
  * Improved ConnectX-5 single core and maximum performance.

* **Updated mlx4 PMD.**

  Updated the mlx4 driver, including the following changes:

  * Added support for Generic flow API basic flow items and actions.
  * Added support for device removal event.

* **Updated the sfc_efx driver.**

  * Added Generic Flow API support for Ethernet, VLAN, IPv4, IPv6, UDP and TCP
    pattern items with QUEUE action for ingress traffic.

  * Added support for virtual functions (VFs).

* **Added LiquidIO network PMD.**

  Added poll mode driver support for Cavium LiquidIO II server adapter VFs.

* **Added Atomic Rules Arkville PMD.**

  Added a new poll mode driver for the Arkville family of
  devices from Atomic Rules. The net/ark PMD supports line-rate
  agnostic, multi-queue data movement on Arkville core FPGA instances.

* **Added support for NXP DPAA2 - FSLMC bus.**

  Added the new bus "fslmc" driver for NXP DPAA2 devices. See the
  "Network Interface Controller Drivers" document for more details of this new
  driver.

* **Added support for NXP DPAA2 Network PMD.**

  Added the new "dpaa2" net driver for NXP DPAA2 devices. See the
  "Network Interface Controller Drivers" document for more details of this new
  driver.

* **Added support for the Wind River Systems AVP PMD.**

  Added a new networking driver for the AVP device type. Theses devices are
  specific to the Wind River Systems virtualization platforms.

* **Added vmxnet3 version 3 support.**

  Added support for vmxnet3 version 3 which includes several
  performance enhancements such as configurable Tx data ring, Receive
  Data Ring, and the ability to register memory regions.

* **Updated the TAP driver.**

  Updated the TAP PMD to:

  * Support MTU modification.
  * Support packet type for Rx.
  * Support segmented packets on Rx and Tx.
  * Speed up Rx on TAP when no packets are available.
  * Support capturing traffic from another netdevice.
  * Dynamically change link status when the underlying interface state changes.
  * Added Generic Flow API support for Ethernet, VLAN, IPv4, IPv6, UDP and
    TCP pattern items with DROP, QUEUE and PASSTHRU actions for ingress
    traffic.

* **Added MTU feature support to Virtio and Vhost.**

  Implemented new Virtio MTU feature in Vhost and Virtio:

  * Add ``rte_vhost_mtu_get()`` API to Vhost library.
  * Enable Vhost PMD's MTU get feature.
  * Get max MTU value from host in Virtio PMD

* **Added interrupt mode support for virtio-user.**

  Implemented Rxq interrupt mode and LSC support for virtio-user as a virtual
  device. Supported cases:

  * Rxq interrupt for virtio-user + vhost-user as the backend.
  * Rxq interrupt for virtio-user + vhost-kernel as the backend.
  * LSC interrupt for virtio-user + vhost-user as the backend.

* **Added event driven programming model library (rte_eventdev).**

  This API introduces an event driven programming model.

  In a polling model, lcores poll ethdev ports and associated
  Rx queues directly to look for a packet. By contrast in an event
  driven model, lcores call the scheduler that selects packets for
  them based on programmer-specified criteria. The Eventdev library
  adds support for an event driven programming model, which offers
  applications automatic multicore scaling, dynamic load balancing,
  pipelining, packet ingress order maintenance and
  synchronization services to simplify application packet processing.

  By introducing an event driven programming model, DPDK can support
  both polling and event driven programming models for packet processing,
  and applications are free to choose whatever model
  (or combination of the two) best suits their needs.

* **Added Software Eventdev PMD.**

  Added support for the software eventdev PMD. The software eventdev is a
  software based scheduler device that implements the eventdev API. This
  PMD allows an application to configure a pipeline using the eventdev
  library, and run the scheduling workload on a CPU core.

* **Added Cavium OCTEONTX Eventdev PMD.**

  Added the new octeontx ssovf eventdev driver for OCTEONTX devices. See the
  "Event Device Drivers" document for more details on this new driver.

* **Added information metrics library.**

  Added a library that allows information metrics to be added and updated
  by producers, typically other libraries, for later retrieval by
  consumers such as applications. It is intended to provide a
  reporting mechanism that is independent of other libraries such
  as ethdev.

* **Added bit-rate calculation library.**

  Added a library that can be used to calculate device bit-rates. Calculated
  bitrates are reported using the metrics library.

* **Added latency stats library.**

  Added a library that measures packet latency. The collected statistics are
  jitter and latency. For latency the minimum, average, and maximum is
  measured.

* **Added NXP DPAA2 SEC crypto PMD.**

  A new "dpaa2_sec" hardware based crypto PMD for NXP DPAA2 devices has been
  added. See the "Crypto Device Drivers" document for more details on this
  driver.

* **Updated the Cryptodev Scheduler PMD.**

  * Added a packet-size based distribution mode, which distributes the enqueued
    crypto operations among two slaves, based on their data lengths.
  * Added fail-over scheduling mode, which enqueues crypto operations to a
    primary slave first. Then, any operation that cannot be enqueued is
    enqueued to a secondary slave.
  * Added mode specific option support, so each scheduling mode can
    now be configured individually by the new API.

* **Updated the QAT PMD.**

  The QAT PMD has been updated with additional support for:

  * AES DOCSIS BPI algorithm.
  * DES DOCSIS BPI algorithm.
  * ZUC EEA3/EIA3 algorithms.

* **Updated the AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for:

  * AES DOCSIS BPI algorithm.

* **Updated the OpenSSL PMD.**

  The OpenSSL PMD has been updated with additional support for:

  * DES DOCSIS BPI algorithm.


Resolved Issues
---------------

.. This section should contain bug fixes added to the relevant
   sections. Sample format:

   * **code/section Fixed issue in the past tense with a full stop.**

     Add a short 1-2 sentence description of the resolved issue in the past
     tense.

     The title should contain the code/lib section like a commit message.

     Add the entries in alphabetic order in the relevant sections below.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


* **l2fwd-keepalive: Fixed unclean shutdowns.**

  Added clean shutdown to l2fwd-keepalive so that it can free up
  stale resources used for inter-process communication.


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue in the present
     tense. Add information on any known workarounds.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **LSC interrupt doesn't work for virtio-user + vhost-kernel.**

  LSC interrupt cannot be detected when setting the backend, tap device,
  up/down as we fail to find a way to monitor such event.


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past
     tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* The LPM ``next_hop`` field is extended from 8 bits to 21 bits for IPv6
  while keeping ABI compatibility.

* **Reworked rte_ring library.**

  The rte_ring library has been reworked and updated. The following changes
  have been made to it:

  * Removed the build-time setting ``CONFIG_RTE_RING_SPLIT_PROD_CONS``.
  * Removed the build-time setting ``CONFIG_RTE_LIBRTE_RING_DEBUG``.
  * Removed the build-time setting ``CONFIG_RTE_RING_PAUSE_REP_COUNT``.
  * Removed the function ``rte_ring_set_water_mark`` as part of a general
    removal of watermarks support in the library.
  * Added an extra parameter to the burst/bulk enqueue functions to
    return the number of free spaces in the ring after enqueue. This can
    be used by an application to implement its own watermark functionality.
  * Added an extra parameter to the burst/bulk dequeue functions to return
    the number elements remaining in the ring after dequeue.
  * Changed the return value of the enqueue and dequeue bulk functions to
    match that of the burst equivalents. In all cases, ring functions which
    operate on multiple packets now return the number of elements enqueued
    or dequeued, as appropriate. The updated functions are:

    - ``rte_ring_mp_enqueue_bulk``
    - ``rte_ring_sp_enqueue_bulk``
    - ``rte_ring_enqueue_bulk``
    - ``rte_ring_mc_dequeue_bulk``
    - ``rte_ring_sc_dequeue_bulk``
    - ``rte_ring_dequeue_bulk``

    NOTE: the above functions all have different parameters as well as
    different return values, due to the other listed changes above. This
    means that all instances of the functions in existing code will be
    flagged by the compiler. The return value usage should be checked
    while fixing the compiler error due to the extra parameter.

* **Reworked rte_vhost library.**

  The rte_vhost library has been reworked to make it generic enough so that
  the user could build other vhost-user drivers on top of it. To achieve this
  the following changes have been made:

  * The following vhost-pmd APIs are removed:

    * ``rte_eth_vhost_feature_disable``
    * ``rte_eth_vhost_feature_enable``
    * ``rte_eth_vhost_feature_get``

  * The vhost API ``rte_vhost_driver_callback_register(ops)`` is reworked to
    be per vhost-user socket file. Thus, it takes one more argument:
    ``rte_vhost_driver_callback_register(path, ops)``.

  * The vhost API ``rte_vhost_get_queue_num`` is deprecated, instead,
    ``rte_vhost_get_vring_num`` should be used.

  * The following macros are removed in ``rte_virtio_net.h``

    * ``VIRTIO_RXQ``
    * ``VIRTIO_TXQ``
    * ``VIRTIO_QNUM``

  * The following net specific header files are removed in ``rte_virtio_net.h``

    * ``linux/virtio_net.h``
    * ``sys/socket.h``
    * ``linux/if.h``
    * ``rte_ether.h``

  * The vhost struct ``virtio_net_device_ops`` is renamed to
    ``vhost_device_ops``

  * The vhost API ``rte_vhost_driver_session_start`` is removed. Instead,
    ``rte_vhost_driver_start`` should be used, and there is no need to create
    a thread to call it.

  * The vhost public header file ``rte_virtio_net.h`` is renamed to
    ``rte_vhost.h``


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced
     in the previous releases and made in this release. Use fixed width quotes
     for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Reorganized the mbuf structure.**

  The order and size of the fields in the ``mbuf`` structure changed,
  as described in the `New Features`_ section.

* The ``rte_cryptodev_info.sym`` structure has a new field ``max_nb_sessions_per_qp``
  to support drivers which may support a limited number of sessions per queue_pair.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item in the past
     tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* KNI vhost support has been removed.

* The dpdk_qat sample application has been removed.

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
   + librte_bitratestats.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_cryptodev.so.2
     librte_distributor.so.1
   + librte_eal.so.4
     librte_ethdev.so.6
   + librte_eventdev.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
   + librte_latencystats.so.1
     librte_lpm.so.2
   + librte_mbuf.so.3
     librte_mempool.so.2
     librte_meter.so.1
   + librte_metrics.so.1
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

.. This section should contain a list of platforms that were tested with this
   release.

   The format is:

   * <vendor> platform with <vendor> <type of devices> combinations

     * List of CPU
     * List of OS
     * List of devices
     * Other relevant details...

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* Intel(R) platforms with Intel(R) NICs combinations

   * CPU

     * Intel(R) Atom(TM) CPU C2758 @ 2.40GHz
     * Intel(R) Xeon(R) CPU D-1540 @ 2.00GHz
     * Intel(R) Xeon(R) CPU E5-4667 v3 @ 2.00GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2699 v3 @ 2.30GHz
     * Intel(R) Xeon(R) CPU E5-2695 v4 @ 2.10GHz
     * Intel(R) Xeon(R) CPU E5-2658 v2 @ 2.40GHz
     * Intel(R) Xeon(R) CPU E5-2658 v3 @ 2.20GHz

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

* Intel(R) platforms with Mellanox(R) NICs combinations

   * Platform details:

     * Intel(R) Xeon(R) CPU E5-2697A v4 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2697 v3 @ 2.60GHz
     * Intel(R) Xeon(R) CPU E5-2680 v2 @ 2.80GHz
     * Intel(R) Xeon(R) CPU E5-2640 @ 2.50GHz

   * OS:

     * Red Hat Enterprise Linux Server release 7.3 (Maipo)
     * Red Hat Enterprise Linux Server release 7.2 (Maipo)
     * Ubuntu 16.10
     * Ubuntu 16.04
     * Ubuntu 14.04

   * MLNX_OFED: 4.0-2.0.0.0

   * NICs:

     * Mellanox(R) ConnectX(R)-3 Pro 40G MCX354A-FCC_Ax (2x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1007
       * Firmware version: 2.40.5030

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 Lx 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.2000

     * Mellanox(R) ConnectX(R)-4 Lx 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1015
       * Firmware version: 14.18.2000

     * Mellanox(R) ConnectX(R)-5 100G MCX556A-ECAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1017
       * Firmware version: 16.19.1200

     * Mellanox(R) ConnectX-5 Ex EN 100G MCX516A-CDAT (2x100G)

       * Host interface: PCI Express 4.0 x16
       * Device ID: 15b3:1019
       * Firmware version: 16.19.1200

* IBM(R) Power8(R) with Mellanox(R) NICs combinations

   * Platform details:

       * Processor: POWER8E (raw), AltiVec supported
       * type-model: 8247-22L
       * Firmware FW810.21 (SV810_108)

   * OS: Ubuntu 16.04 LTS PPC le

   * MLNX_OFED: 4.0-2.0.0.0

   * NICs:

     * Mellanox(R) ConnectX(R)-4 10G MCX4111A-XCAT (1x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 10G MCX4121A-XCAT (2x10G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 25G MCX4111A-ACAT (1x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 25G MCX4121A-ACAT (2x25G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 40G MCX4131A-BCAT/MCX413A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 40G MCX415A-BCAT (1x40G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX4131A-GCAT/MCX413A-GCAT (1x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX414A-BCAT (2x50G)

       * Host interface: PCI Express 3.0 x8
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-GCAT/MCX416A-BCAT/MCX416A-GCAT (2x50G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 50G MCX415A-CCAT (1x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000

     * Mellanox(R) ConnectX(R)-4 100G MCX416A-CCAT (2x100G)

       * Host interface: PCI Express 3.0 x16
       * Device ID: 15b3:1013
       * Firmware version: 12.18.2000
