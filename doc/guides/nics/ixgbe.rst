..  BSD LICENSE
    Copyright(c) 2010-2014 Intel Corporation. All rights reserved.
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions
    are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in
    the documentation and/or other materials provided with the
    distribution.
    * Neither the name of Intel Corporation nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
    "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
    LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
    A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
    OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
    SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
    LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
    DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
    OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

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

*   The RTE_LIBRTE_IXGBE_RX_ALLOW_BULK_ALLOC=y configuration MACRO must be set before compiling the code.

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

*   Enabled by RX_OLFLAGS (RTE_IXGBE_RX_OLFLAGS_DISABLE=n)


To guarantee the constraint, configuration flags in dev_conf.rxmode will be checked:

*   hw_vlan_strip

*   hw_vlan_extend

*   hw_ip_checksum

*   header_split

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

Feature not Supported by RX Vector PMD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

TX vPMD only works when txq_flags is set to IXGBE_SIMPLE_FLAGS.

This means that it does not support TX multi-segment, VLAN offload and TX csum offload.
The following MACROs are used for these three features:

*   ETH_TXQ_FLAGS_NOMULTSEGS

*   ETH_TXQ_FLAGS_NOVLANOFFL

*   ETH_TXQ_FLAGS_NOXSUMSCTP

*   ETH_TXQ_FLAGS_NOXSUMUDP

*   ETH_TXQ_FLAGS_NOXSUMTCP


Sample Application Notes
~~~~~~~~~~~~~~~~~~~~~~~~

testpmd
^^^^^^^

By default, using CONFIG_RTE_IXGBE_RX_OLFLAGS_DISABLE=n:

.. code-block:: console

    ./x86_64-native-linuxapp-gcc/app/testpmd -c 300 -n 4 -- -i --burst=32 --rxfreet=32 --mbcache=250 --txpt=32 --rxht=8 --rxwt=0 --txfreet=32 --txrst=32 --txqflags=0xf01

When CONFIG_RTE_IXGBE_RX_OLFLAGS_DISABLE=y, better performance can be achieved:

.. code-block:: console

    ./x86_64-native-linuxapp-gcc/app/testpmd -c 300 -n 4 -- -i --burst=32 --rxfreet=32 --mbcache=250 --txpt=32 --rxht=8 --rxwt=0 --txfreet=32 --txrst=32 --txqflags=0xf01 --disable-hw-vlan

l3fwd
^^^^^

When running l3fwd with vPMD, there is one thing to note.
In the configuration, ensure that port_conf.rxmode.hw_ip_checksum=0.
Otherwise, by default, RX vPMD is disabled.

load_balancer
^^^^^^^^^^^^^

As in the case of l3fwd, set configure port_conf.rxmode.hw_ip_checksum=0 to enable vPMD.
In addition, for improved performance, use -bsz "(32,32),(64,64),(32,32)" in load_balancer to avoid using the default burst size of 144.
