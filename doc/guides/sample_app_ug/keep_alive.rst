..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2015-2016 Intel Corporation.

Keep Alive Sample Application
=============================

The Keep Alive application is a simple example of a
heartbeat/watchdog for packet processing cores. It demonstrates how
to detect 'failed' DPDK cores and notify a fault management entity
of this failure. Its purpose is to ensure the failure of the core
does not result in a fault that is not detectable by a management
entity.


Overview
--------

The application demonstrates how to protect against 'silent outages'
on packet processing cores. A Keep Alive Monitor Agent Core (main)
monitors the state of packet processing cores (worker cores) by
dispatching pings at a regular time interval (default is 5ms) and
monitoring the state of the cores. Cores states are: Alive, MIA, Dead
or Buried. MIA indicates a missed ping, and Dead indicates two missed
pings within the specified time interval. When a core is Dead, a
callback function is invoked to restart the packet processing core;
A real life application might use this callback function to notify a
higher level fault management entity of the core failure in order to
take the appropriate corrective action.

Note: Only the worker cores are monitored. A local (on the host) mechanism
or agent to supervise the Keep Alive Monitor Agent Core DPDK core is required
to detect its failure.

Note: This application is based on the :doc:`l2_forward_real_virtual`. As
such, the initialization and run-time paths are very similar to those
of the L2 forwarding application.

Compiling the Application
-------------------------

To compile the sample application, see :doc:`compiling`.

The application is located in the ``l2fwd_keep_alive`` sub-directory.

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l2fwd-keepalive [EAL options] \
            -- -p PORTMASK [-q NQ] [-K PERIOD] [-T PERIOD]

where,

* ``p PORTMASK``: A hexadecimal bitmask of the ports to configure

* ``q NQ``: Maximum number of queues per lcore (default is 1)

* ``K PERIOD``: Heartbeat check period in ms(5ms default; 86400 max)

* ``T PERIOD``: statistics will be refreshed each PERIOD seconds (0 to
  disable, 10 default, 86400 maximum).

To run the application in linux environment with 4 lcores, 16 ports
8 RX queues per lcore and a ping interval of 10ms, issue the command:

.. code-block:: console

    ./<build_dir>/examples/dpdk-l2fwd-keepalive -l 0-3 -n 4 -- -q 8 -p ffff -K 10

Refer to the *DPDK Getting Started Guide* for general information on
running applications and the Environment Abstraction Layer (EAL)
options.


Explanation
-----------

The following sections provide explanation of the
Keep-Alive/'Liveliness' conceptual scheme. As mentioned in the
overview section, the initialization and run-time paths are very
similar to those of the :doc:`l2_forward_real_virtual`.

The Keep-Alive/'Liveliness' conceptual scheme:

* A Keep- Alive Agent Runs every N Milliseconds.

* DPDK Cores respond to the keep-alive agent.

* If a keep-alive agent detects time-outs, it notifies the
  fault management entity through a callback function.

The following sections provide explanation of the code aspects
that are specific to the Keep Alive sample application.

The keepalive functionality is initialized with a struct
rte_keepalive and the callback function to invoke in the
case of a timeout.

.. literalinclude:: ../../../examples/l2fwd-keepalive/main.c
    :language: c
    :start-after: Initialize keepalive functionality. 8<
    :end-before: >8 End of initializing keepalive functionality.
    :dedent: 2

The function that issues the pings keepalive_dispatch_pings()
is configured to run every check_period milliseconds.

.. literalinclude:: ../../../examples/l2fwd-keepalive/main.c
    :language: c
    :start-after: Issues the pings keepalive_dispatch_pings(). 8<
    :end-before: >8 End of issuing the pings keepalive_dispatch_pings().
    :dedent: 2

The rest of the initialization and run-time paths follow
the same paths as the L2 forwarding application. The only
addition to the main processing loop is the mark alive
functionality and the example random failures.

.. literalinclude:: ../../../examples/l2fwd-keepalive/main.c
    :language: c
    :start-after: Keepalive heartbeat. 8<
    :end-before: >8 End of keepalive heartbeat.
    :dedent: 2

The rte_keepalive_mark_alive function simply sets the core state to alive.

.. code-block:: c

    static inline void
    rte_keepalive_mark_alive(struct rte_keepalive *keepcfg)
    {
        keepcfg->live_data[rte_lcore_id()].core_state = RTE_KA_STATE_ALIVE;
    }
