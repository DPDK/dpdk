.. SPDX-License-Identifier: BSD-3-Clause
   Copyright(C) 2023 Marvell.

L2 Forwarding MACsec Sample Application
=======================================

The L2 forwarding MACsec application is a simple example of packet processing
using the Data Plane Development Kit (DPDK) which encrypt/decrypt packets
based on rte_security MACsec sessions.

Overview
--------

The L2 forwarding MACsec application performs L2 forwarding for each packet
that is received on an Rx port after encrypting/decrypting the packets
based on rte_security sessions using inline protocol mode.

The destination port is the adjacent port from the enabled portmask, that is,
if the first four ports are enabled (portmask ``0xf``),
ports 1 and 2 forward into each other, and ports 3 and 4 forward into each other.

This application can be used to benchmark performance using a traffic-generator.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``l2fwd-macsec`` sub-directory.

Running the Application
-----------------------

The application requires a number of command line options:

.. code-block:: console

   ./<build_dir>/examples/dpdk-l2fwd-macsec [EAL options] -- -p PORTMASK
       [-q NQ]
       --mcs-tx-portmask OUTBOUND_PORTMASK
       --mcs-rx-portmask INBOUND_PORTMASK
       --mcs-port-config '(port,src_mac,dst_mac)[,(port,src_mac,dst_mac)]'
       [--portmap="(port, port)[,(port, port)]"]
       [-T STAT_INTERVAL]

where,

``p PORTMASK``
  Hexadecimal bitmask of the ports to configure.

``q NQ``
  Number of queues (=ports) per lcore (default is 1).

``T STAT_INTERVAL``
  Time interval in seconds for refreshing the stats (default is 1 sec).
  Value 0 disables stats display.

``--mcs-tx-portmask OUTBOUND_PORTMASK``
  Hexadecimal bitmask of the ports to configure encryption flows.

``--mcs-rx-portmask INBOUND_PORTMASK``
  Hexadecimal bitmask of the ports to configure decryption flows.

``--mcs-port-config '(port,src_mac,dst_mac)[,(port,src_mac,dst_mac)]'``
  Source and destination MAC addresses of incoming packets
  on a port for which MACsec processing is to be done.

``--portmap="(port,port)[,(port,port)]"``
  Forwarding ports mapping.

To run the application in Linux environment with 4 lcores,
4 ports with 2 ports for outbound and 2 ports for outbound,
issue the command:

.. code-block:: console

   $ ./<build_dir>/examples/dpdk-l2fwd-macsec -a 0002:04:00.0 -a 0002:05:00.0 \
       -a 0002:06:00.0 -a 0002:07:00.0 -c 0x1E -- -p 0xf                      \
       --mcs-tx-portmask 0x5 --mcs-rx-portmask 0xA                            \
       --mcs-port-config '(0,02:03:04:05:06:07,01:02:03:04:05:06),            \
       (1,02:03:04:05:06:17,01:02:03:04:05:16),                               \
       (2,02:03:04:05:06:27,01:02:03:04:05:26),                               \
       (3,02:03:04:05:06:37,01:02:03:04:05:36)' -T 10

To run the application in Linux environment with 4 lcores, 4 ports,
to forward Rx traffic of ports 0 & 1 on ports 2 & 3 respectively and vice versa,
issue the command:

.. code-block:: console

   $ ./<build_dir>/examples/dpdk-l2fwd-macsec -a 0002:04:00.0 -a 0002:05:00.0 \
       -a 0002:06:00.0 -a 0002:07:00.0 -c 0x1E -- -p 0xf                      \
       --mcs-tx-portmask 0x5 --mcs-rx-portmask 0xA                            \
       --mcs-port-config="(0,02:03:04:05:06:07,01:02:03:04:05:06),            \
       (1,02:03:04:05:06:17,01:02:03:04:05:16),                               \
       (2,02:03:04:05:06:27,01:02:03:04:05:26),                               \
       (3,02:03:04:05:06:37,01:02:03:04:05:36)" -T 10                         \
       --portmap="(0,2)(1,3)"

Refer to the *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.
