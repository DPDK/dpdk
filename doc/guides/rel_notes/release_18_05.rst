DPDK Release 18.05
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_18_05.html


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

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added bucket mempool driver.**

  Added bucket mempool driver which provides a way to allocate contiguous
  block of objects.
  Number of objects in the block depends on how many objects fit in
  RTE_DRIVER_MEMPOOL_BUCKET_SIZE_KB memory chunk which is build time option.
  The number may be obtained using rte_mempool_ops_get_info() API.
  Contiguous blocks may be allocated using rte_mempool_get_contig_blocks() API.

* **Added PMD-recommended Tx and Rx parameters**

  Applications can now query drivers for device-tuned values of
  ring sizes, burst sizes, and number of queues.

* **Added RSS hash and key update to CXGBE PMD.**

  Support to update RSS hash and key has been added to CXGBE PMD.

* **Added CXGBE VF PMD.**

  CXGBE VF Poll Mode Driver has been added to run DPDK over Chelsio
  T5/T6 NIC VF instances.

* **Updated Solarflare network PMD.**

  Updated the sfc_efx driver including the following changes:

  * Added support for Solarflare XtremeScale X2xxx family adapters.
  * Added support for NVGRE, VXLAN and GENEVE filters in flow API.
  * Added support for DROP action in flow API.
  * Added support for equal stride super-buffer Rx mode (X2xxx only).
  * Added support for MARK and FLAG actions in flow API (X2xxx only).

* **Added Ethernet poll mode driver for AMD XGBE devices.**

  Added the new ``axgbe`` ethernet poll mode driver for AMD XGBE devices.
  See the :doc:`../nics/axgbe` nic driver guide for more details on this
  new driver.

* **Updated szedata2 PMD.**

  Added support for new NFB-200G2QL card.
  New API was introduced in the libsze2 library which the szedata2 PMD depends
  on thus the new version of the library was needed.
  New versions of the packages are available and the minimum required version
  is 4.4.1.

* **Added support for Broadcom NetXtreme-S (BCM58800) family of controllers (aka Stingray)**

  The BCM58800 devices feature a NetXtreme E-Series advanced network controller, a high-performance
  ARM CPU block, PCI Express (PCIe) Gen3 interfaces, key accelerators for compute offload and a high-
  speed memory subsystem including L3 cache and DDR4 interfaces, all interconnected by a coherent
  Network-on-chip (NOC) fabric.

  The ARM CPU subsystem features eight ARMv8 Cortex-A72 CPUs at 3.0 GHz, arranged in a multi-cluster
  configuration.

* **Added IFCVF vDPA driver.**

  Added IFCVF vDPA driver to support Intel FPGA 100G VF device. IFCVF works
  as a HW vhost data path accelerator, it supports live migration and is
  compatible with virtio 0.95 and 1.0. This driver registers ifcvf vDPA driver
  to vhost lib, when virtio connected, with the help of the registered vDPA
  driver the assigned VF gets configured to Rx/Tx directly to VM's virtio
  vrings.

* **Added support for virtio-user server mode.**
  In a container environment if the vhost-user backend restarts, there's no way
  for it to reconnect to virtio-user. To address this, support for server mode
  is added. In this mode the socket file is created by virtio-user, which the
  backend connects to. This means that if the backend restarts, it can reconnect
  to virtio-user and continue communications.

* **Added crypto workload support to vhost library.**

  New APIs are introduced in vhost library to enable virtio crypto support
  including session creation/deletion handling and translating virtio-crypto
  request into DPDK crypto operations. A sample application is also introduced.

* **Added virtio crypto PMD.**

  Added a new poll mode driver for virtio crypto devices, which provides
  AES-CBC ciphering and AES-CBC with HMAC-SHA1 algorithm-chaining. See the
  :doc:`../cryptodevs/virtio` crypto driver guide for more details on
  this new driver.

* **Added AMD CCP Crypto PMD.**

  Added the new ``ccp`` crypto driver for AMD CCP devices. See the
  :doc:`../cryptodevs/ccp` crypto driver guide for more details on
  this new driver.

* **Updated AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for:

  * AES-CMAC (128-bit key).

* **Added Compressdev Library, a generic compression service library.**

  The compressdev library provides an API for offload of compression and
  decompression operations to hardware or software accelerator devices.

* **Added a new compression poll mode driver using Intels ISA-L.**

   Added the new ``ISA-L`` compression driver, for compression and decompression
   operations in software. See the :doc:`../compressdevs/isal` compression driver
   guide for details on this new driver.

* **Added the Event Timer Adapter Library.**

  The Event Timer Adapter Library extends the event-based model by introducing
  APIs that allow applications to arm/cancel event timers that generate
  timer expiry events. This new type of event is scheduled by an event device
  along with existing types of events.

* **Added OcteonTx TIM Driver (Event timer adapter).**

  The OcteonTx Timer block enables software to schedule events for a future
  time, it is exposed to an application via Event timer adapter library.

  See the :doc:`../eventdevs/octeontx` guide for more details

* **Added Event Crypto Adapter Library.**

    Added the Event Crypto Adapter Library.  This library extends the
    event-based model by introducing APIs that allow applications to
    enqueue/dequeue crypto operations to/from cryptodev as events scheduled
    by an event device.

* **Added Ifpga Bus, a generic Intel FPGA Bus library.**

  The Ifpga Bus library provides support for integrating any Intel FPGA device with
  the DPDK framework. It provides Intel FPGA Partial Bit Stream AFU (Accelerated
  Function Unit) scan and drivers probe.

* **Added IFPGA (Intel FPGA) Rawdev Driver.**

  Added a new Rawdev driver called IFPGA(Intel FPGA) Rawdev Driver, which cooperates
  with OPAE (Open Programmable Acceleration Engine) share code provides common FPGA
  management ops for FPGA operation.

  See the :doc:`../rawdevs/ifpga_rawdev` programmer's guide for more details.

* **Added DPAA2 QDMA Driver (in rawdev).**

  The DPAA2 QDMA is an implementation of the rawdev API, that provide means
  to initiate a DMA transaction from CPU. The initiated DMA is performed
  without CPU being involved in the actual DMA transaction.

  See the :doc:`../rawdevs/dpaa2_qdma` guide for more details.

* **Added DPAA2 Command Interface Driver (in rawdev).**

  The DPAA2 CMDIF is an implementation of the rawdev API, that provides
  communication between the GPP and NXP's QorIQ based AIOP Block (Firmware).
  Advanced IO Processor i.e. AIOP is clusters of programmable RISC engines
  optimised for flexible networking and I/O operations. The communication
  between GPP and AIOP is achieved via using DPCI devices exposed by MC for
  GPP <--> AIOP interaction.

  See the :doc:`../rawdevs/dpaa2_cmdif` guide for more details.

* **Added device event monitor framework.**

  Added a general device event monitor framework at EAL, for device dynamic management.
  Such as device hotplug awareness and actions adopted accordingly. The list of new APIs:

  * ``rte_dev_event_monitor_start`` and ``rte_dev_event_monitor_stop`` are for
    the event monitor enable and disable.
  * ``rte_dev_event_callback_register`` and ``rte_dev_event_callback_unregister``
    are for the user's callbacks register and unregister.

  Linux uevent is supported as backend of this device event notification framework.

* **Added support for procinfo and pdump on eth vdev.**

  For ethernet virtual devices (like tap, pcap, etc), with this feature, we can get
  stats/xstats on shared memory from secondary process, and also pdump packets on
  those virtual devices.

* **Added the BPF Library.**

  The BPF Library provides the ability to load and execute
  Enhanced Berkeley Packet Filter (eBPF) within user-space dpdk application.
  Also it introduces basic framework to load/unload BPF-based filters
  on eth devices (right now only via SW RX/TX callbacks).
  It also adds dependency on libelf.


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past
     tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* service cores: no longer marked as experimental.

  The service cores functions are no longer marked as experimental, and have
  become part of the normal DPDK API and ABI. Any future ABI changes will be
  announced at least one release before the ABI change is made. There are no
  ABI breaking changes planned.

* eal: ``rte_lcore_has_role()`` return value changed.

  This function now returns true or false, respectively,
  rather than 0 or <0 for success or failure.
  It makes use of the function more intuitive.

* mempool: capability flags and related functions have been removed.

  Flags ``MEMPOOL_F_CAPA_PHYS_CONTIG`` and
  ``MEMPOOL_F_CAPA_BLK_ALIGNED_OBJECTS`` were used by octeontx mempool
  driver to customize generic mempool library behaviour.
  Now the new driver callbacks ``calc_mem_size`` and ``populate`` may be
  used to achieve it without specific knowledge in the generic code.

* mempool: xmem functions have been deprecated:

  - ``rte_mempool_xmem_create``
  - ``rte_mempool_xmem_size``
  - ``rte_mempool_xmem_usage``
  - ``rte_mempool_populate_iova_tab``

* mbuf: The control mbuf API has been removed in v18.05. The impacted
  functions and macros are:

  - ``rte_ctrlmbuf_init()``
  - ``rte_ctrlmbuf_alloc()``
  - ``rte_ctrlmbuf_free()``
  - ``rte_ctrlmbuf_data()``
  - ``rte_ctrlmbuf_len()``
  - ``rte_is_ctrlmbuf()``
  - ``CTRL_MBUF_FLAG``

  The packet mbuf API should be used as a replacement.

* meter: updated to accommodate configuration profiles.

  The meter API is changed to support meter configuration profiles. The
  configuration profile represents the set of configuration parameters
  for a given meter object, such as the rates and sizes for the token
  buckets. These configuration parameters were previously the part of meter
  object internal data strcuture. The separation of the configuration
  parameters from meter object data structure results in reducing its
  memory footprint which helps in better cache utilization when large number
  of meter objects are used.

* ethdev: The function ``rte_eth_dev_count``, often mis-used to iterate
  over ports, is deprecated and replaced by ``rte_eth_dev_count_avail``.
  There is also a new function ``rte_eth_dev_count_total`` to get the
  total number of allocated ports, available or not.
  The hotplug-proof applications should use ``RTE_ETH_FOREACH_DEV`` or
  ``RTE_ETH_FOREACH_DEV_OWNED_BY`` as port iterators.

* ethdev, in struct ``struct rte_eth_dev_info``, field ``rte_pci_device *pci_dev``
  replaced with field ``struct rte_device *device``.

* **Changes to semantics of rte_eth_dev_configure() parameters.**

   If both the ``nb_rx_q`` and ``nb_tx_q`` parameters are zero,
   ``rte_eth_dev_configure`` will now use PMD-recommended queue sizes, or if
   recommendations are not provided by the PMD the function will use ethdev
   fall-back values. Previously setting both of the parameters to zero would
   have resulted in ``-EINVAL`` being returned.

* **Changes to semantics of rte_eth_rx_queue_setup() parameters.**

   If the ``nb_rx_desc`` parameter is zero, ``rte_eth_rx_queue_setup`` will
   now use the PMD-recommended Rx ring size, or in the case where the PMD
   does not provide a recommendation, will use an ethdev-provided
   fall-back value. Previously, setting ``nb_rx_desc`` to zero would have
   resulted in an error.

* **Changes to semantics of rte_eth_tx_queue_setup() parameters.**

   If the ``nb_tx_desc`` parameter is zero, ``rte_eth_tx_queue_setup`` will
   now use the PMD-recommended Tx ring size, or in the case where the PMD
   does not provide a recoomendation, will use an ethdev-provided
   fall-back value. Previously, setting ``nb_tx_desc`` to zero would have
   resulted in an error.

* ethdev: several changes were made to the flow API.

  * Unused DUP action was removed.
  * Actions semantics in flow rules: list order now matters ("first
    to last" instead of "all simultaneously"), repeated actions are now
    all performed, and they do not individually have (non-)terminating
    properties anymore.
  * Flow rules are now always terminating unless a PASSTHRU action is
    present.
  * C99-style flexible arrays were replaced with standard pointers in RSS
    action and in RAW pattern item structures due to compatibility issues.
  * The RSS action was modified to not rely on external
    ``struct rte_eth_rss_conf`` anymore to instead expose its own and more
    appropriately named configuration fields directly
    (``rss_conf->rss_key`` => ``key``,
    ``rss_conf->rss_key_len`` => ``key_len``,
    ``rss_conf->rss_hf`` => ``types``,
    ``num`` => ``queue_num``), and the addition of missing RSS parameters
    (``func`` for RSS hash function to apply and ``level`` for the
    encapsulation level).
  * The VLAN pattern item (``struct rte_flow_item_vlan``) was modified to
    include inner EtherType instead of outer TPID. Its default mask was also
    modified to cover the VID part (lower 12 bits) of TCI only.
  * A new transfer attribute was added to ``struct rte_flow_attr`` in order
    to clarify the behavior of some pattern items.
  * PF and VF pattern items are now only accepted by PMDs that implement
    them (bnxt and i40e) when the transfer attribute is also present for
    consistency.
  * Pattern item PORT was renamed PHY_PORT to avoid confusion with DPDK port
    IDs.
  * An action counterpart to the PHY_PORT pattern item was added in order to
    redirect matching traffic to a specific physical port.
  * PORT_ID pattern item and actions were added to match and target DPDK
    port IDs at a higher level than PHY_PORT.

* ethdev: change flow APIs regarding count action:
  * ``rte_flow_create()`` API count action now requires the ``struct rte_flow_action_count``.
  * ``rte_flow_query()`` API parameter changed from action type to action structure.

* ethdev: changes to offload API

   A pure per-port offloading isn't requested to be repeated in [rt]x_conf->offloads to
   ``rte_eth_[rt]x_queue_setup()``. Now any offloading enabled in ``rte_eth_dev_configure()``
   can't be disabled by ``rte_eth_[rt]x_queue_setup()``. Any new added offloading which has
   not been enabled in ``rte_eth_dev_configure()`` and is requested to be enabled in
   ``rte_eth_[rt]x_queue_setup()`` must be per-queue type, otherwise trigger an error log.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced
     in the previous releases and made in this release. Use fixed width quotes
     for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* ring: the alignment constraints on the ring structure has been relaxed
  to one cache line instead of two, and an empty cache line padding is
  added between the producer and consumer structures. The size of the
  structure and the offset of the fields remains the same on platforms
  with 64B cache line, but change on other platforms.

* mempool: ops have changed.

  A new callback ``calc_mem_size`` has been added to ``rte_mempool_ops``
  to allow to customize required memory size calculation.
  A new callback ``populate`` has been added to ``rte_mempool_ops``
  to allow to customize objects population.
  Callback ``get_capabilities`` has been removed from ``rte_mempool_ops``
  since its features are covered by ``calc_mem_size`` and ``populate``
  callbacks.
  Callback ``register_memory_area`` has been removed from ``rte_mempool_ops``
  since the new callback ``populate`` may be used instead of it.

* **Additional fields in rte_eth_dev_info.**

  The ``rte_eth_dev_info`` structure has had two extra entries appended to the
  end of it: ``default_rxportconf`` and ``default_txportconf``. Each of these
  in turn are ``rte_eth_dev_portconf`` structures containing three fields of
  type ``uint16_t``: ``burst_size``, ``ring_size``, and ``nb_queues``. These
  are parameter values recommended for use by the PMD.

* ethdev: ABI for all flow API functions was updated.

  This includes functions ``rte_flow_copy``, ``rte_flow_create``,
  ``rte_flow_destroy``, ``rte_flow_error_set``, ``rte_flow_flush``,
  ``rte_flow_isolate``, ``rte_flow_query`` and ``rte_flow_validate``, due to
  changes in error type definitions (``enum rte_flow_error_type``), removal
  of the unused DUP action (``enum rte_flow_action_type``), modified
  behavior for flow rule actions (see API changes), removal of C99 flexible
  array from RAW pattern item (``struct rte_flow_item_raw``), complete
  rework of the RSS action definition (``struct rte_flow_action_rss``),
  sanity fix in the VLAN pattern item (``struct rte_flow_item_vlan``) and
  new transfer attribute (``struct rte_flow_attr``).

**New parameter added to rte_bbdev_op_cap_turbo_dec.**

  A new parameter ``max_llr_modulus`` has been added to
  ``rte_bbdev_op_cap_turbo_dec`` structure to specify maximal LLR (likelihood
  ratio) absolute value.

* **BBdev Queue Groups split into UL/DL Groups**

  Queue Groups have been split into UL/DL Groups in Turbo Software Driver.
  They are independent for Decode/Encode. ``rte_bbdev_driver_info`` reflects
  introduced changes.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item in the past
     tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue in the present
     tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **pdump is not compatible with old applications.**

  As we changed to use generic multi-process communication for pdump negotiation
  instead of previous dedicated unix socket way, pdump applications, including
  dpdk-pdump example and any other applications using librte_pdump, cannot work
  with older version DPDK primary applications.


Shared Library Versions
-----------------------

.. Update any library version updated in this release and prepend with a ``+``
   sign, like this:

     librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.2

   This section is a comment. Do not overwrite or remove it.
   =========================================================


The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bbdev.so.1
     librte_bitratestats.so.2
   + librte_bpf.so.1
     librte_bus_dpaa.so.1
     librte_bus_fslmc.so.1
     librte_bus_pci.so.1
     librte_bus_vdev.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
   + librte_common_octeontx.so.1
   + librte_compressdev.so.1
     librte_cryptodev.so.4
     librte_distributor.so.1
   + librte_eal.so.7
   + librte_ethdev.so.9
   + librte_eventdev.so.4
     librte_flow_classify.so.1
     librte_gro.so.1
     librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
   + librte_mbuf.so.4
   + librte_mempool.so.4
   + librte_meter.so.2
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
     librte_pdump.so.2
     librte_pipeline.so.3
     librte_pmd_bnxt.so.2
     librte_pmd_bond.so.2
     librte_pmd_i40e.so.2
     librte_pmd_ixgbe.so.2
   + librte_pmd_dpaa2_cmdif.so.1
   + librte_pmd_dpaa2_qdma.so.1
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_rawdev.so.1
     librte_reorder.so.1
   + librte_ring.so.2
     librte_sched.so.1
     librte_security.so.1
     librte_table.so.3
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

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================
