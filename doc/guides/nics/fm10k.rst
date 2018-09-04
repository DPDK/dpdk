..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2016 Intel Corporation.

FM10K Poll Mode Driver
======================

The FM10K poll mode driver library provides support for the Intel FM10000
(FM10K) family of 40GbE/100GbE adapters.

FTAG Based Forwarding of FM10K
------------------------------

FTAG Based Forwarding is a unique feature of FM10K. The FM10K family of NICs
support the addition of a Fabric Tag (FTAG) to carry special information.
The FTAG is placed at the beginning of the frame, it contains information
such as where the packet comes from and goes, and the vlan tag. In FTAG based
forwarding mode, the switch logic forwards packets according to glort (global
resource tag) information, rather than the mac and vlan table. Currently this
feature works only on PF.

To enable this feature, the user should pass a devargs parameter to the eal
like "-w 84:00.0,enable_ftag=1", and the application should make sure an
appropriate FTAG is inserted for every frame on TX side.

Vector PMD for FM10K
--------------------

Vector PMD (vPMD) uses Intel® SIMD instructions to optimize packet I/O.
It improves load/store bandwidth efficiency of L1 data cache by using a wider
SSE/AVX ''register (1)''.
The wider register gives space to hold multiple packet buffers so as to save
on the number of instructions when bulk processing packets.

There is no change to the PMD API. The RX/TX handlers are the only two entries for
vPMD packet I/O. They are transparently registered at runtime RX/TX execution
if all required conditions are met.

1.  To date, only an SSE version of FM10K vPMD is available.
    To ensure that vPMD is in the binary code, set
    ``CONFIG_RTE_LIBRTE_FM10K_INC_VECTOR=y`` in the configure file.

Some constraints apply as pre-conditions for specific optimizations on bulk
packet transfers. The following sections explain RX and TX constraints in the
vPMD.


RX Constraints
~~~~~~~~~~~~~~


Prerequisites and Pre-conditions
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

For Vector RX it is assumed that the number of descriptor rings will be a power
of 2. With this pre-condition, the ring pointer can easily scroll back to the
head after hitting the tail without a conditional check. In addition Vector RX
can use this assumption to do a bit mask using ``ring_size - 1``.


Features not Supported by Vector RX PMD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

Some features are not supported when trying to increase the throughput in
vPMD. They are:

*   IEEE1588

*   Flow director

*   Header split

*   RX checksum offload

Other features are supported using optional MACRO configuration. They include:

*   HW VLAN strip

*   L3/L4 packet type

To enable via ``RX_OLFLAGS`` use ``RTE_LIBRTE_FM10K_RX_OLFLAGS_ENABLE=y``.

To guarantee the constraint, the following capabilities in ``dev_conf.rxmode.offloads``
will be checked:

*   ``DEV_RX_OFFLOAD_VLAN_EXTEND``

*   ``DEV_RX_OFFLOAD_CHECKSUM``

*   ``DEV_RX_OFFLOAD_HEADER_SPLIT``

*   ``fdir_conf->mode``


RX Burst Size
^^^^^^^^^^^^^

As vPMD is focused on high throughput, it processes 4 packets at a time. So it assumes
that the RX burst should be greater than 4 packets per burst. It returns zero if using
``nb_pkt`` < 4 in the receive handler. If ``nb_pkt`` is not a multiple of 4, a
floor alignment will be applied.


TX Constraint
~~~~~~~~~~~~~

Features not Supported by TX Vector PMD
^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^^

TX vPMD only works when offloads is set to 0

This means that it does not support any TX offload.

Limitations
-----------


Switch manager
~~~~~~~~~~~~~~

The Intel FM10000 family of NICs integrate a hardware switch and multiple host
interfaces. The FM10000 PMD driver only manages host interfaces. For the
switch component another switch driver has to be loaded prior to to the
FM10000 PMD driver. The switch driver can be acquired from Intel support.
Only Testpoint is validated with DPDK, the latest version that has been
validated with DPDK is 4.1.6.

Support for Switch Restart
~~~~~~~~~~~~~~~~~~~~~~~~~~

For FM10000 multi host based design a DPDK app running in the VM or host needs
to be aware of the switch's state since it may undergo a quit-restart. When
the switch goes down the DPDK app will receive a LSC event indicating link
status down, and the app should stop the worker threads that are polling on
the Rx/Tx queues. When switch comes up, a LSC event indicating ``LINK_UP`` is
sent to the app, which can then restart the FM10000 port to resume network
processing.

CRC striping
~~~~~~~~~~~~

The FM10000 family of NICs strip the CRC for every packets coming into the
host interface. So, keeping CRC is not supported.

Maximum packet length
~~~~~~~~~~~~~~~~~~~~~

The FM10000 family of NICS support a maximum of a 15K jumbo frame. The value
is fixed and cannot be changed. So, even when the ``rxmode.max_rx_pkt_len``
member of ``struct rte_eth_conf`` is set to a value lower than 15364, frames
up to 15364 bytes can still reach the host interface.

Statistic Polling Frequency
~~~~~~~~~~~~~~~~~~~~~~~~~~~

The FM10000 NICs expose a set of statistics via the PCI BARs. These statistics
are read from the hardware registers when ``rte_eth_stats_get()`` or
``rte_eth_xstats_get()`` is called. The packet counting registers are 32 bits
while the byte counting registers are 48 bits. As a result, the statistics must
be polled regularly in order to ensure the consistency of the returned reads.

Given the PCIe Gen3 x8, about 50Gbps of traffic can occur. With 64 byte packets
this gives almost 100 million packets/second, causing 32 bit integer overflow
after approx 40 seconds. To ensure these overflows are detected and accounted
for in the statistics, it is necessary to read statistic regularly. It is
suggested to read stats every 20 seconds, which will ensure the statistics
are accurate.


Interrupt mode
~~~~~~~~~~~~~~

The FM10000 family of NICS need one separate interrupt for mailbox. So only
drivers which support multiple interrupt vectors e.g. vfio-pci can work
for fm10k interrupt mode.
