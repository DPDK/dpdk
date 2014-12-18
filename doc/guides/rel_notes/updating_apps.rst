Updating Applications from Previous Versions
============================================

Although backward compatibility is being maintained across DPDK releases, code written for previous versions of the DPDK
may require some code updates to benefit from performance and user experience enhancements provided in later DPDK releases.

DPDK 1.7 to DPDK 1.8
--------------------

Note that in DPDK 1.8, the structure of the rte_mbuf has changed considerably from all previous versions.
It is recommended that users familiarize themselves with the new structure defined in the file rte_mbuf.h in the release package.
The follow are some common changes that need to be made to code using mbufs, following an update to DPDK 1.8:

*   Any references to fields in the pkt or ctrl sub-structures of the mbuf, need to be replaced with references to the field
    directly from the rte_mbuf, i.e. buf->pkt.data_len should be replace by buf->data_len.

*   Any direct references to the data field of the mbuf (original buf->pkt.data) should now be replace by the macro rte_pktmbuf_mtod
    to get a computed data address inside the mbuf buffer area.

*   Any references to the in_port mbuf field should be replace by references to the port field.

NOTE: The above list is not exhaustive, but only includes the most commonly required changes to code using mbufs.

Intel® DPDK 1.6 to DPDK 1.7
---------------------------

Note the following difference between 1.6 and 1.7:

*   The "default" target has been renamed to "native"

Intel® DPDK 1.5 to Intel® DPDK 1.6
----------------------------------

Note the following difference between 1.5 and 1.6:

*   The CONFIG_RTE_EAL _UNBIND_PORTS configuration option, which was deprecated in Intel® DPDK 1.4.x, has been removed in Intel® DPDK 1.6.x.
    Applications using the Intel® DPDK must be explicitly unbound to the igb_uio driver using the dpdk_nic_bind.py script included in the
    Intel® DPDK release and documented in the *Intel® DPDK Getting Started Guide*.

Intel® DPDK 1.4 to Intel® DPDK 1.5
----------------------------------

Note the following difference between 1.4 and 1.5:

*   Starting with version 1.5, the top-level directory created from unzipping the release package will now contain the release version number,
    that is, DPDK-1.5.2/ rather than just DPDK/ .

Intel® DPDK 1.3 to Intel® DPDK 1.4.x
------------------------------------

Note the following difference between releases 1.3 and 1.4.x:

*   In Release 1.4.x, Intel® DPDK applications will no longer unbind the network ports from the Linux* kernel driver when the application initializes.
    Instead, any ports to be used by Intel® DPDK must be unbound from the Linux driver and bound to the igb_uio driver before the application starts.
    This can be done using the pci_unbind.py script included with the Intel® DPDK release and documented in the *Intel® DPDK Getting Started Guide*.

    If the port unbinding behavior present in previous Intel® DPDK releases is required, this can be re-enabled using the CONFIG_RTE_EAL_UNBIND_PORTS
    setting in the appropriate Intel® DPDK compile-time configuration file.

*   In Release 1.4.x, HPET support is disabled in the Intel® DPDK build configuration files, which means that the existing rte_eal_get_hpet_hz() and
    rte_eal_get_hpet_cycles() APIs are not available by default.
    For applications that require timing APIs, but not the HPET timer specifically, it is recommended that the API calls rte_get_timer_cycles()
    and rte_get_timer_hz() be used instead of the HPET-specific APIs.
    These generic APIs can work with either TSC or HPET time sources, depending on what is requested by an application,
    and on what is available on the system at runtime.

    For more details on this and how to re-enable the HPET if it is needed, please consult the *Intel® DPDK Getting Started Guide*.

Intel® DPDK 1.2 to Intel® DPDK 1.3
----------------------------------

Note the following difference between releases 1.2 and 1.3:

*   In release 1.3, the Intel® DPDK supports two different 1 GBe drivers: igb and em.
    Both of them are located in the same library: lib_pmd_e1000.a.
    Therefore, the name of the library to link with for the igb PMD has changed from librte_pmd_igb.a to librte_pmd_e1000.a.

*   The rte_common.h macros, RTE_ALIGN, RTE_ALIGN_FLOOR and RTE_ALIGN_CEIL were renamed to, RTE_PTR_ALIGN, RTE_PTR_ALIGN_FLOOR
    and RTE_PTR_ALIGN_CEIL.
    The original macros are still available but they have different behavior.
    Not updating the macros results in strange compilation errors.

*   The rte_tailq is now defined statically. The rte_tailq APIs have also been changed from being public to internal use only.
    The old public APIs are maintained for backward compatibility reasons. Details can be found in the *Intel® DPDK API Reference*.

*   The method for managing mbufs on the NIC RX rings has been modified to improve performance.
    To allow applications to use the newer, more optimized, code path,
    it is recommended that the rx_free_thresh field in the rte_eth_conf structure,
    which is passed to the Poll Mode Driver when initializing a network port, be set to a value of 32.

Intel® DPDK 1.1 to Intel® DPDK 1.2
----------------------------------

Note the following difference between release 1.1 and release 1.2:

*   The names of the 1G and 10G Ethernet drivers have changed between releases 1.1 and 1.2. While the old driver names still work,
    it is recommended that code be updated to the new names, since the old names are deprecated and may be removed in a future
    release.

    The items affected are as follows:

    *   Any macros referring to RTE_LIBRTE_82576_PMD should be updated to refer to RTE_LIBRTE_IGB_PMD.

    *   Any macros referring to RTE_LIBRTE_82599_PMD should be updated to refer to RTE_LIBRTE_IXGBE_PMD.

    *   Any calls to the rte_82576_pmd_init() function should be replaced by calls to rte_igb_pmd_init().

    *   Any calls to the rte_82599_pmd_init() function should be replaced by calls to rte_ixgbe_pmd_init().

*   The method used for managing mbufs on the NIC TX rings for the 10 GbE driver has been modified to improve performance.
    As a result, different parameter values should be passed to the rte_eth_tx_queue_setup() function.
    The recommended default values are to have tx_thresh.tx_wt hresh, tx_free_thresh,
    as well as the new parameter tx_rs_thresh (all in the struct rte_eth_txconf datatype) set to zero.
    See the "Configuration of Transmit and Receive Queues" section in the *Intel® DPDK Programmer's Guide* for more details.

.. note::

    If the tx_free_thresh field is set to TX_RING_SIZE+1 , as was previously used in some cases to disable free threshold check,
    then an error is generated at port initialization time.
    To avoid this error, configure the TX threshold values as suggested above.
