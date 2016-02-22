DPDK Release 16.04
==================


**Read this first**

The text below explains how to update the release notes.

Use proper spelling, capitalization and punctuation in all sections.

Variable and config names should be quoted as fixed width text: ``LIKE_THIS``.

Build the docs and view the output file to ensure the changes are correct::

   make doc-guides-html

   firefox build/doc/html/guides/rel_notes/release_16_04.html


New Features
------------

This section should contain new features added in this release. Sample format:

* **Add a title in the past tense with a full stop.**

  Add a short 1-2 sentence description in the past tense. The description
  should be enough to allow someone scanning the release notes to understand
  the new feature.

  If the feature adds a lot of sub-features you can use a bullet list like this.

  * Added feature foo to do something.
  * Enhanced feature bar to do something else.

  Refer to the previous release notes for examples.

* **Enabled bulk allocation of mbufs.**

  A new function ``rte_pktmbuf_alloc_bulk()`` has been added to allow the user
  to allocate a bulk of mbufs.

* **Virtio 1.0.**

  Enabled virtio 1.0 support for virtio pmd driver.

* **Supported virtio for ARM.**

  Enabled virtio support for armv7/v8. Tested for arm64.
  Virtio for arm support VFIO-noiommu mode only.
  Virtio can work with other non-x86 arch too like powerpc.

* **Supported virtio offload in vhost-user.**

  Add the offload and negotiation of checksum and TSO between vhost-user and
  vanilla Linux virtio guest.

* **Added vhost-user live migration support.**

* **Enabled PCI extended tag for i40e.**

  It enabled extended tag by checking and writing corresponding PCI config
  space bytes, to boost the performance. In the meanwhile, it deprecated the
  legacy way via reading/writing sysfile supported by kernel module igb_uio.


Resolved Issues
---------------

This section should contain bug fixes added to the relevant sections. Sample format:

* **code/section: Fixed issue in the past tense with a full stop.**

  Add a short 1-2 sentence description of the resolved issue in the past tense.
  The title should contain the code/lib section like a commit message.
  Add the entries in alphabetic order in the relevant sections below.


EAL
~~~


Drivers
~~~~~~~

* **ethdev: Fixed byte order consistency between fdir flow and mask.**

  Fixed issue in ethdev library that the structure for setting
  fdir's mask and flow entry was not consistent in byte ordering.

* **aesni_mb: Fixed wrong return value when creating a device.**

  cryptodev_aesni_mb_init() was returning the device id of the device created,
  instead of 0 (when success), that rte_eal_vdev_init() expects.
  This made impossible the creation of more than one aesni_mb device
  from command line.


Libraries
~~~~~~~~~

* **hash: Fixed CRC32c hash computation for non multiple of 4 bytes sizes.**

  Fix crc32c hash functions to return a valid crc32c value for data lengths
  not multiple of 4 bytes.


Examples
~~~~~~~~

* **examples/vhost: Fixed frequent mbuf allocation failure.**

  vhost-switch often fails to allocate mbuf when dequeue from vring because it
  wrongly calculates the number of mbufs needed.


Other
~~~~~


Known Issues
------------

This section should contain new known issues in this release. Sample format:

* **Add title in present tense with full stop.**

  Add a short 1-2 sentence description of the known issue in the present
  tense. Add information on any known workarounds.


API Changes
-----------

This section should contain API changes. Sample format:

* Add a short 1-2 sentence description of the API change. Use fixed width
  quotes for ``rte_function_names`` or ``rte_struct_names``. Use the past tense.


ABI Changes
-----------

* Add a short 1-2 sentence description of the ABI change that was announced in
  the previous releases and made in this release. Use fixed width quotes for
  ``rte_function_names`` or ``rte_struct_names``. Use the past tense.

* The fields in ethdev structure ``rte_eth_fdir_masks`` were changed
  to be in big endian.


Shared Library Versions
-----------------------

Update any library version updated in this release and prepend with a ``+`` sign.

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

     libethdev.so.2
     librte_acl.so.2
     librte_cfgfile.so.2
     librte_cmdline.so.1
     librte_distributor.so.1
     librte_eal.so.2
     librte_hash.so.2
     librte_ip_frag.so.1
     librte_ivshmem.so.1
     librte_jobstats.so.1
     librte_kni.so.2
     librte_kvargs.so.1
     librte_lpm.so.2
     librte_mbuf.so.2
     librte_mempool.so.1
     librte_meter.so.1
     librte_pipeline.so.2
     librte_pmd_bond.so.1
     librte_pmd_ring.so.2
     librte_port.so.2
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.2
