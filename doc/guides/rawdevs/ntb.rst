..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2018 Intel Corporation.

.. include:: <isonum.txt>

NTB Rawdev Driver
=================

The ``ntb`` rawdev driver provides a non-transparent bridge between two
separate hosts so that they can communicate with each other. Thus, many
user cases can benefit from this, such as fault tolerance and visual
acceleration.

This PMD allows two hosts to handshake for device start and stop, memory
allocation for the peer to access and read/write allocated memory from peer.
Also, the PMD allows to use doorbell registers to notify the peer and share
some information by using scratchpad registers.

BIOS setting on Intel Xeon
--------------------------

Intel Non-transparent Bridge (NTB) needs special BIOS settings on both systems.
Note that for 4th Generation Intel\ |reg| Xeon\ |reg| Scalable Processors,
option ``Port Subsystem Mode`` should be changed from ``Gen5`` to ``Gen4 Only``,
then reboot.

- Set ``Non-Transparent Bridge PCIe Port Definition`` for needed PCIe ports
  as ``NTB to NTB`` mode, on both hosts.
- Set ``Enable NTB BARs`` as ``Enabled``, on both hosts.
- Set ``Enable SPLIT BARs`` as ``Disabled``, on both hosts.
- Set ``Imbar1 Size``, ``Imbar2 Size``, ``Embar1 Size`` and ``Embar2 Size``,
  as 12-29 (i.e., 4K-512M) for 2nd Generation Intel\ |reg| Xeon\ |reg| Scalable Processors;
  as 12-51 (i.e., 4K-128PB) for 3rd and 4th Generation Intel\ |reg| Xeon\ |reg| Scalable Processors.
  Note that those bar sizes on both hosts should be the same.
- Set ``Crosslink Control override`` as ``DSD/USP`` on one host,
  ``USD/DSP`` on another host.
- Set ``PCIe PLL SSC (Spread Spectrum Clocking)`` as ``Disabled``, on both hosts.
  This is a hardware requirement when using Re-timer Cards.

Device Setup
------------

The Intel NTB devices need to be bound to a DPDK-supported kernel driver
to use, i.e. igb_uio, vfio. The ``dpdk-devbind.py`` script can be used to
show devices status and to bind them to a suitable kernel driver. They will
appear under the category of "Misc (rawdev) devices".

Prerequisites
-------------

NTB PMD needs kernel PCI driver to support write combining (WC) to get
better performance. The difference will be more than 10 times.
To enable WC, there are 2 ways.

- Insert igb_uio with ``wc_activate=1`` flag if use igb_uio driver.

.. code-block:: console

  insmod igb_uio.ko wc_activate=1

- Enable WC for NTB device's Bar 2 and Bar 4 (Mapped memory) manually.
  The reference is https://www.kernel.org/doc/html/latest/x86/mtrr.html
  Get bar base address using ``lspci -vvv -s ae:00.0 | grep Region``.

.. code-block:: console

  # lspci -vvv -s ae:00.0 | grep Region
  Region 0: Memory at 39bfe0000000 (64-bit, prefetchable) [size=64K]
  Region 2: Memory at 39bfa0000000 (64-bit, prefetchable) [size=512M]
  Region 4: Memory at 39bfc0000000 (64-bit, prefetchable) [size=512M]

Using the following command to enable WC.

.. code-block:: console

  echo "base=0x39bfa0000000 size=0x20000000 type=write-combining" >> /proc/mtrr
  echo "base=0x39bfc0000000 size=0x20000000 type=write-combining" >> /proc/mtrr

And the results:

.. code-block:: console

  # cat /proc/mtrr
  reg00: base=0x000000000 (    0MB), size= 2048MB, count=1: write-back
  reg01: base=0x07f000000 ( 2032MB), size=   16MB, count=1: uncachable
  reg02: base=0x39bfa0000000 (60553728MB), size=  512MB, count=1: write-combining
  reg03: base=0x39bfc0000000 (60554240MB), size=  512MB, count=1: write-combining

To disable WC for these regions, using the following.

.. code-block:: console

     echo "disable=2" >> /proc/mtrr
     echo "disable=3" >> /proc/mtrr

Ring Layout
-----------

Since read/write remote system's memory are through PCI bus, remote read
is much more expensive than remote write. Thus, the enqueue and dequeue
based on ntb ring should avoid remote read. The ring layout for ntb is
like the following:

- Ring Format::

   desc_ring:

      0               16                                              64
      +---------------------------------------------------------------+
      |                        buffer address                         |
      +---------------+-----------------------------------------------+
      | buffer length |                      resv                     |
      +---------------+-----------------------------------------------+

   used_ring:

      0               16              32
      +---------------+---------------+
      | packet length |     flags     |
      +---------------+---------------+

- Ring Layout::

      +------------------------+   +------------------------+
      | used_ring              |   | desc_ring              |
      | +---+                  |   | +---+                  |
      | |   |                  |   | |   |                  |
      | +---+      +--------+  |   | +---+                  |
      | |   | ---> | buffer | <+---+-|   |                  |
      | +---+      +--------+  |   | +---+                  |
      | |   |                  |   | |   |                  |
      | +---+                  |   | +---+                  |
      |  ...                   |   |  ...                   |
      |                        |   |                        |
      |            +---------+ |   |            +---------+ |
      |            | tx_tail | |   |            | rx_tail | |
      | System A   +---------+ |   | System B   +---------+ |
      +------------------------+   +------------------------+
                    <---------traffic---------

- Enqueue and Dequeue
  Based on this ring layout, enqueue reads rx_tail to get how many free
  buffers and writes used_ring and tx_tail to tell the peer which buffers
  are filled with data.
  And dequeue reads tx_tail to get how many packets are arrived, and
  writes desc_ring and rx_tail to tell the peer about the new allocated
  buffers.
  So in this way, only remote write happens and remote read can be avoid
  to get better performance.

Limitation
----------

This PMD is only supported on Intel Xeon Platforms:

- 4th Generation Intel® Xeon® Scalable Processors.
- 3rd Generation Intel® Xeon® Scalable Processors.
- 2nd Generation Intel® Xeon® Scalable Processors.
