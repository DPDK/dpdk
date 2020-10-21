..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015 Intel Corporation.

Packet Ordering Application
============================

The Packet Ordering sample app simply shows the impact of reordering a stream.
It's meant to stress the library with different configurations for performance.

Overview
--------

The application uses at least three CPU cores:

* RX core (main core) receives traffic from the NIC ports and feeds Worker
  cores with traffic through SW queues.

* Worker (worker core) basically do some light work on the packet.
  Currently it modifies the output port of the packet for configurations with
  more than one port enabled.

* TX Core (worker core) receives traffic from Worker cores through software queues,
  inserts out-of-order packets into reorder buffer, extracts ordered packets
  from the reorder buffer and sends them to the NIC ports for transmission.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``packet_ordering`` sub-directory.

Running the Application
-----------------------

Refer to *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Application Command Line
~~~~~~~~~~~~~~~~~~~~~~~~

The application execution command line is:

.. code-block:: console

    ./<build_dir>/examples/dpdk-packet_ordering [EAL options] -- -p PORTMASK /
    [--disable-reorder] [--insight-worker]

The -c EAL CPU_COREMASK option has to contain at least 3 CPU cores.
The first CPU core in the core mask is the main core and would be assigned to
RX core, the last to TX core and the rest to Worker cores.

The PORTMASK parameter must contain either 1 or even enabled port numbers.
When setting more than 1 port, traffic would be forwarded in pairs.
For example, if we enable 4 ports, traffic from port 0 to 1 and from 1 to 0,
then the other pair from 2 to 3 and from 3 to 2, having [0,1] and [2,3] pairs.

The disable-reorder long option does, as its name implies, disable the reordering
of traffic, which should help evaluate reordering performance impact.

The insight-worker long option enables output the packet statistics of each worker thread.
