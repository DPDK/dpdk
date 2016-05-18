ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* The ethdev hotplug API is going to be moved to EAL with a notification
  mechanism added to crypto and ethdev libraries so that hotplug is now
  available to both of them. This API will be stripped of the device arguments
  so that it only cares about hotplugging.

* Structures embodying pci and vdev devices are going to be reworked to
  integrate new common rte_device / rte_driver objects (see
  http://dpdk.org/ml/archives/dev/2016-January/031390.html).
  ethdev and crypto libraries will then only handle those objects so that they
  do not need to care about the kind of devices that are being used, making it
  easier to add new buses later.

* ABI changes are planned for struct rte_pci_id, i.e., add new field ``class``.
  This new added ``class`` field can be used to probe pci device by class
  related info. This change should impact size of struct rte_pci_id and struct
  rte_pci_device. The release 16.04 does not contain these ABI changes, but
  release 16.07 will.

* The xstats API and rte_eth_xstats struct will be changed to allow retrieval
  of values without any string copies or parsing.
  No backwards compatibility is planned, as it would require code duplication
  in every PMD that supports xstats.

* ABI changes are planned for adding four new flow types. This impacts
  RTE_ETH_FLOW_MAX. The release 2.2 does not contain these ABI changes,
  but release 2.3 will. [postponed]

* ABI will change for rte_mempool struct to move the cache-related fields
  to the more appropriate rte_mempool_cache struct. The mempool API is
  also changed to enable external cache management that is not tied to EAL
  threads. Some mempool get and put calls are removed in favor of a more
  compact API. The ones that remain are backwards compatible and use the
  per-lcore default cache if available. This change targets release 16.07.

* The rte_mempool struct will be changed in 16.07 to facilitate the new
  external mempool manager functionality.
  The ring element will be replaced with a more generic 'pool' opaque pointer
  to allow new mempool handlers to use their own user-defined mempool
  layout. Also newly added to rte_mempool is a handler index.
  The existing API will be backward compatible, but there will be new API
  functions added to facilitate the creation of mempools using an external
  handler. The 16.07 release will contain these changes.

* A librte_vhost public structures refactor is planned for DPDK 16.07
  that requires both ABI and API change.
  The proposed refactor would expose DPDK vhost dev to applications as
  a handle, like the way kernel exposes an fd to user for locating a
  specific file, and to keep all major structures internally, so that
  we are likely to be free from ABI violations in future.
