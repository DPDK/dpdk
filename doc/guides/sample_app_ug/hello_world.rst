..  SPDX-License-Identifier: BSD-3-Clause
    Copyright(c) 2010-2014 Intel Corporation.

Hello World Sample Application
==============================

The Hello World sample application is an example of the simplest DPDK application that can be written.
The application simply prints an "helloworld" message on every enabled lcore.

Compiling the Application
-------------------------

To compile the sample application see :doc:`compiling`.

The application is located in the ``helloworld`` sub-directory.

Running the Application
-----------------------

To run the example in a linux environment:

.. code-block:: console

    $ ./build/helloworld -l 0-3 -n 4

Refer to *DPDK Getting Started Guide* for general information on running applications
and the Environment Abstraction Layer (EAL) options.

Explanation
-----------

The following sections provide some explanation of code.

EAL Initialization
~~~~~~~~~~~~~~~~~~

The first task is to initialize the Environment Abstraction Layer (EAL).
This is done in the main() function using the following code:

.. code-block:: c

    int

    main(int argc, char **argv)

    {
        ret = rte_eal_init(argc, argv);
        if (ret < 0)
            rte_panic("Cannot init EAL\n");

This call finishes the initialization process that was started before main() is called (in case of a Linux environment).
The argc and argv arguments are provided to the rte_eal_init() function.
The value returned is the number of parsed arguments.

Starting Application Unit Lcores
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Once the EAL is initialized, the application is ready to launch a function on an lcore.
In this example, lcore_hello() is called on every available lcore.
The following is the definition of the function:

.. code-block:: c

    static int
    lcore_hello(__rte_unused void *arg)
    {
        unsigned lcore_id;

        lcore_id = rte_lcore_id();
        printf("hello from core %u\n", lcore_id);
        return 0;
    }

The code that launches the function on each lcore is as follows:

.. code-block:: c

    /* call lcore_hello() on every slave lcore */

    RTE_LCORE_FOREACH_SLAVE(lcore_id) {
       rte_eal_remote_launch(lcore_hello, NULL, lcore_id);
    }

    /* call it on master lcore too */

    lcore_hello(NULL);

The following code is equivalent and simpler:

.. code-block:: c

    rte_eal_mp_remote_launch(lcore_hello, NULL, CALL_MASTER);

Refer to the *DPDK API Reference* for detailed information on the rte_eal_mp_remote_launch() function.
