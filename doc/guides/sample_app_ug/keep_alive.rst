
..  BSD LICENSE
    Copyright(c) 2015-2016 Intel Corporation. All rights reserved.
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
on packet processing cores. A Keep Alive Monitor Agent Core (master)
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

To compile the application:

#.  Go to the sample application directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk cd ${RTE_SDK}/examples/keep_alive

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible RTE_TARGET values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

The application has a number of command line options:

.. code-block:: console

    ./build/l2fwd-keepalive [EAL options] \
            -- -p PORTMASK [-q NQ] [-K PERIOD] [-T PERIOD]

where,

* ``p PORTMASK``: A hexadecimal bitmask of the ports to configure

* ``q NQ``: A number of queues (=ports) per lcore (default is 1)

* ``K PERIOD``: Heartbeat check period in ms(5ms default; 86400 max)

* ``T PERIOD``: statistics will be refreshed each PERIOD seconds (0 to
  disable, 10 default, 86400 maximum).

To run the application in linuxapp environment with 4 lcores, 16 ports
8 RX queues per lcore and a ping interval of 10ms, issue the command:

.. code-block:: console

    ./build/l2fwd-keepalive -c f -n 4 -- -q 8 -p ffff -K 10

Refer to the *DPDK Getting Started Guide* for general information on
running applications and the Environment Abstraction Layer (EAL)
options.


Explanation
-----------

The following sections provide some explanation of the The
Keep-Alive/'Liveliness' conceptual scheme. As mentioned in the
overview section, the initialization and run-time paths are very
similar to those of the :doc:`l2_forward_real_virtual`.

The Keep-Alive/'Liveliness' conceptual scheme:

* A Keep- Alive Agent Runs every N Milliseconds.

* DPDK Cores respond to the keep-alive agent.

* If keep-alive agent detects time-outs, it notifies the
  fault management entity through a callback function.

The following sections provide some explanation of the code aspects
that are specific to the Keep Alive sample application.

The keepalive functionality is initialized with a struct
rte_keepalive and the callback function to invoke in the
case of a timeout.

.. code-block:: c

    rte_global_keepalive_info = rte_keepalive_create(&dead_core, NULL);
    if (rte_global_keepalive_info == NULL)
        rte_exit(EXIT_FAILURE, "keepalive_create() failed");

The function that issues the pings keepalive_dispatch_pings()
is configured to run every check_period milliseconds.

.. code-block:: c

    if (rte_timer_reset(&hb_timer,
            (check_period * rte_get_timer_hz()) / 1000,
            PERIODICAL,
            rte_lcore_id(),
            &rte_keepalive_dispatch_pings,
            rte_global_keepalive_info
            ) != 0 )
        rte_exit(EXIT_FAILURE, "Keepalive setup failure.\n");

The rest of the initialization and run-time path follows
the same paths as the the L2 forwarding application. The only
addition to the main processing loop is the mark alive
functionality and the example random failures.

.. code-block:: c

    rte_keepalive_mark_alive(&rte_global_keepalive_info);
    cur_tsc = rte_rdtsc();

    /* Die randomly within 7 secs for demo purposes.. */
    if (cur_tsc - tsc_initial > tsc_lifetime)
    break;

The rte_keepalive_mark_alive function simply sets the core state to alive.

.. code-block:: c

    static inline void
    rte_keepalive_mark_alive(struct rte_keepalive *keepcfg)
    {
        keepcfg->state_flags[rte_lcore_id()] = ALIVE;
    }
