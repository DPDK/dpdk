DPDK Release 2.2
================

New Features
------------

* **Added RSS dynamic configuration to bonding.**

* **Added e1000 Rx interrupt support.**

* **Added igb TSO support for both PF and VF.**

* **RSS enhancement on Intel x550 NIC**

  * Support 512 entries RSS redirection table.
  * Support per VF RSS redirection table.

* **Flow director enhancement on Intel x550 NIC**

  * Add 2 new flow director modes on x550.
  * One is MAC VLAN mode, the other is tunnel mode.

* **Added i40e vector RX/TX.**

* **Added i40e flow control support.**

* **Added DCB support to i40e PF driver.**

* **Added fm10k TSO support for both PF and VF.**

* **Added fm10k VMDQ support.**

* **New NIC Boulder Rapid support.**

  Boulder Rapid is a new NIC of Intel's fm10k family.

* **Enhanced support for the Chelsio CXGBE driver.**

  *  Added support for Jumbo Frames.
  *  Optimize forwarding performance for Chelsio T5 40GbE cards.

* **Added support for Mellanox ConnectX-4 adapters (mlx5).**

  The mlx5 poll-mode driver implements support for Mellanox ConnectX-4 EN
  and Mellanox ConnectX-4 Lx EN families of 10/25/40/50/100 Gb/s adapters.

  Like mlx4, this PMD is only available for Linux and is disabled by default
  due to external dependencies (libibverbs and libmlx5).

* **Added vhost-user multiple queue support.**

* **Added port hotplug support to xenvirt.**


Resolved Issues
---------------

EAL
~~~

* **eal/linux: Fixed epoll timeout.**

  Fixed issue where the ``rte_epoll_wait()`` function didn't return when the
  underlying call to ``epoll_wait()`` timed out.


Drivers
~~~~~~~

* **igb: Fixed IEEE1588 frame identification in I210.**

  Fixed issue where the flag PKT_RX_IEEE1588_PTP was not being set
  in Intel I210 NIC, as EtherType in RX descriptor is in bits 8:10 of
  Packet Type and not in the default bits 0:2.

* **ixgbe: Fixed issue with X550 DCB.**

  Fixed a DCB issue with x550 where for 8 TCs (Traffic Classes), if a packet
  with user priority 6 or 7 was injected to the NIC, then the NIC would only
  put 3 packets into the queue. There was also a similar issue for 4 TCs.

* **ixgbe: Removed burst size restriction of vector RX.**

  Fixed issue where a burst size less than 32 didn't receive anything.

* **i40e: Fixed base driver allocation when not using first numa node.**

  Fixed i40e issue that occurred when a DPDK application didn't initialize
  ports if memory wasn't available on socket 0.

* **vhost: Fixed Qemu shutdown.**

  Fixed issue with libvirt ``virsh destroy`` not killing the VM.

* **virtio: Fixed crash after changing link state.**

  Fixed io permission in the interrupt handler.

* **virtio: Fixed crash when releasing queue.**

  Fixed issue when releasing null control queue.


Libraries
~~~~~~~~~

* **hash: Fixed memory allocation of Cuckoo Hash key table.**

  Fixed issue where an incorrect Cuckoo Hash key table size could be
  calculated limiting the size to 4GB.


Examples
~~~~~~~~


Other
~~~~~


Known Issues
------------


API Changes
-----------

* The deprecated flow director API is removed.
  It was replaced by rte_eth_dev_filter_ctrl().

* The dcb_queue is renamed to dcb_tc in following dcb configuration
  structures: rte_eth_dcb_rx_conf, rte_eth_dcb_tx_conf,
  rte_eth_vmdq_dcb_conf, rte_eth_vmdq_dcb_tx_conf.

* The function rte_eal_pci_close_one() is removed.
  It was replaced by rte_eal_pci_detach().

* The deprecated ACL API ipv4vlan is removed.

* The deprecated hash function rte_jhash2() is removed.
  It was replaced by rte_jhash_32b().

* The deprecated KNI functions are removed:
  rte_kni_create(), rte_kni_get_port_id() and rte_kni_info_get().

* The deprecated ring PMD functions are removed:
  rte_eth_ring_pair_create() and rte_eth_ring_pair_attach().

* The devargs union field virtual is renamed to virt for C++ compatibility.


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

* librte_table LPM: A new parameter to hold the table name will be added to
  the LPM table parameter structure.

* librte_port: Macros to access the packet meta-data stored within the packet
  buffer has been adjusted to cover the packet mbuf structure.

* librte_cfgfile: Allow longer names and values by increasing the constants
  CFG_NAME_LEN and CFG_VALUE_LEN to 64 and 256 respectively.


Shared Library Versions
-----------------------

The libraries prepended with a plus sign were incremented in this version.

.. code-block:: diff

   + libethdev.so.2
   + librte_acl.so.2
   + librte_cfgfile.so.2
     librte_cmdline.so.1
     librte_distributor.so.1
   + librte_eal.so.2
   + librte_hash.so.2
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
   + librte_port.so.2
     librte_power.so.1
     librte_reorder.so.1
     librte_ring.so.1
     librte_sched.so.1
   + librte_table.so.2
     librte_timer.so.1
     librte_vhost.so.1
