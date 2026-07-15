.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(c) 2026 Intel Corporation.

PTP Software Relay Sample Application
=====================================

The PTP Software Relay sample application demonstrates how to build
a minimal PTP Transparent Clock relay
between a DPDK-bound physical NIC and a kernel TAP interface
using **software timestamps only**.
It uses the PTP definitions from ``rte_ptp.h`` (in ``lib/net/``)
together with a local packet parser.

The application works with an unmodified Linux kernel and stock DPDK.

For background on PTP see:
`Precision Time Protocol
<https://en.wikipedia.org/wiki/Precision_Time_Protocol>`_.


Limitations
-----------

* Tested with L2 PTP (EtherType 0x88F7) on the wire.
  The local parser also classifies VLAN/QinQ and UDP/IPv4/IPv6.
* Only PTP v2 messages are processed.
* Software timestamps have microsecond-class jitter;
  sub-microsecond precision depends on system load and NIC-to-TAP forwarding latency.
* The PTP time transmitter must be reachable on the physical NIC's L2 network.
* Only one physical port and one TAP port are supported.


How the Application Works
-------------------------

Topology
~~~~~~~~

::

   PTP Time Transmitter  Physical NIC             TAP (kernel)
   (ptp4l -H)  ──L2──  (DPDK vfio-pci)  ──────  dtap0
                             │                      │
                       ptp_tap_relay_sw            ptp4l -S
                    (correctionField +=        (SW timestamps,
                     residence time)           adjusts CLOCK_REALTIME)

The relay sits between a DPDK-owned physical NIC
and a kernel TAP virtual interface.
``ptp4l`` runs on the TAP interface in software timestamp mode (``-S``)
as a PTP time receiver.

Packet Flow
~~~~~~~~~~~

#. The physical NIC receives PTP (and non-PTP) packets via DPDK Rx.
#. A software Rx timestamp is recorded using ``clock_gettime(CLOCK_MONOTONIC)``.
#. Each packet is parsed to locate the PTP header.
#. For PTP **event** messages (Sync, Delay_Req, PDelay_Req, PDelay_Resp),
   a Tx software timestamp is taken just before transmission.
#. The residence time (``tx_ts − rx_ts``) is added to the PTP
   ``correctionField`` via ``rte_ptp_add_correction()`` —
   standard IEEE 1588-2019 Transparent Clock behaviour (§10.2).
#. Packets are forwarded bidirectionally:

   * PHY → TAP  (network → ptp4l)
   * TAP → PHY  (ptp4l → network)

A two-pass design is used: first all packets are classified
and PTP header pointers saved, then a single Tx timestamp is taken immediately
before applying corrections and calling ``rte_eth_tx_burst()``.
This minimises the gap between the measured timestamp and the actual wire egress.


Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``ptp_tap_relay_sw`` sub-directory.

.. note::

   The application uses ``rte_ptp.h`` from ``lib/net/`` (built by default)
   and a local ``ptp_parse.h`` header for packet classification.


Running the Application
-----------------------

Prerequisites
~~~~~~~~~~~~~

* A PTP-capable physical NIC bound to DPDK (e.g. via ``vfio-pci``).
* ``linuxptp`` (``ptp4l``) installed on the system.
* A PTP time transmitter reachable on the same L2 network.

Start the relay
~~~~~~~~~~~~~~~

.. code-block:: console

   ./<build_dir>/examples/dpdk-ptp_tap_relay_sw \
       -l 18-19 -a 0000:cc:00.1 --vdev=net_tap0,iface=dtap0 -- \
       -p 0 -t 1 -T 10

Command-line Options
~~~~~~~~~~~~~~~~~~~~

* ``-p PORT`` — Physical NIC port ID (default: 0).
* ``-t PORT`` — TAP port ID (default: 1).
* ``-T SECS`` — Statistics print interval in seconds (default: 10).

Start PTP time transmitter
~~~~~~~~~~~~~~~~~~~~~~~~~~

On a separate terminal or remote host,
start ``ptp4l`` as time transmitter
with hardware timestamps on the physical NIC:

.. code-block:: console

   ptp4l -i <iface> -m -2 -H --serverOnly=1 \
       --logSyncInterval=-4 --logMinDelayReqInterval=-4

Start PTP time receiver
~~~~~~~~~~~~~~~~~~~~~~~

On the TAP interface, start ``ptp4l`` in software timestamp mode:

.. code-block:: console

   ptp4l -i dtap0 -m -2 -s -S \
       --delay_filter=moving_median --delay_filter_length=10

The time receiver will enter UNCALIBRATED state
for approximately 60 seconds while the PI servo estimates the frequency offset,
then step the clock and enter time-receiver (synchronized) state.
Steady-state RMS offset of 500–1000 ns is typical
on a lightly loaded system with a hardware-timestamped time transmitter.

Example Output
~~~~~~~~~~~~~~

Relay statistics printed every ``-T`` seconds:

::

   [PTP-SW] === Statistics ===
   [PTP-SW]   PHY RX total:   5646
   [PTP-SW]   PHY RX PTP:     5598
   [PTP-SW]   TAP TX:         5646
   [PTP-SW]   TAP RX total:   1800
   [PTP-SW]   TAP RX PTP:     1788
   [PTP-SW]   PHY TX:         1800
   [PTP-SW]   Corrections:    3635

Time receiver ``ptp4l`` output after convergence:

::

   ptp4l[451534.520]: rms  630 max 1166 freq -44365 +/- 100 delay 37668 +/-  71
   ptp4l[451539.525]: rms  602 max 1177 freq -44339 +/- 119 delay 37517 +/-  43
   ptp4l[451544.530]: rms  535 max 1194 freq -44345 +/- 103 delay 37410 +/-  81


Code Explanation
----------------

The following sections explain the main components of the application.

Relay Burst Function
~~~~~~~~~~~~~~~~~~~~

The core relay logic is in ``relay_burst()``,
which handles one direction (PHY→TAP or TAP→PHY) per call:

**Pass 1 — Classify:**

For each received packet, ``ptp_hdr_find()`` locates the PTP header (if present).
For event messages, the header pointer is saved for the second pass.

**Pass 2 — Timestamp and correct:**

A single software Tx timestamp is taken via ``clock_gettime(CLOCK_MONOTONIC)``.
The residence time (``tx_ts − rx_ts``) is added to each saved PTP header's
``correctionField`` using ``rte_ptp_add_correction()``.
The burst is then transmitted with ``rte_eth_tx_burst()``.

Main Loop
~~~~~~~~~

The ``relay_loop()`` function polls both directions in a tight loop:

.. code-block:: c

   while (!force_quit) {
       relay_burst(phy_port, tap_port, ...);   /* PHY → TAP */
       relay_burst(tap_port, phy_port, ...);   /* TAP → PHY */
   }

Statistics are printed at the interval specified by ``-T``.

Timestamp Source
~~~~~~~~~~~~~~~~

``CLOCK_MONOTONIC`` is used rather than ``CLOCK_REALTIME``
because the PTP time receiver's servo continuously adjusts ``CLOCK_REALTIME``.
Using ``CLOCK_REALTIME`` would corrupt residence time measurements
during clock stepping or frequency slewing.
``CLOCK_MONOTONIC`` is portable across Linux and FreeBSD.
