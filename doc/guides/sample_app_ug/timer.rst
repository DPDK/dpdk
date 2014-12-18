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

Timer Sample Application
========================

The Timer sample application is a simple application that demonstrates the use of a timer in a DPDK application.
This application prints some messages from different lcores regularly, demonstrating the use of timers.

Compiling the Application
-------------------------

#.  Go to the example directory:

    .. code-block:: console

        export RTE_SDK=/path/to/rte_sdk cd ${RTE_SDK}/examples/timer

#.  Set the target (a default target is used if not specified). For example:

    .. code-block:: console

        export RTE_TARGET=x86_64-native-linuxapp-gcc

    See the *DPDK Getting Started Guide* for possible *RTE_TARGET* values.

#.  Build the application:

    .. code-block:: console

        make

Running the Application
-----------------------

To run the example in linuxapp environment:

.. code-block:: console

    $ ./build/timer -c f -n 4

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Initialization and Main Loop
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to EAL initialization, the timer subsystem must be initialized, by calling the rte_timer_subsystem_init() function.

.. code-block:: c

    /* init EAL */

    ret = rte_eal_init(argc, argv);
    if (ret < 0)
        rte_panic("Cannot init EAL\n");

    /* init RTE timer library */

    rte_timer_subsystem_init();

After timer creation (see the next paragraph),
the main loop is executed on each slave lcore using the well-known rte_eal_remote_launch() and also on the master.

.. code-block:: c

    /* call lcore_mainloop() on every slave lcore  */

    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
        rte_eal_remote_launch(lcore_mainloop, NULL, lcore_id);
    }

    /* call it on master lcore too */

    (void) lcore_mainloop(NULL);

The main loop is very simple in this example:

.. code-block:: c

    while (1) {
        /*
         *   Call the timer handler on each core: as we don't
         *   need a very precise timer, so only call
         *   rte_timer_manage() every ~10ms (at 2 Ghz). In a real
         *   application, this will enhance performances as
         *   reading the HPET timer is not efficient.
        */

        cur_tsc = rte_rdtsc();

        diff_tsc = cur_tsc - prev_tsc;

        if (diff_tsc > TIMER_RESOLUTION_CYCLES) {
            rte_timer_manage();
            prev_tsc = cur_tsc;
        }
    }

As explained in the comment, it is better to use the TSC register (as it is a per-lcore register) to check if the
rte_timer_manage() function must be called or not.
In this example, the resolution of the timer is 10 milliseconds.

Managing Timers
~~~~~~~~~~~~~~~

In the main() function, the two timers are initialized.
This call to rte_timer_init() is necessary before doing any other operation on the timer structure.

.. code-block:: c

    /* init timer structures */

    rte_timer_init(&timer0);
    rte_timer_init(&timer1);

Then, the two timers are configured:

*   The first timer (timer0) is loaded on the master lcore and expires every second.
    Since the PERIODICAL flag is provided, the timer is reloaded automatically by the timer subsystem.
    The callback function is timer0_cb().

*   The second timer (timer1) is loaded on the next available lcore every 333 ms.
    The SINGLE flag means that the timer expires only once and must be reloaded manually if required.
    The callback function is timer1_cb().

.. code-block:: c

    /* load timer0, every second, on master lcore, reloaded automatically */

    hz = rte_get_hpet_hz();

    lcore_id = rte_lcore_id();

    rte_timer_reset(&timer0, hz, PERIODICAL, lcore_id, timer0_cb, NULL);

    /* load timer1, every second/3, on next lcore, reloaded manually */

    lcore_id = rte_get_next_lcore(lcore_id, 0, 1);

    rte_timer_reset(&timer1, hz/3, SINGLE, lcore_id, timer1_cb, NULL);

The callback for the first timer (timer0) only displays a message until a global counter reaches 20 (after 20 seconds).
In this case, the timer is stopped using the rte_timer_stop() function.

.. code-block:: c

    /* timer0 callback */

    static void
    timer0_cb( attribute ((unused)) struct rte_timer *tim, __attribute ((unused)) void *arg)
    {
        static unsigned counter = 0;

        unsigned lcore_id = rte_lcore_id();

        printf("%s() on lcore %u\n", FUNCTION , lcore_id);

        /* this timer is automatically reloaded until we decide to stop it, when counter reaches 20. */

        if ((counter ++) == 20)
            rte_timer_stop(tim);
    }

The callback for the second timer (timer1) displays a message and reloads the timer on the next lcore, using the
rte_timer_reset() function:

.. code-block:: c

    /* timer1 callback */

    static void
    timer1_cb( attribute ((unused)) struct rte_timer *tim, _attribute ((unused)) void *arg)
    {
        unsigned lcore_id = rte_lcore_id();
        uint64_t hz;

        printf("%s() on lcore %u\\n", FUNCTION , lcore_id);

        /* reload it on another lcore */

        hz = rte_get_hpet_hz();

        lcore_id = rte_get_next_lcore(lcore_id, 0, 1);

        rte_timer_reset(&timer1, hz/3, SINGLE, lcore_id, timer1_cb, NULL);
    }
