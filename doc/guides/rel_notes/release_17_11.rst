DPDK Release 17.11
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_17_11.html


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

* **Extended port_id range from uint8_t to uint16_t.**

  Increased port_id range from 8 bits to 16 bits in order to support more than
  256 ports in dpdk. All ethdev APIs which have port_id as parameter are changed
  in the meantime.

* **Modified the return type of rte_eth_stats_reset.**

  Changed return type of ``rte_eth_stats_reset`` from ``void`` to ``int``
  so the caller may know whether a device supports the operation or not
  and if the operation was carried out.

* **Added a new driver for Marvell Armada 7k/8k devices.**

  Added the new mrvl net driver for Marvell Armada 7k/8k devices. See the
  "Network Interface Controller Drivers" document for more details on this new
  driver.

* **Added SoftNIC PMD.**

  Added new SoftNIC PMD. This virtual device offers applications a software
  fallback support for traffic management.

* **nfp: Added PF support.**

  Previously Netronome's NFP PMD had just support for VFs. PF support is
  just as a basic DPDK port and has no VF management yet.

  PF support comes with firmware upload support which allows the PMD to
  independently work from kernel netdev NFP drivers.

  NFP 4000 devices are also now supported along with previous 6000 devices.

* **Updated bnxt PMD.**

  Major enhancements include:

   * Support for Flow API
   * Support for Tx and Rx descriptor status functions

* **Updated QAT crypto PMD.**

  Performance enhancements:

  * Removed atomics from the internal queue pair structure.
  * Coalesce writes to HEAD CSR on response processing.
  * Coalesce writes to TAIL CSR on request processing.

  Additional support for:

  * AES CCM algorithm.

* **Updated the AESNI MB PMD.**

  The AESNI MB PMD has been updated with additional support for:

  * DES CBC algorithm.
  * DES DOCSIS BPI algorithm.

  This requires the IPSec Multi-buffer library 0.47. For more details,
  check out the AESNI MB PMD documenation.

* **Updated the OpenSSL PMD.**

  The OpenSSL PMD has been updated with additional support for:

  * DES CBC algorithm.
  * AES CCM algorithm.

* **Added NXP DPAA SEC crypto PMD.**

  A new "dpaa_sec" hardware based crypto PMD for NXP DPAA devices has been
  added. See the "Crypto Device Drivers" document for more details on this
  driver.

* **Added MRVL crypto PMD.**

  A new crypto PMD has been added, which provides several ciphering and hashing
  algorithms. All cryptography operations use the MUSDK library crypto API.

* **Add new benchmarking mode to dpdk-test-crypto-perf application.**

  Added new "PMD cyclecount" benchmark mode to dpdk-test-crypto-perf application
  that displays more detailed breakdown of CPU cycles used by hardware
  acceleration.

* **Added IOMMU support to libvhost-user**

  Implemented device IOTLB in Vhost-user backend, and enabled Virtio's IOMMU
  feature.

* **Added Membership library (rte_member).**

  Added membership library. It provides an API for DPDK applications to insert a
  new member, delete an existing member, or query the existence of a member in a
  given set, or a group of sets. For the case of a group of sets the library
  will return not only whether the element has been inserted before in one of
  the sets but also which set it belongs to.

  The Membership Library is an extension and generalization of a traditional
  filter (for example Bloom Filter) structure that has multiple usages in a wide
  variety of workloads and applications. In general, the Membership Library is a
  data structure that provides a “set-summary” and responds to set-membership
  queries whether a certain member belongs to a set(s).

  See the :ref:`Membership Library <Member_Library>` documentation in
  the Programmers Guide document, for more information.

* **Added the Generic Segmentation Offload Library.**

  Added the Generic Segmentation Offload (GSO) library to enable
  applications to split large packets (e.g. MTU is 64KB) into small
  ones (e.g. MTU is 1500B). Supported packet types are:

  * TCP/IPv4 packets.
  * VxLAN packets, which must have an outer IPv4 header, and contain
    an inner TCP/IPv4 packet.
  * GRE packets, which must contain an outer IPv4 header, and inner
    TCP/IPv4 headers.

  The GSO library doesn't check if the input packets have correct
  checksums, and doesn't update checksums for output packets.
  Additionally, the GSO library doesn't process IP fragmented packets.


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


EAL
~~~

* **Service core fails to call service callback due to atomic lock**

  In a specific configuration of multi-thread unsafe services and service
  cores, a service core previously did not correctly release the atomic lock
  on the service. This would result in the cores polling the service, but it
  looked like another thread was executing the service callback. The logic for
  atomic locking of the services has been fixed and refactored for readability.

Drivers
~~~~~~~


Libraries
~~~~~~~~~


Examples
~~~~~~~~


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
     quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past
     tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Ethdev device name length increased**

  The size of internal device name is increased to 64 characters
  to allow for storing longer bus specific name.

* **Service cores API updated for usability**

  The service cores API has been changed, removing pointers from the API
  where possible, instead using integer IDs to identify each service. This
  simplifed application code, aids debugging, and provides better
  encapsulation. A summary of the main changes made is as follows:

  * Services identified by ID not by ``rte_service_spec`` pointer
  * Reduced API surface by using ``set`` functions instead of enable/disable
  * Reworked ``rte_service_register`` to provide the service ID to registrar
  * Rework start and stop APIs into ``rte_service_runstate_set``
  * Added API to set runstate of service implementation to indicate readyness

* **The following changes made in mempool library**

  * Moved ``flags`` datatype from int to unsigned int for ``rte_mempool``.
  * Removed ``__rte_unused int flag`` param from ``rte_mempool_generic_put``
    and ``rte_mempool_generic_get`` API.
  * Added ``flags`` param in ``rte_mempool_xmem_size`` and
    ``rte_mempool_xmem_usage``.

* Xen dom0 in EAL was removed, as well as xenvirt PMD and vhost_xen.

* ``rte_mem_phy2mch`` was used in Xen dom0 to obtain the physical address;
  remove this API as Xen dom0 support was removed.

* **Add return value to stats_get dev op API**

  The ``stats_get`` dev op API return value has been changed to be int.
  By this way PMDs can return an error value in case of failure at stats
  getting process time.

* **Modified the rte_cryptodev_allocate_driver function in the cryptodev library.**

  The function ``rte_cryptodev_allocate_driver()`` has been modified.
  An extra parameter ``struct cryptodev_driver *crypto_drv`` has been added.

* **Removed deprecated functions to manage log level or type.**

  The functions ``rte_set_log_level()``, ``rte_get_log_level()``,
  ``rte_set_log_type()`` and ``rte_get_log_type()`` have been removed.
  They are respectively replaced by ``rte_log_set_global_level()``,
  ``rte_log_get_global_level()``, ``rte_log_set_level()`` and
  ``rte_log_get_level()``.


ABI Changes
-----------

.. This section should contain ABI changes. Sample format:

   * Add a short 1-2 sentence description of the ABI change that was announced
     in the previous releases and made in this release. Use fixed width quotes
     for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* **Extended port_id range.**

  The size of the field ``port_id`` in the ``rte_eth_dev_data`` structure
  changed, as described in the `New Features` section.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item in the past
     tense.

   This section is a comment. do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* The crypto performance unit tests have been removed,
  replaced by the dpdk-test-crypto-perf application.


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
   + librte_bitratestats.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_cryptodev.so.3
     librte_distributor.so.1
   + librte_eal.so.6
   + librte_ethdev.so.8
   + librte_eventdev.so.3
     librte_gro.so.1
   + librte_gso.so.1
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_latencystats.so.1
     librte_lpm.so.2
     librte_mbuf.so.3
     librte_mempool.so.2
     librte_meter.so.1
     librte_metrics.so.1
     librte_net.so.1
   + librte_pdump.so.2
     librte_pipeline.so.3
   + librte_pmd_bnxt.so.2
   + librte_pmd_bond.so.2
   + librte_pmd_i40e.so.2
   + librte_pmd_ixgbe.so.2
     librte_pmd_ring.so.2
   + librte_pmd_softnic.so.1
   + librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
   + librte_table.so.3
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
