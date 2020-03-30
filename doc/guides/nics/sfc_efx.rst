..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2019-2020 Xilinx, Inc.
    Copyright(c) 2016-2019 Solarflare Communications Inc.

    This software was jointly developed between OKTET Labs (under contract
    for Solarflare) and Solarflare Communications, Inc.

Solarflare libefx-based Poll Mode Driver
========================================

The SFC EFX PMD (**librte_pmd_sfc_efx**) provides poll mode driver support
for **Solarflare SFN7xxx and SFN8xxx** family of 10/40 Gbps adapters and
**Solarflare XtremeScale X2xxx** family of 10/25/40/50/100 Gbps adapters.
SFC EFX PMD has support for the latest Linux and FreeBSD operating systems.

More information can be found at `Solarflare Communications website
<http://solarflare.com>`_.


Features
--------

SFC EFX PMD has support for:

- Multiple transmit and receive queues

- Link state information including link status change interrupt

- IPv4/IPv6 TCP/UDP transmit checksum offload

- Inner IPv4/IPv6 TCP/UDP transmit checksum offload

- Port hardware statistics

- Extended statistics (see Solarflare Server Adapter User's Guide for
  the statistics description)

- Basic flow control

- MTU update

- Jumbo frames up to 9K

- Promiscuous mode

- Allmulticast mode

- TCP segmentation offload (TSO) including VXLAN and GENEVE encapsulated

- Multicast MAC filter

- IPv4/IPv6 TCP/UDP receive checksum offload

- Inner IPv4/IPv6 TCP/UDP receive checksum offload

- Received packet type information

- Receive side scaling (RSS)

- RSS hash

- Scattered Rx DMA for packet that are larger that a single Rx descriptor

- Receive queue interrupts

- Deferred receive and transmit queue start

- Transmit VLAN insertion (if running firmware variant supports it)

- Flow API

- Loopback


Non-supported Features
----------------------

The features not yet supported include:

- Priority-based flow control

- Configurable RX CRC stripping (always stripped)

- Header split on receive

- VLAN filtering

- VLAN stripping

- LRO


Limitations
-----------

Due to requirements on receive buffer alignment and usage of the receive
buffer for the auxiliary packet information provided by the NIC up to
extra 269 (14 bytes prefix plus up to 255 bytes for end padding) bytes may be
required in the receive buffer.
It should be taken into account when mbuf pool for receive is created.


Equal stride super-buffer mode
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

When the receive queue uses equal stride super-buffer DMA mode, one HW Rx
descriptor carries many Rx buffers which contiguously follow each other
with some stride (equal to total size of rte_mbuf as mempool object).
Each Rx buffer is an independent rte_mbuf.
However dedicated mempool manager must be used when mempool for the Rx
queue is created. The manager must support dequeue of the contiguous
block of objects and provide mempool info API to get the block size.

Another limitation of a equal stride super-buffer mode, imposed by the
firmware, is that it allows for a single RSS context.


Tunnels support
---------------

NVGRE, VXLAN and GENEVE tunnels are supported on SFN8xxx and X2xxx family
adapters with full-feature firmware variant running.
**sfboot** should be used to configure NIC to run full-feature firmware variant.
See Solarflare Server Adapter User's Guide for details.

SFN8xxx and X2xxx family adapters provide either inner or outer packet classes.
If adapter firmware advertises support for tunnels then the PMD
configures the hardware to report inner classes, and outer classes are
not reported in received packets.
However, for VXLAN and GENEVE tunnels the PMD does report UDP as the
outer layer 4 packet type.

SFN8xxx and X2xxx family adapters report GENEVE packets as VXLAN.
If UDP ports are configured for only one tunnel type then it is safe to
treat VXLAN packet type indication as the corresponding UDP tunnel type.


Flow API support
----------------

Supported attributes:

- Ingress

Supported pattern items:

- VOID

- ETH (exact match of source/destination addresses, individual/group match
  of destination address, EtherType in the outer frame and exact match of
  destination addresses, individual/group match of destination address in
  the inner frame)

- VLAN (exact match of VID, double-tagging is supported)

- IPV4 (exact match of source/destination addresses,
  IP transport protocol)

- IPV6 (exact match of source/destination addresses,
  IP transport protocol)

- TCP (exact match of source/destination ports)

- UDP (exact match of source/destination ports)

- VXLAN (exact match of VXLAN network identifier)

- GENEVE (exact match of virtual network identifier, only Ethernet (0x6558)
  protocol type is supported)

- NVGRE (exact match of virtual subnet ID)

Supported actions:

- VOID

- QUEUE

- RSS

- DROP

- FLAG (supported only with ef10_essb Rx datapath)

- MARK (supported only with ef10_essb Rx datapath)

Validating flow rules depends on the firmware variant.

The :ref:`flow_isolated_mode` is supported.

Ethernet destination individual/group match
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Ethernet item supports I/G matching, if only the corresponding bit is set
in the mask of destination address. If destination address in the spec is
multicast, it matches all multicast (and broadcast) packets, otherwise it
matches unicast packets that are not filtered by other flow rules.

Exceptions to flow rules
~~~~~~~~~~~~~~~~~~~~~~~~

There is a list of exceptional flow rule patterns which will not be
accepted by the PMD. A pattern will be rejected if at least one of the
conditions is met:

- Filtering by IPv4 or IPv6 EtherType without pattern items of internet
  layer and above.

- The last item is IPV4 or IPV6, and it's empty.

- Filtering by TCP or UDP IP transport protocol without pattern items of
  transport layer and above.

- The last item is TCP or UDP, and it's empty.


Supported NICs
--------------

- Solarflare XtremeScale Adapters:

   - Solarflare X2522 Dual Port SFP28 10/25GbE Adapter

   - Solarflare X2541 Single Port QSFP28 10/25G/100G Adapter

   - Solarflare X2542 Dual Port QSFP28 10/25G/100G Adapter

- Solarflare Flareon [Ultra] Server Adapters:

   - Solarflare SFN8522 Dual Port SFP+ Server Adapter

   - Solarflare SFN8522M Dual Port SFP+ Server Adapter

   - Solarflare SFN8042 Dual Port QSFP+ Server Adapter

   - Solarflare SFN8542 Dual Port QSFP+ Server Adapter

   - Solarflare SFN8722 Dual Port SFP+ OCP Server Adapter

   - Solarflare SFN7002F Dual Port SFP+ Server Adapter

   - Solarflare SFN7004F Quad Port SFP+ Server Adapter

   - Solarflare SFN7042Q Dual Port QSFP+ Server Adapter

   - Solarflare SFN7122F Dual Port SFP+ Server Adapter

   - Solarflare SFN7124F Quad Port SFP+ Server Adapter

   - Solarflare SFN7142Q Dual Port QSFP+ Server Adapter

   - Solarflare SFN7322F Precision Time Synchronization Server Adapter


Prerequisites
-------------

- Requires firmware version:

   - SFN7xxx: **4.7.1.1001** or higher

   - SFN8xxx: **6.0.2.1004** or higher

Visit `Solarflare Support Downloads <https://support.solarflare.com>`_ to get
Solarflare Utilities (either Linux or FreeBSD) with the latest firmware.
Follow instructions from Solarflare Server Adapter User's Guide to
update firmware and configure the adapter.


Pre-Installation Configuration
------------------------------


Config File Options
~~~~~~~~~~~~~~~~~~~

The following options can be modified in the ``.config`` file.
Please note that enabling debugging options may affect system performance.

- ``CONFIG_RTE_LIBRTE_SFC_EFX_PMD`` (default **y**)

  Enable compilation of Solarflare libefx-based poll-mode driver.

- ``CONFIG_RTE_LIBRTE_SFC_EFX_DEBUG`` (default **n**)

  Enable compilation of the extra run-time consistency checks.


Per-Device Parameters
~~~~~~~~~~~~~~~~~~~~~

The following per-device parameters can be passed via EAL PCI device
whitelist option like "-w 02:00.0,arg1=value1,...".

Case-insensitive 1/y/yes/on or 0/n/no/off may be used to specify
boolean parameters value.

- ``rx_datapath`` [auto|efx|ef10|ef10_esps] (default **auto**)

  Choose receive datapath implementation.
  **auto** allows the driver itself to make a choice based on firmware
  features available and required by the datapath implementation.
  **efx** chooses libefx-based datapath which supports Rx scatter.
  **ef10** chooses EF10 (SFN7xxx, SFN8xxx, X2xxx) native datapath which is
  more efficient than libefx-based and provides richer packet type
  classification.
  **ef10_esps** chooses SFNX2xxx equal stride packed stream datapath
  which may be used on DPDK firmware variant only
  (see notes about its limitations above).

- ``tx_datapath`` [auto|efx|ef10|ef10_simple] (default **auto**)

  Choose transmit datapath implementation.
  **auto** allows the driver itself to make a choice based on firmware
  features available and required by the datapath implementation.
  **efx** chooses libefx-based datapath which supports VLAN insertion
  (full-feature firmware variant only), TSO and multi-segment mbufs.
  Mbuf segments may come from different mempools, and mbuf reference
  counters are treated responsibly.
  **ef10** chooses EF10 (SFN7xxx, SFN8xxx, X2xxx) native datapath which is
  more efficient than libefx-based but has no VLAN insertion support yet.
  Mbuf segments may come from different mempools, and mbuf reference
  counters are treated responsibly.
  **ef10_simple** chooses EF10 (SFN7xxx, SFN8xxx, X2xxx) native datapath which
  is even more faster then **ef10** but does not support multi-segment
  mbufs, disallows multiple mempools and neglects mbuf reference counters.

- ``perf_profile`` [auto|throughput|low-latency] (default **throughput**)

  Choose hardware tuning to be optimized for either throughput or
  low-latency.
  **auto** allows NIC firmware to make a choice based on
  installed licenses and firmware variant configured using **sfboot**.

- ``stats_update_period_ms`` [long] (default **1000**)

  Adjust period in milliseconds to update port hardware statistics.
  The accepted range is 0 to 65535. The value of **0** may be used
  to disable periodic statistics update. One should note that it's
  only possible to set an arbitrary value on SFN8xxx and X2xxx provided that
  firmware version is 6.2.1.1033 or higher, otherwise any positive
  value will select a fixed update period of **1000** milliseconds

- ``fw_variant`` [dont-care|full-feature|ultra-low-latency|
  capture-packed-stream|dpdk] (default **dont-care**)

  Choose the preferred firmware variant to use. In order for the selected
  option to have an effect, the **sfboot** utility must be configured with the
  **auto** firmware-variant option. The preferred firmware variant applies to
  all ports on the NIC.
  **dont-care** ensures that the driver can attach to an unprivileged function.
  The datapath firmware type to use is controlled by the **sfboot**
  utility.
  **full-feature** chooses full featured firmware.
  **ultra-low-latency** chooses firmware with fewer features but lower latency.
  **capture-packed-stream** chooses firmware for SolarCapture packed stream
  mode.
  **dpdk** chooses DPDK firmware with equal stride super-buffer Rx mode
  for higher Rx packet rate and packet marks support and firmware subvariant
  without checksumming on transmit for higher Tx packet rate if
  checksumming is not required.

- ``rxd_wait_timeout_ns`` [long] (default **200 us**)

  Adjust timeout in nanoseconds to head-of-line block to wait for
  Rx descriptors.
  The accepted range is 0 to 400 ms.
  Flow control should be enabled to make it work.
  The value of **0** disables it and packets are dropped immediately.
  When a packet is dropped because of no Rx descriptors,
  ``rx_nodesc_drop_cnt`` counter grows.
  The feature is supported only by the DPDK firmware variant when equal
  stride super-buffer Rx mode is used.


Dynamic Logging Parameters
~~~~~~~~~~~~~~~~~~~~~~~~~~

One may leverage EAL option "--log-level" to change default levels
for the log types supported by the driver. The option is used with
an argument typically consisting of two parts separated by a colon.

Level value is the last part which takes a symbolic name (or integer).
Log type is the former part which may shell match syntax.
Depending on the choice of the expression, the given log level may
be used either for some specific log type or for a subset of types.

SFC EFX PMD provides the following log types available for control:

- ``pmd.net.sfc.driver`` (default level is **notice**)

  Affects driver-wide messages unrelated to any particular devices.

- ``pmd.net.sfc.main`` (default level is **notice**)

  Matches a subset of per-port log types registered during runtime.
  A full name for a particular type may be obtained by appending a
  dot and a PCI device identifier (``XXXX:XX:XX.X``) to the prefix.

- ``pmd.net.sfc.mcdi`` (default level is **notice**)

  Extra logging of the communication with the NIC's management CPU.
  The format of the log is consumed by the Solarflare netlogdecode
  cross-platform tool. May be managed per-port, as explained above.
