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

* **Added Ethernet poll mode driver for AMD XGBE devices.**

  Added the new ``axgbe`` ethernet poll mode driver for AMD XGBE devices.
  See the :doc:`../nics/axgbe` nic driver guide for more details on this
  new driver.

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

* **Added device event monitor framework.**

  Added a general device event monitor framework at EAL, for device dynamic management.
  Such as device hotplug awareness and actions adopted accordingly. The list of new APIs:

  * ``rte_dev_event_monitor_start`` and ``rte_dev_event_monitor_stop`` are for
    the event monitor enable and disable.
  * ``rte_dev_event_callback_register`` and ``rte_dev_event_callback_unregister``
    are for the user's callbacks register and unregister.

  Linux uevent is supported as backend of this device event notification framework.


API Changes
-----------

.. This section should contain API changes. Sample format:

   * Add a short 1-2 sentence description of the API change. Use fixed width
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past
     tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

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


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced
     in the previous releases and made in this release. Use fixed width quotes
     for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Additional fields in rte_eth_dev_info.**

  The ``rte_eth_dev_info`` structure has had two extra entries appended to the
  end of it: ``default_rxportconf`` and ``default_txportconf``. Each of these
  in turn are ``rte_eth_dev_portconf`` structures containing three fields of
  type ``uint16_t``: ``burst_size``, ``ring_size``, and ``nb_queues``. These
  are parameter values recommended for use by the PMD.

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
     librte_bus_dpaa.so.1
     librte_bus_fslmc.so.1
     librte_bus_pci.so.1
     librte_bus_vdev.so.1
     librte_cfgfile.so.2
     librte_cmdline.so.2
   + librte_common_octeontx.so.1
     librte_cryptodev.so.4
     librte_distributor.so.1
   + librte_eal.so.7
   + librte_ethdev.so.9
     librte_eventdev.so.3
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
     librte_mempool.so.3
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
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_rawdev.so.1
     librte_reorder.so.1
     librte_ring.so.1
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
