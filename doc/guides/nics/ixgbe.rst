..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2016 Intel Corporation.

IXGBE Driver
============

Vector PMD for IXGBE
--------------------

Vector PMD uses Intel® SIMD instructions to optimize packet I/O.
It improves load/store bandwidth efficiency of L1 data cache by using a wider SSE/AVX register 1 (1).
The wider register gives space to hold multiple packet buffers so as to save instruction number when processing bulk of packets.

There is no change to PMD API. The RX/TX handler are the only two entries for vPMD packet I/O.
They are transparently registered at runtime RX/TX execution if all condition checks pass.

1.  To date, only an SSE version of IX GBE vPMD is available.
    To ensure that vPMD is in the binary code, ensure that the option CONFIG_RTE_IXGBE_INC_VECTOR=y is in the configure file.

Some constraints apply as pre-conditions for specific optimizations on bulk packet transfers.
The following sections explain RX and TX constraints in the vPMD.

RX Constraints
~~~~~~~~~~~~~~

Prerequisites and Pre-conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

The following prerequisites apply:

*   To enable vPMD to work for RX, bulk allocation for Rx must be allowed.

Ensure that the following pre-conditions are satisfied:

*   rxq->rx_free_thresh >= RTE_PMD_IXGBE_RX_MAX_BURST

*   rxq->rx_free_thresh < rxq->nb_rx_desc

*   (rxq->nb_rx_desc % rxq->rx_free_thresh) == 0

*   rxq->nb_rx_desc  < (IXGBE_MAX_RING_DESC - RTE_PMD_IXGBE_RX_MAX_BURST)

These conditions are checked in the code.

Scattered packets are not supported in this mode.
If an incoming packet is greater than the maximum acceptable length of one "mbuf" data size (by default, the size is 2 KB),
vPMD for RX would be disabled.

By default, IXGBE_MAX_RING_DESC is set to 4096 and RTE_PMD_IXGBE_RX_MAX_BURST is set to 32.

Feature not Supported by RX Vector PMD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some features are not supported when trying to increase the throughput in vPMD.
They are:

*   IEEE1588

*   FDIR

*   Header split

*   RX checksum off load

Other features are supported using optional MACRO configuration. They include:

*   HW VLAN strip

*   HW extend dual VLAN

To guarantee the constraint, capabilities in dev_conf.rxmode.offloads will be checked:

*   DEV_RX_OFFLOAD_VLAN_STRIP

*   DEV_RX_OFFLOAD_VLAN_EXTEND

*   DEV_RX_OFFLOAD_CHECKSUM

*   DEV_RX_OFFLOAD_HEADER_SPLIT

*   dev_conf

fdir_conf->mode will also be checked.

RX Burst Size
^^^^^^^^^^^^^

As vPMD is focused on high throughput, it assumes that the RX burst size is equal to or greater than 32 per burst.
It returns zero if using nb_pkt < 32 as the expected packet number in the receive handler.

TX Constraint
~~~~~~~~~~~~~

Prerequisite
^^^^^^^^^^^^

The only prerequisite is related to tx_rs_thresh.
The tx_rs_thresh value must be greater than or equal to RTE_PMD_IXGBE_TX_MAX_BURST,
but less or equal to RTE_IXGBE_TX_MAX_FREE_BUF_SZ.
Consequently, by default the tx_rs_thresh value is in the range 32 to 64.

Feature not Supported by TX Vector PMD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

TX vPMD only works when offloads is set to 0

This means that it does not support any TX offload.

Application Programming Interface
---------------------------------

In DPDK release v16.11 an API for ixgbe specific functions has been added to the ixgbe PMD.
The declarations for the API functions are in the header ``rte_pmd_ixgbe.h``.

Sample Application Notes
------------------------

l3fwd
~~~~~

When running l3fwd with vPMD, there is one thing to note.
In the configuration, ensure that DEV_RX_OFFLOAD_CHECKSUM in port_conf.rxmode.offloads is NOT set.
Otherwise, by default, RX vPMD is disabled.

load_balancer
~~~~~~~~~~~~~

As in the case of l3fwd, to enable vPMD, do NOT set DEV_RX_OFFLOAD_CHECKSUM in port_conf.rxmode.offloads.
In addition, for improved performance, use -bsz "(32,32),(64,64),(32,32)" in load_balancer to avoid using the default burst size of 144.


Limitations or Known issues
---------------------------

Malicious Driver Detection not Supported
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The Intel x550 series NICs support a feature called MDD (Malicious
Driver Detection) which checks the behavior of the VF driver.
If this feature is enabled, the VF must use the advanced context descriptor
correctly and set the CC (Check Context) bit.
DPDK PF doesn't support MDD, but kernel PF does. We may hit problem in this
scenario kernel PF + DPDK VF. If user enables MDD in kernel PF, DPDK VF will
not work. Because kernel PF thinks the VF is malicious. But actually it's not.
The only reason is the VF doesn't act as MDD required.
There's significant performance impact to support MDD. DPDK should check if
the advanced context descriptor should be set and set it. And DPDK has to ask
the info about the header length from the upper layer, because parsing the
packet itself is not acceptable. So, it's too expensive to support MDD.
When using kernel PF + DPDK VF on x550, please make sure to use a kernel
PF driver that disables MDD or can disable MDD.

Some kernel drivers already disable MDD by default while some kernels can use
the command ``insmod ixgbe.ko MDD=0,0`` to disable MDD. Each "0" in the
command refers to a port. For example, if there are 6 ixgbe ports, the command
should be changed to ``insmod ixgbe.ko MDD=0,0,0,0,0,0``.


Statistics
~~~~~~~~~~

The statistics of ixgbe hardware must be polled regularly in order for it to
remain consistent. Running a DPDK application without polling the statistics will
cause registers on hardware to count to the maximum value, and "stick" at
that value.

In order to avoid statistic registers every reaching the maximum value,
read the statistics from the hardware using ``rte_eth_stats_get()`` or
``rte_eth_xstats_get()``.

The maximum time between statistics polls that ensures consistent results can
be calculated as follows:

.. code-block:: c

  max_read_interval = UINT_MAX / max_packets_per_second
  max_read_interval = 4294967295 / 14880952
  max_read_interval = 288.6218096127183 (seconds)
  max_read_interval = ~4 mins 48 sec.

In order to ensure valid results, it is recommended to poll every 4 minutes.

MTU setting
~~~~~~~~~~~

Although the user can set the MTU separately on PF and VF ports, the ixgbe NIC
only supports one global MTU per physical port.
So when the user sets different MTUs on PF and VF ports in one physical port,
the real MTU for all these PF and VF ports is the largest value set.
This behavior is based on the kernel driver behavior.

VF MAC address setting
~~~~~~~~~~~~~~~~~~~~~~

On ixgbe, the concept of "pool" can be used for different things depending on
the mode. In VMDq mode, "pool" means a VMDq pool. In IOV mode, "pool" means a
VF.

There is no RTE API to add a VF's MAC address from the PF. On ixgbe, the
``rte_eth_dev_mac_addr_add()`` function can be used to add a VF's MAC address,
as a workaround.


Inline crypto processing support
--------------------------------

Inline IPsec processing is supported for ``RTE_SECURITY_ACTION_TYPE_INLINE_CRYPTO``
mode for ESP packets only:

- ESP authentication only: AES-128-GMAC (128-bit key)
- ESP encryption and authentication: AES-128-GCM (128-bit key)

IPsec Security Gateway Sample Application supports inline IPsec processing for
ixgbe PMD.

For more details see the IPsec Security Gateway Sample Application and Security
library documentation.


Virtual Function Port Representors
----------------------------------
The IXGBE PF PMD supports the creation of VF port representors for the control
and monitoring of IXGBE virtual function devices. Each port representor
corresponds to a single virtual function of that device. Using the ``devargs``
option ``representor`` the user can specify which virtual functions to create
port representors for on initialization of the PF PMD by passing the VF IDs of
the VFs which are required.::

  -w DBDF,representor=[0,1,4]

Currently hot-plugging of representor ports is not supported so all required
representors must be specified on the creation of the PF.

Supported Chipsets and NICs
---------------------------

- Intel 82599EB 10 Gigabit Ethernet Controller
- Intel 82598EB 10 Gigabit Ethernet Controller
- Intel 82599ES 10 Gigabit Ethernet Controller
- Intel 82599EN 10 Gigabit Ethernet Controller
- Intel Ethernet Controller X540-AT2
- Intel Ethernet Controller X550-BT2
- Intel Ethernet Controller X550-AT2
- Intel Ethernet Controller X550-AT
- Intel Ethernet Converged Network Adapter X520-SR1
- Intel Ethernet Converged Network Adapter X520-SR2
- Intel Ethernet Converged Network Adapter X520-LR1
- Intel Ethernet Converged Network Adapter X520-DA1
- Intel Ethernet Converged Network Adapter X520-DA2
- Intel Ethernet Converged Network Adapter X520-DA4
- Intel Ethernet Converged Network Adapter X520-QDA1
- Intel Ethernet Converged Network Adapter X520-T2
- Intel 10 Gigabit AF DA Dual Port Server Adapter
- Intel 10 Gigabit AT Server Adapter
- Intel 10 Gigabit AT2 Server Adapter
- Intel 10 Gigabit CX4 Dual Port Server Adapter
- Intel 10 Gigabit XF LR Server Adapter
- Intel 10 Gigabit XF SR Dual Port Server Adapter
- Intel 10 Gigabit XF SR Server Adapter
- Intel Ethernet Converged Network Adapter X540-T1
- Intel Ethernet Converged Network Adapter X540-T2
- Intel Ethernet Converged Network Adapter X550-T1
- Intel Ethernet Converged Network Adapter X550-T2
