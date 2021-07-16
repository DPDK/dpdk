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

    $ ./<build_dir>/examples/dpdk-timer -l 0-3 -n 4

Refer to the *DPDK Getting Started Guide* for general information on running applications and
the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of the code.

Initialization and Main Loop
~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In addition to EAL initialization, the timer subsystem must be initialized, by calling the rte_timer_subsystem_init() function.

.. literalinclude:: ../../../examples/timer/main.c
    :language: c
    :start-after: Init EAL. 8<
    :end-before: >8 End of init EAL.
    :dedent: 1

After timer creation (see the next paragraph), the main loop is
executed on each worker lcore using the well-known
rte_eal_remote_launch() and also on the main.

.. literalinclude:: ../../../examples/timer/main.c
    :language: c
    :start-after: Call lcore_mainloop() on every worker lcore. 8<
    :end-before: >8 End of call lcore_mainloop() on every worker lcore.
    :dedent: 1

The main loop is very simple in this example:

.. literalinclude:: ../../../examples/timer/main.c
    :language: c
    :start-after: Main loop. 8<
    :end-before: >8 End of main loop.
    :dedent: 1

As explained in the comment, it is better to use the TSC register (as it is a per-lcore register) to check if the
rte_timer_manage() function must be called or not.
In this example, the resolution of the timer is 10 milliseconds.

Managing Timers
~~~~~~~~~~~~~~~

In the main() function, the two timers are initialized.
This call to rte_timer_init() is necessary before doing any other operation on the timer structure.

.. literalinclude:: ../../../examples/timer/main.c
    :language: c
    :start-after: Init timer structures. 8<
    :end-before: >8 End of init timer structures.
    :dedent: 1

Then, the two timers are configured:

*   The first timer (timer0) is loaded on the main lcore and expires every second.
    Since the PERIODICAL flag is provided, the timer is reloaded automatically by the timer subsystem.
    The callback function is timer0_cb().

*   The second timer (timer1) is loaded on the next available lcore every 333 ms.
    The SINGLE flag means that the timer expires only once and must be reloaded manually if required.
    The callback function is timer1_cb().

.. literalinclude:: ../../../examples/timer/main.c
    :language: c
    :start-after: Load timer0, every second, on main lcore, reloaded automatically. 8<
    :end-before: >8 End of two timers configured.
    :dedent: 1

The callback for the first timer (timer0) only displays a message until a global counter reaches 20 (after 20 seconds).
In this case, the timer is stopped using the rte_timer_stop() function.

.. literalinclude:: ../../../examples/timer/main.c
    :language: c
    :start-after: timer0 callback. 8<
    :end-before: >8 End of timer0 callback.

The callback for the second timer (timer1) displays a message and reloads the timer on the next lcore, using the
rte_timer_reset() function:

.. literalinclude:: ../../../examples/timer/main.c
    :language: c
    :start-after: timer1 callback. 8<
    :end-before: >8 End of timer1 callback.
