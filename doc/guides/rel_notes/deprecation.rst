ABI and API Deprecation
=======================

See the :doc:`guidelines document for details of the ABI policy </contributing/versioning>`.
API and ABI deprecation notices are to be posted here.


Deprecation Notices
-------------------

* The following fields have been deprecated in rte_eth_stats:
  ibadcrc, ibadlen, imcasts, fdirmatch, fdirmiss,
  tx_pause_xon, rx_pause_xon, tx_pause_xoff, rx_pause_xoff

* The scheduler hierarchy structure (rte_sched_port_hierarchy) will change to
  allow for a larger number of subport entries.
  The number of available traffic_classes and queues may also change.
  The mbuf structure element for sched hierarchy will also change from a single
  32 bit to a 64 bit structure.

* The scheduler statistics structure will change to allow keeping track of
  RED actions.

* librte_table: New functions for table entry bulk add/delete will be added
  to the table operations structure.

* librte_table hash: Key mask parameter will be added to the hash table
  parameter structure for 8-byte key and 16-byte key extendible bucket and
  LRU tables.

* librte_pipeline: The prototype for the pipeline input port, output port
  and table action handlers will be updated:
  the pipeline parameter will be added, the packets mask parameter will be
  either removed (for input port action handler) or made input-only.
