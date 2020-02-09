..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Timer Sample Application
========================

The Timer sample application is a simple application that demonstrates the use of a timer in a DPDK application.
This application prints some messages from different lcores regularly, demonstrating the use of timers.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``timer`` sub-directory.

Running the Application
-----------------------

To run the example in linux environment:

.. code-block:: console

    $ ./build/timer -l 0-3 -n 4

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
         *   rte_timer_manage() every ~10ms (at 2 GHz). In a real
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
    timer0_cb(__rte_unused struct rte_timer *tim, __rte_unused void *arg)
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
    timer1_cb(__rte_unused struct rte_timer *tim, __rte_unused void *arg)
    {
        unsigned lcore_id = rte_lcore_id();
        uint64_t hz;

        printf("%s() on lcore %u\\n", FUNCTION , lcore_id);

        /* reload it on another lcore */

        hz = rte_get_hpet_hz();

        lcore_id = rte_get_next_lcore(lcore_id, 0, 1);

        rte_timer_reset(&timer1, hz/3, SINGLE, lcore_id, timer1_cb, NULL);
    }
