..  SPDX-License-Identifier: BSD-3-Clause
    Copyright 2018 The DPDK contributors

DPDK Release 19.02
==================

.. **Read this first.**

   The text in the sections below explains how to update the release notes.

   Use proper spelling, capitalization and punctuation in all sections.

   Variable and config names should be quoted as fixed width text:
   ``LIKE_THIS``.

   Build the docs and view the output file to ensure the changes are correct::

      make doc-guides-html

      xdg-open build/doc/html/guides/rel_notes/release_19_02.html


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
     * Device abstraction libs and PMDs
       - ethdev (lib, PMDs)
       - cryptodev (lib, PMDs)
       - eventdev (lib, PMDs)
       - etc
     * Other libs
     * Apps, Examples, Tools (if significant)

     This section is a comment. Do not overwrite or remove it.
     Also, make sure to start the actual text at the margin.
     =========================================================

* **Added support to free hugepages exactly as originally allocated.**

  Some applications using memory event callbacks (especially for managing
  RDMA memory regions) require that memory be freed back to the system
  exactly as it was originally allocated. These applications typically
  also require that a malloc allocation not span across two separate
  hugepage allocations.  A new ``--match-allocations`` EAL init flag has
  been added to fulfill both of these requirements.

* **Added API to register external memory in DPDK.**

  A new ``rte_extmem_register``/``rte_extmem_unregister`` API was added to allow
  chunks of external memory to be registered with DPDK without adding them to
  the malloc heap.

* **Support for using virtio-user without hugepages**

  The --no-huge mode was augmented to use memfd-backed memory (on systems that
  support memfd), to allow using virtio-user-based NICs without hugepages.

* **Updated the enic driver.**

  * Added support for ``RTE_ETH_DEV_CLOSE_REMOVE`` flag.
  * Added the handler to get firmware version string.
  * Added support for multicast filtering.

* **Added dynamic queues allocation support for i40e VF.**

  Previously, available queues of VF is reserved by PF at initialize stage.
  Now both DPDK PF and Kernel PF (>=2.1.14) will support dynamic queue
  allocation. At runtime, when VF request more queue number exceed the initial
  reserved amount, PF can allocate up to 16 queues as the request after a VF
  reset.

* **Added ICE net PMD**

  Added the new ``ice`` net driver for Intel® Ethernet Network Adapters E810.
  See the :doc:`../nics/ice` NIC guide for more details on this new driver.

* **Added support for SW-assisted VDPA live migration.**

  This SW-assisted VDPA live migration facility helps VDPA devices without
  logging capability to perform live migration, a mediated SW relay can help
  devices to track dirty pages caused by DMA. IFC driver has enabled this
  SW-assisted live migration mode.

* **Added security checks to cryptodev symmetric session operations.**

  Added a set of security checks to the access cryptodev symmetric session.
  The checks include the session's user data read/write check and the
  session private data referencing status check while freeing a session.

* **Updated the AESNI-MB PMD.**

  * Add support for intel-ipsec-mb version 0.52.
  * Add AES-GMAC algorithm support.
  * Add Plain SHA1, SHA224, SHA256, SHA384, and SHA512 algorithms support.

* **Added IPsec Library.**

  Added an experimental library ``librte_ipsec`` to provide ESP tunnel and
  transport support for IPv4 and IPv6 packets.

  The library provides support for AES-CBC ciphering and AES-CBC with HMAC-SHA1
  algorithm-chaining, and AES-GCM and NULL algorithms only at present. It is
  planned to add more algorithms in future releases.

  See :doc:`../prog_guide/ipsec_lib` for more information.

* **Updated the ipsec-secgw sample application.**

  The ``ipsec-secgw`` sample application has been updated to use the new
  ``librte_ipsec`` library also added in this release.
  The original functionality of ipsec-secgw is retained, a new command line
  parameter ``-l`` has  been added to ipsec-secgw to use the IPsec library,
  instead of the existing IPsec code in the application.

  The IPsec library does not support all the functionality of the existing
  ipsec-secgw application, its is planned to add the outstanding functionality
  in future releases.

  See :doc:`../sample_app_ug/ipsec_secgw` for more information.

* **Enabled checksum support in the ISA-L compressdev driver.**

  Added support for both adler and crc32 checksums in the ISA-L PMD.
  This aids data integrity across both compression and decompression.

* **Added a compression performance test tool.**

  Added a new performance test tool to test the compressdev PMD. The tool tests
  compression ratio and compression throughput.


Removed Items
-------------

.. This section should contain removed items in this release. Sample format:

   * Add a short 1-2 sentence description of the removed item
     in the past tense.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================


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
   =========================================================

* eal: Function ``rte_bsf64`` in ``rte_bitmap.h`` has been renamed to
  ``rte_bsf64_safe`` and moved to ``rte_common.h``. A new ``rte_bsf64`` function
  has been added in ``rte_common.h`` that follows convention set by existing
  ``rte_bsf32`` function.

* eal: Segment fd API on Linux now sets error code to ``ENOTSUP`` in more cases
  where segment fd API is not expected to be supported:

  - On attempt to get segment fd for an externally allocated memory segment
  - In cases where memfd support would have been required to provide segment
    fd's (such as in-memory or no-huge mode)

* eal: Functions ``rte_malloc_dump_stats()``, ``rte_malloc_dump_heaps()`` and
  ``rte_malloc_get_socket_stats()`` are no longer safe to call concurrently with
  ``rte_malloc_heap_create()`` or ``rte_malloc_heap_destroy()`` function calls.

* mbuf: ``RTE_MBUF_INDIRECT()``, which was deprecated in 18.05, was replaced
  with ``RTE_MBUF_CLONED()`` and removed in 19.02.

* sched: As result of the new format of the mbuf sched field, the
  functions ``rte_sched_port_pkt_write()`` and
  ``rte_sched_port_pkt_read_tree_path()`` got an additional parameter of
  type ``struct rte_sched_port``.

* pdump: The ``rte_pdump_set_socket_dir()``, the parameter ``path`` of
  ``rte_pdump_init()`` and enum ``rte_pdump_socktype`` were deprecated
  since 18.05 and are removed in this release.

* cryptodev: The parameter ``session_pool`` in the function
  ``rte_cryptodev_queue_pair_setup()`` is removed.

* cryptodev: a new function ``rte_cryptodev_sym_session_pool_create()`` is
  introduced. This function is now mandatory when creating symmetric session
  header mempool. Please note all crypto applications are required to use this
  function from now on. Failed to do so will cause
  ``rte_cryptodev_sym_session_create()`` function call return error.


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
   =========================================================

* mbuf: The format of the sched field of ``rte_mbuf`` has been changed
  to include the following fields: ``queue ID``, ``traffic class``, ``color``.

* cryptodev: as shown in the the 18.11 deprecation notice, the structure
  ``rte_cryptodev_qp_conf`` has been added two parameters of symmetric session
  mempool and symmetric session private data mempool.

* cryptodev: as shown in the the 18.11 deprecation notice, the structure
  ``rte_cryptodev_sym_session`` has been updated to contain more information
  to ensure safely accessing the session and session private data.

* security: New field ``uint64_t opaque_data`` is added into
  ``rte_security_session`` structure. That would allow upper layer to easily
  associate/de-associate some user defined data with the security session.


Shared Library Versions
-----------------------

.. Update any library version updated in this release
   and prepend with a ``+`` sign, like this:

     libfoo.so.1
   + libupdated.so.2
     libbar.so.1

   This section is a comment. Do not overwrite or remove it.
   =========================================================

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     librte_acl.so.2
     librte_bbdev.so.1
     librte_bitratestats.so.2
     librte_bpf.so.1
     librte_bus_dpaa.so.2
     librte_bus_fslmc.so.2
     librte_bus_ifpga.so.2
     librte_bus_pci.so.2
     librte_bus_vdev.so.2
     librte_bus_vmbus.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.2
     librte_compressdev.so.1
   + librte_cryptodev.so.6
     librte_distributor.so.1
     librte_eal.so.9
     librte_efd.so.1
     librte_ethdev.so.11
     librte_eventdev.so.6
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
   + librte_mbuf.so.5
     librte_member.so.1
     librte_mempool.so.5
     librte_meter.so.2
     librte_metrics.so.1
     librte_net.so.1
     librte_pci.so.1
   + librte_pdump.so.3
     librte_pipeline.so.3
     librte_pmd_bnxt.so.2
     librte_pmd_bond.so.2
     librte_pmd_i40e.so.2
     librte_pmd_ixgbe.so.2
     librte_pmd_dpaa2_qdma.so.1
     librte_pmd_ring.so.2
     librte_pmd_softnic.so.1
     librte_pmd_vhost.so.2
     librte_port.so.3
     librte_power.so.1
     librte_rawdev.so.1
     librte_reorder.so.1
     librte_ring.so.2
   + librte_sched.so.2
   + librte_security.so.2
     librte_table.so.3
     librte_timer.so.1
     librte_vhost.so.4


Known Issues
------------

.. This section should contain new known issues in this release. Sample format:

   * **Add title in present tense with full stop.**

     Add a short 1-2 sentence description of the known issue
     in the present tense. Add information on any known workarounds.

   This section is a comment. Do not overwrite or remove it.
   Also, make sure to start the actual text at the margin.
   =========================================================

* ``AVX-512`` support has been disabled for ``GCC`` builds when ``binutils 2.30``
  is detected [1] because of a crash [2]. This can affect ``native`` machine type
  build targets on the platforms that support ``AVX512F`` like ``Intel Skylake``
  processors, and can cause a possible performance drop. The immediate workaround
  is to use ``clang`` compiler on these platforms.
  Initial workaround in DPDK v18.11 was to disable ``AVX-512`` support for ``GCC``
  completely, but based on information on defect submitted to GCC community [3],
  issue has been identified as ``binutils 2.30`` issue. Since currently only GCC
  generates ``AVX-512`` instructions, the scope is limited to ``GCC`` and
  ``binutils 2.30``

  - [1]: Commit ("mk: fix scope of disabling AVX512F support")
  - [2]: https://bugs.dpdk.org/show_bug.cgi?id=97
  - [3]: https://gcc.gnu.org/bugzilla/show_bug.cgi?id=88096


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
   =========================================================
