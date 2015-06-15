DPDK Release 2.2
================

New Features
------------


Resolved Issues
---------------


Known Issues
------------


API Changes
-----------

* The function rte_eal_pci_close_one() is removed.
  It was replaced by rte_eal_pci_detach().

* The deprecated ACL API ipv4vlan is removed.

* The deprecated KNI functions are removed:
  rte_kni_create(), rte_kni_get_port_id() and rte_kni_info_get().

* The deprecated ring PMD functions are removed:
  rte_eth_ring_pair_create() and rte_eth_ring_pair_attach().


ABI Changes
-----------

* The EAL and ethdev structures rte_intr_handle and rte_eth_conf were changed
  to support Rx interrupt. It was already done in 2.1 for CONFIG_RTE_NEXT_ABI.

* The ethdev flow director entries for SCTP were changed.
  It was already done in 2.1 for CONFIG_RTE_NEXT_ABI.

* The mbuf structure was changed to support unified packet type.
  It was already done in 2.1 for CONFIG_RTE_NEXT_ABI.

* The dummy malloc library is removed. The content was moved into EAL in 2.1.

* The LPM structure is changed. The deprecated field mem_location is removed.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

   + libethdev.so.2
   + librte_acl.so.2
     librte_cfgfile.so.1
     librte_cmdline.so.1
     librte_distributor.so.1
   + librte_eal.so.2
     librte_hash.so.1
     librte_ip_frag.so.1
     librte_ivshmem.so.1
     librte_jobstats.so.1
   + librte_kni.so.2
     librte_kvargs.so.1
   + librte_lpm.so.2
   + librte_mbuf.so.2
     librte_mempool.so.1
     librte_meter.so.1
     librte_pipeline.so.1
     librte_pmd_bond.so.1
   + librte_pmd_ring.so.2
     librte_port.so.1
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
     librte_table.so.1
     librte_timer.so.1
     librte_vhost.so.1
